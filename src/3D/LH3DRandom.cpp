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
