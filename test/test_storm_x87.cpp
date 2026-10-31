/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The tornado's funnel pinned to the original's bits: each value is the original's own function run whole in the
// original executable, on inputs this test makes again with the same generator

#include <cstddef>
#include <cstdint>

#include <array>
#include <bit>
#include <iterator>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Particles/Rules/Storm.h"
#include "TornadoGolden.h"
#include "support/OracleCaseGenerator.h"

namespace golden = openblack::test::tornado_golden;
namespace storm = openblack::particles::storm;
using openblack::test::OracleCaseGenerator;

namespace
{

[[nodiscard]] uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

} // namespace

TEST(TornadoBits, TheFlyingAtomsSpinGivesTheOriginalsBits)
{
	OracleCaseGenerator generator {0x54484554U};
	int mismatches = 0;
	for (size_t i = 0; i < std::size(golden::k_ThetaDots); ++i)
	{
		// ThetaBias and h at their ends and between, then any; the speeds from -16 to 16, the atom's speed from 0.5 to
		// 1.5 and the strength from 0 to 1
		float bias;
		float h;
		float base = 2.0f;
		float top = 7.5f;
		float speed = 1.0f;
		float strength = 1.0f;
		if (i < 9)
		{
			constexpr std::array<float, 3> k_Bias = {0.0f, 1.0f, 0.5f};
			constexpr std::array<float, 3> k_H = {0.0f, 1.0f, 0.25f};
			bias = k_Bias.at(i / 3);
			h = k_H.at(i % 3);
		}
		else
		{
			bias = (generator.Next() & 7U) != 0 ? generator.Unit(10) : generator.Unit(24);
			h = generator.Unit(24);
			base = generator.Signed(12) * 16.0f;
			top = generator.Signed(12) * 16.0f;
			speed = 0.5f + generator.Unit(10);
			strength = generator.Unit(10);
		}
		const uint32_t spin = Bits(storm::ThetaDot(h, bias, base, top, speed, strength));
		if (spin != golden::k_ThetaDots[i] && ++mismatches <= 10)
		{
			ADD_FAILURE() << "case " << i << " bias " << bias << " h " << h << ": " << std::hex << spin << ", the original's "
			              << golden::k_ThetaDots[i];
		}
	}
	EXPECT_EQ(mismatches, 0);
}

TEST(TornadoBits, TheFunnelsBendGivesTheOriginalsBits)
{
	// a centre line from (0, 0, 0) to (1, 1, 1) with no wiggle: its x is the gain
	OracleCaseGenerator generator {0x4741494EU};
	int mismatches = 0;
	for (size_t i = 0; i < std::size(golden::k_Gains); ++i)
	{
		float bend;
		float h;
		if (i < 15)
		{
			constexpr std::array<float, 3> k_Bend = {0.0f, 1.0f, 0.5f};
			constexpr std::array<float, 5> k_H = {0.0f, 1.0f, 0.5f, 0.25f, 0.75f};
			bend = k_Bend.at(i / 5);
			h = k_H.at(i % 5);
		}
		else
		{
			bend = (generator.Next() & 7U) != 0 ? generator.Unit(10) : generator.Unit(24);
			h = (generator.Next() & 15U) != 0 ? generator.Unit(24) : generator.Signed(10) * 2.0f;
		}
		const storm::FunnelLine line {.base = glm::vec3(0.0f),
		                              .top = glm::vec3(1.0f),
		                              .bend = bend,
		                              .wiggleCount = 0,
		                              .wiggleAmplitude = 0.0f,
		                              .tornadoScale = 1.0f};
		const uint32_t gain = Bits(storm::FunnelCentre(h, line).x);
		if (gain != golden::k_Gains[i] && ++mismatches <= 10)
		{
			ADD_FAILURE() << "case " << i << " bend " << bend << " h " << h << ": " << std::hex << gain << ", the original's "
			              << golden::k_Gains[i];
		}
	}
	EXPECT_EQ(mismatches, 0);
}

TEST(TornadoBits, TheFunnelsCentreLineGivesTheOriginalsBits)
{
	// any bend, height and wiggle of 0 to 8 half turns, on lines of any slant somewhere on a 4 km map
	OracleCaseGenerator generator {0x43454E54U};
	int mismatches = 0;
	for (size_t i = 0; i < std::size(golden::k_Centres); ++i)
	{
		storm::FunnelLine line;
		line.bend = generator.Unit(10);
		const float h = generator.Unit(24);
		line.wiggleCount = static_cast<int>(generator.Next() % 9U);
		line.wiggleAmplitude = generator.Unit(12) * 8.0f;
		line.tornadoScale = 0.25f + generator.Unit(10) * 2.0f;
		line.base.x = generator.Unit(20) * 4096.0f;
		line.base.y = generator.Unit(14) * 64.0f;
		line.base.z = generator.Unit(20) * 4096.0f;
		line.top.x = line.base.x + generator.Signed(14) * 64.0f;
		line.top.y = line.base.y + generator.Unit(14) * 256.0f;
		line.top.z = line.base.z + generator.Signed(14) * 64.0f;
		const glm::vec3 centre = storm::FunnelCentre(h, line);
		const auto& expected = golden::k_Centres[i];
		if ((Bits(centre.x) != expected[0] || Bits(centre.y) != expected[1] || Bits(centre.z) != expected[2]) &&
		    ++mismatches <= 10)
		{
			ADD_FAILURE() << "case " << i << ": " << std::hex << Bits(centre.x) << " " << Bits(centre.y) << " "
			              << Bits(centre.z) << ", the original's " << expected[0] << " " << expected[1] << " " << expected[2];
		}
	}
	EXPECT_EQ(mismatches, 0);
}

TEST(TornadoBits, AFlyingAtomsNextPointGivesTheOriginalsBits)
{
	// atoms up to 64 m from the centre line (now and then on it or on its x axis), centres on a 4 km map, distances to
	// 96 m, radii to 48.5 m, spins to 8 and rises to 16 a second, at four steps
	constexpr std::array<uint32_t, 4> k_Steps = {0x3DCCCCCDU, 0x3D4CCCCDU, 0x3D088889U, 0x3D086595U};
	OracleCaseGenerator generator {0x4F524254U};
	int mismatches = 0;
	for (size_t i = 0; i < std::size(golden::k_OrbitPoints); ++i)
	{
		glm::vec3 d;
		d.x = generator.Signed(20) * 64.0f;
		d.y = generator.Signed(20) * 128.0f;
		d.z = generator.Signed(20) * 64.0f;
		if ((generator.Next() & 31U) == 0)
		{
			if ((generator.Next() & 1U) != 0)
			{
				d.x = 0.0f;
			}
			d.z = 0.0f;
		}
		glm::vec3 centre;
		centre.x = generator.Unit(22) * 4096.0f;
		centre.y = generator.Unit(16) * 300.0f;
		centre.z = generator.Unit(22) * 4096.0f;
		const float rho2 = generator.Unit(20) * 96.0f;
		const float r = 0.5f + generator.Unit(16) * 48.0f;
		const float w = generator.Signed(16) * 8.0f;
		const float vy = generator.Signed(16) * 16.0f;
		const float dt = std::bit_cast<float>(k_Steps.at(generator.Next() % k_Steps.size()));
		const glm::vec3 next = storm::OrbitPoint(d, centre, rho2, r, w, vy, dt);
		const auto& expected = golden::k_OrbitPoints[i];
		if ((Bits(next.x) != expected[0] || Bits(next.y) != expected[1] || Bits(next.z) != expected[2]) && ++mismatches <= 10)
		{
			ADD_FAILURE() << "case " << i << ": " << std::hex << Bits(next.x) << " " << Bits(next.y) << " " << Bits(next.z)
			              << ", the original's " << expected[0] << " " << expected[1] << " " << expected[2];
		}
	}
	EXPECT_EQ(mismatches, 0);
}
