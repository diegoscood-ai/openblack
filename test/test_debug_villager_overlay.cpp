/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The villagers' debug overlay colours each label by the villager's town's owner

#include <optional>

#include <gtest/gtest.h>

#include "Debug/VillagerOverlayColour.h"
#include "Gui/StatisticsBoxRules.h"

using namespace openblack;
using debug::gui::villager_overlay::Colour;

TEST(VillagerOverlay, ALabelTakesItsOwnersColourAsTheLandGivesItAndAFemalesIsLighter)
{
	const auto player = gui::statistics_box::PlayerColour(PlayerNames::PLAYER_TWO, 2);
	EXPECT_EQ(Colour(PlayerNames::PLAYER_TWO, false, 2), glm::vec4(glm::vec3(player), 1.0f));
	// 144/255 of the way to white
	const auto female = Colour(PlayerNames::PLAYER_ONE, true, 0);
	const auto male = Colour(PlayerNames::PLAYER_ONE, false, 0);
	EXPECT_FLOAT_EQ(female.g, male.g + (1.0f - male.g) * 144.0f / 255.0f);
	EXPECT_FLOAT_EQ(female.a, 1.0f);
	// no town, or a neutral one, is white
	EXPECT_EQ(Colour(std::nullopt, false, 1), glm::vec4(1.0f));
	EXPECT_EQ(Colour(PlayerNames::NEUTRAL, true, 1), glm::vec4(1.0f));
}
