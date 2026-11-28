/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerEaten.h"

#include <spdlog/spdlog.h>

#include "ECS/BeingEatenRules.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Villager.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerScript.h"
#include "Locator.h"

namespace openblack::ecs
{
using components::DownedVillager;
using components::LivingAction;
using components::Villager;
namespace rules = being_eaten_rules;

namespace
{
/// The clip it plays now plays on to its end, then the next state: waiting for the clip keeps the turns since the state
/// changed, which time the clip, as the builder's wait does
void PlayOnThenSetState(LivingAction& action, VillagerStates next)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	const uint16_t turns = action.turnsSinceStateChange;
	villager::PlayAnimThenSetState(villager, next);
	if (auto* still = registry.TryGet<LivingAction>(villager); still != nullptr)
	{
		still->turnsSinceStateChange = turns;
	}
}
} // namespace

uint32_t VillagerDownedState(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	// the clip of the fall plays on to its end, then it is eaten; the wait counts from the clip's start, the turn the
	// villager was downed, as the clip itself plays on
	PlayOnThenSetState(action, VillagerStates::BeingEaten);
	action.turnsUntilStateChange = rules::k_BeingEatenTurns;
	// (debug readout) the fall under the attack, in the game's log once while it stays downed
	if (auto* downed = registry.TryGet<DownedVillager>(villager); downed != nullptr && !downed->fallTold)
	{
		downed->fallTold = true;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animals: villager {} has fallen under the attack, its fall playing on",
		                   static_cast<uint32_t>(villager));
	}
	return 1;
}

uint32_t VillagerBeingEatenState(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	// (debug readout) its first turn of being eaten, once the fall's clip has played, in the game's log
	if (action.turnsUntilStateChange == rules::k_BeingEatenTurns)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animals: villager {} is being eaten", static_cast<uint32_t>(villager));
	}
	const auto step = rules::BeingEatenTurn(action.turnsUntilStateChange, script_held::CannotBeEaten(villager));
	action.turnsUntilStateChange = step.turnsLeft;
	switch (step.result)
	{
	case rules::EatenTurn::Eaten:
		break;
	case rules::EatenTurn::GetsUp:
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animals: villager {} survives being eaten", static_cast<uint32_t>(villager));
		registry.Remove<DownedVillager>(villager);
		PlayOnThenSetState(action, VillagerStates::Landed);
		break;
	case rules::EatenTurn::Dies:
	{
		// its life, then none, through the villager's life setter; it dies killed by an animal, its own player's loss
		const auto* living = registry.TryGet<Villager>(villager);
		const float life = living != nullptr ? living->life : 0.0f;
		life::SetLife(villager, 0.0f);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animals: villager {} eaten", static_cast<uint32_t>(villager));
		registry.Remove<DownedVillager>(villager);
		villager::VillagerDead(villager, DeathReason::Animal, villager::GetPlayerOf(villager), life, 1);
		break;
	}
	}
	return 1;
}

} // namespace openblack::ecs
