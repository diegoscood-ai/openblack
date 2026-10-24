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

#include <algorithm>
#include <array>
#include <span>

// The reaction step of a game turn, as pure rules. Every turn the game takes one reaction of its list, walking from the
// newest to the oldest and starting over at the newest past the oldest, and offers it again to the Living in its reach.
// On the way a growing reaction widens, and a time-limited one that has run past its turns is shut down.
// Wiki: docs/wiki/beings/animal-life.md#reactions.

namespace openblack::ecs::effects::reaction_turn
{
/// The reaction kinds that end once they have run longer than their info's turns to react (the type table's flag)
inline constexpr std::array<uint8_t, 20> k_TimeLimitedTypes {1,  15, 16, 17, 18, 19, 21, 22, 23, 24,
                                                             25, 26, 27, 29, 31, 32, 34, 35, 38, 40};

[[nodiscard]] constexpr bool IsTimeLimited(uint8_t type)
{
	return std::ranges::find(k_TimeLimitedTypes, type) != k_TimeLimitedTypes.end();
}

/// The reaction offered this turn, by id (0: none). `idsOldestFirst` is the list in creation order; the walk runs the
/// other way. No cursor yet (0), or one that is no longer listed, starts at the newest; else the one made before it,
/// and past the oldest the newest again. So a reaction made while the walk is under way is reached after the wrap
[[nodiscard]] constexpr uint32_t NextCursor(std::span<const uint32_t> idsOldestFirst, uint32_t cursor)
{
	if (idsOldestFirst.empty())
	{
		return 0;
	}
	const auto found = cursor == 0 ? idsOldestFirst.end() : std::ranges::find(idsOldestFirst, cursor);
	if (found == idsOldestFirst.end() || found == idsOldestFirst.begin())
	{
		return idsOldestFirst.back();
	}
	return *(found - 1);
}

/// The cursor once the reactions `removed` leave the list `idsOldestFirst` (the list before they leave). A cursor that
/// is not removed stays. A removed one moves to the nearest older reaction that stays, or to 0 when there is none, so
/// the next turn goes one further. The order the reactions leave in does not change the result
[[nodiscard]] constexpr uint32_t CursorAfterRemoval(std::span<const uint32_t> idsOldestFirst, std::span<const uint32_t> removed,
                                                    uint32_t cursor)
{
	const auto isRemoved = [removed](uint32_t id) { return std::ranges::find(removed, id) != removed.end(); };
	if (cursor == 0 || !isRemoved(cursor))
	{
		return cursor;
	}
	const auto found = std::ranges::find(idsOldestFirst, cursor);
	if (found == idsOldestFirst.end())
	{
		return cursor;
	}
	for (auto older = found; older != idsOldestFirst.begin();)
	{
		--older;
		if (!isRemoved(*older))
		{
			return *older;
		}
	}
	return 0;
}

/// What the step reads of the reaction it takes
struct StepFacts
{
	/// Its initiator is still there and not on its way out
	bool initiatorAvailable {true};
	/// The info's whetherReactionGrows, and how much it grows each time it is taken
	bool grows {false};
	float growth {0.0f};
	/// Its kind is one of k_TimeLimitedTypes
	bool timeLimited {false};
	/// The turn it started (0: not started yet) and the game turn
	uint32_t turnStarted {0};
	uint32_t turn {0};
	/// The info's numGameTurnsForNormalThingsToReact
	uint32_t turnsToReact {0};
	/// Still offered (off while its initiator is in the hand, for the kinds that stop there)
	bool active {true};
	float radius {0.0f};
};

enum class StepAction
{
	/// Every reaction of its initiator goes (nothing else is done)
	RemoveAllOfInitiator,
	/// It is shut down and leaves the list
	ShutDown,
	/// It is offered again over its reach
	Spread,
	/// Inactive: it stays, not offered
	Nothing,
};

struct Step
{
	StepAction action {StepAction::Nothing};
	/// The radius it keeps (grown, for a growing kind)
	float radius {0.0f};
};

/// The step for the reaction taken this turn, in the original's order: an initiator that is gone takes all its
/// reactions away; then a growing reaction widens; then a time-limited one past its turns (counted in unsigned turns,
/// only once it has started) is shut down; else an active one is spread
[[nodiscard]] constexpr Step Decide(const StepFacts& facts)
{
	if (!facts.initiatorAvailable)
	{
		return {.action = StepAction::RemoveAllOfInitiator, .radius = facts.radius};
	}
	float radius = facts.radius;
	if (facts.grows)
	{
		radius = radius + facts.growth;
	}
	if (facts.timeLimited && facts.turnStarted > 0 && facts.turn - facts.turnStarted > facts.turnsToReact)
	{
		return {.action = StepAction::ShutDown, .radius = radius};
	}
	return {.action = facts.active ? StepAction::Spread : StepAction::Nothing, .radius = radius};
}
} // namespace openblack::ecs::effects::reaction_turn
