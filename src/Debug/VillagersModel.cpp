/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagersModel.h"

#include <fmt/format.h>

using namespace openblack;

std::string_view debug::villagers::YesNo(bool value)
{
	return value ? "yes" : "no";
}

std::string debug::villagers::LastLetGoText(std::optional<PlayerNames> player)
{
	return player ? fmt::format("player {}", static_cast<int>(*player)) : std::string("none");
}

std::string debug::villagers::ReceiverText(bool takesAHit, bool takesAHeal)
{
	return fmt::format("a hit: {}, a heal: {}", YesNo(takesAHit), YesNo(takesAHeal));
}

std::string debug::villagers::ReactionText(uint32_t id, std::string_view name, int type, std::optional<uint8_t> priority)
{
	const auto taken = priority ? fmt::format("priority now {}", *priority) : std::string("no priority of its own here");
	return fmt::format("Reaction {}: {} ({}), {}", id, name, type, taken);
}
