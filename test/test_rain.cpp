/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/Rain.h"

using namespace openblack;

namespace
{
/// Hands out its numbers in turn and keeps the ranges it was asked for
struct FakeRandom
{
	std::vector<float> values;
	std::vector<std::pair<float, float>> asked;
	size_t next {0};

	rain::Random Get()
	{
		return [this](float a, float b) {
			asked.emplace_back(a, b);
			return values.at(next++ % values.size());
		};
	}
};
} // namespace

TEST(Rain, PlacedInTheGamesOrder)
{
	FakeRandom random {.values = {10.0f, -20.0f, 3.0f, -4.0f, 0.25f, 0.15f, 0.5f}};
	const auto streak = rain::Place(random.Get());
	EXPECT_FLOAT_EQ(streak.x, 5.0f);
	EXPECT_FLOAT_EQ(streak.z, -10.0f);
	EXPECT_FLOAT_EQ(streak.dx, 3.0f);
	EXPECT_FLOAT_EQ(streak.dz, -4.0f);
	EXPECT_FLOAT_EQ(streak.scroll, 0.25f);
	EXPECT_FLOAT_EQ(streak.speed, 0.15f);
	EXPECT_FLOAT_EQ(streak.phase, 0.5f);
	ASSERT_EQ(random.asked.size(), 7u);
	EXPECT_EQ(random.asked[0], std::make_pair(-160.0f, 160.0f));
	EXPECT_EQ(random.asked[2], std::make_pair(-15.0f, 15.0f));
	EXPECT_EQ(random.asked[5], std::make_pair(0.1f, 0.2f));
}

TEST(Rain, StepsAndStartsAgain)
{
	FakeRandom random {.values = {1.0f, 2.0f, 30.0f, 40.0f}};
	std::array<rain::Streak, 2> streaks {};
	streaks[0] = {.phase = 0.5f, .x = 7.0f, .scroll = 0.9f, .speed = 0.2f};
	streaks[1] = {.phase = 0.9f, .x = 7.0f, .scroll = 0.1f, .speed = 0.1f};
	rain::Step(streaks, 0.1f, 10.0f, random.Get());
	// 0.9 + 10 x 0.2 x 0.1 wraps to 0.1; its life moves on by 0.24
	EXPECT_NEAR(streaks[0].scroll, 0.1f, 1e-5f);
	EXPECT_NEAR(streaks[0].phase, 0.74f, 1e-5f);
	EXPECT_FLOAT_EQ(streaks[0].x, 7.0f);
	// The second's life ends: it starts again with a new slant and place
	EXPECT_NEAR(streaks[1].phase, 0.14f, 1e-5f);
	EXPECT_FLOAT_EQ(streaks[1].dx, 1.0f);
	EXPECT_FLOAT_EQ(streaks[1].dz, 2.0f);
	EXPECT_FLOAT_EQ(streaks[1].x, 15.0f);
	EXPECT_FLOAT_EQ(streaks[1].z, 20.0f);
	EXPECT_EQ(random.asked.size(), 4u);
}

TEST(Rain, FollowsTheNearestStorm)
{
	const auto step = rain::Follow({}, rain::Fall {.height = 500.0f, .speed = 2.0f});
	EXPECT_FLOAT_EQ(step.height, 160.0f + (340.0f * 0.3f));
	EXPECT_FLOAT_EQ(step.speed, 1.3f);
	// Back towards the calm air without one, within bounds
	EXPECT_EQ(rain::Follow({.height = 10.0f, .speed = 0.0f}, std::nullopt), (rain::Fall {.height = 55.0f, .speed = 0.3f}));
	EXPECT_EQ(rain::Follow({.height = 10.0f, .speed = 0.0f}, rain::Fall {.height = 0.0f, .speed = 0.0f}),
	          (rain::Fall {.height = 40.0f, .speed = 0.3f}));
	EXPECT_EQ(rain::Follow({.height = 2000.0f, .speed = 20.0f}, rain::Fall {.height = 2000.0f, .speed = 20.0f}),
	          (rain::Fall {.height = 640.0f, .speed = 5.0f}));
}

TEST(Rain, StreaksFadeInAndOut)
{
	EXPECT_FLOAT_EQ(rain::PhaseFade(0.025f), 0.5f);
	EXPECT_FLOAT_EQ(rain::PhaseFade(0.5f), 1.0f);
	EXPECT_NEAR(rain::PhaseFade(0.975f), 0.5f, 1e-5f);
	const auto ends = rain::Ends({.x = 1.0f, .z = 2.0f, .dx = 3.0f, .dz = -4.0f}, 160.0f);
	EXPECT_EQ(ends[0], glm::vec3(1.0f, -50.0f, 2.0f));
	EXPECT_EQ(ends[1], glm::vec3(4.0f, 160.0f, -2.0f));
}

TEST(Rain, NoFrameInsideTheTempleTakesTheRain)
{
	// Inside the temple the game draws the temple alone: its atmosphere update runs, but the rain is not drawn, so its
	// streaks do not move on. Outside, a frame that draws the world's things takes the tiles
	EXPECT_TRUE(rain::FrameTakesTiles(true, false));
	EXPECT_FALSE(rain::FrameTakesTiles(true, true));
	EXPECT_FALSE(rain::FrameTakesTiles(false, false));
	EXPECT_FALSE(rain::FrameTakesTiles(false, true));
}

TEST(Rain, StrengthIsEightyEightOfAHundredInWholeSteps)
{
	EXPECT_EQ(rain::Strength(6), 5);
	EXPECT_EQ(rain::Strength(50), 44);
	EXPECT_EQ(rain::Strength(52), 45);
	EXPECT_EQ(rain::Strength(100), 88);
	EXPECT_EQ(rain::Strength(127), 111);
}

TEST(Rain, OnlyStrongRainSplashes)
{
	EXPECT_FALSE(rain::SplashAlpha(0).has_value());
	EXPECT_FALSE(rain::SplashAlpha(44).has_value());
	// 255 for each 88 of strength, in whole steps, at most 255
	EXPECT_EQ(rain::SplashAlpha(45), 130);
	EXPECT_EQ(rain::SplashAlpha(87), 252);
	EXPECT_EQ(rain::SplashAlpha(88), 255);
	EXPECT_EQ(rain::SplashAlpha(255), 255);
}

TEST(Rain, SplashFallsOnItsBlockInTheGamesOrder)
{
	FakeRandom random {.values = {10.0f, 150.0f, 0.25f}};
	std::vector<glm::vec2> askedHeights;
	const rain::HeightAt heightAt = [&askedHeights](glm::vec2 at) {
		askedHeights.push_back(at);
		return 3.5f;
	};
	const auto ring = rain::Splash({1040.0f, 2000.0f}, 0x82C0C0C0u, random.Get(), heightAt);
	// z first, then x, each anywhere on the 160 wide block about its centre, then the size
	ASSERT_EQ(random.asked.size(), 3u);
	EXPECT_EQ(random.asked[0], std::make_pair(0.0f, 160.0f));
	EXPECT_EQ(random.asked[1], std::make_pair(0.0f, 160.0f));
	EXPECT_EQ(random.asked[2], std::make_pair(0.0f, 0.3f));
	EXPECT_FLOAT_EQ(ring.position.z, 1930.0f);
	EXPECT_FLOAT_EQ(ring.position.x, 1110.0f);
	ASSERT_EQ(askedHeights.size(), 1u);
	EXPECT_EQ(askedHeights[0], glm::vec2(1110.0f, 1930.0f));
	// one unit above the ground, on land or sea alike
	EXPECT_FLOAT_EQ(ring.position.y, 4.5f);
	EXPECT_FLOAT_EQ(ring.growth, 0.75f);
	EXPECT_EQ(ring.age, 0u);
	EXPECT_FLOAT_EQ(ring.angle, 0.0f);
	EXPECT_FLOAT_EQ(ring.aspect, 1.0f);
	EXPECT_FLOAT_EQ(ring.rate, 1.0f);
	EXPECT_EQ(ring.cell, 0x30);
	EXPECT_EQ(ring.argb, 0x82C0C0C0u);
	EXPECT_FALSE(ring.seaLight);
}

TEST(Rain, StreaksShowByTheStrengthNotTheMostRain)
{
	// 6 % of rain is a block's least that is looked at, but its strength of 5 shows no streaks
	EXPECT_FALSE(rain::ShowsStreaks(rain::Strength(6)));
	EXPECT_TRUE(rain::ShowsStreaks(rain::Strength(7)));
	EXPECT_FALSE(rain::ShowsStreaks(5));
	EXPECT_TRUE(rain::ShowsStreaks(6));
}

TEST(Rain, AQueuedBlockCountsWhateverItShows)
{
	// 6 % of rain is queued, and counts the rain as drawn, though its strength of 5 shows no streaks and is far too
	// little to splash
	EXPECT_FALSE(rain::Queued(5));
	EXPECT_TRUE(rain::Queued(6));
	EXPECT_FALSE(rain::ShowsStreaks(rain::Strength(6)));
	EXPECT_FALSE(rain::SplashAlpha(rain::Strength(6)).has_value());
}
