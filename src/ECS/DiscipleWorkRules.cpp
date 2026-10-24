/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DiscipleWorkRules.h"

#include "Common/GUtilsDistance.h"

using namespace openblack;
using namespace openblack::disciple_work;

namespace
{
/// The distance falloff's reach for the trader's legs, in metres
constexpr float k_TradeReach = 5000.0f;
} // namespace

std::optional<ResourceType> disciple_work::TradeResource(float foodWanted, float woodWanted)
{
	if (foodWanted > woodWanted)
	{
		return ResourceType::Food;
	}
	if (woodWanted == 0.0f)
	{
		return std::nullopt;
	}
	return ResourceType::Wood;
}

TradeLeg disciple_work::ChooseTradeLeg(uint32_t held, uint32_t carryLimit, float metresToOwnStore, float metresToTradeStore)
{
	const float full = static_cast<float>(held) / static_cast<float>(carryLimit);
	const float fetch = gutils::GetDistanceModifier(metresToOwnStore, k_TradeReach) * (1.0f - full);
	const float deliver = gutils::GetDistanceModifier(metresToTradeStore, k_TradeReach) * full;
	return fetch <= deliver ? TradeLeg::Deliver : TradeLeg::Fetch;
}

uint32_t disciple_work::TradeFetchAmount(uint32_t carryLimit, uint32_t held)
{
	// the difference of two floats, truncated through a 64-bit integer and kept as its low 32 bits
	const float room = static_cast<float>(carryLimit) - static_cast<float>(held);
	return static_cast<uint32_t>(static_cast<int64_t>(room));
}
