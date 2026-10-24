/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

// The frame counter in the screen's corner: when it shows. A pure rule, so that it is tested without a game.

namespace openblack::debug::frame_counter
{

/// A run of the checks rather than a person playing: the real input is ignored (OPENBLACK_IGNORE_REAL_INPUT=1, which
/// every check script sets), the game starts on the testbed or a testbed scenario, or a set number of frames is
/// simulated (-n). Such a run never draws the counter, so that its screenshots stay as they were
[[nodiscard]] constexpr bool IsCheckRun(bool ignoreRealInput, bool testbed, uint32_t framesToSimulate) noexcept
{
	return ignoreRealInput || testbed || framesToSimulate != 0;
}

/// The counter shows while its setting is on (the View menu's switch, on by default), and never in a check run
[[nodiscard]] constexpr bool IsVisible(bool setting, bool checkRun) noexcept
{
	return setting && !checkRun;
}

} // namespace openblack::debug::frame_counter
