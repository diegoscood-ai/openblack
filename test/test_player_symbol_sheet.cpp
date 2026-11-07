/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "3D/PlayerSymbolSheet.h"

using namespace openblack::player_symbol_sheet;

TEST(PlayerSymbolSheet, AComputerGodTakesTheImageAfterItsNumberLessOne)
{
	EXPECT_EQ(ComputerImageIndex(1, 2), 0u);
	EXPECT_EQ(ComputerImageIndex(2, 2), 1u);
	EXPECT_EQ(ComputerImageIndex(3, 1), 2u);
	// within the three images
	EXPECT_EQ(ComputerImageIndex(0, 2), 0u);
	EXPECT_EQ(ComputerImageIndex(5, 2), 2u);
}

TEST(PlayerSymbolSheet, TheSecondPlayerIsKhazarOnTheThirdLandAndNemesisOnTheFifth)
{
	EXPECT_EQ(ComputerImageIndex(1, 3), 1u);
	EXPECT_EQ(ComputerImageIndex(1, 5), 2u);
	EXPECT_EQ(ComputerImageIndex(2, 3), 1u);
	EXPECT_EQ(ComputerImageIndex(2, 5), 1u);
	EXPECT_EQ(ComputerImageFile(0), "Lethis.cps");
	EXPECT_EQ(ComputerImageFile(1), "Kazarr.cps");
	EXPECT_EQ(ComputerImageFile(2), "nemesis.cps");
}

TEST(PlayerSymbolSheet, APlayersCellIsFourToARowOfSixtyFourTexels)
{
	EXPECT_EQ(CellOrigin(0), glm::u16vec2(0, 0));
	EXPECT_EQ(CellOrigin(1), glm::u16vec2(64, 0));
	EXPECT_EQ(CellOrigin(3), glm::u16vec2(192, 0));
	EXPECT_EQ(CellOrigin(5), glm::u16vec2(64, 64));
	EXPECT_EQ(k_SheetSize, 256);
}

TEST(PlayerSymbolSheet, EachByteGivesItsTopFourBitsToEveryChannel)
{
	std::vector<uint8_t> image(k_ImageBytes, 0);
	image[0] = 0xFF;
	image[1] = 0x1F;
	image[2] = 0x80;
	image[k_ImageBytes - 1] = 0x7C;
	const auto texels = CellTexels(image);
	ASSERT_TRUE(texels.has_value());
	ASSERT_EQ(texels->size(), k_ImageBytes * 4);
	const std::vector<uint8_t> first(texels->begin(), texels->begin() + 12);
	EXPECT_EQ(first, (std::vector<uint8_t> {255, 255, 255, 255, 17, 17, 17, 17, 136, 136, 136, 136}));
	EXPECT_EQ(texels->back(), 7 * 17);
	EXPECT_EQ((*texels)[12], 0);
}

TEST(PlayerSymbolSheet, AnImageShorterThanACellGivesNothing)
{
	const std::vector<uint8_t> image(k_ImageBytes - 1, 0xFF);
	EXPECT_FALSE(CellTexels(image).has_value());
}
