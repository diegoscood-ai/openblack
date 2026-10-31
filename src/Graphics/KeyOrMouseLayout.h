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

#include <array>

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

/// The cell of the mouse picture (4 x 4 cells, numbered across then down) and whether it is drawn mirrored
struct MouseCell
{
	int32_t cell {0};
	bool mirrored {false};
};

/// The mouse picture's cell for a click type: `row` the picture's row (the kind of mouse), `lit` whether the button is
/// lit this frame and `wheelFrame` the frame's tenth of a second out of five, for the wheel's frames. 1 and anything
/// else is the right button's cell mirrored, the left button
[[nodiscard]] constexpr MouseCell MousePictureCell(int32_t clickType, int32_t row, bool lit, int32_t wheelFrame)
{
	MouseCell out;
	switch (clickType)
	{
	case 0:
		out.cell = 4 * row;
		break;
	case 2:
		out.cell = 4 * row + (lit ? 1 : 0);
		break;
	case 3:
		out.cell = 4 * row + (lit ? 2 : 0);
		break;
	case 4:
		out.cell = 4 * row + (lit ? 3 : 0);
		break;
	case 8:
		out.cell = wheelFrame <= 3 ? 15 - wheelFrame : 11; // the wheel down frames
		break;
	case 0x10:
		out.cell = wheelFrame <= 3 ? 12 + wheelFrame : 11; // the wheel up frames
		break;
	default:
		out.mirrored = true;
		out.cell = 4 * row + (lit ? 1 : 0);
		break;
	}
	return out;
}

/// One piece of a glow panel: its rectangle in pixels and its part of the texture
struct PanelSlice
{
	float x0 {0.0f};
	float y0 {0.0f};
	float x1 {0.0f};
	float y1 {0.0f};
	float u0 {0.0f};
	float v0 {0.0f};
	float u1 {0.0f};
	float v1 {0.0f};
};

/// The soft glow panel behind a key or mouse icon or its words, over the box (x0, y0)-(x1, y1): a 9-slice of the soft
/// blob (0, 0.25)-(0.24609375, 0.49609375) of atmos.raw, row by row from the top left. b is a quarter of the box's
/// height, cut towards zero; each corner is a 2b square centred on the box's corner, the edges and the middle between
[[nodiscard]] constexpr std::array<PanelSlice, 9> PanelSlices(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
	const int32_t b = (y1 - y0) / 4;
	const std::array<float, 4> xs {static_cast<float>(x0 - b), static_cast<float>(x0 + b), static_cast<float>(x1 - b),
	                               static_cast<float>(x1 + b)};
	const std::array<float, 4> ys {static_cast<float>(y0 - b), static_cast<float>(y0 + b), static_cast<float>(y1 - b),
	                               static_cast<float>(y1 + b)};
	// the breaks: u 0, 0.078125, 0.16796875, 0.24609375; v 0.25, 0.328125, 0.41796875, 0.49609375
	constexpr std::array<float, 4> k_U {0.0f, 0.078125f, 0.16796875f, 0.24609375f};
	constexpr std::array<float, 4> k_V {0.25f, 0.328125f, 0.41796875f, 0.49609375f};
	std::array<PanelSlice, 9> slices {};
	for (size_t row = 0; row < 3; ++row)
	{
		for (size_t column = 0; column < 3; ++column)
		{
			slices.at(row * 3 + column) = {
			    .x0 = xs.at(column),
			    .y0 = ys.at(row),
			    .x1 = xs.at(column + 1),
			    .y1 = ys.at(row + 1),
			    .u0 = k_U.at(column),
			    .v0 = k_V.at(row),
			    .u1 = k_U.at(column + 1),
			    .v1 = k_V.at(row + 1),
			};
		}
	}
	return slices;
}

} // namespace openblack::graphics::key_or_mouse
