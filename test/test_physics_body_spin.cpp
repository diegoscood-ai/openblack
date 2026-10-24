/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The share of a body's spin kept at each physics step, pinned to the original's bits: each value is the original's
// own setting up of a body's constants, run in the original executable

#include <cstddef>
#include <cstdint>

#include <bit>
#include <iterator>

#include <gtest/gtest.h>

#include "BodySpinGolden.h"
#include "Physics/Body.h"

namespace golden = openblack::test::body_spin_golden;
using openblack::physics::SpinKeptPerStep;

namespace
{

[[nodiscard]] float F(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

[[nodiscard]] uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

/// The share kept after a second of case i: 9999 floats stepped from 0.2 to 1, then 1, then the base materials
[[nodiscard]] float KeptAfterASecond(size_t i)
{
	constexpr size_t k_Stepped = 9999;
	constexpr uint32_t k_First = 0x3E4CCCCDU; // 0.2
	constexpr uint32_t k_Step = (0x3F800000U - k_First) / k_Stepped;
	if (i < k_Stepped)
	{
		return F(k_First + static_cast<uint32_t>(i) * k_Step);
	}
	if (i == k_Stepped)
	{
		return 1.0f;
	}
	return F(golden::k_Materials[i - k_Stepped - 1]);
}

} // namespace

TEST(PhysicsBodySpin, TheSpinKeptPerStepGivesTheOriginalsBits)
{
	static_assert(std::size(golden::k_KeptPerStep) == 10000 + std::size(golden::k_Materials));
	int mismatches = 0;
	for (size_t i = 0; i < std::size(golden::k_KeptPerStep); ++i)
	{
		const float keep = KeptAfterASecond(i);
		const uint32_t kept = Bits(SpinKeptPerStep(keep));
		if (kept != golden::k_KeptPerStep[i] && ++mismatches <= 10)
		{
			ADD_FAILURE() << "kept after a second " << keep << ": " << std::hex << kept << ", the original's "
			              << golden::k_KeptPerStep[i];
		}
	}
	EXPECT_EQ(mismatches, 0);
}
