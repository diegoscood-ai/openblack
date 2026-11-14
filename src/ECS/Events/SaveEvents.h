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

namespace openblack::ecs::events
{
/// A save the player is to be told of on screen could not be written: the game shows its message box with its text
/// "An error has occurred!" (HELP_TEXT_DIALOG_ERRORHASOCCURED) and an OK, and play goes on under it.
/// See docs/wiki/engine/saved-games.md#the-saves-in-play
struct SaveProblem
{
};
/// A save into a slot starts writing: the game shows its "Saving..." banner over the frame on the screen, which stays
/// until the next frame is drawn, as the original's save draws it before writing. Only a save that can write (a profile
/// is kept) sends it. See docs/wiki/engine/saved-games.md#the-saving-banner
struct GameSaving
{
	uint32_t slot {0};
};
/// The quick load's packet was applied in a turn: the game loads the quick save's slot once the frame ends, as the
/// original's loop ends its frame and then loads. See docs/wiki/engine/saved-games.md#the-quick-load
struct QuickLoadAsked
{
};
/// The save game room's load was answered yes and its slot is to be loaded: the game loads it once the frame ends, as the
/// original's loop ends its frame and then loads. See docs/wiki/interface/temple-interior.md#the-save-game-room
struct RoomLoadAsked
{
	uint32_t slot {0};
};
} // namespace openblack::ecs::events
