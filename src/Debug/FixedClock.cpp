/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FixedClock.h"

#include <atomic>
#include <cstdlib>

#include "GameClock.h"

namespace openblack::fixed_clock
{
namespace
{
uint32_t g_FrameMs = 0;
/// atomic: the music thread reads the ticks too (Audio/LH/MusicStream.cpp through device::TickCount)
std::atomic<uint32_t> g_Ticks {0};

uint32_t FixedTicks()
{
	return g_Ticks.load(std::memory_order_relaxed);
}
} // namespace

void InstallFromEnvironment()
{
	if (const char* ms = std::getenv("OPENBLACK_FIXED_FRAME_MS"); ms != nullptr)
	{
		const long value = std::strtol(ms, nullptr, 10);
		if (value >= 1 && value <= 1000)
		{
			Install(static_cast<uint32_t>(value));
		}
	}
}

void Install(uint32_t frameMs)
{
	g_FrameMs = frameMs;
	g_Ticks.store(0, std::memory_order_relaxed);
	game_clock::SetTickSource(frameMs != 0 ? &FixedTicks : nullptr);
}

bool Enabled()
{
	return g_FrameMs != 0;
}

std::chrono::microseconds AdvanceFrame()
{
	g_Ticks.fetch_add(g_FrameMs, std::memory_order_relaxed);
	return std::chrono::milliseconds(g_FrameMs);
}

} // namespace openblack::fixed_clock
