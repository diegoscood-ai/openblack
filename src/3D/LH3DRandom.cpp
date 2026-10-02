/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LH3DRandom.h"

#include <cstdint>

#include <random>

using namespace openblack::graphics;

namespace
{
constexpr float k_InverseRandMax = 3.0518509e-05f; ///< [0x9A3700], 1 / 32767
uint32_t g_Seed = 1;                               ///< (aproximado) the CRT's seed before any srand
} // namespace

float lh3d::Random(float a, float b) noexcept
{
	// rand() 0x7C8837
	g_Seed = g_Seed * 214013u + 2531011u;
	const auto r = static_cast<float>((g_Seed >> 16u) & 0x7FFFu);
	// 0x81D180: rand() x [0x9A3700] x (b - a) + a
	return r * k_InverseRandMax * (b - a) + a;
}

namespace
{
std::mt19937 g_LocalStream(0x5EED); ///< (aproximado) not LHRand's generator nor its seed
} // namespace

int openblack::grand_local::LocalRand(int count) noexcept
{
	// 0x6DE574: 0 for 0
	if (count <= 0)
	{
		return 0;
	}
	return static_cast<int>(g_LocalStream() % static_cast<uint32_t>(count));
}

float openblack::grand_local::LocalFloatRand(float max) noexcept
{
	// 0x6DE597..0x6DE5AD: 0 for 0
	if (max == 0.0f)
	{
		return 0.0f;
	}
	// 0x6DE5B9..0x6DE5DA: LHRand(0xFFFF), 0 .. 0xFFFE, x max x [0x8D6050] (1 / 65535)
	const auto r = static_cast<float>(g_LocalStream() % 0xFFFFu);
	return r * max * (1.0f / 65535.0f);
}
