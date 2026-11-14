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

#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include <spdlog/sinks/base_sink.h>

namespace openblack::debug::gui
{

/// Collects the game's log lines for the debug Console, as "[logger] [level] text". It keeps the newest `capacity`
/// lines not yet taken, the oldest dropped first, so a Console left closed holds a bounded log
class ConsoleLogSink final: public spdlog::sinks::base_sink<std::mutex>
{
public:
	static constexpr size_t k_DefaultCapacity = 1000;

	explicit ConsoleLogSink(size_t capacity = k_DefaultCapacity);

	/// The lines logged since the last call, oldest first
	[[nodiscard]] std::vector<std::string> Take();

protected:
	void sink_it_(const spdlog::details::log_msg& message) override;
	void flush_() override {}

private:
	size_t _capacity;
	std::deque<std::string> _lines;
};

} // namespace openblack::debug::gui
