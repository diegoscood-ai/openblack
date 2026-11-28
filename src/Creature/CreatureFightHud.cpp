/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFightHud.h"

#include <cstdint>

#include <algorithm>

#include "Creature/CreatureStatusPanel.h"
#include "Gui/LoadingScreenRules.h"

using namespace openblack;
using namespace openblack::creature_fight_hud;

namespace
{
/// The unit is a seventieth of the screen's height
constexpr int k_UnitsPerScreen = 70;
/// The shade's size, in units
constexpr int k_BoxWidth = 33;
constexpr int k_BoxHeight = 9;
/// The bars' width, the names' left edge from the bars' and their widest, in units
constexpr int k_BarWidth = 25;
constexpr int k_NameOffset = 26;
constexpr int k_NameWidth = 30;
/// The first row a unit down, the second three and a half units below it
constexpr float k_RowPitch = 3.5f;
/// The health bar is two units high; the stamina bar starts one and a half units down and ends three down
constexpr float k_HealthHeight = 2.0f;
constexpr float k_StaminaTop = 1.5f;
constexpr float k_StaminaBottom = 3.0f;
/// The names are three units high
constexpr int k_TextHeight = 3;

// Every value is held between 0 and 1 first
[[nodiscard]] float Clamp01(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}
} // namespace

Layout creature_fight_hud::Compute(glm::u16vec2 resolution)
{
	// The shade from the screen's corner, and the unit, in whole pixels, taken into the dialogs' space
	const int screenUnit = resolution.y / k_UnitsPerScreen;
	const auto [x0, y0] = loading::Unadjust({0, 0}, resolution.x, resolution.y);
	const auto [x1, y1] = loading::Unadjust({k_BoxWidth * screenUnit, k_BoxHeight * screenUnit}, resolution.x, resolution.y);
	const int unit = loading::UnadjustSize(screenUnit, resolution.x, resolution.y);
	const auto u = static_cast<float>(unit);
	// A share of units below a whole point, cut towards zero: the space's points can be negative
	const auto below = [u](int from, float units) {
		return static_cast<float>(static_cast<int>(static_cast<float>(from) + (units * u)));
	};

	Layout layout;
	layout.resolution = resolution;
	layout.unit = unit;
	layout.box = {.min = {static_cast<float>(x0), static_cast<float>(y0)},
	              .max = {static_cast<float>(x1), static_cast<float>(y1)}};
	layout.textSize = static_cast<float>(k_TextHeight * unit);
	layout.nameWidth = static_cast<float>(k_NameWidth * unit);
	// The rows start a unit in from the shade's corner
	const int left = x0 + unit;
	const int firstTop = y0 + unit;
	const std::array<int, 2> tops {firstTop, static_cast<int>(below(firstTop, k_RowPitch))};
	for (size_t i = 0; i < layout.rows.size(); ++i)
	{
		const int top = tops.at(i);
		const auto topY = static_cast<float>(top);
		const auto from = static_cast<float>(left);
		const auto to = static_cast<float>(left + (k_BarWidth * unit));
		auto& row = layout.rows.at(i);
		row.name = {static_cast<float>(left + (k_NameOffset * unit)), topY};
		row.health = {.min = {from, topY}, .max = {to, below(top, k_HealthHeight)}};
		row.stamina = {.min = {from, below(top, k_StaminaTop)}, .max = {to, below(top, k_StaminaBottom)}};
	}
	return layout;
}

glm::vec2 creature_fight_hud::Layout::ToScreen(glm::vec2 dialogs) const
{
	return creature_panel::DialogsToScreen(resolution, dialogs);
}

float creature_fight_hud::Layout::TextSizeOnScreen() const
{
	return textSize / creature_panel::DialogsDivisor(resolution);
}

float creature_fight_hud::Layout::NameWidthOnScreen() const
{
	return nameWidth / creature_panel::DialogsDivisor(resolution);
}

glm::u8vec3 creature_fight_hud::HealthColour(float health)
{
	const float fill = Clamp01(health);
	if (fill < 0.5f)
	{
		// Red, with green climbing to yellow at half
		const int steps = static_cast<int>(fill * 512.0f);
		return {255, static_cast<uint8_t>((steps * 255) >> 8), 0};
	}
	// Yellow, with red falling away to green when full
	const int steps = static_cast<int>((fill - 0.5f) * -512.0f);
	return {static_cast<uint8_t>(255 + ((255 * steps) >> 8)), 255, 0};
}

glm::u8vec3 creature_fight_hud::StaminaColour(float stamina)
{
	const int steps = static_cast<int>(Clamp01(stamina) * 255.0f);
	return {static_cast<uint8_t>(32 + ((3 * steps) >> 3)), 128, 255};
}

std::u16string_view creature_fight_hud::FitName(std::u16string_view name, float width,
                                                const std::function<float(std::u16string_view)>& widthOf)
{
	// The width is cut to a whole pixel before it is compared
	while (!name.empty() && static_cast<float>(static_cast<int>(widthOf(name))) > width)
	{
		name.remove_suffix(1);
	}
	return name;
}
