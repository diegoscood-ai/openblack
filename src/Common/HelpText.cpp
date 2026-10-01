/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpText.h"

#include <cstdio>

#include <optional>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

namespace
{
std::vector<openblack::helptext::Entry> s_entries;
bool s_loaded = false;

std::u16string Utf16(const std::vector<uint8_t>& bytes)
{
	std::u16string text;
	size_t start = bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE ? 2 : 0;
	text.reserve((bytes.size() - start) / 2);
	for (size_t i = start; i + 1 < bytes.size(); i += 2)
	{
		text.push_back(static_cast<char16_t>(bytes[i] | (bytes[i + 1] << 8)));
	}
	return text;
}

bool IsBlank(char16_t c)
{
	return c == u' ' || c == u'\t' || c == u'\r' || c == u'\n';
}

std::u16string_view Trim(std::u16string_view s)
{
	while (!s.empty() && IsBlank(s.front()))
	{
		s.remove_prefix(1);
	}
	while (!s.empty() && IsBlank(s.back()))
	{
		s.remove_suffix(1);
	}
	return s;
}

std::string Narrow(std::u16string_view s)
{
	std::string out;
	out.reserve(s.size());
	for (const auto c : s)
	{
		out.push_back(c < 0x80 ? static_cast<char>(c) : '?');
	}
	return out;
}

// A number as the leading digits (inferred: like _wtoi; the reader LHScriptX 0x7E7960 is not read). The declarations of
// W120 end with U+0A0D after the digits (a stray CR LF in the UTF-16 file).
std::optional<int32_t> Number(std::u16string_view s)
{
	s = Trim(s);
	bool negative = false;
	if (!s.empty() && s.front() == u'-')
	{
		negative = true;
		s.remove_prefix(1);
	}
	if (s.empty() || s.front() < u'0' || s.front() > u'9')
	{
		return std::nullopt;
	}
	int32_t value = 0;
	for (const auto c : s)
	{
		if (c < u'0' || c > u'9')
		{
			break;
		}
		value = value * 10 + static_cast<int32_t>(c - u'0');
	}
	return negative ? -value : value;
}

void Load()
{
	s_loaded = true;
	try
	{
		auto& fileSystem = openblack::Locator::filesystem::value();
		const auto path = fileSystem.GetPath<openblack::filesystem::Path::Scripts>() / "InfoScript2.txt";
		s_entries = openblack::helptext::Parse(fileSystem.ReadAll(fileSystem.FindPath(path)));
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Help texts: {} entries from InfoScript2.txt", s_entries.size());
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Help texts: cannot read InfoScript2.txt: {}", e.what());
	}
}
} // namespace

std::u16string openblack::helptext::ConvertScriptText(std::u16string_view text)
{
	// fn_007191F0 (0x719200..0x71926A)
	constexpr char16_t k_Space = 0xF8FE;
	std::u16string out;
	out.reserve(text.size());
	for (size_t i = 0; i < text.size();)
	{
		const char16_t c = text[i];
		const char16_t next = i + 1 < text.size() ? text[i + 1] : u'\0';
		if ((c == u' ' && next == u'~') || (c == u'~' && next == u' ')) // 0x719200 / 0x71921A
		{
			out.push_back(k_Space);
			i += 2;
		}
		else if (c == u'\\' && next == u'n') // 0x719234
		{
			out.push_back(u'\n');
			i += 2;
		}
		else
		{
			out.push_back(c == u'~' ? k_Space : c); // 0x71924E
			++i;
		}
	}
	return out;
}

std::vector<openblack::helptext::Entry> openblack::helptext::Parse(const std::vector<uint8_t>& utf16)
{
	const auto text = Utf16(utf16);
	std::vector<std::u16string_view> lines;
	for (size_t pos = 0; pos < text.size();)
	{
		const auto end = text.find(u'\n', pos);
		const auto stop = end == std::u16string::npos ? text.size() : end;
		lines.emplace_back(std::u16string_view(text).substr(pos, stop - pos));
		pos = stop + 1;
	}

	// the declarations "HELP_TEXT_NARRATOR_GOOD_SPIRIT =  2"
	std::unordered_map<std::u16string, int32_t> values;
	for (const auto line : lines)
	{
		const auto equals = line.find(u'=');
		if (equals == std::u16string_view::npos || line.find(u"ADD_TEXT") != std::u16string_view::npos)
		{
			continue;
		}
		if (const auto value = Number(line.substr(equals + 1)))
		{
			values[std::u16string(Trim(line.substr(0, equals)))] = *value;
		}
	}
	const auto resolve = [&values](std::u16string_view argument) {
		if (const auto number = Number(argument))
		{
			return *number;
		}
		const auto found = values.find(std::u16string(Trim(argument)));
		return found != values.end() ? found->second : k_NarratorUndeclared;
	};

	std::vector<Entry> entries;
	for (const auto line : lines)
	{
		const auto command = line.find(u"ADD_TEXT");
		if (command == std::u16string_view::npos)
		{
			continue;
		}
		Entry entry;
		// the name is the first quoted string, the text the last one; the numbers come before the first quote
		const auto open = line.find(u'(', command);
		const auto nameOpen = line.find(u'"', command);
		const auto nameClose = nameOpen == std::u16string_view::npos ? nameOpen : line.find(u'"', nameOpen + 1);
		const auto textClose = line.rfind(u'"');
		const auto textOpen = textClose == std::u16string_view::npos || textClose == 0 ? std::u16string_view::npos
		                                                                               : line.rfind(u'"', textClose - 1);
		if (open != std::u16string_view::npos && nameOpen != std::u16string_view::npos && open < nameOpen)
		{
			const auto numbers = line.substr(open + 1, nameOpen - open - 1);
			const auto comma = numbers.find(u',');
			if (comma != std::u16string_view::npos)
			{
				entry.arg0 = resolve(numbers.substr(0, comma));
				const auto second = numbers.substr(comma + 1);
				entry.narrator = resolve(second.substr(0, second.find(u',')));
			}
		}
		if (nameClose != std::u16string_view::npos)
		{
			entry.name = Narrow(ConvertScriptText(line.substr(nameOpen + 1, nameClose - nameOpen - 1)));
		}
		if (textOpen != std::u16string_view::npos && textOpen > nameClose)
		{
			entry.text = ConvertScriptText(line.substr(textOpen + 1, textClose - textOpen - 1));
		}
		entries.push_back(std::move(entry));
	}
	return entries;
}

const openblack::helptext::Entry& openblack::helptext::GetEntry(uint32_t id)
{
	if (!s_loaded)
	{
		Load();
	}
	static const Entry k_Empty;
	if (s_entries.empty())
	{
		return k_Empty;
	}
	return id > 0 && id < s_entries.size() ? s_entries[id] : s_entries[0];
}

const std::u16string& openblack::helptext::Get(uint32_t id)
{
	return GetEntry(id).text;
}

size_t openblack::helptext::Count()
{
	if (!s_loaded)
	{
		Load();
	}
	return s_entries.size();
}

std::u16string openblack::helptext::Format(uint32_t id, double value)
{
	const auto& text = Get(id);
	const auto percent = text.find(u'%');
	if (percent == std::u16string::npos)
	{
		return text;
	}
	// the conversion: flags, width, precision up to its letter
	size_t end = percent + 1;
	while (end < text.size() && std::u16string_view(u"-+ #0123456789.").find(text[end]) != std::u16string_view::npos)
	{
		++end;
	}
	if (end >= text.size())
	{
		return text;
	}
	std::string spec;
	for (size_t i = percent; i <= end; ++i)
	{
		spec.push_back(static_cast<char>(text[i]));
	}
	char buffer[64];
	const char letter = spec.back();
	if (letter == 'd' || letter == 'i')
	{
		std::snprintf(buffer, sizeof(buffer), spec.c_str(), static_cast<int>(value));
	}
	else
	{
		std::snprintf(buffer, sizeof(buffer), spec.c_str(), value);
	}
	std::u16string number;
	for (const char* c = buffer; *c != '\0'; ++c)
	{
		number.push_back(static_cast<char16_t>(static_cast<unsigned char>(*c)));
	}
	return text.substr(0, percent) + number + text.substr(end + 1);
}
