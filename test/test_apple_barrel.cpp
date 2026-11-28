/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <numbers>
#include <optional>

#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "Scenery/AppleBarrelDraw.h"
#include "Scenery/AppleBarrelRules.h"

// raffclar's test/test_apple_barrel.cpp (2a7457736), on the map's own coordinates

using namespace openblack;

TEST(AppleBarrel, PitsAreMeasuredFromTheTempleOrAFixedPlace)
{
	const map_coords::MapCoords temple {.x = 11796480, .z = 17039360, .altitude = 0.0f};
	EXPECT_EQ(apple_barrel::MeasuredFrom(temple), temple);
	// 256 metres in along x and z, one map unit short
	EXPECT_EQ(apple_barrel::MeasuredFrom(std::nullopt), (map_coords::MapCoords {.x = 1677721, .z = 1677721}));
}

TEST(AppleBarrel, TheNearestPitCarriesTheBarrel)
{
	const std::array<float, 3> distances {100.0f, 30.0f, 40.0f};
	EXPECT_EQ(apple_barrel::NearestStore(distances), 1U);
	const std::array<float, 3> first {10.0f, 30.0f, 40.0f};
	EXPECT_EQ(apple_barrel::NearestStore(first), 0U);
}

TEST(AppleBarrel, TheFirstOfEquallyNearPitsCarriesIt)
{
	const std::array<float, 2> distances {10.0f, 10.0f};
	EXPECT_EQ(apple_barrel::NearestStore(distances), 0U);
}

TEST(AppleBarrel, NoPitOrNoneNearEnoughCarriesNothing)
{
	EXPECT_FALSE(apple_barrel::NearestStore({}).has_value());
	const std::array<float, 1> far {apple_barrel::k_FarthestStore};
	EXPECT_FALSE(apple_barrel::NearestStore(far).has_value());
	const std::array<float, 1> justNear {apple_barrel::k_FarthestStore - 1.0f};
	EXPECT_EQ(apple_barrel::NearestStore(justNear), 0U);
}

TEST(AppleBarrel, TheBarrelStandsAtThePitsEdgeTheWayItFaces)
{
	const map_coords::MapCoords store {.x = 1000000, .z = 2000000, .altitude = 3.0f};
	// Unturned, out along x: 9 metres are 58982 map units, truncated
	const auto unturned = apple_barrel::BarrelPlace(store, 0.0f, 9.0f);
	EXPECT_EQ(unturned, (map_coords::MapCoords {.x = 1000000 + 58982, .z = 2000000, .altitude = 0.0f}));
	// A quarter turn takes it out along z: 4 metres are 26214 map units
	const auto quarter = apple_barrel::BarrelPlace(store, std::numbers::pi_v<float> / 2.0f, 4.0f);
	EXPECT_EQ(quarter, (map_coords::MapCoords {.x = 1000000, .z = 2000000 + 26214, .altitude = 0.0f}));
}

TEST(AppleBarrel, APitWithNoReachLeavesTheBarrelAtItsMiddle)
{
	const map_coords::MapCoords store {.x = 5000, .z = 6000, .altitude = 1.0f};
	EXPECT_EQ(apple_barrel::BarrelPlace(store, 0.7f, 0.0f), (map_coords::MapCoords {.x = 5000, .z = 6000}));
}

TEST(AppleBarrel, TheGroundIsReadWhereTheBarrelsMetresTakeItBack)
{
	const map_coords::MapCoords place {.x = 58982, .z = 1677721, .altitude = 0.0f};
	EXPECT_EQ(apple_barrel::GroundCoords(place), place);
}

TEST(AppleBarrel, TheWormSitsOnTheBarrelsPoint)
{
	EXPECT_EQ(apple_barrel::WormSpot(glm::vec3(10.0f, 20.0f, 30.0f), glm::vec3(0.25f, 1.5f, -0.5f)),
	          glm::vec3(10.25f, 21.5f, 29.5f));
}

TEST(AppleBarrel, TheWormLoopsRoundItsClip)
{
	EXPECT_EQ(apple_barrel::StepWorm(0, 16, 1000, true), 16U);
	EXPECT_EQ(apple_barrel::StepWorm(990, 16, 1000, true), 6U);
	EXPECT_EQ(apple_barrel::StepWorm(999, 1, 1000, true), 0U);
	EXPECT_EQ(apple_barrel::StepWorm(500, 0, 1000, true), 500U);
}

TEST(AppleBarrel, AClipThatDoesNotLoopStopsShortOfItsEnd)
{
	EXPECT_EQ(apple_barrel::StepWorm(990, 16, 1000, false), 999U);
	EXPECT_EQ(apple_barrel::StepWorm(100, 16, 1000, false), 116U);
}

TEST(AppleBarrel, AClipWithNoPlayTimeStaysAtItsStart)
{
	EXPECT_EQ(apple_barrel::StepWorm(10, 16, 0, true), 0U);
	EXPECT_EQ(apple_barrel::StepWorm(10, 16, 0, false), 0U);
}

// The barrel and its worm are drawn unturned at their own size, at their places, in the land's light alone with the
// haze, both at the barrel's place; the worm with its bones
TEST(AppleBarrel, BothAreDrawnInTheLandsLightAtTheBarrelsPlace)
{
	const glm::vec3 barrel {1000.0f, 12.5f, 2000.0f};
	const glm::vec3 worm {1000.5f, 14.0f, 2000.25f};
	const auto meshes = apple_barrel::DrawnMeshes(565, 563, barrel, worm, {glm::mat4(2.0f)});
	EXPECT_EQ(meshes[0].meshId, 565u);
	EXPECT_EQ(meshes[1].meshId, 563u);
	EXPECT_EQ(glm::vec3(meshes[0].model[3]), barrel);
	EXPECT_EQ(glm::vec3(meshes[1].model[3]), worm);
	EXPECT_EQ(glm::mat3(meshes[1].model), glm::mat3(1.0f));
	for (const auto& drawn : meshes)
	{
		EXPECT_TRUE(drawn.landscapeColour);
		EXPECT_TRUE(drawn.landLightAlone);
		EXPECT_FALSE(drawn.translucent);
		EXPECT_EQ(drawn.lightPoint, std::optional(barrel));
	}
	EXPECT_TRUE(meshes[0].pose.empty());
	ASSERT_EQ(meshes[1].pose.size(), 1u);
}
