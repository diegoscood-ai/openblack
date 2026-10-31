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
#include <array>
#include <bit>
#include <functional>
#include <memory>
#include <numbers>
#include <string_view>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/MapCoords.h"
#include "CHLApi.h"
#include "Common/GameRandomTesting.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Systems/Implementations/ScriptObjectsSystem.h"
#include "ECS/Systems/Implementations/WeatherSystem.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/TerrainMaterial.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Weather/Atmos.h"
#include "ECS/Weather/Calendar.h"
#include "ECS/Weather/Climate.h"
#include "ECS/Weather/Storms.h"
#include "ECS/Weather/Weather.h"
#include "ECS/Weather/WeatherInfo.h"
#include "ECS/Weather/WeatherLand.h"
#include "ECS/Weather/WeatherState.h"
#include "ECS/Weather/WeatherThing.h"
#include "ECS/WindResistance.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Script/CHLWeather.h"
#include "Magic/Script/MapScriptWeather.h"
#include "Particles/Rules/Storm.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"
#include "support/SkyFrameGuard.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::weather;

namespace
{
class WeatherTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto info = std::make_unique<InfoConstants>();
		auto& world = info->climate.at(0);
		world.rainMinSpring = 0.1f;
		world.rainMaxSpring = 0.3f;
		world.tempMinSpring = 12.0f;
		world.tempMaxSpring = 12.0f;
		world.windMinSpring = 10.0f;
		world.windMaxSpring = 20.0f;
		// a local climate's spring rain range, for its start
		auto& local = info->climate.at(3);
		local.rainMinSpring = 0.29f;
		local.rainMaxSpring = 0.75f;
		// a climate whose spring runs from 0 to 10 degrees, for the hour of its target
		auto& mild = info->climate.at(4);
		mild.tempMinSpring = 0.0f;
		mild.tempMaxSpring = 10.0f;
		// two climates whose target is 13 degrees, and -13, whatever the hour
		auto& warm = info->climate.at(5);
		warm.tempMinSpring = 13.0f;
		warm.tempMaxSpring = 13.0f;
		auto& cold = info->climate.at(6);
		cold.tempMinSpring = -13.0f;
		cold.tempMaxSpring = -13.0f;
		Locator::infoConstants::reset(info.release());
		// the game's day / night clock as it starts: noon (the climate's hour reads it)
		Locator::dayNightClock::emplace<ecs::systems::DayNightClockSystem>();
		climate::Reset();
		atmos::Reset();
	}

	void TearDown() override
	{
		storms::SetForkCallback({});
		climate::Reset();
		atmos::Reset();
		Locator::dayNightClock::reset();
		Locator::infoConstants::reset();
	}

	/// A storm like the storm miracle's descriptor, at full strength after two seconds
	static storms::StormId MakeStorm(const glm::vec3& position)
	{
		storms::StormDescriptor d;
		d.position = position;
		d.innerRadius = 50.0f;
		d.outerRadius = 100.0f;
		d.fadeInTime = 1.0f;
		d.lifeTime = 1000.0f;
		d.weather = {};
		d.weather.temperature = 20;
		d.weather.rain = 100;
		d.weather.overcast = 80;
		const auto id = storms::Create(d);
		for (int i = 0; i < 20; ++i)
		{
			atmos::UpdateGame(12.0f, 0.1f);
		}
		return id;
	}
};
} // namespace

TEST(WeatherBytes, WrapClampLerp)
{
	EXPECT_EQ(WrapAdd(120, 10), -126);
	EXPECT_EQ(ClampAdd(120, 10), 127);
	EXPECT_EQ(ClampAdd(-120, -10), -128);
	EXPECT_EQ(LerpByte(10, 20, 128), 15);
	EXPECT_EQ(LerpByte(20, 10, 128), 15);
	EXPECT_EQ(LerpByte(0, 20, 256), 20);
}

TEST(WeatherCalendar, StartDateAndSeasons)
{
	// 5 May 1998 18:05:30: 120 days of January..April, the 5th (added whole), 0.754 of a day
	EXPECT_NEAR(calendar::GetDayOfYear(0), 125.754f, 0.001f);
	EXPECT_EQ(calendar::GetMonth(0), 5);
	EXPECT_NEAR(calendar::GetDayOfMonth(0), 5.754f, 0.001f);
	EXPECT_EQ(calendar::GetSeason(0), 0u); // spring
	// one day every 36000 / 365.25 = 98.56 turns
	EXPECT_NEAR(calendar::GetDayOfYear(99) - calendar::GetDayOfYear(0), 99.0f / 98.5626f, 0.001f);
	// summer from day 171: (171 - 125.754) x 98.5626 = 4459.6 turns
	EXPECT_EQ(calendar::GetSeason(4459), 0u);
	EXPECT_EQ(calendar::GetSeason(4460), 1u);
	// winter before day 79 of the next year
	EXPECT_EQ(calendar::GetSeason(static_cast<uint32_t>((365.25f - 125.754f + 10.0f) * 98.5626f)), 3u);
	// autumn from day 263, winter again from day 354 (GetSeason's fourth start)
	EXPECT_EQ(calendar::GetSeason(static_cast<uint32_t>((300.0f - 125.754f) * 98.5626f)), 2u);
	EXPECT_EQ(calendar::GetSeason(static_cast<uint32_t>((360.0f - 125.754f) * 98.5626f)), 3u);
}

TEST_F(WeatherTest, StormFadeAndCalcAtmos)
{
	const auto id = MakeStorm({1000.0f, 0.0f, 1000.0f});
	const auto* storm = storms::Find(id);
	ASSERT_NE(storm, nullptr);
	EXPECT_FLOAT_EQ(storm->fade, 1.0f);
	EXPECT_FLOAT_EQ(storm->innerRadius, 50.0f);
	EXPECT_FLOAT_EQ(storm->outerRadius, 100.0f);

	weather::WeatherInfo w {};
	storms::CalcAtmos(*storm, {1000.0f, 0.0f, 1000.0f}, w);
	EXPECT_EQ(w.temperature, 20);
	EXPECT_EQ(w.rain, 100);
	EXPECT_EQ(w.overcast, 80);
	// half way between the inner and the outer radius: w = 128
	w = {};
	storms::CalcAtmos(*storm, {1075.0f, 0.0f, 1000.0f}, w);
	EXPECT_EQ(w.rain, 50);
	EXPECT_EQ(w.temperature, 10);
	// the outer radius and past it: nothing
	w = {};
	storms::CalcAtmos(*storm, {1100.0f, 0.0f, 1000.0f}, w);
	EXPECT_EQ(w.rain, 0);
	storms::CalcAtmos(*storm, {1101.0f, 0.0f, 1000.0f}, w);
	EXPECT_EQ(w.rain, 0);
}

TEST_F(WeatherTest, StormFadesInOverFadeInTime)
{
	storms::StormDescriptor d;
	d.position = {500.0f, 0.0f, 500.0f};
	d.fadeInTime = 10.0f;
	d.innerRadius = 100.0f;
	const auto id = storms::Create(d);
	for (int i = 0; i < 50; ++i)
	{
		atmos::UpdateGame(12.0f, 0.1f);
	}
	const auto* storm = storms::Find(id);
	ASSERT_NE(storm, nullptr);
	EXPECT_NEAR(storm->fade, 0.5f, 1e-4f);
	EXPECT_NEAR(storm->innerRadius, 50.0f, 1e-2f);
}

TEST_F(WeatherTest, StormsAreAppliedToACellNewestFirst)
{
	// A cell starts from the calm air and takes every storm over it from the newest to the oldest: each pulls the
	// temperature towards its own, so where two overlap the oldest has the last word; their rain adds up
	const auto make = [](int8_t temperature, int8_t rain) {
		storms::StormDescriptor d;
		d.position = {1000.0f, 0.0f, 1000.0f};
		d.innerRadius = 50.0f;
		d.outerRadius = 100.0f;
		d.fadeInTime = 1.0f;
		d.lifeTime = 1000.0f;
		d.weather = {};
		d.weather.temperature = temperature;
		d.weather.rain = rain;
		return storms::Create(d);
	};
	const auto older = make(20, 30);
	const auto newer = make(-10, 50);
	std::vector<storms::StormId> order;
	storms::ForEach([&order](const storms::Storm& storm) { order.push_back(storm.id); });
	EXPECT_EQ(order, (std::vector<storms::StormId> {newer, older}));
	for (int i = 0; i < 20; ++i)
	{
		atmos::UpdateGame(12.0f, 0.1f);
	}
	const auto w = atmos::GetWeather({1010.0f, 0.0f, 1030.0f});
	EXPECT_EQ(w.temperature, 20);
	EXPECT_EQ(w.rain, 80);
	// Without the older one the newer one's temperature stays
	storms::MarkForDeletion(older);
	atmos::UpdateGame(12.0f, 0.1f);
	EXPECT_EQ(atmos::GetWeather({1010.0f, 0.0f, 1030.0f}).temperature, -10);
}

TEST_F(WeatherTest, WindMagicIsACloudlessStormOverThePoint)
{
	// IS_WIND_MAGIC_AT_POS: some storm with no clouds, marked for deletion or not, nearer to the point than its outer
	// radius (the descriptor's, not the fading one) by the 2D table distance; a storm with clouds never counts
	storms::StormDescriptor d;
	d.position = {1000.0f, 300.0f, 1000.0f};
	d.outerRadius = 100.0f;
	d.numClouds = 8;
	const auto cloudy = storms::Create(d);
	EXPECT_FALSE(storms::IsWindMagicAt({1000.0f, 0.0f, 1000.0f}));
	d.position = {2000.0f, 0.0f, 2000.0f};
	d.numClouds = 0;
	const auto wind = storms::Create(d);
	EXPECT_TRUE(storms::IsWindMagicAt({2000.0f, 0.0f, 2000.0f}));
	EXPECT_TRUE(storms::IsWindMagicAt({2099.0f, 500.0f, 2000.0f})); // the height does not count
	EXPECT_FALSE(storms::IsWindMagicAt({2101.0f, 0.0f, 2000.0f}));
	EXPECT_FALSE(storms::IsWindMagicAt({2000.0f, 0.0f, 2101.0f}));
	// what the native adds to the outer radius: the radius's type read as a float, next to nothing
	EXPECT_FALSE(storms::IsWindMagicAt({2101.0f, 0.0f, 2000.0f}, std::bit_cast<float>(2u)));
	storms::MarkForDeletion(wind);
	EXPECT_TRUE(storms::IsWindMagicAt({2000.0f, 0.0f, 2000.0f}));
	storms::MarkForDeletion(cloudy);
	atmos::UpdateGame(12.0f, 0.1f);
	atmos::UpdateGame(12.0f, 0.1f);
	EXPECT_FALSE(storms::IsWindMagicAt({2000.0f, 0.0f, 2000.0f}));
}

TEST_F(WeatherTest, AtmosGridAndHeight)
{
	MakeStorm({1000.0f, 0.0f, 1000.0f});
	// the cell's corner (1000, 1000) is the storm's centre
	auto w = atmos::GetWeather({1010.0f, 0.0f, 1030.0f});
	EXPECT_EQ(w.rain, 100);
	EXPECT_EQ(w.temperature, 20);
	EXPECT_EQ(w.stamp, atmos::Frame());
	// above 50 m the temperature drops by 0.075 per metre, 11 above 200 m
	EXPECT_EQ(atmos::GetWeather({1010.0f, 100.0f, 1030.0f}).temperature, 17); // 20 + trunc(-3.75)
	EXPECT_EQ(atmos::GetWeather({1010.0f, 250.0f, 1030.0f}).temperature, 9);
	// outside the grid: the ambient weather (0)
	EXPECT_EQ(atmos::GetWeather({-100.0f, 0.0f, 1000.0f}).rain, 0);
	EXPECT_EQ(atmos::GetWeather({6000.0f, 0.0f, 1000.0f}).rain, 0);
}

TEST_F(WeatherTest, TheSmoothWeatherBlendsTheFourNearestCellsAndGivesWayToTheCalmAirHighUp)
{
	// The camera's weather: the four cells about the point blended in 1/256 steps, along x then along z, every byte
	// with a wrapping add and the first cell's stamp; above 200 m it moves towards the calm air by (height - 200) / 4
	// (at most 256), then the temperature drops with the height as in the plain sampling. Each case is the original's
	// smooth sampling run on the exe through the oracle on the same cells
	using Bytes = std::array<int8_t, 8>;
	auto& state = Locator::weatherSystem::value().GetState();
	const auto setCell = [&state](int x, int z, Bytes bytes) {
		state.atmosGrid.at(static_cast<size_t>(z) * atmos::k_GridSize + static_cast<size_t>(x)) =
		    std::bit_cast<weather::WeatherInfo>(bytes);
	};
	// temperature, rain, snow, overcast, wind x, wind z, snow cover, stamp
	setCell(0, 0, {20, 100, 0, 80, 24, -16, 3, 5});
	setCell(1, 0, {-30, 0, 60, 10, -40, 8, 0, 6});
	setCell(0, 1, {10, 50, 20, 100, 0, 127, 9, 7});
	setCell(1, 1, {0, 75, -5, 0, 100, -128, 30, 8});
	setCell(127, 0, {15, 40, 0, 60, 8, 8, 0, 9});
	setCell(127, 1, {-5, 90, 10, 20, -8, 16, 4, 10});
	atmos::SetAmbient(std::bit_cast<weather::WeatherInfo>(Bytes {12, 3, 0, 40, -20, 6, 0, 0}));
	struct Case
	{
		glm::vec3 point;
		Bytes expected;
	};
	const std::array<Case, 9> cases = {{
	    {{13.7f, 0.0f, 27.3f}, {5, 60, 13, 62, 23, 24, 11, 5}},
	    {{39.9f, 0.0f, 0.5f}, {-30, 0, 58, 9, -39, 5, 0, 5}},
	    {{0.0f, 0.0f, 0.0f}, {20, 100, 0, 80, 24, -16, 3, 5}},
	    {{13.7f, 120.0f, 27.3f}, {0, 60, 13, 62, 23, 24, 11, 5}},
	    // the calm air above 200 m: 15 of 256, 175 of 256, then all of it
	    {{13.7f, 260.0f, 27.3f}, {-6, 56, 12, 60, 20, 22, 10, 5}},
	    {{13.7f, 900.0f, 27.3f}, {-2, 21, 4, 46, -7, 11, 3, 5}},
	    {{13.7f, 1500.0f, 27.3f}, {1, 3, 0, 40, -20, 6, 0, 5}},
	    // at the grid's edge the cells past it are the calm air
	    {{5099.0f, 0.0f, 21.0f}, {7, 35, 2, 39, -11, 9, 1, 9}},
	    {{5099.0f, 300.0f, 21.0f}, {-4, 31, 1, 39, -12, 8, 0, 9}},
	}};
	for (const auto& c : cases)
	{
		EXPECT_EQ(std::bit_cast<Bytes>(atmos::GetWeatherSmooth(c.point, false)), c.expected)
		    << c.point.x << ", " << c.point.y << ", " << c.point.z;
	}
}

TEST_F(WeatherTest, FallingSnowIsNotSnowOnTheGround)
{
	// GetSnowAt / GetMaxRainingOrSnowingAt read the snow lying on the ground (not ported: 0), IsSnowingAt the
	// falling snow: a snow storm makes it snow, but neither cools a fire nor counts as snow for the trees
	magic::map_script::CreateWeatherClimate(0, 0, {2560.0f, 0.0f, 2560.0f}, 5120.0f, 5120.0f);
	storms::StormDescriptor d;
	d.position = {1000.0f, 0.0f, 1000.0f};
	d.innerRadius = 50.0f;
	d.outerRadius = 100.0f;
	d.fadeInTime = 1.0f;
	d.lifeTime = 1000.0f;
	d.weather = {};
	d.weather.snow = 90;
	d.weather.rain = 30;
	static_cast<void>(storms::Create(d));
	for (int i = 0; i < 20; ++i)
	{
		atmos::UpdateGame(12.0f, 0.1f);
	}
	const glm::vec3 at(1010.0f, 0.0f, 1030.0f);
	EXPECT_TRUE(IsSnowingAt(at));
	EXPECT_FLOAT_EQ(GetSnowAt(at), 0.0f);
	EXPECT_FLOAT_EQ(GetMaxRainingOrSnowingAt(at), GetRainAt(at));
}

/// Snow lying as deep everywhere, as the weather's cells read it
class EvenSnow final: public ecs::systems::SnowSystemInterface
{
public:
	void Reset() override {}
	void AddStorm(glm::vec2 /*centre*/, float /*innerRadius*/, float /*outerRadius*/, float /*amount*/) override {}
	void Melt(float /*seconds*/) override {}
	void Restore(std::span<const float> /*depths*/, std::span<const float> /*blockChanges*/, float /*meltClock*/,
	             int32_t /*meltBand*/) override
	{
	}
	[[nodiscard]] ecs::systems::SavedSnow Saved() const override { return {}; }
	[[nodiscard]] std::span<const float> GetDepths() const override { return {}; }
	[[nodiscard]] float GetDepth(glm::vec2 /*xz*/) const override { return depth; }
	[[nodiscard]] bool HasSnow() const override { return depth != 0.0f; }
	[[nodiscard]] uint32_t GetRevision() const override { return 0; }

	float depth {0.0f};
};

TEST_F(WeatherTest, AStepOnDeepLyingSnowSoundsOfSnow)
{
	// The sound of a step reads the cell's terrain material, which is snow where the lying snow's byte (half its depth)
	// is 27 or more at the cell's corner, else the land's own (here none, so 1)
	test::RestoreService<Locator::snowSystem> restoreSnow;
	auto& snow = static_cast<EvenSnow&>(Locator::snowSystem::emplace<EvenSnow>());
	magic::map_script::CreateWeatherClimate(0, 0, {2560.0f, 0.0f, 2560.0f}, 5120.0f, 5120.0f);
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->terrainMaterial.at(1).surfaceSound = SoundSurfaceType::Grass;
	info->terrainMaterial.at(27).surfaceSound = SoundSurfaceType::Snow;
	Locator::infoConstants::reset(info.release());
	test::WaterCellIsland island(2);
	island.Cell({5, 5}) = test::MakeWaterCell(3, false, false);
	const glm::vec3 step(55.0f, 0.0f, 55.0f);

	snow.depth = 53.9f; // 26 in the byte
	atmos::UpdateGame(12.0f, 0.1f);
	EXPECT_EQ(ecs::terrain_material::AtCell(&island, {5, 5}), 1U);
	EXPECT_EQ(ecs::sea_cells::GetSurfaceType(island, step), static_cast<int32_t>(SoundSurfaceType::Grass));

	snow.depth = 54.0f; // 27
	atmos::UpdateGame(12.0f, 0.1f);
	EXPECT_EQ(ecs::terrain_material::AtCell(&island, {5, 5}), 27U);
	EXPECT_EQ(ecs::sea_cells::GetSurfaceType(island, step), static_cast<int32_t>(SoundSurfaceType::Snow));
}

TEST_F(WeatherTest, ATornadoOverDeepLyingSnowThrowsUpSnowDust)
{
	// A tornado's dust takes the terrain material of its cell, which is snow where the lying snow's byte (half its depth)
	// is 27 or more at the cell's corner, so over deep snow it throws up the snow's dust, else the land's
	test::RestoreService<Locator::snowSystem> restoreSnow;
	test::RestoreService<Locator::terrainSystem> restoreTerrain;
	auto& snow = static_cast<EvenSnow&>(Locator::snowSystem::emplace<EvenSnow>());
	magic::map_script::CreateWeatherClimate(0, 0, {2560.0f, 0.0f, 2560.0f}, 5120.0f, 5120.0f);
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->terrainMaterial.at(1).tornadoDustColorRGB = {0x12u, 0x34u, 0x56u};
	info->terrainMaterial.at(27).tornadoDustColorRGB = {0xF0u, 0xF4u, 0xF8u};
	Locator::infoConstants::reset(info.release());
	auto& island = static_cast<test::WaterCellIsland&>(Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {2}));
	island.Cell({5, 5}) = test::MakeWaterCell(3, false, false);
	const glm::vec3 base(55.0f, 0.0f, 55.0f);

	snow.depth = 53.9f; // 26 in the byte
	atmos::UpdateGame(12.0f, 0.1f);
	EXPECT_EQ(particles::storm::TornadoDustColour(base), 0xFF123456u);

	snow.depth = 54.0f; // 27
	atmos::UpdateGame(12.0f, 0.1f);
	EXPECT_EQ(particles::storm::TornadoDustColour(base), 0xFFF0F4F8u);
}

TEST_F(WeatherTest, DeletionTakesTwoTurns)
{
	const auto id = MakeStorm({1000.0f, 0.0f, 1000.0f});
	storms::MarkForDeletion(id);
	EXPECT_EQ(storms::Find(id), nullptr);
	int count = 0;
	storms::ForEach([&count](const storms::Storm&) { ++count; });
	EXPECT_EQ(count, 1);
	atmos::UpdateGame(12.0f, 0.1f);
	count = 0;
	storms::ForEach([&count](const storms::Storm&) { ++count; });
	EXPECT_EQ(count, 1);
	atmos::UpdateGame(12.0f, 0.1f);
	count = 0;
	storms::ForEach([&count](const storms::Storm&) { ++count; });
	EXPECT_EQ(count, 0);
}

TEST_F(WeatherTest, KillStormsInArea)
{
	const auto id = MakeStorm({1000.0f, 0.0f, 1000.0f});
	storms::KillStormsInArea({1150.0f, 0.0f, 1000.0f}, 40.0f); // 150 >= 40 + 100: stays
	EXPECT_NE(storms::Find(id), nullptr);
	storms::KillStormsInArea({1150.0f, 0.0f, 1000.0f}, 60.0f); // 150 < 160
	EXPECT_EQ(storms::Find(id), nullptr);
}

TEST_F(WeatherTest, ClimatesAddTemperatureAndWind)
{
	// the order of Land 1's script: the local climates first, the world last (the list is newest first)
	magic::map_script::CreateWeatherClimate(1, 2, {2700.0f, 0.0f, 2560.0f}, 156.666672f, 321.666656f);
	magic::map_script::CreateWeatherClimateTemp(1, -35.2f, -32.0f);
	magic::map_script::CreateWeatherClimate(0, 0, {2560.0f, 0.0f, 2560.0f}, 5120.0f, 5120.0f);
	magic::map_script::CreateWeatherClimateTemp(0, 10.8f, 12.0f);
	magic::map_script::CreateWeatherClimateWind(0, 24.0f, 0.0f, 0.0f);

	// the world's temperature counts twice: trunc(10.8) + trunc(10 x 1 + 10)
	EXPECT_FLOAT_EQ(GetTemperatureAt({1000.0f, 0.0f, 1000.0f}), 20.0f);
	// and its wind: 24 + 24 = 48, 6 m/s
	EXPECT_FLOAT_EQ(GetWindAt({1000.0f, 0.0f, 1000.0f}).x, 6.0f);
	// inside the local climate's inner radius: + (-35)
	EXPECT_FLOAT_EQ(GetTemperatureAt({2700.0f, 0.0f, 2560.0f}), -15.0f);
	// half way between its radii: trunc(-35 x 0.5 + 20) = 2
	EXPECT_FLOAT_EQ(GetTemperatureAt({2700.0f + 239.16666f, 0.0f, 2560.0f}), 2.0f);
	// no storm: no rain
	EXPECT_FLOAT_EQ(GetMaxRainingOrSnowingAt({2700.0f, 0.0f, 2560.0f}), 0.0f);
	EXPECT_FALSE(IsRainingAt({2700.0f, 0.0f, 2560.0f}));
}

TEST_F(WeatherTest, TemperatureOscillatesAroundTheTarget)
{
	magic::map_script::CreateWeatherClimate(0, 0, {2560.0f, 0.0f, 2560.0f}, 5120.0f, 5120.0f);
	magic::map_script::CreateWeatherClimateTemp(0, 10.8f, 12.0f);
	auto& world = climate::World();
	// target = 12 (min = max): +1.2 while below, -1.2 once there, as the saved 10.8 / 12 of the land scripts
	climate::ProcessAll(0);
	EXPECT_NEAR(world.temperature, 12.0f, 1e-5f);
	climate::ProcessAll(1);
	EXPECT_NEAR(world.temperature, 10.8f, 1e-5f);
	// the first turn is a new day: the wind is min + max along the angle
	EXPECT_FLOAT_EQ(world.windX, 30.0f);
	EXPECT_FLOAT_EQ(world.windZ, 0.0f);
}

TEST_F(WeatherTest, TheTemperatureStepsByATenthInTheGamesPrecision)
{
	// A tenth of 13 whole degrees is 1.3 in the original: the double tenth times 13, rounded once to a float. A float
	// tenth times 13 rounds one bit higher. Bits from the original's temperature step run on the exe through the oracle
	auto& warm = climate::Create({1000.0f, 0.0f, 1000.0f}, 5, 100.0f, 200.0f, 0.0f, 1);
	auto& cold = climate::Create({3000.0f, 0.0f, 3000.0f}, 6, 100.0f, 200.0f, 0.0f, 2);
	climate::SetTemperature(1, 0.0f, 0.0f);
	climate::SetTemperature(2, 0.0f, 0.0f);
	climate::ProcessAll(0);
	EXPECT_EQ(std::bit_cast<uint32_t>(warm.temperature), 0x3FA66666u);
	EXPECT_EQ(std::bit_cast<uint32_t>(cold.temperature), 0xBFA66666u);
	EXPECT_FLOAT_EQ(warm.targetTemperature, 13.0f);
	// From above the target it steps down by the same tenth: 50 - 1.3
	climate::SetTemperature(1, 50.0f, 0.0f);
	climate::ProcessAll(1);
	EXPECT_EQ(std::bit_cast<uint32_t>(warm.temperature), 0x4242CCCDu);
}

TEST_F(WeatherTest, RainDesireDrawsFromMinThenMax)
{
	// a new day: the rain desire grows by GameFloatRand(rainMin x 0.02) then GameFloatRand(rainMax x 0.02)
	const game_random::testing::ScopedState state;
	std::vector<float> draws;
	game_random::testing::SetGameRand(nullptr, [&draws](float x) {
		draws.push_back(x);
		return 0.0f;
	});
	magic::map_script::CreateWeatherClimate(0, 0, {2560.0f, 0.0f, 2560.0f}, 5120.0f, 5120.0f);
	climate::ProcessAll(0);
	ASSERT_GE(draws.size(), 2u);
	EXPECT_EQ(draws[0], static_cast<float>(static_cast<double>(0.1f) * 0.02));
	EXPECT_EQ(draws[1], static_cast<float>(static_cast<double>(0.3f) * 0.02));
}

TEST_F(WeatherTest, AClimateStartsWithTheSeasonsLeastRain)
{
	// a new climate's desire to rain starts at the season's rainMin and its raining days at int(rainMin x 100); the
	// season's rainMax is not read. 0.29 gives 29, as the original's start of the rain does
	const auto& local = climate::Create({1000.0f, 0.0f, 1000.0f}, 3, 100.0f, 200.0f, 0.0f, 1);
	EXPECT_EQ(local.rain.desire, 0.29f);
	EXPECT_EQ(local.rain.rainingDays, 29);
	EXPECT_EQ(local.rain.dryDays, 0);
	EXPECT_EQ(local.rain.flags, 0);
	// the world climate the same way, from its own row
	const auto& world = climate::World();
	EXPECT_EQ(world.rain.desire, 0.1f);
	EXPECT_EQ(world.rain.rainingDays, 10);
}

TEST_F(WeatherTest, TheHourTableIsReadAtTheLandsOwnHour)
{
	// A climate's target reads its hour table at the clock's visual hour, cut to a whole hour, passed through the
	// script-to-visual map of the land's day and night, and cut again, as the original does. With a night of 0.2 of the
	// day and a change of 0.1, 3 pm of the script's time is 18.94 h of visual time, and the table is read at 21 h, not
	// at 18 h: hour 18 maps to 21.38. Numbers from the original's map run on the exe through the oracle
	const test::SkyFrameGuard sky;
	auto& clock = Locator::dayNightClock::value().Clock();
	clock.SetCycle(DayNightClock::k_DefaultDuration, 0.2f, 0.1f);
	clock.SetScriptTime(15.0f);
	ASSERT_EQ(clock.GetVisualTime(), 18.942855834960938f);
	// May, the start's month, takes half of the range; the hour 21 is 0.8 of it
	const auto& evening = climate::Create({1000.0f, 0.0f, 1000.0f}, 4, 100.0f, 200.0f, 0.0f, 1);
	EXPECT_FLOAT_EQ(evening.targetTemperature, 0.8f * 0.5f * 10.0f);
	EXPECT_FLOAT_EQ(evening.temperature, evening.targetTemperature);
	// 9 am is 5.06 h visual, and hour 5 maps to 2.33: the table's 2 am (0.3), not its 5 am (0.4)
	clock.SetScriptTime(9.0f);
	ASSERT_EQ(clock.GetVisualTime(), 5.057143211364746f);
	const auto& morning = climate::Create({1000.0f, 0.0f, 1000.0f}, 4, 100.0f, 200.0f, 0.0f, 2);
	EXPECT_FLOAT_EQ(morning.targetTemperature, 0.3f * 0.5f * 10.0f);
	// Noon stays noon (1.0)
	clock.SetScriptTime(12.0f);
	climate::ProcessAll(0);
	EXPECT_FLOAT_EQ(climate::Find(2)->targetTemperature, 1.0f * 0.5f * 10.0f);
}

TEST_F(WeatherTest, ClimateStormAtFullDesire)
{
	// The storm's size is GameRand(1000) clamped to 160..900: a draw of 0 gives the smallest storm
	const game_random::testing::ScopedState state;
	game_random::testing::SetGameRand([](uint32_t) { return 0u; }, nullptr);
	magic::map_script::CreateWeatherClimate(0, 0, {2560.0f, 0.0f, 2560.0f}, 5120.0f, 5120.0f);
	climate::Rain rain;
	rain.desire = 1.0f;
	climate::SetRain(0, rain);
	climate::ProcessAll(0); // a new day: desire 1 -> CreateStorm
	const auto& world = climate::World();
	ASSERT_EQ(world.storms.size(), 1u);
	EXPECT_EQ(world.rain.flags & 1, 1);
	EXPECT_FLOAT_EQ(world.rain.desire, 0.0f);
	const auto* storm = storms::Find(world.storms.front());
	ASSERT_NE(storm, nullptr);
	// the temperature went 12 -> 10.8 this turn: 10 degrees, snow int(exp(-(10 x 0.2)^2) x 100) = 1, rain 99;
	// overcast int(exp(-((10 - 30) / 15)^2) x 100) = 16; no raining days yet: life (rainMax x 100 - 0) x 10 = 300 s;
	// radius 160 (the draw of 0 above), outer int(160 x 1.1)
	EXPECT_EQ(storm->descriptor.weather.temperature, 10);
	EXPECT_EQ(storm->descriptor.weather.rain, 99);
	EXPECT_EQ(storm->descriptor.weather.snow, 1);
	EXPECT_EQ(storm->descriptor.weather.overcast, 16);
	EXPECT_EQ(storm->descriptor.weather.windX, 30);
	EXPECT_NEAR(storm->descriptor.lifeTime, 300.0f, 1e-3f);
	EXPECT_FLOAT_EQ(storm->descriptor.innerRadius, 160.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.outerRadius, 176.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.elevation, 500.0f);
}

TEST_F(WeatherTest, TheRainingDaysCountUpWhileItRainsAndDownWhileItIsDry)
{
	// Once a game day: while a storm of the climate falls only its raining days count up; while it is dry the dry days
	// count up and the raining days down, never below 0
	auto& local = climate::Create({1000.0f, 0.0f, 1000.0f}, 3, 100.0f, 200.0f, 0.0f, 1);
	climate::SetRain(1, {.desire = 0.0f, .dryDays = 0, .rainingDays = 2, .flags = 0});
	// (a day is 98.56 turns; the land starts at 0.754 of the 5th of May, so the 6th begins at turn 24.2)
	climate::ProcessAll(0); // the 5th of May
	EXPECT_EQ(local.rain.dryDays, 1);
	EXPECT_EQ(local.rain.rainingDays, 1);
	climate::ProcessAll(20); // the same day: nothing counts
	EXPECT_EQ(local.rain.dryDays, 1);
	climate::ProcessAll(30); // the 6th
	EXPECT_EQ(local.rain.dryDays, 2);
	EXPECT_EQ(local.rain.rainingDays, 0);
	climate::ProcessAll(130); // the 7th
	EXPECT_EQ(local.rain.dryDays, 3);
	EXPECT_EQ(local.rain.rainingDays, 0);
	local.rain.flags = 1;
	const float desire = local.rain.desire;
	climate::ProcessAll(230); // the 8th, falling: the desire does not grow either
	EXPECT_EQ(local.rain.dryDays, 3);
	EXPECT_EQ(local.rain.rainingDays, 1);
	EXPECT_EQ(local.rain.desire, desire);
}

TEST_F(WeatherTest, TheRainingDaysCapTheSeasonsRainInTheGamesPrecision)
{
	// A climate that wants to rain makes no storm once its raining days x 0.01 reach the season's most rain (0.3 for
	// the world's spring here): its desire and dry days start again. The hundredth is a double, the product rounded
	// once to a float: 30 days are 0.3 and have rained enough, as the original's storm creation run on the exe through
	// the oracle gives, where a float hundredth would make them 0.29999998 and let a storm through
	auto& world = climate::World();
	climate::SetRain(0, {.desire = 1.0f, .dryDays = 5, .rainingDays = 30, .flags = 0});
	climate::CreateStorm(world, 0);
	EXPECT_TRUE(world.storms.empty());
	EXPECT_EQ(world.rain.desire, 0.0f);
	EXPECT_EQ(world.rain.dryDays, 0);
	EXPECT_EQ(world.rain.flags & 1, 0);
	// 29 days are not enough: a storm
	climate::SetRain(0, {.desire = 1.0f, .dryDays = 5, .rainingDays = 29, .flags = 0});
	climate::CreateStorm(world, 0);
	EXPECT_EQ(world.storms.size(), 1u);
	EXPECT_EQ(world.rain.flags & 1, 1);

	// On a new day a storm of a climate that has rained more than the season's most starts to clear: its age jumps
	// to where its fade out begins (its life less twice its fade)
	auto& local = climate::Create({1000.0f, 0.0f, 1000.0f}, 3, 100.0f, 200.0f, 0.0f, 1);
	storms::StormDescriptor d;
	d.position = {1000.0f, 300.0f, 1000.0f};
	d.fadeInTime = 10.0f;
	d.lifeTime = 100.0f;
	climate::CreateScriptStorm(1, d, 0.0f, 0.0f, d.position);
	ASSERT_EQ(local.storms.size(), 1u);
	const auto* storm = storms::Find(local.storms.front());
	ASSERT_NE(storm, nullptr);
	// the season's most is 0.75: 75 days are not more
	climate::SetRain(1, {.desire = 0.0f, .dryDays = 0, .rainingDays = 75, .flags = 1});
	climate::ProcessAll(0);
	EXPECT_EQ(storm->age, 0.0f);
	// 77 are (the day counted up to 76 after the storms' turn)
	climate::SetRain(1, {.desire = 0.0f, .dryDays = 0, .rainingDays = 77, .flags = 1});
	climate::ProcessAll(99);
	EXPECT_EQ(storm->age, 80.0f);
}

TEST_F(WeatherTest, ALocalClimateMakesItsStormNearItsMiddle)
{
	// A local climate's storm is at r^2 x its inner radius from its centre's cell, r a draw of 0..1, in a direction
	// drawn from 0..2 pi, cut to a 10 m cell, at a height of 300. The original's placement, run on the exe through the
	// oracle for 6000 random places, gives the same cells
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {32});
	std::vector<float> draws;
	game_random::testing::SetGameRand(nullptr, [&draws](float x) {
		draws.push_back(x);
		return x == 1.0f ? 0.5f : 0.0f; // the direction 0 (along x), r 0.5
	});
	auto& local = climate::Create({1000.0f, 0.0f, 1000.0f}, 3, 100.0f, 200.0f, 0.0f, 1);
	local.rain.desire = 1.0f;
	climate::CreateStorm(local, 0);
	ASSERT_EQ(local.storms.size(), 1u);
	// on the land at once: one direction and one distance drawn
	ASSERT_EQ(draws.size(), 2u);
	EXPECT_EQ(draws[0], std::numbers::pi_v<float> * 2.0f);
	EXPECT_EQ(draws[1], 1.0f);
	const auto* storm = storms::Find(local.storms.front());
	ASSERT_NE(storm, nullptr);
	// 0.25 x 100 x 0.1 = 2.5 cells along x from the cell 100: the cell 102
	EXPECT_EQ(storm->descriptor.position, glm::vec3(1020.0f, 300.0f, 1000.0f));
	// its size is its distance from the centre, 20, raised to the least, 160
	EXPECT_FLOAT_EQ(storm->descriptor.innerRadius, 160.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.outerRadius, 176.0f);
}

TEST_F(WeatherTest, AStormSpotWithNoLandBlockIsTriedAgain)
{
	// A local climate tries another spot only while its spot is off the 512 x 512 cells or in a cell of no land block,
	// 20 tries at most: the original's water test gives 1 only there. What the cell holds does not count, not even
	// bytes that look like the island's answer for no block
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	// 8 x 8 blocks: the cells from 128 on have no block
	auto& island = static_cast<test::WaterCellIsland&>(Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {8}));
	lnd::LNDCell marker {};
	marker.properties.unused6 = true;
	island.Cell(glm::u16vec2(122, 120)) = marker;
	std::vector<float> distances;
	size_t next = 0;
	int draws = 0;
	game_random::testing::SetGameRand(nullptr, [&](float x) {
		++draws;
		if (x != 1.0f)
		{
			return 0.0f; // the direction: along x
		}
		const float r = distances.at(std::min(next, distances.size() - 1));
		++next;
		return r;
	});
	const auto makeStorm = [](int32_t id, float x) -> const storms::Storm* {
		auto& local = climate::Create({x, 0.0f, 1200.0f}, 3, 100.0f, 200.0f, 0.0f, id);
		climate::CreateStorm(local, 0);
		return local.storms.empty() ? nullptr : storms::Find(local.storms.front());
	};

	// r = 1 from the cell 120 is 10 cells away, the cell 130, with no block: tried again; r = 0.5 is 2.5 cells, the
	// cell 122, kept even though its bytes are the marker's
	distances = {1.0f, 0.5f};
	const auto* storm = makeStorm(1, 1200.0f);
	ASSERT_NE(storm, nullptr);
	EXPECT_EQ(draws, 4);
	EXPECT_EQ(storm->descriptor.position, glm::vec3(1220.0f, 300.0f, 1200.0f));

	// Always off the land: after 20 tries the last spot is taken
	distances = {1.0f};
	next = 0;
	draws = 0;
	storm = makeStorm(2, 1250.0f);
	ASSERT_NE(storm, nullptr);
	EXPECT_EQ(draws, 40);
	EXPECT_EQ(storm->descriptor.position, glm::vec3(1350.0f, 300.0f, 1200.0f));
}

TEST_F(WeatherTest, TheWorldClimateMakesItsStormAnywhereOnTheIsland)
{
	// The world climate's storm is at a random 10 m cell of the 512 (x drawn first), its size a draw of 1000. Its own
	// radius covers the whole map, so the first place is always taken
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {32});
	std::vector<uint32_t> draws;
	game_random::testing::SetGameRand(
	    [&draws](uint32_t max) {
		    draws.push_back(max);
		    return max == 1000 ? 500u : (draws.size() == 1 ? 100u : 200u);
	    },
	    nullptr);
	auto& world = climate::World();
	world.rain.desire = 1.0f;
	climate::CreateStorm(world, 0);
	ASSERT_EQ(world.storms.size(), 1u);
	EXPECT_EQ(draws, (std::vector<uint32_t> {512, 512, 1000}));
	const auto* storm = storms::Find(world.storms.front());
	ASSERT_NE(storm, nullptr);
	EXPECT_EQ(storm->descriptor.position, glm::vec3(1000.0f, 300.0f, 2000.0f));
	EXPECT_FLOAT_EQ(storm->descriptor.innerRadius, 500.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.outerRadius, 550.0f);
}

TEST_F(WeatherTest, TheWorldClimatesStormPlaceIsWeighedByItsOwnRadius)
{
	// A place off the land is drawn again while no climate's centre is within the creating climate's own outer radius
	// of it, whatever the other climates' radii. Here the world climate's radius is cut to 10 m, and a local climate
	// whose own 1000 m would cover the first place (500 m from its centre) does not keep it: the place is drawn again
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	// land on the cells 0..255 only: the first place, cell (400, 400), is off it, the second, (100, 200), on it
	Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {16});
	std::vector<uint32_t> draws;
	game_random::testing::SetGameRand(
	    [&draws](uint32_t max) {
		    draws.push_back(max);
		    if (max == 1000)
		    {
			    return 500u;
		    }
		    constexpr std::array<uint32_t, 4> k_Cells {400, 400, 100, 200};
		    return draws.size() <= k_Cells.size() ? k_Cells.at(draws.size() - 1) : 0u;
	    },
	    nullptr);
	climate::Create({4500.0f, 0.0f, 4000.0f}, 3, 1000.0f, 1000.0f, 0.0f, 1);
	auto& world = climate::World();
	world.outerRadius = 10.0f;
	world.rain.desire = 1.0f;
	climate::CreateStorm(world, 0);
	ASSERT_EQ(world.storms.size(), 1u);
	EXPECT_EQ(draws, (std::vector<uint32_t> {512, 512, 512, 512, 1000}));
	const auto* storm = storms::Find(world.storms.front());
	ASSERT_NE(storm, nullptr);
	EXPECT_EQ(storm->descriptor.position, glm::vec3(1000.0f, 300.0f, 2000.0f));
}

TEST_F(WeatherTest, AStormThatLeavesItsClimateStartsToClear)
{
	// On a new day a local climate's storm farther from the climate's cell than its outer radius jumps to where its
	// fade out begins (its life less twice its fade), and the climate wants a new storm (desire 1). One inside stays.
	// The world climate's storms clear once they are inside any climate, the world's own among them: its radius covers
	// the whole map, so they all start to clear on their first new day. (No new storms here, so the desires stay.)
	climate::SetStormCreationEnabled(false);
	storms::StormDescriptor d;
	d.fadeInTime = 10.0f;
	d.lifeTime = 100.0f;
	const auto addStorm = [&d](int32_t id, const glm::vec3& at) -> const storms::Storm* {
		d.position = at;
		climate::CreateScriptStorm(id, d, 0.0f, 0.0f, at);
		const auto* c = climate::Find(id);
		return c != nullptr && !c->storms.empty() ? storms::Find(c->storms.front()) : nullptr;
	};
	auto& away = climate::Create({1000.0f, 0.0f, 1000.0f}, 3, 100.0f, 200.0f, 0.0f, 1);
	auto& home = climate::Create({3000.0f, 0.0f, 3000.0f}, 3, 100.0f, 200.0f, 0.0f, 2);
	auto& world = climate::World();
	const auto* left = addStorm(1, {1500.0f, 300.0f, 1000.0f});
	const auto* stayed = addStorm(2, {3000.0f, 300.0f, 3000.0f});
	const auto* worlds = addStorm(0, {500.0f, 300.0f, 500.0f});
	ASSERT_NE(left, nullptr);
	ASSERT_NE(stayed, nullptr);
	ASSERT_NE(worlds, nullptr);
	climate::ProcessAll(0);
	EXPECT_EQ(left->age, 80.0f);
	EXPECT_EQ(away.rain.desire, 1.0f);
	EXPECT_EQ(stayed->age, 0.0f);
	EXPECT_LT(home.rain.desire, 1.0f);
	EXPECT_EQ(worlds->age, 80.0f);
	EXPECT_EQ(world.rain.desire, 1.0f);
	// Not again on the same day, nor once it is already clearing
	climate::ProcessAll(10);
	EXPECT_EQ(left->age, 80.0f);
}

TEST_F(WeatherTest, AClimatesStormsDriftWithTheWindWhereTheyAre)
{
	// Every turn a climate's storm moves by the atmosphere's wind at its centre (the grid's, its own included) times
	// 0.01 m
	auto& local = climate::Create({1000.0f, 0.0f, 1000.0f}, 3, 500.0f, 600.0f, 0.0f, 1);
	storms::StormDescriptor d;
	d.position = {1000.0f, 300.0f, 1000.0f};
	d.innerRadius = 100.0f;
	d.outerRadius = 200.0f;
	d.fadeInTime = 1.0f;
	d.lifeTime = 1000.0f;
	d.weather = {};
	d.weather.windX = 64;
	d.weather.windZ = -32;
	climate::CreateScriptStorm(1, d, 0.0f, 0.0f, d.position);
	ASSERT_EQ(local.storms.size(), 1u);
	const auto* storm = storms::Find(local.storms.front());
	ASSERT_NE(storm, nullptr);
	// faded in: at its centre, the corner of its cell, the grid's wind is the storm's
	for (int i = 0; i < 20; ++i)
	{
		atmos::UpdateGame(12.0f, 0.1f);
	}
	ASSERT_EQ(atmos::GetWeather(storm->descriptor.position).windX, 64);
	climate::ProcessAll(0);
	const float x = 1000.0f + static_cast<float>(64) * 0.01f;
	const float z = 1000.0f + static_cast<float>(-32) * 0.01f;
	EXPECT_EQ(storm->descriptor.position.x, x);
	EXPECT_EQ(storm->descriptor.position.z, z);
	EXPECT_EQ(storm->descriptor.position.y, 300.0f);
	// the cell is not worked out again until the next frame: the same wind once more
	climate::ProcessAll(1);
	EXPECT_EQ(storm->descriptor.position.x, x + static_cast<float>(64) * 0.01f);
	EXPECT_EQ(storm->descriptor.position.z, z + static_cast<float>(-32) * 0.01f);
}

TEST_F(WeatherTest, ScriptStormBytes)
{
	magic::map_script::CreateWeatherClimate(1, 1, {1000.0f, 0.0f, 1000.0f}, 100.0f, 200.0f);
	magic::map_script::CreateWeatherStorm(1, {1000.0f, 0.0f, 1000.0f}, 3.0f, 4, "10,20,1,50,1,1", "0.5,160,5,6,7,8",
	                                      "5,6,7,8,9,10", 2.0f, {1100.0f, 0.0f, 1000.0f});
	const auto* c = climate::Find(1);
	ASSERT_NE(c, nullptr);
	ASSERT_EQ(c->storms.size(), 1u);
	const auto* storm = storms::Find(c->storms.front());
	ASSERT_NE(storm, nullptr);
	EXPECT_FLOAT_EQ(storm->age, 3.0f);
	EXPECT_FLOAT_EQ(storm->speed, 2.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.innerRadius, 10.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.lifeTime, 50.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.forkMin, 5.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.sheetMax, 8.0f);
	EXPECT_EQ(storm->descriptor.numClouds, 4);
	// "%d" into bytes: overcast and snow are overwritten by the later writes
	EXPECT_EQ(storm->descriptor.weather.temperature, 7);
	EXPECT_EQ(storm->descriptor.weather.rain, 8);
	EXPECT_EQ(storm->descriptor.weather.snow, 0);
	EXPECT_EQ(storm->descriptor.weather.overcast, 0);
	EXPECT_EQ(storm->descriptor.weather.windX, 9);
	EXPECT_EQ(storm->descriptor.weather.windZ, 10);
}

TEST_F(WeatherTest, StormForkEventCallsTheForkCallback)
{
	// Each fork reloads the timer with forkMin + GameFloatRand(forkMax - forkMin), starts the 0.5 flash and calls the
	// fork callback with the storm, the point under its cloud (x, land + elevation, z) and the outer radius. The draw
	// here gives a wait of 2 + 1.05 s: the next fork comes 31 turns later
	const game_random::testing::ScopedState state;
	std::vector<float> draws;
	game_random::testing::SetGameRand(nullptr, [&draws](float x) {
		draws.push_back(x);
		return 1.05f;
	});
	struct Call
	{
		int turn;
		storms::StormId storm;
		glm::vec3 point;
		float radius;
	};
	std::vector<Call> calls;
	int turn = 0;
	storms::SetForkCallback([&calls, &turn](const storms::Storm& storm, const glm::vec3& point, float radius) {
		calls.push_back({turn, storm.id, point, radius});
	});

	storms::StormDescriptor d;
	d.position = {1000.0f, 0.0f, 1200.0f};
	d.innerRadius = 50.0f;
	d.outerRadius = 100.0f;
	// faded in after the 11th turn of 0.1 s
	d.fadeInTime = 1.05f;
	d.lifeTime = 1000.0f;
	d.elevation = 300.0f;
	d.forkMin = 2.0f;
	d.forkMax = 4.0f;
	const auto id = storms::Create(d);
	for (turn = 1; turn <= 50; ++turn)
	{
		atmos::UpdateGame(12.0f, 0.1f);
	}

	ASSERT_EQ(calls.size(), 2u);
	EXPECT_EQ(calls[0].turn, 11);
	EXPECT_EQ(calls[1].turn, 42);
	const glm::vec3 point(1000.0f, LandHeightAt(1000.0f, 1200.0f) + 300.0f, 1200.0f);
	for (const auto& call : calls)
	{
		EXPECT_EQ(call.storm, id);
		EXPECT_EQ(call.point, point);
		EXPECT_FLOAT_EQ(call.radius, 100.0f);
	}
	// one timer draw per fork, of forkMax - forkMin
	EXPECT_EQ(std::count(draws.begin(), draws.end(), 2.0f), 2);
	// the fork's light flash, of 0.5, at the same point over the outer radius; and the wait counted down since
	const auto* storm = storms::Find(id);
	ASSERT_NE(storm, nullptr);
	EXPECT_FLOAT_EQ(storm->flash.strength, 0.5f);
	EXPECT_EQ(storm->flash.position, point);
	EXPECT_FLOAT_EQ(storm->flash.radius, 100.0f);
	EXPECT_NEAR(storm->forkTimer, 3.05f - 0.8f, 1e-4f);
}

TEST_F(WeatherTest, StormWithoutForkLightningNeverCallsTheForkCallback)
{
	int calls = 0;
	storms::SetForkCallback([&calls](const storms::Storm&, const glm::vec3&, float) { ++calls; });
	storms::StormDescriptor d;
	d.position = {1000.0f, 0.0f, 1000.0f};
	d.fadeInTime = 1.0f;
	d.lifeTime = 1000.0f;
	d.forkMin = 2.0f;
	d.forkMax = 0.0f;
	const auto id = storms::Create(d);
	for (int i = 0; i < 100; ++i)
	{
		atmos::UpdateGame(12.0f, 0.1f);
	}
	EXPECT_EQ(calls, 0);
	const auto* storm = storms::Find(id);
	ASSERT_NE(storm, nullptr);
	EXPECT_FLOAT_EQ(storm->forkTimer, 0.0f);
}

// The weather system's methods are the weather functions above. Like them, they work on the state of the system the
// test services put in the locator; the system made here only gives the calls their names.

TEST_F(WeatherTest, TheWeatherSystemSetsUpClimatesAsTheLandScriptDid)
{
	ecs::systems::WeatherSystem system;
	system.CreateClimate(1, 2, {2700.0f, 2560.0f}, 321.666656f, 156.666672f);
	system.CreateClimate(0, 0, {2560.0f, 2560.0f}, 5120.0f, 5120.0f);
	system.SetClimateTemperature(0, 10.8f, 12.0f);
	system.SetClimateWind(0, 24.0f, 0.0f, 0.5f);
	// the rain flags keep the whole byte the script gave
	system.SetClimateRain(0, 0.5f, 3, 4, 3);

	const auto* local = climate::Find(1);
	ASSERT_NE(local, nullptr);
	EXPECT_EQ(local->info, 2);
	EXPECT_FLOAT_EQ(local->innerRadius, 156.666672f);
	EXPECT_FLOAT_EQ(local->outerRadius, 321.666656f);
	// at the point the land script's own call puts it
	const auto& direct = climate::Create({2700.0f, 0.0f, 2560.0f}, 2, 321.666656f, 156.666672f, 0.0f, 2);
	EXPECT_EQ(local->x, direct.x);
	EXPECT_EQ(local->z, direct.z);
	EXPECT_FLOAT_EQ(local->y, direct.y);

	const auto& world = climate::World();
	EXPECT_FLOAT_EQ(world.temperature, 10.8f);
	EXPECT_FLOAT_EQ(world.targetTemperature, 12.0f);
	EXPECT_FLOAT_EQ(world.windX, 24.0f);
	EXPECT_FLOAT_EQ(world.windAngle, 0.5f);
	EXPECT_FLOAT_EQ(world.rain.desire, 0.5f);
	EXPECT_EQ(world.rain.dryDays, 3);
	EXPECT_EQ(world.rain.rainingDays, 4);
	EXPECT_EQ(world.rain.flags, 3);

	// the world's temperature and wind count twice, as in ClimatesAddTemperatureAndWind
	const glm::vec3 at(1000.0f, 0.0f, 1000.0f);
	EXPECT_EQ(system.GetWeather(at).temperature, 20);
	EXPECT_EQ(system.GetWeather(at).windX, 48);
	EXPECT_EQ(system.GetWeatherSmooth(at).temperature, 20);
}

TEST_F(WeatherTest, TheWeatherSystemSwitchesTheClimates)
{
	ecs::systems::WeatherSystem system;
	EXPECT_TRUE(system.IsClimateSystemEnabled());
	EXPECT_TRUE(system.IsStormCreationEnabled());
	system.SetClimateSystemEnabled(false);
	system.SetStormCreationEnabled(false);
	EXPECT_FALSE(climate::IsClimateSystemEnabled());
	EXPECT_FALSE(climate::IsStormCreationEnabled());
	EXPECT_FALSE(system.IsClimateSystemEnabled());
	EXPECT_FALSE(system.IsStormCreationEnabled());
}

TEST_F(WeatherTest, TheWeatherSystemReadsTheStorms)
{
	ecs::systems::WeatherSystem system;
	const auto id = MakeStorm({1000.0f, 0.0f, 1000.0f});
	// the overcast of the atmosphere alone, the cell's corner being the storm's centre: 80 percent
	EXPECT_FLOAT_EQ(system.GetOvercast({1000.0f, 0.0f, 1000.0f}), 0.8f);
	// no lightning has struck
	EXPECT_EQ(system.GetLightningFlash({1000.0f, 0.0f, 1000.0f}), 0);
	// a storm ends when its middle is nearer than the radius and its outer radius
	system.KillStormsInArea({1150.0f, 0.0f, 1000.0f}, 40.0f);
	EXPECT_NE(storms::Find(id), nullptr);
	system.KillStormsInArea({1150.0f, 0.0f, 1000.0f}, 60.0f);
	EXPECT_EQ(storms::Find(id), nullptr);
}

TEST_F(WeatherTest, TheWeatherSystemResetsForANewIsland)
{
	ecs::systems::WeatherSystem system;
	system.CreateClimate(0, 0, {2560.0f, 2560.0f}, 5120.0f, 5120.0f);
	system.SetClimateSystemEnabled(false);
	MakeStorm({1000.0f, 0.0f, 1000.0f});
	system.Reset();
	EXPECT_FALSE(climate::HasWorld());
	EXPECT_TRUE(system.IsClimateSystemEnabled());
	int count = 0;
	storms::ForEach([&count](const storms::Storm&) { ++count; });
	EXPECT_EQ(count, 0);
}

namespace
{
/// A registry for the things of one test, gone with it
struct ScopedRegistry
{
	ScopedRegistry() { Locator::entitiesRegistry::emplace<ecs::Registry>(); }
	~ScopedRegistry() { Locator::entitiesRegistry::reset(); }
	ScopedRegistry(const ScopedRegistry&) = delete;
	ScopedRegistry& operator=(const ScopedRegistry&) = delete;
	ScopedRegistry(ScopedRegistry&&) = delete;
	ScopedRegistry& operator=(ScopedRegistry&&) = delete;
};
} // namespace

TEST_F(WeatherTest, AScriptResizesAWeatherThingsStormAndLetsTheWindCarryIt)
{
	const game_random::testing::ScopedState random;
	const ScopedRegistry scoped;
	auto& registry = Locator::entitiesRegistry::value();
	const auto thing = weather_thing::Create({1000.0f, 0.0f, 1000.0f}, 0);
	ASSERT_TRUE(weather_thing::IsWeather(thing));
	// The inner and outer reach go into the thing's descriptor as they are given, and on to its storm
	weather_thing::SetInnerOuterProperties(thing, 40.0f, 250.0f);
	const auto& kept = registry.Get<const ecs::components::WeatherThing>(thing);
	EXPECT_FLOAT_EQ(kept.descriptor.innerRadius, 40.0f);
	EXPECT_FLOAT_EQ(kept.descriptor.outerRadius, 250.0f);
	const auto* storm = storms::Find(kept.storm);
	ASSERT_NE(storm, nullptr);
	EXPECT_FLOAT_EQ(storm->descriptor.innerRadius, 40.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.outerRadius, 250.0f);
	// The wind does not carry a weather thing until a script says so
	EXPECT_FALSE(kept.affectedByWind);
	weather_thing::SetAffectedByWind(thing, true);
	EXPECT_TRUE(registry.Get<const ecs::components::WeatherThing>(thing).affectedByWind);
	// Anything else is left alone
	const auto other = registry.Create();
	EXPECT_FALSE(weather_thing::IsWeather(other));
	weather_thing::SetInnerOuterProperties(other, 1.0f, 2.0f);
	weather_thing::SetAffectedByWind(other, true);
	EXPECT_FALSE(registry.AllOf<ecs::components::WeatherThing>(other));
}

TEST_F(WeatherTest, AScriptCreatesAWeatherThingOnTheLandUnderItsPoint)
{
	// CREATE of a weather thing as a script calls it: the type, the sub-type, then the point. Its storm stands at the
	// point's map coordinates, on the land there: the height the script gives is dropped
	const game_random::testing::ScopedState random;
	const ScopedRegistry scoped;
	const test::RestoreService<Locator::vm> restoreVm;
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	const test::RestoreService<Locator::scriptObjects> restoreScriptObjects;
	if (!spdlog::get("scripting"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
	}
	auto& vm = Locator::vm::emplace<lhvm::LHVM>();
	Locator::terrainSystem::emplace<test::HeightFieldIsland>([](glm::vec2 xz) { return 20.0f + (xz.x * 0.01f); });
	Locator::scriptObjects::emplace<ecs::systems::ScriptObjectsSystem>();
	auto& registry = Locator::entitiesRegistry::value();
	// entity 0 is the scripts' "no thing"
	registry.Create();
	chlapi::CHLApi api;
	const auto& table = api.GetFunctionsTable();
	const auto create =
	    std::ranges::find_if(table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "CREATE"; });
	ASSERT_NE(create, table.end());
	constexpr int32_t k_WeatherThingType = 15;
	const glm::vec3 given(1000.07f, 300.0f, 1200.03f);
	vm.Pushi(k_WeatherThingType);
	vm.Pushi(2);
	vm.Pushv(given.x);
	vm.Pushv(given.y);
	vm.Pushv(given.z);
	create->impl();
	const auto thing = static_cast<entt::entity>(vm.Pop().uintVal);
	ASSERT_TRUE(weather_thing::IsWeather(thing));
	const float x = map_coords::Quantise(given.x);
	const float z = map_coords::Quantise(given.z);
	const auto& kept = registry.Get<const ecs::components::WeatherThing>(thing);
	EXPECT_EQ(kept.descriptor.position.x, x);
	EXPECT_EQ(kept.descriptor.position.z, z);
	EXPECT_EQ(kept.descriptor.position.y, 20.0f + (x * 0.01f));
}

TEST_F(WeatherTest, AScriptChangesAWeatherThingsCloudsAndTheirStorm)
{
	// CHANGE_CLOUD_PROPERTIES as a script calls it: the thing, the number of clouds, the blackness and the height
	// pushed in that order. The number is cut to a whole one; the thing's descriptor and its storm take all three
	const game_random::testing::ScopedState random;
	const ScopedRegistry scoped;
	const test::RestoreService<Locator::vm> restoreVm;
	if (!spdlog::get("scripting"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
	}
	auto& vm = Locator::vm::emplace<lhvm::LHVM>();
	auto& registry = Locator::entitiesRegistry::value();
	const auto thing = weather_thing::Create({1000.0f, 0.0f, 1000.0f}, 0);
	ASSERT_TRUE(weather_thing::IsWeather(thing));
	vm.Pusho(static_cast<uint32_t>(thing));
	vm.Pushf(5.9f);
	vm.Pushf(0.25f);
	vm.Pushf(320.0f);
	magic::script::ChangeCloudProperties();
	const auto& kept = registry.Get<const ecs::components::WeatherThing>(thing);
	EXPECT_EQ(kept.descriptor.numClouds, 5);
	EXPECT_FLOAT_EQ(kept.descriptor.blackness, 0.25f);
	EXPECT_FLOAT_EQ(kept.descriptor.elevation, 320.0f);
	const auto* storm = storms::Find(kept.storm);
	ASSERT_NE(storm, nullptr);
	EXPECT_EQ(storm->descriptor.numClouds, 5);
	EXPECT_FLOAT_EQ(storm->descriptor.blackness, 0.25f);
	EXPECT_FLOAT_EQ(storm->descriptor.elevation, 320.0f);
	// Anything that is not a weather thing is left alone, its four values taken off the stack all the same
	const auto other = registry.Create();
	vm.Pusho(static_cast<uint32_t>(other));
	vm.Pushf(3.0f);
	vm.Pushf(1.0f);
	vm.Pushf(10.0f);
	magic::script::ChangeCloudProperties();
	EXPECT_FALSE(registry.AllOf<ecs::components::WeatherThing>(other));
	EXPECT_EQ(registry.Get<const ecs::components::WeatherThing>(thing).descriptor.numClouds, 5);
}

TEST_F(WeatherTest, AScriptChangesAWeatherThingsWeatherLightningAndTimes)
{
	// CHANGE_WEATHER_PROPERTIES, CHANGE_LIGHTNING_PROPERTIES and CHANGE_TIME_FADE_PROPERTIES as a script calls them,
	// the thing pushed first. The weather goes in as bytes: the temperature in whole degrees, the rain, snow and
	// overcast as hundredths, cut; the thing's descriptor goes to its storm, whose fork and then sheet lightning waits
	// are drawn again from their ranges
	const game_random::testing::ScopedState random;
	std::vector<float> draws;
	game_random::testing::SetGameRand(nullptr, [&draws](float x) {
		draws.push_back(x);
		return 1.0f;
	});
	const ScopedRegistry scoped;
	const test::RestoreService<Locator::vm> restoreVm;
	if (!spdlog::get("scripting"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
	}
	auto& vm = Locator::vm::emplace<lhvm::LHVM>();
	auto& registry = Locator::entitiesRegistry::value();
	const auto thing = weather_thing::Create({1000.0f, 0.0f, 1000.0f}, 0);
	ASSERT_TRUE(weather_thing::IsWeather(thing));
	const auto& kept = registry.Get<const ecs::components::WeatherThing>(thing);

	vm.Pusho(static_cast<uint32_t>(thing));
	vm.Pushf(12.7f);  // temperature
	vm.Pushf(0.555f); // rain
	vm.Pushf(0.3f);   // snow
	vm.Pushf(1.0f);   // overcast
	vm.Pushf(2.5f);   // how fast the rain falls
	magic::script::ChangeWeatherProperties();
	EXPECT_EQ(kept.descriptor.weather.temperature, 12);
	EXPECT_EQ(kept.descriptor.weather.rain, 55);
	EXPECT_EQ(kept.descriptor.weather.snow, 30);
	EXPECT_EQ(kept.descriptor.weather.overcast, 100);
	EXPECT_FLOAT_EQ(kept.descriptor.fallSpeed, 2.5f);
	const auto* storm = storms::Find(kept.storm);
	ASSERT_NE(storm, nullptr);
	EXPECT_EQ(storm->descriptor.weather.rain, 55);
	EXPECT_FLOAT_EQ(storm->descriptor.fallSpeed, 2.5f);

	draws.clear();
	vm.Pusho(static_cast<uint32_t>(thing));
	vm.Pushf(2.0f); // sheet lightning, least wait
	vm.Pushf(6.0f); // and most
	vm.Pushf(3.0f); // forked, least
	vm.Pushf(9.0f); // and most
	magic::script::ChangeLightningProperties();
	EXPECT_FLOAT_EQ(kept.descriptor.sheetMin, 2.0f);
	EXPECT_FLOAT_EQ(kept.descriptor.sheetMax, 6.0f);
	EXPECT_FLOAT_EQ(kept.descriptor.forkMin, 3.0f);
	EXPECT_FLOAT_EQ(kept.descriptor.forkMax, 9.0f);
	EXPECT_EQ(draws, (std::vector<float> {6.0f, 4.0f}));
	EXPECT_FLOAT_EQ(storm->forkTimer, 1.0f + 3.0f);
	EXPECT_FLOAT_EQ(storm->sheetTimer, 1.0f + 2.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.forkMax, 9.0f);

	vm.Pusho(static_cast<uint32_t>(thing));
	vm.Pushf(50.0f); // how long it lasts
	vm.Pushf(5.0f);  // how long it fades
	magic::script::ChangeTimeFadeProperties();
	EXPECT_FLOAT_EQ(kept.descriptor.lifeTime, 50.0f);
	EXPECT_FLOAT_EQ(kept.descriptor.fadeInTime, 5.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.lifeTime, 50.0f);
	EXPECT_FLOAT_EQ(storm->descriptor.fadeInTime, 5.0f);
}

TEST_F(WeatherTest, AScriptSendsAWeatherThingOffAtAHeadingAndSpeed)
{
	// SET_HEADING_AND_SPEED on a weather thing: the line from it (at the land's height) to the place, as long as the
	// speed, becomes its movement: its wind bytes are the velocity x 8, cut and kept to -127..127, handed to its storm
	const game_random::testing::ScopedState random;
	const ScopedRegistry scoped;
	auto& registry = Locator::entitiesRegistry::value();
	const auto thing = weather_thing::Create({1000.0f, 0.0f, 1000.0f}, 0);
	const auto& kept = registry.Get<const ecs::components::WeatherThing>(thing);
	// (0, 30, 40) is 50 long: at a speed of 5, (0, 3, 4)
	ASSERT_TRUE(magic::script::SetWeatherHeadingAndSpeed(thing, {1000.0f, 30.0f, 1040.0f}, 5.0f));
	EXPECT_EQ(kept.descriptor.weather.windX, 0);
	EXPECT_EQ(kept.descriptor.weather.windZ, 32);
	const auto* storm = storms::Find(kept.storm);
	ASSERT_NE(storm, nullptr);
	EXPECT_EQ(storm->descriptor.weather.windZ, 32);
	// too fast either way: 127 and -127
	ASSERT_TRUE(magic::script::SetWeatherHeadingAndSpeed(thing, {1100.0f, 0.0f, 1000.0f}, 100.0f));
	EXPECT_EQ(kept.descriptor.weather.windX, 127);
	ASSERT_TRUE(magic::script::SetWeatherHeadingAndSpeed(thing, {900.0f, 0.0f, 1000.0f}, 100.0f));
	EXPECT_EQ(kept.descriptor.weather.windX, -127);
	EXPECT_EQ(kept.descriptor.weather.windZ, 0);
	// anything else is not sent off here
	EXPECT_FALSE(magic::script::SetWeatherHeadingAndSpeed(registry.Create(), {1100.0f, 0.0f, 1000.0f}, 5.0f));
}

TEST_F(WeatherTest, AWeatherThingEndsWithItsStormUnlessAScriptHoldsIt)
{
	// Each turn a weather thing whose storm is over forgets it; then, if no script holds it, it is deleted. One a script
	// holds stays, with no storm, so the script can tell it is over. While its storm lasts the thing stands at the
	// storm's x and z as a map position, and the storm takes whether a script controls the thing. The deletion needs the
	// world's lists (the dead list, the map cells), made before the registry and reset after it, as the game does
	const game_random::testing::ScopedState random;
	const test::ScopedWorldSystems world;
	const ScopedRegistry scoped;
	auto& registry = Locator::entitiesRegistry::value();
	const auto loose = weather_thing::Create({1000.0f, 0.0f, 1000.0f}, 0);
	const auto held = weather_thing::Create({3000.0f, 0.0f, 3000.0f}, 0);
	ecs::script_held::AddScriptThing(held, true);
	ecs::script_held::IncrementReference(held);
	ASSERT_TRUE(ecs::script_held::IsInScript(held));
	ASSERT_TRUE(ecs::script_held::IsControlledByScript(held));
	const auto heldStorm = registry.Get<const ecs::components::WeatherThing>(held).storm;
	const auto looseStorm = registry.Get<const ecs::components::WeatherThing>(loose).storm;
	// near the map's corner a float is finer than the map's resolution (1 / 6553.6), so the cut shows
	storms::Find(heldStorm)->descriptor.position = {12.34567f, 0.0f, 2999.987f};

	weather_thing::ProcessWeatherThings();
	EXPECT_TRUE(storms::Find(heldStorm)->controlledByScript);
	EXPECT_FALSE(storms::Find(looseStorm)->controlledByScript);
	const auto& at = registry.Get<const ecs::components::Transform>(held).position;
	EXPECT_EQ(at, glm::vec3(map_coords::ToMetres(map_coords::ToFixed(12.34567f)), 0.0f,
	                        map_coords::ToMetres(map_coords::ToFixed(2999.987f))));
	EXPECT_NE(at.x, 12.34567f);

	// both storms end
	storms::MarkForDeletion(heldStorm);
	storms::MarkForDeletion(looseStorm);
	weather_thing::ProcessWeatherThings();
	EXPECT_FALSE(ecs::IsAvailable(loose));
	ASSERT_TRUE(ecs::IsAvailable(held));
	EXPECT_EQ(registry.Get<const ecs::components::WeatherThing>(held).storm, storms::k_NoStorm);
	EXPECT_EQ(weather_thing::SpeedOf(held), 0.0f);
	// another turn: still there while the script holds it
	weather_thing::ProcessWeatherThings();
	EXPECT_TRUE(ecs::IsAvailable(held));
}

TEST_F(WeatherTest, AScriptSetsAndReadsAPlayersResistanceToWind)
{
	// SET_PLAYER_WIND_RESISTANCE pops the script player, then the value the player keeps as it was given, and pushes
	// nothing; GET_PLAYER_WIND_RESISTANCE pops the script player alone and pushes the value as a boolean (0 for a
	// player that is not there). Script player 1 is the game's first player
	const ScopedRegistry scoped;
	const test::RestoreService<Locator::playerSystem> restorePlayers;
	const test::RestoreService<Locator::vm> restoreVm;
	if (!spdlog::get("scripting"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& players = Locator::playerSystem::emplace<ecs::systems::PlayerSystem>();
	const auto one = registry.Create();
	registry.Assign<ecs::components::Player>(one, ecs::components::Player {.name = PlayerNames::PLAYER_ONE});
	players.AddPlayer(one);
	EXPECT_EQ(ecs::wind_resistance::Get(PlayerNames::PLAYER_ONE), 0u);
	EXPECT_EQ(ecs::wind_resistance::Get(PlayerNames::PLAYER_TWO), 0u);

	auto& vm = Locator::vm::emplace<lhvm::LHVM>();
	chlapi::CHLApi api;
	const auto native = [&api](std::string_view name) {
		const auto& table = api.GetFunctionsTable();
		const auto found = std::ranges::find_if(table, [name](const auto& f) { return f.name == name; });
		EXPECT_NE(found, table.end()) << name;
		return found != table.end() ? found->impl : std::function<void()> {};
	};
	vm.Pushb(true);
	vm.Pushf(1.0f);
	native("SET_PLAYER_WIND_RESISTANCE")();
	EXPECT_EQ(ecs::wind_resistance::Get(PlayerNames::PLAYER_ONE), 1u);
	// a player that is not there keeps nothing
	vm.Pushb(true);
	vm.Pushf(2.0f);
	native("SET_PLAYER_WIND_RESISTANCE")();
	EXPECT_EQ(ecs::wind_resistance::Get(PlayerNames::PLAYER_TWO), 0u);

	vm.Pushf(1.0f);
	native("GET_PLAYER_WIND_RESISTANCE")();
	lhvm::DataType type {};
	const auto answer = vm.Pop(type);
	EXPECT_EQ(type, lhvm::DataType::Boolean);
	EXPECT_EQ(answer.uintVal, 1u);
	vm.Pushf(2.0f);
	native("GET_PLAYER_WIND_RESISTANCE")();
	EXPECT_EQ(vm.Pop().uintVal, 0u);

	// with no body to clear, a throw by the player's hand changes nothing
	ecs::wind_resistance::AfterThrowFromHand(registry.Create(), PlayerNames::PLAYER_ONE);
}

TEST(WeatherCalendar, TheWeatherSystemKeepsTheCalendar)
{
	ecs::systems::WeatherSystem system;
	EXPECT_EQ(system.GetDaysFromStart(0), calendar::GetDaysFromStart(0));
	EXPECT_EQ(system.GetDaysFromStart(123456), calendar::GetDaysFromStart(123456));
	EXPECT_EQ(system.GetSeason(4459), 0u);
	EXPECT_EQ(system.GetSeason(4460), 1u);
}

TEST_F(WeatherTest, AWeatherThingsSpeedIsItsStormsUntilTheStormIsGone)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::weatherSystem> restoreWeather;
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& system = Locator::weatherSystem::emplace<ecs::systems::WeatherSystem>();
	const auto thing = weather_thing::Create({1000.0f, 0.0f, 1000.0f}, 0);
	ASSERT_TRUE(weather_thing::SpeedOf(thing).has_value());
	weather_thing::SetSpeed(thing, 20.0f);
	EXPECT_EQ(weather_thing::SpeedOf(thing), 20.0f);
	// once its storm is gone it has none to set or read
	system.KillStormsInArea({1000.0f, 0.0f, 1000.0f}, 1000.0f);
	weather_thing::SetSpeed(thing, 30.0f);
	EXPECT_EQ(weather_thing::SpeedOf(thing), 0.0f);
	// anything else is no weather thing
	EXPECT_FALSE(weather_thing::SpeedOf(Locator::entitiesRegistry::value().Create()).has_value());
}

TEST_F(WeatherTest, AScriptMovesAWeatherThingsStormTowardsThePoint)
{
	// MOVE_GAME_THING on a weather thing: its storm takes the script's point as the place it drifts to, height and
	// all; the storm then goes there at its speed, and the thing follows it. Once its storm is gone: nothing
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::weatherSystem> restoreWeather;
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& system = Locator::weatherSystem::emplace<ecs::systems::WeatherSystem>();
	const auto thing = weather_thing::Create({1000.0f, 0.0f, 1000.0f}, 0);
	const auto id = Locator::entitiesRegistry::value().Get<ecs::components::WeatherThing>(thing).storm;
	ASSERT_NE(storms::Find(id), nullptr);
	weather_thing::SetTarget(thing, {1100.0f, 25.0f, 1000.0f});
	EXPECT_EQ(storms::Find(id)->target, glm::vec3(1100.0f, 25.0f, 1000.0f));
	// a second at the speed of 1: a metre towards it
	weather_thing::SetSpeed(thing, 1.0f);
	storms::UpdateAll(1.0f);
	EXPECT_FLOAT_EQ(storms::Find(id)->descriptor.position.x, 1001.0f);
	EXPECT_FLOAT_EQ(storms::Find(id)->descriptor.position.z, 1000.0f);
	system.KillStormsInArea({1000.0f, 0.0f, 1000.0f}, 1000.0f);
	weather_thing::SetTarget(thing, {0.0f, 0.0f, 0.0f});
	EXPECT_EQ(storms::Find(id), nullptr);
}

// CREATE_WEATHER_CLIMATE_RAIN sets the four rain values of the climate of the line's number, the newest one of that
// number: the desire to rain, the dry days, the raining days and the flags' low byte. Number 0 is the world climate,
// made if there is none yet; an unknown number does nothing. Land 1's third climate and its rain line, as the original
// (docs/wiki/scripts/map-script-reader.md#arenas-climates-and-waypoints, docs/wiki/world/weather.md#gclimate-the-climates)
TEST_F(WeatherTest, TheLandScriptsClimateRainSetsTheNumberedClimatesRain)
{
	magic::map_script::CreateWeatherClimate(0, 0, {2560.0f, 0.0f, 2560.0f}, 5120.0f, 5120.0f);
	magic::map_script::CreateWeatherClimate(1, 2, {2701.29f, 0.0f, 2567.86f}, 156.666672f, 321.666656f);
	const auto worldBefore = climate::World().rain;
	magic::map_script::CreateWeatherClimateRain(1, 0.630036f, 97, 0, 0);
	const auto* local = climate::Find(1);
	ASSERT_NE(local, nullptr);
	EXPECT_EQ(local->rain.desire, 0.630036f);
	EXPECT_EQ(local->rain.dryDays, 97);
	EXPECT_EQ(local->rain.rainingDays, 0);
	EXPECT_EQ(local->rain.flags, 0);
	// the world's rain stays as it was
	EXPECT_EQ(climate::World().rain.desire, worldBefore.desire);
	EXPECT_EQ(climate::World().rain.rainingDays, worldBefore.rainingDays);

	// only the flags' low byte is kept
	magic::map_script::CreateWeatherClimateRain(1, 0.5f, 2, 3, 0x101);
	EXPECT_EQ(climate::Find(1)->rain.flags, 1);
	EXPECT_EQ(climate::Find(1)->rain.rainingDays, 3);

	// an unknown number: no climate is made and none changes
	size_t climates = 0;
	climate::ForEach([&climates](const climate::Climate&) { ++climates; });
	magic::map_script::CreateWeatherClimateRain(9, 0.9f, 1, 1, 0);
	size_t after = 0;
	climate::ForEach([&after](const climate::Climate&) { ++after; });
	EXPECT_EQ(after, climates);
	EXPECT_EQ(climate::Find(9), nullptr);
	EXPECT_EQ(climate::Find(1)->rain.desire, 0.5f);
}

// With no world climate yet, number 0 makes it and gives it the line's rain, as the original
// (docs/wiki/world/weather.md#gclimate-the-climates)
TEST_F(WeatherTest, TheLandScriptsClimateRainMakesTheWorldClimateWhenThereIsNone)
{
	ASSERT_FALSE(climate::HasWorld());
	magic::map_script::CreateWeatherClimateRain(0, 0.25f, 3, 4, 0);
	ASSERT_TRUE(climate::HasWorld());
	EXPECT_EQ(climate::World().rain.desire, 0.25f);
	EXPECT_EQ(climate::World().rain.dryDays, 3);
	EXPECT_EQ(climate::World().rain.rainingDays, 4);
	EXPECT_EQ(climate::World().rain.flags, 0);
}
