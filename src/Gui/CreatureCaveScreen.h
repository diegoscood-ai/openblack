/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::gui
{

/// Draws the Creature Cave's screen while it is open, with ImGui: a page for each of the cave's four scrolls, written as
/// the temple's scrolls are, with what the creature has learnt to do and not to do and how far it has learnt each
/// miracle, and a page for its tattoos. It is the 2D stand-in for the parts of the creature's room the temple doesn't
/// show yet, the creature itself above all, which a click on opens the tattoo editor in the game.
void DrawCreatureCaveScreen();

/// Opens the tattoo editor on the player's creature without the cave's camera, for the cave's screen and the debug
/// menu: false, and nothing opened, without the editor, a creature or while it is open already
bool OpenTattooEditorOnCreature();

} // namespace openblack::gui
