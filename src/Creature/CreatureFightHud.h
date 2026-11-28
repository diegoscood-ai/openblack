/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <functional>
#include <string>
#include <string_view>

#include <glm/gtc/type_precision.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "Enums.h"

/// The panel shown during a fight, flush in the screen's top left corner: a shade that fades out from that corner, and
/// a row for each fighter with its fight health bar over its stamina bar and its name to their right. Everything is
/// measured in a unit of a seventieth of the screen's height, whole pixels, so the panel is the same share of every
/// screen's height and keeps to the left edge on wide screens. There are no numbers, and it never fades. As the
/// creature's status panel, it is laid out in the dialogs' space and each corner goes to the screen on its own.
namespace openblack::creature_fight_hud
{
struct Values
{
	struct Side
	{
		std::u16string name;
		float health {1.0f};
		float stamina {1.0f};
	};
	std::array<Side, 2> sides;
};

struct Rect
{
	glm::vec2 min {0.0f};
	glm::vec2 max {0.0f};
};

struct Layout
{
	/// The screen the panel is laid out for; everything else is in the dialogs' space of that screen
	glm::u16vec2 resolution {800, 600};
	/// The unit everything is measured in: a seventieth of the screen's height in whole pixels, taken into the
	/// dialogs' space in whole units
	int unit {0};
	/// The shade, darkest at its top left corner
	Rect box;
	float textSize {0.0f};
	/// How wide a name may be before it is cut short
	float nameWidth {0.0f};
	struct Row
	{
		/// The name's top left
		glm::vec2 name;
		/// The health bar, drawn over the lower stamina bar, which it overlaps
		Rect health;
		Rect stamina;
	};
	std::array<Row, 2> rows;

	/// A point of the dialogs' space on the screen, in whole pixels
	[[nodiscard]] glm::vec2 ToScreen(glm::vec2 dialogs) const;
	/// The names' height and widest on the screen: as in the dialogs' space, shrunk with it on a smaller screen
	[[nodiscard]] float TextSizeOnScreen() const;
	[[nodiscard]] float NameWidthOnScreen() const;
};

/// Where everything goes on a screen of a resolution: the shade's corners at the screen's corner and at units of the
/// screen in whole pixels, taken into the dialogs' space, and the rows laid out from there in that space
[[nodiscard]] Layout Compute(glm::u16vec2 resolution);
/// The health bar's colour, as 0-255 channels: red when empty, yellow at half, green when full, in whole steps
[[nodiscard]] glm::u8vec3 HealthColour(float health);
/// The stamina bar's colour, as 0-255 channels: blue, a little paler the more stamina there is
[[nodiscard]] glm::u8vec3 StaminaColour(float stamina);
/// The longest start of a name that fits a width, letters taken off its end one at a time; a start fits when its
/// width, cut to a whole pixel, is no more than the width
[[nodiscard]] std::u16string_view FitName(std::u16string_view name, float width,
                                          const std::function<float(std::u16string_view)>& widthOf);
} // namespace openblack::creature_fight_hud
