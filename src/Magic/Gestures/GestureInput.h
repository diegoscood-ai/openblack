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
#include "GestureMouseMessages.h"

// Where the gesture samples come from: the mouse (no button needed), the camera and the land under the cursor.
// Wiki: docs/wiki/magic/gestures.md#gestures-the-buffer-and-the-recogniser.

namespace openblack::magic::gestures::sampling
{
/// Screen width / height
[[nodiscard]] float ScreenRatio();
/// The eye and an integer pixel's point on the near plane, as the game finds them now
struct PixelLine
{
	glm::vec3 eye;
	glm::vec3 nearPoint;
};
/// That line, none without a camera, without a screen or with an eye that is not a number
[[nodiscard]] std::optional<PixelLine> LineThroughPixel(glm::ivec2 pixel);
/// The land the interface takes to be under a pixel: the land or near sea along that line, else the sea's level
/// wherever it goes down, kept within reach of the map's middle; none when the line finds neither
[[nodiscard]] std::optional<glm::vec3> InterfaceLandUnderPixel(glm::ivec2 pixel);
/// The land or near sea along that line, at the land's height read at the quantised point; else the point `length`
/// along it
[[nodiscard]] glm::vec3 LandOrPointAlongPixel(glm::ivec2 pixel, float length);
/// The camera services of PacketFromResult, now
[[nodiscard]] Projection CurrentProjection();
/// The camera's position changed this frame
[[nodiscard]] bool CameraMoving();

/// Every frame (real seconds): the camera's position for CameraMoving, then the mouse samples: the frame's mouse
/// messages (GestureMouseMessages.h), a move when the cursor moved and the input thread's own while it stays still,
/// each feeding the buffer once the handler's sum of their ms is past 28 ms.
void Update(float realSeconds);
/// The mouse messages' clock and the handler's sum, as the last frame left them (the gesture window shows them)
[[nodiscard]] const mouse_messages::Clock& MessageClock();

/// OPENBLACK_TEST_GESTURE: pixels played as mouse messages, one per 28 ms, instead of the mouse
void PlayStroke(std::vector<glm::ivec2> pixels);
[[nodiscard]] bool PlayingStroke();
} // namespace openblack::magic::gestures::sampling
