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
#include <string_view>

#include "ECS/SavedSlot.h"

// When the game saves itself in play: the autosave at the end of a turn into the land's circle of three slots, the
// save on exit and when a temple is built (slot 20), the quick save (slot 19), the scripts' save into a slot. The rules
// are pure; the triggers ask the game's services and save through saved_slot. A run of the checks keeps no profile, so
// none of them writes anything there. A save that fails never stops the game: it writes nothing new, logs one line and
// keeps the old files, and the game's message box tells the player when the game would. See
// docs/wiki/engine/saved-games.md#the-saves-in-play.
namespace openblack::ecs::save_triggers
{

/// Turns between two autosaves
inline constexpr uint32_t k_AutoSaveInterval = 6000;
/// The land's autosaves take turns among three slots from 15
inline constexpr uint32_t k_CircleFirstSlot = 15;
inline constexpr uint32_t k_CircleSlotCount = 3;
/// The quick save's slot, and the slot of the save on exit and when a temple is built
inline constexpr uint32_t k_QuickSaveSlot = 19;
inline constexpr uint32_t k_AutoSaveSlot = 20;
/// The player's slots, which the save game room saves into
inline constexpr uint32_t k_RoomSlots = 14;
/// The slot a tap on the scroll of a challenge that saves as it is tapped saves into, per land
inline constexpr uint32_t k_ChallengeScrollSlot = 14;
/// The Gods' Playground, which never autosaves nor saves on exit
inline constexpr uint32_t k_PlaygroundLand = 6;
/// A quick save is asked for again only more than this many turns from the last
inline constexpr int32_t k_QuickSaveGap = 3;

/// What the autosave looks at
struct AutoSaveConditions
{
	/// A script holds the wide screen
	bool scriptWideScreen {false};
	bool networkGame {false};
	uint32_t land {0};
	uint32_t turn {0};
	uint32_t lastAutoSaveTurn {0};
	bool paused {false};
};
/// Whether the autosave saves: not in a script's wide screen, a network game or the Gods' Playground; then when forced,
/// or when more than k_AutoSaveInterval turns went by since the last (counted as unsigned, so a turn below the last
/// counts as a great many) and the game is not paused
[[nodiscard]] bool AutoSaveDue(const AutoSaveConditions& conditions, bool force);
/// The slot of the land's autosave by the count of the land's autosaves so far
[[nodiscard]] constexpr uint32_t CircleSlot(uint32_t count)
{
	return count % k_CircleSlotCount + k_CircleFirstSlot;
}
/// Whether the end of a turn asks for the autosave: the options' autosave is on and the game is not over
[[nodiscard]] constexpr bool EndTurnAsks(bool autoSaveOption, bool gameOver)
{
	return autoSaveOption && !gameOver;
}
/// Whether the quick save key asks for a save: not in a network game, a script's wide screen or once the game is over,
/// and more than k_QuickSaveGap turns from the turn the last was asked for, either way
[[nodiscard]] bool QuickSaveAllowed(bool networkGame, bool scriptWideScreen, bool gameOver, uint32_t lastQuickSaveTurn,
                                    uint32_t turn);
/// Whether leaving the game saves into slot 20: not once the game is over, in a network game or a skirmish, nor on the
/// Gods' Playground
[[nodiscard]] bool ExitSaves(bool gameOver, bool networkGame, bool skirmish, uint32_t land);
/// The slot a script names, popped as a number and cut to a whole one toward zero; none for a slot below 0
[[nodiscard]] std::optional<uint32_t> ScriptSlot(float popped);
/// Whether a save that ended so is told on screen, by the game's message box: a file of the save that could not be
/// written, whoever asked for the save, as the game tells every save whose writing failed; a save openblack could not
/// make (a class it does not write yet) only when the player asked for it (the quick save, the save game room, the
/// debug bar), so an autosave says it in the log alone. A temporary file that did not open is told nowhere, as in the
/// game, nor a save with no slot
[[nodiscard]] constexpr bool ToldOnScreen(saved_slot::SlotSaveOutcome outcome, bool askedByPlayer)
{
	return outcome == saved_slot::SlotSaveOutcome::WriteFailed ||
	       (outcome == saved_slot::SlotSaveOutcome::NotMade && askedByPlayer);
}

/// The autosave, as the game's: when due, the turn is the last autosave's, the slot is the land's next in its circle
/// (the profile's count of the land's autosaves), the game is saved there and the count goes up, whether it was written
/// or not. How the save ended, none when it was not due
std::optional<saved_slot::SlotSave> AutoSave(bool force);
/// The end of a turn: the autosave, when the options' autosave is on and the game is not over
void EndTurn();
/// The quick save key: when allowed, its turn is kept and the quick save is sent as a packet, which the next turn
/// applies
void QuickSaveKey();
/// The quick save's packet saves the game into slot 19
void RegisterPacketHandlers();
/// The player's temple is built: the game saves into slot 20, outside a network game or a skirmish
void TempleBuilt();
/// The game is left: it saves into slot 20 when ExitSaves says so
void Exiting();
/// SAVE_GAME_IN_SLOT: the game saves into the slot the script names
void ScriptSave(float popped);
/// A script finds the scroll of a challenge that saves as it is tapped (GAME_THING_CLICKED): the game saves into slot 14
void ChallengeScrollTapped();
/// A save the player asks for into a slot (the debug bar's, for the save game room): the player's slots as the room
/// saves them under the name given, the others as the game's instant save. How it ended
saved_slot::SlotSave SaveAsked(uint32_t slot, std::u16string_view roomName);

} // namespace openblack::ecs::save_triggers
