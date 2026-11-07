/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GpuCommands.h"

#include <cstdlib>

#include <string>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Common/ProcessLogOnce.h"

namespace openblack::engine::gpu
{

void Submit(std::function<void()> command)
{
	// one thread: at once (the queue for the drawing thread comes with the threads)
	if (command)
	{
		const ScopedPhase draw(Phase::Draw);
		command();
	}
}

void Flush() {}

namespace
{
// The phase of the calling thread, set by ScopedPhase around a part of its frame: each thread has its own, which no
// service of the locator can hold, as a slot is the same for every thread
thread_local Phase g_Phase = Phase::Logic;

bool Enabled()
{
	static const bool s_Enabled = [] {
		const char* value = std::getenv("OPENBLACK_GPU_CALLS");
		return value != nullptr && value[0] == '1';
	}();
	return s_Enabled;
}
} // namespace

ScopedPhase::ScopedPhase(Phase phase)
    : _previous(g_Phase)
{
	g_Phase = phase;
}

ScopedPhase::~ScopedPhase()
{
	g_Phase = _previous;
}

void NoteResourceCall(std::string_view what, std::string_view name)
{
	if (g_Phase != Phase::Logic || !Enabled())
	{
		return;
	}
	if (log_once::First(fmt::format("graphics/gpu-call-in-logic/{}/{}", what, name)))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "GPU call in the logic phase: {} '{}'", what, name);
	}
}

} // namespace openblack::engine::gpu
