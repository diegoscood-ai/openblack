/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// A list's scroll bar thumb: where it is drawn, where it takes a press and where dragging it scrolls the list to. Pure,
// so the numbers are tested without a painter. Wiki: docs/wiki/interface/screen-and-readouts.md (the menu's lists)
namespace openblack::gui::list_bar
{

/// How far above the list's foot its last item ends when it is scrolled to the end
constexpr int k_EndSpace = 8;

/// The thumb's top and bottom, from the top of the list or of the part of the bar it moves along
struct Thumb
{
	int top {0};
	int bottom {0};
};

/// The thumb along a run of `run` units, for a list `height` tall scrolled by `scroll` over items `content` tall: from
/// the scroll's share of the items to the share of the shown part less the end's space, each cut to a whole unit
[[nodiscard]] constexpr Thumb ThumbAlong(int scroll, int content, int height, int run)
{
	const auto items = static_cast<float>(content);
	const auto along = static_cast<float>(run);
	return {.top = static_cast<int>(static_cast<float>(scroll) / items * along),
	        .bottom = static_cast<int>(static_cast<float>(height + scroll - k_EndSpace) / items * along)};
}

/// The thumb as drawn: along the bar less 3 at each end, from 3 below the list's top
[[nodiscard]] constexpr Thumb DrawnThumb(int scroll, int content, int height)
{
	return ThumbAlong(scroll, content, height, height - 6);
}

/// The thumb as it takes presses, in the dialog's space: along the bar's whole height, from the list's top `listTop`
[[nodiscard]] constexpr Thumb PressedThumb(int scroll, int content, int height, int listTop)
{
	const auto items = static_cast<float>(content);
	const auto along = static_cast<float>(height);
	const auto top = static_cast<float>(listTop);
	return {.top = static_cast<int>((static_cast<float>(scroll) / items * along) + top),
	        .bottom = static_cast<int>((static_cast<float>(height + scroll - k_EndSpace) / items * along) + top)};
}

/// The scroll the thumb grabbed at `grabScroll` is dragged to, `moved` units down: the items' height for the bar's
/// whole height, cut to a whole unit (before the list keeps it between its top and its end)
[[nodiscard]] constexpr int DraggedScroll(int grabScroll, int moved, int content, int height)
{
	return static_cast<int>((static_cast<float>(moved) * static_cast<float>(content) / static_cast<float>(height)) +
	                        static_cast<float>(grabScroll));
}

} // namespace openblack::gui::list_bar
