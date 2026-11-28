/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include "Creature/TattooEditor.h"

/// The tattoo editor of the options' Players page, which works on a body of its own drawn over the menu
/// (components::TattooMenuCreature): its Edit Tattoo square, its opening on the body read from the profile's physique
/// file, and the body's frame while it is open.
namespace openblack::ecs::tattoo_menu_body
{

/// The Edit Tattoo square shows while the profile's physique file is there, the scripts let the temple show the
/// creature, and leaving the page does not start the game again (tattoo_menu::ButtonShown)
[[nodiscard]] bool ButtonShown(bool restartPending);

/// The square's click, once the page's edits are kept: the body is read from the profile's physique file, or is the
/// stand-in without one, and the editor opens on it with the player's creature's tattoos. False when it can't open
bool Open();

/// Where the dialogs' 800 by 600 space is on the window, in pixels: its top left corner and its size. The editor's
/// view is drawn over it
struct Viewport
{
	glm::vec2 origin {0.0f};
	glm::vec2 size {0.0f};
};

/// The editor's camera: its view from its eye to its focus, and the world camera's projection made 4:3 over the
/// dialogs' space, as the interface's creatures are seen
[[nodiscard]] glm::mat4 View(const creature_tattoo_editor::Orbit& orbit);
[[nodiscard]] glm::mat4 Projection();

/// Each frame the editor is drawn from the Players page, once the body is drawn: the place on it under the pointer, as
/// the body was posed, then its frame (tattoo_menu::Step) by the mouse's buttons and the frame's real milliseconds,
/// its eyes looking where it does. Nothing while the editor shows no body
void Frame(const Viewport& viewport);

} // namespace openblack::ecs::tattoo_menu_body
