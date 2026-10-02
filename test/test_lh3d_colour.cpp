/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// lh3d_colour:: (src/Graphics/Lh3dColour.h) against the instructions of runblack.exe: fn_0080BF10's diffuse
// (0x80BFA3..0x80C00B, emulated below), the 0x80808081 divides (signed imul of LH3DMist 0x7FA6DF, unsigned mul of
// fn_005E25C0 0x5E2729..0x5E273B), the alpha rules of fn_00809D80 / fn_00809DE0 / LH3DMist / LH3DCreature::DrawNow
// and the model light's (c f) >> 8 (model_light::Apply)

#include <cstdint>

#include <gtest/gtest.h>

#include "Graphics/Lh3dColour.h"
#include "Graphics/ModelLight.h"

using namespace openblack;

namespace
{
/// fn_0080BF10 0x80BFA3..0x80C00B instruction by instruction: c = obj+0x4C, t = the tint
uint32_t EmulateDiffuse(uint32_t c, uint32_t t)
{
	uint32_t esi = c;
	uint32_t ecx = (t >> 16) & 0xFFu; // 0x80BFA8 mov cl, [esp + 0x12]
	uint32_t edi = esi >> 8;          // 0x80BFAE
	uint32_t edx = edi & 0xFF00u;     // 0x80BFB3
	uint32_t eax = t;                 // 0x80BFBC
	ecx *= edx;                       // 0x80BFC0
	edx = edi & 0xFF0000u;            // 0x80BFC5
	const uint32_t ebx = eax >> 24;   // 0x80BFCD
	edx *= ebx;                       // 0x80BFD0
	edx &= 0xFF00FFFFu;               // 0x80BFD3
	ecx |= edx;                       // 0x80BFD9
	edx = (eax >> 8) & 0xFFu;         // 0x80BFDB..0x80BFDD mov dl, ah
	edi &= 0xFFu;                     // 0x80BFDF
	esi &= 0xFFu;                     // 0x80BFE5
	eax &= 0xFFu;                     // 0x80BFEB
	esi *= eax;                       // 0x80BFF0
	edx *= edi;                       // 0x80BFF3
	ecx &= 0xFFFF00FFu;               // 0x80BFF6
	ecx |= edx;                       // 0x80BFFC
	esi >>= 8;                        // 0x80BFFE
	ecx &= 0xFFFFFF00u;               // 0x80C002
	ecx |= esi;                       // 0x80C008
	return ecx;
}

/// LH3DMist 0x7FA6DF..0x7FA6F4: imul by 0x80808081, + x, sar 7, + the sign bit
int32_t EmulateSignedDiv255(int32_t x)
{
	const auto product = static_cast<int64_t>(static_cast<int32_t>(0x80808081u)) * x;
	auto edx = static_cast<int32_t>(static_cast<uint64_t>(product) >> 32);
	edx += x;
	edx >>= 7;
	return edx + static_cast<int32_t>(static_cast<uint32_t>(edx) >> 31);
}

/// fn_005E25C0 0x5E272C..0x5E273B: mul by 0x80808081, shr 7
uint32_t EmulateUnsignedDiv255(uint32_t x)
{
	return static_cast<uint32_t>((static_cast<uint64_t>(0x80808081u) * x) >> 32) >> 7;
}

/// A small LCG so the sweep is reproducible
uint32_t Next(uint32_t& state)
{
	state = state * 1664525u + 1013904223u;
	return state;
}
} // namespace

TEST(Lh3dColour, MulShr8_4MatchesFn0080BF10)
{
	uint32_t state = 12345u;
	for (int i = 0; i < 100000; ++i)
	{
		const uint32_t c = Next(state);
		const uint32_t t = Next(state);
		ASSERT_EQ(lh3d_colour::MulShr8_4(c, t), EmulateDiffuse(c, t)) << std::hex << c << " " << t;
	}
	for (const uint32_t c : {0u, 0xFFFFFFFFu, 0xFF000000u, 0x00FFFFFFu, 0x80808080u})
	{
		for (const uint32_t t : {0u, 0xFFFFFFFFu, 0x96FFFFFFu, 0xFF505050u, 0x01010101u})
		{
			EXPECT_EQ(lh3d_colour::MulShr8_4(c, t), EmulateDiffuse(c, t));
		}
	}
}

TEST(Lh3dColour, MulShr8_4Values)
{
	// a tint of 0xFF takes one off every channel, so a field (BlendColor alpha 0xFF, 0x528510) ends at 254
	EXPECT_EQ(lh3d_colour::MulShr8_4(0xFFFFFFFFu, 0xFFFFFFFFu), 0xFEFEFEFEu);
	// the one-shot orb: the land colour's alpha 0xFF (fn_00801C90) times [0xBE8E8C] low byte 0x96 = 0x95
	EXPECT_EQ(lh3d_colour::Alpha(lh3d_colour::MulShr8_4(0xFF000000u, 0x96FFFFFFu)), 0x95u);
}

TEST(Lh3dColour, AddSat)
{
	// fn_0080BF10: all four channels, the alpha too
	EXPECT_EQ(lh3d_colour::AddSat_4(0xF0F01020u, 0x20200101u), 0xFFFF1121u);
	EXPECT_EQ(lh3d_colour::AddSat_4(0u, 0u), 0u);
	EXPECT_EQ(lh3d_colour::AddSat_4(0xFFFFFFFFu, 0xFFFFFFFFu), 0xFFFFFFFFu);
	// fn_00809DE0: the first argument's alpha (0x809E46)
	EXPECT_EQ(lh3d_colour::AddSat_3KeepA(0x40F01020u, 0xFF200101u), 0x40FF1121u);
	EXPECT_EQ(lh3d_colour::AddSat_3KeepA(0x00000000u, 0xFFFFFFFFu), 0x00FFFFFFu);
}

TEST(Lh3dColour, MulShr8KeepA)
{
	// fn_00809D80: the first argument's alpha (0x809DCF)
	EXPECT_EQ(lh3d_colour::MulShr8_3KeepA(0x12FFFFFFu, 0xFF808080u), 0x127F7F7Fu);
	EXPECT_EQ(lh3d_colour::MulShr8_3KeepA(0xFFFFFFFFu, 0x00FFFFFFu), 0xFFFEFEFEu);
	// the RGB is the same as the four-channel product's
	uint32_t state = 777u;
	for (int i = 0; i < 10000; ++i)
	{
		const uint32_t a = Next(state);
		const uint32_t b = Next(state);
		ASSERT_EQ(lh3d_colour::MulShr8_3KeepA(a, b) & 0x00FFFFFFu, lh3d_colour::MulShr8_4(a, b) & 0x00FFFFFFu);
		ASSERT_EQ(lh3d_colour::Alpha(lh3d_colour::MulShr8_3KeepA(a, b)), lh3d_colour::Alpha(a));
	}
	// the scalar form: a grey t = k in every channel
	for (uint32_t k = 0; k <= 255; ++k)
	{
		const uint32_t grey = lh3d_colour::Argb(k, k, k, 0);
		ASSERT_EQ(lh3d_colour::ScaleShr8_3KeepA(0xA0C08040u, k), lh3d_colour::MulShr8_3KeepA(0xA0C08040u, grey));
	}
}

TEST(Lh3dColour, ScaleShr8MatchesModelLight)
{
	// model_light::Apply is ScaleShr8_3KeepA by the factor: I = 255 with amb 90 gives f = 254 (0x84BBC3..0x84BBE5)
	EXPECT_EQ(model_light::Apply(0x80FFFFFFu, 255, 90), 0x80FDFDFDu);
	for (int intensity = -255; intensity <= 255; intensity += 3)
	{
		const auto f = static_cast<uint32_t>(model_light::Factor(intensity, 90));
		EXPECT_EQ(model_light::Apply(0x7F10E0A5u, intensity, 90), lh3d_colour::ScaleShr8_3KeepA(0x7F10E0A5u, f));
	}
}

TEST(Lh3dColour, Mul255IsTheBinarysDivide)
{
	// trunc(x / 255) for every product of two bytes, by the signed (LH3DMist 0x7FA6DF) and unsigned (fn_005E25C0)
	// forms of the 0x80808081 multiply
	for (uint32_t x = 0; x <= 255u * 255u; ++x)
	{
		ASSERT_EQ(static_cast<uint32_t>(EmulateSignedDiv255(static_cast<int32_t>(x))), x / 255u) << x;
		ASSERT_EQ(EmulateUnsignedDiv255(x), x / 255u) << x;
	}
	for (uint32_t a = 0; a <= 255u; ++a)
	{
		for (uint32_t b = 0; b <= 255u; ++b)
		{
			ASSERT_EQ(lh3d_colour::Mul255(a, b), EmulateUnsignedDiv255(a * b));
		}
	}
}

TEST(Lh3dColour, Mul255AlphaRules)
{
	// fn_007ACF70: the four channels
	EXPECT_EQ(lh3d_colour::Mul255_4(0x80808080u, 0xFFFFFFFFu), 0x80808080u);
	EXPECT_EQ(lh3d_colour::Mul255_4(0xC8C8C8C8u, 0x80808080u), 0x64646464u);
	// LH3DMist: the colour's alpha, not the light's (0x7FA753)
	EXPECT_EQ(lh3d_colour::Mul255_3KeepA(0x80C8C8C8u, 0x00808080u), 0x80646464u);
	EXPECT_EQ(lh3d_colour::Mul255_3KeepA(0x10FF8040u, 0xFFFFFFFFu), 0x10FF8040u);
	// LH3DCreature::DrawNow: opaque (0x48EF8F)
	EXPECT_EQ(lh3d_colour::Mul255_3OpaqueA(0x00FFFFFFu, 0x00FFFFFFu), 0xFFFFFFFFu);
	EXPECT_EQ(lh3d_colour::Mul255_3OpaqueA(0x12C8C8C8u, 0x34808080u), 0xFF646464u);
}

TEST(Lh3dColour, Conversions)
{
	EXPECT_EQ(lh3d_colour::Argb(1, 2, 3, 4), 0x04010203u);
	EXPECT_EQ(lh3d_colour::Argb(0x101, 0x102, 0x103), 0x00010203u);
	EXPECT_EQ(lh3d_colour::Red(0x11223344u), 0x22u);
	EXPECT_EQ(lh3d_colour::Green(0x11223344u), 0x33u);
	EXPECT_EQ(lh3d_colour::Blue(0x11223344u), 0x44u);
	EXPECT_EQ(lh3d_colour::Alpha(0x11223344u), 0x11u);
	EXPECT_EQ(lh3d_colour::ToAbgr(0x11223344u), 0x11443322u);
	EXPECT_EQ(lh3d_colour::ToAbgr(0x11223344u, 0x99u), 0x99443322u);
	EXPECT_EQ(lh3d_colour::ToAbgr(glm::vec4(1.0f, 0.0f, 0.5f, 1.0f)), 0xFF8000FFu);
	EXPECT_EQ(lh3d_colour::ToAbgr(glm::vec4(-1.0f, 2.0f, 0.0f, 0.0f)), 0x0000FF00u);
	EXPECT_EQ(lh3d_colour::ToVec4(0xFF000000u), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	EXPECT_EQ(lh3d_colour::ToVec4(0x00FF0000u), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
	EXPECT_EQ(lh3d_colour::ToVec3(0x000000FFu), glm::vec3(0.0f, 0.0f, 1.0f));
	EXPECT_FLOAT_EQ(lh3d_colour::ToVec4(0x80808080u).g, 128.0f / 255.0f);
}
