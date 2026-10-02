/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// graphics::argb4444 (src/Graphics/Argb4444.h) against the instructions of runblack.exe: fn_00837400's 4444 branch
// (0x8374F0..0x837533 and 0x83767E..0x837687, emulated below), its leftover alpha (0x83761E), fn_0081FAA0's
// human_shadow texel (0x81FCDA..0x81FCE2) and the alpha flag stems

#include <cstdint>

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/Argb4444.h"

using namespace openblack::graphics;

namespace
{
/// fn_00837400 0x8374F0..0x837533 (one colour pixel) and 0x83767E..0x837687 (its alpha), instruction by instruction
uint16_t EmulateTexel(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	uint32_t eax = b;                       // 0x8374F0 mov al, [edi + ecx + 2]
	eax = (eax & 0xFFu) >> 4;               // 0x8374F4 shr al, 4
	uint32_t edx = 0;                       // 0x8374F7 xor dx, dx
	edx = (edx & ~0xFFu) | (eax & 0xFFu);   // 0x8374FA mov dl, al
	eax = g;                                // 0x8374FC movzx ax, [edi + ecx + 1]
	eax &= 0xFFF0u;                         // 0x837502
	edx += eax;                             // 0x837507
	eax = r;                                // 0x837509 movzx ax, [edi + ecx]
	eax &= 0xFF0u;                          // 0x837512
	eax <<= 4;                              // 0x837517
	edx += eax;                             // 0x83751A
	auto word = static_cast<uint16_t>(edx); // 0x837533 mov word [ebp + eax * 2], dx
	uint32_t al = a & 0xF0u;                // 0x83767E mov al, [edx + ecx] / 0x837681 and al, 0xF0
	const uint32_t dx = al << 8;            // 0x837683 xor edx, edx / 0x837685 mov dh, al
	word = static_cast<uint16_t>(word | dx); // 0x837687 or [edi], dx
	return word;
}
} // namespace

TEST(Argb4444, QuantizeExpandCut)
{
	for (uint32_t v = 0; v < 256; ++v)
	{
		const auto byte = static_cast<uint8_t>(v);
		EXPECT_EQ(argb4444::Quantize(byte), v >> 4);
		EXPECT_EQ(argb4444::Cut(byte), (v & 0xF0u) | (v >> 4)); // the old LandIsland quantise
		EXPECT_EQ(argb4444::Cut(byte), (v >> 4) * 17);          // the old Loaders sky cut
		EXPECT_EQ(argb4444::Cut(argb4444::Cut(byte)), argb4444::Cut(byte));
	}
	for (uint32_t n = 0; n < 16; ++n)
	{
		EXPECT_EQ(argb4444::Expand(static_cast<uint8_t>(n)), n * 17);
		EXPECT_EQ(argb4444::Quantize(argb4444::Expand(static_cast<uint8_t>(n))), n);
	}
	EXPECT_EQ(argb4444::Cut(228), 238); // smokea's example (map-loading.md)
	EXPECT_EQ(argb4444::Cut(255), 255);
	EXPECT_EQ(argb4444::Cut(15), 0);
}

TEST(Argb4444, PackMatchesFn00837400)
{
	for (uint32_t v = 0; v < 256; v += 1)
	{
		const auto a = static_cast<uint8_t>(v);
		const auto b = static_cast<uint8_t>(255 - v);
		const auto c = static_cast<uint8_t>(v * 7);
		const auto d = static_cast<uint8_t>(v * 13 + 5);
		EXPECT_EQ(argb4444::Pack(a, b, c, d), EmulateTexel(a, b, c, d));
		EXPECT_EQ(argb4444::Pack(d, c, b, a), EmulateTexel(d, c, b, a));
	}
}

TEST(Argb4444, UnpackIsCutOfPack)
{
	for (uint32_t t = 0; t < 0x10000; ++t)
	{
		const auto texel = static_cast<uint16_t>(t);
		const auto rgba = argb4444::Unpack(texel);
		EXPECT_EQ(rgba[0], ((t >> 8) & 0xF) * 17);
		EXPECT_EQ(rgba[1], ((t >> 4) & 0xF) * 17);
		EXPECT_EQ(rgba[2], (t & 0xF) * 17);
		EXPECT_EQ(rgba[3], ((t >> 12) & 0xF) * 17);
		EXPECT_EQ(argb4444::Pack(rgba[0], rgba[1], rgba[2], rgba[3]), texel);
	}
}

TEST(Argb4444, PackRaw)
{
	const std::vector<uint8_t> rgb = {0x12, 0x34, 0x56, 0xFF, 0x80, 0x0F};
	const std::vector<uint8_t> alpha = {0xE4, 0x07};
	const auto rgba = argb4444::PackRaw(rgb, alpha);
	ASSERT_EQ(rgba.size(), 8u);
	for (size_t i = 0; i < 2; ++i)
	{
		const auto texel = EmulateTexel(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2], alpha[i]);
		const auto expected = argb4444::Unpack(texel);
		EXPECT_TRUE(std::equal(expected.begin(), expected.end(), rgba.begin() + static_cast<ptrdiff_t>(i * 4)));
	}
	// no a.raw: the alpha of pixel i is colour stream byte i (0x83761E)
	const auto leftover = argb4444::PackRaw(rgb, {});
	EXPECT_EQ(leftover[3], argb4444::Cut(0x12));
	EXPECT_EQ(leftover[7], argb4444::Cut(0x34));
	EXPECT_EQ(leftover[0], argb4444::Cut(0x12));
	EXPECT_EQ(leftover[4], argb4444::Cut(0xFF));
	// a short a.raw: LHLoadData reads min(length, 0x10000) (0x7BCEC9), the tail keeps the colour stream
	const std::vector<uint8_t> shortAlpha = {0xE4};
	const auto partial = argb4444::PackRaw(rgb, shortAlpha);
	EXPECT_EQ(partial[3], argb4444::Cut(0xE4));
	EXPECT_EQ(partial[7], argb4444::Cut(0x34));
	// a longer one is truncated to the pixels
	const std::vector<uint8_t> longAlpha = {0xE4, 0x07, 0xAA};
	EXPECT_EQ(argb4444::PackRaw(rgb, longAlpha), rgba);
}

TEST(Argb4444, HumanShadowTexel)
{
	// fn_0081FAA0 0x81FCDA..0x81FCE2: mov cl, [edi + eax] / and cl, 0xF0 / xor ebx, ebx / mov bh, cl
	for (uint32_t v = 0; v < 256; ++v)
	{
		const uint32_t texel = (v & 0xF0u) << 8;
		EXPECT_EQ(argb4444::Unpack(static_cast<uint16_t>(texel))[3], argb4444::Cut(static_cast<uint8_t>(v)));
		EXPECT_EQ(argb4444::Unpack(static_cast<uint16_t>(texel))[0], 0);
	}
}

TEST(Argb4444, AlphaFlagStems)
{
	for (const auto& stem : argb4444::k_AlphaFlagStems)
	{
		EXPECT_TRUE(std::ranges::none_of(stem, [](char c) { return c >= 'A' && c <= 'Z'; })) << stem;
		EXPECT_EQ(std::ranges::count(argb4444::k_AlphaFlagStems, stem), 1) << stem;
		EXPECT_TRUE(argb4444::HasAlphaFlag(stem)) << stem;
		EXPECT_TRUE(argb4444::HasAlphaFlag(std::string(stem) + "a")) << stem;
	}
	EXPECT_TRUE(argb4444::HasAlphaFlag("Sky"));
	EXPECT_TRUE(argb4444::HasAlphaFlag("skya"));
	EXPECT_TRUE(argb4444::HasAlphaFlag("ATMOS"));
	EXPECT_TRUE(argb4444::HasAlphaFlag("ATMOSA"));
	EXPECT_TRUE(argb4444::HasAlphaFlag("S_TileLandscapeA"));
	EXPECT_TRUE(argb4444::HasAlphaFlag("smokea"));
	EXPECT_TRUE(argb4444::HasAlphaFlag("S_Volcano_Base_Alpha"));
	EXPECT_TRUE(argb4444::HasAlphaFlag("S_Volcano_Base_Alphaa"));
	EXPECT_TRUE(argb4444::HasAlphaFlag("blobsa"));
	// not cut on load: sun (flags 1, 0x81E851: no alpha flag, the 555 / 565 branch); ChallengeScroll and human_shadow
	// are 0x44, so they do have the alpha flag (a 4444 surface), but of type 4 (flags & 0x3F), memory textures the game
	// fills itself without fn_00837400 (human_shadow has its own cut, fn_0081FAA0); the LightMaps, files the original
	// never loads
	EXPECT_FALSE(argb4444::HasAlphaFlag("sun"));
	EXPECT_FALSE(argb4444::HasAlphaFlag("ChallengeScroll"));
	EXPECT_FALSE(argb4444::HasAlphaFlag(argb4444::k_HumanShadowStem));
	EXPECT_FALSE(argb4444::HasAlphaFlag("sclouds"));
	EXPECT_FALSE(argb4444::HasAlphaFlag("gba"));
	EXPECT_FALSE(argb4444::HasAlphaFlag(""));
	EXPECT_FALSE(argb4444::HasAlphaFlag("a"));
	EXPECT_FALSE(argb4444::HasAlphaFlag("skyaa"));
	// the alpha's colour, which has to pass the 0x30000 guard (0x837318) before xa.raw is read
	EXPECT_EQ(argb4444::ColourOfAlpha("Skya"), "Sky");
	EXPECT_EQ(argb4444::ColourOfAlpha("S_IceEnvMapGreya"), "S_IceEnvMapGrey");
	EXPECT_EQ(argb4444::ColourOfAlpha("S_Volcano_Base_Alphaa"), "S_Volcano_Base_Alpha");
	EXPECT_TRUE(argb4444::ColourOfAlpha("S_Volcano_Base_Alpha").empty());
	EXPECT_TRUE(argb4444::ColourOfAlpha("sky").empty());
	EXPECT_TRUE(argb4444::ColourOfAlpha("skyaa").empty());
	EXPECT_TRUE(argb4444::IsAlphaFlagColour("ATMOS"));
	EXPECT_FALSE(argb4444::IsAlphaFlagColour("ATMOSA"));
	static_assert(argb4444::HasAlphaFlag("misc0a"));
}
