/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <string>
#include <string_view>

// The text splitter of HelpText (runblack.exe W120): fn_005CB590 and the character tests and skips it uses
// (0x5CB0F0..0x5CB583). One splitter for the word count of the reading time (fn_005CBEC0, HelpSystem CountWords) and for
// the display (fn_005CB750 measure, fn_005CB960 draw, HelpTextDisplay). The escape codes change the display state
// (fn_005CB2A0: $C colour fn_005CB400, $F font fn_005CCEA0, $M control icon fn_005CB3E0); the counting caller passes a
// throwaway state and flag 0, as fn_005CBEC0 does.

namespace openblack::help
{

/// The GatheringText fonts of HelpText (+0xC/+0x10/+0x14, table 0xECCD08), chosen by fn_005CCEA0
enum class TextFont : uint8_t
{
	J0, ///< Data\j0 "Ocean Sans MM": font[0], everything that is not 2 or 3
	F1, ///< Data\f1 "Footlight MT": font[1] (2 = GOOD_SPIRIT), j0 when not loaded (0x5CAE58..0x5CAE6E)
	F3, ///< Data\f3 "Orange LET": font[3] (3 = EVIL_SPIRIT), j0 when not loaded (0x5CAE3F..0x5CAE55)
};

namespace text_splitter
{

/// '~' in InfoScript2.txt becomes 0xF8FE when loaded (fn_007191F0): a blank that adds no space
constexpr char16_t k_Tilde = 0xF8FE;
/// A word stops after 0x2F characters (0x5CB6E3); the rest is the next piece
constexpr size_t k_MaxWord = 0x2F;

/// fn_005CB0F0
[[nodiscard]] bool IsBlank(char16_t c);
/// fn_005CB190
[[nodiscard]] bool IsEscape(char16_t c);
/// fn_005CB1B0
[[nodiscard]] bool IsDigit(char16_t c);
/// fn_005CB1D0: a digit, or fn_005CB220: C/c (fn_005CB200) or one of DFMNPdfmnp (byte table 0x5CB26C over 'D'..'p')
[[nodiscard]] bool IsCode(char16_t c);

/// The piece types fn_005CB590 returns
enum Piece : int
{
	k_PieceWord = 0,    ///< a word (or nothing) followed by a non-blank
	k_PieceNewLine = 1, ///< a blank run containing '\n', or $N
	k_PieceStop = 2,    ///< $1
	k_PieceSpaced = 3,  ///< a word followed by blanks (or a code followed by blanks): a space before the next piece
	k_PiecePercent = 4, ///< $P: the number with "%3.3f%%" (0xBF1A74)
	k_PieceNumber = 5,  ///< $D: the number with "%3.3f" (0xBF1A68)
	k_PieceTilde = 6,   ///< a word followed by 0xF8FE: no space
};

/// The HelpText state that fn_005CB2A0's codes change while a text is measured and drawn: the current font +0x8 and the
/// colour of the entry being drawn (+0x34 + 16 * (+0x98) + 0xC..0xE)
struct DrawState
{
	TextFont font {TextFont::J0};
	uint8_t r {0xFF};
	uint8_t g {0xFF};
	uint8_t b {0xFF};
	int32_t narrator {0};
	bool f1Loaded {true};
	bool f3Loaded {true};
	/// The last $M<n> seen with the flag set; -1 when none. (pending) fn_005C5B50, the control icon, is not ported
	int controlIcon {-1};

	/// fn_005CCEA0(n): 2 -> +0x14 (f1, or j0 when not loaded), 3 -> +0x10 (f3, or j0), else +0xC (j0)
	void SetFont(int32_t n);
	/// fn_005CB400(c): c != 0 -> b = c >> 16, g = c >> 8, r = c (0x5CB408..0x5CB433, c as 0xBBGGRR); c == 0 -> the
	/// narrator default: 2 -> (235,235,183), 3 -> (255,180,180), else white (0x5CB436..0x5CB4AE)
	void SetColour(int32_t c);
	/// fn_005CCF50: font and colour defaults of the entry being drawn
	void SetEntry(int32_t entryNarrator);
};

/// fn_005CB590(text, &next, word, flag) as an object walking one text
class Splitter
{
public:
	explicit Splitter(std::u16string_view text);

	/// The end of the text (the callers' `while (*text)`)
	[[nodiscard]] bool AtEnd() const { return At(_pos) == 0; }

	/// The next piece: its type (Piece) and its word (empty for codes and new lines). flag = fn_005CB590's last argument,
	/// passed to fn_005CB2A0: only with it does $M act (0x5CB348)
	int Next(std::u16string& word, DrawState& state, bool flag);

private:
	[[nodiscard]] char16_t At(size_t i) const { return i < _text.size() ? _text[i] : u'\0'; }

	/// _wtoi 0x7C87D8 from position i (approximate: the CRT white space set)
	[[nodiscard]] int32_t Wtoi(size_t i) const;
	/// fn_005CB120: past the blanks; 1 when it went past a line feed (stops right after it), 3 otherwise
	int SkipBlanks();
	/// fn_005CB2A0(text, flag) at an escape: the code's piece type and its effect on the state
	int Code(DrawState& state, bool flag) const;
	/// fn_005CB4E0: past the escape, its code and the code's digits (and one more character after C<digits>)
	void SkipCode();

	std::u16string_view _text;
	size_t _pos {0};
};

} // namespace text_splitter

} // namespace openblack::help
