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

#include "Enums.h"

namespace openblack::ecs::components
{

/// The computer mind of a player. A land script switches it on, and its first turn after that starts it on the land.
/// Whether the player is a computer player is its PlayerMagic's player type, which switching the mind on sets.
struct ComputerPlayer
{
	/// The mind plays the player
	bool switchedOn {false};
	/// The mind made its start on the land
	bool started {false};
	/// A script paused the mind. It does not start while it is paused.
	bool paused {false};
	/// The player that a land script named for the size of this player's creature. Any name starts the size change.
	std::optional<PlayerNames> creatureLike;
};

} // namespace openblack::ecs::components
