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
#include "ECS/AnimalAnimations.h"
#include "ECS/Animations.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Villager.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerSpeed.h"
#include "ECS/WaterQueries.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Villager/VillagerCore.h"
#include <spdlog/spdlog.h>

#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Spells/SpellFlock.h"
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
using components::DownedVillager;
using components::LivingAction;
using components::Villager;

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

/// Animal::SetSpeed's clip part: the state's clip at the new speed, not restarted
void SetAnimalAnimHelper(Context& ctx)
{
	SetAnimalAnim(ctx.entity, AnimalAnimId(ctx.entity), false);
}

bool IsDowned(entt::entity entity);

/// vt+0xBB4 IsHuntingTargetValid: Animal 0x418DA0 = the target is not null; SpellWolf 0x420D60 (the flock miracle's
/// wolves, Magic/Spells/SpellFlock): not fading (+0x16C, the fade's destination, != 0), a Living (RTDynamicCast) whose
/// GetFinalState (vt+0xB04) is not DYING 0xE / DEAD 0xF / DOWNED 0x11 / BEING_EATEN 0x12 and not downed (+0xB4 & 0x80),
/// inside its corridor (IsPosOnCorridor 0x420E10) and outside its turning circles (vt+0xB3C IsPosValidForTurnAngle)
bool IsHuntingTargetValid(const Context& ctx, entt::entity target)
{
	if (target == entt::null)
	{
		return false;
	}
	if (ctx.animal.type != AnimalInfo::SpellWolf)
	{
		return true;
	}
	const auto* wolf = magic::spell_flock::AnimalOf(ctx.entity);
	if (wolf == nullptr || wolf->fade.destination == 0.0f)
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	uint8_t final;
	if (const auto* brain = registry.TryGet<const AnimalBrain>(target); brain != nullptr)
	{
		// Animal::GetFinalState 0x41A240: the top state if the table marks it final, else the destination
		const auto& state = Locator::infoConstants::value().animalStateTable.at(std::min<size_t>(brain->topState, 52));
		final = state.field0xc != 0 ? brain->topState : brain->finalState;
	}
	else if (registry.AllOf<Villager>(target))
	{
		final = static_cast<uint8_t>(ecs::villager::GetFinalState(target)); // Villager::GetFinalState 0x751DD0
	}
	else
	{
		return false;
	}
	if (final == 0xE || final == 0xF || final == 0x11 || final == 0x12 || IsDowned(target))
	{
		return false;
	}
	const glm::vec2 p = Xz(registry.Get<const Transform>(target));
	return magic::spell_flock::IsOnCorridor(ctx.entity, p) && IsPosValidForTurnAngle(ctx, p);
}

/// fn_004196D0's "nearer than the stored candidate": 2 x Chebyshev(me, prey) (raw MapCoords) against Chebyshev of my
/// CELL indices and the stored raw MapCoords (fn_0074CF30; the original's unit mix-up: in practice always nearer)
bool NearerThanStored(const Context& ctx, glm::vec2 p)
{
	if (ctx.brain.preyCell == glm::vec2(0.0f))
	{
		return true;
	}
	const glm::ivec2 me(map_coords::ToFixed(ctx.transform.position.x), map_coords::ToFixed(ctx.transform.position.z));
	const glm::ivec2 prey(map_coords::ToFixed(p.x), map_coords::ToFixed(p.y));
	const glm::ivec2 stored(map_coords::ToFixed(ctx.brain.preyCell.x), map_coords::ToFixed(ctx.brain.preyCell.y));
	const glm::ivec2 myCell(map_coords::CellOf(me.x), map_coords::CellOf(me.y)); // the high words
	const int64_t lhs = 2 * static_cast<int64_t>(std::max(std::abs(me.x - prey.x), std::abs(me.y - prey.y)));
	const int64_t rhs = std::max(std::abs(static_cast<int64_t>(stored.x) - myCell.x), std::abs(static_cast<int64_t>(stored.y) - myCell.y));
	return lhs < rhs;
}

/// fn_004196D0 for a villager (info type 2): outside (in the map, drawn), with meat, not already caught or dying
bool IsVillagerPrey(const Context& ctx, entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(entity);
	const auto* info = VillagerInfoOf(entity);
	if (info == nullptr || AltitudeAboveLand(transform) > 2.0f || FlockOf(ctx.animal) == nullptr)
	{
		return false;
	}
	if ((static_cast<uint32_t>(info->foodType) & static_cast<uint32_t>(ctx.info.needsFoodTypes)) == 0 || info->foodValue == 0.0f)
	{
		return false;
	}
	if (const auto* action = registry.TryGet<const LivingAction>(entity); action != nullptr)
	{
		const auto state = action->states[0];
		if (state >= static_cast<uint8_t>(VillagerStates::SetDying) && state <= static_cast<uint8_t>(VillagerStates::BeingEaten))
		{
			return false;
		}
	}
	const glm::vec2 p = Xz(transform);
	if (!IsPosValidForTurnAngle(ctx, p) || (IsChild(ctx) && !IsDowned(entity)))
	{
		return false;
	}
	if (!NearerThanStored(ctx, p))
	{
		return false;
	}
	// JustWholeMapXZ::Init: the prey's exact x, z
	ctx.brain.preyCell = p;
	return true;
}

/// fn_004196D0: is it prey? Another species' animal with meat, on the ground, alive, reachable...
bool IsPrey(const Context& ctx, entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (entity == ctx.entity || !Available(entity))
	{
		return false;
	}
	// +0x25 & 0x40 (0x4196F4): it cannot be eaten; +0x24 & 0x400 (0x41973B): a script's, only for a hunter in a script.
	// Both are tested for the villagers too.
	if (script_held::CannotBeEaten(entity) || !script_held::MayTarget(ctx.entity, entity))
	{
		return false;
	}
	// vt+0xBB4 (0x419715), after IsReachable
	if (!IsHuntingTargetValid(ctx, entity))
	{
		return false;
	}
	if (registry.AllOf<Villager>(entity))
	{
		return IsVillagerPrey(ctx, entity);
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
	// Living::IsSkeleton (0x416FF0): status bit 0x40 (never set for animals: their corpses are prey too)
	if (!IsPosValidForTurnAngle(ctx, p) || (brain->status & 0x40) != 0)
	{
		return false;
	}
	// a cub only takes a downed prey
	if (IsChild(ctx) && (brain->status & 0x80) == 0)
	{
		return false;
	}
	if (!NearerThanStored(ctx, p))
	{
		return false;
	}
	// JustWholeMapXZ::Init: the prey's exact x, z
	ctx.brain.preyCell = p;
	return true;
}

/// fn_00419490: the first prey of a spiral of that many map cells from its own
bool FindPrey(Context& ctx, int cells)
{
	const auto& map = Locator::entitiesMap::value();
	const glm::vec2 me = Xz(ctx.transform);
	// GUtils::Spiral (0x74D7E0 at 0x41953E) over a copy of its own MapCoords (0x41949A..0x4194B6): InBounds 0x4194D6, the
	// cell's object list 0x4194E9, and += 0x41954B (whole cells on the high words, the fraction kept)
	Spiral spiral;
	map_coords::MapCoords coords = map_coords::FromMetres(me);
	for (int i = 0; i < cells; ++i)
	{
		const glm::vec2 c = map_coords::ToMetres(coords);
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
		spiral.Advance(coords);
	}
	return false;
}

/// fn_00419340: the remembered target is still worth chasing
bool CurrentTargetOk(Context& ctx)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto target = ctx.brain.target;
	// the script test at 0x419376 (script_held::MayTarget)
	if (target != entt::null && Available(target) && script_held::MayTarget(ctx.entity, target) &&
	    AltitudeAboveLand(registry.Get<const Transform>(target)) <= 2.0f &&
	    // fn_0074CD50 = GUtils::GetDistanceInMetres 0x74CD70 (HuntingMoveToPos 0x418DB0 / fn_00419340)
	    gutils::GetDistanceInMetres(Xz(ctx.transform), Xz(registry.Get<const Transform>(target))) < ctx.info.huntingDistance)
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
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(entity) && registry.AllOf<DownedVillager>(entity))
	{
		return true;
	}
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
	// MobileWallHug::SetupMobileMoveToObject (0x60ACD0): STEP_THROUGH at it; then Living::SetState (0x5F2A80), raw
	SetupMobileMoveToPos(ctx, Xz(Locator::entitiesRegistry::value().Get<const Transform>(target)));
	ctx.brain.topState = static_cast<uint8_t>(AnimalState::HuntingMoveToPos);
	ctx.brain.turnsSinceStateChange = 0;
}

/// Lion::ReactToAnimalFoodNeeds (0x41FF40), every predator: chase the target seen last turn, else look for one
int ReactToAnimalFoodNeeds(Context& ctx)
{
	if (CurrentTargetOk(ctx))
	{
		SetSpeed(ctx, Speed(ctx.info, 4));
		ctx.brain.chaseStart = Turn();
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
	// vt+0xBB8: SpellWolf::HuntingMoveToPosAbandon 0x420F30: +0x60 (the target), +0xF4 / +0xF8 (the prey cell) = 0,
	// then SetRunToFinalDest
	if (ctx.animal.type == AnimalInfo::SpellWolf)
	{
		ctx.brain.target = entt::null;
		ctx.brain.preyCell = glm::vec2(0.0f);
		SetRunToFinalDest(ctx);
		return;
	}
	ctx.brain.target = entt::null;
	ctx.brain.preyCell = glm::vec2(0.0f);
	SetSpeed(ctx, SpeedDefault(ctx));
	SetTopState(ctx, AnimalState::StartWander);
}

/// fn_0060ADC0: SetupMobileMoveToObject's STEP_THROUGH walk at the object, whose position is the goal each turn
/// [inferred: the goal follows the object]
int StepTowards(Context& ctx, glm::vec2 goal)
{
	ctx.brain.goal = goal;
	if (ctx.brain.moveState != k_MoveFinalStep && ctx.brain.moveState != k_MoveArrived)
	{
		ctx.brain.moveState = k_MoveStepThrough;
	}
	return MoveTo(ctx);
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

void SetRunToFinalDest(Context& ctx)
{
	// SpellWolf::SetRunToFinalDest (0x4209C0): Animal::SetSpeed(scale x speed4 x 1.1), then to the spell's final
	// destination and dead there (SET_DYING)
	ctx.brain.speed = static_cast<uint16_t>(std::min(Scale(ctx) * static_cast<float>(Speed(ctx.info, 4)) * 1.1f, 65535.0f));
	SetAnimalAnimHelper(ctx);
	SetupMoveToPos(ctx, ctx.brain.finalDestination, AnimalState::SetDying);
}

void SpellWolfMoveToPos(Context& ctx)
{
	// SpellWolf::MoveToPos 0x421300, after Living::MoveToPos: hunger (+0xE4) >= info +0x20C (no zero test) -> vt+0xBC0
	// (Lion::ReactToAnimalFoodNeeds 0x41FF40), then within 30 m (0x8BF51C, GUtils::GetDistanceInMetres 0x74CD70) of the
	// final destination (+0x148) -> SetDying (vt+0x6A4, SpellWolf 0x420CF0: the fade)
	if (ctx.brain.hunger >= static_cast<int32_t>(ctx.info.hunger))
	{
		ReactToAnimalFoodNeeds(ctx);
	}
	// (the exe's fixed-point hypotenuse 0x74F680 through water_queries, not the exact float length)
	const glm::vec3 end {ctx.brain.finalDestination.x, 0.0f, ctx.brain.finalDestination.y};
	if (water_queries::GetDistanceInMetres(ctx.transform.position, end) < magic::spell_flock::k_WolfArrive)
	{
		SetDying(ctx.entity, ctx.brain);
	}
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
	// the script test at 0x418DD7 (script_held::MayTarget)
	if (target == entt::null || !Available(target) || !script_held::MayTarget(ctx.entity, target) ||
	    Turn() - ctx.brain.chaseStart >= ctx.info.chaseTime || !IsHuntingTargetValid(ctx, target))
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
		if (gutils::GetAngleDifference(ctx.brain.angle, gutils::GetAngleFromXZ(me, at)) <= 0x80u)
		{
			// saved and restored round the exit from the move state, which drops it
			const auto kept = ctx.brain.target;
			SetTopState(ctx, AnimalState::TargetPounce);
			ctx.brain.target = kept;
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
		speed = (Turn() % 100) < 33 ? Speed(ctx.info, 2) : Speed(ctx.info, 4);
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
	// fn_005EC240: the current move, and on arrival SetTopStateToFinal
	if (StepTowards(ctx, at) == 0xA)
	{
		SetTopState(ctx, static_cast<AnimalState>(ctx.brain.finalState));
	}
	if (glm::distance(Xz(ctx.transform), at) <= 1.0f && !IsDowned(target))
	{
		// fn_005EC480: the prey falls, with 0.05 of its life
		if (auto* villager = registry.TryGet<Villager>(target); villager != nullptr)
		{
			villager->life = 0.05f;
			registry.AssignOrReplace<DownedVillager>(target);
			SetVillagerState(target, VillagerStates::Downed);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animals: villager {} downed by animal {}", static_cast<uint32_t>(target),
			                   static_cast<uint32_t>(ctx.entity));
		}
		else if (auto* prey = PreyBrain(target); prey != nullptr)
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

void ProcessDownedVillagers()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> caught;
	registry.Each<const DownedVillager>([&caught](entt::entity entity, const DownedVillager&) { caught.push_back(entity); });
	for (const auto entity : caught)
	{
		auto* action = registry.TryGet<LivingAction>(entity);
		auto* villager = registry.TryGet<Villager>(entity);
		if (action == nullptr || villager == nullptr)
		{
			registry.Remove<DownedVillager>(entity);
			continue;
		}
		const auto state = static_cast<VillagerStates>(action->states[0]);
		auto& downed = registry.Get<DownedVillager>(entity);
		if (state == VillagerStates::Downed)
		{
			// Living::Downed (0x5EC4B0): its clip (P_ATTACKED_BY_LION), then being eaten for 300 turns
			if (VillagerAnimationDone(entity, action->turnsSinceStateChange))
			{
				SetVillagerState(entity, VillagerStates::BeingEaten);
				downed.counter = 300;
			}
			continue;
		}
		if (state != VillagerStates::BeingEaten)
		{
			// picked up, thrown...: no longer caught
			registry.Remove<DownedVillager>(entity);
			continue;
		}
		if (--downed.counter > 0)
		{
			continue;
		}
		// Villager::BeingEaten (0x76B380): counter 50; +0x25 & 0x40: PlayAnimThenSetState(LANDED 11), it gets up and
		// lives (openblack sets LANDED at once: the BEING_EATEN clip has played for 300 turns [approximated])
		if (script_held::CannotBeEaten(entity))
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animals: villager {} survives being eaten", static_cast<uint32_t>(entity));
			registry.Remove<DownedVillager>(entity);
			SetVillagerState(entity, VillagerStates::Landed);
			continue;
		}
		// Villager::BeingEaten (0x76B380): l = GetLife(); SetLife(0); VillagerDead(ANIMAL 3, GetPlayer() (the villager's:
		// its town's owner), l, 1). ecs::villager kills it at the end of the turn (life::Kill -> ToBeDeleted)
		const float life = villager->life;
		villager->life = 0.0f;
		const auto* town = registry.TryGet<const Town>(villager->town);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animals: villager {} eaten", static_cast<uint32_t>(entity));
		registry.Remove<DownedVillager>(entity);
		ecs::villager::VillagerDead(entity, DeathReason::Animal, town != nullptr ? town->owner : PlayerNames::NEUTRAL, life, 1);
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
	// +0x25 & 0x40 (0x5EC4E0): it survives, PlayAnimThenSetState(LANDED 11, 1): it gets up and lives
	if (script_held::CannotBeEaten(ctx.entity))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animals: animal {} survives being eaten", static_cast<uint32_t>(ctx.entity));
		PlayAnimThenSetState(ctx, AnimalState::Landed);
		return;
	}
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
	// 0x421A0E: a leader in a script hunts from its lair at any hour, without the hunger test
	if (FlockOf(ctx.animal) != nullptr && IsLeader(ctx) && script_held::IsInScript(ctx.entity) &&
	    ReactToAnimalFoodNeeds(ctx) == k_Started)
	{
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
