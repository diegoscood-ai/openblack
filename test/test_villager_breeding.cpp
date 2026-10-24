/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rules of love out of doors and of the breeder disciple (ECS/VillagerBreedingRules): how long a kiss lasts, when a
// breeder breeds, the breeder's spot by a home or by the store and its wander, each with the draws it asks for in order.
// The pregnancy and the birth are covered by test_villager_birth.cpp.

#include <cstdint>

#include <functional>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/VillagerBreedingRules.h"

using namespace openblack;
using namespace openblack::ecs::villager_breeding;
namespace mc = openblack::map_coords;

namespace
{
/// Hands out the given draws in turn, recording the limits asked for
template <typename T>
struct Draws
{
	std::vector<T> values;
	std::vector<T> limits;
	size_t next {0};
	std::function<T(T)> Make()
	{
		return [this](T limit) {
			limits.push_back(limit);
			return values.at(next++);
		};
	}
};

/// A point given in metres, as map coordinates
glm::ivec2 At(float x, float z)
{
	return {mc::ToFixed(x), mc::ToFixed(z)};
}

float Metres(glm::ivec2 a, glm::ivec2 b)
{
	return gutils::GetDistanceInMetres(a, b);
}
} // namespace

TEST(VillagerBreeding, LoversMakeLoveForHalfOfTheirTimesWithTheirOwnCountedTwice)
{
	EXPECT_EQ(LoveTurns(2, 2), 4);
	EXPECT_EQ(LoveTurns(4, 2), 5);
	EXPECT_EQ(LoveTurns(2, 4), 6);
	// the sum is halved whole
	EXPECT_EQ(LoveTurns(3, 1), 3);
}

TEST(VillagerBreeding, ABreederBreedsUntilItsTownIsTooFarOverFull)
{
	const TownHeadcount full {.children = 10, .childPlaces = 10, .adults = 20, .adultPlaces = 20};
	EXPECT_TRUE(BreederMayBreed(full, 8.0f));
	EXPECT_TRUE(BreederMayBreed({.children = 17, .childPlaces = 10, .adults = 20, .adultPlaces = 20}, 8.0f));
	EXPECT_FALSE(BreederMayBreed({.children = 18, .childPlaces = 10, .adults = 20, .adultPlaces = 20}, 8.0f));
	EXPECT_TRUE(BreederMayBreed({.children = 0, .childPlaces = 10, .adults = 51, .adultPlaces = 20}, 8.0f));
	EXPECT_FALSE(BreederMayBreed({.children = 0, .childPlaces = 10, .adults = 52, .adultPlaces = 20}, 8.0f));
	// fewer than its places counts below nought, not as a large number
	EXPECT_TRUE(BreederMayBreed({.children = 0, .childPlaces = 30, .adults = 0, .adultPlaces = 40}, 0.5f));
}

TEST(VillagerBreeding, ABreedersSpotByAHomeIsOutFromItsDoor)
{
	Draws<uint32_t> side {.values = {1}};
	Draws<float> floats {.values = {0.0f, 0.0f}};
	const auto home = At(200.0f, 200.0f);
	const auto door = At(200.0f, 205.0f);
	const auto spot = BreederSpotByHome(home, door, 6.0f, side.Make(), floats.Make());
	EXPECT_EQ(side.limits, (std::vector<uint32_t> {2}));
	ASSERT_EQ(floats.limits.size(), 2u);
	EXPECT_FLOAT_EQ(floats.limits.at(0), k_BreederFan);
	EXPECT_FLOAT_EQ(floats.limits.at(1), 3.0f);
	// Straight out from the door, half the home's size
	EXPECT_NEAR(mc::ToMetres(spot.x), 200.0f, 0.05f);
	EXPECT_NEAR(mc::ToMetres(spot.y), 208.0f, 0.05f);
	// the whole size away at most
	Draws<uint32_t> other {.values = {0}};
	Draws<float> most {.values = {0.5f, 3.0f}};
	EXPECT_NEAR(Metres(BreederSpotByHome(home, door, 6.0f, other.Make(), most.Make()), door), 6.0f, 0.05f);
}

TEST(VillagerBreeding, ABreedersSpotByTheStoreAndItsWander)
{
	Draws<float> store {.values = {1.0f, 0.0f}};
	const auto pit = At(300.0f, 300.0f);
	EXPECT_NEAR(Metres(BreederSpotByStore(pit, store.Make()), pit), 4.0f, 0.05f);
	ASSERT_EQ(store.limits.size(), 2u);
	EXPECT_FLOAT_EQ(store.limits.at(0), k_BreederStoreRange);
	Draws<float> walk {.values = {0.0f, 6.0f}};
	const mc::MapCoords area {pit.x, pit.y, 0.0f};
	const auto goal = WanderGoal(area, walk.Make());
	EXPECT_NEAR(gutils::GetDistanceInMetres(goal, area), 6.0f, 0.05f);
	ASSERT_EQ(walk.limits.size(), 2u);
	EXPECT_FLOAT_EQ(walk.limits.at(1), k_WanderReach);
	Draws<uint32_t> pause {.values = {39}};
	EXPECT_EQ(WanderPause(pause.Make()), 69);
	EXPECT_EQ(pause.limits, (std::vector<uint32_t> {40}));
}
