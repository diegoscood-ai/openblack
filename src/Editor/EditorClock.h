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

#include "GameClock.h"
#include "Locator.h"

/// The few calls the editor makes on the game's clock: the turn, pausing for its play, pause and step buttons, and the
/// speed for its slider. The
/// editor goes through these alone, so that moving it to another clock service changes this file only.
namespace openblack::editor::clock
{

[[nodiscard]] inline bool Available()
{
	return Locator::time::has_value();
}

[[nodiscard]] inline uint32_t Turn()
{
	return Locator::time::value().GetTurn();
}

[[nodiscard]] inline bool IsPaused()
{
	return Locator::time::value().IsPaused();
}

inline void SetPaused(bool paused)
{
	Locator::time::value().SetPaused(paused);
}

/// The turn length multiplier: 2 is half speed, 0.5 double. The clock keeps its inverse, the speed-up factor
[[nodiscard]] inline float TurnMultiplier()
{
	return 1.0f / Locator::time::value().GetSpeed();
}

inline void SetTurnMultiplier(float multiplier)
{
	Locator::time::value().SetSpeed(1.0f / multiplier);
}

} // namespace openblack::editor::clock
