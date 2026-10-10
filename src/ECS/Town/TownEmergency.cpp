/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownEmergency.h"

#include <vector>

#include <spdlog/spdlog.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/TownAggression.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerEmergency.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/WorshipPercentage.h"

// The town's emergency (TownEmergency.h)

namespace openblack::ecs::town_emergency
{
using namespace components;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Town* TownComponent(entt::entity town)
{
	auto& registry = Entities();
	return town != entt::null && registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr;
}

/// Whether the storage pit / the town centre is on fire. (openblack, guard) false for an entity that is gone
bool OnFire(entt::entity object)
{
	return object != entt::null && Entities().Valid(object) && fire::IsOnFire(object);
}
} // namespace

void CallAllVillagersToTownEmergency(entt::entity town)
{
	auto& registry = Entities();
	// Each abode of the town (town_stats::AbodesOf, the newest first), each inhabitant, in list order. (approximate) a
	// copy of each list: the original reads the next villager after SetTopState, which may move it (the exit
	// functions)
	for (const auto abode : town_stats::AbodesOf(town))
	{
		const auto* a = registry.TryGet<const Abode>(abode);
		if (a == nullptr)
		{
			continue;
		}
		const std::vector<entt::entity> inhabitants = a->inhabitants;
		for (const auto villager : inhabitants)
		{
			// The final state's row test and SetTopState(242) are villager::CallToTownEmergency's (VillagerEmergency.h)
			villager::CallToTownEmergency(villager);
		}
	}
	// The original also runs a debug hook here. (not ported) no effect on the game
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		if (const auto* t = TownComponent(town); t != nullptr)
		{
			SPDLOG_LOGGER_DEBUG(logger, "Town {}: emergency, the housed villagers called", t->id);
		}
	}
}

void SetInStateOfEmergency(entt::entity town)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	// No emergency running -> CallAllVillagersToTownEmergency
	if (t->emergencyStartTurn == 0)
	{
		CallAllVillagersToTownEmergency(town);
		t = TownComponent(town);
		if (t == nullptr)
		{
			return;
		}
	}
	// The start = the game turn in both branches (literal: at turn 0 it stays "no emergency")
	t->emergencyStartTurn = villager::CurrentTurn();
}

void ProcessTownEmergency(entt::entity town)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	if (town_queries::IsInStateOfEmergency(*t))
	{
		// The last aggression's turn is this turn -> SetInStateOfEmergency (the start refreshed while the town is
		// attacked)
		if (t->aggression.lastTurn == villager::CurrentTurn())
		{
			SetInStateOfEmergency(town);
		}
		// A worship percentage != 0 -> saved; SetWorshipPercentage(0)
		const float worship = worship::percentage::GetWorshipPercentage(town);
		if (worship != 0.0f)
		{
			if ((t = TownComponent(town)) != nullptr)
			{
				t->savedWorshipPercentage = worship;
			}
			worship::percentage::SetWorshipPercentage(town, 0.0f);
		}
		return;
	}
	// The storage pit or the town centre on fire -> SetInStateOfEmergency, return. The town centre is not tested for
	// IsAvailable (literal)
	if (OnFire(town_queries::GetStoragePit(town)) || OnFire(t->centre))
	{
		SetInStateOfEmergency(town);
		return;
	}
	// A saved percentage and the current one 0 -> SetWorshipPercentage(saved) (the worship comes back)
	const float saved = t->savedWorshipPercentage;
	if (saved != 0.0f && worship::percentage::GetWorshipPercentage(town) == 0.0f)
	{
		worship::percentage::SetWorshipPercentage(town, saved);
	}
	// Clear the saved percentage and the start (every turn out of the emergency, also once it has run out)
	if ((t = TownComponent(town)) != nullptr)
	{
		t->savedWorshipPercentage = 0.0f;
		t->emergencyStartTurn = 0;
	}
}

void UpdateAggressor(entt::entity town, std::optional<PlayerNames> causedPlayer, float aggression)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	// The caused player; none -> the neutral player. The harm counts as the town's own player's when the aggressor
	// owns the town (a neutral town's own is the neutral player)
	const auto aggressor = causedPlayer.value_or(PlayerNames::NEUTRAL);
	const float firstTimeAddition =
	    Locator::infoConstants::has_value() ? Locator::infoConstants::value().town.firstTimeDamageDoneAddition : 0.0f;
	town_aggression::Attacked(t->aggression, aggressor, aggressor == t->owner, aggression, firstTimeAddition,
	                          villager::CurrentTurn());
}

void AttackTown(entt::entity object, float damage, std::optional<PlayerNames> causedPlayer)
{
	const auto town = town_queries::GetTown(object);
	if (TownComponent(town) == nullptr)
	{
		return;
	}
	// The object's own info: an abode's (a field's too) is its abode info, the other kinds' their object info
	const GObjectInfo* info = fire::traits::AbodeInfo(object);
	if (info == nullptr)
	{
		info = fire::traits::InfoOf(object);
	}
	const float value = info != nullptr ? info->aggressorValue : 0.0f;
	UpdateAggressor(town, causedPlayer, town_aggression::AggressionFromDamage(damage, value));
}

std::optional<PlayerNames> HitterPlayer(entt::entity hitter)
{
	auto& registry = Entities();
	if (hitter == entt::null || !registry.Valid(hitter))
	{
		return std::nullopt;
	}
	if (registry.AllOf<Villager>(hitter))
	{
		return villager::GetPlayerOf(hitter);
	}
	if (const auto* creature = registry.TryGet<const Creature>(hitter))
	{
		return creature->owner;
	}
	if (registry.AllOf<Tree>(hitter))
	{
		return std::nullopt;
	}
	// A rock, a fragment or a felled tree answers the player it was made with, the neutral player when it was made
	// with none; openblack keeps no such player, so they answer the neutral player (pending, see the wiki). The other
	// things in the physics answer the neutral player
	return PlayerNames::NEUTRAL;
}
} // namespace openblack::ecs::town_emergency
