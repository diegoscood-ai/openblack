/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/fwd.hpp>

#include "Enums.h"

// The trader disciple: it carries food or wood from its own town's store to the store of the town it trades with
// (Villager::tradeTown), whichever that town wants more, and waits by the trade town's centre when it wants neither.
// While it works at the trade town its town is swapped for that one, as the original's work does.

namespace openblack::ecs::components
{
struct LivingAction;
}

namespace openblack::ecs::villager
{
/// No trade town -> 0. The trade town's raw desires (raw + boost + boostA) for food and wood
/// (disciple_work::TradeResource): one of them -> the target cleared, tradeResource set, SetTopState(the leg), 1; neither
/// -> SetTraderNothingToDo
uint32_t CheckTrader(entt::entity villager);
/// The leg (disciple_work::ChooseTradeLeg): the larger load against maxTraderFoodCarried, the distances to its own
/// town's and the trade town's drop-off points -> 198 ARRIVES_AT_STORAGE_PIT_FOR_TRADER_DROP_OFF, else 197 _PICK_UP
[[nodiscard]] VillagerStates CheckTraderPickUpOrDropOff(entt::entity villager, ResourceType type);
/// 197: not food or wood -> 0; ArrivesAtStoragePitForResource(the larger load's type (it rewrites tradeResource's, as
/// the original's does), TradeFetchAmount(the type's maxTrader...Carried, the larger load), 163, 163); 0 ->
/// SetTraderNothingToDo
uint32_t ArrivesAtStoragePitForTraderPickUp(components::LivingAction& action);
/// 198: ArrivesAtStoragePitForDropOff as a villager of the trade town; what it dropped then moves from its own town's
/// carried totals to the trade town's
uint32_t ArrivesAtStoragePitForTraderDropOff(components::LivingAction& action);
/// SetDiscipleNothingToDo as a villager of the trade town (its centre)
uint32_t SetTraderNothingToDo(entt::entity villager);
} // namespace openblack::ecs::villager
