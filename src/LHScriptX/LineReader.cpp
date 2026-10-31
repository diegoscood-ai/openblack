/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LineReader.h"

#include <cstdlib>

#include <algorithm>
#include <array>

using namespace openblack::lhscriptx;

namespace
{
/// The bytes above 0x7F that the original's C runtime takes as spaces between words
constexpr std::array<unsigned char, 8> k_HighSpaces = {0x9D, 0x9E, 0xAD, 0xAE, 0xB6, 0xBE, 0xFB, 0xFD};

/// The C locale's spaces, as the number readers see them: space, tab, line feed, vertical tab, form feed, carriage
/// return
[[nodiscard]] constexpr bool IsNumberSpace(char c) noexcept
{
	return c == ' ' || (c >= '\t' && c <= '\r');
}

[[nodiscard]] constexpr bool IsDigit(char c) noexcept
{
	return c >= '0' && c <= '9';
}

/// A character that ends a bare word and is dropped with it, or is passed over after a quoted text
[[nodiscard]] constexpr bool IsSeparator(char c) noexcept
{
	return c == ',' || c == '(' || c == ')' || c == ':';
}

/// A character that makes a word of its own and ends a bare word before it
[[nodiscard]] constexpr bool IsOperator(char c) noexcept
{
	return c == '=' || c == '!' || c == '>' || c == '<';
}

[[nodiscard]] constexpr char Upper(char c) noexcept
{
	return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
}
} // namespace

bool line_reader::IsWordSpace(char c) noexcept
{
	return IsNumberSpace(c) || std::ranges::find(k_HighSpaces, static_cast<unsigned char>(c)) != k_HighSpaces.end();
}

line_reader::LineCursor::LineCursor(std::string_view line)
    : _line(line.substr(0, line.find('\0')))
{
}

char line_reader::LineCursor::At() const noexcept
{
	return _at < _line.size() ? _line[_at] : '\0';
}

void line_reader::LineCursor::SkipSpaces() noexcept
{
	while (IsWordSpace(At()))
	{
		++_at;
	}
}

void line_reader::LineCursor::NoteEndOfLine() noexcept
{
	if (At() == '\n' || At() == '\0')
	{
		_endOfLine = true;
	}
}

line_reader::Word line_reader::LineCursor::Next()
{
	Word word;
	SkipSpaces();
	NoteEndOfLine();
	if (At() == '"')
	{
		word.quoted = true;
		++_at;
		while (At() != '"' && At() != '\0' && At() != '\n')
		{
			word.text += At();
			++_at;
		}
		if (At() == '"')
		{
			++_at;
			SkipSpaces();
			if (IsSeparator(At()))
			{
				++_at;
			}
		}
		NoteEndOfLine();
		return word;
	}
	if (IsOperator(At()))
	{
		word.text += At();
		++_at;
		if (At() == '=' || At() == '>')
		{
			word.text += At();
			++_at;
		}
	}
	else
	{
		while (!IsWordSpace(At()) && At() != '\0')
		{
			const char c = At();
			if (IsSeparator(c))
			{
				++_at;
				break;
			}
			if (IsOperator(c))
			{
				break;
			}
			++_at;
			word.text += Upper(c);
		}
	}
	NoteEndOfLine();
	return word;
}

int32_t line_reader::AtoL(std::string_view text) noexcept
{
	size_t at = 0;
	while (at < text.size() && IsNumberSpace(text[at]))
	{
		++at;
	}
	bool negative = false;
	if (at < text.size() && (text[at] == '-' || text[at] == '+'))
	{
		negative = text[at] == '-';
		++at;
	}
	// unsigned, so that a number too big wraps as the original's does
	uint32_t total = 0;
	for (; at < text.size() && IsDigit(text[at]); ++at)
	{
		total = total * 10u + static_cast<uint32_t>(text[at] - '0');
	}
	if (negative)
	{
		total = 0u - total;
	}
	return static_cast<int32_t>(total);
}

double line_reader::AtoF(std::string_view text)
{
	size_t at = 0;
	while (at < text.size() && IsNumberSpace(text[at]))
	{
		++at;
	}
	// The number's own characters, copied with the exponent letter as 'e' so that strtod reads exactly them
	std::string number;
	if (at < text.size() && (text[at] == '-' || text[at] == '+'))
	{
		number += text[at++];
	}
	size_t digits = 0;
	for (; at < text.size() && IsDigit(text[at]); ++at, ++digits)
	{
		number += text[at];
	}
	if (at < text.size() && text[at] == '.')
	{
		number += text[at++];
		for (; at < text.size() && IsDigit(text[at]); ++at, ++digits)
		{
			number += text[at];
		}
	}
	if (digits == 0)
	{
		return 0.0;
	}
	if (at < text.size() && (text[at] == 'e' || text[at] == 'E' || text[at] == 'd' || text[at] == 'D'))
	{
		std::string exponent = "e";
		size_t next = at + 1;
		if (next < text.size() && (text[next] == '-' || text[next] == '+'))
		{
			exponent += text[next++];
		}
		const auto first = next;
		for (; next < text.size() && IsDigit(text[next]); ++next)
		{
			exponent += text[next];
		}
		if (next > first)
		{
			number += exponent;
		}
	}
	return std::strtod(number.c_str(), nullptr);
}

void line_reader::ForEachLine(std::string_view text, const std::function<bool(std::string_view line, int number)>& run)
{
	int number = 0;
	size_t start = 0;
	while (start < text.size())
	{
		const auto end = text.find('\n', start);
		++number;
		const auto line = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
		if (!line.empty() && !run(line, number))
		{
			return;
		}
		if (end == std::string_view::npos)
		{
			return;
		}
		start = end + 1;
	}
}

openblack::map_coords::MapCoords line_reader::PositionUnits(std::string_view text)
{
	map_coords::MapCoords at {.x = map_coords::ToFixedGUtils(static_cast<float>(AtoF(text)))};
	const auto comma = text.find(',');
	if (comma == std::string_view::npos)
	{
		return at;
	}
	const auto rest = text.substr(comma + 1);
	at.z = map_coords::ToFixedGUtils(static_cast<float>(AtoF(rest)));
	if (const auto second = rest.find(','); second != std::string_view::npos)
	{
		at.altitude = static_cast<float>(AtoF(rest.substr(second + 1)));
	}
	return at;
}
