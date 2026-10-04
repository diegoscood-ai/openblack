/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpTextDisplay.h"

#include <cstdio>

#include <algorithm>
#include <utility>

namespace openblack::help
{

namespace
{
using text_splitter::DrawState;
using text_splitter::k_PieceNewLine;
using text_splitter::k_PieceNumber;
using text_splitter::k_PiecePercent;
using text_splitter::k_PieceSpaced;
using text_splitter::k_PieceStop;
using text_splitter::k_PieceWord;
using text_splitter::Splitter;

// Constants of runblack.exe W120 (float unless noted)
constexpr float k_BoxHeightFraction = 0.12666666507720947f; // [0x92A43C] = 19/150
constexpr float k_BoxMarginFraction = 0.02500000037252903f; // [0x92A440]
constexpr float k_LineHeightFraction = 0.03333333507180214f; // [0x8CF3F8] = 1/30
constexpr float k_BiggerLineHeightFraction = 0.0357142873108387f; // [0x92A53C] = 1/28
constexpr float k_AnimSpeed = 0.003000000026077032f;          // [0x8DF89C] per ms
constexpr float k_AnimNearlyDone = 0.9998999834060669f;       // [0x8CF368]
constexpr float k_SlotThird = 0.3333333432674408f;            // [0x8AB26C] (and -1/3 [0x92A550])
constexpr float k_EnterScale = 1.0752688646316528f;           // 0x3F89A269 (= 1/0.93), 0x5CC88E
constexpr float k_SecondScale = 0.8600000143051147f;          // [0x92A548]
constexpr float k_OlderScale = 0.9300000071525574f;           // [0x92A54C]

/// __ftol 0x7A1400: truncation toward zero of the value on the FPU stack, which (the FPU at 24 bits, fn_007DEE00) the
/// last operation already rounded to a float
int Ftol(float value)
{
	return static_cast<int>(value);
}

/// UNICODE_sprintf 0x5CB88E / 0x5CBB80 of the entry's number with "%3.3f%%" (0xBF1A74) or "%3.3f" (0xBF1A68)
std::u16string FormatNumber(float number, bool percent)
{
	std::array<char, 64> buffer {};
	std::snprintf(buffer.data(), buffer.size(), percent ? "%3.3f%%" : "%3.3f", static_cast<double>(number));
	std::u16string out;
	for (const char* c = buffer.data(); *c != '\0'; ++c)
	{
		out.push_back(static_cast<char16_t>(static_cast<unsigned char>(*c)));
	}
	return out;
}

struct Measure
{
	float width {0.0f};
	float height {0.0f};
};

/// fn_005CB750(region, text, &width, &height): the widest line and the height of the wrapped text, with the font,
/// colour and line height of the state (the codes it walks change the state, as in the original: the draw that follows
/// starts with the font and colour the measure ended with). The font +0x8 is never null here (0x5CB779 returns
/// without writing the outputs when it is).
Measure MeasureText(const TextRegion& region, std::u16string_view text, float lineHeight, float number,
                    DrawState& state, const WidthFn& widthFn)
{
	const int maxLines = Ftol(static_cast<float>(region.bottom - region.top + 1) / lineHeight); // 0x5CB782..0x5CB797
	int pen = 0;
	int widest = 0;
	int line = 0;
	int type = k_PieceWord;
	Splitter splitter(text);
	std::u16string word;
	if (maxLines > 0) // 0x5CB7A8
	{
		do
		{
			if (type == k_PieceStop || splitter.AtEnd()) // 0x5CB7B4..0x5CB7C5
			{
				break;
			}
			if (type == k_PieceNewLine) // 0x5CB7CB..0x5CB7EB
			{
				widest = std::max(widest, pen);
				pen = 0;
				if (++line >= maxLines)
				{
					break;
				}
			}
			else if (type == k_PieceSpaced) // 0x5CB7F3..0x5CB82A: GetStringWidth(" ", 1, lineH) (0x9D00CC)
			{
				pen = Ftol(widthFn(state.font, u" ", lineHeight) + static_cast<float>(pen)); // fiadd 0x5CB81F
			}
			type = splitter.Next(word, state, false); // 0x5CB841, flag 0
			if (type == k_PiecePercent || type == k_PieceNumber) // 0x5CB848..0x5CB894 (the type stays 4/5: no space)
			{
				word = FormatNumber(number, type == k_PiecePercent);
			}
			if (!word.empty()) // 0x5CB897
			{
				const int w = Ftol(widthFn(state.font, word, lineHeight)); // 0x5CB8C4..0x5CB8C9
				if ((region.right - region.left) - w - pen + 1 < 0)        // 0x5CB8CE..0x5CB8DB
				{
					widest = std::max(widest, pen);
					pen = 0;
					if (++line >= maxLines) // 0x5CB8F8: the word is dropped
					{
						break;
					}
				}
				pen += w; // 0x5CB8FA
			}
		} while (line < maxLines); // 0x5CB90C
		widest = std::max(widest, pen); // 0x5CB91A
	}
	Measure out;
	out.width = static_cast<float>(widest);                                            // 0x5CB928..0x5CB932
	out.height = static_cast<float>(std::min(line + 1, maxLines)) * lineHeight; // 0x5CB924..0x5CB94B
	return out;
}

/// fn_005CB960(region, text, vAlign, yOffset, flag): measure, place and draw the words of one entry; returns the measured
/// height. lineHeight = +0x9C, alpha = +0xA4 (its low byte is the colour alpha, 0x5CBC8E).
float DrawEntryText(const TextRegion& region, std::u16string_view text, int vAlign, float yOffset, bool iconFlag,
               float lineHeight, int alpha, float number, DrawState& state, const WidthFn& widthFn,
               std::vector<TextRun>& runs)
{
	const int maxLines = Ftol(static_cast<float>(region.bottom - region.top + 1) / lineHeight); // 0x5CB9A8..0x5CB9BF
	const Measure measure = MeasureText(region, text, lineHeight, number, state, widthFn);      // 0x5CB9D6
	const int regionHeight = region.bottom - region.top + 1;
	float yAcc = 0.0f; // 0x5CB978
	if (vAlign == 2)   // 0x5CB9E6..0x5CB9FE: bottom
	{
		yAcc = static_cast<float>(regionHeight) - measure.height;
	}
	else if (vAlign != 0) // 0x5CBA00..0x5CBA19: centred (not truncated)
	{
		yAcc = (static_cast<float>(regionHeight) - measure.height) * 0.5f;
	}
	// 0x5CBA1D..0x5CBA37: the block is centred; every line starts at x0 (ragged right)
	const int x0 = Ftol((static_cast<float>(region.right - region.left + 1) - measure.width) * 0.5f);
	int pen = x0;
	int line = 0;
	int type = k_PieceWord;
	// (pending) the missionaries song karaoke (0x5CBA3C..0x5CBA62, 0x5CBB91..0x5CBDFB): music type GGlobal+0x28 in
	// 0x33..0x35, fn_00426C40, [0xD17CB0] (newest) and the word index == GScript+0x9C get a highlight box fn_00447BA0
	// driven by the Zoomers 0xD17BE8/0xD17C18/0xD17C48; not drawn. The word counter [esp+0x3C] is kept for it.
	int wordIndex = 0;
	Splitter splitter(text);
	std::u16string word;
	if (maxLines > 0) // 0x5CBA78
	{
		do
		{
			if (type == k_PieceStop || splitter.AtEnd()) // 0x5CBA84..0x5CBA99
			{
				break;
			}
			++wordIndex;                // 0x5CBAA3
			if (type == k_PieceNewLine) // 0x5CBAAD..0x5CBACC
			{
				yAcc += lineHeight;
				pen = x0;
				if (++line >= maxLines)
				{
					break;
				}
			}
			else if (type == k_PieceSpaced) // 0x5CBAD4..0x5CBB0B
			{
				pen = Ftol(widthFn(state.font, u" ", lineHeight) + static_cast<float>(pen)); // fiadd 0x5CBB00
			}
			type = splitter.Next(word, state, iconFlag); // 0x5CBB2B
			if (type == k_PiecePercent || type == k_PieceNumber) // 0x5CBB30..0x5CBB89
			{
				word = FormatNumber(number, type == k_PiecePercent);
				type = k_PieceSpaced; // 0x5CBB89: unlike the measure, a space follows the number
			}
			if (!word.empty()) // 0x5CBBD3
			{
				const float w = widthFn(state.font, word, lineHeight); // 0x5CBC0D
				// 0x5CBC16..0x5CBC39: wrap when (right - left + 1) - (pen + w) < 0; pen already includes x0
				if (static_cast<float>(region.right - region.left + 1) - (static_cast<float>(pen) + w) < 0.0f)
				{
					yAcc += lineHeight; // 0x5CBC3B..0x5CBC50
					pen = x0;
					if (++line >= maxLines) // 0x5CBC5C: the rest is dropped (no ellipsis)
					{
						break;
					}
				}
				// 0x5CBDFE..0x5CBE7F: DrawTextRaw(word, len, left + pen, top + yAcc + yOffset, near * 1.2, lineH,
				// {b,g,r,+0xA4}, 0, 0, top, bottom)
				TextRun run {};
				run.font = state.font;
				run.text = word;
				run.x = static_cast<float>(region.left + pen);
				run.y = static_cast<float>(region.top) + yAcc + yOffset; // 0x5CBE45..0x5CBE5F
				run.size = lineHeight;
				run.r = state.r;
				run.g = state.g;
				run.b = state.b;
				run.a = static_cast<uint8_t>(alpha & 0xFF);
				run.clipTop = static_cast<float>(region.top);
				run.clipBottom = static_cast<float>(region.bottom);
				runs.push_back(std::move(run));
				pen = Ftol(static_cast<float>(pen) + w); // 0x5CBE84..0x5CBE97
			}
		} while (line < maxLines); // 0x5CBE9B
	}
	static_cast<void>(wordIndex);
	return measure.height; // 0x5CBEAD
}
} // namespace

TextRegion ComputeTextRegion(int width, int height, int barPixels)
{
	// fn_005C57B0: H, W = [0xE8505A] / [0xE85058]; bar = fn_005C5780
	const int margin = Ftol(static_cast<float>(height - 2 * barPixels) * k_BoxMarginFraction); // 0x5C57DB..0x5C5800
	const int boxHeight = Ftol(static_cast<float>(height) * k_BoxHeightFraction);              // 0x5C57EA..0x5C5824
	TextRegion region {};
	region.left = 0;                                    // 0x5C581E
	region.bottom = height - margin - barPixels - 1;    // 0x5C5805..0x5C581D
	region.top = region.bottom - boxHeight;             // 0x5C5829..0x5C582D
	region.right = width;                               // 0x5C5830..0x5C5836
	return region;
}

int ClickCueHeight(const TextRegion& r)
{
	return (r.bottom - r.top + 1) / 3; // 0x5C5974..0x5C5985 (signed, toward zero)
}

int ClickCueY(const TextRegion& r)
{
	return r.bottom - ClickCueHeight(r) / 2 + 1; // 0x5C5987..0x5C599B
}

HelpTextDisplay::HelpTextDisplay(int screenHeight, bool biggerText)
{
	// fn_005CADC0 0x5CADE2..0x5CAE26: +0x9C = +0xA0 = H * (1/28 or 1/30); +0xA4 = 0xFF (0x5CAE2C, rewritten per entry)
	const float fraction = biggerText ? k_BiggerLineHeightFraction : k_LineHeightFraction;
	_baseLineHeight = static_cast<float>(screenHeight) * fraction; // fild; fmul 0x5CADFB / 0x5CAE14
	Reset(true); // 0x5CAE82 / 0x5CAFEA
}

void HelpTextDisplay::SetFontsLoaded(bool f1, bool f3)
{
	_f1Loaded = f1;
	_f3Loaded = f3;
}

void HelpTextDisplay::Add(std::u16string text, float number, int32_t narrator, bool drawable)
{
	// fn_005CCED0
	_newest = (_newest + 1) % k_Entries; // 0x5CCED0..0x5CCEEE (+0x98 too)
	_anim = 0.0f;                        // 0x5CCEE2: the slide-in restarts
	Entry& entry = _entries[static_cast<size_t>(_newest)];
	entry.used = true;
	entry.text = std::move(text); // 0x5CCF07 (the original keeps the pointer)
	entry.number = number;        // 0x5CCF0D
	entry.narrator = narrator;    // 0x5CCF14
	if (_entries[static_cast<size_t>((_newest + 5) % k_Entries)].used) // 0x5CCF17..0x5CCF2A
	{
		_singleLine = false; // 0x5CCF2C
	}
	_hidden = false;  // 0x5CCF33
	_boxShown = true; // 0x5CCF39
	// HelpSystem+0x584 (the current text) feeds fn_005C6E60; it is set by the caller, not here (inferred: the input of
	// the TEXT_DRAW gate is taken per Add). Reset leaves it alone: after ClearAllText the ring and the box are empty.
	_drawable = drawable;
}

void HelpTextDisplay::Reset(bool all)
{
	// 0x5CB020
	if (all)
	{
		for (Entry& entry : _entries) // 0x5CB02B..0x5CB03C: text and narrator (number and colour stay)
		{
			entry.used = false;
			entry.text.clear();
			entry.narrator = 0;
		}
		_newest = 0; // 0x5CB03E (+0x98 = 0, 0x5CB044)
	}
	// 0x5CB04A..0x5CB059: font +0x8 = 0 (set again per entry by fn_005CCF50), +0xAC, +0xB0, +0xB4
	_hidden = false;
	_singleLine = false;
	_boxShown = false;
}

void HelpTextDisplay::Close()
{
	_boxShown = false; // fn_005CB010
	_hidden = true;    // fn_005CB000
}

void HelpTextDisplay::SetSingleLine(bool on)
{
	_singleLine = on;
}

bool HelpTextDisplay::IsSingleLine() const
{
	return _singleLine;
}

bool HelpTextDisplay::IsTextDrawn(int textDraw) const
{
	// fn_005C6E60: 0 -> never (0x5C6EA4); 1 -> the current text's HelpTextData+4 (0x5C6E75..0x5C6EA3; no current
	// text -> 1); other -> always (0x5C6E6F)
	bool gate = true;
	if (textDraw == 0)
	{
		gate = false;
	}
	else if (textDraw == 1)
	{
		gate = _drawable;
	}
	return gate && !_hidden; // 0x5CC78B
}

void HelpTextDisplay::Advance(float dtMs, int textDraw)
{
	if (!IsTextDrawn(textDraw)) // fn_005CC760 0x5CC785 / 0x5CC793 return before the animation
	{
		return;
	}
	// 0x5CC7D3..0x5CC80A: +0xA8 += dt * 0.003; if (+0xA8 > 1) +0xA8 = 1
	_anim = dtMs * k_AnimSpeed + _anim; // each step rounded to a float (the FPU at 24 bits)
	if (_anim > 1.0f)
	{
		_anim = 1.0f;
	}
}

TextFrame HelpTextDisplay::Layout(int width, int height, int barPixels, int textDraw, bool topToBottom,
                                  const WidthFn& widthFn) const
{
	TextFrame frame;
	// fn_005C6BB0 0x5C6C08..0x5C6C2B recomputes the region every frame
	const TextRegion region = ComputeTextRegion(width, height, barPixels);
	frame.box = region;
	// Draw3D 0x5C59B0..0x5C59CB: the box only needs fn_005C6E60 (not +0xAC; Close clears +0xB4 anyway); fn_005CCE60
	// draws it when +0xB4. (g_game+0x250188 == 0 is the caller's)
	bool gate = true;
	if (textDraw == 0)
	{
		gate = false;
	}
	else if (textDraw == 1)
	{
		gate = _drawable;
	}
	frame.boxShown = gate && _boxShown;
	frame.boxAlpha = 0x80; // 0x5CCE89

	// fn_005CC760
	if (!IsTextDrawn(textDraw)) // 0x5CC77E..0x5CC793
	{
		return frame;
	}
	const float base = _baseLineHeight;
	const float anim = _anim;
	// 0x5CC799..0x5CC7C9: y0 = trunc(+0xA0 * (TOPTOBOTTOM ? -1/3 : 1/3)); scale 1
	int yCur = Ftol(base * (topToBottom ? -k_SlotThird : k_SlotThird));
	float scaleCur = 1.0f;
	// 0x5CC810..0x5CC872: the centred position of a single line and the slot before the first
	const int halfRegion = (region.bottom - region.top + 1) / 2;
	// (each step rounded to a float: 0x5CC828..0x5CC840 / 0x5CC854..0x5CC86C)
	float centre = 0.0f;
	int yPrev = 0;
	if (topToBottom)
	{
		centre = static_cast<float>(-halfRegion) + base * 0.5f;
		yPrev = Ftol(static_cast<float>(yCur) + base);
	}
	else
	{
		centre = static_cast<float>(halfRegion) - base * 0.5f;
		yPrev = Ftol(static_cast<float>(yCur) - base);
	}
	float scalePrev = k_EnterScale; // 0x5CC88E
	const bool previous = _entries[static_cast<size_t>((_newest + 5) % k_Entries)].used;
	if (!previous)
	{
		if (_singleLine) // 0x5CC8A1..0x5CC8C6: centred, no zoom (fade only)
		{
			yCur = Ftol(centre);
			yPrev = yCur;
			scalePrev = 1.0f;
		}
	}
	else if (!_entries[static_cast<size_t>((_newest + 4) % k_Entries)].used && _singleLine) // 0x5CC8C8..0x5CC905
	{
		yCur = Ftol((static_cast<float>(yCur) - centre) * anim + centre); // 0x5CC8E8..0x5CC8F2
		yPrev = yCur;
	}
	// 0x5CC90B..0x5CC926: six entries while the animation runs, four once it is done
	const int count = anim < k_AnimNearlyDone ? 6 : 4;
	for (int i = 0; i < count; ++i)
	{
		const int index = (_newest - i + k_Entries) % k_Entries; // 0x5CC93C..0x5CC94F (+0x98)
		const Entry& entry = _entries[static_cast<size_t>(index)];
		if (!entry.used) // 0x5CC95C
		{
			break;
		}
		// 0x5CC964..0x5CC9A5: from the previous slot to this one
		const float scale = (scaleCur - scalePrev) * anim + scalePrev;                         // 0x5CC964..0x5CC985
		const float y = static_cast<float>(yCur - yPrev) * anim + static_cast<float>(yPrev); // 0x5CC989..0x5CC997
		const float lineHeight = scale * base; // +0x9C
		// 0x5CC9AB..0x5CC9F9: alpha
		int alpha = Ftol(scale * 255.0f);
		if (alpha < 0xFF)
		{
			alpha -= 20;
		}
		if (alpha < 0)
		{
			alpha = 0;
		}
		if (i == 0)
		{
			alpha = Ftol(static_cast<float>(alpha) * anim); // the newest fades in (0x5CC9E8..0x5CC9F4)
		}
		// 0x5CCA01..0x5CCA36: fn_005CB960(&region, text, TOPTOBOTTOM ? 2 : 0, y, i == 0); [0xD17CB0] = (i == 0)
		DrawState state;
		state.f1Loaded = _f1Loaded;
		state.f3Loaded = _f3Loaded;
		state.SetEntry(entry.narrator); // fn_005CCF50 0x5CB984
		const float textHeight = DrawEntryText(region, entry.text, topToBottom ? 2 : 0, y, i == 0, lineHeight, alpha,
		                                  entry.number, state, widthFn, frame.runs);
		if (i == 0)
		{
			frame.controlIcon = state.controlIcon;
		}
		// 0x5CCA3B..0x5CCA93: the next slot
		scalePrev = scaleCur;
		yPrev = yCur;
		const float slot = textHeight / scale * scaleCur; // 0x5CCA4B fdiv, 0x5CCA5B / 0x5CCA63 fmul
		yCur = topToBottom ? Ftol(static_cast<float>(yCur) - slot) : Ftol(slot + static_cast<float>(yCur));
		scaleCur = scaleCur * (i == 0 ? k_SecondScale : k_OlderScale);
	}
	return frame;
}

} // namespace openblack::help
