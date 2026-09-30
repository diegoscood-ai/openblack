/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Alignment.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

#include <spdlog/spdlog.h>

#include "InfoConstants.h"
#include "Locator.h"

namespace
{
struct PlayerAlignment
{
	float value {0.0f};   ///< GAlignment +0x08
	float pending {0.0f}; ///< GAlignment +0x0C, the change gathered this turn
};

/// One per player; like the original's GAlignment it lives with the player, not with the land (no reset on map load).
std::array<PlayerAlignment, static_cast<size_t>(openblack::PlayerNames::_COUNT)> g_players;

PlayerAlignment& Of(openblack::PlayerNames player)
{
	return g_players.at(std::min(static_cast<size_t>(player), g_players.size() - 1));
}

bool Trace()
{
	return std::getenv("OPENBLACK_ALIGNMENT_TRACE") != nullptr;
}
} // namespace

namespace openblack::ecs::alignment
{

float Get(PlayerNames player)
{
	return Of(player).value;
}

void Set(PlayerNames player, float value)
{
	Of(player).value = std::clamp(value, -1.0f, 1.0f);
}

void AddNow(PlayerNames player, float change)
{
	auto& alignment = Of(player);
	alignment.value = std::clamp(alignment.value + change, -1.0f, 1.0f);
}

float Weigh(float alignment, float change)
{
	// same sign (0 counts as positive for both): diminishing; opposite signs: stronger
	const bool sameSign = (alignment < 0.0f) == (change < 0.0f);
	return change * (sameSign ? 1.0f - std::abs(alignment * 0.5f) : 1.0f + std::abs(alignment * 0.5f));
}

void AddPending(PlayerNames player, float change)
{
	Of(player).pending += change;
}

void UpdateForTree(PlayerNames player, bool good)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const float change = Locator::infoConstants::value().player.treePullPutAlignmentChange;
	auto& alignment = Of(player);
	// TODO: CAlignmentHistory::Add 0x415260 (the history the good/evil display and the advisors read)
	const float weighed = Weigh(alignment.value, good ? change : -change);
	alignment.pending += weighed;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Alignment: player {} tree {} {:+.5f} (pending {:+.5f}, alignment {:+.4f})",
		                   static_cast<int>(player), good ? "planted" : "uprooted", weighed, alignment.pending,
		                   alignment.value);
	}
}

void ProcessTurn()
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const float cap = Locator::infoConstants::value().player.maxAlignmentChangePerGameTurn;
	for (size_t i = 0; i < g_players.size(); ++i)
	{
		auto& alignment = g_players[i];
		if (alignment.pending == 0.0f)
		{
			continue;
		}
		// TODO: GGuidance::HelpSpritesAlignmentProcess for the local player (the good and evil advisors)
		const float change = cap * std::clamp(alignment.pending, -1.0f, 1.0f);
		alignment.value = std::clamp(alignment.value + change, -1.0f, 1.0f);
		alignment.pending = 0.0f;
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Alignment: player {} {:+.5f} -> {:+.4f}", i, change, alignment.value);
		}
	}
}

} // namespace openblack::ecs::alignment
