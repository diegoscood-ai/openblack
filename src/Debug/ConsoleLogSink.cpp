/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ConsoleLogSink.h"

#include <iterator>
#include <string_view>
#include <utility>

#include <spdlog/common.h>

using namespace openblack::debug::gui;

ConsoleLogSink::ConsoleLogSink(size_t capacity)
    : _capacity(capacity)
{
}

std::vector<std::string> ConsoleLogSink::Take()
{
	const std::lock_guard lock(mutex_);
	std::vector<std::string> taken(std::make_move_iterator(_lines.begin()), std::make_move_iterator(_lines.end()));
	_lines.clear();
	return taken;
}

void ConsoleLogSink::sink_it_(const spdlog::details::log_msg& message)
{
	if (_capacity == 0)
	{
		return;
	}
	if (_lines.size() == _capacity)
	{
		_lines.pop_front();
	}
	const auto level = spdlog::level::to_string_view(message.level);
	std::string line;
	line.reserve(message.logger_name.size() + level.size() + message.payload.size() + 6);
	line.append("[").append(message.logger_name.data(), message.logger_name.size()).append("] [");
	line.append(level.data(), level.size()).append("] ");
	line.append(message.payload.data(), message.payload.size());
	_lines.push_back(std::move(line));
}
