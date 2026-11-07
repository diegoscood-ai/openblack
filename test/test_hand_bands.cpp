/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The power-up bands on the player's hand (magic::hand_bands), against the original's values in
// docs/wiki/magic/hand-casting.md#the-hand and docs/wiki/project/progress/hand/hand_effects_and_glows.md

#include <cmath>

#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "Magic/Hand/HandBandRules.h"

using namespace openblack;
namespace hb = openblack::magic::hand_bands;

TEST(HandBands, FiveBandsFlyOntoTheHandATenthOfASecondApart)
{
	EXPECT_EQ(hb::k_FlyInBands, 5);
	EXPECT_EQ(hb::FlyInStart(1, false), 1.0f * 0.1f);
	EXPECT_EQ(hb::FlyInStart(2, false), 2.0f * 0.1f);
	EXPECT_EQ(hb::FlyInStart(5, false), 5.0f * 0.1f);
	EXPECT_NEAR(hb::FlyInStart(3, false) - hb::FlyInStart(2, false), 0.1f, 1e-6f);
}

TEST(HandBands, TheBandsOfAMiracleStillSettlingStartTwoPointFourSecondsLater)
{
	EXPECT_EQ(hb::k_DelayedStart, 2.4f);
	EXPECT_EQ(hb::FlyInStart(1, true), 1.0f * 0.1f + 2.4f);
	EXPECT_EQ(hb::FlyInStart(5, true), 5.0f * 0.1f + 2.4f);
	// a power-up level's bands too
	EXPECT_EQ(hb::PermanentStart(false), 0.0f);
	EXPECT_EQ(hb::PermanentStart(true), 2.4f);
}

TEST(HandBands, EachFurtherBraceletSitsHigherUpTheArmAndSpinsFaster)
{
	// 10 + 40 x its place along the forearm
	EXPECT_EQ(hb::HeightAlongArm(0), 10.0f);
	EXPECT_EQ(hb::HeightAlongArm(1), 50.0f);
	EXPECT_EQ(hb::HeightAlongArm(5), 210.0f);
	// (1 + 0.2 x its place) x 12 radians a second
	EXPECT_EQ(hb::NextSpinAngle(0.0f, 0, 0.1f), std::fmod(1.0f * 12.0f * 0.1f, glm::two_pi<float>()));
	EXPECT_FLOAT_EQ(hb::NextSpinAngle(0.0f, 0, 0.25f), 3.0f);
	EXPECT_FLOAT_EQ(hb::NextSpinAngle(0.0f, 5, 0.25f), 6.0f);
	EXPECT_GT(hb::NextSpinAngle(0.0f, 2, 0.1f), hb::NextSpinAngle(0.0f, 1, 0.1f));
	// wrapped at a turn
	EXPECT_FLOAT_EQ(hb::NextSpinAngle(6.0f, 0, 0.1f), 6.0f + 1.2f - glm::two_pi<float>());
}

TEST(HandBands, BandsStandStillWhileThePauseGivesNoGameTime)
{
	for (const float angle : {0.0f, 1.5f, 6.0f})
	{
		for (const int index : {0, 1, 4})
		{
			EXPECT_EQ(hb::NextSpinAngle(angle, index, 0.0f), angle) << angle << " " << index;
		}
	}
}

TEST(HandBands, ShakingTheMiracleOffSendsOneBandBackOverASecond)
{
	EXPECT_EQ(hb::k_ShakeOffSeconds, 1.0f);
	EXPECT_EQ(hb::k_ShakeOffAlpha0, 5);
	EXPECT_EQ(hb::k_ShakeOffAlpha1, 50);
}

TEST(HandBands, AChargePulsesMoreOftenAsItFills)
{
	// one each time the timer passes lerp(6, 0.3, charge) seconds; reaching it exactly is not enough
	EXPECT_FALSE(hb::ChargeBandDue(false, 5.99f, 0.0f));
	EXPECT_FALSE(hb::ChargeBandDue(false, 6.0f, 0.0f));
	EXPECT_TRUE(hb::ChargeBandDue(false, 6.01f, 0.0f));
	EXPECT_FALSE(hb::ChargeBandDue(false, 0.29f, 1.0f));
	EXPECT_FALSE(hb::ChargeBandDue(false, 6.0f + (0.3f - 6.0f) * 1.0f, 1.0f));
	EXPECT_TRUE(hb::ChargeBandDue(false, 0.31f, 1.0f));
	EXPECT_FALSE(hb::ChargeBandDue(false, 6.0f + (0.3f - 6.0f) * 0.5f, 0.5f));
	EXPECT_TRUE(hb::ChargeBandDue(false, 3.16f, 0.5f));
	EXPECT_FALSE(hb::ChargeBandDue(false, 3.1f, 0.5f));
	// each lasts lerp(3.5, 1, charge) seconds
	EXPECT_EQ(hb::ChargeBandSeconds(0.0f), 3.5f);
	EXPECT_EQ(hb::ChargeBandSeconds(1.0f), 1.0f);
	EXPECT_EQ(hb::ChargeBandSeconds(0.5f), 2.25f);
	// its alpha from lerp(3, 5, charge) to lerp(15, 50, charge), truncated
	EXPECT_EQ(hb::ChargeBandAlpha0(0.0f), 3);
	EXPECT_EQ(hb::ChargeBandAlpha0(0.99f), 4);
	EXPECT_EQ(hb::ChargeBandAlpha0(1.0f), 5);
	EXPECT_EQ(hb::ChargeBandAlpha1(0.0f), 15);
	EXPECT_EQ(hb::ChargeBandAlpha1(0.5f), 32);
	EXPECT_EQ(hb::ChargeBandAlpha1(1.0f), 50);
}

TEST(HandBands, TheFirstPulseComesOnTheFrameChargingStarts)
{
	// whatever the timer and the charge
	EXPECT_TRUE(hb::ChargeBandDue(true, 0.0f, 0.0f));
	EXPECT_TRUE(hb::ChargeBandDue(true, 0.0f, 1.0f));
	EXPECT_FALSE(hb::ChargeBandDue(false, 0.0f, 0.0f));
}
