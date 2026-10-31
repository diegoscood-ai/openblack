/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptTexts.h"

#include <cmath>

#include <algorithm>
#include <limits>

using namespace openblack::gui;

namespace
{
/// A float cut to a whole number as the game's conversion does, out of range giving the lowest int
int32_t Truncate(float value)
{
	if (!std::isfinite(value) || value >= 2147483648.0f || value < -2147483648.0f)
	{
		return std::numeric_limits<int32_t>::min();
	}
	return static_cast<int32_t>(value);
}
} // namespace

script_text::ScreenText script_text::Write(std::u16string_view words, glm::vec2 corner, glm::vec2 extent, float size,
                                           float fadeSeconds, glm::u8vec3 colour)
{
	ScreenText text {
	    .words = std::u16string(words.substr(0, std::min(words.size(), k_MaxCharacters))),
	    .corner = corner,
	    .extent = extent,
	    .size = size,
	    .colour = colour,
	    .alpha = 0.0f,
	    .target = 1.0f,
	    .rate = 0.0f,
	};
	// The words end at their first NUL, as the game copies them
	if (const auto end = text.words.find(u'\0'); end != std::u16string::npos)
	{
		text.words.resize(end);
	}
	if (fadeSeconds == 0.0f)
	{
		text.alpha = 1.0f;
	}
	else
	{
		text.rate = 1.0f / fadeSeconds;
	}
	return text;
}

void script_text::FadeOut(ScreenText& text, float seconds)
{
	text.target = 0.0f;
	if (seconds == 0.0f)
	{
		text.alpha = 0.0f;
		text.rate = 0.0f;
		return;
	}
	text.rate = 1.0f / seconds;
}

bool script_text::Step(ScreenText& text, int32_t milliseconds)
{
	const auto seconds = static_cast<float>(milliseconds) * 0.001f;
	// Up towards the target, stopping at it; then down towards it, stopping at it
	if (text.alpha < text.target)
	{
		text.alpha += seconds * text.rate;
		if (text.alpha > text.target)
		{
			text.alpha = text.target;
		}
	}
	if (text.alpha > text.target)
	{
		text.alpha -= seconds * text.rate;
		if (text.alpha < text.target)
		{
			text.alpha = text.target;
		}
	}
	return text.alpha > k_GoneAlpha;
}

uint8_t script_text::AlphaByte(float alpha)
{
	return static_cast<uint8_t>(std::clamp(Truncate(alpha * 255.0f), 0, 255));
}

glm::u8vec3 script_text::ColourFromScript(float red, float green, float blue)
{
	const auto byte = [](float value) { return static_cast<uint8_t>(static_cast<uint32_t>(Truncate(value)) & 0xFFu); };
	return {byte(red), byte(green), byte(blue)};
}

script_text::ScreenBox script_text::Place(const ScreenText& text, glm::vec2 screen)
{
	const auto min = text.corner * screen;
	return {.min = min, .max = min + (text.extent * screen), .size = text.size * screen.y};
}

std::vector<script_text::DrawnLine> script_text::Lay(const ScreenBox& box, std::span<const std::u16string_view> lines,
                                                     const WidthFn& width)
{
	std::vector<DrawnLine> drawn;
	const auto boxWidth = box.max.x - box.min.x;
	if (boxWidth < box.size * 0.5f || box.max.y - box.min.y < box.size)
	{
		return drawn;
	}
	auto top = box.min.y;
	for (const auto& line : lines)
	{
		drawn.push_back({
		    .words = line,
		    .at = {((boxWidth - width(line)) * 0.5f) + box.min.x, top},
		    .topShare = box.max.y < top ? 0.0f : 1.0f,
		    .bottomShare = box.max.y < top + box.size ? 0.0f : 1.0f,
		});
		top += box.size;
		if (box.max.y < top)
		{
			break;
		}
	}
	return drawn;
}

void script_text::OpenClip(ClipWindow& clip, glm::vec2 corner, glm::vec2 extent, float seconds)
{
	clip.target = 1.0f;
	if (seconds == 0.0f)
	{
		clip.alpha = 1.0f;
		clip.rate = 0.0f;
	}
	else
	{
		clip.rate = 1.0f / seconds;
	}
	clip.corner = corner;
	clip.extent = extent;
}

void script_text::CloseClip(ClipWindow& clip, float seconds)
{
	if (seconds == 0.0f)
	{
		clip.target = 0.0f;
		clip.alpha = 0.0f;
		clip.rate = 0.0f;
		return;
	}
	clip.target = 0.0f;
	clip.rate = 1.0f / seconds;
}

void script_text::StepClip(ClipWindow& clip, int32_t milliseconds)
{
	const auto seconds = static_cast<float>(milliseconds) * 0.001f;
	// up towards the target, or down towards it, stopping at it
	if (clip.target <= clip.alpha)
	{
		if (clip.target < clip.alpha)
		{
			clip.alpha -= clip.rate * seconds;
			if (clip.alpha < clip.target)
			{
				clip.alpha = clip.target;
			}
		}
	}
	else
	{
		clip.alpha += clip.rate * seconds;
		if (clip.target < clip.alpha)
		{
			clip.alpha = clip.target;
		}
	}
}

std::optional<uint8_t> script_text::ClipAlphaByte(const ClipWindow& clip)
{
	if (!(clip.alpha > k_GoneAlpha))
	{
		return std::nullopt;
	}
	// the black's own alpha 255 times the drawing alpha, shifted down a byte
	return static_cast<uint8_t>((255u * AlphaByte(clip.alpha)) >> 8u);
}

std::array<script_text::PixelBox, 4> script_text::ClipBoxes(const ClipWindow& clip, glm::ivec2 screen)
{
	// the window in pixels of the screen, each a float product, its far edges float sums, then cut to whole pixels
	const auto width = static_cast<float>(screen.x);
	const auto height = static_cast<float>(screen.y);
	const float x = width * clip.corner.x;
	const float y = height * clip.corner.y;
	const float w = width * clip.extent.x;
	const float h = height * clip.extent.y;
	const int32_t left = Truncate(x);
	const int32_t top = Truncate(y);
	const int32_t right = Truncate(w + x);
	const int32_t bottom = Truncate(h + y);
	// The game's boxes run from corner to corner drawn half a pixel in, which covers the pixels after the first corner
	// up to and with the second: one more than each corner here. Its first corners off the screen are -1
	const auto box = [](int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
		return PixelBox {.min = {x0 + 1, y0 + 1}, .max = {x1 + 1, y1 + 1}};
	};
	return {
	    box(-1, -1, screen.x, top),
	    box(-1, bottom, screen.x, screen.y),
	    box(-1, top, left, bottom),
	    box(right, top, screen.x, bottom),
	};
}
