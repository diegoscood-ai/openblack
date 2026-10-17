/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/SnowCover.h"
#include "ECS/Systems/Implementations/SnowSystem.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Weather/Atmos.h"
#include "ECS/Weather/Storms.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
std::vector<float> Grid(float depth = 0.0f)
{
	return std::vector<float>(snow_cover::k_Cells, depth);
}

float& Cell(std::vector<float>& grid, int x, int z)
{
	return grid.at((static_cast<size_t>(z) * snow_cover::k_GridSize) + static_cast<size_t>(x));
}

/// The snow as the weather sees it: what the storms lay, how often it melts, where it is asked for, and a depth of its
/// own everywhere
class FakeSnow final: public ecs::systems::SnowSystemInterface
{
public:
	struct Laid
	{
		glm::vec2 centre;
		float inner;
		float outer;
		float amount;
	};

	void Reset() override { ++resets; }
	void AddStorm(glm::vec2 centre, float innerRadius, float outerRadius, float amount) override
	{
		laid.push_back({centre, innerRadius, outerRadius, amount});
	}
	void Melt(float seconds) override { melted.push_back(seconds); }
	[[nodiscard]] std::span<const float> GetDepths() const override { return {}; }
	[[nodiscard]] float GetDepth(glm::vec2 xz) const override
	{
		asked.push_back(xz);
		return depth;
	}
	[[nodiscard]] bool HasSnow() const override { return depth != 0.0f; }
	[[nodiscard]] uint32_t GetRevision() const override { return 0; }

	float depth {0.0f};
	int resets {0};
	std::vector<Laid> laid;
	std::vector<float> melted;
	mutable std::vector<glm::vec2> asked;
};

/// The weather's atmosphere and storms cleared before and after, and the snow put back as it was
class SnowWeatherTest: public ::testing::Test
{
protected:
	void SetUp() override { weather::atmos::Reset(); }
	void TearDown() override { weather::atmos::Reset(); }

private:
	test::RestoreService<Locator::snowSystem> _restoreSnow;
};
} // namespace

TEST(SnowCover, StormsSnowBySnowRateAndFade)
{
	EXPECT_NEAR(snow_cover::StormSnowPerTurn(100, 1.0f, 1.0f), 0.3f, 1e-6f);
	// The storm's default rate of 1.2: the whole part of 100 x 1.2 x the fade
	EXPECT_NEAR(snow_cover::StormSnowPerTurn(100, 1.2f, 1.0f), 0.36f, 1e-6f);
	EXPECT_NEAR(snow_cover::StormSnowPerTurn(100, 1.2f, 0.5f), 0.18f, 1e-6f);
	EXPECT_FLOAT_EQ(snow_cover::StormSnowPerTurn(100, 1.2f, 0.004f), 0.0f);
	EXPECT_FLOAT_EQ(snow_cover::StormSnowPerTurn(0, 1.2f, 1.0f), 0.0f);
}

TEST(SnowCover, TheGridKeepsHalfTheDepthTowardsZero)
{
	EXPECT_EQ(snow_cover::GridByte(0.0f), 0);
	EXPECT_EQ(snow_cover::GridByte(1.9f), 0);
	EXPECT_EQ(snow_cover::GridByte(2.0f), 1);
	EXPECT_EQ(snow_cover::GridByte(9.0f), 4);
	EXPECT_EQ(snow_cover::GridByte(9.9f), 4);
	EXPECT_EQ(snow_cover::GridByte(254.0f), 127);
	EXPECT_EQ(snow_cover::GridByte(snow_cover::k_MaxDepth), 127);
	EXPECT_EQ(snow_cover::GridByte(-3.0f), -1);
	EXPECT_EQ(snow_cover::GridByte(-300.0f), -128);
}

TEST(SnowCover, StormsSnowLessBeyondTheirInnerRadius)
{
	auto grid = Grid();
	// Centred on cell (10, 10), all of it within 2 cells, none from 5
	snow_cover::AddStorm(grid, {400.0f, 400.0f}, 80.0f, 200.0f, 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 10, 10), 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 12, 10), 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 13, 10), 12.0f / 21.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 14, 10), 5.0f / 21.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 15, 10), 0.0f);
}

TEST(SnowCover, StormsTakeSnowAwayNearTheirEdge)
{
	auto grid = Grid(1.0f);
	// 6 cells out, 2 in: 5 across and 3 down lies beyond what is left of the outer radius once the inner is taken off
	snow_cover::AddStorm(grid, {400.0f, 400.0f}, 80.0f, 240.0f, 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 15, 13), 1.0f - (2.0f / 32.0f));
	// And never below none
	auto bare = Grid();
	snow_cover::AddStorm(bare, {400.0f, 400.0f}, 80.0f, 240.0f, 1.0f);
	EXPECT_FLOAT_EQ(Cell(bare, 15, 13), 0.0f);
}

TEST(SnowCover, SnowLiesNoDeeperThanItsMost)
{
	auto grid = Grid(254.5f);
	snow_cover::AddStorm(grid, {400.0f, 400.0f}, 80.0f, 200.0f, 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 10, 10), snow_cover::k_MaxDepth);
}

TEST(SnowCover, MeltsABandAtATime)
{
	auto grid = Grid(3.0f);
	snow_cover::Melting melting;
	EXPECT_TRUE(snow_cover::Melt(grid, melting, 0.31f));
	// The second band of sixteen rows, then the third
	EXPECT_FLOAT_EQ(Cell(grid, 0, 15), 3.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 5, 16), 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 5, 31), 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 5, 32), 3.0f);
	EXPECT_TRUE(snow_cover::Melt(grid, melting, 0.3f));
	EXPECT_FLOAT_EQ(Cell(grid, 5, 32), 1.0f);
	// Snow of 2 or less is gone
	melting.band = 0;
	melting.clock = 0.31f;
	EXPECT_TRUE(snow_cover::Melt(grid, melting, 0.0f));
	EXPECT_FLOAT_EQ(Cell(grid, 5, 16), 0.0f);
	// Nothing melts between the bands' turns, nor where no snow lies
	EXPECT_FALSE(snow_cover::Melt(grid, melting, 0.1f));
	auto bare = Grid();
	snow_cover::Melting bareMelting;
	EXPECT_FALSE(snow_cover::Melt(bare, bareMelting, 10.0f));
}

TEST(SnowCover, DepthBlendsBetweenCells)
{
	auto grid = Grid();
	Cell(grid, 1, 1) = 4.0f;
	Cell(grid, 2, 1) = 8.0f;
	EXPECT_FLOAT_EQ(snow_cover::DepthAt(grid, {60.0f, 40.0f}), 6.0f);
	EXPECT_FLOAT_EQ(snow_cover::DepthAt(grid, {-1.0f, 40.0f}), 0.0f);
	EXPECT_FLOAT_EQ(snow_cover::DepthAt(grid, {127.0f * 40.0f, 40.0f}), 0.0f);
}

TEST(SnowCover, ObjectsShowSnowBeyond20)
{
	EXPECT_EQ(snow_cover::ObjectLevel(10.0f, snow_cover::k_ObjectRate, 255), 0);
	EXPECT_EQ(snow_cover::ObjectLevel(120.0f, snow_cover::k_ObjectRate, 255), 99);
	// A field's crop, at a quarter
	EXPECT_EQ(snow_cover::ObjectLevel(120.0f, 64, 64), 25);
	EXPECT_EQ(snow_cover::ObjectLevel(255.0f, 64, 64), 58);
	// The more snow, the more of the texture shows
	EXPECT_EQ(snow_cover::ObjectThreshold(0), 250);
	EXPECT_EQ(snow_cover::ObjectThreshold(100), 150);
	EXPECT_EQ(snow_cover::ObjectThreshold(300), 0);
}

TEST(SnowCover, LandWhitensOverTheNoise)
{
	EXPECT_EQ(snow_cover::LandLevel(100.0f, 90), 1);
	EXPECT_EQ(snow_cover::LandLevel(90.0f, 100), 0);
	EXPECT_EQ(snow_cover::LandLevel(200.7f, 72), 16);
	EXPECT_EQ(snow_cover::LandLevel(255.0f, 0), 16);
	EXPECT_EQ(snow_cover::LandWhite(0), 14);
	EXPECT_EQ(snow_cover::LandWhite(2), 15);
	EXPECT_EQ(snow_cover::LandWhite(6), 14);
}

TEST(SnowSystem, StormsThatSnowLayItAndItMelts)
{
	ecs::systems::SnowSystem snow;
	EXPECT_FALSE(snow.HasSnow());
	const auto revision = snow.GetRevision();
	snow.AddStorm({400.0f, 400.0f}, 80.0f, 200.0f, 0.3f);
	snow.AddStorm({400.0f, 400.0f}, 80.0f, 200.0f, 0.0f);
	EXPECT_NEAR(snow.GetDepth({400.0f, 400.0f}), 0.3f, 1e-6f);
	EXPECT_TRUE(snow.HasSnow());
	EXPECT_NE(snow.GetRevision(), revision);
	// A good while later, with no more snow, it has all melted
	for (int turn = 0; turn < 100; ++turn)
	{
		snow.Melt(0.1f);
	}
	EXPECT_FLOAT_EQ(snow.GetDepth({400.0f, 400.0f}), 0.0f);
	EXPECT_FALSE(snow.HasSnow());
}

TEST(SnowSystem, NoSnowChangesNothing)
{
	ecs::systems::SnowSystem snow;
	const auto revision = snow.GetRevision();
	snow.AddStorm({400.0f, 400.0f}, 80.0f, 200.0f, 0.0f);
	for (int turn = 0; turn < 100; ++turn)
	{
		snow.Melt(0.1f);
	}
	snow.Reset();
	EXPECT_EQ(snow.GetRevision(), revision);
	EXPECT_FALSE(snow.HasSnow());
	EXPECT_TRUE(std::ranges::all_of(snow.GetDepths(), [](float depth) { return depth == 0.0f; }));
}

TEST(SnowSystem, ResetClearsTheSnow)
{
	ecs::systems::SnowSystem snow;
	snow.AddStorm({400.0f, 400.0f}, 80.0f, 200.0f, 0.3f);
	snow.Reset();
	EXPECT_FLOAT_EQ(snow.GetDepth({400.0f, 400.0f}), 0.0f);
	EXPECT_FALSE(snow.HasSnow());
}

TEST_F(SnowWeatherTest, AStormLaysItsSnowThenSomeMelts)
{
	auto& snow = static_cast<FakeSnow&>(Locator::snowSystem::emplace<FakeSnow>());
	weather::storms::StormDescriptor d;
	d.position = {400.0f, 0.0f, 480.0f};
	d.innerRadius = 100.0f;
	d.outerRadius = 300.0f;
	d.fadeInTime = 1.0f;
	d.lifeTime = 100.0f;
	d.weather = {};
	d.weather.snow = 100;
	static_cast<void>(weather::storms::Create(d));
	weather::atmos::UpdateGame(12.0f, 0.1f);
	ASSERT_EQ(snow.laid.size(), 1U);
	EXPECT_FLOAT_EQ(snow.laid[0].centre.x, 400.0f);
	EXPECT_FLOAT_EQ(snow.laid[0].centre.y, 480.0f);
	// A tenth of the way faded in: its inner radius and its fade a tenth of theirs
	EXPECT_FLOAT_EQ(snow.laid[0].inner, 10.0f);
	EXPECT_FLOAT_EQ(snow.laid[0].outer, 300.0f);
	EXPECT_FLOAT_EQ(snow.laid[0].amount, snow_cover::StormSnowPerTurn(100, d.snowCoverRate, 0.1f));
	ASSERT_EQ(snow.melted.size(), 1U);
	EXPECT_FLOAT_EQ(snow.melted[0], 0.1f);
}

TEST_F(SnowWeatherTest, TheGridCellTakesHalfTheSnowAtItsCorner)
{
	auto& snow = static_cast<FakeSnow&>(Locator::snowSystem::emplace<FakeSnow>());
	snow.depth = 9.9f;
	EXPECT_EQ(weather::atmos::GetWeather({1010.0f, 0.0f, 1030.0f}).snowCover, 4);
	// read at the corner of the cell, not at the point
	ASSERT_FALSE(snow.asked.empty());
	EXPECT_FLOAT_EQ(snow.asked.back().x, 1000.0f);
	EXPECT_FLOAT_EQ(snow.asked.back().y, 1000.0f);
	// and as deep as it gets, the byte's most, in the next turn's cells
	snow.depth = snow_cover::k_MaxDepth;
	weather::atmos::UpdateGame(12.0f, 0.1f);
	EXPECT_EQ(weather::atmos::GetWeather({1010.0f, 0.0f, 1030.0f}).snowCover, 127);
}

TEST_F(SnowWeatherTest, WithoutSnowTheGridKeepsNone)
{
	Locator::snowSystem::reset();
	EXPECT_EQ(weather::atmos::GetWeather({1010.0f, 0.0f, 1030.0f}).snowCover, 0);
	auto& snow = static_cast<FakeSnow&>(Locator::snowSystem::emplace<FakeSnow>());
	weather::atmos::UpdateGame(12.0f, 0.1f);
	EXPECT_EQ(weather::atmos::GetWeather({1010.0f, 0.0f, 1030.0f}).snowCover, 0);
	EXPECT_TRUE(snow.laid.empty());
}
