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

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Systems/SaveGameSystemInterface.h"

namespace openblack::ecs
{
class Registry;
}

// A save of the game into a slot without the save room, as the game's instant save does it: the slot's title and
// the date and time of the save, the pictures, the world file from the entities, the creatures' mind files and the
// script machine's state, written through the save service. See docs/wiki/engine/saved-games.md#openblack.
namespace openblack::ecs::saved_slot
{

/// The help text each slot's title is, by slot: none (the database's first text) for the player's slots, then the
/// previous land, the three land autosaves, the next land, the quick save and the autosave on exit
inline constexpr std::array<uint32_t, 21> k_SlotTitleTexts {0, 0, 0, 0,      0,      0,      0,      0,      0,      0,     0,
                                                            0, 0, 0, 0x1A20, 0x1A22, 0x1A22, 0x1A22, 0x1A21, 0x1A05, 0x1A23};
/// The most characters, each text's end included, the time and the date of a save may take together; past it the
/// save has no date
inline constexpr size_t k_DateRoom = 62;

/// A save's date: the time, a space and the date, as the locale writes them, when both fit with their ends in
/// k_DateRoom characters, else nothing
[[nodiscard]] std::u16string DateText(std::u16string_view time, std::u16string_view date);
/// The time and the long date now, as the player's locale writes them
[[nodiscard]] std::u16string NowText();
/// The slot's title: its help text with the land's number written into it
[[nodiscard]] std::u16string TitleOf(uint32_t slot, uint32_t land);

/// The creatures in the order of the game's list of creatures: the newest first (a creature goes to the list's head
/// as it is set up), by their creation numbers
[[nodiscard]] std::vector<entt::entity> CreaturesInListOrder(const Registry& registry);
/// Each creature's mind file, by its place in that list, as the creature is now; a creature whose mind is not set up
/// has none
[[nodiscard]] std::vector<std::pair<uint32_t, std::vector<uint8_t>>> CreatureMinds();

/// How a save of a slot ended
enum class SlotSaveOutcome
{
	/// Its files were written
	Written,
	/// No slot exists (no profile kept, as in a run of the checks): nothing was made
	NoSlot,
	/// The save could not be made: a world file that cannot be made whole (an object of a class openblack does not
	/// write yet) or a step that stopped before any file. Nothing was written
	NotMade,
	/// The world file's temporary file did not open: nothing more was written and the old files stay
	NotOpened,
	/// A file of the save could not be written: the old world files stay and no temporary file is left
	WriteFailed,
};

/// A save of a slot: how it ended, and why when it was not written
struct SlotSave
{
	SlotSaveOutcome outcome {SlotSaveOutcome::NoSlot};
	/// What stopped it, for the log; empty when it was written or no slot exists
	std::string failure;

	[[nodiscard]] bool Written() const noexcept { return outcome == SlotSaveOutcome::Written; }
};

/// Saves the game into the slot with the title given, through the Locator's services: the date and time, the pictures,
/// the world file from the entities and what the last load kept, the creatures' minds and the scripts' state; nothing
/// is made with no profile kept, and nothing is written when the world file cannot be made whole. It never throws:
/// what stops it is its outcome
SlotSave SaveSlot(uint32_t slot, std::u16string title);
/// Saves the game into the slot as the game's instant save does: SaveSlot with the slot's title
SlotSave InstantSave(uint32_t slot);
/// Saves the game into the slot as the save game room's save does: SaveSlot with the name the player gave it; a save
/// written counts in the profile's statistics
SlotSave RoomSave(uint32_t slot, std::u16string_view name);

} // namespace openblack::ecs::saved_slot
