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

#include <functional>
#include <optional>

#include <glm/vec3.hpp>

#include "3D/OrientedText.h"

/// The pictures the game draws over a scroll seen close up: a control's key or mouse picture on the line after the text
/// that names the control
namespace openblack::temple_scroll_pictures
{

/// A point as the frame's camera puts it on the screen: its pixel, y from the top, and whether it is on the screen
struct ScreenPoint
{
	float x;
	float y;
	bool onScreen;
};
/// None when the point is nearer than the camera's near plane
using Project = std::function<std::optional<ScreenPoint>(const glm::vec3&)>;

/// Where a control's picture goes on the screen, as the game hands it to its key and mouse draw: across, half its size
/// past the scroll's middle, down, its middle, and its size, in pixels
struct Place
{
	int32_t x;
	int32_t y;
	int32_t size;

	bool operator==(const Place&) const = default;
};

/// A line of a scroll seen close up that pictures a control: the scroll's frame, a texel's size along its across and
/// down axes, the place across the texture of the text line read before it, the line's place down the texture, and the
/// control
struct Line
{
	TextFrame frame;
	float across;
	float down;
	float lineX;
	float lineY;
	int32_t control;
};

/// A control's picture from a line's height on the screen, the scroll's left, top, right and bottom on the screen, and
/// the height on the screen the picture is put from: two lines high and that much higher, and only when it lies within
/// the scroll's top and bottom
[[nodiscard]] std::optional<Place> PlaceAt(float lineHeight, float left, float top, float right, float bottom, float y);

/// The picture of the control named on a scroll's line while the camera is close to the scroll: two lines high on the
/// screen, a line below the line's top, across the middle of the scroll. `lineX` is the place across the texture of the
/// text line read before it, `lineY` the line's place down the texture. None when the line or the scroll's corners don't
/// project, or when the picture would reach past the scroll's top or bottom on the screen.
[[nodiscard]] std::optional<Place> ControlPicture(const TextFrame& frame, float across, float down, float lineX, float lineY,
                                                  const Project& project);

} // namespace openblack::temple_scroll_pictures
