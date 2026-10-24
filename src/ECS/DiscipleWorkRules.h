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

#include <optional>

#include "Enums.h"

/// The rules of the trader disciple's work once put down: carrying food or wood between its town and the town it trades
/// with. Free of the game's state so they can be tested on their own.
namespace openblack::disciple_work
{

/// The trader carries what the town it trades with wants more: food when it wants food more than wood, else wood when it
/// wants any wood (so wood when it wants both as much); nothing when it wants neither
[[nodiscard]] std::optional<ResourceType> TradeResource(float foodWanted, float woodWanted);

/// Which way a trader goes next: to fetch from its own town's store or to deliver to the other town's
enum class TradeLeg : uint8_t
{
	Fetch,
	Deliver,
};
/// The fuller its load (of `carryLimit`), the more it is drawn to deliver, the emptier, to fetch, each weighed by how
/// near the store is (the game's distance falloff out to five kilometres); fetching wins only when it is drawn more
[[nodiscard]] TradeLeg ChooseTradeLeg(uint32_t held, uint32_t carryLimit, float metresToOwnStore, float metresToTradeStore);

/// How much a trader asks to fetch: its limit less what it holds, worked out in floats and taken as an unsigned count,
/// so a load above the limit asks for a wrapped-round amount, as the game does
[[nodiscard]] uint32_t TradeFetchAmount(uint32_t carryLimit, uint32_t held);

} // namespace openblack::disciple_work
