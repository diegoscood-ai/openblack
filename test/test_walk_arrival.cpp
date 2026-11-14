/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/WallHug.h"
#include "ECS/WalkArrival.h"

using namespace openblack::ecs;
using openblack::ecs::components::MoveState;
using openblack::map_coords::MapCoords;

TEST(WalkArrival, AWalkStillUnderWayIsNotOver)
{
	const glm::vec2 goal {1000.0f, 1000.0f};
	for (const auto state : {MoveState::Linear, MoveState::Orbit, MoveState::ExitCircle, MoveState::StepThrough})
	{
		EXPECT_FALSE(walk_arrival::WalkIsOver(state, goal + glm::vec2 {3.0f, 0.0f}, goal));
		EXPECT_FALSE(walk_arrival::WalkIsOver(state, goal, goal));
	}
}

TEST(WalkArrival, WithinAStepTheWalkIsOverOnlyOnceOnTheGoal)
{
	const glm::vec2 goal {1000.0f, 1000.0f};
	for (const auto state : {MoveState::FinalStep, MoveState::Arrived})
	{
		EXPECT_FALSE(walk_arrival::WalkIsOver(state, goal + glm::vec2 {3.0f, -2.0f}, goal));
		EXPECT_FALSE(walk_arrival::WalkIsOver(state, goal + glm::vec2 {0.0f, 0.001f}, goal));
		EXPECT_TRUE(walk_arrival::WalkIsOver(state, goal, goal));
	}
}

TEST(WalkArrival, NoWalkUnderWayIsOver)
{
	EXPECT_TRUE(walk_arrival::WalkIsOver(std::nullopt, {0.0f, 0.0f}, {500.0f, 500.0f}));
}

TEST(WalkArrival, AreWeThereIsStrictAtTheReach)
{
	// whole units: a speed of 100 a turn reaches 99 away, not 100, on either side and on either axis
	const MapCoords goal {.x = 1000000, .z = 2000000};
	EXPECT_TRUE(walk_arrival::AreWeThere({.x = goal.x + 99, .z = goal.z}, goal, 100, 0.0f));
	EXPECT_FALSE(walk_arrival::AreWeThere({.x = goal.x + 100, .z = goal.z}, goal, 100, 0.0f));
	EXPECT_FALSE(walk_arrival::AreWeThere({.x = goal.x - 100, .z = goal.z}, goal, 100, 0.0f));
	EXPECT_FALSE(walk_arrival::AreWeThere({.x = goal.x, .z = goal.z + 100}, goal, 100, 0.0f));
	EXPECT_TRUE(walk_arrival::AreWeThere({.x = goal.x, .z = goal.z - 99}, goal, 100, 0.0f));
	// 60^2 + 80^2 = 100^2: on the reach, not within it
	EXPECT_FALSE(walk_arrival::AreWeThere({.x = goal.x + 60, .z = goal.z + 80}, goal, 100, 0.0f));
	EXPECT_TRUE(walk_arrival::AreWeThere({.x = goal.x + 60, .z = goal.z + 79}, goal, 100, 0.0f));
	// standing still with no speed is never there, not even on the goal
	EXPECT_FALSE(walk_arrival::AreWeThere(goal, goal, 0, 0.0f));
}

TEST(WalkArrival, AreWeThereExtraIsInMapUnits)
{
	// the extra is added to the whole speed: half a unit takes the walker on the reach in, a unit past it stays out
	const MapCoords goal {.x = 0, .z = 0};
	EXPECT_FALSE(walk_arrival::AreWeThere({.x = 3276, .z = 0}, goal, 3276, 0.0f));
	EXPECT_TRUE(walk_arrival::AreWeThere({.x = 3276, .z = 0}, goal, 3276, 0.5f));
	EXPECT_FALSE(walk_arrival::AreWeThere({.x = 3277, .z = 0}, goal, 3276, 0.5f));
	// with no speed the extra alone is the reach
	EXPECT_TRUE(walk_arrival::AreWeThere({.x = 0, .z = 0}, goal, 0, 0.5f));
	EXPECT_FALSE(walk_arrival::AreWeThere({.x = 1, .z = 0}, goal, 0, 0.5f));
}

TEST(WalkArrival, AreWeThereRoundsLikeTheGame)
{
	// each square, the sum and the reach squared are rounded to a float: these walkers are within the reach in exact
	// integers (65534^2 + 362^2 = 4294836200 < 65535^2 = 4294836225), but the rounded values are equal, so not there
	const MapCoords goal {.x = 0, .z = 0};
	EXPECT_FALSE(walk_arrival::AreWeThere({.x = 65534, .z = 362}, goal, 65535, 0.0f));
	EXPECT_FALSE(walk_arrival::AreWeThere({.x = 39350, .z = 1316}, goal, 39372, 0.0f));
	EXPECT_FALSE(walk_arrival::AreWeThere({.x = -64963, .z = 3533}, goal, 65059, 0.0f));
	// and one just inside after the rounding (40054^2 + 1722^2 = 1607288200 < 40091^2 = 1607288281)
	EXPECT_TRUE(walk_arrival::AreWeThere({.x = 40054, .z = 1722}, goal, 40091, 0.0f));
}
