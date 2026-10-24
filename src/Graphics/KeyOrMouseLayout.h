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

#include <glm/vec2.hpp>

#include "Common/TruncateToInt.h"

// Where a key or mouse icon goes on the screen (Renderer::DrawKeyOrMouse): its box at the hand, kept on the screen, and
// the side of the hand it is laid out on. Pure, so the layout is tested without a renderer. Wiki: the drawing in
// docs/wiki/interface/tooltips.md
namespace openblack::graphics::key_or_mouse
{

/// The box's top left at the hand's point on the screen plus (x, y), the hand's point cut to whole pixels: y kept in
/// [0, height - s], then x in [0, width - s]
[[nodiscard]] inline glm::ivec2 BoxAtTheHand(glm::vec2 hand, int32_t x, int32_t y, int32_t s, int32_t width, int32_t height)
{
	int32_t boxX = static_cast<int32_t>(hand.x) + x;
	int32_t boxY = static_cast<int32_t>(hand.y) + y;
	boxY = boxY < 0 ? 0 : boxY;
	boxY = boxY > height - s ? height - s : boxY;
	boxX = boxX < 0 ? 0 : boxX;
	boxX = boxX > width - s ? width - s : boxX;
	return {boxX, boxY};
}

/// The box's left edge and whether it is right-aligned
struct Horizontal
{
	int32_t x {0};
	bool rightAligned {false};
};

/// The horizontal layout of a box `total` wide whose anchor is boxX. align & 1 right-aligns it (it ends at boxX);
/// align & 2 picks the side of the anchor with the hysteresis `side`, shared by every icon: 1 once boxX is past 2/3 of
/// the screen's width, 0 once it is under 1/3, unchanged in between. Side 1 ends s / 2 right of the anchor, side 0
/// starts s / 2 left of it
[[nodiscard]] constexpr Horizontal Layout(int32_t boxX, float total, int32_t s, uint32_t align, int32_t width, int32_t& side)
{
	Horizontal out {.x = boxX, .rightAligned = (align & 1) != 0};
	if ((align & 2) != 0)
	{
		if (boxX > 2 * width / 3)
		{
			side = 1;
		}
		if (boxX < width / 3)
		{
			side = 0;
		}
		if (side != 0)
		{
			out.x = TruncateToInt(static_cast<float>(boxX + s / 2) - total);
			out.rightAligned = true;
		}
		else
		{
			out.x = boxX - s / 2;
			out.rightAligned = false;
		}
	}
	else if (out.rightAligned)
	{
		out.x = TruncateToInt(static_cast<float>(boxX) - total);
	}
	return out;
}

} // namespace openblack::graphics::key_or_mouse
