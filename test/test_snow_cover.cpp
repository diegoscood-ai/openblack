/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <functional>
#include <span>
#include <vector>

#include <glm/mat3x3.hpp>
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
	void Restore(std::span<const float> /*depths*/, std::span<const float> /*blockChanges*/, float /*meltClock*/,
	             int32_t /*meltBand*/) override
	{
	}
	[[nodiscard]] ecs::systems::SavedSnow Saved() const override { return {}; }
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

TEST(SnowCover, AnObjectReadsTheSnowTextureByItsOwnPositionAndNormal)
{
	// The bits of the original's float steps, the whole parts cut towards zero
	const auto uv = snow_cover::ObjectUv({1.5f, 2.25f, -3.75f}, {0.0f, 1.0f, 0.0f});
	EXPECT_EQ(std::bit_cast<uint32_t>(uv.x), 0x3F6CCCCDu);
	EXPECT_EQ(std::bit_cast<uint32_t>(uv.y), 0xBDEB8533u);
	const auto turned = snow_cover::ObjectUv({-12.3f, 4.6f, 7.9f}, {0.6f, 0.8f, 0.0f});
	EXPECT_EQ(std::bit_cast<uint32_t>(turned.x), 0x3F4C49BAu);
	EXPECT_EQ(std::bit_cast<uint32_t>(turned.y), 0x3F0E5600u);
	// A morphing object turns the normal by its axes made unit length: a quarter turn about y at twice the size
	const glm::mat3 axes {glm::vec3(0.0f, 0.0f, -2.0f), glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(2.0f, 0.0f, 0.0f)};
	EXPECT_EQ(snow_cover::MorphingObjectNormal(axes, {1.0f, 0.0f, 0.0f}), glm::vec3(0.0f, 0.0f, -1.0f));
	EXPECT_EQ(snow_cover::MorphingObjectNormal(axes, {0.0f, 1.0f, 0.0f}), glm::vec3(0.0f, 1.0f, 0.0f));
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

namespace
{
std::vector<float> LcgGrid(uint32_t seed, float scale)
{
	std::vector<float> grid(snow_cover::k_Cells);
	uint32_t x = seed;
	for (auto& depth : grid)
	{
		x = x * 1103515245u + 12345u;
		depth = static_cast<float>(static_cast<double>(x >> 16) / 65536.0 * static_cast<double>(scale));
	}
	return grid;
}

uint64_t FnvOf(std::span<const float> values)
{
	uint64_t hash = 0xCBF29CE484222325ull;
	for (const auto value : values)
	{
		const auto bits = std::bit_cast<uint32_t>(value);
		for (int k = 0; k < 4; ++k)
		{
			hash ^= (bits >> (8 * k)) & 0xFFu;
			hash *= 0x100000001B3ull;
		}
	}
	return hash;
}
} // namespace

TEST(SnowCover, StormsLaySnowAsTheOriginal)
{
	// the original's storm (fn_0086C2D0) run natively by the exe oracle on these grids: the depths and the blocks' changes
	// after it by their hash
	struct Case
	{
		const char* name;
		uint32_t seed;
		float scale;
		glm::vec2 centre;
		float inner;
		float outer;
		float amount;
		uint64_t hash;
		uint64_t changeHash;
	};
	const std::array<Case, 7> cases {{
	    {"centre", 1, 0.0f, {2600.0f, 2400.0f}, 200.0f, 600.0f, 3.0f, 0xE3D3B32E6AC58FBDull, 0xCF4BF23917F21869ull},
	    {"on_snow", 2, 120.0f, {1200.0f, 3300.0f}, 80.0f, 500.0f, 1.5f, 0x682CF7EBC02BB9EAull, 0x2525ABDEE2A441D2ull},
	    {"negative_amount", 3, 30.0f, {2000.0f, 2000.0f}, 100.0f, 400.0f, -4.0f, 0x3D3B7541BF19A833ull, 0xD9D508665117D08Bull},
	    // an outer radius not past the inner one is taken 0.01 cells past it
	    {"outer_not_past_inner",
	     4,
	     10.0f,
	     {1800.0f, 2200.0f},
	     300.0f,
	     300.0f,
	     2.0f,
	     0xD0E515FE47E16F12ull,
	     0xD89A575EFEA3E7B2ull},
	    {"off_the_grid_edge", 5, 50.0f, {30.0f, 5100.0f}, 150.0f, 700.0f, 2.5f, 0x622A9AC291191699ull, 0xEAB6DC38C287A038ull},
	    {"block_corners", 6, 0.0f, {1280.0f, 1280.0f}, 0.0f, 330.0f, 1.0f, 0x604F0231104E25F0ull, 0xF395A0482EEC8F82ull},
	    {"clamped_high", 7, 254.0f, {3000.0f, 1500.0f}, 250.0f, 450.0f, 5.0f, 0x0AA8260A941F31FCull, 0x3B0A9AF70060A67Aull},
	}};
	for (const auto& c : cases)
	{
		auto grid = LcgGrid(c.seed, c.scale);
		snow_cover::BlockChanges changes;
		snow_cover::AddStorm(grid, c.centre, c.inner, c.outer, c.amount, &changes);
		EXPECT_EQ(FnvOf(grid), c.hash) << c.name;
		// what each cell gained or lost, counted for its block and its neighbours across the edges it is on
		EXPECT_EQ(FnvOf(changes.change), c.changeHash) << c.name;
	}
}

TEST(SnowCover, TheMeltListsTheBlocksItsSnowChangedAsTheOriginal)
{
	// the original's melt and its block list (fn_0086C7A0, fn_0086C5D0) run natively by the exe oracle, with a block at
	// every place: the depths and the blocks' changes after it by their hash, the clock, the band, the list and the
	// blocks marked (each by its place x * 32 + z, modulo 255, as the oracle's fake blocks are numbered)
	struct Case
	{
		const char* name;
		uint32_t depthSeed;
		float depthScale;
		uint32_t changeSeed;
		float changeScale;
		float clock;
		int32_t band;
		int32_t listed;
		uint32_t listSeed;
		uint64_t depthHash;
		uint64_t changeHash;
		uint32_t clockAfter;
		int32_t bandAfter;
		std::vector<uint32_t> values;
		std::vector<int32_t> places;
		std::vector<int32_t> marked;
	};
	const std::vector<Case> cases {
	    {"one_band",
	     11,
	     30.0f,
	     12,
	     300.0f,
	     0.25f,
	     3,
	     0,
	     0,
	     0xA32D12AC70CB21AAull,
	     0x6063E0F7D08D4FDDull,
	     0x3D4CCCC8,
	     4,
	     {0x43963073, 0x439561CC, 0x4394AF61, 0x43947EC0, 0x438EBDE0, 0x438B1612, 0x43828D00, 0x436D6280, 0x43648654,
	      0x43617BA0, 0x4356A428, 0x434C0F40, 0x4343CFC6, 0x433B3A46, 0x433A9F3C, 0x4301DB68},
	     {179, 117, 51, 52, 178, 243, 243, 180, 18, 114, 147, 208, 82, 240, 21, 245},
	     {20, 84, 115, 147, 209}},
	    {"wrap_flushes",
	     13,
	     40.0f,
	     14,
	     250.0f,
	     0.25f,
	     7,
	     5,
	     15,
	     0x9C11A28DCFF2DB64ull,
	     0x19216CF9860D2444ull,
	     0x3D4CCCC8,
	     0,
	     {0x438C5420, 0x437A2B90, 0x43424C50, 0x4339BFE0, 0x433886E0, 0x43356870, 0x432F1100, 0x431B1BA0, 0x4319FA90,
	      0x43131CC0, 0x43108D00, 0x430EBF90, 0x430D2FF0, 0x43086E60, 0x430085C0, 0x42F1AB20},
	     {227, 165, 134, 227, 130, 195, 164, 2, 35, 69, 66, 163, 194, 101, 131, 196},
	     {3, 33, 34, 71, 89, 96, 99, 121, 128, 163, 191, 197, 224, 229, 245}},
	    {"full_list_evicts",
	     16,
	     60.0f,
	     17,
	     900.0f,
	     0.25f,
	     2,
	     16,
	     18,
	     0xEF74A48E9DBF6979ull,
	     0x6AF0EF7BDD154D8Bull,
	     0x3D4CCCC8,
	     3,
	     {0x4468C5F6, 0x44518CA1, 0x44460C70, 0x441E5540, 0x440EBBA6, 0x440297B0, 0x43B406A0, 0x43868BBC, 0x4385B6B4,
	      0x4382A470, 0x4373CE08, 0x435E0E74, 0x435346B0, 0x43321E40, 0x43071A20, 0x42FEE240},
	     {140, 80, 82, 145, 145, 208, 209, 37, 143, 176, 154, 221, 33, 112, 204, 14},
	     {15,  22,  38,  46,  47,  48,  49,  63,  76,  77,  78,  80,  112,
	      141, 144, 149, 152, 160, 172, 176, 178, 184, 207, 209, 238, 241}},
	    {"no_melt_yet",
	     19,
	     30.0f,
	     20,
	     300.0f,
	     0.1f,
	     1,
	     3,
	     21,
	     0x4C15FEF8D52B9EC0ull,
	     0xBE05873D9D573AFCull,
	     0x3E4CCCCD,
	     1,
	     {0x43A78F4C, 0x4357ABB0, 0x431ACE70},
	     {5, 138, 124},
	     {}},
	};
	const auto lcg = [](uint32_t seed, size_t count) {
		std::vector<uint16_t> out(count);
		uint32_t x = seed;
		for (auto& value : out)
		{
			x = x * 1103515245u + 12345u;
			value = static_cast<uint16_t>(x >> 16);
		}
		return out;
	};
	for (const auto& c : cases)
	{
		auto grid = LcgGrid(c.depthSeed, c.depthScale);
		snow_cover::BlockChanges changes;
		const auto changeWords = lcg(c.changeSeed, changes.change.size());
		for (size_t i = 0; i < changes.change.size(); ++i)
		{
			const auto word = changeWords[i];
			changes.change.at(i) =
			    word % 4 == 0 ? static_cast<float>((static_cast<double>(word) / 65536.0 * 2.0 - 1.0) * c.changeScale) : 0.0f;
		}
		// the list before: the values the oracle's case had, sorted, each at the place of its block's number
		const auto listWords = lcg(c.listSeed, 32);
		std::vector<float> values;
		for (int32_t i = 0; i < c.listed; ++i)
		{
			values.push_back(
			    static_cast<float>(97.0 + static_cast<double>(listWords.at(static_cast<size_t>(i))) / 65536.0 * 300.0));
		}
		std::ranges::sort(values, std::greater {});
		for (int32_t i = 0; i < c.listed; ++i)
		{
			const int32_t place = listWords.at(static_cast<size_t>(16 + i)) % 255;
			changes.listed.at(static_cast<size_t>(i)) = values.at(static_cast<size_t>(i));
			changes.places.at(static_cast<size_t>(i)) = {place / 32, place % 32};
		}
		changes.count = c.listed;
		snow_cover::Melting melting {.clock = c.clock, .band = c.band};
		std::vector<glm::ivec2> marked;
		snow_cover::Melt(grid, melting, 0.1f, &changes, &marked);
		EXPECT_EQ(FnvOf(grid), c.depthHash) << c.name;
		EXPECT_EQ(FnvOf(changes.change), c.changeHash) << c.name;
		EXPECT_EQ(std::bit_cast<uint32_t>(melting.clock), c.clockAfter) << c.name;
		EXPECT_EQ(melting.band, c.bandAfter) << c.name;
		ASSERT_EQ(changes.count, static_cast<int32_t>(c.values.size())) << c.name;
		for (size_t i = 0; i < c.values.size(); ++i)
		{
			EXPECT_EQ(std::bit_cast<uint32_t>(changes.listed.at(i)), c.values.at(i)) << c.name << " " << i;
			const auto place = changes.places.at(i);
			EXPECT_EQ((place.x * 32 + place.y) % 255, c.places.at(i)) << c.name << " " << i;
		}
		std::vector<int32_t> numbers;
		for (const auto& place : marked)
		{
			numbers.push_back((place.x * 32 + place.y) % 255);
		}
		std::ranges::sort(numbers);
		const auto end = std::ranges::unique(numbers).begin();
		numbers.erase(end, numbers.end());
		EXPECT_EQ(numbers, c.marked) << c.name;
	}
}
