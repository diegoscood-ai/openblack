/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Bubble.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <bit>
#include <string>
#include <utility>

using namespace openblack::help;

BubbleSizes openblack::help::DefaultBubbleSizes(float width, float height, float textSize)
{
	BubbleSizes sizes {
	    .width = width,
	    .height = height,
	    .margin = width * 0.5f * 0.2f,
	    .textSize = textSize,
	    .farWidth = width * 0.5f * (1.0f / 7.0f),
	    .farHeight = height * 0.5f * (1.0f / 7.0f),
	    .farMargin = width * 0.5f * (1.0f / 35.0f),
	    .farTextSize = textSize * (1.0f / 7.0f),
	};
	return sizes;
}

float openblack::help::GetFrac(float v, float a, float b)
{
	// (pending) the a == b case is not read: b == a gives 1 here
	if (v <= a)
	{
		return 0.0f;
	}
	if (v >= b)
	{
		return 1.0f;
	}
	return (v - a) / (b - a);
}

std::optional<BubbleLayout> openblack::help::LayoutBubble(const BubbleSizes& sizes, float depth, glm::vec2 screen, float offset,
                                                          float place)
{
	if (!(depth >= 1.0f))
	{
		return std::nullopt;
	}
	const float f = GetFrac(depth, sizes.nearDepth, sizes.farDepth);
	const auto lerp = [f](float a, float b) { return a + (b - a) * f; };
	BubbleLayout layout {
	    .width = lerp(sizes.width, sizes.farWidth),
	    .height = lerp(sizes.height, sizes.farHeight),
	    .margin = lerp(sizes.margin, sizes.farMargin),
	    .textSize = lerp(sizes.textSize, sizes.farTextSize),
	};
	// the distance fade, in below the fade-in end and out above the fade-out start, both bounded by the middle of the
	// range
	const float middle = (sizes.nearDepth + sizes.farDepth) * 0.5f;
	const float inEnd = std::min(sizes.fadeInEnd, middle);
	const float outStart = std::max(sizes.fadeOutStart, middle);
	layout.fade = 1.0f;
	if (depth < inEnd)
	{
		layout.fade *= GetFrac(depth, sizes.nearDepth, inEnd);
	}
	if (depth > outStart)
	{
		layout.fade *= 1.0f - GetFrac(depth, outStart, sizes.farDepth);
	}
	layout.depth = depth;
	layout.tailPlace = place;
	layout.topLeft = {screen.x + offset - place * layout.width, screen.y - layout.margin - layout.height};
	layout.tailRight = offset < -0.1f;
	return layout;
}

std::optional<glm::vec2> openblack::help::AnchorOnScreen(glm::vec3 projected, float depth, float nearClip, float height)
{
	// a depth that is not a number fails too, as the original's compare does
	if (!(depth >= nearClip))
	{
		return std::nullopt;
	}
	return glm::vec2(projected.x, height - projected.y);
}

BubbleTail openblack::help::TailFor(float screenX, int screenWidth)
{
	float u = screenX / static_cast<float>(screenWidth) * 2.0f - 1.0f;
	float offset = 10.0f;
	if (u < -1.0f)
	{
		u = -1.0f;
	}
	else if (u > 1.0f)
	{
		u = 1.0f;
		offset = -10.0f;
	}
	else if (u > 0.0f)
	{
		offset = -10.0f;
	}
	// the doubles 0.6 and 0.2
	const auto place = static_cast<float>(std::fabs(static_cast<double>(u)) * 0.6000000238418579 + 0.20000000298023224);
	return {offset, place};
}

float openblack::help::DrawDepth(float depth, float pitch, float nearPlane)
{
	// p = clamp(|pitch|, 0, pi/2) x 2/pi; n = 2 x near; e = max(d - 5, n) - n; max(n + e (1 - p^2), n)
	constexpr double k_HalfPi = 1.5707963705062866;                                                         // pi / 2
	const double p = std::clamp(std::fabs(static_cast<double>(pitch)), 0.0, k_HalfPi) * 0.6366197546520227; // 2 / pi
	const float n = nearPlane + nearPlane;
	const float e = std::max(depth - 5.0f, n) - n;
	return std::max(n + static_cast<float>(static_cast<double>(e) - std::fabs(static_cast<double>(e)) * p * p), n);
}

BubbleDepths openblack::help::DepthsOf(float drawDepth)
{
	return {.box = drawDepth, .shadow = drawDepth - 1.0f, .text = drawDepth - 2.0f};
}

std::vector<BubbleVertex> openblack::help::BubbleShape(float left, float top, float right, float bottom, float corner,
                                                       float tailX, bool tailRight, uint32_t argb, bool hasTail, uint8_t a0,
                                                       uint8_t a1)
{
	std::vector<BubbleVertex> out;
	const float w = right - left;
	const float h = bottom - top;
	if (w <= 0.0f || h <= 0.0f)
	{
		return out;
	}
	const float c = std::min({corner, w * 0.5f, h * 0.5f});
	const auto alpha = static_cast<float>(argb >> 24);
	const uint32_t rgb = argb & 0x00FFFFFFu;
	const auto colourAt = [&](float y) {
		// trunc((y - top) x slope + a0') with slope = alpha (a1 - a0) / (255 h), a0' = alpha a0 / 255
		const float slope = alpha * static_cast<float>(a1 - a0) / h * (1.0f / 255.0f);
		const float a = (y - top) * slope + alpha * static_cast<float>(a0) * (1.0f / 255.0f);
		const auto byte = static_cast<uint32_t>(std::clamp(static_cast<int>(a), 0, 255));
		return (byte << 24) | rgb;
	};
	// the grid: u {0, 0.5, 0.5, 1} x 0.25, v {0 or 0.3, 0.5, 0.5, 1} x 0.25
	const std::array<float, 4> xs = {left, left + c, right - c, right};
	const std::array<float, 4> ys = {top, top + c, bottom - c, bottom};
	const std::array<float, 4> us = {0.0f, 0.125f, 0.125f, 0.25f};
	const std::array<float, 4> vs = {hasTail ? 0.3f * 0.25f : 0.0f, 0.125f, 0.125f, 0.25f};
	const auto quad = [&out](const BubbleVertex& a, const BubbleVertex& b, const BubbleVertex& cc, const BubbleVertex& d) {
		out.insert(out.end(), {a, b, cc, a, cc, d});
	};
	// each corner is cut to a whole pixel before it goes into the world; its colour is taken at the uncut height
	const auto pixel = [](float v) { return std::trunc(v); };
	for (size_t row = 0; row < 3; ++row)
	{
		for (size_t col = 0; col < 3; ++col)
		{
			quad({pixel(xs[col]), pixel(ys[row]), us[col], vs[row], colourAt(ys[row])},
			     {pixel(xs[col + 1]), pixel(ys[row]), us[col + 1], vs[row], colourAt(ys[row])},
			     {pixel(xs[col + 1]), pixel(ys[row + 1]), us[col + 1], vs[row + 1], colourAt(ys[row + 1])},
			     {pixel(xs[col]), pixel(ys[row + 1]), us[col], vs[row + 1], colourAt(ys[row + 1])});
		}
	}
	if (hasTail)
	{
		// the tail, as wide as the corner, kept 1.8 corners from the ends, 0.85 / 0.15 of it left of the point, from 0.091
		// corners above the bottom down; cell (1, 0), mirrored for the right side; the bottom's colour
		float x = std::max(tailX, left + 1.8f * c);
		x = std::min(x, right - 1.8f * c);
		const float x0 = x - (tailRight ? 0.85f * c : 0.15f * c);
		const float x1 = x0 + c;
		const float y0 = bottom - 0.091f * c;
		const float y1 = y0 + c;
		const float uLeft = tailRight ? 0.5f : 0.25f;
		const float uRight = tailRight ? 0.25f : 0.5f;
		const uint32_t colour = colourAt(bottom);
		quad({pixel(x0), pixel(y0), uLeft, 0.0f, colour}, {pixel(x1), pixel(y0), uRight, 0.0f, colour},
		     {pixel(x1), pixel(y1), uRight, 0.25f, colour}, {pixel(x0), pixel(y1), uLeft, 0.25f, colour});
	}
	return out;
}

std::vector<std::u16string> openblack::help::WrapGatheringText(std::u16string_view text, float maxWidth,
                                                               const std::function<float(std::u16string_view)>& widthOf)
{
	constexpr char16_t k_Space = 0xF8FE; // the script text's joined space
	const auto isBreakLine = [](char16_t c) { return c == u'\r' || c == u'\n'; };
	const auto isSpace = [](char16_t c) { return c == u' ' || c == u'\t' || c == u'\r' || c == u'\n' || c == 0x3000; };
	std::vector<std::u16string> lines;
	size_t start = 0;
	while (start < text.size())
	{
		// grow by up to 4 characters while it fits
		size_t count = 0;
		float width = 0.0f;
		while (start + count < text.size())
		{
			size_t step = 0;
			while (step < 4 && start + count + step < text.size())
			{
				const char16_t c = text[start + count + step];
				++step;
				if (isBreakLine(c))
				{
					break;
				}
			}
			width = widthOf(text.substr(start, count + step));
			if (width > maxWidth)
			{
				break;
			}
			count += step;
		}
		size_t next = start + count; // where the next line starts
		if (start + count < text.size() && width > maxWidth)
		{
			// back off from the last character measured to a break, whitespace / U+F8FE (dropped) or after a '-' (kept), until
			// the line fits; with no break, one character at a time
			size_t end = start + count + 4; // the overflowing measure took up to 4 more
			end = std::min(end, text.size());
			bool found = false;
			while (end > start + 1)
			{
				size_t i = end - 1;
				while (i > start && !isSpace(text[i]) && text[i] != k_Space && text[i] != u'-')
				{
					--i;
				}
				if (i == start)
				{
					break;
				}
				const size_t length = text[i] == u'-' ? i - start + 1 : i - start;
				if (widthOf(text.substr(start, length)) <= maxWidth)
				{
					count = length;
					next = i + 1;
					found = true;
					break;
				}
				end = i;
			}
			if (!found)
			{
				size_t length = std::max<size_t>(count, 1);
				while (length > 1 && widthOf(text.substr(start, length)) > maxWidth)
				{
					--length;
				}
				count = length;
				next = start + count;
			}
		}
		// a CR / LF inside the line ends it there
		for (size_t i = start; i < start + count; ++i)
		{
			if (isBreakLine(text[i]))
			{
				count = i - start;
				next = i + 1;
				break;
			}
		}
		lines.emplace_back(text.substr(start, count));
		if (next <= start)
		{
			next = start + 1; // (openblack guard) never loop on a character wider than the line
		}
		start = next;
	}
	return lines;
}

const std::vector<std::u16string>& GatheringWrapCache::Wrap(std::u16string_view text, float maxWidth, float size,
                                                            const std::function<float(std::u16string_view)>& widthOf)
{
	// the widths by their bits (a NaN is never kept)
	const auto same = [](float a, float b) {
		return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b) && !std::isnan(a);
	};
	if (!_kept || text != _text || !same(maxWidth, _maxWidth) || !same(size, _size))
	{
		_lines = WrapGatheringText(text, maxWidth, widthOf);
		_text = text;
		_maxWidth = maxWidth;
		_size = size;
		_kept = true;
	}
	return _lines;
}

Bubble::Bubble()
    : _sizes(DefaultBubbleSizes(400.0f, 250.0f, 19.0f))
{
}

void Bubble::Restart()
{
	_scroll = 1000.0f; // the last line
	_textHeight = 0.0f;
}

void Bubble::Age(float frameMs)
{
	_life -= frameMs * 0.0002f;
	if (_life < 0.0f)
	{
		_life = 0.0f;
	}
}

Bubble::ScrollMarks Bubble::ClampScroll(float boxHeight, float margin, float lineHeight)
{
	const float textHeight = _textHeight;
	float last = 0.0f;
	if (textHeight != 0.0f)
	{
		last = textHeight - (boxHeight - (margin + margin));
		if (lineHeight != 0.0f)
		{
			last /= lineHeight;
		}
		if (last < _scroll)
		{
			_scroll = last;
		}
	}
	if (_scroll < 0.0f)
	{
		_scroll = 0.0f;
	}
	return {.below = last - 1.0f > _scroll, .above = _scroll > 1.0f};
}

std::array<std::u16string, Bubble::k_Parts> openblack::help::BubbleParts(std::u16string_view tip, std::u16string_view title)
{
	// the middle part is a single space, so it takes one line
	return {std::u16string(tip), u" ", std::u16string(title)};
}

namespace
{
/// The bounds one call of the gathering text's draw takes: the area's left and right, the clip's top, the end of the
/// fade in, the start of the fade out and the clip's bottom
struct LineBounds
{
	float left;
	float right;
	float clipTop;
	float fadeInEnd;
	float fadeOutStart;
	float clipBottom;
};

/// The alpha of a line's edge at a height: faded in over [clip top, fade in end] (GetFrac) and out over [fade out
/// start, clip bottom] (1 - GetFrac), the colour's alpha byte times that, truncated
uint8_t EdgeAlpha(const LineBounds& bounds, float edge, uint8_t full)
{
	float f = 1.0f;
	if (edge < bounds.fadeInEnd)
	{
		f = GetFrac(edge, bounds.clipTop, bounds.fadeInEnd);
	}
	if (edge > bounds.fadeOutStart)
	{
		f = 1.0f - GetFrac(edge, bounds.fadeOutStart, bounds.clipBottom);
	}
	return static_cast<uint8_t>(static_cast<int>(f * static_cast<float>(full)));
}
} // namespace

BubbleWords openblack::help::LayOutBubbleWords(const BubbleLayout& layout, float scrollPixels, float alpha, uint32_t dark,
                                               std::span<const std::u16string> parts, const BubbleWrapFn& wrap,
                                               const BubbleWidthFn& widthOf, const BubbleKeyFn& keyOf)
{
	BubbleWords words;
	const auto a = static_cast<uint32_t>(std::clamp(static_cast<int>(alpha * 255.0f), 0, 255));
	const float size = layout.textSize;
	// the text area: m = margin x 0.333333; t = (bottom - top) x 0.333333 + top; the bottom b = bottom - m, or t when
	// that is less; the clip b + m / 4; the start y = b + the scroll in pixels
	const float m = layout.margin * 0.333333f;
	const float left = layout.topLeft.x + m;
	const float right = layout.topLeft.x + layout.width - m;
	const float top = layout.topLeft.y;
	const float boxBottom = layout.topLeft.y + layout.height;
	const float third = (boxBottom - top) * 0.333333f + top;
	const float bottom = boxBottom - m < third ? third : boxBottom - m;
	const float clip = bottom + m * 0.25f;
	float y = bottom + scrollPixels;
	for (size_t partIndex = 0; partIndex < parts.size(); ++partIndex)
	{
		const auto& whole = parts[partIndex];
		// a part that starts with $g / $m keeps only that token (4 characters); else the text up to the first $g (or
		// else $m), then that token. As the original, what follows it is not drawn
		std::u16string part = whole;
		std::u16string token;
		auto at = whole.find(u"$g");
		if (at == std::u16string::npos)
		{
			at = whole.find(u"$m");
		}
		if (at != std::u16string::npos)
		{
			token = whole.substr(at, 4);
			part = whole.substr(0, at);
		}
		// the gathering text's line breaking (help::WrapGatheringText), each line `size` high
		const auto& wrapped = wrap(partIndex, part, right - left, size);
		const float height = size * static_cast<float>(wrapped.size());
		y -= height; // bottom up
		words.textHeight += height;
		// the text's call: the area, faded in over [top, top + h / 3] and out over [b, b + m / 4]; the shadow's call has
		// every bound and its start two pixels down and right
		const LineBounds textBounds {left, right, top, third, bottom, clip};
		const LineBounds shadowBounds {left + 2.0f, right + 2.0f, top + 2.0f, third + 2.0f, bottom + 2.0f, clip + 2.0f};
		const auto shadowA = static_cast<uint8_t>(std::clamp(static_cast<int>(alpha * 128.0f), 0, 255));
		const auto textA = static_cast<uint8_t>(a);
		// a call draws nothing where a line cannot fit: half a line wider than its area, or a clip less than a line
		// tall. Its lines go down from its start until one starts below its clip; a line whose bottom (y + size) is at
		// or above the clip's top is not drawn. Else a line is centred across the call's area, with the alphas of its
		// top and bottom edges
		const auto addLine = [&words, &widthOf, size](const LineBounds& bounds, const std::u16string& text, float ly,
		                                              uint8_t grey, uint8_t full, bool shadow) {
			if (size * 0.5f > bounds.right - bounds.left || bounds.clipBottom - bounds.clipTop < size ||
			    ly > bounds.clipBottom || ly + size <= bounds.clipTop)
			{
				return;
			}
			const float lx = (bounds.right - bounds.left - widthOf(text, size)) * 0.5f + bounds.left;
			words.lines.push_back({{TextFont::J0, text, lx, ly, size, grey, grey, grey, EdgeAlpha(bounds, ly, full),
			                        bounds.clipTop, bounds.clipBottom},
			                       EdgeAlpha(bounds, ly + size, full),
			                       shadow});
		};
		for (size_t i = 0; i < wrapped.size(); ++i)
		{
			const float step = size * static_cast<float>(i);
			addLine(shadowBounds, wrapped[i], y + 2.0f + step, 0, shadowA, true);
			addLine(textBounds, wrapped[i], y + step, 255, textA, false);
		}
		// $g<n> (0 < n < 24) a square of 2 x size on its own step, its cell n % 16 of S_Gesture<n / 16>, a shadow at +2
		// in the dark colour then the glyph. (approximate) the square's place: the line's left, one step of 2 x size
		// above; the original's step after it is a product x 0.7 (taken off y and added to the text height), its
		// factors not traced
		if (token.size() >= 3 && token[1] == u'm')
		{
			// the action's binding, then DrawKeyOrMouse
			const int action = std::atoi(std::string(token.begin() + 2, token.end()).c_str());
			if (auto binding = keyOf(action))
			{
				const float side = 2.0f * size;
				y -= side;
				words.textHeight += side;
				words.keyIcons.push_back({std::move(*binding), static_cast<int32_t>((left + right) * 0.5f),
				                          static_cast<int32_t>(y + side * 0.5f), static_cast<int32_t>(side),
				                          static_cast<uint8_t>(a)});
			}
		}
		if (token.size() >= 3 && token[1] == u'g')
		{
			const int n = std::atoi(std::string(token.begin() + 2, token.end()).c_str());
			if (n > 0 && n < 24)
			{
				const float side = 2.0f * size;
				y -= side;
				words.textHeight += side;
				const int cell = n % 16;
				const float u0 = static_cast<float>(cell % 4) * 0.25f;
				const float v0 = static_cast<float>(cell / 4) * 0.25f;
				const auto page = static_cast<size_t>(n / 16);
				const uint32_t darkAbgr = (a << 24) | ((dark & 0xFFu) << 16) | (dark & 0xFF00u) | ((dark >> 16) & 0xFFu);
				// the quad cut to [top, clip], its v moved by the part cut (a quarter of the cell per side length)
				const auto clipped = [&words, page, top, clip, side](float x, float qy, float uu, float vv, uint32_t abgr,
				                                                     bool shadow) {
					float y0 = qy;
					float y1 = qy + side;
					float vTop = vv;
					float vBottom = vv + 0.25f;
					if (y1 <= top || y0 >= clip)
					{
						return;
					}
					if (y0 < top)
					{
						vTop += (top - y0) / side * 0.25f;
						y0 = top;
					}
					if (y1 > clip)
					{
						vBottom -= (y1 - clip) / side * 0.25f;
						y1 = clip;
					}
					words.gestures.push_back({page, x, y0, side, y1 - y0, uu, vTop, vBottom, abgr, shadow});
				};
				clipped(left + 2.0f, y + 2.0f, u0, v0, darkAbgr, true);
				clipped(left, y, u0, v0, (a << 24) | 0x00FFFFFFu, false);
			}
		}
		// no more parts once one has reached above the box's top, its height counted
		if (y < top)
		{
			break;
		}
	}
	return words;
}

void Bubble::Drag(bool dragging, int mouseY, float lineHeight)
{
	if (!dragging)
	{
		_dragging = false;
		return;
	}
	if (lineHeight != 0.0f)
	{
		if (!_dragging)
		{
			_dragY = mouseY;
		}
		const int dy = mouseY - _dragY;
		_dragY = mouseY;
		_scroll += static_cast<float>(dy) / lineHeight;
	}
	_dragging = true;
}
