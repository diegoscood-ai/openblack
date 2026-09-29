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
#include <vector>

#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

namespace
{
std::vector<std::u16string> s_texts;
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

void Load()
{
	s_loaded = true;
	try
	{
		auto& fileSystem = openblack::Locator::filesystem::value();
		const auto path = fileSystem.GetPath<openblack::filesystem::Path::Scripts>() / "InfoScript2.txt";
		const auto text = Utf16(fileSystem.ReadAll(fileSystem.FindPath(path)));
		// every ADD_TEXT( ... ) line: the text is the last quoted string
		size_t pos = 0;
		const std::u16string command = u"ADD_TEXT";
		while ((pos = text.find(command, pos)) != std::u16string::npos)
		{
			const auto end = text.find(u'\n', pos);
			const auto line = text.substr(pos, end == std::u16string::npos ? std::u16string::npos : end - pos);
			const auto close = line.rfind(u'"');
			const auto open = close == std::u16string::npos || close == 0 ? std::u16string::npos : line.rfind(u'"', close - 1);
			s_texts.push_back(open == std::u16string::npos ? std::u16string() : line.substr(open + 1, close - open - 1));
			pos = end == std::u16string::npos ? text.size() : end;
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Help texts: {} entries from InfoScript2.txt", s_texts.size());
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Help texts: cannot read InfoScript2.txt: {}", e.what());
	}
}
} // namespace

const std::u16string& openblack::helptext::Get(uint32_t id)
{
	if (!s_loaded)
	{
		Load();
	}
	static const std::u16string k_Empty;
	if (s_texts.empty())
	{
		return k_Empty;
	}
	return id > 0 && id < s_texts.size() ? s_texts[id] : s_texts[0];
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
