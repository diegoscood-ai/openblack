/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "ECS/Heartbeat.h"
#include "Enums.h"

namespace openblack::ecs::systems
{

/// The players' heartbeats as the temples' columns of light read them. The beat is stepped every turn by the guidance
/// (audio::guidance::ProcessGameTurn), so this service only reads it
class HeartbeatSystemInterface
{
public:
	virtual ~HeartbeatSystemInterface() = default;

	/// A player's pulse a fraction of the way through the turn: 0 between beats, 1 at the top; 0 for a player without
	/// a heartbeat
	[[nodiscard]] virtual float Pulse(PlayerNames player, float turnFraction) const = 0;
	/// A player's heartbeat as it stands, none for a player without one
	[[nodiscard]] virtual std::optional<heartbeat::State> StateOf(PlayerNames player) const = 0;
};

} // namespace openblack::ecs::systems
