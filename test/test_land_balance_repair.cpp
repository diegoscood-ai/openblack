/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The debug bar's repair of a save openblack wrote before its saves wrote the land balance: such a save holds it as all
// 0, and the repair reads it as the land's script sets it (docs/wiki/engine/saved-games.md#openblack)

#include <cstdint>

#include <array>
#include <bit>
#include <string_view>

#include <gtest/gtest.h>

#include "ECS/SavedGameLoad.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
// The first lines of the shipped Land 3 script, as written there
constexpr std::string_view k_LandThreeStart = "VERSION(2.300000)\n"
                                              "SET_LAND_NUMBER(3)\n"
                                              "LOAD_LANDSCAPE(\".\\DATA\\LANDSCAPE\\Land3.lnd\")\n"
                                              "START_CAMERA_POS(\"3237.21,3157.85\")\n"
                                              "SET_GLOBAL_LAND_BALANCE(4, 1.250000)\n"
                                              "SET_GLOBAL_LAND_BALANCE(5, 1.219512)\n"
                                              "SET_GLOBAL_LAND_BALANCE(6, 2.032520)\n"
                                              "SET_TOWN_INFLUENCE_MULTIPLIER(0.500000)\n";
} // namespace

TEST(LandBalanceRepair, OnlyABalanceOfAllZeroBytesIsUnwritten)
{
	std::array<uint8_t, 32> saved {};
	EXPECT_TRUE(saved_game_load::LandBalanceUnwritten(saved));
	// the original's every save: eight multipliers of 1
	std::array<float, land_balance::k_Count> ones {};
	ones.fill(1.0f);
	EXPECT_FALSE(saved_game_load::LandBalanceUnwritten(std::bit_cast<std::array<uint8_t, 32>>(ones)));
	saved.at(31) = 1;
	EXPECT_FALSE(saved_game_load::LandBalanceUnwritten(saved));
}

TEST(LandBalanceRepair, TheLandsScriptSetsItsNumbersOverTheClearsOnes)
{
	const auto values = saved_game_load::LandBalanceOfScript(k_LandThreeStart);
	const std::array<float, land_balance::k_Count> expected {1.0f, 1.0f, 1.0f, 1.0f, 1.25f, 1.219512f, 2.03252f, 1.0f};
	EXPECT_EQ(values, expected);
}

TEST(LandBalanceRepair, TheLandsScriptIsNamedAsTheSavedLandsFile)
{
	EXPECT_EQ(saved_game_load::LandScriptNameOf(R"(.\DATA\LANDSCAPE\Land3.lnd)"), "Land3.txt");
	EXPECT_EQ(saved_game_load::LandScriptNameOf("Data/Landscape/Land2.lnd"), "Land2.txt");
	EXPECT_EQ(saved_game_load::LandScriptNameOf("Land1.lnd"), "Land1.txt");
}

TEST(LandBalanceRepair, AScriptWithNoBalanceLinesLeavesEveryMultiplierAtOne)
{
	const auto values = saved_game_load::LandBalanceOfScript("VERSION(2.000000)\r\nCREATE_TOWN(0, \"1.0,1.0\", "
	                                                         "\"PLAYER_ONE\", 0, \"NORSE\")\r\n");
	for (const auto value : values)
	{
		EXPECT_EQ(value, 1.0f);
	}
}

TEST(LandBalanceRepair, ANumberOutsideTheEightOrALineMissingItsFractionSetsNothing)
{
	const auto values = saved_game_load::LandBalanceOfScript("SET_GLOBAL_LAND_BALANCE(8, 3.0)\r\n"
	                                                         "SET_GLOBAL_LAND_BALANCE(-1, 3.0)\r\n"
	                                                         "SET_GLOBAL_LAND_BALANCE(2)\r\n"
	                                                         "\"SET_GLOBAL_LAND_BALANCE\"(3, 3.0)\r\n"
	                                                         "SET_GLOBAL_LAND_BALANCE(0, 2.0)\r\n"
	                                                         "SET_GLOBAL_LAND_BALANCE(0, 0.5)\r\n");
	const std::array<float, land_balance::k_Count> expected {0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	EXPECT_EQ(values, expected);
}
