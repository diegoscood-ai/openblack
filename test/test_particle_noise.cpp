/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>

#include <gtest/gtest.h>

#include "Particles/Noise.h"

using namespace openblack::particles;

TEST(ParticleNoise, TheThreeDimensionalNoiseIsTheOriginals)
{
	// The lattice noise UR_GustyWind reads, blended linearly between the eight cells about the point: each case is the
	// original's noise of the same point run on the exe through the oracle, its lattice made as the game makes it once
	struct Case
	{
		float x, y, z;
		uint32_t bits;
	};
	constexpr std::array<Case, 8> k_Cases = {{
	    {0.0f, 0.0f, 0.0f, 0x3EDC72DCu},
	    {0.5f, 0.25f, 0.75f, 0x3E9957D9u},
	    {1.3f, -2.7f, 4.1f, 0x3E2BBF35u},
	    {-0.4f, 10.6f, 3.3f, 0x3E3D6141u},
	    {123.456f, -78.9f, 0.001f, 0x3E1E426Bu},
	    {7.0f, 7.0f, 7.0f, 0x3E70ECF0u},
	    {255.5f, 256.25f, -300.75f, 0x3E896BCAu},
	    {0.0123f, 0.98f, -0.5f, 0x3D80886Au},
	}};
	for (const auto& c : k_Cases)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(noise::ValueNoise3(c.x, c.y, c.z)), c.bits) << c.x << ", " << c.y << ", " << c.z;
	}
	// a cell's corner is its lattice value
	EXPECT_EQ(noise::ValueNoise3(3.0f, 4.0f, 5.0f), noise::Lattice3(3, 4, 5));
}

TEST(ParticleNoise, TheLinearNoiseIsTheOriginals)
{
	// The lattice noise the burning objects' glow reads, blended linearly between the two whole numbers about x: each
	// case is the original's noise of the same x run on the exe through the oracle, its lattice made as the game makes it
	// once
	struct Case
	{
		float x;
		uint32_t bits;
	};
	constexpr std::array<Case, 18> k_Cases = {{
	    {0.0f, 0xBE6A4CE8u},
	    {0.25f, 0xBE43C4C2u},
	    {0.5f, 0xBE1D3C9Cu},
	    {0.999f, 0xBDA0A78Au},
	    {1.0f, 0xBDA058A0u},
	    {1.5f, 0xBEE77CE8u},
	    {-0.5f, 0x3DDF90E0u},
	    {-1.25f, 0x3E2E62ACu},
	    {3.7f, 0x3E5CCA7Cu},
	    {17.3f, 0x3D5F2DA0u},
	    {255.5f, 0x3DDF90E0u},
	    {256.0f, 0xBE6A4CE8u},
	    {1000.6f, 0x3F090795u},
	    {12345.678f, 0x3F21C279u},
	    {40000.4f, 0xBE7CE29Eu},
	    {65535.9f, 0xBE240094u},
	    {100000.3f, 0xBEC2A4D3u},
	    {-7.9f, 0xBCD3AD9Fu},
	}};
	for (const auto& c : k_Cases)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(noise::LinearValueNoise(c.x)), c.bits) << c.x;
	}
	// a whole number is its lattice value
	EXPECT_EQ(noise::LinearValueNoise(9.0f), noise::Lattice(9));
}
