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

#include <functional>
#include <string>
#include <string_view>

#include "3D/MapCoords.h"

/// How the original reads the text of a land script: its lines, the words of a line and the numbers written in a word
/// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read). Pure functions with no game state; Script.cpp runs the
/// words.
namespace openblack::lhscriptx::line_reader
{

/// One word of a line
struct Word
{
	/// A quoted text as it is written; a bare word in capitals
	std::string text;
	/// The word was written in quotes, so it is never a command or a variable's name
	bool quoted {false};
};

/// Whether the reader takes a character as space between words: a space, a tab, a line feed, a vertical tab, a form
/// feed or a carriage return, and eight bytes above 0x7F (0x9D, 0x9E, 0xAD, 0xAE, 0xB6, 0xBE, 0xFB and 0xFD), which the
/// original's C runtime answers as spaces for a character read as negative
[[nodiscard]] bool IsWordSpace(char c) noexcept;

/// The words of one line, one after the other. A line ends at its end or at its first NUL. A quoted text runs to its
/// closing quote, the end of the line or a line feed; after the closing quote the spaces and one of `,()` or `:` are
/// passed over. `=`, `!`, `<` and `>` make a word of their own, with a second `=` or `>` after them. A bare word ends at
/// a space, before one of `=!<>`, after one of `,():` (which is dropped) or at the end of the line, and is read in
/// capitals (a to z only)
class LineCursor
{
public:
	explicit LineCursor(std::string_view line);

	/// The next word: an empty one at the end of the line (and between two commas)
	[[nodiscard]] Word Next();

	/// Whether the cursor has stood at the end of the line (or at a line feed) after a word's leading spaces or after a
	/// word. Once set it stays set: an argument after it is missing
	[[nodiscard]] bool EndOfLine() const noexcept { return _endOfLine; }

private:
	/// The character under the cursor, NUL at the end of the line
	[[nodiscard]] char At() const noexcept;
	void SkipSpaces() noexcept;
	void NoteEndOfLine() noexcept;

	std::string_view _line;
	size_t _at {0};
	bool _endOfLine {false};
};

/// A whole number as the original's C runtime reads one (atol): spaces, one sign, then the digits up to the first other
/// character, wrapping modulo 2^32. "99999900000" is 1215652192, "1.9" is 1, "+5" is 5, "" and "abc" are 0
[[nodiscard]] int32_t AtoL(std::string_view text) noexcept;

/// A fraction as the original's C runtime reads one (atof): spaces, one sign, the digits with one point, then an
/// exponent after `e`, `E`, `d` or `D` when digits follow it; correctly rounded. Anything else ends it, so hexadecimal,
/// "inf" and "nan" are 0; a number too big is infinite
[[nodiscard]] double AtoF(std::string_view text);

/// A position's text in map units, as the original reads one: x is the text's fraction up to the first comma and z
/// the fraction after it (0 with no comma), each x 65536 / 10 in floats and truncated; the altitude is the fraction
/// after a second comma, as it is (0 with none)
[[nodiscard]] map_coords::MapCoords PositionUnits(std::string_view text);

/// Calls `run` with each line of a text and its number from 1, in order, until `run` answers false. A line ends at a
/// line feed, which is dropped (a carriage return stays in the line); an empty line is passed over, and a last line
/// with no line feed after it is run too
void ForEachLine(std::string_view text, const std::function<bool(std::string_view line, int number)>& run);

} // namespace openblack::lhscriptx::line_reader
