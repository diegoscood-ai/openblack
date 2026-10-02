/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fire's graphic (ECS/Fire/FireGraphic): the charring grey of fn_00730570, checked against the original's integer
// code (0x730585..0x7305D7) for every charring byte.

#include <cstdint>

#include <gtest/gtest.h>

#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireGraphic.h"

using namespace openblack::ecs;

namespace
{
/// fn_00730570's blue byte as the exe computes it: ecx = -(k x 7 x 25), shr 8, dec (0x7305A4..0x7305B9)
uint8_t OriginalBlue(uint32_t k)
{
	const uint32_t ecx = 0u - k * 7u * 25u;
	return static_cast<uint8_t>((ecx >> 8) - 1u);
}
/// the green byte: (ecx << 8) >> 8, - 0x100, the byte ch (0x730585..0x7305A2)
uint8_t OriginalGreen(uint32_t k)
{
	const uint32_t ecx = (((0u - k * 7u * 25u) << 8) >> 8) - 0x100u;
	return static_cast<uint8_t>(ecx >> 8);
}
/// the red byte: (ecx << 16) >> 8, - 0x10000, bits 16..23 (0x7305BB..0x7305D7)
uint8_t OriginalRed(uint32_t k)
{
	const uint32_t eax = (((0u - k * 7u * 25u) << 16) >> 8) - 0x10000u;
	return static_cast<uint8_t>(eax >> 16);
}
} // namespace

TEST(FireGraphic, CharringGreyMatchesTheExe)
{
	fire::FireEffect effect;
	for (uint32_t k = 0; k < 256; ++k)
	{
		// a charring whose ftol(x 255) is exactly k
		effect.charring = static_cast<float>(k) / 255.0f;
		ASSERT_EQ(static_cast<uint32_t>(static_cast<int>(effect.charring * 255.0f) & 0xFF), k);
		const auto grey = fire::graphic::CharringGrey(effect);
		EXPECT_EQ(grey, OriginalBlue(k)) << "k " << k;
		EXPECT_EQ(grey, OriginalGreen(k)) << "k " << k;
		EXPECT_EQ(grey, OriginalRed(k)) << "k " << k;
	}
}

TEST(FireGraphic, CharringGreyEnds)
{
	fire::FireEffect effect;
	effect.charring = 0.0f;
	EXPECT_EQ(fire::graphic::CharringGrey(effect), 255);
	effect.charring = 1.0f;
	EXPECT_EQ(fire::graphic::CharringGrey(effect), 80); // 255 - ceil(175 x 255 / 256)
}
