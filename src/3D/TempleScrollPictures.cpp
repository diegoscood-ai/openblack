/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleScrollPictures.h"

#include <cstdlib>

#include "3D/TempleScroll.h"

using namespace openblack;

std::optional<temple_scroll_pictures::Place> temple_scroll_pictures::PlaceAt(float lineHeight, float left, float top,
                                                                             float right, float bottom, float y)
{
	// Two lines high, at both of the line's scales of one
	const float side = ((1.0f * 1.0f) * lineHeight) * 2.0f;
	const float from = y - side;
	const float to = side + from;
	if (from <= top || to >= bottom)
	{
		return std::nullopt;
	}
	const float middle = (left + right) * 0.5f;
	return Place {.x = static_cast<int32_t>((side * 0.5f) + middle),
	              .y = static_cast<int32_t>(((to + from) * 0.5f) - (side * 0.2f)),
	              .size = static_cast<int32_t>(side)};
}

std::optional<temple_scroll_pictures::Place> temple_scroll_pictures::ControlPicture(const TextFrame& frame, float across,
                                                                                    float down, float lineX, float lineY,
                                                                                    const Project& project)
{
	// A place on the texture, from its middle, in the scroll's frame: across along its z axis, down along its y
	const auto at = [&frame, across, down](float u, float v) {
		const float a = u * across;
		const float b = v * down;
		return ((a * frame.axes[2]) + frame.origin) + (b * frame.axes[1]);
	};
	constexpr float k_Middle = static_cast<float>(TempleScrollTexture::k_Size) * 0.5f;
	// The texture's corners must be in front of the camera, and the line's top and the line below on the screen
	const auto first = project(at(-k_Middle, -k_Middle));
	const auto last = project(at(k_Middle, k_Middle));
	const glm::vec3 top = at(lineX - k_Middle, lineY - k_Middle);
	const auto lineTop = project(top);
	const auto lineBottom = project(top + ((static_cast<float>(TempleScrollTexture::k_LineHeight) * frame.axes[1]) * down));
	if (!first || !last || !lineTop || !lineTop->onScreen || !lineBottom || !lineBottom->onScreen)
	{
		return std::nullopt;
	}
	// The line's height on the screen, whole pixels; the picture is put from two of them and one more below the line's top
	const auto height = static_cast<float>(std::abs(static_cast<int32_t>(lineBottom->y - lineTop->y)));
	const float y = ((height + height) + lineTop->y) + height;
	return PlaceAt(height, first->x, first->y, last->x, last->y, y);
}
