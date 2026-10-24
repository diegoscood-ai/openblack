/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Game/GameStats.h"

// GameStats: the data of the in-game menu's Statistics page.

using namespace openblack::game_stats;

TEST(GameStats, HistoryAveragesAndHalves)
{
	History h;
	for (int i = 0; i < 50; ++i)
	{
		h.Add(10.0f); // 50 samples of 10 / 50
	}
	EXPECT_EQ(h.count, 1u);
	EXPECT_NEAR(h.buckets.at(0), 10.0f, 1e-4f);
	// fill the rest: at 500 buckets they are averaged in pairs and the period doubles
	for (uint32_t b = 1; b < History::k_Capacity; ++b)
	{
		for (int i = 0; i < 50; ++i)
		{
			h.Add(static_cast<float>(b % 2));
		}
	}
	EXPECT_EQ(h.count, 250u);
	EXPECT_EQ(h.period, 100u);
	EXPECT_EQ(h.samples, 0u);
	EXPECT_NEAR(h.buckets.at(0), (10.0f + 1.0f) * 0.5f, 1e-4f); // buckets 0 and 1
	EXPECT_NEAR(h.buckets.at(1), 0.5f, 1e-4f);
}

TEST(GameStats, WhenFullThePairsAreAveragedAndTheBytesClearedLeaveTheLastBuckets)
{
	// As the game clears them, the buckets just after the middle are emptied but the last ones keep their old values
	History h;
	for (uint32_t bucket = 0; bucket < History::k_Capacity; ++bucket)
	{
		for (int i = 0; i < 50; ++i)
		{
			h.Add(static_cast<float>(bucket % 2 == 0 ? 2 : 4));
		}
	}
	EXPECT_EQ(h.count, 250u);
	EXPECT_EQ(h.period, 100u);
	EXPECT_NEAR(h.buckets.at(0), 3.0f, 1e-4f);
	EXPECT_NEAR(h.buckets.at(249), 3.0f, 1e-4f);
	EXPECT_EQ(h.buckets.at(250), 0.0f);
	EXPECT_EQ(h.buckets.at(311), 0.0f);
	EXPECT_NEAR(h.buckets.at(313), 4.0f, 1e-4f);
	EXPECT_NEAR(h.buckets.at(499), 4.0f, 1e-4f);
	// The new samples go on top of what was left
	for (int i = 0; i < 100; ++i)
	{
		h.Add(1.0f);
	}
	EXPECT_NEAR(h.buckets.at(250), 1.0f, 1e-4f);
}

TEST(GameStats, TheBoxDrawsOnlyTheFinishedPoints)
{
	// The box takes the points before the one being filled: a point still being averaged is left out
	History h;
	EXPECT_TRUE(h.Finished().empty());
	for (int i = 0; i < 50; ++i)
	{
		h.Add(10.0f);
	}
	for (int i = 0; i < 20; ++i)
	{
		h.Add(4.0f);
	}
	ASSERT_EQ(h.count, 1u);
	const auto points = h.Finished();
	ASSERT_EQ(points.size(), 1u);
	EXPECT_NEAR(points.at(0), 10.0f, 1e-4f);
}

TEST(GameStats, SacrificesWoodAndTheStartGoToTheirPlayer)
{
	ClearAll();
	Sacrificed(2);
	Sacrificed(2);
	WoodUsed(2, 30);
	WoodUsed(2, 12);
	EXPECT_EQ(Of(2).sacrifices, 2u);
	EXPECT_EQ(Of(2).woodUsed, 42u);
	EXPECT_EQ(Of(1).sacrifices, 0u);
	Init(2, 0.25f, -0.5f, 1234, 6);
	EXPECT_FLOAT_EQ(Of(2).alignmentAtStart, 0.25f);
	EXPECT_FLOAT_EQ(Of(2).creatureAtStart, -0.5f);
	EXPECT_EQ(GetStatics().startTime, 1234);
	EXPECT_EQ(GetStatics().townCount, 6u);
}

TEST(GameStats, EachLandsClearKeepsTheHistoriesAndClearsTheRest)
{
	ClearAll();
	ChildBorn(1);
	Sacrificed(1);
	for (int i = 0; i < 50; ++i)
	{
		Of(1).population.Add(6.0f);
		Of(1).influence.Add(2.0f);
	}
	GetStatics().townCount = 4;
	const auto population = Of(1).population;
	const auto influence = Of(1).influence;
	ClearAll();
	EXPECT_EQ(Of(1).births, 0u);
	EXPECT_EQ(Of(1).sacrifices, 0u);
	EXPECT_EQ(Of(1).minPopulation, 0xFFFFFFFFu);
	EXPECT_EQ(GetStatics().townCount, 0u);
	EXPECT_EQ(Of(1).population.count, population.count);
	EXPECT_EQ(Of(1).population.buckets, population.buckets);
	EXPECT_EQ(Of(1).influence.buckets, influence.buckets);
}

TEST(GameStats, SpellsAndDisciples)
{
	ClearAll();
	openblack::ecs::components::PlayerMagic magic;
	magic.castCount.at(1) = 1;
	magic.castCount.at(35) = 2;
	magic.castCount.at(36) = 3; // the original counts nothing for 36..40
	magic.castCount.at(41) = 4;
	EXPECT_EQ(SpellsCast(magic, 1), 1u);
	EXPECT_EQ(SpellsCast(magic, 35), 2u);
	EXPECT_EQ(SpellsCast(magic, 36), 0u);
	EXPECT_EQ(SpellsCast(magic, 41), 4u);
	DiscipleMade(1, 1);
	DiscipleMade(1, 6); // not counted
	DiscipleMade(1, 8);
	EXPECT_EQ(Of(1).disciples.at(0), 1u);
	EXPECT_EQ(Of(1).disciples.at(5), 0u);
	EXPECT_EQ(Of(1).disciples.at(7), 1u);
}

TEST(GameStats, PopulationTrackerSkipsTheMinimumOnANewMaximum)
{
	ClearAll();
	TrackPopulation(2, 10, 5); // a new maximum: the minimum (-1) stays
	EXPECT_EQ(Of(2).maxPopulation, 15u);
	EXPECT_EQ(Of(2).minPopulation, 0xFFFFFFFFu);
	TrackPopulation(2, 4, 1);
	EXPECT_EQ(Of(2).minPopulation, 5u);
}

TEST(GameStats, DeathsGoToTheTownAndTheKiller)
{
	ClearAll();
	VillagerDied(1, 3);
	EXPECT_EQ(Of(1).deaths, 1u);
	EXPECT_EQ(Of(1).killedVillagers, 0u);
	EXPECT_EQ(Of(3).killedVillagers, 1u);
	BuildingStoppedFunctional(1);
	BuildingStoppedFunctional(1);
	TownTakenOver(1, 7);
	TownTakenOver(1, 3);
	EXPECT_EQ(Of(1).townTakenOverPopulation, 10u);
	EXPECT_EQ(Of(1).buildingStopped, 2u);
}

TEST(GameStats, WorldBeliefSkipsTheMinimumOnANewMaximum)
{
	ClearAll();
	WorldBelief(0, 5.0f); // a new maximum: the minimum (FLT_MAX) stays
	EXPECT_EQ(Of(0).maxWorldBelief, 5.0f);
	EXPECT_EQ(Of(0).minWorldBelief, 3.40282347e+38f);
	WorldBelief(0, 2.0f);
	EXPECT_EQ(Of(0).minWorldBelief, 2.0f);
}

TEST(GameStats, SerializeSize)
{
	// the original's order after the header and the player: 4 + 8 + 0x28 + 0x64 + 2 x 0x7DC + 0x2C + 0x18 (the
	// statics) + 0x90 (the spells) + 4 + 4 + 4
	const auto bytes = Serialize(Stats {}, openblack::ecs::components::PlayerMagic {});
	EXPECT_EQ(bytes.size(), 4u + 8u + 0x28u + 0x64u + 2u * 0x7DCu + 0x2Cu + 0x18u + 0x90u + 12u);
}

TEST(GameStats, TheLinesOfCodeStepEveryTenTurnsOrFiveWhenTheTurnEndsPaused)
{
	EXPECT_EQ(UpdateInterval(false), 10u);
	EXPECT_EQ(UpdateInterval(true), 5u);
}

TEST(GameStats, EachBuildingBuiltCountsOnceAndByTheFirstOfItsKinds)
{
	ClearAll();
	BuildingBuilt(1, 0x2);   // a home
	BuildingBuilt(1, 0xA);   // a windmill, a home's bit and its own: a home
	BuildingBuilt(1, 0x404); // a town centre, a civic building
	BuildingBuilt(1, 0x44);  // a creche, a civic building
	BuildingBuilt(1, 0x100); // a wonder
	BuildingBuilt(1, 0x1);   // a building of no kind: only the total
	EXPECT_EQ(Of(1).buildingsBuilt, 6u);
	EXPECT_EQ(Of(1).buildingsBuiltByKind.at(0), 2u);
	EXPECT_EQ(Of(1).buildingsBuiltByKind.at(1), 2u);
	EXPECT_EQ(Of(1).buildingsBuiltByKind.at(2), 1u);
	// A type with both a home's and a wonder's bit counts as a home only
	BuildingBuilt(1, 0x102);
	EXPECT_EQ(Of(1).buildingsBuiltByKind.at(0), 3u);
	EXPECT_EQ(Of(1).buildingsBuiltByKind.at(2), 1u);
	EXPECT_EQ(Of(2).buildingsBuilt, 0u);
}

TEST(GameStats, TheFoodEatenIsAddedToTheTownsPlayer)
{
	ClearAll();
	FoodEaten(1, 30);
	FoodEaten(1, 12);
	EXPECT_EQ(Of(1).foodEaten, 42u);
	EXPECT_EQ(Of(2).foodEaten, 0u);
	// A whole-number count that wraps round
	Of(1).foodEaten = 0xFFFFFFF0u;
	FoodEaten(1, 0x20);
	EXPECT_EQ(Of(1).foodEaten, 0x10u);
}
