/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>

// The Tab keys of the camera's bookmarks, as pure rules: Ctrl and Tab step to the next set bookmark, Shift and Tab to
// the one before, and Tab alone flies to the player's next town. Tested without the game.

namespace openblack::bookmark_steps
{

/// The steps go round this many slots: the eight bookmarks the keys and the markers have (0 to 7), the town's (8), one
/// with no use (9) and the view before a jump (10). Only the first eight can be stepped to.
inline constexpr size_t k_Slots = 11;
inline constexpr size_t k_SteppedSlots = 8;
/// The slot Tab alone keeps the town it flies to in, which then is the current bookmark. It is never drawn nor stepped to
inline constexpr uint8_t k_TownSlot = 8;

/// Ctrl and Tab: the first set slot after the current one, round all the slots, of those that can be stepped to (the
/// current one again, last, when it is the only one set); none when none is set. `isSet` is read for 0 to 7
[[nodiscard]] std::optional<uint8_t> Next(uint8_t current, std::span<const bool, k_SteppedSlots> isSet);
/// Shift and Tab: the first set slot before the current one, round all the slots, of those that can be stepped to;
/// never the current one itself
[[nodiscard]] std::optional<uint8_t> Previous(uint8_t current, std::span<const bool, k_SteppedSlots> isSet);

/// One of the player's towns as Tab alone sees it
struct Town
{
	/// The number the town was made with
	uint32_t id {0};
	/// Whether it has a storage pit or a town centre to fly to
	bool hasPlace {false};
};
/// Tab alone: which of the player's towns (given from the head of the player's list to its tail) the camera flies to.
/// The towns are walked from the tail: past the town flown to last (by its number), or from the tail when there is
/// none, the town before it in the list (the tail after the head) is taken, unless it has no place, when the walk goes
/// on from the town after that one. When no town has the number, the walk is made again as if there were none. None
/// for no town, or no town found with a place.
[[nodiscard]] std::optional<size_t> NextTown(std::span<const Town> headToTail, std::optional<uint32_t> lastFlownTo);

/// Where Tab alone flies the camera to: from this far off the town's place (to the west, up and to the south), looking
/// at that place
inline constexpr float k_TownViewX = -24.45f;
inline constexpr float k_TownViewY = 28.3786f;
inline constexpr float k_TownViewZ = 47.4f;

} // namespace openblack::bookmark_steps
