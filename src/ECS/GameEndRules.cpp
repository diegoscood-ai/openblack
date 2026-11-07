/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameEndRules.h"

#include <algorithm>
#include <optional>

namespace openblack::game_end
{

namespace
{
/// The number of gods among the first four players that take part
[[nodiscard]] uint32_t GodsThatCanLose(const std::vector<PlayerNames>& gods)
{
	return static_cast<uint32_t>(std::ranges::count_if(gods, [](PlayerNames god) { return CanLose(god); }));
}

/// The number of gods among the first four players that are out
[[nodiscard]] uint32_t GodsOut(World& world, const std::vector<PlayerNames>& gods)
{
	return static_cast<uint32_t>(
	    std::ranges::count_if(gods, [&world](PlayerNames god) { return CanLose(god) && world.Standing(god).out; }));
}

/// Whether so many gods are out that one god at most is left. The game compares the counts as numbers with no sign, so
/// a game with no god that can lose counts as decided too
[[nodiscard]] bool OneGodLeft(World& world, const std::vector<PlayerNames>& gods)
{
	return GodsOut(world, gods) >= GodsThatCanLose(gods) - 1u;
}
} // namespace

void CountOut(World& world, PlayerNames god, bool skirmish)
{
	const auto gods = world.ActiveGods();
	// Every other human god that is still in saw a god defeated. A god that a computer mind plays makes the human gods
	// count only in a skirmish.
	const bool counts = world.IsHuman(god) || skirmish;
	std::optional<PlayerNames> lastHumanIn;
	for (const auto other : gods)
	{
		if (other == god || world.Standing(other).out || !world.IsHuman(other))
		{
			continue;
		}
		lastHumanIn = other;
		if (counts)
		{
			world.CountGodDefeated(other);
		}
	}
	// A human god takes the next place
	if (world.IsHuman(god))
	{
		world.TakeNextPlace(god);
	}
	// When exactly one god is left, the last human god that is still in takes the place after that
	if (GodsOut(world, gods) == GodsThatCanLose(gods) - 1u && lastHumanIn.has_value())
	{
		world.TakeNextPlace(*lastHumanIn);
	}
}

std::vector<Event> ProcessTurn(World& world, bool skirmish, State& state)
{
	std::vector<Event> events;
	if (!skirmish || state.decided)
	{
		return events;
	}
	const auto gods = world.ActiveGods();
	// A god among the first four players with no temple is out, and its creature goes from the land
	for (const auto god : gods)
	{
		if (!CanLose(god) || world.HasTemple(god))
		{
			continue;
		}
		auto& standing = world.Standing(god);
		if (standing.out)
		{
			continue;
		}
		standing.out = true;
		world.RemoveCreature(god);
	}
	// The game counts each god that is now out. When one god at most is left, the game is decided.
	for (const auto god : gods)
	{
		if (!world.Standing(god).out || world.Standing(god).finished)
		{
			continue;
		}
		CountOut(world, god, skirmish);
		if (OneGodLeft(world, gods))
		{
			state.decided = true;
			events.push_back({.kind = Event::Kind::GodOutAndDecided, .god = god});
		}
		else
		{
			events.push_back({.kind = Event::Kind::GodOut, .god = god});
		}
		world.Standing(god).finished = true;
		world.StopMind(god);
	}
	return events;
}

} // namespace openblack::game_end
