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
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "TextSplitter.h"

// The display side of the script dialogue texts: HelpText (runblack.exe W120, HelpSystem+0x14, 0xBC bytes), its ring of
// six entries, the box region, the stack of texts with its slide-in animation and the word layout. Pure logic: no bgfx,
// no Locator, no game globals; the renderer draws the TextFrame that Layout returns (the box with fn_0081E360, each run
// with GatheringText::DrawTextRaw 0x832C60). Sources: dev\documentacion\intro\spec_text.md and the disassembly of
// 0x5CAD40..0x5CAFF2, 0x5CB000..0x5CB750 (splitter), 0x5CB750..0x5CB953 (measure), 0x5CB960..0x5CBEBB (draw),
// 0x5CC760..0x5CCAA4 (stack), 0x5CCE60..0x5CCF72 (box, fonts, AddText), 0x5C57B0, 0x5C5970, 0x5C6E60 (bwdis.py).

namespace openblack::help
{

/// LHRegion HelpText+0x24 (ints, inclusive right/bottom as the users treat them: width = right - left + 1)
struct TextRegion
{
	int left;
	int top;
	int right;
	int bottom;
};

/// fn_005C57B0: the box, a full-width strip above the bottom bar. barPixels = fn_005C5780 (ScreenFade::LetterboxHeight)
[[nodiscard]] TextRegion ComputeTextRegion(int width, int height, int barPixels);
/// fn_005C5970: the vertical centre of the "click to continue" KMIcon, bottom - trunc(ClickCueHeight / 2) + 1
[[nodiscard]] int ClickCueY(const TextRegion& r);
/// The height of that KMIcon, trunc((bottom - top + 1) / 3) (0x5C5974..0x5C5985)
[[nodiscard]] int ClickCueHeight(const TextRegion& r);

/// One DrawTextRaw call (0x5CBE04..0x5CBE7F): one word at (x, y), font size = line height, colour {r,g,b,a}, clipped
/// vertically to [clipTop, clipBottom] (the box top and bottom). z = near * 1.2 and depth 0 are the renderer's.
struct TextRun
{
	TextFont font;
	std::u16string text;
	float x;
	float y;
	float size;
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
	float clipTop;
	float clipBottom;
};

/// What one frame draws: the background box (fn_005CCE60, black {b,g,r of +0x1C} at alpha 0x80) and the words
struct TextFrame
{
	bool boxShown {false};
	TextRegion box {};
	uint8_t boxAlpha {0x80};
	std::vector<TextRun> runs;
	/// The last $M<n> of the newest text seen this frame (the BINDABLE_ACTION n for fn_005C5B50 via fn_005CB3E0), -1 when
	/// none. (pending) the control icon KMIcon itself is not ported: nothing is drawn for it.
	int controlIcon {-1};
};

/// GatheringText::GetStringWidth 0x831130 of a font: the width of the text at that font size
using WidthFn = std::function<float(TextFont, std::u16string_view, float size)>;

/// HelpText (HelpSystem+0x14), 0xBC bytes
class HelpTextDisplay
{
public:
	/// The ring of entries (+0x34 + 16 * k, k < 6)
	static constexpr int k_Entries = 6;

	/// ctor 0x5CAD40 / fn_005CADC0: base line height +0xA0 = screenHeight * (1/30) (0x8CF3F8), or * (1/28) (0x92A53C) when
	/// NeedsBiggerText 0x4079C0; computed once. Ends with Reset(1) (0x5CAFEA).
	explicit HelpTextDisplay(int screenHeight, bool biggerText = false);

	/// Whether Data\f1 / Data\f3 were loaded ([0xECCD0C] / [0xECCD14]); a missing one is replaced by j0 (0x5CAE3F..0x5CAE6E)
	void SetFontsLoaded(bool f1, bool f3);

	/// AddText fn_005CCED0(text, number, narrator). drawable = the TEXT_DRAW gate input of the text (HelpTextData+4 != 0,
	/// fn_005C6E60 0x5C6E75..0x5C6EA3)
	void Add(std::u16string text, float number, int32_t narrator, bool drawable);
	/// HelpText::Reset 0x5CB020: all -> ring emptied (text and narrator of the 6 entries, +0x94 = +0x98 = 0); always font
	/// +0x8, hidden +0xAC, singleLine +0xB0, box shown +0xB4 = 0
	void Reset(bool all);
	/// fn_005CB010: box hidden (+0xB4 = 0), then fn_005CB000 (+0xAC = 1: the texts are not drawn)
	void Close();

	/// +0xB0, set by RUN_TEXT / TEMP_TEXT (0x6F7D60, 0x6F7E40)
	void SetSingleLine(bool on);
	[[nodiscard]] bool IsSingleLine() const;
	[[nodiscard]] bool IsHidden() const { return _hidden; }
	[[nodiscard]] bool IsBoxShown() const { return _boxShown; }
	/// +0xA8, the slide-in animation of the newest text, 0..1
	[[nodiscard]] float GetAnimation() const { return _anim; }

	/// The gate of fn_005CC760 (0x5CC77E..0x5CC793): fn_005C6E60 (TEXT_DRAW profile value) and not hidden (+0xAC)
	[[nodiscard]] bool IsTextDrawn(int textDraw) const;
	/// fn_005CC760 0x5CC7BE..0x5CC80A: +0xA8 += dt * 0.003 (0x8DF89C), clamp 1. dt in ms: g_delta_time in the citadel
	/// (g_game+0x205A28 == 1), else g_game_time_inc. The original does it inside the draw, after the gate: it does not
	/// advance when IsTextDrawn(textDraw) is false (pass the TEXT_DRAW value; the default 2 = always)
	void Advance(float dtMs, int textDraw = 2);

	/// The frame: the region (fn_005C57B0), the box (Draw3D 0x5C59B0..0x5C59CB -> fn_005CCE60) and the stack of texts
	/// (fn_005CC760 -> fn_005CB960 -> fn_005CB750). textDraw = profile TEXT_DRAW (HelpSystem+0x4604), topToBottom =
	/// profile TEXT_TOPTOBOTTOM ([0xD16180]).
	[[nodiscard]] TextFrame Layout(int width, int height, int barPixels, int textDraw, bool topToBottom,
	                               const WidthFn& widthFn) const;

	/// +0xA0 / +0x9C at rest
	[[nodiscard]] float GetBaseLineHeight() const { return _baseLineHeight; }

private:
	struct Entry
	{
		bool used {false};      ///< +0 wchar* text != 0
		std::u16string text;    ///< +0
		float number {0.0f};    ///< +4, the value of $P / $D
		int32_t narrator {0};   ///< +8
	};

	std::array<Entry, k_Entries> _entries {};
	int _newest {0};               ///< +0x94
	float _baseLineHeight {0.0f};  ///< +0xA0
	float _anim {0.0f};            ///< +0xA8 (not set by the ctor; 0 here, inferred)
	bool _hidden {false};          ///< +0xAC
	bool _singleLine {false};      ///< +0xB0
	bool _boxShown {false};        ///< +0xB4
	bool _drawable {true};         ///< HelpTextData+4 of the current text (HelpSystem+0x584), see Add / Reset
	bool _f1Loaded {true};
	bool _f3Loaded {true};
};

} // namespace openblack::help
