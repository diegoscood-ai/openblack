/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rings' ageing and the ring pool. The colour a ring takes from the land's light when it is added is covered with
// the light table, in test_land_light.cpp

#include <chrono>

#include <gtest/gtest.h>

#include "3D/WaterRings.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/WaterRingSystem.h"

using namespace openblack;

TEST(WaterRings, GoesAtTheEndOfItsLife)
{
	water_rings::Ring ring {.growth = 7.0f, .argb = 0xFFFFFFFFu};
	ASSERT_TRUE(water_rings::Advance(ring, 350.0f));
	EXPECT_EQ(ring.age, 350u);
	EXPECT_TRUE(water_rings::Advance(ring, 349.0f));
	EXPECT_FALSE(water_rings::Advance(ring, 1.0f));
}

TEST(WaterRings, AgesAtItsRateInWholeMilliseconds)
{
	water_rings::Ring ring {.rate = 0.5f};
	ASSERT_TRUE(water_rings::Advance(ring, 33.0f));
	EXPECT_EQ(ring.age, 16u);
}

TEST(WaterRingSystem, HoldsAtMostItsRingsAndLetsThemGo)
{
	ecs::systems::WaterRingSystem rings;
	for (size_t i = 0; i < water_rings::k_MostRings; ++i)
	{
		ASSERT_TRUE(rings.Add({}));
	}
	EXPECT_FALSE(rings.Add({}));
	rings.Update(std::chrono::duration<float, std::milli>(699.0f));
	EXPECT_EQ(rings.GetRings().size(), water_rings::k_MostRings);
	rings.Update(std::chrono::duration<float, std::milli>(1.0f));
	EXPECT_TRUE(rings.GetRings().empty());
	ASSERT_TRUE(rings.Add({}));
	rings.Reset();
	EXPECT_TRUE(rings.GetRings().empty());
}

TEST(WaterRingSystem, KeepsTheOrderTheRingsCameIn)
{
	ecs::systems::WaterRingSystem rings;
	ASSERT_TRUE(rings.Add({.rate = 2.0f, .cell = 1}));
	ASSERT_TRUE(rings.Add({.rate = 1.0f, .cell = 2}));
	ASSERT_TRUE(rings.Add({.rate = 2.0f, .cell = 3}));
	// The fast ones go first, the others keep their places
	rings.Update(std::chrono::duration<float, std::milli>(350.0f));
	ASSERT_EQ(rings.GetRings().size(), 1u);
	EXPECT_EQ(rings.GetRings()[0].cell, 2);
	EXPECT_EQ(rings.GetRings()[0].age, 350u);
}
