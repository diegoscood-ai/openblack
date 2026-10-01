/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SoundAction.h"

#include <cctype>
#include <cstdlib>

#include <map>
#include <mutex>

#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

using namespace openblack::psys;

namespace
{
struct Names
{
	std::map<std::string, int32_t, std::less<>> byName;
	std::map<int32_t, std::string> byValue;
};

const Names& Loaded()
{
	static Names names;
	static std::once_flag once;
	std::call_once(once, [] {
		if (!openblack::Locator::filesystem::has_value())
		{
			return;
		}
		auto& fileSystem = openblack::Locator::filesystem::value();
		try
		{
			const auto bytes = fileSystem.ReadAll(fileSystem.GetPath<openblack::filesystem::Path::Data>() / "SoundAction.h");
			const std::string text(bytes.begin(), bytes.end());
			for (auto& [name, value] : ParseEnumHeader(text))
			{
				names.byValue.try_emplace(value, name);
				names.byName.insert_or_assign(std::move(name), value);
			}
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: cannot read Data\\SoundAction.h: {}", e.what());
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "PSys: {} sound actions", names.byName.size());
	});
	return names;
}
} // namespace

std::vector<std::pair<std::string, int32_t>> openblack::psys::ParseEnumHeader(std::string_view text)
{
	// strip the comments, then read the body of the first enum
	std::string code;
	code.reserve(text.size());
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '/')
		{
			while (i < text.size() && text[i] != '\n')
			{
				++i;
			}
		}
		else if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '*')
		{
			const auto end = text.find("*/", i + 2);
			i = end == std::string_view::npos ? text.size() : end + 1;
			continue;
		}
		if (i < text.size())
		{
			code.push_back(text[i]);
		}
	}
	std::vector<std::pair<std::string, int32_t>> result;
	const auto keyword = code.find("enum");
	const auto open = keyword == std::string::npos ? std::string::npos : code.find('{', keyword);
	const auto close = open == std::string::npos ? std::string::npos : code.find('}', open);
	if (close == std::string::npos)
	{
		return result;
	}
	const std::string_view body(code.data() + open + 1, close - open - 1);
	int32_t next = 0;
	size_t start = 0;
	while (start <= body.size())
	{
		auto end = body.find(',', start);
		if (end == std::string_view::npos)
		{
			end = body.size();
		}
		std::string_view entry = body.substr(start, end - start);
		const auto trim = [](std::string_view s) {
			while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0)
			{
				s.remove_prefix(1);
			}
			while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())) != 0)
			{
				s.remove_suffix(1);
			}
			return s;
		};
		std::string_view name = trim(entry);
		if (const auto equals = name.find('='); equals != std::string_view::npos)
		{
			const std::string number(trim(name.substr(equals + 1)));
			next = static_cast<int32_t>(std::strtol(number.c_str(), nullptr, 0));
			name = trim(name.substr(0, equals));
		}
		if (!name.empty())
		{
			result.emplace_back(std::string(name), next++);
		}
		start = end + 1;
	}
	return result;
}

int32_t openblack::psys::SoundActionValue(std::string_view name)
{
	const auto& names = Loaded().byName;
	const auto it = names.find(name);
	return it == names.end() ? -1 : it->second;
}

std::string openblack::psys::SoundActionName(int32_t value)
{
	const auto& names = Loaded().byValue;
	const auto it = names.find(value);
	return it == names.end() ? std::string("?") : it->second;
}

SoundAction openblack::psys::ReadSoundAction(const Object& object, std::string_view key)
{
	SoundAction result;
	const auto it = object.properties.find(key);
	if (it == object.properties.end() || it->second.type != Value::Type::Sound)
	{
		return result;
	}
	const auto& value = it->second;
	const auto flag = [&value](size_t i) { return i < value.array.size() && value.array[i] != 0; };
	// LOOPING bit 0, (ONLYONE only read), SOFTRELEASE bit 2, USESURFACE bit 3; the other bits are kept
	result.flags = static_cast<uint8_t>((flag(0) ? SoundAction::Looping : 0) | (flag(2) ? SoundAction::SoftRelease : 0) |
	                                    (flag(3) ? SoundAction::UseSurface : 0));
	result.action = value.text == "NO_SOUND" ? -1 : SoundActionValue(value.text);
	return result;
}
