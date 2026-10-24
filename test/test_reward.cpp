/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <numbers>

#include <gtest/gtest.h>

#include "ECS/RewardRules.h"

using namespace openblack;
using namespace openblack::ecs;

TEST(Reward, AChestFromTheSkyFallsAHundredAndFiftyMetresInThreeSecondsTurningHalfARoundASecond)
{
	EXPECT_FLOAT_EQ(reward::FallHeight(0.0f), 150.0f);
	EXPECT_FLOAT_EQ(reward::FallHeight(1.0f), 100.0f);
	EXPECT_FLOAT_EQ(reward::FallHeight(3.0f), 0.0f);
	EXPECT_FLOAT_EQ(reward::FallYaw(1.0f), std::numbers::pi_v<float>);
}

TEST(Reward, ItsDustSpreadsAboutItInSandColoursAndGrowsAsItFades)
{
	const auto dust = reward::MakeDust([](float a, float b) { return b + 0.0f * a; });
	EXPECT_EQ(dust.size(), 27u);
	EXPECT_FLOAT_EQ(dust[0].offset.x, 2.0f);
	EXPECT_FLOAT_EQ(dust[0].offset.y, 2.0f);
	EXPECT_EQ(dust[0].frame, 15u);
	EXPECT_EQ(dust[0].rgb, (std::array<uint8_t, 3> {212, 180, 140}));
	const auto fresh = reward::LookOfDust(2500.0f);
	EXPECT_EQ(fresh.alpha, 255);
	EXPECT_FLOAT_EQ(fresh.size, 0.7f);
	const auto gone = reward::LookOfDust(0.0f);
	EXPECT_EQ(gone.alpha, 0);
	EXPECT_FLOAT_EQ(gone.size, 2.5f);
}

TEST(Reward, ItsDustIsPlacedAcrossThenUpThenAlong)
{
	// Each draw is told apart by its order
	float next = 0.0f;
	const auto dust = reward::MakeDust([&next](float /*from*/, float /*to*/) { return next += 1.0f; });
	EXPECT_FLOAT_EQ(dust[0].offset.z, 1.0f);
	EXPECT_FLOAT_EQ(dust[0].offset.y, 2.0f);
	EXPECT_FLOAT_EQ(dust[0].offset.x, 3.0f);
}

TEST(Reward, ItsDustShowsOnTheFrameItLandsThenOnceItIsInTheMap)
{
	// The frame it lands, then not until the next turn puts it in the map's cells
	EXPECT_TRUE(reward::DrawsDust(true, 2500.0f, false));
	EXPECT_FALSE(reward::DrawsDust(false, 2500.0f, false));
	EXPECT_TRUE(reward::DrawsDust(false, 2400.0f, true));
	// Not once its time is up, nor for a chest that was put on the land
	EXPECT_FALSE(reward::DrawsDust(false, 0.0f, true));
	EXPECT_FALSE(reward::DrawsDust(true, 0.0f, false));
}

TEST(Reward, ItsDustsTimeGoesDownByTheFramesWholeMilliseconds)
{
	EXPECT_FLOAT_EQ(reward::DustAfterFrame(2500.0f, 16.6f), 2484.0f);
	EXPECT_FLOAT_EQ(reward::DustAfterFrame(10.0f, 16.0f), 0.0f);
}
