/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

// What shows a disciple: an orange glow over every disciple of a kind that glows (the disciple table's marker), and the
// symbol of its work, in yellow, over the one in the player's hand or under the pointer. Drawn as sprites
// (components::Sprite with components::DiscipleMark), made only while there are disciples.

namespace openblack::ecs::disciple_marks
{
/// Every frame, once the villagers' drawn positions are set: the glows follow their disciples where they are drawn and
/// their puffs turn with the game's time; a glow whose villager is gone or no longer glows goes. The symbol of the
/// disciple in the hand, or under the pointer when the hand holds none, stands over its glow, as wide on the screen
/// wherever it is (`eye`: the camera, `screenWidth`: in pixels)
void Update(const glm::vec3& eye, float screenWidth);

/// The debug switch: the glows and the symbol drawn (on by default). Off, the marks are taken away
void SetShown(bool shown);
[[nodiscard]] bool IsShown();
} // namespace openblack::ecs::disciple_marks
