/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "GestureMatch.h"

// Where the gesture samples come from: the mouse (no button needed), the camera and the land under the cursor.
// Wiki: docs/bw1-notes/magic.md, "Gestos".

namespace openblack::magic::gestures::sampling
{
/// Screen width / height (the u16s at 0xE85058 / 0xE8505A)
[[nodiscard]] float ScreenRatio();
/// fn_005E5620 (the land ray of g_game +0x205A20): the landscape (or sea) point under a pixel
[[nodiscard]] std::optional<glm::vec3> ScreenToLand(glm::vec2 pixel);
/// The camera services of PacketFromResult, now
[[nodiscard]] Projection CurrentProjection();
/// GCamera::IsMoving (+0x74, set every frame at 0x442660 when the camera's position changed)
[[nodiscard]] bool CameraMoving();

/// Every frame (real seconds): the camera's position for CameraMoving, then the mouse samples. GGame::MouseHandler
/// 0x54FFE0 adds the time of the mouse events and, past 28 ms, sends CMouse::ProcessPosition 0x61A110's message 0 (the
/// cursor position), whose post handler feeds the buffer (fn_005CEAD0). Here the frames in which the mouse moved count.
void Update(float realSeconds);

/// OPENBLACK_TEST_GESTURE: pixels played as mouse messages, one per 28 ms, instead of the mouse
void PlayStroke(std::vector<glm::ivec2> pixels);
[[nodiscard]] bool PlayingStroke();
} // namespace openblack::magic::gestures::sampling
