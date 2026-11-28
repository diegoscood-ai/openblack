/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The forests' read-only lookups with fake forests and trees: the closest forest with trees to a point (strictly
// within the distance, across the land, the newer of two as near, forests with no trees skipped), and a forest's trees
// in the original's index order (the growing list, then the grown list, each nearest the centre first) with the two
// lists' sizes.

#include <cmath>
#include <cstdint>

#include <array>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GameDistance.h"
#include "ECS/ForestQueries.h"

using namespace openblack::ecs::forest_queries;

namespace
{
entt::entity Entity(uint32_t value)
{
	return static_cast<entt::entity>(value);
}
} // namespace

TEST(ForestQueries, TheNearestForestWithTreesIsFound)
{
	const std::array forests {
	    ForestCandidate {.key = 1, .created = 1, .centre = {100.0f, 0.0f, 0.0f}, .trees = 3},
	    ForestCandidate {.key = 2, .created = 2, .centre = {40.0f, 0.0f, 0.0f}, .trees = 1},
	    ForestCandidate {.key = 3, .created = 3, .centre = {70.0f, 0.0f, 0.0f}, .trees = 2},
	};
	EXPECT_EQ(ClosestForestWithTrees(forests, glm::vec3(0.0f), 1000.0f), 2U);
}

TEST(ForestQueries, AForestWithNoTreesIsSkipped)
{
	const std::array forests {
	    ForestCandidate {.key = 1, .created = 1, .centre = {100.0f, 0.0f, 0.0f}, .trees = 3},
	    ForestCandidate {.key = 2, .created = 2, .centre = {10.0f, 0.0f, 0.0f}, .trees = 0},
	};
	EXPECT_EQ(ClosestForestWithTrees(forests, glm::vec3(0.0f), 1000.0f), 1U);
	const std::array empty {ForestCandidate {.key = 2, .created = 2, .centre = {10.0f, 0.0f, 0.0f}, .trees = 0}};
	EXPECT_EQ(ClosestForestWithTrees(empty, glm::vec3(0.0f), 1000.0f), std::nullopt);
	EXPECT_EQ(ClosestForestWithTrees({}, glm::vec3(0.0f), 1000.0f), std::nullopt);
}

TEST(ForestQueries, OnlyAForestStrictlyNearerThanTheDistanceIsFound)
{
	const glm::vec3 pos {12.5f, 0.0f, -3.25f};
	const std::array forests {ForestCandidate {.key = 7, .created = 1, .centre = {50.0f, 0.0f, 20.0f}, .trees = 1}};
	const float distance = openblack::game_math::GetDistanceInMetres(pos, forests[0].centre);
	EXPECT_EQ(ClosestForestWithTrees(forests, pos, distance), std::nullopt);
	EXPECT_EQ(ClosestForestWithTrees(forests, pos, std::nextafter(distance, 2.0f * distance)), 7U);
}

TEST(ForestQueries, TheDistanceIsAcrossTheLand)
{
	// The height of the point and of the centre do not count
	const std::array forests {
	    ForestCandidate {.key = 1, .created = 1, .centre = {0.0f, 500.0f, 30.0f}, .trees = 1},
	    ForestCandidate {.key = 2, .created = 2, .centre = {0.0f, 0.0f, 40.0f}, .trees = 1},
	};
	EXPECT_EQ(ClosestForestWithTrees(forests, glm::vec3(0.0f), 35.0f), 1U);
}

TEST(ForestQueries, OfTwoAsNearTheNewerIsFound)
{
	// The forests are given oldest first by key, but the game's list is newest first and only a nearer one replaces
	const std::array forests {
	    ForestCandidate {.key = 1, .created = 4, .centre = {0.0f, 0.0f, 25.0f}, .trees = 1},
	    ForestCandidate {.key = 2, .created = 9, .centre = {25.0f, 0.0f, 0.0f}, .trees = 1},
	    ForestCandidate {.key = 3, .created = 6, .centre = {-25.0f, 0.0f, 0.0f}, .trees = 1},
	};
	EXPECT_EQ(ClosestForestWithTrees(forests, glm::vec3(0.0f), 1000.0f), 2U);
}

TEST(ForestQueries, TheTreesComeGrowingFirstThenGrownEachNearestFirst)
{
	const std::vector trees {
	    ForestTree {.entity = Entity(1), .growing = false, .made = 1, .distance = 8.0f},
	    ForestTree {.entity = Entity(2), .growing = true, .made = 2, .distance = 5.0f},
	    ForestTree {.entity = Entity(3), .growing = false, .made = 3, .distance = 2.0f},
	    ForestTree {.entity = Entity(4), .growing = true, .made = 4, .distance = 1.0f},
	    ForestTree {.entity = Entity(5), .growing = false, .made = 5, .distance = 4.0f},
	};
	const std::vector expected {Entity(4), Entity(2), Entity(3), Entity(5), Entity(1)};
	EXPECT_EQ(OrderedTrees(trees), expected);
}

TEST(ForestQueries, TreesAsNearComeInTheOrderTheyWereMade)
{
	const std::vector trees {
	    ForestTree {.entity = Entity(10), .growing = false, .made = 30, .distance = 3.0f},
	    ForestTree {.entity = Entity(11), .growing = false, .made = 10, .distance = 3.0f},
	    ForestTree {.entity = Entity(12), .growing = false, .made = 20, .distance = 3.0f},
	    ForestTree {.entity = Entity(13), .growing = false, .made = 5, .distance = 6.0f},
	};
	const std::vector expected {Entity(11), Entity(12), Entity(10), Entity(13)};
	EXPECT_EQ(OrderedTrees(trees), expected);
}

TEST(ForestQueries, TheTwoListsAreCounted)
{
	const std::vector trees {
	    ForestTree {.entity = Entity(1), .growing = false, .made = 1, .distance = 8.0f},
	    ForestTree {.entity = Entity(2), .growing = true, .made = 2, .distance = 5.0f},
	    ForestTree {.entity = Entity(3), .growing = false, .made = 3, .distance = 2.0f},
	};
	const auto counts = CountTrees(trees);
	EXPECT_EQ(counts.grown, 2U);
	EXPECT_EQ(counts.growing, 1U);
	EXPECT_EQ(counts.Total(), 3U);
	EXPECT_EQ(CountTrees({}).Total(), 0U);
}
