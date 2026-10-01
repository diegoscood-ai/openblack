/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The water queries (ECS/WaterQueries) and the creature's LandAvoid mask (3D/LandAvoid) on the real Land1: they need
// the original game data, so the test only runs with OPENBLACK_TEST_GAME_PATH set to the game's folder (else skipped).

#include <cmath>
#include <cstdlib>

#include <3D/LandAvoid.h>
#include <3D/LandIslandInterface.h>
#include <ECS/Components/Stream.h>
#include <ECS/Registry.h>
#include <ECS/SeaCells.h>
#include <ECS/WaterQueries.h>
#include <FileSystem/FileSystemInterface.h>
#include <Game.h>
#include <Locator.h>
#include <gtest/gtest.h>

using namespace openblack;

class WaterQueries: public ::testing::Test
{
protected:
	static void SetUpTestSuite()
	{
		const char* path = std::getenv("OPENBLACK_TEST_GAME_PATH");
		if (path == nullptr)
		{
			return;
		}
		auto args = openblack::Arguments {
		    .graphicsBackend = openblack::GraphicsBackend::Noop,
		    .gamePath = path,
		    .numFramesToSimulate = 0,
		    .logFile = "stdout",
		};
		std::fill_n(args.logLevels.begin(), args.logLevels.size(), spdlog::level::warn);
		s_game = std::make_unique<openblack::Game>(std::move(args));
		ASSERT_TRUE(s_game->Initialize());
		const auto script = Locator::filesystem::value().GetPath<filesystem::Path::Scripts>() / "Land1.txt";
		ASSERT_TRUE(s_game->LoadMap(script));
	}
	static void TearDownTestSuite() { s_game.reset(); }

	void SetUp() override
	{
		if (!s_game)
		{
			GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH is not set";
		}
	}

	static const LandIslandInterface& Island() { return Locator::terrainSystem::value(); }

	static std::unique_ptr<openblack::Game> s_game;
};

std::unique_ptr<openblack::Game> WaterQueries::s_game;

// PLAN §4.1 test points: open sea (146, 201), shallow coast water (148, 201), dry land (178, 271)
TEST_F(WaterQueries, land_avoid_on_land1)
{
	// open sea, all corners at altitude 0: never walkable
	const auto sea = land_avoid::At(146, 201);
	EXPECT_TRUE(sea == land_avoid::k_Avoid || sea == land_avoid::k_Unreachable) << int(sea);
	// dry land in the middle of the island: reached land
	EXPECT_EQ(land_avoid::At(178, 271), land_avoid::k_Land);
	EXPECT_TRUE(land_avoid::IsPosValid({1788.4f, 0.0f, 2710.0f}));
	EXPECT_FALSE(land_avoid::IsPosValid({1464.0f, 0.0f, 2016.0f}));
	// off the map
	EXPECT_FALSE(land_avoid::IsPosValid({-5.0f, 0.0f, 2710.0f}));

	// every 6 is a water cell and every 0 a land cell; some of each
	size_t water = 0;
	size_t land = 0;
	for (int32_t z = 0; z < 512; ++z)
	{
		for (int32_t x = 0; x < 512; ++x)
		{
			const auto value = land_avoid::At(x, z);
			ASSERT_TRUE(value == 0 || value == 1 || value == 2 || value == 6) << x << "," << z << ": " << int(value);
			if (value == land_avoid::k_Water)
			{
				++water;
				EXPECT_TRUE(ecs::sea_cells::IsWater(Island(), {x, z}));
			}
			else if (value == land_avoid::k_Land)
			{
				++land;
				EXPECT_FALSE(ecs::sea_cells::IsWater(Island(), {x, z}));
			}
		}
	}
	EXPECT_GT(water, 0u);
	EXPECT_GT(land, 10000u);
}

TEST_F(WaterQueries, nearest_coastal)
{
	// from the open sea next to the start beach, a coastal (land, coast line) cell within 100
	const glm::vec3 from {1464.0f, 0.0f, 2016.0f};
	const auto coast = ecs::water_queries::FindNearestCoastalTo(Island(), from, 100.0f);
	ASSERT_TRUE(coast.has_value());
	EXPECT_TRUE(ecs::sea_cells::IsCoastal(Island(), ecs::sea_cells::CellOf(*coast)));
	EXPECT_LE(ecs::water_queries::GetDistanceInMetres(from, *coast), 100.0f);
	// the spiral keeps the fraction of the start: whole cells away
	EXPECT_NEAR(std::fmod(coast->x - from.x + 1000.0f, 10.0f), 0.0f, 0.01f);
	// a coastal cell answers itself
	const auto again = ecs::water_queries::FindNearestCoastalTo(Island(), *coast, 0.0f);
	ASSERT_TRUE(again.has_value());
	EXPECT_NEAR(again->x, coast->x, 0.01f);
	EXPECT_NEAR(again->z, coast->z, 0.01f);
	// inland, far from the coast: nothing within 5
	EXPECT_FALSE(ecs::water_queries::FindNearestCoastalTo(Island(), {1788.4f, 0.0f, 2710.0f}, 5.0f).has_value());
}

TEST_F(WaterQueries, drinking_water)
{
	auto& registry = Locator::entitiesRegistry::value();
	size_t streams = 0;
	glm::vec3 riverPoint {0.0f};
	registry.Each<const ecs::components::Stream>([&](const ecs::components::Stream& stream) {
		++streams;
		if (!stream.points.empty())
		{
			riverPoint = stream.points.front();
		}
	});
	EXPECT_EQ(streams, 11u); // Land1.txt: 11 CREATE_STREAM

	// next to a river point: the river is found within 20, and the drinking water is at most as far as the river
	const glm::vec3 from {riverPoint.x + 3.0f, 0.0f, riverPoint.z + 4.0f};
	const auto river = ecs::water_queries::FindNearestStreamPosTo(Island(), from, 20.0f);
	ASSERT_TRUE(river.has_value());
	// the nearest of all the river points (the rivers' points are close together: not always riverPoint)
	glm::vec3 nearest {0.0f};
	float best = 1e9f;
	registry.Each<const ecs::components::Stream>([&](const ecs::components::Stream& stream) {
		for (const auto& point : stream.points)
		{
			const float d = std::hypot(point.x - from.x, point.z - from.z);
			if (d < best)
			{
				best = d;
				nearest = point;
			}
		}
	});
	EXPECT_NEAR(river->x, nearest.x, 0.01f);
	EXPECT_NEAR(river->z, nearest.z, 0.01f);
	EXPECT_NEAR(river->y, nearest.y - Island().GetHeightAt({river->x, river->z}), 0.05f);

	glm::vec3 water {0.0f};
	ASSERT_TRUE(ecs::water_queries::FindNearestDrinkingWater(Island(), from, water, 20.0f));
	EXPECT_LE(ecs::water_queries::GetDistanceInMetres(from, water),
	          ecs::water_queries::GetDistanceInMetres(from, *river) + 0.01f);

	// the abode cache
	ecs::water_queries::DrinkingWater cache;
	EXPECT_FALSE(ecs::water_queries::GetNearestWaterPos(cache).has_value());
	EXPECT_TRUE(ecs::water_queries::FindNearestDrinkingWater(Island(), cache, from,
	                                                         ecs::water_queries::k_AbodeDrinkingWaterRadius));
	ASSERT_TRUE(ecs::water_queries::GetNearestWaterPos(cache).has_value());

	// nothing within 1 in the middle of the open sea, and the out position does not change
	glm::vec3 unchanged {1.0f, 2.0f, 3.0f};
	EXPECT_FALSE(ecs::water_queries::FindNearestDrinkingWater(Island(), {1440.0f, 0.0f, 1990.0f}, unchanged, 1.0f));
	EXPECT_EQ(unchanged, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST_F(WaterQueries, distance_in_metres)
{
	// the table inverse square root of GUtils is within about 0.1 %
	const float d = ecs::water_queries::GetDistanceInMetres({100.0f, 0.0f, 100.0f}, {130.0f, 0.0f, 140.0f});
	EXPECT_NEAR(d, 50.0f, 0.1f);
	EXPECT_EQ(ecs::water_queries::GetDistanceInMetres({100.0f, 0.0f, 100.0f}, {100.0f, 0.0f, 100.0f}), 0.0f);
}
