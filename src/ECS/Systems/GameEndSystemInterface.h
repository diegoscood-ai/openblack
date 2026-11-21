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
#include <vector>

#include "ECS/GameEndRules.h"

namespace openblack::ecs::systems
{

/// The end of a skirmish (game_end): after every player's turn, a god with no temple is out and its creature goes, and
/// once one god at most is left the game is decided. The boxes that the player sees follow the events of the turns.
class GameEndSystemInterface
{
public:
	virtual ~GameEndSystemInterface() = default;

	/// After every player has taken its turn: the gods with no temple are out, and those now out are counted out
	virtual void ProcessTurn() = 0;
	/// The game is decided: one god at most is left
	[[nodiscard]] virtual bool IsDecided() const = 0;
	/// The gods counted out since last asked, in the order they were counted
	[[nodiscard]] virtual std::vector<game_end::Event> TakeEvents() = 0;
	/// The first god of the game that is not out, the winner once the game is decided; none when every god is out
	[[nodiscard]] virtual std::optional<PlayerNames> LastGodIn() = 0;
	/// A land loads: nothing is decided, and no event waits
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
