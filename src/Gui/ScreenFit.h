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

/// The game's sizes in pixels, made for its default screen, on a screen of any size and shape
namespace openblack::gui
{

/// The game's default screen (when no resolution has been chosen), which its sizes in pixels were made for
inline constexpr int32_t k_ReferenceWidth = 800;
inline constexpr int32_t k_ReferenceHeight = 600;

/// How many pixels a pixel of the default screen becomes on a screen: the width of a 4:3 screen as tall as it, or its
/// own width when it is narrower than that, over the default screen's width. 1 on the default screen. Sizes in pixels
/// multiplied by it take the same share of a 4:3 screen, or of the 4:3 middle of a wider one, as on the default screen
[[nodiscard]] float ReferenceScale(int32_t screenWidth, int32_t screenHeight) noexcept;

/// A picture's shape, as its width to its height
struct Aspect
{
	int32_t width {4};
	int32_t height {3};

	bool operator==(const Aspect&) const = default;
};

/// The shape the game's own screens (logos, tips, pre-intro, the new player's backdrop) are shown at
inline constexpr Aspect k_FourByThree {.width = 4, .height = 3};

/// A rectangle on the screen in pixels, from its top left corner
struct ScreenRect
{
	int32_t x {0};
	int32_t y {0};
	int32_t width {0};
	int32_t height {0};

	bool operator==(const ScreenRect&) const = default;
};

/// The biggest rectangle of `aspect` inside a screen, centred. The pixels it leaves over are shared between two bars,
/// each half of them rounded towards zero, as the game shares what a 16:9 film leaves of a 4:3 screen. A screen of
/// the picture's own shape is filled exactly
[[nodiscard]] ScreenRect FitInside(Aspect aspect, int32_t screenWidth, int32_t screenHeight) noexcept;

} // namespace openblack::gui
