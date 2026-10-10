/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <optional>

#include <gtest/gtest.h>

#include "Graphics/HandWaterGlow.h"

namespace hand_water_glow = openblack::graphics::hand_water_glow;

TEST(HandWaterGlow, WarmsThePaletteColourTowardsOrange)
{
	// A quarter of the way from black to (255, 128, 64), rounding down; full strength is 190 of alpha
	EXPECT_EQ(hand_water_glow::Colour(0x000000, 1.0f), 0xBE3F2010u);
	EXPECT_EQ(hand_water_glow::Colour(0xFFFFFF, 0.5f), 0x5FFFDFCFu);
	EXPECT_EQ(hand_water_glow::Colour(0x123456, 0.0f), 0x004D4750u);
	// (200, 100, 0) becomes (213, 107, 16); the palette's alpha byte is not read
	EXPECT_EQ(hand_water_glow::Colour(0xFFC86400u, 1.0f), 0xBED56B10u);
	// Green over 128 and blue over 64 move down: (255, 200, 100) becomes (255, 182, 91); 95 of alpha, truncated
	EXPECT_EQ(hand_water_glow::Colour(0xFFFFC864u, 0.5f), 0x5FFFB65Bu);
}

TEST(HandWaterGlow, NeverShowsInsideTheTemple)
{
	// However strong the hand's light, the temple has no sea for the glow to lie on
	EXPECT_FALSE(hand_water_glow::Shows(true, 1.0f));
	EXPECT_FALSE(hand_water_glow::Shows(true, 0.5f));
	// Outside, it shows once the hand's light is strong enough
	EXPECT_TRUE(hand_water_glow::Shows(false, 1.0f));
	EXPECT_FALSE(hand_water_glow::Shows(false, hand_water_glow::k_MinimumStrength));
}

TEST(HandWaterGlow, ShowsOnlyNearWater)
{
	// High land everywhere
	const auto high = [](int /*x*/, int /*z*/) { return std::optional<uint16_t>(40); };
	EXPECT_FALSE(hand_water_glow::NearLowLand({2560.0f, 2560.0f}, high));

	// One low cell within reach, 70 units from the hand
	const auto lowCell = [](int x, int z) { return std::optional<uint16_t>(x == 263 && z == 256 ? 4 : 40); };
	EXPECT_TRUE(hand_water_glow::NearLowLand({2560.0f, 2560.0f}, lowCell));
	EXPECT_FALSE(hand_water_glow::NearLowLand({2400.0f, 2560.0f}, lowCell));

	// Where there is no land at all
	const auto noLand = [](int x, int /*z*/) { return x < 300 ? std::optional<uint16_t>(40) : std::nullopt; };
	EXPECT_TRUE(hand_water_glow::NearLowLand({2950.0f, 2560.0f}, noLand));
}

TEST(HandWaterGlow, FirstCellsStopAtTheMapsEdges)
{
	// Near the map's first cell the search starts there; a smaller map's last cell bounds the first cells too
	int lowestX = 1000;
	const auto record = [&lowestX](int x, int /*z*/) {
		lowestX = std::min(lowestX, x);
		return std::optional<uint16_t>(40);
	};
	EXPECT_FALSE(hand_water_glow::NearLowLand({20.0f, 20.0f}, record));
	EXPECT_EQ(lowestX, 0);
	lowestX = 1000;
	EXPECT_FALSE(hand_water_glow::NearLowLand({3000.0f, 20.0f}, record, 100));
	EXPECT_EQ(lowestX, 100);
}
