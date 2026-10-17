/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorshipTrace.h"

#include <cstdlib>

#include "Debug/TraceLog.h"

bool openblack::worship::trace::Enabled()
{
	static const bool enabled = std::getenv("OPENBLACK_WORSHIP_TRACE") != nullptr;
	// its lines go to the game's log and, for the villagers' worship, to the AI's
	return debug_env::TraceLogs(debug_env::TraceLogs(enabled, "game"), "ai");
}
