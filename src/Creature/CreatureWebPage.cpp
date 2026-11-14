/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureWebPage.h"

#include <algorithm>
#include <ranges>

#include "Common/FixedFormat.h"
#include "Creature/CreatureAttention.h"
#include "Help/ScriptSpirits.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace openblack::creature_web_page
{

namespace
{

/// A value with a fraction, as "%.2f" writes it
std::string TwoDigits(float value)
{
	return fixed_format::Fixed(static_cast<double>(value), 0, 2);
}

/// A count, as "%d" writes it: the game holds its counts signed
std::string Whole(uint32_t value)
{
	return std::to_string(static_cast<int32_t>(value));
}

/// The longest piece of a line read at once, one less than the reader's buffer
constexpr size_t k_LongestPiece = 0x800 - 1;

constexpr char k_LineFeed = '\n';
constexpr char k_CarriageReturn = '\r';
constexpr char k_EndOfText = '\x1A';

/// The text as a text file reads it: each carriage return before a line feed dropped, and nothing from the first
/// Ctrl+Z on
std::string ReadAsText(std::string_view raw)
{
	std::string text;
	text.reserve(raw.size());
	for (size_t i = 0; i < raw.size(); ++i)
	{
		const char c = raw[i];
		if (c == k_EndOfText)
		{
			break;
		}
		if (c == k_CarriageReturn && i + 1 < raw.size() && raw[i + 1] == k_LineFeed)
		{
			continue;
		}
		text.push_back(c);
	}
	return text;
}

/// The value of a key, the last one given of it
const std::string* Find(std::span<const Field> fields, std::string_view key)
{
	const auto found = std::ranges::find(std::views::reverse(fields), key, &Field::key);
	return found == std::views::reverse(fields).end() ? nullptr : &found->value;
}

/// One piece of a line filled in. The piece is read as the game's buffer holds it, with nothing but zeros after it; it
/// ends at its first zero byte, and a key with no value leaves a zero in the line written, which ends it there
std::string FillPiece(std::string_view piece, std::span<const Field> fields)
{
	const auto at = [piece](size_t i) { return i < piece.size() ? piece[i] : '\0'; };
	std::string out;
	bool cut = false;
	size_t i = 0;
	while (at(i) != '\0')
	{
		if (at(i) == '[' && at(i + 1) == '$')
		{
			i += 2;
			std::string key;
			while (at(i) != '\0' && at(i) != '$')
			{
				key.push_back(at(i));
				++i;
			}
			if (const auto* value = Find(fields, key); value != nullptr)
			{
				if (!cut)
				{
					out += *value;
				}
			}
			else
			{
				cut = true;
			}
			// the closing "$" is passed over, and the character after it (the "]") with it, unless the line ends there
			if (at(i) == '$' && at(i + 1) != '\0')
			{
				++i;
			}
		}
		else if (!cut)
		{
			out.push_back(at(i));
		}
		++i;
	}
	return out;
}

/// The text as a text file writes it: each line feed as a carriage return and a line feed
std::string WriteAsText(std::string_view text)
{
	std::string out;
	out.reserve(text.size() + (text.size() / 16));
	for (const char c : text)
	{
		if (c == k_LineFeed)
		{
			out.push_back(k_CarriageReturn);
		}
		out.push_back(c);
	}
	return out;
}

/// The characters of Windows' Western code page from 0x80 to 0x9F, by their byte; the five bytes it leaves undefined
/// stand for the control characters of the same number
constexpr std::array<char16_t, 32> k_Western80 {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
    0x2039, 0x0152, 0x008D, 0x017D, 0x008F, 0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
    0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
};

constexpr char16_t k_FirstHighSurrogate = 0xD800;
constexpr char16_t k_FirstLowSurrogate = 0xDC00;
constexpr char16_t k_AfterLowSurrogates = 0xE000;

/// The Western code page's conversion where the system has none of its own: the characters it has as their byte, any
/// other as "?" (no best fit)
[[maybe_unused]] std::string WesternCodePage(std::u16string_view text)
{
	std::string out;
	out.reserve(text.size());
	for (size_t i = 0; i < text.size(); ++i)
	{
		const char16_t c = text[i];
		if (c < 0x80 || (c >= 0xA0 && c <= 0xFF))
		{
			out.push_back(static_cast<char>(c));
			continue;
		}
		if (const auto found = std::ranges::find(k_Western80, c); found != k_Western80.end())
		{
			out.push_back(static_cast<char>(0x80 + std::distance(k_Western80.begin(), found)));
			continue;
		}
		// a pair of surrogates is one character, and one "?"
		if (c >= k_FirstHighSurrogate && c < k_FirstLowSurrogate && i + 1 < text.size() && text[i + 1] >= k_FirstLowSurrogate &&
		    text[i + 1] < k_AfterLowSurrogates)
		{
			++i;
		}
		out.push_back('?');
	}
	return out;
}

/// The bytes the game's conversion of a name holds at most, its closing zero with them
constexpr size_t k_NarrowBuffer = 0x3FF;

} // namespace

std::vector<Field> FieldsOf(const Facts& facts)
{
	std::vector<Field> fields {
	    {"name", Narrow(facts.name)},
	    {"health", TwoDigits(facts.health)},
	    {"alignment", TwoDigits(facts.alignment)},
	    {"age", Whole(facts.age)},
	    {"energy", TwoDigits(facts.energy)},
	    {"strength", TwoDigits(facts.strength)},
	    {"fatness", TwoDigits(facts.fatness)},
	    {"exhaustion", TwoDigits(facts.exhaustion)},
	    {"dehydration", TwoDigits(facts.dehydration)},
	    {"itchiness", TwoDigits(facts.itchiness)},
	    {"amount-of-poo", TwoDigits(facts.poo)},
	};
	constexpr std::array<std::string_view, 7> k_TallyKeys {
	    "num-people-killed", "num-animals-killed", "num-creatures-killed", "num-battles-fought",
	    "num-battles-won",   "num-poos",           "num-mushrooms",
	};
	for (size_t i = 0; i < k_TallyKeys.size(); ++i)
	{
		fields.push_back({std::string(k_TallyKeys.at(i)), Whole(facts.tallies.at(i))});
	}
	for (const auto& desire : facts.desires)
	{
		fields.push_back({"desire-" + desire.name, TwoDigits(desire.level)});
	}
	for (size_t kind = 0; kind < facts.beliefs.size(); ++kind)
	{
		fields.push_back({"beliefs_" + std::to_string(kind), Whole(facts.beliefs.at(kind))});
	}
	if (facts.spellReceived.has_value())
	{
		// named by the number of the game's text for the miracle received
		fields.push_back({"spell_received_" + std::to_string(*facts.spellReceived + 0x416u), "1"});
	}
	const auto nice = static_cast<uint32_t>(help::script_spirits::DiscreteAlignment(facts.attitudeToPlayer)) +
	                  static_cast<uint32_t>(k_FirstNiceText);
	fields.push_back({"how_nice_player_has_been", std::to_string(static_cast<int32_t>(nice))});
	const auto attention = creature_attention::HowMuch(creature_attention::Share(facts.turnsLookedAt, facts.turnsLived));
	fields.push_back({"how_much_attention_player_has_given_me", std::to_string(attention + k_FirstAttentionText)});
	return fields;
}

std::string Fill(std::string_view templateText, std::span<const Field> fields)
{
	const auto text = ReadAsText(templateText);
	std::string page;
	for (size_t start = 0; start < text.size();)
	{
		// a piece runs to the end of its line, the line feed with it, or to the longest piece the reader takes
		const auto lineEnd = text.find(k_LineFeed, start);
		const auto end =
		    std::min({lineEnd == std::string::npos ? text.size() : lineEnd + 1, start + k_LongestPiece, text.size()});
		page += FillPiece(std::string_view(text).substr(start, end - start), fields);
		start = end;
	}
	return WriteAsText(page);
}

std::string Narrow(std::u16string_view text)
{
	// the text up to its first zero, as the game hands the system a text that a zero ends
	text = text.substr(0, text.find(u'\0'));
#ifdef _WIN32
	// the system's own conversion, through the machine's code page, with its best fit for a character the code page
	// lacks and its own default character, into a buffer of 0x3FF bytes; a name that doesn't fit gives nothing
	std::array<char, k_NarrowBuffer> buffer {};
	const auto written =
	    WideCharToMultiByte(CP_ACP, 0, reinterpret_cast<const wchar_t*>(text.data()), static_cast<int>(text.size()),
	                        buffer.data(), static_cast<int>(buffer.size()), nullptr, nullptr);
	// the game's own count takes its closing zero too, which must fit as well
	if (written <= 0 || static_cast<size_t>(written) >= buffer.size())
	{
		return {};
	}
	return {buffer.data(), static_cast<size_t>(written)};
#else
	auto out = WesternCodePage(text);
	return out.size() >= k_NarrowBuffer ? std::string {} : out;
#endif
}

std::filesystem::path PagePath(std::u16string_view creatureName)
{
	return std::filesystem::path(std::string(k_Folder)) / std::filesystem::path(Narrow(creatureName) + ".html");
}

} // namespace openblack::creature_web_page
