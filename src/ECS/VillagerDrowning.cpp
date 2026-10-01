/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerDrowning.h"

#include <cstdlib>

#include <spdlog/spdlog.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Life.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerSpeed.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs
{
using namespace components;

namespace
{
/// GVillagerInfo::drowningTime (+0x39A in memory, +0x38A in the file): 600 turns for every villager of info.dat
uint16_t DrowningTime(entt::entity villager)
{
	const auto* info = VillagerInfoOf(villager);
	return info != nullptr ? info->drowningTime : static_cast<uint16_t>(600);
}

/// Villager::VillagerDead (0x7506C0) with DEATH_REASON_PLAYER_INTERACTION_DROWN (6). TODO(villager-death): the death
/// states, GAlignment::Update for the reason, the town's counters (fn_73E0A0 / fn_73E440), the guide's texts (0x99A364),
/// CreateDroppedResource / DropWood / DropFood, and the DEAD state (Villager::Dead 0x76A5E0): the fire effect goes,
/// CreateSmokyStuff, and only out of the water fn_00828790: a 12-byte record (global list 0xEB9A7C) with a new
/// LH3DObject of the villager's mesh (the child mesh GVillagerInfo +0x204 under the age +0x138) at its matrix playing
/// the soul's clip, P_DEAD1/2_GOTO_HEAVEN or _HELL (244/245 or 247/248: the pair by the dead clip "M_P_DEAD1", heaven
/// or hell 50 %); then the villager's own mesh becomes mesh 0x1FF PersonSkeletonMale ([0xDCB164]). In the water:
/// smoke and the skeleton, no soul. Until they exist the villager goes through ecs::life::Kill (TODO(villager-death):
/// session mapas will provide villager::Dead(e, DROWNED)).
void VillagerDeadDrowned(entt::entity villager)
{
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Villager {} drowned (DEATH_REASON_PLAYER_INTERACTION_DROWN)",
	                   static_cast<uint32_t>(villager));
	life::Kill(villager, "drowned");
}

LivingAction* ActionOf(entt::entity villager)
{
	return Locator::entitiesRegistry::value().TryGet<LivingAction>(villager);
}

/// stateCounter (+0x58, a u16 shared by DYING / DEAD / DROWNING / BEING_EATEN) = drowningTime; SetTopState(DROWNING)
void StartDrowning(entt::entity villager)
{
	if (auto* action = ActionOf(villager))
	{
		action->turnsUntilStateChange = DrowningTime(villager);
	}
	SetVillagerState(villager, VillagerStates::Drowning);
}
} // namespace

bool HasSunk(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Villager>(entity))
	{
		// Villager::HasSunk 0x750AB0. The DYING branch (flag +0xB4 bit 0, already dead: DYING and
		// dyingTimeWithoutGraveyard) is not reachable in openblack: a villager at 0 life is removed at once.
		StartDrowning(entity);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: villager {} sank: DROWNING for {} turns", static_cast<uint32_t>(entity),
		                   DrowningTime(entity));
		return true;
	}
	if (registry.AllOf<Animal>(entity))
	{
		// Living::HasSunk 0x5ED370: SetDying (vt +0x6A4), SetTopState(LIVING_DEAD 15), ToBeDeleted(0)
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: animal {} sank", static_cast<uint32_t>(entity));
		ToBeDeleted(entity);
		return true;
	}
	return false; // Object::HasSunk 0x637470
}

void VillagerEndPhysicsInWater(entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.TryGet<const Villager>(villager);
	if (component == nullptr)
	{
		return;
	}
	// GetLife (vt +0x11C) > 0 -> stateCounter = drowningTime, lastPlayerToInteract (+0x104) = PhysicsObject::GetPlayer
	// 0x647460 (the player of the GInterfaceStatus at po +0x24: the hand that dropped or threw it, inherited through
	// what it hit; 0 without a PhysicsObject), SetTopState(DROWNING). Its only reader is Drowning's VillagerDead
	// (TODO(players): openblack has no players to keep there). At 0 life Living::IsDead (vt +0xAF4) would give DYING
	// with dyingTimeWithoutGraveyard; openblack has no dead-but-kept villager, so it is the VillagerDead branch
	// (stateCounter 0, the physics' player or the local player g_game +0x205A5B).
	if (life::LifeOf(villager) > 0.0f)
	{
		StartDrowning(villager);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: villager {} at rest in the water: DROWNING for {} turns",
		                   static_cast<uint32_t>(villager), DrowningTime(villager));
		return;
	}
	if (auto* action = ActionOf(villager))
	{
		action->turnsUntilStateChange = 0;
	}
	VillagerDeadDrowned(villager);
}

uint32_t VillagerDrowningState(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	// Called every game turn: GGame::ProcessTurn -> Living::ProcessLiving 0x5EC810 -> ProcessState (vt +0x620,
	// Villager 0x74FF70) -> CallState 0x7521D0; GVillagerInfo::processChecksEvery (+0x2DC) only spaces out
	// CheckEveryTime's periodic checks (0x750518), and the state waits while an into / out-of clip plays (+0xE0 0x800).
	// An INDESTRUCTIBLE villager (Flags +0x24 bit 0x4000, SET_INDESTRUCTABLE) has his counter set to 10 first, so he
	// never gets to 0. The u16 wraps like "dec word ptr [esi + 0x58]" when it was already 0.
	if (registry.AllOf<Indestructible>(villager))
	{
		action.turnsUntilStateChange = 10;
	}
	--action.turnsUntilStateChange;
	if (action.turnsUntilStateChange % 100 == 0 && std::getenv("OPENBLACK_TEST_SEA") != nullptr)
	{
		const auto* animation = registry.TryGet<const SkeletalAnimation>(villager);
		const auto* transform = registry.TryGet<const Transform>(villager);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Sea test: villager {} drowning, {} turns left, clip {}, y {:.2f}",
		                   static_cast<uint32_t>(villager), action.turnsUntilStateChange,
		                   animation != nullptr ? animation->clipIndex : -1, transform != nullptr ? transform->position.y : 0.0f);
	}
	if (action.turnsUntilStateChange == 0)
	{
		VillagerDeadDrowned(villager);
	}
	return 1;
}

bool IsDrowning(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return false;
	}
	if (registry.AllOf<Villager>(entity))
	{
		const auto* action = registry.TryGet<const LivingAction>(entity);
		return action != nullptr &&
		       static_cast<VillagerStates>(action->states[static_cast<size_t>(LivingAction::Index::Top)]) == VillagerStates::Drowning;
	}
	// Object::IsDrowning: flag 0x40 of +0x24 = the object has a PhysicsObject (asleep proxies too; RemoveObject 0x646B56
	// clears it), and the body's centre of mass (po +0xCC, the y of the +0xC8 point) is under 0
	const auto* po = physics::PhysicsObjects::Find(entity);
	return po != nullptr && po->body.Centre().y < 0.0f;
}

} // namespace openblack::ecs
