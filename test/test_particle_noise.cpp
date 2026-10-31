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
