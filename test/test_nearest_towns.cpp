/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A town's nearest town (ECS/Town/NearestTowns): the whole-cell distance and the search over the walk of every
// player's towns (docs/wiki/beings/towns.md#the-nearest-town)

#include <array>
#include <optional>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Town/NearestTowns.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::ecs::nearest_towns;

namespace
{
constexpr auto k_A = static_cast<entt::entity>(10);
constexpr auto k_B = static_cast<entt::entity>(11);
constexpr auto k_C = static_cast<entt::entity>(12);
constexpr auto k_D = static_cast<entt::entity>(13);
} // namespace

// The larger difference plus half the smaller, rounded down, whatever the signs and the order
TEST(NearestTowns, TheDistanceIsTheLargerDifferencePlusHalfTheSmaller)
{
	EXPECT_EQ(CellDistance(3, 10), 11u);
	EXPECT_EQ(CellDistance(10, 3), 11u);
	EXPECT_EQ(CellDistance(-3, 10), 11u);
	EXPECT_EQ(CellDistance(3, -10), 11u);
	EXPECT_EQ(CellDistance(5, 5), 7u);
	EXPECT_EQ(CellDistance(7, 0), 7u);
	EXPECT_EQ(CellDistance(0, 0), 0u);
	EXPECT_EQ(CellDistance(1, 1), 1u);
}

// A town never finds itself: alone, it has no nearest town
TEST(NearestTowns, ATownIsNeverItsOwnNearest)
{
	const std::array walk {Place {.town = k_A, .cellX = 20, .cellZ = 20, .tribe = Tribe::CELTIC}};
	EXPECT_EQ(NearestOf(walk, walk.at(0)), entt::entity {entt::null});
	const std::array two {walk.at(0), Place {.town = k_B, .cellX = 60, .cellZ = 20, .tribe = Tribe::NORSE}};
	EXPECT_EQ(NearestOf(two, two.at(0)), k_B);
	EXPECT_EQ(NearestOf(two, two.at(1)), k_A);
}

// The nearest by whole cells, not by metres: 8 cells across and 8 down (12) is nearer than 13 straight across
TEST(NearestTowns, TheNearestIsByTheWholeCellDistance)
{
	const std::array walk {
	    Place {.town = k_A, .cellX = 100, .cellZ = 100, .tribe = Tribe::CELTIC},
	    Place {.town = k_B, .cellX = 113, .cellZ = 100, .tribe = Tribe::CELTIC},
	    Place {.town = k_C, .cellX = 92, .cellZ = 92, .tribe = Tribe::CELTIC},
	};
	EXPECT_EQ(NearestOf(walk, walk.at(0)), k_C);
}

// Of two equally near, the first walked is kept: the search takes only a strictly nearer one
TEST(NearestTowns, OfTwoEquallyNearTheFirstWalkedIsKept)
{
	const std::array walk {
	    Place {.town = k_A, .cellX = 10, .cellZ = 0, .tribe = Tribe::CELTIC},
	    Place {.town = k_B, .cellX = 20, .cellZ = 0, .tribe = Tribe::CELTIC},
	    Place {.town = k_C, .cellX = 30, .cellZ = 0, .tribe = Tribe::CELTIC},
	};
	EXPECT_EQ(NearestOf(walk, walk.at(1)), k_A);
	const std::array reversed {walk.at(2), walk.at(1), walk.at(0)};
	EXPECT_EQ(NearestOf(reversed, reversed.at(1)), k_C);
}

// A town of no tribe is never found, however near; a thing with no tribe at all is
TEST(NearestTowns, ATownOfNoTribeIsNeverFound)
{
	const std::array walk {
	    Place {.town = k_A, .cellX = 0, .cellZ = 0, .tribe = Tribe::CELTIC},
	    Place {.town = k_B, .cellX = 1, .cellZ = 0, .tribe = Tribe::NONE},
	    Place {.town = k_C, .cellX = 50, .cellZ = 0, .tribe = Tribe::AZTEC},
	    Place {.town = k_D, .cellX = 40, .cellZ = 0, .tribe = std::nullopt},
	};
	EXPECT_EQ(NearestOf(walk, walk.at(0)), k_D);
	// the town of no tribe finds its own nearest all the same
	EXPECT_EQ(NearestOf(walk, walk.at(1)), k_A);
}

// No town at all: none
TEST(NearestTowns, WithNoTownsThereIsNone)
{
	const Place from {.town = k_A, .cellX = 0, .cellZ = 0, .tribe = Tribe::CELTIC};
	EXPECT_EQ(NearestOf({}, from), entt::entity {entt::null});
}
