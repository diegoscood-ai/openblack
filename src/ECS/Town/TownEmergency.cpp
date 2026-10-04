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
#include "ECS/Components/Town.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Villager/VillagerCore.h"
#include "Locator.h"
#include "Worship/WorshipPercentage.h"

// Town.cpp of runblack.exe W120 (TownEmergency.h)

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

/// Object::IsOnFire 0x637CC0 of the storage pit / the town centre. (openblack, guard) false for an entity that is gone
bool OnFire(entt::entity object)
{
	return object != entt::null && Entities().Valid(object) && fire::IsOnFire(object);
}
} // namespace

void CallAllVillagersToTownEmergency(entt::entity town)
{
	auto& registry = Entities();
	// 0x747898..0x747949: each abode of +0x754 (next +0x9C; town_stats::AbodesOf, the newest first), each inhabitant
	// of +0xA0 (next +0xE4), in list order. (approximate) a copy of each list: the original reads the next villager
	// (+0xE4) after SetTopState, which may move it (the exit functions)
	for (const auto abode : town_stats::AbodesOf(town))
	{
		const auto* a = registry.TryGet<const Abode>(abode);
		if (a == nullptr)
		{
			continue;
		}
		const std::vector<entt::entity> inhabitants = a->inhabitants;
		for ([[maybe_unused]] const auto villager : inhabitants)
		{
			// 0x7478B6..0x74792D: row = 0xD091E8 + 0x90 x (GetFinalState (vt +0xB04) & 0xFF); row.fn and
			// (v ->* row.fn)() != 0 (the this adjustment +0x54 is 0 in every row) -> SetTopState(0xF2) (vt +0x8E8).
			// V11_spec §6.1 / §6.2
			// TODO(Personas HEAD): villager::CallToTownEmergency(villager); (VillagerEmergency.h, V11_spec §10)
		}
	}
	// 0x747951..0x74795B: a debug hook through [0xC22E8C] when [0xC22E84] == 0. (not ported) no effect on the game
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
	// 0x7479A3..0x7479AE: +0xF1C == 0 -> CallAllVillagersToTownEmergency(*this)
	if (t->emergencyStartTurn == 0)
	{
		CallAllVillagersToTownEmergency(town);
		t = TownComponent(town);
		if (t == nullptr)
		{
			return;
		}
	}
	// 0x7479B3..0x7479D5: +0xF1C = g_game +0x205A40 in both branches (literal: at turn 0 it stays "no emergency")
	t->emergencyStartTurn = villager::CurrentTurn();
}

void ProcessTownEmergency(entt::entity town)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	// 0x7477A5..0x7477AC: IsInStateOfEmergency 0x747970
	if (town_queries::IsInStateOfEmergency(*t))
	{
		// 0x7477AE..0x7477C4: +0xEB0 (the last aggression's turn) == g_game +0x205A40 -> SetInStateOfEmergency (the
		// start refreshed while the town is attacked)
		if (t->aggressorTurn == villager::CurrentTurn())
		{
			SetInStateOfEmergency(town);
		}
		// 0x7477C9..0x7477F0: +0x5C0 != 0 (fcomp 0; test ah, 0x40) -> +0xEC4 = +0x5C0; SetWorshipPercentage(0) 0x73C060
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
	// 0x7477F9..0x747826: (pit = GetStoragePit 0x73B5B0) && pit->IsOnFire 0x637CC0, or (c = +0x9A4) && c->IsOnFire ->
	// SetInStateOfEmergency, return. The town centre is not tested for IsAvailable (literal)
	if (OnFire(town_queries::GetStoragePit(town)) || OnFire(t->centre))
	{
		SetInStateOfEmergency(town);
		return;
	}
	// 0x74782F..0x747864: +0xEC4 != 0 && +0x5C0 == 0 -> SetWorshipPercentage(+0xEC4) (the worship comes back)
	const float saved = t->savedWorshipPercentage;
	if (saved != 0.0f && worship::percentage::GetWorshipPercentage(town) == 0.0f)
	{
		worship::percentage::SetWorshipPercentage(town, saved);
	}
	// 0x747869..0x747873: +0xEC4 = 0; +0xF1C = 0 (every turn out of the emergency, also once it has run out)
	if ((t = TownComponent(town)) != nullptr)
	{
		t->savedWorshipPercentage = 0.0f;
		t->emergencyStartTurn = 0;
	}
}

void UpdateAggressor(entt::entity town, std::optional<PlayerNames> causedPlayer)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	// 0x73C9BC..0x73C9DF: GetCausedPlayer; none -> the neutral player. (not ported) 0x73C9E3..0x73CA76: the slot's
	// weight and TownAttackSFX. 0x73CA82: +0xEAC = that player; 0x73CA98: +0xEB0 = g_game +0x205A40
	t->aggressor = causedPlayer.value_or(PlayerNames::NEUTRAL);
	t->aggressorTurn = villager::CurrentTurn();
	// (not ported) 0x73CA9E fn_0073E0F0 (the slot) and 0x73CAA3.. the creature part
}
} // namespace openblack::ecs::town_emergency
