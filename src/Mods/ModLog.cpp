/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ModLog.h"

#include <spdlog/spdlog.h>

namespace openblack::mods::log
{
namespace
{
constexpr size_t k_MaxEntries = 2000;

std::deque<Entry>& Buffer()
{
	static std::deque<Entry> entries;
	return entries;
}
} // namespace

void Write(Level level, std::string_view mod, std::string_view text)
{
	auto& entries = Buffer();
	entries.push_back({level, std::string(mod), std::string(text)});
	while (entries.size() > k_MaxEntries)
	{
		entries.pop_front();
	}
	if (auto logger = spdlog::get("game"))
	{
		const auto spdLevel = level == Level::Error     ? spdlog::level::err
		                      : level == Level::Warning ? spdlog::level::warn
		                                                : spdlog::level::info;
		if (mod.empty())
		{
			logger->log(spdLevel, "Mods: {}", text);
		}
		else
		{
			logger->log(spdLevel, "Mods: {}: {}", mod, text);
		}
	}
}

const std::deque<Entry>& Entries()
{
	return Buffer();
}

void Clear()
{
	Buffer().clear();
}

} // namespace openblack::mods::log
