/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>

#include <algorithm>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "3D/Rain.h"
#include "3D/Snowfall.h"
#include "Common/GameRandom.h"
#include "ECS/Systems/Implementations/RainSystem.h"
#include "ECS/Systems/Implementations/SnowfallSystem.h"
#include "ECS/Weather/WeatherLoop.h"
#include "Locator.h"

using namespace openblack;

namespace
{
/// One stream of numbers shared by the snow and the rain, as the CRT stream is: it records who drew each number, and
/// hands out the middle of each range
struct Stream
{
	std::vector<char> drawnBy;

	snowfall::Random For(char who)
	{
		return [this, who](float a, float b) {
			drawnBy.push_back(who);
			return (a + b) * 0.5f;
		};
	}
};

std::vector<snowfall::Tile> Snowing(bool snows)
{
	return snows ? std::vector<snowfall::Tile> {{.corner = {80.0f, 80.0f}, .flakes = 256, .alpha = 255}}
	             : std::vector<snowfall::Tile> {};
}

/// A rain that only counts how often its streaks are placed
class CountingRain final: public ecs::systems::RainSystemInterface
{
public:
	explicit CountingRain(int& resets)
	    : _resets(resets)
	{
	}

	void Reset() override { ++_resets; }
	void Update(float /*seconds*/, const glm::vec3& /*camera*/) override {}
	[[nodiscard]] std::vector<rain::Tile> TakeTiles(const glm::vec3& /*camera*/) override { return {}; }
	[[nodiscard]] std::span<const rain::Streak> GetStreaks() const override { return {}; }
	[[nodiscard]] float GetHeight() const override { return 160.0f; }
	[[nodiscard]] rain::Fall GetFall() const override { return {}; }

private:
	int& _resets;
};

constexpr size_t k_SnowDraws = 2 * 14 * snowfall::k_Flakes;
constexpr size_t k_RainDraws = 2 * 7 * rain::k_Streaks;
static_assert(k_SnowDraws == 7168);
static_assert(k_RainDraws == 1792);
} // namespace

TEST(AtmosStart, ScattersTheSnowTwiceThenPlacesTheRainTwice)
{
	Stream stream;
	ecs::systems::SnowfallSystem flakes(stream.For('s'), [](const glm::vec3&) { return Snowing(false); });
	ecs::systems::RainSystem streaks(stream.For('r'));
	weather::StartAtmos(true, flakes, streaks);
	ASSERT_EQ(stream.drawnBy.size(), k_SnowDraws + k_RainDraws);
	const auto firstRain = stream.drawnBy.begin() + static_cast<std::ptrdiff_t>(k_SnowDraws);
	EXPECT_EQ(static_cast<size_t>(std::count(stream.drawnBy.begin(), firstRain, 's')), k_SnowDraws);
	EXPECT_EQ(static_cast<size_t>(std::count(firstRain, stream.drawnBy.end(), 'r')), k_RainDraws);
	// every flake and streak placed
	EXPECT_FLOAT_EQ(flakes.GetFlakes().back().fall, 7.5f);
	EXPECT_FLOAT_EQ(streaks.GetStreaks().back().speed, 0.15f);
}

TEST(AtmosStart, NothingWithTheWeatherSettingOff)
{
	Stream stream;
	ecs::systems::SnowfallSystem flakes(stream.For('s'), [](const glm::vec3&) { return Snowing(false); });
	ecs::systems::RainSystem streaks(stream.For('r'));
	weather::StartAtmos(false, flakes, streaks);
	EXPECT_TRUE(stream.drawnBy.empty());
}

TEST(AtmosStart, ALandsLoadDrawsNoAtmosNumbers)
{
	int resets = 0;
	Locator::rainSystem::emplace<CountingRain>(resets);
	const auto seed = game_random::crt::Seed();
	weather::OnLoadMap();
	EXPECT_EQ(resets, 0);
	// no number drawn from the CRT stream at all
	EXPECT_EQ(game_random::crt::Seed(), seed);
}

TEST(AtmosStart, AfterTheStartTheFlakesAndStreaksStepAsBefore)
{
	Stream stream;
	bool snows = false;
	ecs::systems::SnowfallSystem flakes(stream.For('s'), [&snows](const glm::vec3&) { return Snowing(snows); });
	ecs::systems::RainSystem streaks(stream.For('r'));
	weather::StartAtmos(true, flakes, streaks);
	const auto started = stream.drawnBy.size();
	const auto y = flakes.GetFlakes().front().position.y;
	// No snow drawn and no rain drawn: no numbers and nothing moves
	flakes.Update(0.5f, 1.0f, 160.0f);
	streaks.Update(0.5f, {0.0f, 50.0f, 0.0f});
	EXPECT_EQ(stream.drawnBy.size(), started);
	EXPECT_FLOAT_EQ(flakes.GetFlakes().front().position.y, y);
	// Snow drawn: no scatter on drawing, and the flakes fall at the next update
	snows = true;
	EXPECT_EQ(flakes.TakeTiles({0.0f, 50.0f, 0.0f}).size(), 1u);
	EXPECT_EQ(stream.drawnBy.size(), started);
	flakes.Update(0.5f, 1.0f, 160.0f);
	EXPECT_LT(flakes.GetFlakes().front().position.y, y);
}
