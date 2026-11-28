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

#include "ECS/DiscipleRules.h"
#include "ECS/Systems/DiscipleGlowClockInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The clock at 0 and no cell chosen when made
class DiscipleGlowClockService final: public DiscipleGlowClockInterface
{
public:
	void Step(uint32_t frameGameMs) override { _clock.Step(frameGameMs); }
	[[nodiscard]] uint32_t GlowCell() override { return _clock.GlowCell(); }
	[[nodiscard]] std::optional<uint32_t> ChosenCell() const override { return _clock.cell; }
	[[nodiscard]] int32_t ClockMs() const override { return _clock.clock; }

private:
	disciple::GlowClock _clock;
};
} // namespace openblack::ecs::systems
