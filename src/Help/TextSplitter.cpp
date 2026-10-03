/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TextSplitter.h"

namespace openblack::help::text_splitter
{

bool IsBlank(char16_t c)
{
	return c == 0x20 || c == 0x09 || c == 0x0D || c == 0x0A || c == k_Tilde;
}

bool IsEscape(char16_t c)
{
	return c == u'$' || c == u'\\';
}

bool IsDigit(char16_t c)
{
	return c >= u'0' && c <= u'9';
}

bool IsCode(char16_t c)
{
	return IsDigit(c) || std::u16string_view(u"CDFMNPcdfmnp").find(c) != std::u16string_view::npos;
}

void DrawState::SetFont(int32_t n)
{
	if (n == 2) // 0x5CCEA4
	{
		font = f1Loaded ? TextFont::F1 : TextFont::J0;
	}
	else if (n == 3) // 0x5CCEA9
	{
		font = f3Loaded ? TextFont::F3 : TextFont::J0;
	}
	else
	{
		font = TextFont::J0; // 0x5CCEAC
	}
}

void DrawState::SetColour(int32_t c)
{
	if (c != 0)
	{
		b = static_cast<uint8_t>((c >> 16) & 0xFF);
		g = static_cast<uint8_t>((c >> 8) & 0xFF);
		r = static_cast<uint8_t>(c & 0xFF);
		return;
	}
	if (narrator == 2) // 0x5CB490
	{
		r = 0xEB;
		g = 0xEB;
		b = 0xB7;
	}
	else if (narrator == 3) // 0x5CB46D
	{
		r = 0xFF;
		g = 0xB4;
		b = 0xB4;
	}
	else // 0x5CB44D
	{
		r = 0xFF;
		g = 0xFF;
		b = 0xFF;
	}
}

void DrawState::SetEntry(int32_t entryNarrator)
{
	narrator = entryNarrator;
	SetFont(entryNarrator); // 0x5CCF63
	SetColour(0);           // 0x5CCF6C
}

Splitter::Splitter(std::u16string_view text)
    : _text(text)
{
}

int Splitter::Next(std::u16string& word, DrawState& state, bool flag)
{
	word.clear(); // 0x5CB5A1
	int result = k_PieceWord;
	if (IsBlank(At(_pos))) // 0x5CB5B3
	{
		const int blanks = SkipBlanks(); // fn_005CB120: 1 or 3 (the test for 6 at 0x5CB5CE never matches)
		if (blanks != k_PieceSpaced && blanks != k_PieceTilde)
		{
			return blanks; // 0x5CB5D3..0x5CB5E2: a new line, empty word
		}
		result = k_PieceWord; // 0x5CB5E5
	}
	if (IsEscape(At(_pos))) // 0x5CB5F9
	{
		if (IsEscape(At(_pos + 1))) // 0x5CB613 -> 0x5CB68B: "$$" is the second character as a word character
		{
			word.push_back(At(_pos + 1));
			_pos += 2;
		}
		else
		{
			const int code = Code(state, flag); // fn_005CB2A0 0x5CB628
			SkipCode();                         // fn_005CB4E0 0x5CB636
			if (code != 0)
			{
				return code; // 0x5CB643..0x5CB650
			}
			if (IsBlank(At(_pos))) // 0x5CB65B: the blanks after a code are part of this (empty) piece
			{
				return SkipBlanks(); // 0x5CB670
			}
			return k_PieceWord;
		}
	}
	// 0x5CB6A9..0x5CB703: the word, up to a blank, an escape, the end or 0x2F characters
	while (!IsBlank(At(_pos)) && !IsEscape(At(_pos)) && At(_pos) != 0 && word.size() < k_MaxWord)
	{
		word.push_back(At(_pos));
		++_pos;
	}
	// 0x5CB716..0x5CB740: followed by blanks -> 3, by 0xF8FE -> 6 (the blanks are skipped by the next call)
	if (IsBlank(At(_pos)))
	{
		result = At(_pos) == k_Tilde ? k_PieceTilde : k_PieceSpaced;
	}
	return result;
}

int32_t Splitter::Wtoi(size_t i) const
{
	while (At(i) == u' ' || (At(i) >= 0x09 && At(i) <= 0x0D))
	{
		++i;
	}
	bool negative = false;
	if (At(i) == u'-' || At(i) == u'+')
	{
		negative = At(i) == u'-';
		++i;
	}
	int32_t value = 0;
	while (IsDigit(At(i)))
	{
		value = value * 10 + (At(i) - u'0');
		++i;
	}
	return negative ? -value : value;
}

int Splitter::SkipBlanks()
{
	if (!IsBlank(At(_pos)))
	{
		return k_PieceSpaced;
	}
	while (IsBlank(At(_pos)))
	{
		if (At(_pos) == u'\n') // 0x5CB13D
		{
			++_pos;
			return k_PieceNewLine;
		}
		++_pos;
	}
	return k_PieceSpaced;
}

int Splitter::Code(DrawState& state, bool flag) const
{
	const char16_t code = At(_pos + 1);
	if (!IsCode(code)) // 0x5CB2CC: unknown codes ($I, $s, $g...) only lose the escape (fn_005CB4E0 0x5CB54C)
	{
		return 0;
	}
	switch (code)
	{
	case u'C':
	case u'c':
		state.SetColour(Wtoi(_pos + 2)); // 0x5CB2FB..0x5CB30A
		return 0;
	case u'F':
	case u'f':
		state.SetFont(Wtoi(_pos + 2)); // 0x5CB35C..0x5CB36B
		return 0;
	case u'M':
	case u'm':
		// 0x5CB338..0x5CB34F: with the flag, fn_005CB3E0 -> HelpSystem fn_005C5B50(n), the control icon KMIcon at the
		// right of the box. (pending) not ported: only recorded in DrawState::controlIcon
		if (flag)
		{
			state.controlIcon = Wtoi(_pos + 2);
		}
		return 0;
	case u'N':
	case u'n':
		return k_PieceNewLine; // 0x5CB317
	case u'P':
	case u'p':
		return k_PiecePercent; // 0x5CB322
	case u'D':
	case u'd':
		return k_PieceNumber; // 0x5CB32D
	default:
		// 0x5CB378: _wtoi of the digits, fn_005CB4C0 (2 for 1, else 0)
		return Wtoi(_pos + 1) == 1 ? k_PieceStop : 0;
	}
}

void Splitter::SkipCode()
{
	++_pos; // 0x5CB501
	const char16_t code = At(_pos);
	if (code == u'C' || code == u'c') // fn_005CB200 0x5CB507
	{
		++_pos;
		while (IsDigit(At(_pos)))
		{
			++_pos;
		}
		// 0x5CB53A skips one more character (the original reads past a terminating 0 here; not done)
		if (_pos < _text.size())
		{
			++_pos;
		}
		return;
	}
	if (!IsCode(code)) // 0x5CB54C: only the escape is skipped
	{
		return;
	}
	++_pos; // 0x5CB555..0x5CB57D
	while (IsDigit(At(_pos)))
	{
		++_pos;
	}
}

} // namespace openblack::help::text_splitter
