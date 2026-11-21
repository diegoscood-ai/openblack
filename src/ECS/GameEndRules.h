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

#include <vector>

#include "ECS/Components/GameStanding.h"
#include "Enums.h"

// The end of a skirmish. Each turn, after every player has taken its turn:
//
// - A god among the first four players with no temple is out, and its creature goes from the land.
// - Each god that is now out is counted out once: every other human god still in counts one more god defeated, a human
//   god takes the next place in the order of the gods that left, and the god's computer mind stops. When so many gods
//   are out that one at most is left, the game is decided, and nothing of this runs again.
//
// Only the gods that a human or a computer mind plays take part; players five to seven never lose this way. The winning
// conditions and the time limit of a network game, which share this step in the game, are not here: openblack has no
// network game.
//
// Pure, tested with a fake world.

namespace openblack::game_end
{

/// The world that the end of the game looks at and changes
class World
{
public:
	virtual ~World() = default;
	/// The players that a human or a computer mind plays, in the order of the players, without the neutral player
	[[nodiscard]] virtual std::vector<PlayerNames> ActiveGods() const = 0;
	/// A human plays the god
	[[nodiscard]] virtual bool IsHuman(PlayerNames god) const = 0;
	[[nodiscard]] virtual bool HasTemple(PlayerNames god) const = 0;
	[[nodiscard]] virtual ecs::components::GameStanding& Standing(PlayerNames god) = 0;
	/// The creature of the god goes from the land, if the god has one
	virtual void RemoveCreature(PlayerNames god) = 0;
	/// The statistics of the god count one more god defeated
	virtual void CountGodDefeated(PlayerNames god) = 0;
	/// The god takes the next place in the order of the gods that left the game, in its statistics
	virtual void TakeNextPlace(PlayerNames god) = 0;
	/// The computer mind of the god stops
	virtual void StopMind(PlayerNames god) = 0;
};

/// What the end of the game keeps between turns
struct State
{
	/// The game is decided: one god at most is left
	bool decided {false};
};

/// A god counted out in the turn, which the player sees
struct Event
{
	enum class Kind : uint8_t
	{
		/// The god is out and the game goes on
		GodOut,
		/// The god is out and one god at most is left: the game is decided
		GodOutAndDecided,
	};
	Kind kind;
	PlayerNames god;
};

/// Whether a player can lose the game: only the first four players win or lose
[[nodiscard]] constexpr bool CanLose(PlayerNames player)
{
	return static_cast<uint8_t>(player) < 4;
}

/// A god is out: every other human god still in counts one more god defeated (only for a human god that is out, but for
/// any god in a skirmish), a human god takes the next place, and once one god is left the last human god still in takes
/// the place after that
void CountOut(World& world, PlayerNames god, bool skirmish);

/// The end of the game for one turn: nothing outside a skirmish, or once the game is decided
[[nodiscard]] std::vector<Event> ProcessTurn(World& world, bool skirmish, State& state);

} // namespace openblack::game_end
