/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <limits>
#include <optional>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include "3D/L3DAnim.h"
#include "3D/LandIslandInterface.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/Animations.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

/// The predators (lion, tiger, leopard, wolf) like the original: their decisions, lairs and hunting (docs/bw1-notes/
/// animals.md; research dev\tmp_dis\animals\predator_ai.md and hunting.md). Villagers are prey in the original too;
/// openblack's villagers have no downed / eaten states yet, so for now only animals are hunted.
namespace openblack::ecs::animal_ai::detail
{
using components::Animal;
using components::AnimalBrain;
using components::BigForest;
using components::Flock;
using components::Life;
using components::Town;
using components::Transform;
using components::Tree;

namespace
{
uint32_t Speed(const GAnimalInfo& info, int index)
{
	const auto& group = info.speedGroup;
	const SpeedState entries[] = {group.speedDefault, group.speedFleeing, group.speed2, group.speed3, group.speed4, group.speed5};
	return static_cast<uint32_t>(entries[std::clamp(index, 0, 5)]);
}

float AltitudeAboveLand(const Transform& transform)
{
	return transform.position.y - Locator::terrainSystem::value().GetHeightAt(Xz(transform));
}

bool IsChild(const Context& ctx)
{
	return ctx.animal.age < ctx.info.grownUpAge;
}

/// the Pounce slot's clip (Lion 100 POUNCE_HI, Tiger 158, Leopard 74, Wolf 179 POUNCE)
int32_t PounceClip(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Lion:
	case AnimalInfo::PuzzleLion:
		return 100;
	case AnimalInfo::Tiger:
		return 158;
	case AnimalInfo::Leopard:
		return 74;
	default:
		return 179;
	}
}

/// the clip header's stride (AllAnims.anm +0x28)
float ClipStride(int32_t index)
{
	auto& animations = Locator::resources::value().GetAnimations();
	const auto id = ClipId(static_cast<uint32_t>(index));
	return animations.Contains(id) ? animations.Handle(id)->GetCycleDistance() : 0.0f;
}

float Scale(const Context& ctx)
{
	return ctx.transform.scale.x > 0.0f ? ctx.transform.scale.x : 1.0f;
}

/// Animal::IsPosValidForTurnAngle (0x41B210): outside both of its turning circles (radius 2 x speed / turnAngle,
/// centred that far to its left and right)
bool IsPosValidForTurnAngle(const Context& ctx, glm::vec2 p)
{
	const float turn = static_cast<float>(ctx.info.turnAngle) * glm::two_pi<float>() / k_Circle;
	if (turn <= 0.0f)
	{
		return true;
	}
	const float radius = 2.0f * Metres(ctx.brain.speed) / turn;
	const float a = static_cast<float>(ctx.brain.angle) * glm::two_pi<float>() / k_Circle;
	const glm::vec2 side(-std::sin(a), std::cos(a));
	const glm::vec2 me = Xz(ctx.transform);
	return glm::distance(p, me + side * radius) >= radius && glm::distance(p, me - side * radius) >= radius;
}

/// fn_004196D0: is it prey? Another species' animal with meat, on the ground, alive, reachable...
bool IsPrey(const Context& ctx, entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (entity == ctx.entity || !Available(entity))
	{
		return false;
	}
	const auto* animal = registry.TryGet<const Animal>(entity);
	const auto* brain = registry.TryGet<const AnimalBrain>(entity);
	if (animal == nullptr || brain == nullptr || animal->type == ctx.animal.type)
	{
		return false;
	}
	const auto& transform = registry.Get<const Transform>(entity);
	if (AltitudeAboveLand(transform) > 2.0f || FlockOf(ctx.animal) == nullptr)
	{
		return false;
	}
	// Object::GetFoodValue(needsFoodTypes): foodValue when its foodType has that bit (every animal but the tortoise)
	const auto& info = InfoOf(*animal);
	if ((static_cast<uint32_t>(info.foodType) & static_cast<uint32_t>(ctx.info.needsFoodTypes)) == 0 || info.foodValue == 0.0f)
	{
		return false;
	}
	const glm::vec2 p = Xz(transform);
	if (!IsPosValidForTurnAngle(ctx, p) || (brain->status & 1) != 0)
	{
		return false;
	}
	// a cub only takes a downed prey
	if (IsChild(ctx) && (brain->status & 0x80) == 0)
	{
		return false;
	}
	const glm::vec2 me = Xz(ctx.transform);
	if (ctx.brain.preyCell != glm::vec2(0.0f) && !(2.0f * glm::distance(me, p) < glm::distance(me, ctx.brain.preyCell)))
	{
		return false;
	}
	ctx.brain.preyCell = MapInterface::GetCellCenter(CellOf(p));
	return true;
}

/// fn_00419490: the first prey of a spiral of that many map cells from its own
bool FindPrey(Context& ctx, int cells)
{
	const auto& map = Locator::entitiesMap::value();
	const glm::vec2 me = Xz(ctx.transform);
	int x = 0;
	int y = 0;
	int dx = 0;
	int dy = -1;
	for (int i = 0; i < cells; ++i)
	{
		const glm::vec2 c = me + 10.0f * glm::vec2(static_cast<float>(x), static_cast<float>(y));
		if (InBounds(c))
		{
			for (const auto entity : map.GetMobileInGridCell(CellOf(c)))
			{
				if (IsPrey(ctx, entity))
				{
					ctx.brain.target = entity;
					return true;
				}
			}
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

/// fn_00419340: the remembered target is still worth chasing
bool CurrentTargetOk(Context& ctx)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto target = ctx.brain.target;
	if (target != entt::null && Available(target) && AltitudeAboveLand(registry.Get<const Transform>(target)) <= 2.0f &&
	    glm::distance(Xz(ctx.transform), Xz(registry.Get<const Transform>(target))) < ctx.info.huntingDistance)
	{
		return true;
	}
	ctx.brain.target = entt::null;
	return false;
}

AnimalBrain* PreyBrain(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) ? registry.TryGet<AnimalBrain>(entity) : nullptr;
}

bool IsDowned(entt::entity entity)
{
	const auto* brain = PreyBrain(entity);
	return brain != nullptr && (brain->status & 0x80) != 0;
}

/// Animal::FinishPouncing (0x419120): fed; to the prey, one scale-metre short of it, to eat it
void FinishPouncing(Context& ctx, entt::entity prey)
{
	auto& registry = Locator::entitiesRegistry::value();
	ctx.brain.hunger = 0;
	ctx.brain.target = entt::null;
	ctx.brain.preyCell = glm::vec2(0.0f);
	SetSpeed(ctx, SpeedDefault(ctx));
	const glm::vec2 me = Xz(ctx.transform);
	const glm::vec2 at = Xz(registry.Get<const Transform>(prey));
	const glm::vec2 d = at - me;
	const glm::vec2 goal = glm::length(d) > 0.0f ? at - glm::normalize(d) * Scale(ctx) : at;
	SetupMoveToPos(ctx, goal, AnimalState::StartToEat);
	ctx.brain.foodTarget = Available(prey) ? prey : entt::null;
}

/// Animal::SetupMoveToTarget (0x418D20): a downed prey is eaten, any other chased (the state only, the clip stays)
void SetupMoveToTarget(Context& ctx, entt::entity target)
{
	ctx.brain.target = target;
	if (IsDowned(target))
	{
		FinishPouncing(ctx, target);
		SetSpeed(ctx, Speed(ctx.info, 4));
		return;
	}
	// Living::SetState (0x5F2A80): raw
	ctx.brain.topState = static_cast<uint8_t>(AnimalState::HuntingMoveToPos);
	ctx.brain.turnsSinceStateChange = 0;
}

/// Lion::ReactToAnimalFoodNeeds (0x41FF40), every predator: chase the target seen last turn, else look for one
int ReactToAnimalFoodNeeds(Context& ctx)
{
	if (CurrentTargetOk(ctx))
	{
		SetSpeed(ctx, Speed(ctx.info, 4));
		ctx.brain.chaseStart = g_Turn;
		SetupMoveToTarget(ctx, ctx.brain.target);
		return k_Started;
	}
	// found: it hunts from the next call
	if (FindPrey(ctx, static_cast<int>(ctx.info.farSightDistance)))
	{
		return k_Nothing;
	}
	if (ctx.brain.preyCell != glm::vec2(0.0f))
	{
		SetSpeed(ctx, Speed(ctx.info, 4));
		SetupMoveToPos(ctx, ctx.brain.preyCell, AnimalState::StartWander);
		ctx.brain.preyCell = glm::vec2(0.0f);
		return k_Started;
	}
	return k_Nothing;
}

/// Animal::HuntingMoveToPosAbandon (0x418FD0)
void Abandon(Context& ctx)
{
	ctx.brain.target = entt::null;
	ctx.brain.preyCell = glm::vec2(0.0f);
	SetSpeed(ctx, SpeedDefault(ctx));
	SetTopState(ctx, AnimalState::StartWander);
}

/// one step at its speed towards the point, turning like SetTowardsAngle (openblack: no wall hug round obstacles)
void StepTowards(Context& ctx, glm::vec2 goal)
{
	const glm::vec2 d = goal - Xz(ctx.transform);
	const float distance = glm::length(d);
	if (distance <= Metres(ctx.brain.speed))
	{
		if (InBounds(goal))
		{
			ctx.transform.position = glm::vec3(goal.x, Locator::terrainSystem::value().GetHeightAt(goal), goal.y);
			ctx.brain.movedLastTurn += distance;
		}
		return;
	}
	SetTowardsAngle(ctx, AngleOf(d), distance);
	ctx.brain.step = Step(ctx.brain.angle, ctx.brain.speed);
	FaceAngle(ctx.transform, ctx.brain.angle);
	MoveBy(ctx, ctx.brain.step);
}

/// the nearest of the entities with a Transform that `accept` takes (-1: none)
template <typename Component, typename Accept>
std::optional<glm::vec3> Nearest(glm::vec2 from, Accept accept)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::optional<glm::vec3> best;
	float bestDistance = std::numeric_limits<float>::max();
	registry.Each<const Component, const Transform>([&](entt::entity, const Component& component, const Transform& transform) {
		if (!accept(component))
		{
			return;
		}
		const float d = glm::distance(from, Xz(transform));
		if (d < bestDistance)
		{
			bestDistance = d;
			best = transform.position;
		}
	});
	return best;
}
} // namespace

void CalculeLairPos(Context& ctx)
{
	auto* flock = FlockOf(ctx.animal);
	if (flock == nullptr || !IsLeader(ctx))
	{
		return;
	}
	const glm::vec2 me = Xz(ctx.transform);
	std::optional<glm::vec3> lair;
	const auto inForest = [](const Tree& tree) { return tree.forestId != 0; };
	switch (HunterOf(ctx.animal.type))
	{
	case Hunter::Tiger:
		// Tiger::CalculeLairPos (0x421470): the nearest forest (the first tree of the best-scored one [approximated by the
		// nearest forest tree])
		lair = Nearest<Tree>(me, inForest);
		break;
	case Hunter::Wolf:
		// Wolf::CalculeLairPos (0x421730): the nearest big forest, else forest, else tree
		lair = Nearest<BigForest>(me, [](const BigForest&) { return true; });
		if (!lair)
		{
			lair = Nearest<Tree>(me, inForest);
		}
		if (!lair)
		{
			lair = Nearest<Tree>(me, [](const Tree&) { return true; });
		}
		break;
	default:
		break; // Lion::CalculeLairPos (0x420010): where it is
	}
	flock->domainCentre = lair.value_or(ctx.transform.position);
}

int PredatorReactToAnimalNeeds(Context& ctx)
{
	if (HunterOf(ctx.animal.type) == Hunter::Wolf)
	{
		// Wolf::ReactToAnimalNeeds (0x421950)
		switch (CheckNeeds(ctx))
		{
		case 3:
			SetTopState(ctx, AnimalState::GivesBirth);
			return k_Started;
		case 1:
			return ReactToAnimalFoodNeeds(ctx);
		case 2:
			if (const auto* flock = FlockOf(ctx.animal); flock != nullptr)
			{
				SetupMoveToPos(ctx, {flock->domainCentre.x, flock->domainCentre.z}, AnimalState::Sleeps);
				SetSpeed(ctx, SpeedDefault(ctx));
				return k_Started;
			}
			return k_Nothing;
		default:
			return k_Nothing;
		}
	}
	// Animal::ReactToAnimalNeeds (0x4183C0): hunger (no food: on to sleep), then sleep
	if (ctx.info.hunger != 0 && ctx.brain.hunger >= static_cast<int32_t>(ctx.info.hunger) && ReactToAnimalFoodNeeds(ctx) == k_Started)
	{
		return k_Started;
	}
	if (ctx.info.sleep != 0 && ctx.brain.sleep >= static_cast<int32_t>(ctx.info.sleep) && ctx.brain.sleepCell != glm::u16vec2(0))
	{
		SetTopState(ctx, AnimalState::SeekSleep);
		return k_Started;
	}
	return k_Nothing;
}

void PredatorDecideWhatToDo(Context& ctx)
{
	if (HunterOf(ctx.animal.type) == Hunter::Wolf)
	{
		// Wolf::DecideWhatToDo (0x4216B0): back to the lair
		const auto* flock = FlockOf(ctx.animal);
		if (flock == nullptr || PredatorReactToAnimalNeeds(ctx) == k_Started)
		{
			return;
		}
		SetupMoveToPos(ctx, SquarePos({flock->domainCentre.x, flock->domainCentre.z}, static_cast<float>(flock->members.size())),
		               AnimalState::HideInLair);
		SetSpeed(ctx, SpeedDefault(ctx));
		return;
	}
	// Lion::DecideWhatToDo (0x41FE70), also the tiger and the leopard
	if (CheckNeeds(ctx) == 3)
	{
		SetTopState(ctx, AnimalState::GivesBirth);
		return;
	}
	if (g_VisualTime > 22.0f)
	{
		SetTopState(ctx, AnimalState::SeekSleep);
		ctx.brain.sleep = static_cast<int16_t>(ctx.info.sleep);
		return;
	}
	if (KeepLeaderWithinDomain(ctx) == k_Started || KeepFlockMemberWithinFlockArea(ctx) == k_Started)
	{
		return;
	}
	if (PredatorReactToAnimalNeeds(ctx) != k_Started)
	{
		SetTopState(ctx, AnimalState::StartWander);
	}
}

void HuntingMoveToPos(Context& ctx)
{
	// Animal::HuntingMoveToPos (0x418DB0)
	auto& registry = Locator::entitiesRegistry::value();
	const auto target = ctx.brain.target;
	if (target == entt::null || !Available(target) || g_Turn - ctx.brain.chaseStart >= ctx.info.chaseTime)
	{
		Abandon(ctx);
		return;
	}
	const glm::vec2 at = Xz(registry.Get<const Transform>(target));
	StepTowards(ctx, at);
	const glm::vec2 me = Xz(ctx.transform);
	const float d = glm::distance(me, at);
	const float reach = d < ctx.info.attackDistance ? Scale(ctx) * ClipStride(PounceClip(ctx.animal.type)) * 0.5f : 0.5f;
	if (d <= reach)
	{
		// fn_00418CD0(pos, 0x100): the prey within +-22.5 degrees of its heading
		if (std::abs(AngleDiff(ctx.brain.angle, AngleOf(at - me))) <= 0x80)
		{
			SetTopState(ctx, AnimalState::TargetPounce);
		}
		else
		{
			Abandon(ctx);
		}
		return;
	}
	// sprint inside the attack distance, stalk (prowl) inside the stalking one
	uint32_t speed;
	if (d < ctx.info.attackDistance)
	{
		speed = Speed(ctx.info, 3);
	}
	else if (d < ctx.info.stalkingDistance)
	{
		speed = Speed(ctx.info, 2);
	}
	else
	{
		speed = (g_Turn % 100) < 33 ? Speed(ctx.info, 2) : Speed(ctx.info, 4);
	}
	SetSpeed(ctx, speed);
	if (d >= ctx.info.huntingDistance)
	{
		Abandon(ctx);
	}
}

void TargetPounce(Context& ctx)
{
	// Animal::TargetPounce (0x419010): the leap lasts the pounce clip's stride; within 1 m the prey is downed
	auto& registry = Locator::entitiesRegistry::value();
	const float covered = Metres(ctx.brain.speed) * static_cast<float>(ctx.brain.turnsSinceStateChange);
	const float pounceLength = Scale(ctx) * ClipStride(PounceClip(ctx.animal.type));
	if (!Available(ctx.brain.target))
	{
		ctx.brain.target = entt::null;
	}
	const auto target = ctx.brain.target;
	if (target == entt::null)
	{
		StartWander(ctx);
		return;
	}
	const glm::vec2 at = Xz(registry.Get<const Transform>(target));
	StepTowards(ctx, at);
	if (glm::distance(Xz(ctx.transform), at) <= 1.0f && !IsDowned(target))
	{
		// fn_005EC480: the prey falls, with 0.05 of its life
		if (auto* prey = PreyBrain(target); prey != nullptr)
		{
			prey->status |= 0x80;
			(registry.AllOf<Life>(target) ? registry.Get<Life>(target) : registry.Assign<Life>(target)).value = 0.05f;
			SetTopState(target, *prey, AnimalState::Downed);
		}
	}
	if (covered >= pounceLength)
	{
		if (IsDowned(target))
		{
			FinishPouncing(ctx, target);
			return;
		}
		// missed: after it again
		SetupMoveToTarget(ctx, target);
	}
}

void BeingEaten(Context& ctx)
{
	// Living::StateBeingEaten (0x5EC4D0): 300 turns, then dead (the corpse 50 turns)
	if (--ctx.brain.counter > 0)
	{
		return;
	}
	ctx.brain.counter = 50;
	auto& registry = Locator::entitiesRegistry::value();
	(registry.AllOf<Life>(ctx.entity) ? registry.Get<Life>(ctx.entity) : registry.Assign<Life>(ctx.entity)).value = 0.0f;
	PlayAnimThenSetState(ctx, AnimalState::Dead);
}

void HideInLair(Context& ctx)
{
	// Wolf::HideInLair (0x4219E0): lies in the lair; hungry after 23:00 the pack goes hunting
	if (ctx.brain.sleep > 0)
	{
		ctx.brain.sleep = static_cast<int16_t>(ctx.brain.sleep - 2);
		return;
	}
	if (CheckNeeds(ctx) != 1 || g_VisualTime <= 23.0f)
	{
		return;
	}
	const auto* flock = FlockOf(ctx.animal);
	if (flock == nullptr)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const glm::vec2 me = Xz(ctx.transform);
	const float size = static_cast<float>(flock->members.size());
	// the nearest flock leader no stronger than it, on the ground, within domainRadius MapCoords / 100 (1.2 m: in
	// practice its own flock, so the leader hunts where it is and the others head for the nearest town)
	std::optional<glm::vec2> goal;
	float best = std::numeric_limits<float>::max();
	registry.Each<const Flock>([&](entt::entity, const Flock& other) {
		const auto leader = LeaderOf(other);
		if (leader == entt::null || !registry.AllOf<Animal, Transform>(leader))
		{
			return;
		}
		const auto& transform = registry.Get<const Transform>(leader);
		const float d = glm::distance(me, Xz(transform)) * k_MapCoordsPerMetre / 100.0f;
		if (InfoOf(registry.Get<const Animal>(leader)).strength <= ctx.info.strength && AltitudeAboveLand(transform) <= 2.0f &&
		    d <= static_cast<float>(flock->domainRadius) && d < best)
		{
			best = d;
			goal = Xz(transform);
		}
	});
	if (!goal)
	{
		if (const auto town = Nearest<Town>(me, [](const Town&) { return true; }); town)
		{
			goal = glm::vec2(town->x, town->z);
		}
	}
	if (goal)
	{
		SetSpeed(ctx, SpeedDefault(ctx));
		SetupMoveToPos(ctx, SquarePos(*goal, size), AnimalState::StartWander);
	}
}

} // namespace openblack::ecs::animal_ai::detail
