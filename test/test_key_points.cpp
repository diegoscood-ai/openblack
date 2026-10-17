/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The key-point spline (Particles/Rules/KeyPoints) against the game's own float steps. The expected bit patterns come
// from a step-by-step float model of the game's spline build and its one-value reader (each operation rounded to a
// float, as the game's FPU does), not from this code.

#include <cstdint>

#include <bit>
#include <vector>

#include <gtest/gtest.h>

#include "Particles/Rules/KeyPoints.h"

using namespace openblack::psys;

namespace
{
float Bits(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

uint32_t BitsOf(float value)
{
	return std::bit_cast<uint32_t>(value);
}

struct Pinned
{
	uint32_t t;
	uint32_t value;
};
} // namespace

TEST(KeyPoints, theHealKeysAreBuiltAndReadAsTheGameDoes)
{
	// SF_HealChakraPU's UR_KPStretchHeight keys
	const auto spline = key_points::Make({0.0f, 0.1f, 2.0f, 1.0f, 6.0f, 0.1f});
	ASSERT_EQ(spline.keys.size(), 3u);
	EXPECT_EQ(BitsOf(spline.keys[0].z), 0x3F819999u);
	EXPECT_EQ(BitsOf(spline.keys[1].z), 0xBF2CCCCCu);
	// The last second derivative is one where dividing gave another bit
	EXPECT_EQ(BitsOf(spline.keys[2].z), 0x3F019999u);
	const std::vector<Pinned> pinned {
	    {0x3E99999Au, 0x3E112B02u}, // 0.3
	    {0x3F8CCCCDu, 0x3F068000u}, // 1.1
	    {0x402CCCCDu, 0x3F8B3BA0u}, // 2.7
	    {0x408CCCCDu, 0x3F0BE0DEu}, // 4.4
	    {0x40BCCCCDu, 0x3DD1E218u}, // 5.9
	};
	for (const auto& [t, value] : pinned)
	{
		EXPECT_EQ(BitsOf(key_points::Evaluate(spline, Bits(t), -1.0f)), value) << Bits(t);
	}
}

TEST(KeyPoints, fiveUnevenKeysAreBuiltAndReadAsTheGameDoes)
{
	const auto spline = key_points::Make({0.0f, 1.5f, 0.7f, -2.25f, 1.3f, 0.8f, 2.9f, 3.1f, 4.0f, -0.4f});
	ASSERT_EQ(spline.keys.size(), 5u);
	const std::vector<uint32_t> curvatures {0xC226328Eu, 0x4214B8B3u, 0xC103C64Cu, 0xC09EE774u, 0x413291ACu};
	for (size_t i = 0; i < curvatures.size(); ++i)
	{
		EXPECT_EQ(BitsOf(spline.keys[i].z), curvatures[i]) << i;
	}
	const std::vector<Pinned> pinned {
	    {0x3EB33333u, 0xBE76FCF8u}, // 0.35
	    {0x3F800000u, 0xBFB028F6u}, // 1.0
	    {0x400CCCCDu, 0x4084D984u}, // 2.2
	    {0x40533333u, 0x3FBF8566u}, // 3.3
	    {0x4079999Au, 0xBEB17AE5u}, // 3.9
	};
	for (const auto& [t, value] : pinned)
	{
		EXPECT_EQ(BitsOf(key_points::Evaluate(spline, Bits(t), -1.0f)), value) << Bits(t);
	}
}

TEST(KeyPoints, keysAtTheSameTimeWriteNothing)
{
	const auto spline = key_points::Make({0.5f, 1.0f, 0.5f, 2.0f});
	EXPECT_FLOAT_EQ(key_points::Evaluate(spline, 0.5f, 7.0f), 7.0f);
}
