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

#include <optional>

namespace openblack::ecs::systems
{
/// The clock the disciples' glows take their one cell from (disciple::GlowClock), kept for the whole process as the
/// game keeps its glows' sprites: a new game in the same process keeps the cell (Locator::discipleGlowClock)
class DiscipleGlowClockInterface
{
public:
	virtual ~DiscipleGlowClockInterface() = default;

	/// Once a frame, at the frame's start, by the frame's game milliseconds
	virtual void Step(uint32_t frameGameMs) = 0;
	/// The cell every glow shows, chosen when it is first asked, as the first glow is drawn
	[[nodiscard]] virtual uint32_t GlowCell() = 0;
	/// The cell chosen, none before the first glow; and the clock, in milliseconds (the debug readout)
	[[nodiscard]] virtual std::optional<uint32_t> ChosenCell() const = 0;
	[[nodiscard]] virtual int32_t ClockMs() const = 0;
};
} // namespace openblack::ecs::systems
