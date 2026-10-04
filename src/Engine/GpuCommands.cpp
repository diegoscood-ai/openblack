/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GpuCommands.h"

namespace openblack::engine::gpu
{

void Submit(std::function<void()> command)
{
	// one thread: at once (the queue for the drawing thread comes with the threads, M3)
	if (command)
	{
		command();
	}
}

void Flush() {}

} // namespace openblack::engine::gpu
