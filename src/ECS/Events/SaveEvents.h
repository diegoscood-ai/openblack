/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::events
{
/// A save the player is to be told of on screen could not be written: the game shows its message box with its text
/// "An error has occurred!" (HELP_TEXT_DIALOG_ERRORHASOCCURED) and an OK, and play goes on under it.
/// See docs/wiki/engine/saved-games.md#the-saves-in-play
struct SaveProblem
{
};
} // namespace openblack::ecs::events
