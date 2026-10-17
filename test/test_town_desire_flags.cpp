/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <numbers>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <gtest/gtest.h>

#include "3D/Billboard.h"
#include "ECS/TownDesireFlags.h"
#include "Enums.h"

namespace flags = openblack::ecs::town_desire_flags;
using openblack::TownDesireInfo;

namespace
{
constexpr float k_ShowsAfter = 0.1f;
} // namespace

TEST(TownDesireFlags, AFlagFliesAtTheStoresPointAfterTheFirstTwelve)
{
	EXPECT_EQ(flags::PointOf(0), 12u);
	EXPECT_EQ(flags::PointOf(5), 17u);
	EXPECT_FALSE(flags::PointOf(-1).has_value());
}

TEST(TownDesireFlags, AFlagShowsOnlyOverAWorkingStoreOnceItIsUpFarEnough)
{
	EXPECT_TRUE(flags::Shows(true, 0.1f, k_ShowsAfter));
	EXPECT_TRUE(flags::Shows(true, 0.5f, k_ShowsAfter));
	EXPECT_FALSE(flags::Shows(true, 0.09f, k_ShowsAfter));
	EXPECT_FALSE(flags::Shows(false, 0.5f, k_ShowsAfter));
}

TEST(TownDesireFlags, AHiddenFlagIsPutStraightAtItsDesireEvenPastWhole)
{
	openblack::Zoomer next {.value = 0.3f, .speed = 0.7f};
	flags::Step(next, false, 1.6f, 0.016f);
	EXPECT_FLOAT_EQ(next.value, 1.6f);
	EXPECT_FLOAT_EQ(next.speed, 0.0f);
}

TEST(TownDesireFlags, AShownFlagEasesUpToItsDesireNeverPastWhole)
{
	openblack::Zoomer level;
	level.SetPosition(0.2f);
	// A step of a frame moves it a little only, setting off from rest
	auto first = level;
	flags::Step(first, true, 0.8f, 0.02f);
	EXPECT_GT(first.value, 0.2f);
	EXPECT_LT(first.value, 0.21f);
	EXPECT_GT(first.speed, 0.0f);
	// Frame after frame it comes to rest at its desire, held to wholly wanted
	for (int i = 0; i < 1000; ++i)
	{
		flags::Step(level, true, 3.0f, 0.02f);
	}
	EXPECT_NEAR(level.value, 1.0f, 1e-4f);
	EXPECT_NEAR(level.speed, 0.0f, 1e-3f);
	// A frame of a second or more puts it there at once
	openblack::Zoomer jump;
	jump.SetPosition(0.2f);
	flags::Step(jump, true, 0.6f, 1.0f);
	EXPECT_FLOAT_EQ(jump.value, 0.6f);
	EXPECT_FLOAT_EQ(jump.speed, 0.0f);
}

TEST(TownDesireFlags, AFlagRisesFourMetresAndTwoFifthsWhenWhollyWanted)
{
	EXPECT_FLOAT_EQ(flags::Rise(0.0f), 0.0f);
	EXPECT_FLOAT_EQ(flags::Rise(1.0f), 4.4f);
	EXPECT_FLOAT_EQ(flags::Rise(0.5f), 2.2f);
}

TEST(TownDesireFlags, AFlagGrowsFromNothingAsItFirstShows)
{
	// Its whole size from a fifth of the room above its first showing: 0.18 for a desire that shows from 0.1
	EXPECT_FLOAT_EQ(flags::DrawnScale(0.1f, k_ShowsAfter), 0.0f);
	EXPECT_NEAR(flags::DrawnScale(0.14f, k_ShowsAfter), 0.5f, 1e-5f);
	EXPECT_FLOAT_EQ(flags::DrawnScale(0.18f, k_ShowsAfter), 1.0f);
	EXPECT_FLOAT_EQ(flags::DrawnScale(0.9f, k_ShowsAfter), 1.0f);
}

TEST(TownDesireFlags, AFlagTurnsAQuarterPastTheWayToTheCamera)
{
	// The inline billboards' turn (billboard::YawToEyeAngle), as his own rule had it
	const glm::vec3 flag {10.0f, 0.0f, 10.0f};
	EXPECT_NEAR(openblack::graphics::billboard::YawToEyeAngle(flag, {20.0f, 5.0f, 10.0f}), std::numbers::pi_v<float> / 2.0f,
	            1e-6f);
	EXPECT_NEAR(openblack::graphics::billboard::YawToEyeAngle(flag, {10.0f, 5.0f, 20.0f}), std::numbers::pi_v<float>, 1e-6f);
}

TEST(TownDesireFlags, AFlagFacesTheCameraAsHisTurnHadIt)
{
	// His flags turned by glm::eulerAngleY(-angle); ours by the billboard's own turn: the same axes, the flag's +Z
	// pointing from the camera to the flag across the land
	const glm::vec3 flag {10.0f, 0.0f, 10.0f};
	for (const auto eye : {glm::vec3 {20.0f, 5.0f, 10.0f}, glm::vec3 {10.0f, 5.0f, 20.0f}, glm::vec3 {3.0f, 9.0f, -4.0f}})
	{
		const auto ours = openblack::graphics::billboard::YawToEye(flag, eye);
		const auto his = glm::mat3(glm::eulerAngleY(-openblack::graphics::billboard::YawToEyeAngle(flag, eye)));
		for (int column = 0; column < 3; ++column)
		{
			for (int row = 0; row < 3; ++row)
			{
				EXPECT_NEAR(ours[column][row], his[column][row], 1e-6f) << column << row;
			}
		}
		const auto forward = ours * glm::vec3(0.0f, 0.0f, 1.0f);
		const auto away = glm::normalize(glm::vec3(flag.x - eye.x, 0.0f, flag.z - eye.z));
		EXPECT_NEAR(forward.x, away.x, 1e-5f);
		EXPECT_NEAR(forward.z, away.z, 1e-5f);
	}
}

TEST(TownDesireFlags, OnlyPointsInsideTheViewAreInView)
{
	EXPECT_TRUE(flags::InView({0.5f, -0.5f, 0.5f, 1.0f}));
	EXPECT_FALSE(flags::InView({1.5f, 0.0f, 0.5f, 1.0f}));
	EXPECT_FALSE(flags::InView({0.0f, 0.0f, 0.5f, -1.0f}));
}

TEST(TownDesireFlags, EachFlagShowsItsOwnWordsOverTheNeed)
{
	EXPECT_EQ(flags::ToolTipWords(TownDesireInfo::ForFood), 91u);
	EXPECT_EQ(flags::ToolTipWords(TownDesireInfo::ForWood), 92u);
	EXPECT_EQ(flags::ToolTipWords(TownDesireInfo::ForProtection), 94u);
	EXPECT_EQ(flags::ToolTipWords(TownDesireInfo::ForMercy), 95u);
	EXPECT_EQ(flags::ToolTipWords(TownDesireInfo::ForAbodes), 96u);
	EXPECT_EQ(flags::ToolTipWords(TownDesireInfo::ForCivicBuilding), 97u);
	EXPECT_EQ(flags::ToolTipWords(TownDesireInfo::ForChildren), 99u);
	EXPECT_FALSE(flags::ToolTipWords(TownDesireInfo::ForPlaytime).has_value());
	EXPECT_FALSE(flags::ToolTipWords(TownDesireInfo::ForSleep).has_value());
}

TEST(TownDesireFlags, TheNeedShownIsTheDesireInHundredthsHeldToWhole)
{
	EXPECT_FLOAT_EQ(flags::NeedPercent(0.42f), 42.0f);
	EXPECT_FLOAT_EQ(flags::NeedPercent(1.7f), 100.0f);
	EXPECT_FLOAT_EQ(flags::NeedPercent(-0.2f), -20.0f);
}
