/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{

/// On a player: where the god stands in a skirmish (game_end). A god that is out has lost: it had no temple. The game
/// counts a god out once, after which the god is finished. Both last until the game ends.
struct GameStanding
{
	/// The god is out of the game
	bool out {false};
	/// The game has counted the god out: its statistics, the boxes it opens and its computer mind stopped
	bool finished {false};
};

} // namespace openblack::ecs::components
