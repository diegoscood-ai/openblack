/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The includer builds its own locator: it defines LOCATOR_IMPLEMENTATIONS before its includes
#include "ECS/Systems/Implementations/SkyFrameSystem.h"
#include "Locator.h"
#include "support/RestoreService.h"

namespace openblack::test
{
/// Gives a test a sky frame of its own, as the game starts it, and puts the one before back when the test ends, also
/// after a failed ASSERT. What a day-night clock writes there (the sky type's thresholds as its cycle is set, the frame
/// sampled and the dome rebuilt as its time jumps) then does not reach the next test.
class SkyFrameGuard
{
public:
	SkyFrameGuard() { Locator::skyFrameSystem::emplace<ecs::systems::SkyFrameSystem>(); }
	SkyFrameGuard(const SkyFrameGuard&) = delete;
	SkyFrameGuard& operator=(const SkyFrameGuard&) = delete;
	SkyFrameGuard(SkyFrameGuard&&) = delete;
	SkyFrameGuard& operator=(SkyFrameGuard&&) = delete;
	~SkyFrameGuard() = default;

private:
	/// Made before the constructor's body, so it keeps the sky frame the test found
	RestoreService<Locator::skyFrameSystem> _restore;
};
} // namespace openblack::test
