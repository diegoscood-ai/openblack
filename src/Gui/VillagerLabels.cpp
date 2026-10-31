/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerLabels.h"

#include <cmath>

#include <algorithm>
#include <ranges>

namespace openblack::gui::villager_labels
{

namespace
{
/// A channel half way to white, rounding down
constexpr uint8_t HalfWayToWhite(uint8_t channel)
{
	return static_cast<uint8_t>(channel + ((255u - channel) >> 1u));
}

/// A float cut towards zero to a whole number, as the game turns a corner into a pixel
int32_t Truncated(float value)
{
	return static_cast<int32_t>(value);
}
} // namespace

float StepNameFade(float fade, bool shown, uint32_t milliseconds)
{
	const float target = shown ? 1.0f : 0.0f;
	const float step = static_cast<float>(static_cast<int32_t>(milliseconds)) * k_NameFadePerMillisecond;
	// Growing, it stops at the target; otherwise it shrinks, and stops at the target (at the target it stays there)
	if (fade < target)
	{
		return std::min(fade + step, target);
	}
	return std::max(fade - step, target);
}

NameFadeFrame StepNameFadeFrame(float fade, bool shown, uint32_t milliseconds, bool landDrawn)
{
	return {.drawn = fade, .next = landDrawn ? StepNameFade(fade, shown, milliseconds) : fade};
}

std::optional<float> NameSize(const Who& who)
{
	// Speaking, its speech bubble shows instead of any label
	if (!who.named || who.speaking)
	{
		return std::nullopt;
	}
	if (!(who.nameFade > k_SmallestSize) || who.inScript || who.nameHidden)
	{
		return std::nullopt;
	}
	return std::min(who.nameFade, 1.0f);
}

glm::u8vec3 NameColour(std::optional<glm::u8vec3> player)
{
	constexpr uint8_t k_AllButBlack = 10;
	auto colour = player.value_or(glm::u8vec3(255));
	if (colour.r < k_AllButBlack && colour.g < k_AllButBlack && colour.b < k_AllButBlack)
	{
		colour = glm::u8vec3(255);
	}
	return {HalfWayToWhite(colour.r), HalfWayToWhite(colour.g), HalfWayToWhite(colour.b)};
}

bool Board::Add(const Request& request, uint16_t screenWidth, const WidthFn& width)
{
	if (request.size < k_SmallestSize)
	{
		return false;
	}
	const float size = std::min(request.size, 1.0f);
	Label label {.pixel = glm::vec2(request.pixel),
	             .depth = request.depth,
	             .point = request.point,
	             .size = size,
	             .colour = request.colour};

	// Its lines: at the line breaks, the last of them taking the rest
	const auto text = std::u16string_view(request.text).substr(0, k_MaxCharacters);
	size_t start = 0;
	while (label.lines.size() + 1 < k_MaxLines)
	{
		const auto lineBreak = text.find(u'\n', start);
		if (lineBreak == std::u16string_view::npos)
		{
			break;
		}
		label.lines.emplace_back(text.substr(start, lineBreak - start));
		start = lineBreak + 1;
	}
	label.lines.emplace_back(text.substr(start));

	label.lineHeight = size * k_LineHeight;
	const float extraWidth = size * k_ExtraWidth;
	float boxWidth = 0.0f;
	for (const auto& line : label.lines)
	{
		boxWidth = std::max(boxWidth, width(line, label.lineHeight) + extraWidth);
	}
	// Wider than the screen it is cut to half the screen, in whole pixels
	const auto screen = static_cast<float>(screenWidth);
	if (screen < boxWidth)
	{
		boxWidth = static_cast<float>(screenWidth >> 1u);
	}
	label.left = label.pixel.x - (boxWidth * 0.5f);
	if (label.left < 0.0f)
	{
		label.left = 0.0f;
	}
	label.right = boxWidth + label.left;
	if (screen < label.right)
	{
		label.right = screen;
		label.left = screen - boxWidth;
	}
	label.bottom = label.pixel.y - k_RaisedBy;
	label.inset = size + size;
	label.top = label.bottom - ((size * k_ExtraHeight) + (static_cast<float>(label.lines.size()) * label.lineHeight));

	// Up over every label it covers, a pixel above it, looking again from the leftmost each time it moves
	for (size_t i = 0; i < _labels.size() && _labels[i].left < label.right;)
	{
		const auto& other = _labels[i];
		const bool covers = other.right > label.left && other.top <= label.bottom && other.bottom >= label.top;
		if (!covers)
		{
			++i;
			continue;
		}
		const float rise = label.top - label.bottom;
		label.bottom = other.top - 1.0f;
		label.top = label.bottom + rise;
		if (other.top < 0.0f)
		{
			break;
		}
		i = 0;
	}
	if (label.top < 0.0f)
	{
		return false;
	}
	label.boxAlpha = static_cast<uint8_t>(Truncated(size * k_BoxAlpha));
	label.wordsAlpha = static_cast<uint8_t>(Truncated(size * k_WordsAlpha));
	// Kept from left to right; a label goes before the first that starts as far left as it does
	const auto at = std::ranges::find_if(_labels, [&label](const Label& other) { return other.left >= label.left; });
	_labels.insert(at, std::move(label));
	return true;
}

std::array<PointerBox, 2> PointerShape(const Label& label)
{
	const float x = label.pixel.x;
	const float y = label.pixel.y;
	const int32_t headTop = Truncated(y - k_HeadHeightPixels);
	// The stalk stretches one row of the bubble texture's pointer piece; the head is the piece's tip
	return {PointerBox {.x0 = Truncated(x - k_StalkHalfWidth),
	                    .y0 = Truncated(label.bottom),
	                    .x1 = Truncated(x + k_StalkHalfWidth),
	                    .y1 = headTop,
	                    .uv0 = {0.53125f, 0.05f},
	                    .uv1 = {0.59375f, 0.05f}},
	        PointerBox {.x0 = Truncated(x - k_HeadHalfWidth),
	                    .y0 = headTop,
	                    .x1 = Truncated(x + k_HeadHalfWidth),
	                    .y1 = Truncated(y),
	                    .uv0 = {0.5f, 0.05f},
	                    .uv1 = {0.625f, 0.125f}}};
}

} // namespace openblack::gui::villager_labels
