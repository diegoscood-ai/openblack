/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A model drawn as a special light, as the cone of light out of an opened reward chest: its colour, how its texture
// turns and which of its vertices are solid
// (docs/wiki/scripts/map-script-objects.md#reward-chests-create_reward-239-create_reward_in_town-240)

#include <cmath>
#include <cstdint>

#include <numbers>
#include <vector>

#include <L3DFile.h>
#include <gtest/gtest.h>

#include "Graphics/SpecialLight.h"

using namespace openblack;
using namespace openblack::graphics::special_light;

TEST(SpecialLight, EveryModelDrawnSoShinesInTheSameBlue)
{
	EXPECT_EQ(k_Colour, 0xFF203DE8u);
	EXPECT_EQ((k_Colour >> 16) & 0xFFu, 32u);
	EXPECT_EQ((k_Colour >> 8) & 0xFFu, 61u);
	EXPECT_EQ(k_Colour & 0xFFu, 232u);
	// the opened chest's cone of light is its first sub-mesh's eighth primitive
	EXPECT_EQ(k_ChestLightSubMesh, 0);
	EXPECT_EQ(k_ChestLightPrimitive, 7);
}

TEST(SpecialLight, TheTextureTurnsHalfATurnEveryTenSecondsTheNegativeWay)
{
	EXPECT_EQ(TextureTurn(0), 0.0f);
	// the milliseconds times the game's single-precision -pi / 10000
	EXPECT_EQ(TextureTurn(10000), 10000.0f * -0.000314159261f);
	EXPECT_NEAR(TextureTurn(10000), -std::numbers::pi_v<float>, 1e-5f);
	EXPECT_NEAR(TextureTurn(5000), -std::numbers::pi_v<float> / 2.0f, 1e-5f);
}

TEST(SpecialLight, TheTextureIsMirroredTopToBottomAndTurnedAboutItsMiddle)
{
	// not turned: across stays, the top comes to the bottom
	const auto still = TurnedUv({0.875f, 0.125f}, 1.0f, 0.0f);
	EXPECT_FLOAT_EQ(still.x, 0.875f);
	EXPECT_FLOAT_EQ(still.y, 0.875f);
	// a quarter turn the negative way: across goes to minus down
	const auto turned = TurnedUv({1.0f, 0.5f}, 0.0f, -1.0f);
	EXPECT_FLOAT_EQ(turned.x, 0.5f);
	EXPECT_FLOAT_EQ(turned.y, 0.0f);
	// the middle never moves
	const auto middle = TurnedUv({0.5f, 0.5f}, std::cos(1.0f), std::sin(1.0f));
	EXPECT_FLOAT_EQ(middle.x, 0.5f);
	EXPECT_FLOAT_EQ(middle.y, 0.5f);
}

TEST(SpecialLight, ItIsSolidOnlyOnItsSecondGroupsVertices)
{
	// the opened chest's cone of light: nine groups, the second on bone 0, which no other group has
	const std::vector<l3d::L3DVertexGroup> chest {{2, 2}, {8, 0}, {2, 1}, {2, 4}, {2, 3}, {2, 8}, {2, 5}, {2, 7}, {2, 6}};
	EXPECT_EQ(SolidBone(chest), uint16_t {0});
	// one group: none is solid
	const std::vector<l3d::L3DVertexGroup> one {{14, 0}};
	EXPECT_FALSE(SolidBone(one).has_value());
	// a bone that another group has too can't tell the second group's vertices apart
	const std::vector<l3d::L3DVertexGroup> shared {{16, 0}, {8, 9}, {4, 9}};
	EXPECT_FALSE(SolidBone(shared).has_value());
}
