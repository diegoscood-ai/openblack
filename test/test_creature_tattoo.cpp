/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureTattoo.h"

using namespace openblack;
using namespace openblack::creature_tattoo;

namespace
{
constexpr uint32_t k_AtlasSize = 256;
constexpr size_t k_SkinTexels = static_cast<size_t>(k_SkinSize) * k_SkinSize;

/// An atlas whose design cells hold a level in the top four bits of the blue: each cell's own index, or a pattern
std::vector<std::array<uint8_t, 3>> FakeAtlas()
{
	std::vector<std::array<uint8_t, 3>> atlas(static_cast<size_t>(k_AtlasSize) * k_AtlasSize, {0, 0, 0});
	for (uint32_t y = 0; y < k_AtlasSize; ++y)
	{
		for (uint32_t x = 0; x < k_AtlasSize; ++x)
		{
			const auto design = ((y / k_DesignSize) * k_DesignsPerRow) + (x / k_DesignSize);
			// Red and green are ignored; the blue's low bits too
			atlas[(y * k_AtlasSize) + x] = {200, 100, static_cast<uint8_t>((design << 4u) | 0x7u)};
		}
	}
	return atlas;
}

/// A mask of levels counting along its texels
Mask Counting(uint32_t size)
{
	Mask mask {.size = size, .levels = {}};
	for (uint32_t i = 0; i < size * size; ++i)
	{
		mask.levels.push_back(static_cast<uint8_t>(i));
	}
	return mask;
}

Design Solid(uint8_t level)
{
	Design design {};
	for (size_t i = 0; i < design.size(); ++i)
	{
		const auto size = k_DesignSize >> i;
		design.at(i) = {.size = size, .levels = std::vector<uint8_t>(static_cast<size_t>(size) * size, level)};
	}
	return design;
}
} // namespace

TEST(CreatureTattoo, DesignsComeFromTheBlueOfTheirCells)
{
	const auto atlas = FakeAtlas();
	const auto design = DesignFromAtlas(atlas, k_AtlasSize, 6);
	ASSERT_EQ(design.front().size, k_DesignSize);
	EXPECT_EQ(design.front().At(0, 0), 6);
	EXPECT_EQ(design.front().At(63, 63), 6);
	// Each smaller mask is half the size
	EXPECT_EQ(design.at(1).size, 32u);
	EXPECT_EQ(design.back().size, 4u);
	EXPECT_EQ(design.back().At(3, 3), 6);
}

TEST(CreatureTattoo, WithHalfSizePicturesOddRowsReadAnotherDesign)
{
	const auto atlas = FakeAtlas();
	// an even row of the mask is its own design's row at half size, an odd row the design two cells to the side
	const auto first = DesignFromAtlas(atlas, k_AtlasSize, 0, true);
	EXPECT_EQ(first.front().At(0, 0), 0);
	EXPECT_EQ(first.front().At(63, 0), 0);
	EXPECT_EQ(first.front().At(0, 1), 2);
	EXPECT_EQ(first.front().At(63, 1), 2);
	const auto middle = DesignFromAtlas(atlas, k_AtlasSize, 5, true);
	EXPECT_EQ(middle.front().At(10, 40), 5);
	EXPECT_EQ(middle.front().At(10, 41), 7);
	// the last design's last row reads past the end of the half atlas: skin
	const auto last = DesignFromAtlas(atlas, k_AtlasSize, 15, true);
	EXPECT_EQ(last.front().At(0, 62), 15);
	EXPECT_EQ(last.front().At(0, 63), 0);
	EXPECT_EQ(last.front().At(63, 63), 0);
	// the smaller masks still average the largest
	EXPECT_EQ(first.at(1).size, 32u);
	EXPECT_EQ(first.at(1).At(0, 0), 1);
}

TEST(CreatureTattoo, WithHalfSizePicturesEachTexelOfTheHalfAtlasShowsTwice)
{
	std::vector<std::array<uint8_t, 3>> atlas(static_cast<size_t>(k_AtlasSize) * k_AtlasSize, {0, 0, 0});
	// the half atlas keeps the top left texel of each square of four: the texel at (2, 0) is the half atlas's (1, 0)
	atlas[2] = {0, 0, 0xF0};
	atlas[3] = {0, 0, 0x90};
	atlas[k_AtlasSize + 2] = {0, 0, 0x90};
	const auto design = DesignFromAtlas(atlas, k_AtlasSize, 0, true);
	EXPECT_EQ(design.front().At(1, 0), 0);
	EXPECT_EQ(design.front().At(2, 0), 15);
	EXPECT_EQ(design.front().At(3, 0), 15);
	EXPECT_EQ(design.front().At(4, 0), 0);
}

TEST(CreatureTattoo, SmallerMasksAverageSquaresOfFourRoundingDown)
{
	std::vector<std::array<uint8_t, 3>> atlas(static_cast<size_t>(k_AtlasSize) * k_AtlasSize, {0, 0, 0});
	// The first design's top left square of four: 15, 0, 0 and 2, which average 4.25
	atlas[0] = {0, 0, 0xF0};
	atlas[k_AtlasSize + 1] = {0, 0, 0x20};
	const auto design = DesignFromAtlas(atlas, k_AtlasSize, 0);
	EXPECT_EQ(design.at(1).At(0, 0), 4);
}

TEST(CreatureTattoo, MaskSizeFollowsTheTattoosSize)
{
	EXPECT_EQ(MaskLevel(1.0f), 0u);
	EXPECT_EQ(MaskLevel(0.75f), 0u);
	// 0.74 * 64 is 47.36
	EXPECT_EQ(MaskLevel(0.74f), 1u);
	EXPECT_EQ(MaskLevel(0.375f), 1u);
	EXPECT_EQ(MaskLevel(0.25f), 2u);
	EXPECT_EQ(MaskLevel(0.1f), 3u);
	EXPECT_EQ(MaskLevel(0.05f), 4u);
	// Bigger than the skin is the skin
	EXPECT_EQ(MaskLevel(3.0f), 0u);
}

TEST(CreatureTattoo, MasksTurnAndFlip)
{
	const auto mask = Counting(2);
	// 0 1 / 2 3 turned a quarter clockwise is 2 0 / 3 1
	const auto turned = Oriented(mask, 1, false);
	EXPECT_EQ(turned.levels, (std::vector<uint8_t> {2, 0, 3, 1}));
	EXPECT_EQ(Oriented(mask, 2, false).levels, (std::vector<uint8_t> {3, 2, 1, 0}));
	EXPECT_EQ(Oriented(mask, 3, false).levels, (std::vector<uint8_t> {1, 3, 0, 2}));
	// Flipped left to right after the turn
	EXPECT_EQ(Oriented(mask, 0, true).levels, (std::vector<uint8_t> {1, 0, 3, 2}));
	EXPECT_EQ(Oriented(mask, 1, true).levels, (std::vector<uint8_t> {0, 2, 1, 3}));
}

TEST(CreatureTattoo, TexelsMoveTowardsTheColourIn15ths)
{
	// Full level: the colour's top four bits, opaque
	EXPECT_EQ(PaintTexel(0x0123, 15, {0xA5, 0xB6, 0xC7}), 0xFABC);
	// None: the skin, made opaque
	EXPECT_EQ(PaintTexel(0x0123, 0, {0xA5, 0xB6, 0xC7}), 0xF123);
	// Level 5 of black over 0xF on every channel: 0xF * 10 / 15 = 10
	EXPECT_EQ(PaintTexel(0x0FFF, 5, {0, 0, 0}), 0xFAAA);
	// Level 5 of white over black: 15 * 5 / 15 = 5
	EXPECT_EQ(PaintTexel(0x0000, 5, {0xFF, 0xFF, 0xFF}), 0xF555);
	// Rounded down: (3 * 8 + 15 * 7) / 15 = 8.6
	EXPECT_EQ(PaintTexel(0x0333, 7, {0xFF, 0xFF, 0xFF}), 0xF888);
}

TEST(CreatureTattoo, TattoosSpanTheirSizeOfTheLargestMask)
{
	std::vector<uint16_t> skin(k_SkinTexels, 0x0000);
	// Half size: drawn from the 32 texel mask at twice its size, 32 texels across, centred on 100, 120
	const Site site {.enabled = true, .u = 100, .v = 120, .skin = 0, .size = 0.5f, .mirror = false, .rotation = 0};
	Paint(skin, Solid(15), {0xFF, 0xFF, 0xFF}, site);
	const auto at = [&skin](int x, int y) { return skin.at(static_cast<size_t>((y * 256) + x)); };
	EXPECT_EQ(at(84, 104), 0xFFFF);
	EXPECT_EQ(at(115, 135), 0xFFFF);
	EXPECT_EQ(at(83, 104), 0x0000);
	EXPECT_EQ(at(116, 104), 0x0000);
	EXPECT_EQ(at(84, 136), 0x0000);
}

TEST(CreatureTattoo, TheMaskIsWalkedInFloatStepsCutTowardsNought)
{
	// 64 texels over 48: the step 1.3333334 added up falls short of 12 at the ninth texel and of 16 at the twelfth, where
	// the exact ninth and twelfth of 64 over 48 are 12 and 16
	const auto walk = creature_tattoo::MaskWalk(64, 48);
	ASSERT_EQ(walk.size(), 48u);
	EXPECT_EQ(walk.at(0), 0u);
	EXPECT_EQ(walk.at(3), 4u);
	EXPECT_EQ(walk.at(9), 11u);
	EXPECT_EQ(walk.at(12), 15u);
	EXPECT_EQ(walk.at(30), 39u);
	EXPECT_EQ(walk.back(), 62u);
	// over 62 and 54 the sum falls short once, half way
	EXPECT_EQ(creature_tattoo::MaskWalk(64, 62).at(31), 31u);
	EXPECT_EQ(creature_tattoo::MaskWalk(64, 54).at(27), 31u);
	// a span the width's own size shows each texel once
	const auto same = creature_tattoo::MaskWalk(64, 64);
	for (uint32_t i = 0; i < 64; ++i)
	{
		EXPECT_EQ(same.at(i), i);
	}
	EXPECT_TRUE(creature_tattoo::MaskWalk(64, 0).empty());
}

TEST(CreatureTattoo, TattoosOverAnEdgeArentPainted)
{
	std::vector<uint16_t> skin(k_SkinTexels, 0x0000);
	// 64 texels across, centred 20 texels from the left edge
	const Site site {.enabled = true, .u = 20, .v = 128, .skin = 0, .size = 1.0f, .mirror = false, .rotation = 0};
	Paint(skin, Solid(15), {0xFF, 0xFF, 0xFF}, site);
	EXPECT_TRUE(std::ranges::all_of(skin, [](uint16_t texel) { return texel == 0; }));
}

TEST(CreatureTattoo, PaletteColoursBrightenAndDarken)
{
	std::vector<std::array<uint8_t, 3>> palette(static_cast<size_t>(k_PaletteColumns) * k_PaletteRows, {0, 0, 0});
	palette.at((3 * k_PaletteColumns) + 2) = {100, 50, 200};
	EXPECT_EQ(PaletteColour(palette, 2, 3, 0.5f), glm::u8vec3(100, 50, 200));
	// All the way to white, and to black
	EXPECT_EQ(PaletteColour(palette, 2, 3, 1.0f), glm::u8vec3(255, 255, 255));
	EXPECT_EQ(PaletteColour(palette, 2, 3, 0.0f), glm::u8vec3(0, 0, 0));
	// A quarter: half the colour
	EXPECT_EQ(PaletteColour(palette, 2, 3, 0.25f), glm::u8vec3(50, 25, 100));
	// Three quarters: half way to white, rounded down
	EXPECT_EQ(PaletteColour(palette, 2, 3, 0.75f), glm::u8vec3(177, 152, 227));
}

TEST(CreatureTattoo, DesignsGoInTheSlotAsTheEditorPutsThem)
{
	Slots slots {};
	// The first empty slot
	EXPECT_EQ(SlotFor(slots, 2, 5), 0u);
	slots[0] = {.design = 5, .site = 2, .colour = {}};
	slots[1] = {.design = 7, .site = 3, .colour = {}};
	// The same design on the same site recolours it
	EXPECT_EQ(SlotFor(slots, 2, 5), 0u);
	EXPECT_EQ(SlotFor(slots, 4, 1), 2u);
	// Every slot full: the one on the same site
	for (size_t i = 2; i < slots.size(); ++i)
	{
		slots.at(i) = {.design = 0, .site = static_cast<uint8_t>(i), .colour = {}};
	}
	EXPECT_EQ(SlotFor(slots, 3, 9), 1u);
	slots[1].site = 2;
	// Nowhere for it when every slot holds another site's tattoo
	EXPECT_FALSE(SlotFor(slots, 1, 9).has_value());
}
