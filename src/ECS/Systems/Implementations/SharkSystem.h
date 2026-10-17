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

#include "ECS/Systems/SharkSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The sharks (class Whale; docs/wiki/world/sea-life-and-scenery.md#sharks), made by the CHL CREATE of type Whale
/// (archetypes::SharkArchetype), moved by the script's WALK_PATH (the walk path system), drawn in two parts cut by the
/// water (Renderer::DrawCutBelowWater / DrawCutAboveWater), with a wake of rings
class SharkSystem final: public SharkSystemInterface
{
public:
	/// Every game turn, before the walk path system moves them: each shark's turn starts where it is
	void ProcessTurn() override;
	/// The frame part of their draw, after the animations: the heading of the turn's move, the drawn position between
	/// the turn's start and end, and the wake (a ring every 50 ms of the one timer, at the first EBone point)
	void Update(std::chrono::duration<float, std::milli> gameTime, float turnFraction) override;
	[[nodiscard]] int32_t GetWakeTimer() const override { return _wakeTimer; }

private:
	/// One timer for all the sharks: each adds the frame's whole milliseconds to it, so two sharks share the rings
	int32_t _wakeTimer {0};
};

} // namespace openblack::ecs::systems
