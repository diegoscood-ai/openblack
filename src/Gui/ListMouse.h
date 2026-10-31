/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <optional>

// Where the key binding list draws an item's mouse picture. Pure, so the place is tested without a painter. Wiki:
// docs/wiki/interface/screen-and-readouts.md (the Controls page)
namespace openblack::gui::list_mouse
{

/// The mouse picture's square: its left, its top and its side
struct Place
{
	int x {0};
	int y {0};
	int size {0};
};

/// An item's mouse picture in a list whose box runs from `left` to `right` and from `listTop` to `listBottom`, for an
/// item from `itemTop` to `itemBottom` (a selected item's cut to the list), its text `textWidth` wide and the part
/// before the picture's gap `prefixWidth` wide (both cut to the box's width). The picture stands at the item's top, as
/// high as the text size or the item if that is less, and across from the middle of the box less half the text's
/// width, by the prefix; none when the item is not wholly inside the list.
[[nodiscard]] constexpr std::optional<Place> PlaceOf(int left, int right, int listTop, int listBottom, int itemTop,
                                                     int itemBottom, int textSize, int textWidth, int prefixWidth)
{
	if (itemTop < listTop || itemBottom > listBottom)
	{
		return std::nullopt;
	}
	const int span = right - left;
	const int width = std::min(textWidth, span);
	const int prefix = std::min(prefixWidth, span);
	const int bottom = textSize != 0 && itemBottom - itemTop > textSize ? itemTop + textSize : itemBottom;
	return Place {.x = ((left - width + right) / 2) + prefix, .y = itemTop, .size = bottom - itemTop};
}

} // namespace openblack::gui::list_mouse
