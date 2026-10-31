/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/HeartbeatSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// Ours behind his interface: the heartbeat is the guidance's (audio::guidance), the local interface's, which is
/// openblack's only one. The local player has it; every other player has none
class HeartbeatSystem final: public HeartbeatSystemInterface
{
public:
	[[nodiscard]] float Pulse(PlayerNames player, float turnFraction) const override;
	[[nodiscard]] std::optional<heartbeat::State> StateOf(PlayerNames player) const override;
};

} // namespace openblack::ecs::systems
