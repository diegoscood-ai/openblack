/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>
#include <numbers>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <gtest/gtest.h>

#include "3D/Billboard.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
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

TEST(TownDesireFlags, AFlagIsMadeFiveMetresFromItsTownAtItsPlacesShareOfTheCircle)
{
	namespace map_coords = openblack::map_coords;
	namespace gutils = openblack::gutils;
	// the step is the float the game turns by, a seventeenth of a circle
	EXPECT_EQ(std::bit_cast<uint32_t>(flags::k_StartAngleStep), 0x3EBD3C19u);
	const map_coords::MapCoords town {map_coords::ToFixed(1234.5f), map_coords::ToFixed(876.25f), 0.0f};
	// place 0 is due along x
	const auto first = flags::StartPlace(town, 0);
	EXPECT_EQ(first.x, town.x + map_coords::ToFixedGUtils(5.0f));
	EXPECT_EQ(first.z, town.z);
	// the others turn by the step times their place, rounded to a float once
	for (const int32_t place : {1, 2, 3, 4, 5})
	{
		const auto angle = static_cast<float>(static_cast<double>(place) * static_cast<double>(flags::k_StartAngleStep));
		const auto offset = gutils::GetPosFromAngle(angle, 5.0f);
		const auto at = flags::StartPlace(town, place);
		EXPECT_EQ(at.x, town.x + offset.x) << place;
		EXPECT_EQ(at.z, town.z + offset.z) << place;
	}
}

TEST(TownDesireFlags, AFlagShowsOnlyOverAWorkingStoreOnceItIsUpFarEnough)
{
	EXPECT_TRUE(flags::Shows(true, 0.1f, k_ShowsAfter));
	EXPECT_TRUE(flags::Shows(true, 0.5f, k_ShowsAfter));
	EXPECT_FALSE(flags::Shows(true, 0.09f, k_ShowsAfter));
	EXPECT_FALSE(flags::Shows(false, 0.5f, k_ShowsAfter));
}

TEST(TownDesireFlags, AShownFrameHoldsTheDesireItFliesForToWholeAndAHiddenOneKeepsIt)
{
	// The town's turn gives a flag its raw desire with the boosts, past whole or not; a frame that shows the flag keeps
	// it held to whole from then on, until the next turn
	EXPECT_FLOAT_EQ(flags::DesireAfterFrame(true, 1.6f), 1.0f);
	EXPECT_FLOAT_EQ(flags::DesireAfterFrame(true, 0.4f), 0.4f);
	EXPECT_FLOAT_EQ(flags::DesireAfterFrame(false, 1.6f), 1.6f);
	EXPECT_FLOAT_EQ(flags::DesireAfterFrame(false, -0.3f), -0.3f);
	// so a flag that shows and then hides before the next turn is put at the held desire, not the town's
	openblack::Zoomer level;
	const float desire = flags::DesireAfterFrame(true, 1.6f);
	flags::Step(level, false, flags::DesireAfterFrame(false, desire), 0.016f);
	EXPECT_FLOAT_EQ(level.value, 1.0f);
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
