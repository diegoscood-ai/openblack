/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>

#include <array>
#include <limits>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureLook.h"
#include "Creature/CreatureObjectBeliefs.h"

using namespace openblack;
using namespace openblack::creature_object_beliefs;

namespace
{
constexpr uint32_t k_Forest = 1;
constexpr uint32_t k_Temple = 2;
constexpr uint32_t k_Flock = 22;

entt::entity Thing(uint32_t n)
{
	return static_cast<entt::entity>(n);
}
} // namespace

TEST(CreatureObjectBeliefs, ItKeepsOneBeliefAboutEachThing)
{
	Store store;
	const auto first = store.Add(Thing(1), k_Forest);
	ASSERT_NE(first.belief, nullptr);
	EXPECT_TRUE(first.isNew);
	const auto again = store.Add(Thing(1), k_Forest);
	EXPECT_FALSE(again.isNew);
	EXPECT_EQ(store.Size(), 1u);
	ASSERT_NE(store.Find(Thing(1)), nullptr);
	EXPECT_EQ(store.Find(Thing(1))->kind, k_Forest);
	EXPECT_EQ(store.Find(Thing(2)), nullptr);
}

TEST(CreatureObjectBeliefs, ItCountsTheBeliefsOfEachKind)
{
	Store store;
	store.Add(Thing(1), k_Temple);
	store.Add(Thing(2), k_Flock);
	store.Add(Thing(3), k_Flock);
	store.Add(Thing(4), k_Forest);
	store.Add(Thing(5), k_Forest);
	store.Add(Thing(6), k_Forest);
	EXPECT_EQ(store.Count(k_Temple), 1u);
	EXPECT_EQ(store.Count(k_Flock), 2u);
	EXPECT_EQ(store.Count(k_Forest), 3u);
	EXPECT_EQ(store.Count(0), 0u);
	// The game has no kinds beyond its 24
	EXPECT_EQ(store.Add(Thing(7), k_KindCount).belief, nullptr);
	EXPECT_EQ(store.Count(k_KindCount), 0u);
}

TEST(CreatureObjectBeliefs, ItKeepsTwentyTemplesFortyFlocksAndFiftyForests)
{
	EXPECT_EQ(k_Capacity[k_Temple], 20u);
	EXPECT_EQ(k_Capacity[k_Flock], 40u);
	EXPECT_EQ(k_Capacity[k_Forest], 50u);
	Store store;
	for (uint32_t i = 0; i < 60; ++i)
	{
		store.Add(Thing(i), k_Forest);
	}
	EXPECT_EQ(store.Count(k_Forest), 50u);
}

TEST(CreatureObjectBeliefs, OnceFullEachNewBeliefReplacesTheOnePutAtTheOldestPlace)
{
	Store store;
	for (uint32_t i = 0; i < 20; ++i)
	{
		store.Add(Thing(i), k_Temple);
	}
	// The first temple's place is the oldest kept: the next temple takes it, and the one after takes it again
	EXPECT_TRUE(store.Add(Thing(100), k_Temple).isNew);
	EXPECT_EQ(store.Find(Thing(0)), nullptr);
	EXPECT_NE(store.Find(Thing(100)), nullptr);
	store.Add(Thing(101), k_Temple);
	EXPECT_EQ(store.Find(Thing(100)), nullptr);
	EXPECT_NE(store.Find(Thing(1)), nullptr);
	EXPECT_EQ(store.Count(k_Temple), 20u);
}

TEST(CreatureObjectBeliefs, ForgettingTheBeliefAtTheOldestPlaceMovesTheMarkerOn)
{
	Store store;
	for (uint32_t i = 0; i < 20; ++i)
	{
		store.Add(Thing(i), k_Temple);
	}
	EXPECT_TRUE(store.Forget(Thing(0)));
	EXPECT_FALSE(store.Forget(Thing(0)));
	// A free place is taken first, then the marker's, which has moved on to the second temple's
	store.Add(Thing(100), k_Temple);
	EXPECT_NE(store.Find(Thing(1)), nullptr);
	store.Add(Thing(101), k_Temple);
	EXPECT_EQ(store.Find(Thing(1)), nullptr);
	EXPECT_NE(store.Find(Thing(100)), nullptr);
	EXPECT_NE(store.Find(Thing(2)), nullptr);
}

TEST(CreatureObjectBeliefs, TheMarkerGoesRoundFromTheLastPlaceToTheFirst)
{
	Store store;
	// Things 0 to 19 in places 0 to 19; forgetting 0 to 18 one by one moves the marker to the last place
	for (uint32_t i = 0; i < 20; ++i)
	{
		store.Add(Thing(i), k_Temple);
	}
	for (uint32_t i = 0; i < 19; ++i)
	{
		store.Forget(Thing(i));
	}
	// Forgetting the last place's belief with others still held takes the marker round to the first place
	for (uint32_t i = 0; i < 19; ++i)
	{
		store.Add(Thing(100 + i), k_Temple);
	}
	store.Forget(Thing(19));
	store.Add(Thing(200), k_Temple);
	store.Add(Thing(201), k_Temple);
	EXPECT_EQ(store.Find(Thing(100)), nullptr);
	EXPECT_NE(store.Find(Thing(200)), nullptr);
	EXPECT_NE(store.Find(Thing(101)), nullptr);
}

TEST(CreatureObjectBeliefs, ItForgetsBeliefsAboutThingsThatAreGone)
{
	Store store;
	store.Add(Thing(1), k_Forest);
	store.Add(Thing(2), k_Flock);
	store.Add(Thing(3), k_Forest);
	store.ForgetIf([](entt::entity thing) { return thing != Thing(2); });
	EXPECT_EQ(store.Size(), 1u);
	EXPECT_EQ(store.Count(k_Forest), 0u);
	EXPECT_EQ(store.Count(k_Flock), 1u);
	// With nothing of a kind held, the first place taken next is the oldest
	store.Add(Thing(4), k_Forest);
	EXPECT_EQ(store.Size(), 2u);
	EXPECT_EQ(store.Count(k_Forest), 1u);
}

TEST(CreatureObjectBeliefs, ItsEyesSweepASquareThatGrowsWithItsSize)
{
	// The odd number below the root of sixty times its size and a hundred and sixty, squared
	EXPECT_EQ(creature_look::SweepCells(1.0f), 15u * 15u);
	EXPECT_EQ(creature_look::SweepCells(0.22f), 13u * 13u);
	EXPECT_EQ(creature_look::SweepCells(4.0f), 21u * 21u);
	EXPECT_EQ(creature_look::SweepCells(0.0f), 13u * 13u);
}

TEST(CreatureObjectBeliefs, EachRingGoesRoundFromTheCornerAtTheLeastXAndZ)
{
	const glm::ivec2 centre {10, 10};
	EXPECT_EQ(RingCell(centre, 1, 0), centre);
	const std::vector<glm::ivec2> ring2 {{9, 9}, {10, 9}, {11, 9}, {11, 10}, {11, 11}, {10, 11}, {9, 11}, {9, 10}};
	for (uint32_t step = 0; step < 8; ++step)
	{
		EXPECT_EQ(RingCell(centre, 2, step), ring2[step]) << step;
	}
	EXPECT_EQ(RingCell(centre, 3, 0), glm::ivec2(8, 8));
	EXPECT_EQ(RingCell(centre, 3, 4), glm::ivec2(12, 8));
	EXPECT_EQ(RingCell(centre, 3, 15), glm::ivec2(8, 9));
}

TEST(CreatureObjectBeliefs, ASweepLooksAtEveryCellOnceANinthAtATimeThenStartsAgain)
{
	Sweep sweep;
	const glm::ivec2 centre {100, 100};
	std::set<std::pair<int, int>> seen;
	size_t looked = 0;
	for (int turn = 0; turn < 25; ++turn)
	{
		const auto cells = NextCells(sweep, centre, 1.0f);
		ASSERT_EQ(cells.size(), 9u) << turn;
		if (turn == 0)
		{
			EXPECT_EQ(cells[0], centre);
			EXPECT_EQ(cells[1], glm::ivec2(99, 99));
		}
		for (const auto cell : cells)
		{
			EXPECT_LE(std::abs(cell.x - centre.x), 7);
			EXPECT_LE(std::abs(cell.y - centre.y), 7);
			seen.insert({cell.x, cell.y});
		}
		looked += cells.size();
	}
	EXPECT_EQ(looked, 225u);
	EXPECT_EQ(seen.size(), 225u);
	EXPECT_EQ(sweep.left, 0u);
	// Then it starts again from its own cell
	EXPECT_EQ(NextCells(sweep, centre, 1.0f).front(), centre);
}

TEST(CreatureObjectBeliefs, TheLastTurnOfASweepLooksAtWhatIsLeft)
{
	// 169 cells, nine a turn: eighteen turns of nine and one of seven
	Sweep sweep;
	for (int turn = 0; turn < 18; ++turn)
	{
		EXPECT_EQ(NextCells(sweep, {50, 50}, 0.0f).size(), 9u);
	}
	EXPECT_EQ(NextCells(sweep, {50, 50}, 0.0f).size(), 7u);
	EXPECT_EQ(NextCells(sweep, {50, 50}, 0.0f).size(), 9u);
}

// When it last looked another creature over is kept with its belief, through coming across it again, and starts afresh
// for a new belief
TEST(CreatureObjectBeliefs, ItKeepsWhenItLastLookedACreatureOver)
{
	constexpr uint32_t k_Creature = 8;
	Store store;
	EXPECT_FALSE(store.Add(Thing(1), k_Creature).belief->inspected.has_value());
	store.Find(Thing(1))->inspected = 600;
	store.Add(Thing(1), k_Creature);
	EXPECT_EQ(store.Find(Thing(1))->inspected, 600u);
	store.Forget(Thing(1));
	EXPECT_FALSE(store.Add(Thing(1), k_Creature).belief->inspected.has_value());
}

// Seeing a thing again takes the game's common steps for every kind of belief but an abode's
TEST(CreatureObjectBeliefs, EveryBeliefButAnAbodesIsStirredBySeeingItsThingAgain)
{
	constexpr uint32_t k_Town = 0;
	constexpr uint32_t k_Abode = 3;
	constexpr uint32_t k_Villager = 6;
	EXPECT_TRUE(SeeingAgainStirs(k_Town));
	EXPECT_TRUE(SeeingAgainStirs(k_Villager));
	EXPECT_FALSE(SeeingAgainStirs(k_Abode));
}

// The whole things a creature may act on are the towns, the forests, the temples, the creatures and the flocks
TEST(CreatureObjectBeliefs, TheWholeThingsAreTownsForestsTemplesCreaturesAndFlocks)
{
	for (uint32_t kind = 0; kind < k_KindCount; ++kind)
	{
		const bool whole = kind == 0 || kind == k_Forest || kind == k_Temple || kind == 8 || kind == k_Flock;
		EXPECT_EQ(InListOfWholeThings(kind), whole) << kind;
	}
}

// A whole thing seen again stirs the sources only more than 200 turns after it last did, never the creature itself,
// and never a thing that is not whole; the turns are counted on whole numbers that may wrap, as the game counts them
TEST(CreatureObjectBeliefs, AWholeThingSeenAgainStirsItsSourcesOnlyPastTwoHundredTurns)
{
	EXPECT_FALSE(SightingStirsSources(k_Forest, false, 200, 0));
	EXPECT_TRUE(SightingStirsSources(k_Forest, false, 201, 0));
	EXPECT_FALSE(SightingStirsSources(k_Forest, false, 700, 500));
	EXPECT_TRUE(SightingStirsSources(k_Forest, false, 701, 500));
	EXPECT_FALSE(SightingStirsSources(k_Forest, true, 701, 500));
	constexpr uint32_t k_Villager = 6;
	EXPECT_FALSE(SightingStirsSources(k_Villager, false, 701, 500));
	EXPECT_TRUE(SightingStirsSources(k_Temple, false, 100, 4000000000u));
}

// Only a thing more than a tenth useful to a source's desire stirs the source
TEST(CreatureObjectBeliefs, OnlyAThingMoreThanATenthUsefulStirsASource)
{
	EXPECT_FALSE(UsefulEnoughToStir(0.0f));
	EXPECT_FALSE(UsefulEnoughToStir(0.1f));
	EXPECT_TRUE(UsefulEnoughToStir(0.11f));
	EXPECT_EQ(k_SightingSources, (std::array<uint32_t, 3> {1, 3, 7}));
	EXPECT_EQ(k_ToySightingSource, 12u);
	EXPECT_EQ(k_SightingSourceAmount, 0.5f);
	EXPECT_EQ(k_ToySightingAmount, 0.5f);
}

// A belief keeps the turn its thing last stirred the sources, through coming across it again; a new one starts at 0
TEST(CreatureObjectBeliefs, ItKeepsTheTurnItsThingLastStirredTheSources)
{
	Store store;
	EXPECT_EQ(store.Add(Thing(2), k_Forest).belief->sourcesTurn, 0u);
	store.Find(Thing(2))->sourcesTurn = 450;
	EXPECT_EQ(store.Add(Thing(2), k_Forest).belief->sourcesTurn, 450u);
	store.Forget(Thing(2));
	EXPECT_EQ(store.Add(Thing(2), k_Forest).belief->sourcesTurn, 0u);
}

// A loose thing of the mobile objects' class is believed in by its row's kind, as the original reads it: magic food's
// kind (4) as magic food (14), a lump of poo's (5) as poo (13), every other kind as a mobile object (19)
TEST(CreatureObjectBeliefs, ALooseThingIsBelievedInByItsRowsKind)
{
	EXPECT_EQ(MobileObjectBeliefKind(4), 14u);
	EXPECT_EQ(MobileObjectBeliefKind(5), 13u);
	EXPECT_EQ(MobileObjectBeliefKind(0), 19u);
	EXPECT_EQ(MobileObjectBeliefKind(8), 19u);
	EXPECT_EQ(MobileObjectBeliefKind(25), 19u);
	EXPECT_EQ(MobileObjectBeliefKind(-1), 19u);
	EXPECT_EQ(MobileObjectBeliefKind(std::nullopt), 19u);
}

// A static that can be moved is believed in as a rock (12) when it is of the rock class, else as a mobile object (19)
TEST(CreatureObjectBeliefs, AMovableStaticIsARockOnlyOfTheRockClass)
{
	EXPECT_EQ(MobileStaticBeliefKind(true), 12u);
	EXPECT_EQ(MobileStaticBeliefKind(false), 19u);
}

// A thing that moves, not the creature itself, is kept as the last moving thing seen while the record counts no more
// than a half (as the game compares it, a record that compares with nothing keeps it too); it is then looked at for
// fewer turns than two seconds' whole turns, counted on whole numbers that may wrap
TEST(CreatureObjectBeliefs, TheLastMovingThingSeenIsKeptAndLookedAtForTwoSeconds)
{
	EXPECT_EQ(k_MovingSeenWeight, 0.5f);
	EXPECT_TRUE(KeepsMovingThing(true, false, 0.0f));
	EXPECT_TRUE(KeepsMovingThing(true, false, 0.5f));
	EXPECT_FALSE(KeepsMovingThing(true, false, 0.51f));
	EXPECT_FALSE(KeepsMovingThing(true, false, 1.0f));
	EXPECT_TRUE(KeepsMovingThing(true, false, std::numeric_limits<float>::quiet_NaN()));
	EXPECT_FALSE(KeepsMovingThing(false, false, 0.0f));
	EXPECT_FALSE(KeepsMovingThing(true, true, 0.0f));
	const MovingSeen start {};
	EXPECT_EQ(start.turn, 0u);
	EXPECT_EQ(start.weight, 0.0f);
	constexpr uint32_t k_TurnsPerSecond = 10;
	EXPECT_TRUE(MovingSeenRecently(500, 500, k_TurnsPerSecond));
	EXPECT_TRUE(MovingSeenRecently(519, 500, k_TurnsPerSecond));
	EXPECT_FALSE(MovingSeenRecently(520, 500, k_TurnsPerSecond));
	EXPECT_FALSE(MovingSeenRecently(499, 500, k_TurnsPerSecond));
	EXPECT_TRUE(MovingSeenRecently(5, 4294967290u, k_TurnsPerSecond));
}
