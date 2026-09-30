/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimalAI.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Common/RandomNumberManager.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/AnimalAnimations.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Transform.h"
#include "ECS/Map.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/VillagerAnimations.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs::animal_ai
{
using components::Animal;
using components::AnimalBrain;
using components::Fixed;
using components::Flock;
using components::Life;
using components::Transform;

namespace detail
{
const GAnimalInfo& InfoOf(const Animal& animal)
{
	return Locator::infoConstants::value().animal.at(static_cast<size_t>(animal.type));
}

const GAnimalStateTableInfo& StateInfo(uint8_t state)
{
	return Locator::infoConstants::value().animalStateTable.at(std::min<size_t>(state, 52));
}

/// the grazing class (constructor 0x41D0B0, the "Cow" vtable): sheep, tortoise, cow, horse, pig (and the puzzle sheep)
bool IsGrazer(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Sheep:
	case AnimalInfo::Tortoise:
	case AnimalInfo::Cow:
	case AnimalInfo::Horse:
	case AnimalInfo::Pig:
	case AnimalInfo::PuzzleSheep:
		return true;
	default:
		return false;
	}
}

Hunter HunterOf(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Lion:
	case AnimalInfo::Leopard:
	case AnimalInfo::PuzzleLion:
		return Hunter::Cat;
	case AnimalInfo::Tiger:
		return Hunter::Tiger;
	case AnimalInfo::Wolf:
	case AnimalInfo::PuzzleWolf:
		return Hunter::Wolf;
	default:
		return Hunter::None;
	}
}

/// GGameInfo::GetVisualTime (hours) of this turn
float g_VisualTime = 12.0f;
/// g_game+0x205A40, the game turn
uint32_t g_Turn = 0;

// ---- MapCoords and the angle tables ----

/// COS / SIN tables 0xC31E14 / 0xC31614: 2048 entries per circle, 65536 = 1
int32_t Cos(uint16_t a)
{
	return static_cast<int32_t>(std::lround(65536.0 * std::cos(static_cast<double>(a & 0x7FF) * glm::two_pi<double>() / k_Circle)));
}
int32_t Sin(uint16_t a)
{
	return static_cast<int32_t>(std::lround(65536.0 * std::sin(static_cast<double>(a & 0x7FF) * glm::two_pi<double>() / k_Circle)));
}

/// the step of that speed along the angle: ((speed >> 4) * COS[a]) >> 12, MapCoords per turn
glm::ivec2 Step(uint16_t angle, uint32_t speed)
{
	const auto s = static_cast<int32_t>(speed >> 4);
	return {(s * Cos(angle)) >> 12, (s * Sin(angle)) >> 12};
}

/// GUtils::GetAngleFromDXDZ (0x74D200)
uint16_t AngleOf(glm::vec2 d)
{
	const double a = std::atan2(static_cast<double>(d.y), static_cast<double>(d.x)) * k_Circle / glm::two_pi<double>();
	return static_cast<uint16_t>(static_cast<int32_t>(std::lround(a)) & 0x7FF);
}

/// the shortest signed difference b - a in 2048ths
int32_t AngleDiff(uint16_t a, uint16_t b)
{
	return ((static_cast<int32_t>(b) - static_cast<int32_t>(a) + k_Circle / 2) & (k_Circle - 1)) - k_Circle / 2;
}

/// fn_0041A590: v * num / den, truncated
int32_t Scale(int32_t v, int32_t num, int32_t den)
{
	return den == 0 ? 0 : static_cast<int32_t>(static_cast<double>(v) * num / den);
}

float Metres(uint32_t speed)
{
	return static_cast<float>(speed) / k_MapCoordsPerMetre;
}

glm::vec2 Xz(const Transform& transform)
{
	return {transform.position.x, transform.position.z};
}

MapInterface::CellId CellOf(glm::vec2 p)
{
	return MapInterface::GetGridCell(glm::max(p, glm::vec2(0.0f)));
}

/// the drawn rotation of a mobile heading along the angle (as PathfindingSystem's InitializeStep)
void FaceAngle(Transform& transform, uint16_t angle)
{
	const float theta = static_cast<float>(angle) * glm::two_pi<float>() / k_Circle;
	transform.rotation = glm::mat3(glm::eulerAngleY(-theta - glm::half_pi<float>()));
}

/// the angle of a drawn rotation (its forward column)
uint16_t AngleOfRotation(const glm::mat3& rotation)
{
	const float yaw = std::atan2(rotation[2].x, rotation[2].z);
	const double theta = (-static_cast<double>(yaw) - glm::half_pi<double>()) * k_Circle / glm::two_pi<double>();
	return static_cast<uint16_t>(static_cast<int32_t>(std::lround(theta)) & 0x7FF);
}

// ---- the land ----

bool InBounds(glm::vec2 p)
{
	if (!Locator::terrainSystem::has_value())
	{
		return false;
	}
	const auto extent = Locator::terrainSystem::value().GetExtent();
	return p.x > std::max(extent.minimum.x, 0.0f) && p.y > std::max(extent.minimum.y, 0.0f) && p.x < extent.maximum.x &&
	       p.y < extent.maximum.y;
}

/// Object::Collide(info.collideType) [inferred]: the sea or a fixed object's footprint
bool Collides(glm::vec2 p)
{
	if (Locator::terrainSystem::value().GetHeightAt(p) <= 0.0f)
	{
		return true;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	for (const auto fixedEntity : Locator::entitiesMap::value().GetFixedInGridCell(CellOf(p)))
	{
		if (const auto* fixed = registry.TryGet<const Fixed>(fixedEntity);
		    fixed != nullptr && glm::distance(fixed->boundingCenter, p) < fixed->boundingRadius)
		{
			return true;
		}
	}
	return false;
}

/// fn_0074F310: uniform in a square of that side around c (half - GameFloatRand(size) per axis)
glm::vec2 SquarePos(glm::vec2 c, float size)
{
	auto& rng = Locator::rng::value();
	const float half = size * 0.5f;
	const float x = size > 0.0f ? rng.NextValue(0.0f, size) : 0.0f;
	const float z = size > 0.0f ? rng.NextValue(0.0f, size) : 0.0f;
	return c + glm::vec2(half - x, half - z);
}

/// Object::IsAvailable [inferred]: still there, not in the hand and not flying
bool Available(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<Transform>(entity) || physics::PhysicsObjects::IsFlying(entity))
	{
		return false;
	}
	const auto* brain = registry.TryGet<const AnimalBrain>(entity);
	return brain == nullptr || static_cast<AnimalState>(brain->topState) != AnimalState::InHand;
}

/// Living::CalcRandomPos (0x5ED080): up to 25 tries in the ring rMin..rMax around c, in bounds and free
glm::vec2 CalcRandomPos(glm::vec2 c, float rMin, float rMax)
{
	auto& rng = Locator::rng::value();
	for (int i = 0; i < 25; ++i)
	{
		const float a = rng.NextValue(0.0f, glm::two_pi<float>());
		const float r = rMin + (rMax > rMin ? rng.NextValue(0.0f, rMax - rMin) : 0.0f);
		const glm::vec2 p = c + r * glm::vec2(std::cos(a), std::sin(a));
		if (InBounds(p) && !Collides(p))
		{
			return p;
		}
	}
	return c;
}

// ---- the flock ----

Flock* FlockOf(const Animal& animal)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(animal.flock) ? registry.TryGet<Flock>(animal.flock) : nullptr;
}

entt::entity LeaderOf(const Flock& flock)
{
	const auto& registry = Locator::entitiesRegistry::value();
	for (const auto member : flock.members)
	{
		if (registry.Valid(member))
		{
			return member;
		}
	}
	return entt::null;
}

/// Flock::GetFlockPos (0x530570): the leader's position, else the domain centre
glm::vec2 FlockPos(const Context& ctx)
{
	const auto* flock = FlockOf(ctx.animal);
	if (flock == nullptr)
	{
		return Xz(ctx.transform);
	}
	const auto leader = LeaderOf(*flock);
	if (leader == entt::null)
	{
		return {flock->domainCentre.x, flock->domainCentre.z};
	}
	return Xz(Locator::entitiesRegistry::value().Get<const Transform>(leader));
}

/// Living::PosWithinDomain (0x5ED010, factor 1.0): no flock, no domain
bool PosWithinDomain(const Context& ctx, glm::vec2 p)
{
	const auto* flock = FlockOf(ctx.animal);
	if (flock == nullptr)
	{
		return true;
	}
	return glm::distance(glm::vec2(flock->domainCentre.x, flock->domainCentre.z), p) <= static_cast<float>(flock->domainRadius);
}

uint16_t FlockDistance(const Context& ctx)
{
	const auto* flock = FlockOf(ctx.animal);
	return flock != nullptr ? flock->flockDistance : static_cast<uint16_t>(ctx.info.flockDistance);
}

uint16_t DomainRadius(const Context& ctx)
{
	const auto* flock = FlockOf(ctx.animal);
	return flock != nullptr ? flock->domainRadius : static_cast<uint16_t>(ctx.info.domainRadius);
}

void LeaveFlock(entt::entity entity, Animal& animal)
{
	if (auto* flock = FlockOf(animal); flock != nullptr)
	{
		auto& members = flock->members;
		members.erase(std::remove(members.begin(), members.end(), entity), members.end());
	}
	animal.flock = entt::null;
}

// ---- states ----

bool ExitAllows(uint8_t from, AnimalState to)
{
	const bool death = to >= AnimalState::SetDying && to <= AnimalState::Downed;
	switch (static_cast<AnimalState>(from))
	{
	case AnimalState::InHand:
		// Living::ExitInHand (0x5ED500): only thrown, landed or dying while held
		return to == AnimalState::Flying || to == AnimalState::Landed || death;
	case AnimalState::Flying:
		// Living::ExitInFlying (0x5ED540): caught, landed or dying
		return to == AnimalState::InHand || to == AnimalState::Landed || death;
	default:
		return true;
	}
}

/// Living::SetTopState (0x5F28E0): the exit filter, the state, TurnsSinceStateChange = 0 and the state's clip
/// (Animal::SetStateSpeed 0x41A2B0 is empty; there are no into / out-of clips)
void SetTopState(entt::entity entity, AnimalBrain& brain, AnimalState state)
{
	if (!ExitAllows(brain.topState, state))
	{
		return;
	}
	static const bool trace = std::getenv("OPENBLACK_ANIMAL_TRACE") != nullptr;
	if (trace)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal {}: state {} -> {}", static_cast<uint32_t>(entity), brain.topState,
		                   static_cast<int>(state));
	}
	brain.topState = static_cast<uint8_t>(state);
	brain.turnsSinceStateChange = 0;
	SetAnimalStateAnim(entity);
}

void SetTopState(Context& ctx, AnimalState state)
{
	SetTopState(ctx.entity, ctx.brain, state);
}

/// Living::PlayAnimThenSetState (0x5ECAC0): WAIT_FOR_ANIMATION with the clip unchanged, then the state
void PlayAnimThenSetState(Context& ctx, AnimalState state)
{
	if (!ExitAllows(ctx.brain.topState, AnimalState::WaitForAnimation))
	{
		return;
	}
	ctx.brain.topState = static_cast<uint8_t>(AnimalState::WaitForAnimation);
	ctx.brain.finalState = static_cast<uint8_t>(state);
	ctx.brain.turnsSinceStateChange = 0;
}

/// Animal::SetSpeed (0x417FE0): MobileWallHug::SetSpeed (0..0xFFFF), then the clip for it without a restart
void SetSpeed(Context& ctx, uint32_t speed)
{
	ctx.brain.speed = static_cast<uint16_t>(std::min<uint32_t>(speed, 0xFFFF));
	SetAnimalAnim(ctx.entity, AnimalAnimId(ctx.entity), false);
}

uint32_t SpeedDefault(const Context& ctx)
{
	return static_cast<uint32_t>(ctx.info.speedGroup.speedDefault);
}

/// Living::SetupMoveToPos (0x5F2830): the info's move state (MOVE_TO_POS) towards p, then `final`
void SetupMoveToPos(Context& ctx, glm::vec2 p, AnimalState final)
{
	ctx.brain.goal = p;
	ctx.brain.finalState = static_cast<uint8_t>(final);
	SetTopState(ctx, static_cast<AnimalState>(static_cast<uint8_t>(ctx.info.moveState)));
}

/// Object::MoveMapObject: one step; true when it entered another 10 m map cell (7) rather than staying in its own (6)
bool MoveBy(Context& ctx, glm::ivec2 step)
{
	const glm::vec2 from = Xz(ctx.transform);
	const glm::vec2 to = from + glm::vec2(step) / k_MapCoordsPerMetre;
	if (!InBounds(to))
	{
		return false;
	}
	ctx.transform.position = glm::vec3(to.x, Locator::terrainSystem::value().GetHeightAt(to), to.y);
	ctx.brain.movedLastTurn += glm::distance(from, to);
	return CellOf(from) != CellOf(to);
}

/// fn_0041A5B0: adds v to out within the speed's budget (the largest axis); true when the budget is used up
bool AddSteer(const AnimalBrain& brain, glm::ivec2& out, glm::ivec2 v)
{
	const int32_t budget = static_cast<int32_t>(brain.speed) - std::max(std::abs(out.x), std::abs(out.y));
	if (budget <= 0)
	{
		return true;
	}
	const int32_t m = std::min(std::max(std::abs(v.x), std::abs(v.y)), budget);
	out.y += Scale(v.y, m, brain.speed);
	out.x += Scale(v.x, m, brain.speed);
	return m == budget;
}

/// fn_0041AD70: the flock's pull (cohesion to the others' centre, the nearest member per axis, its step); true when
/// the budget is used up
bool FlockSteer(const Context& ctx, glm::ivec2& out)
{
	if (static_cast<int32_t>(ctx.brain.speed) - std::max(std::abs(out.x), std::abs(out.y)) <= 0)
	{
		return true;
	}
	const auto* flock = FlockOf(ctx.animal);
	if (flock == nullptr)
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const glm::ivec2 me(Xz(ctx.transform) * k_MapCoordsPerMetre);
	glm::ivec2 sum(0);
	int32_t count = 0;
	int32_t nearestDistance = 0x7FFFFFFF;
	entt::entity nearest = entt::null;
	for (const auto member : flock->members)
	{
		if (member == ctx.entity || !registry.Valid(member) || !registry.AllOf<Transform>(member))
		{
			continue;
		}
		const glm::ivec2 them(Xz(registry.Get<const Transform>(member)) * k_MapCoordsPerMetre);
		// fn_0074D090 [inferred |dx| + |dz|]
		const int32_t d = std::abs(them.x - me.x) + std::abs(them.y - me.y);
		if (d < nearestDistance)
		{
			nearestDistance = d;
			nearest = member;
		}
		sum += them;
		++count;
	}
	if (nearest == entt::null)
	{
		return false;
	}
	const int32_t speed = ctx.brain.speed;
	// cohesion: a fifth of the speed towards the others' centre
	const auto toCentre = Step(AngleOf(glm::vec2(sum / count - me)), ctx.brain.speed);
	glm::ivec2 v(Scale(toCentre.x, speed / 5, speed), Scale(toCentre.y, speed / 5, speed));
	if (AddSteer(ctx.brain, out, v))
	{
		return true;
	}
	// the nearest member, per axis: closer than FlockDistance away, farther towards it. The distance is compared with raw
	// MapCoords (6553.6 per metre), so in practice it always pulls; and the cohesion vector goes in a second time.
	const glm::ivec2 them(Xz(registry.Get<const Transform>(nearest)) * k_MapCoordsPerMetre);
	const glm::ivec2 d = them - me;
	const auto flockDistance = static_cast<int32_t>(FlockDistance(ctx));
	for (int axis = 0; axis < 2; ++axis)
	{
		const int32_t sign = d[axis] < 0 ? -1 : 1;
		if (std::abs(d[axis]) > flockDistance)
		{
			v[axis] += sign * ((speed / 5 * 8) / 5);
		}
		else if (std::abs(d[axis]) < flockDistance)
		{
			v[axis] += 2 * sign * -(speed / 5);
		}
	}
	if (AddSteer(ctx.brain, out, v))
	{
		return true;
	}
	// alignment: three fifths of the nearest member's step
	const auto* other = registry.TryGet<const AnimalBrain>(nearest);
	const glm::ivec2 theirStep = other != nullptr ? other->step : glm::ivec2(0);
	return AddSteer(ctx.brain, out, {Scale(theirStep.x, speed * 3 / 5, speed), Scale(theirStep.y, speed * 3 / 5, speed)});
}

/// Animal::SetNewWander (0x41A3F0): the new straight step (towards / away from c, the flock, a random turn)
void SetNewWander(Context& ctx, glm::vec2 c, float rMin, float rMax)
{
	glm::ivec2 out(0);
	const glm::vec2 me = Xz(ctx.transform);
	const float d = glm::distance(c, me);
	if (d > rMax || d < rMin)
	{
		const auto a = AngleOf(d > rMax ? c - me : me - c);
		if (a != 0)
		{
			const auto full = Step(a, ctx.brain.speed);
			AddSteer(ctx.brain, out, {Scale(full.x, 9, 10), Scale(full.y, 9, 10)});
		}
	}
	if (!FlockSteer(ctx, out))
	{
		const auto turn = static_cast<int32_t>(ctx.info.turnAngle);
		const int32_t random = turn > 0 ? static_cast<int32_t>(Locator::rng::value().NextValue<uint32_t>(0, turn - 1)) : 0;
		const auto a = static_cast<uint16_t>((ctx.brain.angle - turn / 2 + random) & 0x7FF);
		AddSteer(ctx.brain, out, Step(a, ctx.brain.speed));
	}
	ctx.brain.step = out;
	// fn_0060C000: the heading follows the step at once
	if (out != glm::ivec2(0))
	{
		ctx.brain.angle = AngleOf(glm::vec2(out));
		FaceAngle(ctx.transform, ctx.brain.angle);
	}
}

/// Animal::SetTowardsAngle (0x418560): turns at most turnAngle a turn, less inside its turning circle
void SetTowardsAngle(Context& ctx, uint16_t target, float distance)
{
	const int32_t diff = AngleDiff(ctx.brain.angle, target);
	const auto turnAngle = static_cast<int32_t>(ctx.info.turnAngle);
	int32_t turn = std::min(std::abs(diff), turnAngle);
	if (std::abs(diff) > turnAngle && turnAngle > 0)
	{
		const float radius = 2.0f * Metres(ctx.brain.speed) / (static_cast<float>(turnAngle) * glm::two_pi<float>() / k_Circle);
		if (distance < radius)
		{
			turn = static_cast<int32_t>(static_cast<float>(turnAngle) * (1.0f - distance / radius));
		}
	}
	ctx.brain.angle = static_cast<uint16_t>((ctx.brain.angle + (diff < 0 ? -turn : turn)) & 0x7FF);
}

// ---- needs ----

bool IsLeader(const Context& ctx)
{
	const auto* flock = FlockOf(ctx.animal);
	return flock != nullptr && LeaderOf(*flock) == ctx.entity;
}

/// Animal::ProcessNeeds (0x417DC0)
void ProcessNeeds(Context& ctx)
{
	auto* flock = FlockOf(ctx.animal);
	if (ctx.info.needToBreed != 0 && flock != nullptr && flock->members.size() >= 2 && flock->members.size() < flock->maxMembers &&
	    ctx.brain.breed < static_cast<int32_t>(ctx.info.needToBreed) && ctx.animal.age >= ctx.info.grownUpAge)
	{
		++ctx.brain.breed;
	}
	if (ctx.info.hunger != 0 && ctx.brain.hunger < static_cast<int32_t>(ctx.info.hunger))
	{
		++ctx.brain.hunger;
	}
	if (ctx.info.sleep != 0 && ctx.brain.sleep < static_cast<int32_t>(ctx.info.sleep))
	{
		++ctx.brain.sleep;
	}
	// the leader counts its turns when no shepherd guards the flock (openblack has no shepherds)
	if (flock != nullptr && IsLeader(ctx))
	{
		++flock->leaderTurns;
	}
}

/// Cow::LookForFoodPos (0x41D440) -> Animal::LookForGrazePos (0x41A8B0) / FindGrazingPosition (0x41A980): the first
/// free cell of a spiral of (domainRadius / 10)^2 map cells around it, ahead of it, not its own and no member's
bool LookForFoodPos(const Context& ctx, glm::vec2& out)
{
	auto& registry = Locator::entitiesRegistry::value();
	const glm::vec2 me = Xz(ctx.transform);
	const auto myCell = CellOf(me);
	const auto* flock = FlockOf(ctx.animal);
	const int cells = (DomainRadius(ctx) / 10) * (DomainRadius(ctx) / 10);
	// the square spiral from its own cell [inferred order]
	int x = 0;
	int y = 0;
	int dx = 0;
	int dy = -1;
	for (int i = 0; i < cells; ++i)
	{
		const glm::vec2 c = me + 10.0f * glm::vec2(static_cast<float>(x), static_cast<float>(y));
		const auto cell = CellOf(c);
		bool ok = cell != myCell && PosWithinDomain(ctx, c) && InBounds(c);
		// fn_00418CD0: within viewAngle / 2 of its heading
		ok = ok && std::abs(AngleDiff(ctx.brain.angle, AngleOf(c - me))) <= static_cast<int32_t>(ctx.info.viewAngle) / 2;
		// fn_00419980: no other member of its flock stands there or goes there
		if (ok && flock != nullptr)
		{
			for (const auto member : flock->members)
			{
				if (member == ctx.entity || !registry.Valid(member))
				{
					continue;
				}
				const auto* brain = registry.TryGet<const AnimalBrain>(member);
				if (CellOf(Xz(registry.Get<const Transform>(member))) == cell ||
				    (brain != nullptr && brain->goal != glm::vec2(0.0f) && CellOf(brain->goal) == cell))
				{
					ok = false;
					break;
				}
			}
		}
		if (ok && !Collides(c))
		{
			out = c;
			return true;
		}
		if (x == y || (x < 0 && x == -y) || (x > 0 && x == 1 - y))
		{
			const int t = dx;
			dx = -dy;
			dy = t;
		}
		x += dx;
		y += dy;
	}
	return false;
}

/// Animal::CheckNeeds (0x418450): 3 ready to breed (else the breed counter restarts when the flock is full), 1 hungry,
/// 2 sleepy, 0 none
int CheckNeeds(Context& ctx)
{
	if (ctx.info.needToBreed != 0 && ctx.brain.breed >= static_cast<int32_t>(ctx.info.needToBreed))
	{
		const auto* flock = FlockOf(ctx.animal);
		if (flock != nullptr && flock->maxMembers > flock->members.size())
		{
			return 3;
		}
		ctx.brain.breed = 0;
	}
	if (ctx.info.hunger != 0 && ctx.brain.hunger >= static_cast<int32_t>(ctx.info.hunger))
	{
		return 1;
	}
	if (ctx.info.sleep != 0 && ctx.brain.sleep >= static_cast<int32_t>(ctx.info.sleep))
	{
		return 2;
	}
	return 0;
}

/// Cow::ReactToAnimalNeeds (0x41D310): breeding, then hunger, then sleep
int CowReactToAnimalNeeds(Context& ctx)
{
	const auto* flock = FlockOf(ctx.animal);
	if (ctx.info.needToBreed != 0 && ctx.animal.age >= ctx.info.grownUpAge)
	{
		// fn_00418220 again (ProcessNeeds counts it too)
		if (flock != nullptr && flock->members.size() >= 2 && flock->members.size() < flock->maxMembers &&
		    ctx.brain.breed < static_cast<int32_t>(ctx.info.needToBreed))
		{
			++ctx.brain.breed;
		}
		if (ctx.brain.breed >= static_cast<int32_t>(ctx.info.needToBreed))
		{
			ctx.brain.breed = 0;
			if (flock != nullptr && flock->maxMembers > flock->members.size())
			{
				SetTopState(ctx, AnimalState::GivesBirth);
				return k_Started;
			}
		}
	}
	if (ctx.info.hunger != 0 && ctx.brain.hunger >= static_cast<int32_t>(ctx.info.hunger))
	{
		glm::vec2 p;
		if (LookForFoodPos(ctx, p))
		{
			SetSpeed(ctx, SpeedDefault(ctx));
			SetupMoveToPos(ctx, p, AnimalState::StartToEat);
			return k_Started;
		}
		// no food found: the sleep test is skipped
		return k_Nothing;
	}
	if (ctx.info.sleep != 0 && ctx.brain.sleep >= static_cast<int32_t>(ctx.info.sleep) && ctx.brain.sleepCell != glm::u16vec2(0))
	{
		SetTopState(ctx, AnimalState::SeekSleep);
		return k_Started;
	}
	return k_Nothing;
}

/// vt+0xBBC: the grazers' (Cow) or the predators' (Animal / Wolf) ReactToAnimalNeeds
int ReactToAnimalNeeds(Context& ctx)
{
	return HunterOf(ctx.animal.type) != Hunter::None ? PredatorReactToAnimalNeeds(ctx) : CowReactToAnimalNeeds(ctx);
}

/// Living::KeepLeaderWithinDomain (0x41AAD0): the leader takes the herd to a new place every stayTime turns
int KeepLeaderWithinDomain(Context& ctx)
{
	auto* flock = FlockOf(ctx.animal);
	if (flock == nullptr || !IsLeader(ctx))
	{
		return 0;
	}
	if (!PosWithinDomain(ctx, FlockPos(ctx)) || flock->leaderTurns >= ctx.info.stayTime)
	{
		const auto p = CalcRandomPos({flock->domainCentre.x, flock->domainCentre.z}, static_cast<float>(ctx.info.domainInnerRadius),
		                             static_cast<float>(flock->domainRadius));
		SetupMoveToPos(ctx, p, AnimalState::DecideWhatToDo);
		flock->leaderTurns = 0;
		return k_Started;
	}
	return 0;
}

/// Living::KeepFlockMemberWithinFlockArea (0x41ABB0): back towards the leader when too far from it
int KeepFlockMemberWithinFlockArea(Context& ctx)
{
	const glm::vec2 me = Xz(ctx.transform);
	const glm::vec2 leader = FlockPos(ctx);
	const auto flockDistance = static_cast<float>(FlockDistance(ctx));
	if (PosWithinDomain(ctx, me) && glm::distance(leader, me) <= flockDistance)
	{
		return 1;
	}
	const auto p = CalcRandomPos(leader, 0.0f, flockDistance);
	if (PosWithinDomain(ctx, p) || !PosWithinDomain(ctx, leader))
	{
		SetupMoveToPos(ctx, p, AnimalState::DecideWhatToDo);
	}
	return k_Started;
}

// ---- the state functions ----

/// Animal::StartWander (0x417C90)
void StartWander(Context& ctx)
{
	ctx.brain.step = Step(ctx.brain.angle, ctx.brain.speed);
	SetSpeed(ctx, SpeedDefault(ctx));
	SetTopState(ctx, AnimalState::Wander);
	const auto* flock = FlockOf(ctx.animal);
	SetNewWander(ctx, FlockPos(ctx), static_cast<float>(ctx.info.domainInnerRadius),
	             static_cast<float>(flock != nullptr ? flock->domainRadius : ctx.info.domainRadius));
}

/// Cow::DecideWhatToDo (0x41D1B0)
void DecideWhatToDo(Context& ctx)
{
	if (HunterOf(ctx.animal.type) != Hunter::None)
	{
		PredatorDecideWhatToDo(ctx);
		return;
	}
	if (!IsGrazer(ctx.animal.type))
	{
		return;
	}
	// Animal::CheckNeeds (0x418450) == 3: ready to breed
	if (CheckNeeds(ctx) == 3)
	{
		ctx.brain.breed = 0;
		SetTopState(ctx, AnimalState::GivesBirth);
		return;
	}
	// LookForFlocksInSpiral with the info's flocksCanMerge: 0 for every grazer, nothing
	if (FlockOf(ctx.animal) != nullptr)
	{
		if (KeepLeaderWithinDomain(ctx) == k_Started)
		{
			return;
		}
		if (KeepFlockMemberWithinFlockArea(ctx) == k_Started)
		{
			return;
		}
	}
	if (ReactToAnimalNeeds(ctx) != k_Started)
	{
		SetTopState(ctx, AnimalState::StartWander);
	}
}

/// Cow::Wander (0x41D280): a straight line, re-steered in every new map cell
void Wander(Context& ctx)
{
	if (ReactToAnimalNeeds(ctx) == k_Started)
	{
		return;
	}
	if (!PosWithinDomain(ctx, Xz(ctx.transform)))
	{
		SetTopState(ctx, AnimalState::DecideWhatToDo);
		return;
	}
	// fn_0060BD00
	if (MoveBy(ctx, ctx.brain.step))
	{
		SetNewWander(ctx, FlockPos(ctx), 0.0f, static_cast<float>(FlockDistance(ctx)));
	}
}

/// Animal::MoveToPos (0x41BAF0) -> Living::MoveToPos (0x5EC270) with MobileWallHug::MoveTo. openblack's animals go
/// straight at the goal, turning like SetTowardsAngle (the wall hug round obstacles is not ported for them).
void MoveToPos(Context& ctx)
{
	const glm::vec2 me = Xz(ctx.transform);
	const glm::vec2 d = ctx.brain.goal - me;
	const float distance = glm::length(d);
	const float stepLength = Metres(ctx.brain.speed);
	// openblack's safety: a goal it keeps circling (the turning limit) is reached after 100 s
	if (distance <= stepLength || ctx.brain.turnsSinceStateChange > 1000)
	{
		if (InBounds(ctx.brain.goal))
		{
			ctx.transform.position = glm::vec3(ctx.brain.goal.x, Locator::terrainSystem::value().GetHeightAt(ctx.brain.goal), ctx.brain.goal.y);
			ctx.brain.movedLastTurn += distance;
		}
		// SetTopStateToFinal (0x5ECA80)
		SetTopState(ctx, static_cast<AnimalState>(ctx.brain.finalState));
		return;
	}
	SetTowardsAngle(ctx, AngleOf(d), distance);
	ctx.brain.step = Step(ctx.brain.angle, ctx.brain.speed);
	FaceAngle(ctx.transform, ctx.brain.angle);
	MoveBy(ctx, ctx.brain.step);
}

/// Animal::StartToEat (0x418280): 15..24 eat clips; the grazers' Cow::StartToEat (0x41D4A0) then makes it 20..34
void StartToEat(Context& ctx)
{
	auto& rng = Locator::rng::value();
	ctx.brain.counter = static_cast<int16_t>(rng.NextValue<uint32_t>(0, 9) + 15);
	if (IsGrazer(ctx.animal.type))
	{
		ctx.brain.counter = static_cast<int16_t>(rng.NextValue<uint32_t>(0, 14) + 20);
	}
	SetSpeed(ctx, SpeedDefault(ctx));
	PlayAnimThenSetState(ctx, AnimalState::Eat);
}

/// Animal::Eat (0x4182D0)
void Eat(Context& ctx)
{
	if (--ctx.brain.counter == 0)
	{
		PlayAnimThenSetState(ctx, AnimalState::FinishEating);
		ctx.brain.hunger = 0;
		ctx.brain.foodTarget = entt::null;
	}
	else
	{
		PlayAnimThenSetState(ctx, AnimalState::Eat);
	}
	// Lion::Eat (0x41FE40), every predator: its prey is gone (also once the meal is over: then the up-from-eat clip
	// never shows, straight to DECIDE)
	if (HunterOf(ctx.animal.type) != Hunter::None && !Available(ctx.brain.foodTarget))
	{
		ctx.brain.counter = 0;
		SetTopState(ctx, AnimalState::DecideWhatToDo);
	}
}

/// Animal::SeekSleep (0x4180D0): to a spot in a square around its sleep cell, 2 m per flock member (fn_0074F310)
void SeekSleep(Context& ctx)
{
	if (ctx.brain.sleepCell == glm::u16vec2(0))
	{
		SetTopState(ctx, AnimalState::StartWander);
		return;
	}
	const auto* flock = FlockOf(ctx.animal);
	const float size = 2.0f * static_cast<float>(flock != nullptr ? flock->members.size() : 1);
	const auto centre = MapInterface::GetCellCenter(ctx.brain.sleepCell);
	SetupMoveToPos(ctx, SquarePos(centre, size), AnimalState::Sleeps);
}

/// Animal::Sleeps (0x418330): the sleep counter runs down by 2 a turn (ProcessNeeds adds 1)
void Sleeps(Context& ctx)
{
	ctx.brain.sleep = static_cast<int16_t>(ctx.brain.sleep - 2);
	if (ctx.brain.sleep <= 0)
	{
		SetTopState(ctx, AnimalState::StartWander);
		ctx.brain.sleep = 0;
	}
}

/// Animal::GivesBirth (0x418230): a young one (age 1) of its kind joins the flock
void GivesBirth(Context& ctx)
{
	const auto position = ctx.transform.position;
	const auto type = ctx.animal.type;
	const auto town = ctx.animal.town;
	const auto flock = ctx.animal.flock;
	SetTopState(ctx, AnimalState::StartWander);
	archetypes::AnimalArchetype::Create(position, type, town, flock, 1);
}

/// Living::SetDying (0x5EC390): nothing while it flies
void SetDying(entt::entity entity, AnimalBrain& brain)
{
	if (physics::PhysicsObjects::IsFlying(entity))
	{
		return;
	}
	if ((brain.status & 1) == 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		(registry.AllOf<Life>(entity) ? registry.Get<Life>(entity) : registry.Assign<Life>(entity)).value = 0.0f;
		SetTopState(entity, brain, AnimalState::Dying);
		brain.status |= 0x31;
	}
	brain.counter = k_TurnsToDieOver;
}

/// Living::StateDead (0x5EC400): off the flock; the corpse lies 600 turns, then (CreateSmokyStuff, not done) goes
bool Dead(Context& ctx)
{
	LeaveFlock(ctx.entity, ctx.animal);
	return ctx.brain.counter-- == 0;
}

/// Animal::ProcessState (0x417EE0) and the state table; true when the animal is to be deleted
bool ProcessState(Context& ctx)
{
	++ctx.brain.turnsSinceStateChange;
	ctx.brain.movedLastTurn = 0.0f;
	// Living::ProcessReaction (0x5F1270): the flight from a predator ends after its turns or when it has gone
	ProcessReaction(ctx);
	if (StateInfo(ctx.brain.topState).field0xa4 != 0)
	{
		ProcessNeeds(ctx);
	}
	// what it was eating has gone
	if (ctx.brain.foodTarget != entt::null && !Available(ctx.brain.foodTarget))
	{
		ctx.brain.foodTarget = entt::null;
		ctx.brain.counter = 0;
		SetTopState(ctx, AnimalState::DecideWhatToDo);
		return false;
	}
	const bool walker = IsGrazer(ctx.animal.type) || HunterOf(ctx.animal.type) != Hunter::None;
	switch (static_cast<AnimalState>(ctx.brain.topState))
	{
	case AnimalState::MoveToPos:
		MoveToPos(ctx);
		break;
	case AnimalState::Landed:
		// Animal::Landed (0x417D50): CalculeLairPos, the flock now centres where it landed (the predators: their lair)
		if (HunterOf(ctx.animal.type) != Hunter::None)
		{
			CalculeLairPos(ctx);
		}
		else if (auto* flock = FlockOf(ctx.animal); flock != nullptr)
		{
			flock->domainCentre = ctx.transform.position;
		}
		PlayAnimThenSetState(ctx, AnimalState::InteractDecideWhatToDo);
		break;
	case AnimalState::SetDying:
		SetDying(ctx.entity, ctx.brain);
		break;
	case AnimalState::Dying:
	case AnimalState::Drowning:
		PlayAnimThenSetState(ctx, AnimalState::Dead);
		break;
	case AnimalState::Dead:
		return Dead(ctx);
	case AnimalState::WaitForAnimation:
		// Living::WaitForAnimation (0x5EC990): turns x 100 ms >= the clip's length
		if (VillagerAnimationDone(ctx.entity, ctx.brain.turnsSinceStateChange))
		{
			SetTopState(ctx, static_cast<AnimalState>(ctx.brain.finalState));
		}
		break;
	case AnimalState::MoveInFlock:
	case AnimalState::StartWander:
		if (walker)
		{
			StartWander(ctx);
		}
		break;
	case AnimalState::HuntingMoveToPos:
		HuntingMoveToPos(ctx);
		break;
	case AnimalState::TargetPounce:
		TargetPounce(ctx);
		break;
	case AnimalState::Downed:
		// Living::Downed (0x5EC4B0): the Dying clip, then being eaten for 300 turns
		PlayAnimThenSetState(ctx, AnimalState::BeingEaten);
		ctx.brain.counter = 300;
		break;
	case AnimalState::BeingEaten:
		BeingEaten(ctx);
		break;
	case AnimalState::HideInLair:
		HideInLair(ctx);
		break;
	case AnimalState::FleeingFromPredatorReaction:
		FleeingFromPredatorReaction(ctx);
		break;
	case AnimalState::FleeingFromObjectReaction:
		FleeingFromObjectReaction(ctx);
		break;
	case AnimalState::FleeingAndLookingAtObjectReaction:
		FleeingAndLookingReaction(ctx);
		break;
	case AnimalState::Wander:
		Wander(ctx);
		break;
	case AnimalState::Eat:
		Eat(ctx);
		break;
	case AnimalState::SeekSleep:
		SeekSleep(ctx);
		break;
	case AnimalState::Sleeps:
		Sleeps(ctx);
		break;
	case AnimalState::StartToEat:
		StartToEat(ctx);
		break;
	case AnimalState::FinishEating:
		PlayAnimThenSetState(ctx, AnimalState::DecideWhatToDo);
		break;
	case AnimalState::DecideWhatToDo:
		DecideWhatToDo(ctx);
		break;
	case AnimalState::InteractDecideWhatToDo:
		// LookForFlocksInSpiral(merge = 1) (flock merging not done), then StartWander
		if (walker)
		{
			StartWander(ctx);
		}
		else
		{
			SetTopState(ctx, AnimalState::DecideWhatToDo);
		}
		break;
	case AnimalState::GivesBirth:
		GivesBirth(ctx);
		break;
	default:
		// IN_HAND, FLYING and the states grazers never reach do nothing
		break;
	}
	return false;
}

/// Animal::Animal (0x416EB0) and Living::Living (0x5EBEC0): counters 0, speedDefault, DECIDE_WHAT_TO_DO, the life
/// of info.dat; the sleep place is the flock's domain centre cell at creation (fn_005E18E0)
AnimalBrain& Initialise(entt::entity entity, const Animal& animal, const Transform& transform)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = InfoOf(animal);
	auto& brain = registry.Assign<AnimalBrain>(entity);
	brain.speed = static_cast<uint16_t>(std::min<uint32_t>(static_cast<uint32_t>(info.speedGroup.speedDefault), 0xFFFF));
	brain.angle = AngleOfRotation(transform.rotation);
	if (const auto* flock = FlockOf(animal); flock != nullptr)
	{
		brain.sleepCell = CellOf({flock->domainCentre.x, flock->domainCentre.z});
	}
	if (!registry.AllOf<Life>(entity))
	{
		registry.Assign<Life>(entity).value = info.life;
	}
	return brain;
}

void Delete(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* animal = registry.TryGet<Animal>(entity); animal != nullptr)
	{
		LeaveFlock(entity, *animal);
	}
	physics::PhysicsObjects::RemoveObject(entity);
	registry.Destroy(entity);
	registry.SetDirty();
}
} // namespace detail

using namespace detail;

void ProcessAnimalsTurn(float visualTime)
{
	if (!Locator::infoConstants::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	g_VisualTime = visualTime;
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> animals;
	registry.Each<const Animal, const Transform>(
	    [&animals](entt::entity entity, const Animal&, const Transform&) { animals.push_back(entity); });
	std::vector<entt::entity> gone;
	for (const auto entity : animals)
	{
		if (!registry.Valid(entity))
		{
			continue;
		}
		auto& animal = registry.Get<Animal>(entity);
		auto& transform = registry.Get<Transform>(entity);
		auto* brain = registry.TryGet<AnimalBrain>(entity);
		const bool fresh = brain == nullptr;
		if (fresh)
		{
			brain = &Initialise(entity, animal, transform);
		}
		Context ctx {entity, animal, *brain, transform, InfoOf(animal)};
		// fn_00419C20: a predator made without a flock gets its lair (CalculeLairPos), which is also its sleep place
		if (const auto* flock = FlockOf(animal);
		    fresh && flock != nullptr && flock->id < 0 && flock->members.size() == 1 && HunterOf(animal.type) != Hunter::None)
		{
			CalculeLairPos(ctx);
			brain->sleepCell = CellOf({flock->domainCentre.x, flock->domainCentre.z});
		}
		if (ProcessState(ctx))
		{
			gone.push_back(entity);
		}
	}
	for (const auto entity : gone)
	{
		Delete(entity);
	}
	RunDebugHooks(g_Turn++);
}

AnimalState TopState(entt::entity entity)
{
	const auto* brain = Locator::entitiesRegistry::value().TryGet<const AnimalBrain>(entity);
	return brain != nullptr ? static_cast<AnimalState>(brain->topState) : AnimalState::DecideWhatToDo;
}

uint16_t LandType(entt::entity entity)
{
	const auto* brain = Locator::entitiesRegistry::value().TryGet<const AnimalBrain>(entity);
	return brain != nullptr ? (brain->status >> 4) & 3 : 3;
}

bool ValidForPlaceInHand(entt::entity entity)
{
	const auto* animal = Locator::entitiesRegistry::value().TryGet<const Animal>(entity);
	return animal == nullptr || !Locator::infoConstants::has_value() || InfoOf(*animal).playerCanPickUp != 0;
}

namespace detail
{
AnimalBrain* BrainOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<Animal, Transform>(entity) || !Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	if (auto* brain = registry.TryGet<AnimalBrain>(entity); brain != nullptr)
	{
		return brain;
	}
	return &Initialise(entity, registry.Get<const Animal>(entity), registry.Get<const Transform>(entity));
}
} // namespace detail

void PlaceInHand(entt::entity entity)
{
	auto* brain = BrainOf(entity);
	if (brain == nullptr)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& animal = registry.Get<Animal>(entity);
	// Flock::SeperateLivingIntoNewFlock (0x52FE10): a flock of its own with the old one's domain and distances,
	// centred where it was picked up [inferred details]
	if (auto* old = FlockOf(animal); old != nullptr && old->members.size() > 1)
	{
		const Flock copy = *old;
		LeaveFlock(entity, animal);
		const auto flockEntity = registry.Create();
		auto& flock = registry.Assign<Flock>(flockEntity);
		flock.domainCentre = registry.Get<const Transform>(entity).position;
		flock.savedDomainCentre = flock.domainCentre;
		flock.domainRadius = copy.domainRadius;
		flock.flockDistance = copy.flockDistance;
		flock.town = copy.town;
		flock.members.push_back(entity);
		flock.maxMembers = 1;
		animal.flock = flockEntity;
	}
	SetTopState(entity, *brain, AnimalState::InHand);
	SnapDrawPosition(entity);
}

void InitialisePhysics(entt::entity entity)
{
	if (auto* brain = BrainOf(entity); brain != nullptr)
	{
		SetTopState(entity, *brain, AnimalState::Flying);
		SnapDrawPosition(entity);
	}
}

void EndPhysics(entt::entity entity, const glm::mat3& rotation)
{
	auto* brain = BrainOf(entity);
	if (brain == nullptr)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// the landType from the body's right row y (po+0xD8): on its right side, its left side or its feet
	const float right = rotation[0].y;
	const uint16_t landType = right > 0.5f ? 1 : (right < -0.5f ? 2 : 0);
	// the heading of the body's forward row. The original adds pi to GetYAngle of that row; openblack builds the body
	// from the drawn rotation, so the drawn yaw is kept as it is.
	brain->angle = AngleOfRotation(rotation);
	brain->step = glm::ivec2(0);
	auto& transform = registry.Get<Transform>(entity);
	FaceAngle(transform, brain->angle);
	// Object::EndPhysics: coords = Pos, no slide from where it was
	SnapDrawPosition(entity);
	brain->status = static_cast<uint16_t>((brain->status & ~0x30) | (landType << 4));
	static const bool trace = std::getenv("OPENBLACK_ANIMAL_TRACE") != nullptr;
	if (trace)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal {}: end of physics, landType {} (right.y {:.2f})", static_cast<uint32_t>(entity),
		                   landType, right);
	}
	const auto* life = registry.TryGet<const Life>(entity);
	if (life == nullptr || life->value > 0.0f)
	{
		SetTopState(entity, *brain, AnimalState::Landed);
		return;
	}
	const bool wasDying = (brain->status & 1) != 0;
	if (!wasDying)
	{
		SetTopState(entity, *brain, AnimalState::Dying);
		brain->status |= 1;
	}
	brain->counter = k_TurnsToDieOver;
	brain->status = static_cast<uint16_t>((brain->status & ~0x30) | (landType << 4));
	if (wasDying)
	{
		// a thrown corpse lies dead again
		SetTopState(entity, *brain, AnimalState::Dead);
	}
}

void PutDown(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<Animal, Transform>(entity))
	{
		return;
	}
	auto& transform = registry.Get<Transform>(entity);
	const float yaw = std::atan2(transform.rotation[2].x, transform.rotation[2].z);
	transform.rotation = glm::mat3(glm::eulerAngleY(yaw));
	EndPhysics(entity, transform.rotation);
}

void DestroyedByEffect(entt::entity entity)
{
	if (auto* brain = BrainOf(entity); brain != nullptr)
	{
		SetDying(entity, *brain);
	}
}

void Forget(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* animal = registry.TryGet<Animal>(entity); animal != nullptr)
	{
		LeaveFlock(entity, *animal);
	}
}

} // namespace openblack::ecs::animal_ai
