/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The thrown fireball's direction, pinned to the original's bits: each value is the original's own pitching up of the
// hand's direction, run whole in the original executable, on inputs this test makes again with the same generator

#include <cstddef>
#include <cstdint>

#include <array>
#include <bit>
#include <iterator>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "FireballDirectionGolden.h"
#include "Particles/Rules/Fireball.h"
#include "support/OracleCaseGenerator.h"

namespace golden = openblack::test::fireball_direction_golden;
using openblack::particles::fireball_rules::PitchedUp;
using openblack::test::OracleCaseGenerator;

namespace
{

[[nodiscard]] uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

} // namespace

TEST(FireballDirection, ThePitchedUpDirectionGivesTheOriginalsBits)
{
	// none, then the five axes but up, then directions with each part from -1 to 1
	constexpr std::array<glm::vec3, 6> k_Axes = {glm::vec3(0.0f, 0.0f, 0.0f),  glm::vec3(1.0f, 0.0f, 0.0f),
	                                             glm::vec3(0.0f, 1.0f, 0.0f),  glm::vec3(0.0f, -1.0f, 0.0f),
	                                             glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(-1.0f, 0.0f, 0.0f)};
	OracleCaseGenerator generator {0x46495245U};
	int mismatches = 0;
	for (size_t i = 0; i < std::size(golden::k_Directions); ++i)
	{
		glm::vec3 d;
		if (i < k_Axes.size())
		{
			d = k_Axes.at(i);
		}
		else
		{
			d.x = generator.Signed(13);
			d.y = generator.Signed(13);
			d.z = generator.Signed(13);
		}
		const glm::vec3 direction = PitchedUp(d);
		const auto& expected = golden::k_Directions[i];
		if ((Bits(direction.x) != expected[0] || Bits(direction.y) != expected[1] || Bits(direction.z) != expected[2]) &&
		    ++mismatches <= 10)
		{
			ADD_FAILURE() << "case " << i << ": " << std::hex << Bits(direction.x) << " " << Bits(direction.y) << " "
			              << Bits(direction.z) << ", the original's " << expected[0] << " " << expected[1] << " "
			              << expected[2];
		}
	}
	EXPECT_EQ(mismatches, 0);
}

TEST(FireballDirection, AWindyAtomIsPulledTowardsTheWind)
{
	// UpdateRuleGravityWithFloor with its wind on: the velocity moves towards the wind times WindMagnification times
	// 0.1, by Damping times the difference times the step, as the original's rule (its default magnification 100)
	const auto v =
	    openblack::particles::fireball_rules::WindVelocity({0.0f, -2.0f, 3.0f}, {1.0f, 0.0f, 0.5f}, 100.0f, 0.5f, 0.1f);
	EXPECT_FLOAT_EQ(v.x, (1.0f * 100.0f * 0.1f) * 0.5f * 0.1f);
	EXPECT_FLOAT_EQ(v.y, -2.0f + 2.0f * 0.5f * 0.1f);
	EXPECT_FLOAT_EQ(v.z, 3.0f + (0.5f * 100.0f * 0.1f - 3.0f) * 0.5f * 0.1f);
}

TEST(FireballRules, AlphaWithHeightAboveTheLand)
{
	using openblack::particles::fireball_rules::AlphaWithHeight;
	// below the land: the alpha at zero; above the reference height: the alpha there; a straight line in between
	EXPECT_EQ(AlphaWithHeight(200.0f, 50.0f, 100.0f, -1.0f), 200);
	EXPECT_EQ(AlphaWithHeight(200.0f, 50.0f, 100.0f, 0.0f), 200);
	EXPECT_EQ(AlphaWithHeight(200.0f, 50.0f, 100.0f, 25.0f), 162); // 162.5 truncated
	EXPECT_EQ(AlphaWithHeight(200.0f, 50.0f, 100.0f, 100.0f), 50);
	EXPECT_EQ(AlphaWithHeight(200.0f, 50.0f, 100.0f, 1000.0f), 50);
	// fading in with height
	EXPECT_EQ(AlphaWithHeight(0.0f, 255.0f, 10.0f, 5.0f), 127); // 127.5 truncated
	// kept in 0..255
	EXPECT_EQ(AlphaWithHeight(-40.0f, 300.0f, 100.0f, 0.0f), 0);
	EXPECT_EQ(AlphaWithHeight(-40.0f, 300.0f, 100.0f, 200.0f), 255);
}

// A thrown ball's whoosh: small, medium or large by its speed over 200 (more than 0.6 large, more than 0.3 medium)
TEST(FireballRules, AThrownBallsWhooshIsSizedByItsSpeed)
{
	using openblack::particles::fireball_rules::ThrowSoundSize;
	EXPECT_EQ(ThrowSoundSize(0.0f), 3);
	EXPECT_EQ(ThrowSoundSize(50.0f), 3);
	EXPECT_EQ(ThrowSoundSize(70.0f), 2);
	EXPECT_EQ(ThrowSoundSize(110.0f), 2);
	EXPECT_EQ(ThrowSoundSize(130.0f), 1);
	EXPECT_EQ(ThrowSoundSize(200.0f), 1);
}
