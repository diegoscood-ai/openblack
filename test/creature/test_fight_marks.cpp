/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The marks of the player's presses in a fight (Creature/FightMarks.h): at most twelve, the newest held while its press
// is, a replace keeping only the newest, each move taken taking the oldest, the sparkles' colours by their place and the
// flicker of a mark on the opponent, and a ground mark in its creature's frame

#include <cstdint>

#include <array>

#include <gtest/gtest.h>

#include "Creature/FightMarks.h"

using namespace openblack;
namespace marks = openblack::creature_fight_marks;
using Kind = openblack::creature_fight_marks::Mark::Kind;

TEST(FightMarks, AtMostTwelveTheNewestHeldWhileItsPressIs)
{
	marks::Marks list;
	list.AddOnGround({1.0f, 0.0f, 2.0f}, {0.0f, 0.0f, -1.0f});
	EXPECT_FALSE(list.pending);
	list.AddOnSkin(Kind::Opponent, {.part = 2, .corners = {1, 2, 3}, .u = 0.5f, .v = 0.25f});
	EXPECT_TRUE(list.pending);
	list.Hold(100);
	list.Hold(100);
	EXPECT_EQ(list.marks.at(1).heldMs, 200u);
	EXPECT_EQ(list.marks.at(0).heldMs, 0u);
	for (int i = 0; i < 20; ++i)
	{
		list.Hold(100);
	}
	EXPECT_EQ(list.marks.at(1).heldMs, marks::k_MostHeldMs);
	list.LetGo();
	list.Hold(100);
	EXPECT_EQ(list.marks.at(1).heldMs, marks::k_MostHeldMs);
	while (!list.Full())
	{
		list.AddOnSkin(Kind::Own, {});
	}
	EXPECT_EQ(list.count, marks::k_Most);
	list.AddOnGround({}, {});
	EXPECT_EQ(list.count, marks::k_Most);
}

TEST(FightMarks, AReplaceKeepsTheNewestAndEachMoveTakenTakesTheOldest)
{
	marks::Marks list;
	list.AddOnGround({1.0f, 0.0f, 0.0f}, {});
	list.AddOnGround({2.0f, 0.0f, 0.0f}, {});
	list.AddOnGround({3.0f, 0.0f, 0.0f}, {});
	list.Shunt();
	ASSERT_EQ(list.count, 2u);
	EXPECT_EQ(list.marks.at(0).world.x, 2.0f);
	list.KeepNewest();
	ASSERT_EQ(list.count, 1u);
	EXPECT_EQ(list.marks.at(0).world.x, 3.0f);
	// a hit takes the waiting blow's mark and lets its press go
	list.AddOnSkin(Kind::Opponent, {});
	list.DropWaiting();
	EXPECT_EQ(list.count, 1u);
	EXPECT_FALSE(list.pending);
	list.Shunt();
	list.Shunt();
	EXPECT_EQ(list.count, 0u);
}

// The colours worked out as the game's sparkle draw does, by place among the queue's twelve: yellow next, pale yellow,
// orange, red and dark red towards the twelfth, the alpha 0xA0
TEST(FightMarks, EachSparkleIsColouredByItsPlace)
{
	constexpr std::array<uint32_t, 12> k_Colours {0xa0ffff06, 0xa0ffffc6, 0xa0ffff85, 0xa0ffff45, 0xa0ffff05, 0xa0ffc300,
	                                              0xa0ff8300, 0xa0ff4200, 0xa0ff0200, 0xa0c10000, 0xa0800000, 0xa0400000};
	for (size_t i = 0; i < k_Colours.size(); ++i)
	{
		EXPECT_EQ(marks::Colour(i), k_Colours.at(i)) << i;
	}
}

TEST(FightMarks, AMarkOnTheOpponentFlickersTheFasterTheLongerItWasHeld)
{
	EXPECT_EQ(marks::NextFrame(0, 0), 0u);
	EXPECT_EQ(marks::NextFrame(3, 149), 3u);
	EXPECT_EQ(marks::NextFrame(3, 150), 4u);
	EXPECT_EQ(marks::NextFrame(14, 1200), 6u);
}

TEST(FightMarks, AGroundMarkTurnsAndMovesWithItsCreature)
{
	// facing -z at (100, 0, 100): a point 10 ahead and 5 up
	const glm::vec3 at(100.0f, 0.0f, 100.0f);
	const auto local = marks::LocalOf({100.0f, 5.0f, 90.0f}, at, 0.0f);
	EXPECT_FLOAT_EQ(local.x, 0.0f);
	EXPECT_FLOAT_EQ(local.y, 5.0f);
	EXPECT_FLOAT_EQ(local.z, -10.0f);
	const auto back = marks::WorldOf(local, at, 0.0f);
	EXPECT_FLOAT_EQ(back.x, 100.0f);
	EXPECT_FLOAT_EQ(back.z, 90.0f);
	// moved on by 4 along x, it follows
	const auto moved = marks::WorldOf(local, {104.0f, 0.0f, 100.0f}, 0.0f);
	EXPECT_FLOAT_EQ(moved.x, 104.0f);
	// half way through the turn it is drawn half way from the turn's place
	const auto drawn = marks::DrawnOnGround(back, moved, 0.5f);
	EXPECT_FLOAT_EQ(drawn.x, 102.0f);
	// and half way to the camera
	const auto seen = marks::TowardsCamera({0.0f, 0.0f, 0.0f}, {10.0f, 20.0f, -4.0f});
	EXPECT_FLOAT_EQ(seen.x, 5.0f);
	EXPECT_FLOAT_EQ(seen.y, 10.0f);
	EXPECT_FLOAT_EQ(seen.z, -2.0f);
}
