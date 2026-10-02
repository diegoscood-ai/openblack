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
#include <array>
#include <limits>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include <LNDFile.h>

#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Common/RandomNumberManager.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/AnimalAnimations.h"
#include "ECS/AnimalWallHug.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Map.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/SeaCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/SmokyStuff.h"
#include "ECS/VillagerAnimations.h"
#include "InfoConstants.h"
#include "GameClock.h"
#include "Locator.h"

namespace openblack::ecs::animal_ai
{
using components::Animal;
using components::AnimalBrain;
using components::Fixed;
using components::Flock;
using components::Life;
using components::LivingAction;
using components::Mesh;
using components::Villager;
using components::Transform;

namespace
{
/// GRand::GameFloatRand 0x6DE530 -> fn_005106B0: 0 for max == 0 (`fcomp 0; test ah, 0x40`), else float(LHRand(0xFFFF))
/// x max x 1/65535 ([0x8D6050] = 0x37800080) (0x510710..0x510736), so in [0, max] for a negative max too.
/// (aproximado) LHRand 0x7DB600 on g_game +0x205A30 is openblack's generator here: uniform in 0..0xFFFE, as the `div`
/// by 0xFFFF
float GameFloatRand(float max)
{
	if (max == 0.0f)
	{
		return 0.0f;
	}
	constexpr float k_InvFFFF = 1.0f / 65535.0f; // [0x8D6050] = 0x37800080
	const auto random = Locator::rng::value().NextValue<uint32_t>(0, 0xFFFE);
	return static_cast<float>(random) * max * k_InvFFFF;
}
} // namespace

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
uint32_t Turn()
{
	return game_clock::Turn(); // g_game +0x205A40, the reactions' clock too
}

// ---- MapCoords and the angle tables ----

/// COS / SIN tables 0xC31E14 / 0xC31614 (gutils::Cos / Sin): 2048 entries per circle, 65536 = 1
int32_t Cos(uint16_t a)
{
	return gutils::Cos(a);
}
int32_t Sin(uint16_t a)
{
	return gutils::Sin(a);
}

/// the step of that speed along the angle: ((speed >> 4) * COS[a]) >> 12, MapCoords per turn (fn_0074D3A0 / 0x74D3C0,
/// gutils::StepFromAngle)
glm::ivec2 Step(uint16_t angle, uint32_t speed)
{
	return gutils::StepFromAngle(angle, static_cast<int32_t>(speed));
}

/// GUtils::GetAngleFromDXDZ 0x74D200 = LHArcTan 0x74D0C0 (gutils::GetAngleFromDXDZ)
uint16_t AngleOfMapCoords(int32_t dx, int32_t dz)
{
	return gutils::GetAngleFromDXDZ(dx, dz);
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
	return MapInterface::GetGridCell(p); // map_coords::CellOf: off the map (>= 512, 0xFFFF when negative) stays off it
}

/// the drawn rotation of a mobile heading along the angle (as PathfindingSystem's InitializeStep)
void FaceAngle(Transform& transform, uint16_t angle)
{
	const float theta = static_cast<float>(angle) * glm::two_pi<float>() / k_Circle;
	transform.rotation = lh_matrix::AngleY(theta + glm::half_pi<float>()); // the "Scawen" angle
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
	// MapCoords::InBounds (0x6042C0): the cell's unsigned high words inside the map, not the land's extent
	return sea_cells::InBounds(glm::vec3(p.x, 0.0f, p.y));
}

/// Object::Collide(info.collideType) [inferred]: the sea or a fixed object's footprint
bool Collides(glm::vec2 p, uint32_t collideType)
{
	// MapCoords::Collide (0x6033C0): no map cell -> everything; else MapCell::Collide 0x601BD0 (ECS/SeaCells: 0x10 off
	// the game map, 1 water, 2 land) & collideType. Fixed objects' own footprints do not count; the object bits (fields,
	// forest trees) are not in openblack's map cells yet
	if (!InBounds(p))
	{
		return collideType != 0;
	}
	return (sea_cells::CollideLandscape(glm::vec3(p.x, 0.0f, p.y)) & collideType) != 0;
}

/// fn_0074F310: uniform in a square of that side around c (half - GameFloatRand(size) per axis, 0x74F346 / 0x74F35E)
glm::vec2 SquarePos(glm::vec2 c, float size)
{
	const float half = size * 0.5f; // [0x8AA3B4]
	const float x = GameFloatRand(size);
	const float z = GameFloatRand(size);
	return c + glm::vec2(half - x, half - z);
}

/// GameThing::IsAvailable (0x401810): only "not being deleted" (held or flying animals are available);
/// Villager::IsAvailable (0x751D50): and not DYING
bool Available(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<Transform>(entity))
	{
		return false;
	}
	if (registry.AllOf<Villager>(entity))
	{
		const auto* action = registry.TryGet<const LivingAction>(entity);
		return action == nullptr || static_cast<VillagerStates>(action->states[0]) != VillagerStates::Dying;
	}
	return true;
}

/// Dove::IsPosValidForMapCellExistance (0x41F840) [the tie rules not read]: a land block under the point
bool IsPosValidForMapCellExistance(const Context& ctx, glm::vec2 p)
{
	if (!IsBird(ctx.animal.type))
	{
		return true; // Living: 1
	}
	const auto cellCoords = glm::u16vec2(glm::max(p, glm::vec2(0.0f)) / 10.0f);
	const auto& cell = Locator::terrainSystem::value().GetCell(cellCoords);
	return !(cell.properties.fullWater && cell.r == 0 && cell.g == 0 && cell.b == 0 && cell.properties.country == 0);
}

bool IsPosValidForTurnAngle(const Context& ctx, glm::vec2 p)
{
	const float turn = gutils::ConvertGameAngleTo3D(static_cast<int32_t>(ctx.info.turnAngle)); // 0x41B229
	// 0x41B229..0x41B246: R = ConvertWholeDistanceToMeters(ftol(2 x speed (+0x5A, fild; fadd st0, st0) / turn)), the
	// speed in MapCoords per turn and turn = ConvertGameAngleTo3D(turnAngle). No test of the turn: with 0 the quotient
	// is inf (or NaN with no speed), __ftol gives 0x80000000, R = -327680 m and both distances below exceed it (true)
	const float twice = static_cast<float>(ctx.brain.speed) * 2.0f;
	const float radius = gutils::ConvertWholeDistanceToMeters(map_coords::FtoL(twice / turn));
	// 0x41B24F..0x41B27B and 0x41B29D..0x41B2C4: the two centres me + fn_0074D6A0(+0x5C +- 0x200, R) (MapCoords, the
	// `sar 4` of 0x74D3A0 drops the low 4 bits of R), then GetDistanceInMetres 0x74CD70(p, centre) > R for both
	// (`fcomp; test ah, 0x41; jne` -> 0)
	const auto me = map_coords::FromMetres(Xz(ctx.transform));
	const auto at = map_coords::FromMetres(p);
	const auto left = static_cast<uint16_t>((ctx.brain.angle + 0x200) & 0x7FF);
	const auto right = static_cast<uint16_t>((ctx.brain.angle - 0x200) & 0x7FF);
	if (!(gutils::GetDistanceInMetres(at, me + gutils::GetPosFromGameAngle(left, radius)) > radius))
	{
		return false;
	}
	return gutils::GetDistanceInMetres(at, me + gutils::GetPosFromGameAngle(right, radius)) > radius;
}

glm::vec2 CalcRandomPos(const Context& ctx, glm::vec2 c, float rMin, float rMax)
{
	const auto collideType = static_cast<uint32_t>(ctx.info.collideType);
	// 0x5ED083..0x5ED094: range = rMax - rMin (float), once
	const float range = rMax - rMin;
	// two random points, each followed by a 25-cell spiral from it (0x5ED156 the 25, 0x5ED1D6 `cmp ax, 2; jb`)
	for (int attempt = 0; attempt < 2; ++attempt)
	{
		// 0x5ED0B9: a = GameFloatRand(2 pi [0x40C90FDB]); 0x5ED0D2..0x5ED0D7: r = GameFloatRand(range) + rMin, always
		// called (GameFloatRand itself gives 0 for 0)
		const float a = GameFloatRand(glm::two_pi<float>());
		const float r = GameFloatRand(range) + rMin;
		// 0x5ED0DB..0x5ED152: AddDistanceFromAngle 0x74D510 inline: the centre's x and z go to metres (fild, x 10
		// [0x92B400], x 1/65536 [0x8AC41C]), cos(a) r is added and the sum goes back to 16.16 with GUtils' x 65536
		// [0x8AC408] / 10 and __ftol; the spiral then walks that MapCoords (InBounds 0x5ED16C, Collide 0x5ED181, the two
		// vt tests, += 0x5ED1C8). (openblack) the centre arrives in metres, as openblack keeps positions, and becomes a
		// MapCoords here; the original takes the MapCoords itself, so a centre that was not one already may be a unit
		// apart (Quantise is not idempotent)
		map_coords::MapCoords coords = map_coords::FromMetres(c);
		gutils::AddDistanceFromAngle(coords, a, r);
		Spiral spiral;
		for (int i = 0; i < 25; ++i)
		{
			const glm::vec2 p = map_coords::ToMetres(coords);
			if (InBounds(p) && !Collides(p, collideType) && IsPosValidForTurnAngle(ctx, p) && IsPosValidForMapCellExistance(ctx, p))
			{
				return p;
			}
			spiral.Advance(coords);
		}
	}
	// 0x5ED1E4..0x5ED218: the centre if it is outside its turning circles (vt +0xB3C); else 0x5ED23F..0x5ED2A5: me +
	// fn_0074D650(+0x5C, 10) (MapCoords::operator+ 0x605520), whose `sar 4` makes the step 0, so its own position (the
	// vt +0xB3C test of 0x5ED27E is not used)
	return IsPosValidForTurnAngle(ctx, c) ? c : Xz(ctx.transform);
}

void SetDomainCentre(Flock& flock, glm::vec3 position)
{
	const auto leader = LeaderOf(flock);
	if (leader != entt::null)
	{
		if (auto* brain = Locator::entitiesRegistry::value().TryGet<AnimalBrain>(leader); brain != nullptr)
		{
			brain->goal = glm::vec2(position.x, position.z);
		}
	}
	flock.domainCentre = position;
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
	// fn_0074CD50 = GUtils::GetDistanceInMetres 0x74CD70 (PosWithinDomain 0x5ED010), then <= the radius
	return gutils::GetDistanceInMetres(glm::vec2(flock->domainCentre.x, flock->domainCentre.z), p) <=
	       static_cast<float>(flock->domainRadius);
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

/// Animal::GetFinalState (0x41A240): the top state if it is final, else the destination
uint8_t FinalStateOf(const AnimalBrain& brain)
{
	return StateInfo(brain.topState).field0xc != 0 ? brain.topState : brain.finalState;
}

/// Animal::CallExitStateFunction (0x41A2C0): the exit function of its FINAL state, with the new state
bool CallExitStateFunction(AnimalBrain& brain, AnimalState to)
{
	const bool death = to >= AnimalState::SetDying && to <= AnimalState::Downed;
	const auto from = FinalStateOf(brain);
	switch (from)
	{
	case 24:
		// Living::ExitInHand (0x5ED500): only thrown, landed or dying while held
		return to == AnimalState::Flying || to == AnimalState::Landed || death;
	case 10:
		// Living::ExitInFlying (0x5ED540): caught, landed or dying
		return to == AnimalState::InHand || to == AnimalState::Landed || death;
	case 1:
	case 2:
	case 3:
	case 27:
	case 28:
	case 29:
	case 41:
	case 42:
	case 44:
		// Living::ExitMoveToPos (0x5EDDA0): the target (+0x60) is dropped
		brain.target = entt::null;
		return true;
	default:
		if (IsReactionState(from))
		{
			ExitReaction(brain, static_cast<uint8_t>(to));
		}
		return true;
	}
}

/// Living::SetTopState (0x5F28E0): the exit filter, the state, TurnsSinceStateChange = 0 and the state's clip
/// (Animal::SetStateSpeed 0x41A2B0 is empty; there are no into / out-of clips)
void SetTopState(entt::entity entity, AnimalBrain& brain, AnimalState state)
{
	// Living::SetTopState (0x5F28E0): the exit test (refused: 0x2E); the entry functions all accept
	if (!CallExitStateFunction(brain, state))
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
	// the exit of its final state tested with the new final state, then raw sets
	if (!CallExitStateFunction(ctx.brain, state))
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
	// SpellWolf::SetSpeed (vt+0x864, 0x4209B0) does nothing: only SetRunToFinalDest sets its speed
	if (ctx.animal.type == AnimalInfo::SpellWolf)
	{
		return;
	}
	ctx.brain.speed = static_cast<uint16_t>(std::min<uint32_t>(speed, 0xFFFF));
	SetAnimalAnim(ctx.entity, AnimalAnimId(ctx.entity), false);
}

uint32_t SpeedDefault(const Context& ctx)
{
	return static_cast<uint32_t>(ctx.info.speedGroup.speedDefault);
}

/// Living::SetupMoveToPos (0x5F2830): SetCurrentAndDestinationState(the info's move state, final), then
/// SetupMobileMoveToPos(p): a STEP_THROUGH walk (no obstacle hugging)
void SetupMoveToPos(Context& ctx, glm::vec2 p, AnimalState final)
{
	if (SetCurrentAndDestinationState(ctx, final))
	{
		SetupMobileMoveToPos(ctx, p);
	}
}

bool SetCurrentAndDestinationState(Context& ctx, AnimalState final)
{
	// SetCurrentAndDestinationState (0x5F2980): the exit test against the destination, then both states and the clip
	if (!CallExitStateFunction(ctx.brain, final))
	{
		return false;
	}
	ctx.brain.topState = static_cast<uint8_t>(ctx.info.moveState);
	ctx.brain.finalState = static_cast<uint8_t>(final);
	ctx.brain.turnsSinceStateChange = 0;
	SetAnimalStateAnim(ctx.entity);
	return true;
}

/// MobileWallHug::AreWeThere(0) (0x60AD40): within one turn's step of the goal
bool AreWeThere(const Context& ctx)
{
	const glm::vec2 d = ctx.brain.goal - Xz(ctx.transform);
	const float step = Metres(ctx.brain.speed);
	return glm::dot(d, d) <= step * step;
}

/// InitStepsXZ (0x60BFA0): SetTowardsAngle at the goal (the species' turn limit) and the step along the new heading
void InitStepsXZ(Context& ctx)
{
	const glm::vec2 d = ctx.brain.goal - Xz(ctx.transform);
	SetTowardsAngle(ctx, gutils::GetAngleFromXZ(Xz(ctx.transform), ctx.brain.goal), glm::length(d));
	ctx.brain.step = Step(ctx.brain.angle, ctx.brain.speed);
	FaceAngle(ctx.transform, ctx.brain.angle);
}

namespace
{
/// the ARRIVED / FINAL_STEP snap: Pos = goal
int SnapToGoal(Context& ctx)
{
	const glm::vec2 goal = ctx.brain.goal;
	if (InBounds(goal))
	{
		ctx.brain.movedLastTurn += glm::distance(Xz(ctx.transform), goal);
		ctx.transform.position = glm::vec3(goal.x, Locator::terrainSystem::value().GetHeightAt(goal) + ctx.brain.altitude, goal.y);
	}
	return 0xA;
}
} // namespace

void SetupMobileMoveToPos(Context& ctx, glm::vec2 p)
{
	ctx.brain.goal = p;
	InitStepsXZ(ctx);
	// +0x76 = 0 (0x60AB7D writes it only with a g_CircleHugStateInfo entry; unobserved: only ORBIT reads it). Not
	// kept: +0x78 = 1 with STEP_THROUGH (0x60ABA4; field not identified [inferred: not read on the animals' path])
	ctx.brain.hugGoalDistance = 0;
	if (AreWeThere(ctx))
	{
		ctx.brain.moveState = k_MoveArrived;
		return;
	}
	// CircleHugInfo::Reset (0x60A9F0): no circle, TurnsToObj 0xFF
	ctx.brain.hugCircle.set = false;
	ctx.brain.turnsToObj = 0xFF;
	ctx.brain.moveState = k_MoveStepThrough;
}

int MoveTo(Context& ctx)
{
	switch (ctx.brain.moveState)
	{
	case k_MoveArrived:
		// 0x60AFC0: there already, else on through STEP_THROUGH
		if (AreWeThere(ctx))
		{
			return SnapToGoal(ctx);
		}
		ctx.brain.moveState = k_MoveStepThrough;
		[[fallthrough]];
	case k_MoveStepThrough:
	{
		// STEP_THROUGH 0x60B02A: an animal re-aims every turn and walks straight, no obstacle handling
		InitStepsXZ(ctx);
		const int r = MoveBy(ctx, ctx.brain.step) ? 7 : 6;
		if (AreWeThere(ctx))
		{
			ctx.brain.moveState = k_MoveFinalStep;
		}
		return r;
	}
	case k_MoveFinalStep:
		return SnapToGoal(ctx);
	default:
		// LINEAR / ORBIT / EXIT_CIRCLE (0xC..0x12): the circle hug of SetupMoveToWithHug (ECS/AnimalWallHug.cpp)
		return IsHugMoveState(ctx.brain.moveState) ? HugMoveTo(ctx) : 0;
	}
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
	ctx.transform.position = glm::vec3(to.x, Locator::terrainSystem::value().GetHeightAt(to) + ctx.brain.altitude, to.y);
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
	// the list from its head: the newest member first
	for (auto it = flock->members.rbegin(); it != flock->members.rend(); ++it)
	{
		const auto member = *it;
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
	const auto centre = sum / count - me;
	const auto toCentre = Step(AngleOfMapCoords(centre.x, centre.y), ctx.brain.speed);
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

/// Animal::SetNewWander(MapCoords const&, int, int) (0x41A3F0): the new straight step (towards / away from c, the flock,
/// a random turn)
void SetNewWander(Context& ctx, glm::vec2 c, int32_t rMin, int32_t rMax)
{
	glm::ivec2 out(0);
	const glm::vec2 me = Xz(ctx.transform);
	// fn_0074CD50 = GetDistanceInMetres 0x74CD70, then __ftol 0x41A421: the whole metres compared as integers with the
	// int arguments (0x41A426 `cmp eax, rMax; jle`, 0x41A430 `cmp eax, rMin; jge`)
	const int32_t d = map_coords::FtoL(gutils::GetDistanceInMetres(c, me));
	if (d > rMax || d < rMin)
	{
		const auto a = d > rMax ? gutils::GetAngleFromXZ(me, c) : gutils::GetAngleFromXZ(c, me);
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
		ctx.brain.angle = AngleOfMapCoords(out.x, out.y);
		FaceAngle(ctx.transform, ctx.brain.angle);
	}
}

/// Animal::SetTowardsAngle (0x418560): turns at most turnAngle a turn; with the goal inside its turning circle
/// (R = 2 x speed / turnAngle in radians) it turns |diff| - turnAngle x d / R instead, nearly all the way when close
void SetTowardsAngle(Context& ctx, uint16_t target, float distance)
{
	// 0x4185E9: GetAngleDirection 0x74D6F0(+0x5C, target) and GetAngleDifference 0x74D740: the side and the size of
	// the turn (+0x400 turns +1, as the original)
	const int32_t direction = gutils::GetAngleDirection(ctx.brain.angle, target);
	const auto absDiff = static_cast<int32_t>(gutils::GetAngleDifference(ctx.brain.angle, target));
	const int32_t diff = direction * absDiff;
	const auto turnAngle = static_cast<int32_t>(ctx.info.turnAngle);
	int32_t turn = std::min(absDiff, turnAngle);
	if (absDiff > turnAngle)
	{
		// 0x418681: a turnAngle of 0 gives an infinite R (the x87 division), so the turn is the whole |diff|
		const float radians = static_cast<float>(turnAngle) * glm::two_pi<float>() / k_Circle;
		const float radius = turnAngle > 0 ? 2.0f * Metres(ctx.brain.speed) / radians : std::numeric_limits<float>::infinity();
		if (distance < radius)
		{
			// 0x4186B4..0x4186FC: max(turnAngle, |diff|) - turnAngle x d / R, truncated, at most |diff|
			const float reduced = static_cast<float>(std::max(turnAngle, absDiff)) -
			                      (turnAngle > 0 ? static_cast<float>(turnAngle) * distance / radius : 0.0f);
			turn = std::min(static_cast<int32_t>(reduced), absDiff);
		}
	}
	ctx.brain.angle = static_cast<uint16_t>((ctx.brain.angle + (diff < 0 ? -turn : turn)) & 0x7FF);
	// the bank zoomer: level again when it needs no turn, else +-GetBankAngle, over GetTimeToBank
	const float time = TimeToBank(ctx.animal.type);
	const float bankTarget = diff == 0 ? 0.0f : (diff < 0 ? -BankAngle(ctx.animal.type) : BankAngle(ctx.animal.type));
	// Animal::SetTowardsAngle 0x4185D5 calls 0x407D60 straight: its own threshold (0x407D67..0x407DA8) sets it below 0.001
	ctx.brain.bank.SetDestinationWithSpeedAndTime(bankTarget, 0.0f, time);
}

// ---- needs ----

bool IsLeader(const Context& ctx)
{
	const auto* flock = FlockOf(ctx.animal);
	return flock != nullptr && LeaderOf(*flock) == ctx.entity;
}

uint32_t AgeOf(const AnimalBrain& brain)
{
	// GGameInfo +0x0C: 1500 game turns per year
	const int32_t turns = static_cast<int32_t>(Turn()) - brain.birthTurn;
	return turns > 0 ? static_cast<uint32_t>(turns / 1500) : 0;
}

/// Animal::SetScaleForAge (0x417A40) for a young one: a random part of the way to the next age's scale
void SetScaleForAge(Context& ctx, uint32_t age)
{
	const auto& values = ctx.info.ageToScale.values;
	if (age + 1 >= values.size())
	{
		return;
	}
	const float step = 0.75f * (values[age + 1] - ctx.transform.scale.x);
	if (step > 0.0f)
	{
		const float scale = ctx.transform.scale.x + Locator::rng::value().NextValue(0.0f, step);
		ctx.transform.scale = glm::vec3(scale);
	}
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
	// GUtils::Spiral (0x74D7E0) over a copy of its own MapCoords (0x41A8C5..0x41A8D5): PosWithinDomain 0x41A906,
	// InBounds 0x41A913, FindGrazingPosition 0x41A924, then += 0x41A945 (whole cells on the high words)
	Spiral spiral;
	map_coords::MapCoords coords = map_coords::FromMetres(me);
	for (int i = 0; i < cells; ++i)
	{
		const glm::vec2 c = map_coords::ToMetres(coords);
		const auto cell = CellOf(c);
		bool ok = cell != myCell && PosWithinDomain(ctx, c) && InBounds(c);
		// fn_00418CD0: within viewAngle / 2 of its heading
		ok = ok && static_cast<int32_t>(gutils::GetAngleDifference(ctx.brain.angle, gutils::GetAngleFromXZ(map_coords::FromMetres(me), coords))) <=
		               static_cast<int32_t>(ctx.info.viewAngle) / 2;
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
		if (ok && !Collides(c, static_cast<uint32_t>(ctx.info.collideType)))
		{
			out = c;
			return true;
		}
		spiral.Advance(coords);
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

/// vt+0xBBC: the grazers' (Cow), the predators' (Animal / Wolf) or the birds' (Dove) ReactToAnimalNeeds
int ReactToAnimalNeeds(Context& ctx)
{
	if (IsBird(ctx.animal.type))
	{
		return BirdReactToAnimalNeeds(ctx);
	}
	return HunterOf(ctx.animal.type) != Hunter::None ? PredatorReactToAnimalNeeds(ctx) : CowReactToAnimalNeeds(ctx);
}

/// fn_00530210: the keeper takes every member of the other flock
void TakeAllMembers(Flock& keeper, entt::entity keeperEntity, Flock& other, uint32_t maxMembers)
{
	auto& registry = Locator::entitiesRegistry::value();
	// from the other's head (its newest), each one added at the keeper's head
	for (auto it = other.members.rbegin(); it != other.members.rend(); ++it)
	{
		const auto member = *it;
		if (!registry.Valid(member) || !registry.AllOf<Animal>(member))
		{
			continue;
		}
		keeper.members.push_back(member);
		registry.Get<Animal>(member).flock = keeperEntity;
		// the quirk: the other's max is added once per member moved, then clamped to the species' maxFlockSize
		keeper.maxMembers = std::min(keeper.maxMembers + other.maxMembers, maxMembers);
	}
	other.members.clear();
}

/// Animal::LookForFlocksAtPos (0x41A790) + fn_005302A0: the bigger flock keeps everyone
void LookForFlocksInSpiral(Context& ctx, float radius, bool merge)
{
	if (!merge)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto* mine = FlockOf(ctx.animal);
	// 0x41A6BE / 0x41A6CF: a flock with a town or a shepherd (+0x30, the villager of VillagerBecomesShepherd
	// 0x768C1C) never merges. openblack has no shepherds, so only the town is tested
	if (mine == nullptr || mine->town != entt::null)
	{
		return;
	}
	const auto mineEntity = ctx.animal.flock;
	const glm::vec2 me = Xz(ctx.transform);
	const int cells = std::max(1, static_cast<int>((radius / 10.0f) * (radius / 10.0f)));
	// the spiral's cells, in order, looking for another flock of my species: GUtils::Spiral (0x74D7E0 at 0x41A75D) over a
	// copy of its own MapCoords, InBounds 0x41A736, LookForFlocksAtPos 0x41A748 and += 0x41A76A (whole cells)
	Spiral spiral;
	map_coords::MapCoords coords = map_coords::FromMetres(me);
	for (int i = 0; i < cells; ++i)
	{
		const glm::vec2 c = map_coords::ToMetres(coords);
		if (InBounds(c))
		{
			for (const auto entity : Locator::entitiesMap::value().GetMobileInGridCell(CellOf(c)))
			{
				if (entity == ctx.entity || !registry.Valid(entity) || !registry.AllOf<Animal>(entity))
				{
					continue;
				}
				const auto& other = registry.Get<const Animal>(entity);
				if (other.type != ctx.animal.type || other.flock == entt::null || other.flock == mineEntity ||
				    !registry.Valid(other.flock))
				{
					continue;
				}
				const auto* brain = registry.TryGet<const AnimalBrain>(entity);
				if (brain != nullptr && (brain->status & 1) != 0)
				{
					continue; // dead
				}
				auto& theirs = registry.Get<Flock>(other.flock);
				// LookForFlocksAtPos 0x41A790: both flocks' GetPlayer equal (0x41A810..0x41A828; openblack's Flock
				// has no player: not tested [approximated]) and theirs without a shepherd (+0x30, 0x41A82A: none in
				// openblack); 0x41A837: not two script flocks (+0x24 & 0x400)
				const bool theirsScript = script_held::IsControlledByScript(other.flock);
				if (theirsScript && script_held::IsControlledByScript(mineEntity))
				{
					continue;
				}
				if (mine->members.size() + theirs.members.size() > ctx.info.maxFlockSize)
				{
					continue;
				}
				// fn_005302A0(mine, theirs): the flock with more members keeps them all; a script flock of theirs
				// (0x5302B4) always keeps them
				if (mine->members.size() >= theirs.members.size() && !theirsScript)
				{
					TakeAllMembers(*mine, mineEntity, theirs, ctx.info.maxFlockSize);
				}
				else
				{
					TakeAllMembers(theirs, other.flock, *mine, ctx.info.maxFlockSize);
				}
				return;
			}
		}
		spiral.Advance(coords);
	}
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
		const auto p = CalcRandomPos(ctx, {flock->domainCentre.x, flock->domainCentre.z}, static_cast<float>(ctx.info.domainInnerRadius),
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
	// fn_0074CD50 = GetDistanceInMetres 0x74CD70 to the leader
	if (PosWithinDomain(ctx, me) && gutils::GetDistanceInMetres(leader, me) <= flockDistance)
	{
		return 1;
	}
	const auto p = CalcRandomPos(ctx, leader, 0.0f, flockDistance);
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
	ctx.brain.moveState = k_MoveWander;
	ctx.brain.step = Step(ctx.brain.angle, ctx.brain.speed);
	SetSpeed(ctx, SpeedDefault(ctx));
	SetTopState(ctx, AnimalState::Wander);
	const auto* flock = FlockOf(ctx.animal);
	SetNewWander(ctx, FlockPos(ctx), static_cast<int32_t>(ctx.info.domainInnerRadius),
	             static_cast<int32_t>(flock != nullptr ? flock->domainRadius : ctx.info.domainRadius));
}

/// Cow::DecideWhatToDo (0x41D1B0)
void DecideWhatToDo(Context& ctx)
{
	if (HunterOf(ctx.animal.type) != Hunter::None)
	{
		PredatorDecideWhatToDo(ctx);
		return;
	}
	if (IsBird(ctx.animal.type))
	{
		BirdDecideWhatToDo(ctx);
		return;
	}
	if (ctx.animal.type == AnimalInfo::SpellWolf)
	{
		SetRunToFinalDest(ctx);
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
		SetNewWander(ctx, FlockPos(ctx), 0, static_cast<int32_t>(FlockDistance(ctx)));
	}
}

/// Animal::MoveToPos (0x41BAF0) -> Living::MoveToPos (0x5EC270): MobileWallHug::MoveTo; arrived (0xA) ->
/// SetTopStateToFinal (0x5ECA80). Inside its turning circle SetTowardsAngle turns nearly straight at the goal.
void MoveToPos(Context& ctx)
{
	if (MoveTo(ctx) == 0xA)
	{
		SetTopState(ctx, static_cast<AnimalState>(ctx.brain.finalState));
	}
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
	// (SpellWolf's vt+0xB50 is Lion::Eat too)
	if ((HunterOf(ctx.animal.type) != Hunter::None || ctx.animal.type == AnimalInfo::SpellWolf) &&
	    !Available(ctx.brain.foodTarget))
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
AnimalBrain& Initialise(entt::entity entity, const Animal& animal, const Transform& transform);
void DecideWhatToDo(Context& ctx);

void GivesBirth(Context& ctx)
{
	const auto position = ctx.transform.position;
	const auto type = ctx.animal.type;
	const auto town = ctx.animal.town;
	const auto flock = ctx.animal.flock;
	// fn_00419D10: the newborn (age 1) runs DecideWhatToDo at once, then the mother goes to START_WANDER
	const auto born = archetypes::AnimalArchetype::Create(position, type, town, flock, 1);
	auto& registry = Locator::entitiesRegistry::value();
	if (born != entt::null && registry.AllOf<Animal, Transform>(born))
	{
		auto& bornAnimal = registry.Get<Animal>(born);
		auto& bornTransform = registry.Get<Transform>(born);
		auto& bornBrain = Initialise(born, bornAnimal, bornTransform);
		Context child {born, bornAnimal, bornBrain, bornTransform, InfoOf(bornAnimal)};
		DecideWhatToDo(child);
	}
	auto& mother = registry.Get<AnimalBrain>(ctx.entity);
	SetTopState(ctx.entity, mother, AnimalState::StartWander);
}

/// Living::SetDying (0x5EC390): nothing while it flies
std::vector<std::pair<uint32_t, DeathCallback>> g_DeathListeners;
std::vector<SpeciesDying> g_SpeciesDying;

void SetDying(entt::entity entity, AnimalBrain& brain)
{
	// vt+0x6A4: the species' own SetDying replaces Living's
	if (const auto* animal = Locator::entitiesRegistry::value().TryGet<Animal>(entity); animal != nullptr)
	{
		const auto type = static_cast<size_t>(animal->type);
		if (type < g_SpeciesDying.size() && g_SpeciesDying[type])
		{
			g_SpeciesDying[type](entity);
			return;
		}
	}
	if (physics::PhysicsObjects::IsFlying(entity))
	{
		return;
	}
	if ((brain.status & 1) == 0)
	{
		// a copy: a listener may add or remove listeners
		const auto listeners = g_DeathListeners;
		for (const auto& [id, listener] : listeners)
		{
			listener(entity);
		}
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

/// Living::StateDead (0x5EC400): off the flock; the corpse lies 600 turns, then its smoke puff and it goes. A
/// script-controlled one (+0x25 & 4, 0x5EC41E) never times out (the death reason SACRIFICE 7 is not tracked).
bool Dead(Context& ctx)
{
	LeaveFlock(ctx.entity, ctx.animal);
	if (script_held::IsControlledByScript(ctx.entity) || ctx.brain.counter-- != 0)
	{
		return false;
	}
	// Object::CreateSmokyStuff(0, 1.0, white) at half its height
	const float height = 0.5f * ctx.transform.scale.y * 2.0f;
	SmokyStuff::Create(ctx.transform.position + glm::vec3(0.0f, height, 0.0f), 1.0f);
	return true;
}

/// Animal::ProcessState (0x417EE0) and the state table; true when the animal is to be deleted
bool ProcessState(Context& ctx)
{
	++ctx.brain.turnsSinceStateChange;
	ctx.brain.movedLastTurn = 0.0f;
	// Living::ProcessReaction (0x5F1270): the flight from a predator ends after its turns or when it has gone
	ProcessReaction(ctx);
	// fn_00417E90: a target that is no longer available is dropped
	if (ctx.brain.target != entt::null && !Available(ctx.brain.target))
	{
		ctx.brain.target = entt::null;
	}
	if (StateInfo(ctx.brain.topState).field0xa4 != 0)
	{
		ProcessNeeds(ctx);
		// fn_004179F0: a young one grows four times a year (every 1500 / 4 turns)
		const auto age = AgeOf(ctx.brain);
		if (age < ctx.info.grownUpAge && Turn() % 375 == 0)
		{
			ctx.animal.age = age;
			SetScaleForAge(ctx, age);
		}
	}
	// what it was eating has gone
	if (ctx.brain.foodTarget != entt::null && !Available(ctx.brain.foodTarget))
	{
		ctx.brain.foodTarget = entt::null;
		ctx.brain.counter = 0;
		SetTopState(ctx, AnimalState::DecideWhatToDo);
		return false;
	}
	// (SpellWolf's vt+0xB48 is Animal::StartWander 0x417C90 too: after a hunt it wanders, and its Wander runs on)
	const bool walker = IsGrazer(ctx.animal.type) || HunterOf(ctx.animal.type) != Hunter::None ||
	                    ctx.animal.type == AnimalInfo::SpellWolf;
	switch (static_cast<AnimalState>(ctx.brain.topState))
	{
	case AnimalState::MoveToPos:
		MoveToPos(ctx);
		if (ctx.animal.type == AnimalInfo::SpellWolf)
		{
			SpellWolfMoveToPos(ctx); // SpellWolf::MoveToPos 0x421300 (vt+0xB40)
		}
		break;
	case AnimalState::Landed:
		// Animal::Landed (0x417D50): CalculeLairPos, the flock now centres where it landed (the predators: their lair)
		if (HunterOf(ctx.animal.type) != Hunter::None)
		{
			CalculeLairPos(ctx);
		}
		else if (auto* flock = FlockOf(ctx.animal); flock != nullptr)
		{
			SetDomainCentre(*flock, ctx.transform.position);
		}
		PlayAnimThenSetState(ctx, AnimalState::InteractDecideWhatToDo);
		break;
	case AnimalState::SetDying:
		SetDying(ctx.entity, ctx.brain);
		break;
	case AnimalState::Dying:
	case AnimalState::Drowning:
		// Dove::Dying (0x41F1B0): a bird falls into the physics with its flight
		if (IsBird(ctx.animal.type) && ctx.brain.altitude > 0.0f)
		{
			BirdDying(ctx);
		}
		else
		{
			PlayAnimThenSetState(ctx, AnimalState::Dead);
		}
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
		if (IsBird(ctx.animal.type))
		{
			BirdStartWander(ctx);
		}
		else if (walker)
		{
			StartWander(ctx);
		}
		break;
	case AnimalState::SpecialMoveToPos:
		SpecialMoveToPos(ctx);
		break;
	case AnimalState::FollowFlock:
		FollowFlock(ctx);
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
	case AnimalState::GotoFoodReaction:
		GotoFoodReaction(ctx);
		break;
	case AnimalState::ArrivesAtFoodReaction:
		ArrivesAtFoodReaction(ctx);
		break;
	case AnimalState::FleeingFromObjectReaction:
		FleeingFromObjectReaction(ctx);
		break;
	case AnimalState::FleeingAndLookingAtObjectReaction:
		FleeingAndLookingReaction(ctx);
		break;
	case AnimalState::Wander:
		if (ctx.animal.type == AnimalInfo::SpellWolf)
		{
			SetRunToFinalDest(ctx); // SpellWolf::Wander 0x420A10
		}
		else
		{
			Wander(ctx);
		}
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
		// Animal::InteractDecideWhatToDo (0x417D80): LookForFlocksInSpiral(2 x domainRadius, merge = 1), then StartWander
		LookForFlocksInSpiral(ctx, 2.0f * static_cast<float>(ctx.info.domainRadius), true);
		if (IsBird(ctx.animal.type))
		{
			BirdStartWander(ctx);
		}
		else if (walker)
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
	// MobileWallHug::SetToZero (0x60F760): game angle 0, facing +x; nothing at creation sets another
	brain.angle = 0;
	// Living::SetAge (0x5ED2C0): BirthTurn = the turn it would have been born to be this old
	brain.birthTurn = static_cast<int32_t>(Turn()) - static_cast<int32_t>(animal.age) * 1500;
	// the Dove constructor's altitude (the archetype put it at the land + altitudeNormal)
	if (IsBird(animal.type) && Locator::terrainSystem::has_value())
	{
		brain.altitude = std::max(0.0f, transform.position.y - Locator::terrainSystem::value().GetHeightAt(Xz(transform)));
		brain.goalAltitude = brain.altitude;
	}
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
	// the living list is walked newest first (the creation order; openblack's villagers still take their turn apart)
	std::sort(animals.begin(), animals.end(),
	          [](entt::entity a, entt::entity b) { return object_index::Of(a) > object_index::Of(b); });
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
	ProcessDownedVillagers();
	// (the reactions whose initiator went are pruned at the start of the turn: ECS/Effects/Reactions BeginTurn)
	RunDebugHooks(Turn());
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
	// Flock::SeperateLivingIntoNewFlock (0x52FE10): RemoveLivingFromFlock (an emptied flock is deleted), then
	// Flock::Flock(Living*) 0x52F950 where it was picked up with the old radius and distance: no town, max 0
	if (auto* old = FlockOf(animal); old != nullptr)
	{
		const auto oldEntity = animal.flock;
		const uint16_t radius = old->domainRadius;
		const uint16_t distance = old->flockDistance;
		// 0x419B75: a script flock (+0x25 & 4) gives up the script reference FlockAttach took for the animal
		// (GScript::DecrementScriptReference 0x70CFD0) and is kept even empty (SeperateLivingIntoNewFlock arg 0)
		const bool scriptFlock = script_held::IsControlledByScript(oldEntity);
		if (scriptFlock)
		{
			script_held::DecrementReference(entity);
		}
		LeaveFlock(entity, animal);
		if (old->members.empty() && !scriptFlock)
		{
			registry.Destroy(oldEntity);
		}
		const auto flockEntity = registry.Create();
		auto& flock = registry.Assign<Flock>(flockEntity);
		flock.domainCentre = registry.Get<const Transform>(entity).position;
		flock.savedDomainCentre = flock.domainCentre;
		flock.domainRadius = radius;
		flock.flockDistance = distance;
		flock.members.push_back(entity);
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

void EndPhysics(entt::entity entity, const glm::mat3& rotation, const glm::mat3& turnStartRotation)
{
	auto* brain = BrainOf(entity);
	if (brain == nullptr)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// the landType from the turn-start matrix's right row y (po+0xD8): on its right side, its left side or its feet
	const float right = turnStartRotation[0].y;
	const uint16_t landType = right > 0.5f ? 1 : (right < -0.5f ? 2 : 0);
	// the heading of the body's forward row. The original adds pi to GetYAngle of that row; openblack builds the body
	// from the drawn rotation, so the drawn yaw is kept as it is.
	brain->angle = AngleOfRotation(rotation);
	brain->step = glm::ivec2(0);
	// altitude(+0x1C) = 0: on the land
	brain->altitude = 0.0f;
	brain->bank.SetPosition(0.0f);
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
	transform.rotation = lh_matrix::AngleY(-yaw); // yaw is glm's angle (the game's is -yaw); (inferido) EndPhysics
	EndPhysics(entity, transform.rotation, transform.rotation);
}

void SetScriptState(entt::entity entity, AnimalState state)
{
	auto* brain = BrainOf(entity);
	// GScript::SetScriptState (0x6F82E0), a Living that is available (not dying: status & 1) and on the map [inferred for an animal: not in the
	// hand]
	if (brain == nullptr || brain->topState == static_cast<uint8_t>(AnimalState::InHand) || (brain->status & 1) != 0)
	{
		return;
	}
	// StorePreviousState (0x417040): its final state; CallExitStateFunction (0x41A2C0), its answer not read;
	// CallEntryStateFunctionUc (0x41A310): no animal state the scripts set has an entry function, so SetState(0, s);
	// SetAnim(1) (vt+0x8FC); the counter (+0x58) 0
	brain->previousState = FinalStateOf(*brain);
	CallExitStateFunction(*brain, state);
	brain->topState = static_cast<uint8_t>(state);
	brain->turnsSinceStateChange = 0;
	SetAnimalStateAnim(entity);
	brain->counter = 0;
}

void ScriptMoveTo(entt::entity entity, glm::vec2 position)
{
	auto* brain = BrainOf(entity);
	// MOVE_GAME_THING (GScript 0x6F8F6C) on a Living: on the map and not drowning (an animal never drowns: Object 0),
	// then AreWeThere(pos, 0) (vt+0x85C) -> SetScriptState(IN_SCRIPT 4), else SetupMoveToPos(pos, IN_SCRIPT 4)
	if (brain == nullptr || brain->topState == static_cast<uint8_t>(AnimalState::InHand))
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& animal = registry.Get<Animal>(entity);
	Context ctx {entity, animal, *brain, registry.Get<Transform>(entity), InfoOf(animal)};
	const glm::vec2 d = position - Xz(ctx.transform);
	const float step = Metres(brain->speed);
	if (glm::dot(d, d) <= step * step)
	{
		SetScriptState(entity, AnimalState::InScript);
		return;
	}
	SetupMoveToPos(ctx, position, AnimalState::InScript);
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
