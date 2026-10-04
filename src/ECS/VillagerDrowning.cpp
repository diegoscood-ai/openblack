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
#include <unordered_map>

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
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerScript.h"
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

/// lastPlayerToInteract (+0x104): Villager::EndPhysics 0x5F0BAF writes PhysicsObject::GetPlayer 0x647460 there (the
/// player of the hand that dropped or threw it, inherited through what hit it). openblack has one hand, the local
/// player PLAYER_ONE (inferido: no other players drop things yet), so a body thrown by the hand gives PLAYER_ONE.
std::unordered_map<entt::entity, PlayerNames> g_lastPlayerToInteract;

/// Villager::Drowning 0x76A7A0..0x76A7CB: VillagerDead(DEATH_REASON_PLAYER_INTERACTION_DROWN 6, player, 0.01f
/// (0x3C23D70A), 1), player = GetPlayerWhoLastDroppedMe (vt +0x6C)->GetPlayer (vt +0x1C), else lastPlayerToInteract
/// (+0x104); NEUTRAL when there is none. The death itself (states, alignment, counters, soul and skeleton, smoke) is
/// ecs::villager::VillagerDead (session mapas, 0x7506C0). GetPlayerWhoLastDroppedMe: openblack keeps no dropper
/// apart from the physics' player, which is what +0x104 already holds, so both give the same player here.
void VillagerDeadDrowned(entt::entity villager)
{
	PlayerNames player = PlayerNames::NEUTRAL;
	if (const auto found = g_lastPlayerToInteract.find(villager); found != g_lastPlayerToInteract.end())
	{
		player = found->second;
		g_lastPlayerToInteract.erase(found);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Villager {} drowned (DEATH_REASON_PLAYER_INTERACTION_DROWN, player {})",
	                   static_cast<uint32_t>(villager), static_cast<int>(player));
	villager::VillagerDead(villager, DeathReason::PlayerInteractionDrown, player, 0.01f, 1);
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

void RememberLastPlayerToInteract(entt::entity villager, bool byPlayer)
{
	if (!Locator::entitiesRegistry::value().AllOf<Villager>(villager))
	{
		return;
	}
	if (byPlayer)
	{
		g_lastPlayerToInteract[villager] = PlayerNames::PLAYER_ONE;
	}
	else
	{
		g_lastPlayerToInteract.erase(villager);
	}
}

bool HasSunk(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Villager>(entity))
	{
		// Villager::HasSunk 0x750AB0: 0x750AB5 IsAvailable (vt +0x2C) == 0 -> 0 (not sunk). (not ported) 0x750AC1..0x750AED
		// the player who last dropped it: ConsiderMakingCreatureMimicPlayer
		if (!villager::IsAvailable(entity))
		{
			return false;
		}
		// 0x750AF5..0x750B0F: the status bit alone (+0xB4 & 1, not Living::IsDead) -> SetTopState(14 DYING) and the
		// counter = dyingTimeWithoutGraveyard (+0x290, 0x750B1E)
		const auto& component = registry.Get<const Villager>(entity);
		if ((component.status & Villager::k_StatusDead) != 0)
		{
			villager::SetTopState(entity, VillagerStates::Dying);
			if (auto* action = ActionOf(entity))
			{
				action->turnsUntilStateChange = static_cast<uint16_t>(villager::InfoOf(entity).dyingTimeWithoutGraveyard);
			}
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: dead villager {} sank: DYING", static_cast<uint32_t>(entity));
			return true;
		}
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
	// (TODO(players): openblack has no players to keep there). At 0 life (0x5F0BC0..0x5F0C41): Living::IsDead (vt +0xAF4)
	// -> SetTopState(14 DYING) and the counter = dyingTimeWithoutGraveyard (+0x290; never the graveyard's time); else the
	// counter 0 and VillagerDead (the physics' player or the neutral player g_game +0x205A5B, 0.01, 1).
	if (life::LifeOf(villager) > 0.0f)
	{
		StartDrowning(villager);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: villager {} at rest in the water: DROWNING for {} turns",
		                   static_cast<uint32_t>(villager), DrowningTime(villager));
		return;
	}
	if (villager::IsDead(villager))
	{
		villager::SetTopState(villager, VillagerStates::Dying);
		if (auto* action = ActionOf(villager))
		{
			action->turnsUntilStateChange = static_cast<uint16_t>(villager::InfoOf(villager).dyingTimeWithoutGraveyard);
		}
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
