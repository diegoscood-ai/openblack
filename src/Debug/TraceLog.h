/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <spdlog/spdlog.h>

namespace openblack::debug_env
{

/// Whether a trace logs: its switch is on and the logger it writes to exists. A test or a tool can run the game's code
/// without the game's loggers, and a trace switched on there must then log nothing rather than write through a null
/// logger
[[nodiscard]] inline bool TraceLogs(bool switchOn, const char* logger = "game")
{
	return switchOn && spdlog::get(logger) != nullptr;
}

} // namespace openblack::debug_env
