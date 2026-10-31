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
#include <string>
#include <string_view>

#include "Enums.h"

// What the Villagers debug window shows of a villager's reactions, as text. Pure functions, tested with hand-made data.
namespace openblack::debug::villagers
{

/// "yes" or "no"
[[nodiscard]] std::string_view YesNo(bool value);
/// The player whose interface last let go of a thing, or "none"
[[nodiscard]] std::string LastLetGoText(std::optional<PlayerNames> player);
/// Whether a villager takes an effect now: a hit and a heal, each answered on its own
[[nodiscard]] std::string ReceiverText(bool takesAHit, bool takesAHeal);
/// A reaction the villager follows: its id, its kind's name and number, and the priority it would be taken up at now
/// (none when its kind has no priority of its own here)
[[nodiscard]] std::string ReactionText(uint32_t id, std::string_view name, int type, std::optional<uint8_t> priority);

} // namespace openblack::debug::villagers
