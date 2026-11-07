/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The lion maze's rules (ecs::lion_maze): its walls, the wolf's slide and the grid's places. The walls are pinned to the
// original's own step test, run on the exe for every square and way. See docs/wiki/world/lion-maze.md

#include <cstddef>
#include <cstdint>

#include <array>
#include <span>
#include <utility>

#include <gtest/gtest.h>

#include "ECS/LionMazeRules.h"

using namespace openblack::ecs;
using lion_maze::Square;
using lion_maze::Way;

namespace
{
/// The original's step test on type 12, square by square (y rows, x columns): bit n set when a wall stops a step the
/// way n (+y, +x, -y, -x)
constexpr std::array<std::array<uint8_t, 7>, 7> k_Blocked {{
    {0xC, 0x7, 0xC, 0x6, 0xC, 0x4, 0x7},
    {0x8, 0x4, 0x0, 0x0, 0x1, 0x0, 0x6},
    {0x9, 0x0, 0x2, 0x9, 0x4, 0x3, 0xA},
    {0xD, 0x0, 0x1, 0x4, 0x0, 0x7, 0xA},
    {0xC, 0x0, 0x5, 0x0, 0x3, 0xC, 0x2},
    {0xA, 0x9, 0x7, 0x8, 0x4, 0x0, 0x3},
    {0x9, 0x5, 0x5, 0x1, 0x3, 0x9, 0x7},
}};
} // namespace

TEST(LionMazeRules, TheWallsStopTheStepsTheOriginalsStop)
{
	for (int32_t y = 0; y < lion_maze::k_Squares; ++y)
	{
		for (int32_t x = 0; x < lion_maze::k_Squares; ++x)
		{
			for (size_t way = 0; way < lion_maze::k_Ways; ++way)
			{
				const bool blocked = (k_Blocked.at(static_cast<size_t>(y)).at(static_cast<size_t>(x)) >> way & 1U) != 0;
				EXPECT_EQ(lion_maze::Blocked({.x = x, .y = y}, static_cast<Way>(way)), blocked)
				    << "square " << x << "," << y << " way " << way;
			}
		}
	}
	// off the map's eight by eight there is always a wall
	EXPECT_TRUE(lion_maze::WallAt(-1, 3, lion_maze::Wall::AlongX));
	EXPECT_TRUE(lion_maze::WallAt(3, 8, lion_maze::Wall::AlongY));
}

// The wolf slides from its start each way until a wall stops it; a wall in front of it leaves it where it is
TEST(LionMazeRules, TheWolfSlidesUntilAWallStopsIt)
{
	const auto start = lion_maze::k_WolfStart;
	EXPECT_EQ(lion_maze::Slide(start, Way::PlusY), (Square {.x = 1, .y = 5}));
	EXPECT_EQ(lion_maze::Slide(start, Way::PlusX), (Square {.x = 5, .y = 3}));
	EXPECT_EQ(lion_maze::Slide(start, Way::MinusY), (Square {.x = 1, .y = 1}));
	EXPECT_EQ(lion_maze::Slide(start, Way::MinusX), (Square {.x = 0, .y = 3}));
	EXPECT_FALSE(lion_maze::Slide({.x = 1, .y = 1}, Way::MinusY).has_value());
}

// Eight bells take the wolf from its start to the sheep: -y, +x, +y, -x, +y, -x, -y, +x
TEST(LionMazeRules, EightBellsTakeTheWolfToTheSheep)
{
	constexpr std::array<std::pair<Way, Square>, 8> k_Path {{
	    {Way::MinusY, {.x = 1, .y = 1}},
	    {Way::PlusX, {.x = 6, .y = 1}},
	    {Way::PlusY, {.x = 6, .y = 5}},
	    {Way::MinusX, {.x = 3, .y = 5}},
	    {Way::PlusY, {.x = 3, .y = 6}},
	    {Way::MinusX, {.x = 0, .y = 6}},
	    {Way::MinusY, {.x = 0, .y = 4}},
	    {Way::PlusX, {.x = 4, .y = 4}},
	}};
	auto at = lion_maze::k_WolfStart;
	for (const auto& [way, to] : k_Path)
	{
		const auto slid = lion_maze::Slide(at, way);
		ASSERT_TRUE(slid.has_value());
		EXPECT_EQ(*slid, to);
		at = *slid;
	}
	EXPECT_EQ(at, lion_maze::k_SheepStart);
}

// The grid's places in half squares: a square's middle, the walls on its near sides, and a wall's angle
TEST(LionMazeRules, TheGridsPlacesAndAngles)
{
	EXPECT_EQ(lion_maze::SquareSpot({.x = 4, .y = 4}).across, 9);
	EXPECT_EQ(lion_maze::SquareSpot({.x = 4, .y = 4}).along, 9);
	EXPECT_EQ(lion_maze::WallSpot(2, 3, lion_maze::Wall::AlongY).across, 5);
	EXPECT_EQ(lion_maze::WallSpot(2, 3, lion_maze::Wall::AlongY).along, 6);
	EXPECT_EQ(lion_maze::WallSpot(2, 3, lion_maze::Wall::AlongX).across, 4);
	EXPECT_EQ(lion_maze::WallSpot(2, 3, lion_maze::Wall::AlongX).along, 7);
	EXPECT_EQ(lion_maze::WallAngle(0, lion_maze::Wall::AlongY), 0.0f);
	EXPECT_EQ(lion_maze::WallAngle(0, lion_maze::Wall::AlongX), 1.5707964f);
	// the outer walls stand all round: across x at both ends of every row, across y at both ends of every column
	for (int32_t i = 0; i < lion_maze::k_Squares; ++i)
	{
		EXPECT_TRUE(lion_maze::HasNearWall(0, i, lion_maze::Wall::AlongX));
		EXPECT_TRUE(lion_maze::HasNearWall(lion_maze::k_Squares, i, lion_maze::Wall::AlongX));
		EXPECT_TRUE(lion_maze::HasNearWall(i, 0, lion_maze::Wall::AlongY));
		EXPECT_TRUE(lion_maze::HasNearWall(i, lion_maze::k_Squares, lion_maze::Wall::AlongY));
	}
	EXPECT_FALSE(lion_maze::HasNearWall(3, lion_maze::k_Squares, lion_maze::Wall::AlongX));
	EXPECT_FALSE(lion_maze::HasNearWall(lion_maze::k_Squares, 3, lion_maze::Wall::AlongY));
}

namespace
{
/// The original's step test on types 10 and 11 (the Theseus mazes), as k_Blocked
constexpr std::array<std::array<uint8_t, 7>, 7> k_TheseusOneBlocked {{
    {0xC, 0x5, 0x5, 0x5, 0x5, 0x5, 0x5},
    {0xA, 0xE, 0xC, 0x4, 0x5, 0x5, 0x6},
    {0xA, 0xA, 0x8, 0x0, 0x6, 0xE, 0xA},
    {0x8, 0x1, 0x1, 0x2, 0x8, 0x2, 0xA},
    {0x8, 0x4, 0x5, 0x1, 0x1, 0x3, 0xA},
    {0xA, 0x9, 0x5, 0x5, 0x5, 0x5, 0x2},
    {0x9, 0x5, 0x5, 0x5, 0x5, 0x5, 0x3},
}};
constexpr std::array<std::array<uint8_t, 8>, 8> k_TheseusTwoBlocked {{
    {0xC, 0x5, 0x5, 0x4, 0x4, 0x4, 0x4, 0x6},
    {0xA, 0xC, 0x4, 0x0, 0x0, 0x0, 0x0, 0x2},
    {0xA, 0xB, 0x9, 0x0, 0x0, 0x0, 0x0, 0x2},
    {0x8, 0x6, 0xE, 0x8, 0x0, 0x0, 0x0, 0x2},
    {0x8, 0x0, 0x0, 0x2, 0x8, 0x0, 0x0, 0x2},
    {0x8, 0x1, 0x0, 0x0, 0x2, 0x9, 0x0, 0x2},
    {0xA, 0xD, 0x1, 0x1, 0x1, 0x6, 0x8, 0x2},
    {0x9, 0x5, 0x5, 0x5, 0x5, 0x1, 0x0, 0x3},
}};

template <size_t N>
void ExpectBlocked(const std::array<std::array<uint8_t, N>, N>& table, const lion_maze::Kind& kind)
{
	for (size_t y = 0; y < N; ++y)
	{
		for (size_t x = 0; x < N; ++x)
		{
			for (size_t way = 0; way < lion_maze::k_Ways; ++way)
			{
				const bool blocked = (table.at(y).at(x) >> way & 1U) != 0;
				EXPECT_EQ(lion_maze::Blocked({.x = static_cast<int32_t>(x), .y = static_cast<int32_t>(y)},
				                             static_cast<Way>(way), kind),
				          blocked)
				    << "square " << x << "," << y << " way " << way;
			}
		}
	}
}

/// A Theseus maze played by its rules: each step (a way, or k_StepWait) moves the villager, then the wolf chases him;
/// the villager is out once he stands past the grid
bool Escapes(const lion_maze::Kind& kind, std::span<const int32_t> steps)
{
	auto villager = kind.preyStart;
	auto wolf = kind.wolfStart;
	for (const auto step : steps)
	{
		if (step != lion_maze::k_StepWait)
		{
			if (lion_maze::Blocked(villager, static_cast<Way>(step), kind))
			{
				return false;
			}
			villager.x += step == 1 ? 1 : step == 3 ? -1 : 0;
			villager.y += step == 0 ? 1 : step == 2 ? -1 : 0;
		}
		if (lion_maze::Escaped(villager, kind))
		{
			return true;
		}
		wolf = lion_maze::ChaseOf(wolf, villager, kind).wolf;
		if (wolf == villager)
		{
			return false;
		}
	}
	return false;
}
} // namespace

TEST(LionMazeRules, TheTheseusMazesWallsStopTheStepsTheOriginalsStop)
{
	ExpectBlocked(k_TheseusOneBlocked, lion_maze::k_TheseusOne);
	ExpectBlocked(k_TheseusTwoBlocked, lion_maze::k_TheseusTwo);
}

// A click in the villager's row or column is a step that way, his own square a wait, a slant nothing, and a wall in the
// way nothing
TEST(LionMazeRules, AClickAsksTheVillagerForAStep)
{
	const auto& kind = lion_maze::k_TheseusOne;
	const Square villager {.x = 3, .y = 2};
	EXPECT_EQ(lion_maze::ClickedStep(villager, {.across = 7, .along = 5}, kind), lion_maze::k_StepWait);
	EXPECT_EQ(lion_maze::ClickedStep(villager, {.across = 7, .along = 9}, kind), static_cast<int32_t>(Way::PlusY));
	EXPECT_EQ(lion_maze::ClickedStep(villager, {.across = 7, .along = 1}, kind), static_cast<int32_t>(Way::MinusY));
	EXPECT_EQ(lion_maze::ClickedStep(villager, {.across = 13, .along = 5}, kind), static_cast<int32_t>(Way::PlusX));
	EXPECT_EQ(lion_maze::ClickedStep(villager, {.across = 1, .along = 5}, kind), static_cast<int32_t>(Way::MinusX));
	EXPECT_EQ(lion_maze::ClickedStep(villager, {.across = 9, .along = 7}, kind), lion_maze::k_StepNone);
	// from (1, 1) a wall stands +x
	EXPECT_EQ(lion_maze::ClickedStep({.x = 1, .y = 1}, {.across = 5, .along = 3}, kind), lion_maze::k_StepNone);
}

// A click's ground in half squares, put on a square's middle; off the grid's box, none
TEST(LionMazeRules, AClickLandsOnASquaresMiddle)
{
	constexpr lion_maze::GridSteps k_Steps {.cosine = 65536 / 5, .sine = 0};
	constexpr int32_t k_Place = 0x1000000;
	const int32_t far = k_Place + 16 * k_Steps.cosine;
	const auto spot = lion_maze::ClickedSpot(k_Place, k_Place, far, far, k_Steps, 1.0f, k_Place + 5 * k_Steps.cosine + 100,
	                                         k_Place + 2 * k_Steps.cosine + 10);
	ASSERT_TRUE(spot.has_value());
	EXPECT_EQ(spot->across, 5);
	EXPECT_EQ(spot->along, 3);
	EXPECT_FALSE(lion_maze::ClickedSpot(k_Place, k_Place, far, far, k_Steps, 1.0f, k_Place - 1, k_Place).has_value());
	EXPECT_FALSE(lion_maze::ClickedSpot(k_Place, k_Place, far, far, k_Steps, 1.0f, k_Place, far + 1).has_value());
}

// The wolf takes up to two steps towards the villager, across first; its angle is its ways', half way for a pair
TEST(LionMazeRules, TheWolfChasesTheVillagerTwoSteps)
{
	const auto& kind = lion_maze::k_TheseusTwo;
	const auto chase = lion_maze::ChaseOf({.x = 3, .y = 3}, {.x = 6, .y = 5}, kind);
	EXPECT_EQ(chase.wolf, (Square {.x = 5, .y = 3}));
	EXPECT_EQ(chase.ways, 3);
	EXPECT_EQ(lion_maze::ChaseAngle(3), 0);
	EXPECT_EQ(lion_maze::ChaseAngle(12), 256);
	EXPECT_EQ(lion_maze::ChaseAngle(4), 1792);
	EXPECT_EQ(lion_maze::ChaseAngle(19), 1280);
	EXPECT_EQ(lion_maze::ChaseAngle(27), 768);
	EXPECT_FALSE(lion_maze::ChaseAngle(0).has_value());
	EXPECT_EQ(lion_maze::StepAngle(Way::PlusY), 512);
	EXPECT_EQ(lion_maze::StepAngle(Way::MinusY), 1536);
	EXPECT_EQ(lion_maze::StepAngle(Way::MinusX), 1024);
	EXPECT_EQ(lion_maze::StepAngle(Way::PlusX), 0);
}

// Both mazes can be got out of with the wolf after the villager
TEST(LionMazeRules, TheVillagerCanGetOutOfBothTheseusMazes)
{
	constexpr std::array<int32_t, 30> k_One {3, 1, 2, 2, 1, 1, 1, 1, 0, 0, 2, 2, 3, 3, 3,
	                                         0, 0, 3, 3, 3, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1};
	constexpr std::array<int32_t, 26> k_Two {3, 3, 2, 2, 2, 2, 2, 2, 2, 1, 1, 3, -1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0};
	EXPECT_TRUE(Escapes(lion_maze::k_TheseusOne, k_One));
	EXPECT_TRUE(Escapes(lion_maze::k_TheseusTwo, k_Two));
}
