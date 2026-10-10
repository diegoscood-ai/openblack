/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// dxt_alpha, the alpha of the mesh pack's DXT textures as ARGB 4444 nibbles, and the 64 x 64 pick mask PickMaskLoader
// makes from it (issue #130), over fake blocks and a fake pack texture

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <vector>

#include <PackFile.h>
#include <gtest/gtest.h>

#include "3D/DxtAlpha.h"
#include "3D/PickMask.h"
#include "Graphics/ShadowMath.h"
#include "Resources/Loaders.h"

using openblack::dxt_alpha::AlphaWords;
using openblack::dxt_alpha::BlockAlpha;
using openblack::dxt_alpha::Format;
using openblack::graphics::shadow_math::MakeAlphaMap;

TEST(DxtAlpha, Dxt1TransparentOnlyInThreeColourBlocks)
{
	// every texel on the fourth index
	const std::array<uint8_t, 8> threeColours = {0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	const std::array<uint8_t, 8> fourColours = {0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF};
	EXPECT_EQ(BlockAlpha(Format::Dxt1, threeColours, 0), 0);
	EXPECT_EQ(BlockAlpha(Format::Dxt1, threeColours, 15), 0);
	EXPECT_EQ(BlockAlpha(Format::Dxt1, fourColours, 0), 0xF);
	// equal colours are a three-colour block; another index is opaque
	const std::array<uint8_t, 8> equal = {0x34, 0x12, 0x34, 0x12, 0xFE, 0x00, 0x00, 0x00};
	EXPECT_EQ(BlockAlpha(Format::Dxt1, equal, 0), 0xF); // index 2
	EXPECT_EQ(BlockAlpha(Format::Dxt1, equal, 1), 0);   // index 3
	EXPECT_EQ(BlockAlpha(Format::Dxt1, equal, 4), 0xF); // index 0
}

TEST(DxtAlpha, Dxt3NibblesLowFirst)
{
	std::array<uint8_t, 16> block {};
	block[0] = 0x10;
	block[1] = 0x32;
	block[7] = 0xF0;
	EXPECT_EQ(BlockAlpha(Format::Dxt3, block, 0), 0x0);
	EXPECT_EQ(BlockAlpha(Format::Dxt3, block, 1), 0x1);
	EXPECT_EQ(BlockAlpha(Format::Dxt3, block, 2), 0x2);
	EXPECT_EQ(BlockAlpha(Format::Dxt3, block, 3), 0x3);
	EXPECT_EQ(BlockAlpha(Format::Dxt3, block, 14), 0x0);
	EXPECT_EQ(BlockAlpha(Format::Dxt3, block, 15), 0xF);
}

TEST(DxtAlpha, Dxt5StepsAndTheirTopFourBits)
{
	// texel 0 code 2, texel 1 code 7, texel 2 code 6, texel 3 code 1: 010 111 110 001 from bit 0
	const uint32_t codes = 2u | (7u << 3) | (6u << 6) | (1u << 9);
	const auto block = [codes](uint8_t alpha0, uint8_t alpha1) {
		std::array<uint8_t, 16> bytes {};
		bytes[0] = alpha0;
		bytes[1] = alpha1;
		bytes[2] = static_cast<uint8_t>(codes & 0xFF);
		bytes[3] = static_cast<uint8_t>(codes >> 8);
		return bytes;
	};
	// eight alphas: (6 x 255) / 7 = 218 -> 13, 255 / 7 = 36 -> 2, (2 x 255) / 7 = 72 -> 4, the second alpha 0
	const auto eight = block(255, 0);
	EXPECT_EQ(BlockAlpha(Format::Dxt5, eight, 0), 13);
	EXPECT_EQ(BlockAlpha(Format::Dxt5, eight, 1), 2);
	EXPECT_EQ(BlockAlpha(Format::Dxt5, eight, 2), 4);
	EXPECT_EQ(BlockAlpha(Format::Dxt5, eight, 3), 0);
	// six alphas and 0, 255: 255 / 5 = 51 -> 3, 255 -> 15, 0, the second alpha 255 -> 15
	const auto six = block(0, 255);
	EXPECT_EQ(BlockAlpha(Format::Dxt5, six, 0), 3);
	EXPECT_EQ(BlockAlpha(Format::Dxt5, six, 1), 15);
	EXPECT_EQ(BlockAlpha(Format::Dxt5, six, 2), 0);
	EXPECT_EQ(BlockAlpha(Format::Dxt5, six, 3), 15);
	// an alpha of 15 has no top bits: the mask is 0 there
	EXPECT_EQ(BlockAlpha(Format::Dxt5, block(15, 15), 3), 0);
}

TEST(DxtAlpha, WordsAcrossBlocks)
{
	// 8 x 4: two DXT3 blocks side by side; texel (5, 1) is the second block's index 5
	std::vector<uint8_t> data(32, 0);
	data[16 + 2] = 0x90; // the second block's row 1, its first two texels: the second's nibble high
	const auto words = AlphaWords(Format::Dxt3, 8, 4, data);
	ASSERT_EQ(words.size(), 32u);
	EXPECT_EQ(words[1 * 8 + 5], 0x9000);
	EXPECT_EQ(std::count(words.begin(), words.end(), uint16_t {0}), 31);
	// too short for the texture
	EXPECT_TRUE(AlphaWords(Format::Dxt3, 8, 8, data).empty());
	EXPECT_TRUE(AlphaWords(Format::Dxt1, 0, 4, data).empty());
}

TEST(DxtAlpha, PickMaskSamplesTheTexels)
{
	// a 4 x 4 DXT1 texture with only its first texel transparent: the mask's cells take texel (r 4 / 64, c 4 / 64),
	// so the first 16 cells of the first 16 rows are 0 and the others the opaque nibble
	const std::vector<uint8_t> data = {0x00, 0x00, 0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00};
	const auto words = AlphaWords(Format::Dxt1, 4, 4, data);
	const auto mask = MakeAlphaMap(words, 4, 4);
	EXPECT_EQ(mask[0], 0);
	EXPECT_EQ(mask[15], 0);
	EXPECT_EQ(mask[16], 0xF0);
	EXPECT_EQ(mask[15 * 64 + 15], 0);
	EXPECT_EQ(mask[16 * 64], 0xF0);
	EXPECT_EQ(std::count(mask.begin(), mask.end(), uint8_t {0}), 16 * 16);
}

TEST(DxtAlpha, LoaderMakesThePackTexturesMask)
{
	// the same 4 x 4 DXT1 texture as a pack texture: its mask under PickMaskLoader
	openblack::pack::G3DTexture texture {};
	texture.ddsHeader.width = 4;
	texture.ddsHeader.height = 4;
	texture.ddsHeader.format.fourCC = {'D', 'X', 'T', '1'};
	texture.ddsData = {0x00, 0x00, 0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00};
	const auto mask = openblack::resources::PickMaskLoader {}(openblack::resources::PickMaskLoader::FromPackTag {}, texture);
	ASSERT_NE(mask, nullptr);
	EXPECT_EQ(mask->cells[0], 0);
	EXPECT_EQ(mask->cells[16], 0xF0);
	EXPECT_EQ(std::count(mask->cells.begin(), mask->cells.end(), uint8_t {0}), 16 * 16);
	// (port guard) too few bytes: not 0 anywhere
	texture.ddsData.resize(4);
	const auto guarded = openblack::resources::PickMaskLoader {}(openblack::resources::PickMaskLoader::FromPackTag {}, texture);
	ASSERT_NE(guarded, nullptr);
	EXPECT_EQ(std::count(guarded->cells.begin(), guarded->cells.end(), uint8_t {0xF0}), 64 * 64);
}
