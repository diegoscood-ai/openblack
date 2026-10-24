/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The lightning bolt's pure maths (Particles/LightningMaths): the cells it searches, the forks it keeps, where two
// bolts clash, and the electric arcs over what it struck, pinned to the original's own code.

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <numbers>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Particles/LightningMaths.h"

using namespace openblack::particles::maths;

TEST(LightningMaths, SearchesASquareOfCellsTwiceItsRadiusAcross)
{
	EXPECT_EQ(StrikeSearchCells(60.0f), 144u);
	EXPECT_EQ(StrikeSearchCells(100.0f), 400u);
	EXPECT_EQ(StrikeSearchCells(140.0f), 784u);
	EXPECT_EQ(StrikeSearchCells(5.0f), 4u);
	EXPECT_EQ(StrikeSearchCells(20.0f), 16u);
}

TEST(LightningMaths, ABoltFromACloudSearchesASquareItsRadiusAcross)
{
	EXPECT_EQ(CloudSearchCells(60.0f), 36u);
	EXPECT_EQ(CloudSearchCells(25.0f), 9u);
	EXPECT_EQ(CloudSearchCells(5.0f), 1u);
}

TEST(LightningMaths, ASearchOfNoRadiusSearchesNoCell)
{
	EXPECT_EQ(StrikeSearchCells(0.0f), 0u);
	EXPECT_EQ(CloudSearchCells(0.0f), 0u);
	// Just below 0 the side rounds up to 0
	EXPECT_EQ(StrikeSearchCells(-5.0f), 0u);
	// From -10 down the negative side is squared
	EXPECT_EQ(StrikeSearchCells(-15.0f), 4u);
	EXPECT_EQ(CloudSearchCells(-25.0f), 4u);
	// A side that doesn't fit the integer searches none
	EXPECT_EQ(StrikeSearchCells(std::nanf("")), 0u);
}

TEST(LightningMaths, KeepsTwoForksForEachTarget)
{
	EXPECT_EQ(ForkCount(4, 6), 12u);
	EXPECT_EQ(ForkCount(10, 3), 6u);
	EXPECT_EQ(ForkCount(20, 28), 56u);
	EXPECT_EQ(ForkCount(10, 0), 0u);
}

TEST(LightningMaths, ABoltThatFindsACreatureStrikesOnlyCreatures)
{
	// No creature in sight: the targets stay as they are
	EXPECT_TRUE(CreatureRetargets({false, false, false}).empty());
	EXPECT_TRUE(CreatureRetargets({}).empty());
	// Creatures at 1 and 3 among five targets: each target takes the next of them in turn
	EXPECT_EQ(CreatureRetargets({false, true, false, true, false}), (std::vector<size_t> {1, 3, 1, 3, 1}));
	// One creature: every target strikes it
	EXPECT_EQ(CreatureRetargets({false, false, true}), (std::vector<size_t> {2, 2, 2}));
}

TEST(LightningMaths, BoltsPointingTheSameWayClash)
{
	const float north = std::numbers::pi_v<float> / 2.0f;
	const BoltPose older {.origin = {-15.0f, 10.0f, 0.0f}, .centroid = {0.0f, 2.0f, 40.0f}, .heading = north};
	const BoltPose newer {.origin = {-5.0f, 10.0f, 0.0f}, .centroid = {0.0f, 2.0f, 40.0f}, .heading = north};
	const auto meeting = ClashPoint(newer, older, 60.0f);
	ASSERT_TRUE(meeting.has_value());
	// On the way from the older bolt's middle back towards its origin, pulled towards the newer one's heading
	EXPECT_NEAR(meeting->x, -4.9643f, 1e-3f);
	EXPECT_NEAR(meeting->y, 4.6476f, 1e-3f);
	EXPECT_NEAR(meeting->z, 12.3778f, 1e-3f);
	// Out of reach (13.5 m away), or pointing apart, they don't
	EXPECT_FALSE(ClashPoint(newer, older, 13.0f).has_value());
	EXPECT_TRUE(ClashPoint(newer, older, 14.0f).has_value());
	BoltPose away = newer;
	away.heading = -north;
	EXPECT_FALSE(ClashPoint(away, older, 60.0f).has_value());
}

TEST(LightningMaths, TheMeetingMustLieInFrontOnceTheWayIsUnitLength)
{
	// The meeting point is 23.5 m away and 63 degrees off the newer bolt's heading: wider than the widest angle once
	// the way to it is unit length, though its unscaled dot with the heading (10.8) would pass
	const float north = std::numbers::pi_v<float> / 2.0f;
	const BoltPose older {.origin = {-15.0f, 10.0f, 0.0f}, .centroid = {0.0f, 2.0f, 40.0f}, .heading = north};
	const BoltPose newer {.origin = {15.0f, 10.0f, 0.0f}, .centroid = {0.0f, 2.0f, 40.0f}, .heading = north};
	EXPECT_FALSE(ClashPoint(newer, older, 60.0f).has_value());
}

TEST(LightningMaths, AClashedForkIsThickerAndStrikesTwiceAsHard)
{
	EXPECT_EQ(k_ClashForkScale, 3.0f);
	EXPECT_EQ(k_ClashStrength, 2.0f);
	EXPECT_EQ(static_cast<float>(std::cos(k_ClashWidestAngle)), static_cast<float>(std::cos(1.036725640296936)));
}

namespace
{
using Bits3 = std::array<uint32_t, 3>;

glm::vec3 Vec(const Bits3& bits)
{
	return {std::bit_cast<float>(bits[0]), std::bit_cast<float>(bits[1]), std::bit_cast<float>(bits[2])};
}

/// The original's arc lay, run on its own code (the oracle): the ends and the rule's shares, then each ball draw it made
/// (the two tangents' nudges, then one per joint) and the joints it laid, all as float bits
struct LayCase
{
	Bits3 from;
	Bits3 to;
	Bits3 fromNormal;
	Bits3 toNormal;
	uint32_t randomFrac;
	uint32_t scaleTangents;
	uint32_t randomTangents;
	std::vector<Bits3> draws;
	std::vector<Bits3> joints;
};
const std::vector<LayCase> k_LayCases {
    LayCase {.from = Bits3 {0x44E16CCD, 0x41DC0000, 0x45255800},
             .to = Bits3 {0x44E13CCD, 0x41FA0000, 0x45257200},
             .fromNormal = Bits3 {0x3F19999A, 0x00000000, 0x3F4CCCCD},
             .toNormal = Bits3 {0x00000000, 0x3F800000, 0x00000000},
             .randomFrac = 0x3DCCCCCD,
             .scaleTangents = 0x404365FE,
             .randomTangents = 0x3E4CCCCD,
             .draws = {Bits3 {0x3D713100, 0x3D8DD890, 0xBED3F6D4}, Bits3 {0xBE0D748C, 0xBE99029A, 0x3F3A17BA},
                       Bits3 {0xBE9DB29E, 0x3F032784, 0x3EBED2C0}, Bits3 {0x3EFDBEFC, 0xBF271DA7, 0x3DB678B0},
                       Bits3 {0x3F06B586, 0xBE934A94, 0xBDB698B8}, Bits3 {0xBDE678E8, 0x3C0BC080, 0x3F4F69D0},
                       Bits3 {0xBEE596E6, 0xBE5074D0, 0xBD0AF090}, Bits3 {0xBF241FA4, 0xBE9FAAA0, 0xBF0B578C},
                       Bits3 {0xBE96CE96, 0x3EED92EC, 0x3F1BA19C}, Bits3 {0xBF3E01BE, 0x3E8FB690, 0xBE335CB4}},
             .joints = {Bits3 {0x44E16883, 0x41DDC8C9, 0x45255A99}, Bits3 {0x44E18C90, 0x41D9CB3F, 0x45256987},
                        Bits3 {0x44E1913C, 0x41DB54A6, 0x452570F7}, Bits3 {0x44E17DFD, 0x41DD8CD1, 0x452579FA},
                        Bits3 {0x44E16667, 0x41DFA59A, 0x45257337}, Bits3 {0x44E14E46, 0x41E47A69, 0x45256D70},
                        Bits3 {0x44E14125, 0x41EF7C98, 0x45257438}, Bits3 {0x44E13276, 0x41FAFA43, 0x452570C8}}},
    LayCase {.from = Bits3 {0x00000000, 0x00000000, 0x00000000},
             .to = Bits3 {0x40800000, 0x00000000, 0x00000000},
             .fromNormal = Bits3 {0x00000000, 0x3F800000, 0x00000000},
             .toNormal = Bits3 {0x00000000, 0xBF800000, 0x00000000},
             .randomFrac = 0x3DCCCCCD,
             .scaleTangents = 0x40400000,
             .randomTangents = 0x3E4CCCCD,
             .draws = {Bits3 {0xBEB002B0, 0x3D17D0A0, 0x3F3EC5BE}, Bits3 {0xBEC876C8, 0x3D05D080, 0xBF09438A},
                       Bits3 {0xBF3CE5BD, 0xBE815E82, 0x3DECF8F0}, Bits3 {0xBDAC68B0, 0xBF41F1C2, 0x3D1690A0},
                       Bits3 {0x3DFB9900, 0xBEA6EEA6, 0x3F6D7DEE}, Bits3 {0x3E6FD4F0, 0xBD5850E0, 0x3E573CD8},
                       Bits3 {0xBE991E9A, 0xBD8D4890, 0xBF0A7D8A}, Bits3 {0xBEC12EC2, 0xBE806E80, 0xBEE40EE4},
                       Bits3 {0x3F3867B8, 0xBECF22D0, 0x3EA27AA4}, Bits3 {0xBEE08AE0, 0x3E916A90, 0x3D8A1890}},
             .joints = {Bits3 {0xBE971E31, 0xBDCEFD9D, 0x3D3D93F3}, Bits3 {0x3DF123D7, 0x3F964FEE, 0x3E665364},
                        Bits3 {0x3F46D387, 0x4014ECE4, 0x3F34F2C0}, Bits3 {0x3FD35A5A, 0x403AFFEA, 0x3EF0586A},
                        Bits3 {0x401675C1, 0x403A35FB, 0x3E1B0A2A}, Bits3 {0x404946C4, 0x4015E9E1, 0x3DE8DCA0},
                        Bits3 {0x4084D2F5, 0x3FA6838A, 0x3E962978}, Bits3 {0x4074C5DB, 0x3DE8AA80, 0x3CDCF41A}}},
    LayCase {.from = Bits3 {0x44B22000, 0x427B999A, 0x4511BE66},
             .to = Bits3 {0x44B25000, 0x42700000, 0x4511A000},
             .fromNormal = Bits3 {0x3F13B646, 0x3F13B646, 0x3F13B646},
             .toNormal = Bits3 {0xBF350481, 0x00000000, 0x3F350481},
             .randomFrac = 0x3E4CCCCD,
             .scaleTangents = 0x3F800000,
             .randomTangents = 0x3F4CCCCD,
             .draws = {Bits3 {0xBD4FB0D0, 0x3EF63AF8, 0x3ED82AD8}, Bits3 {0x3EB802B8, 0xBEAFAAB0, 0xBF59B1DA},
                       Bits3 {0x3F12A992, 0xBDE138E0, 0xBF0E7F8E}, Bits3 {0x3E9C9E9C, 0xBF2A13AA, 0xBDEF28F0},
                       Bits3 {0xBEBE46BE, 0xBF1A0D9A, 0x3EFD26FC}, Bits3 {0xBF4F1DCF, 0xBEF376F4, 0xBE4034C0},
                       Bits3 {0x3F011B82, 0xBDA178A0, 0xBE038484}},
             .joints = {Bits3 {0x44B22DDA, 0x427B4486, 0x4511B7AB}, Bits3 {0x44B23A64, 0x427A048B, 0x4511BFF0},
                        Bits3 {0x44B23D76, 0x427650FC, 0x4511BBE2}, Bits3 {0x44B23F1D, 0x4271A3CF, 0x4511A4D8},
                        Bits3 {0x44B25C31, 0x426FC2FF, 0x45119E73}}},
    LayCase {.from = Bits3 {0x41200000, 0x40000000, 0xC0400000},
             .to = Bits3 {0x41200000, 0x40000000, 0xC0400000},
             .fromNormal = Bits3 {0x00000000, 0x3F800000, 0x00000000},
             .toNormal = Bits3 {0x3F800000, 0x00000000, 0x00000000},
             .randomFrac = 0x3DCCCCCD,
             .scaleTangents = 0x40400000,
             .randomTangents = 0x3E4CCCCD,
             .draws = {Bits3 {0xBF2B8DAC, 0x3ED736D8, 0x3F02D582}, Bits3 {0x3F6645E6, 0x3DE178E0, 0x3EB976B8},
                       Bits3 {0xBF1F43A0, 0x3E4F6CD0, 0x3EC32AC4}, Bits3 {0xBE090C88, 0xBE25FCA8, 0xBEC96ECA},
                       Bits3 {0x3D940890, 0xBED76ED8, 0x3F4DEBCE}, Bits3 {0x3EBBFEBC, 0x3EB682B8, 0xBE2BC4AC},
                       Bits3 {0x3F0CE98C, 0x3F20E5A0, 0x3EE82AE8}, Bits3 {0xBF1A779A, 0x3D5290E0, 0xBED50ED6},
                       Bits3 {0xBE215CA0, 0x3EC356C4, 0x3EC8CAC8}, Bits3 {0x3F37E1B8, 0xBE45ECC4, 0xBF1EB99E},
                       Bits3 {0xBE97EE98, 0x3E2AD4A8, 0x3F396BBA}, Bits3 {0xBE832E84, 0xBEBC96BC, 0xBD8DA890},
                       Bits3 {0xBD1FB0A0, 0x3F738BF4, 0xBE8ADE8A}, Bits3 {0x3E97BE98, 0x3F3461B4, 0x3F0F6390},
                       Bits3 {0x3EE672E8, 0xBE8CA68C, 0x3F2459A4}, Bits3 {0xBD787100, 0x3F7173F2, 0x3E82F684},
                       Bits3 {0xBD5F30E0, 0x3F160196, 0x3E4374C0}, Bits3 {0xBEEEBAEE, 0xBE9C929C, 0xBF4093C0},
                       Bits3 {0xBECA1ACA, 0xBE878288, 0x3E49ACC8}, Bits3 {0x3F34AFB4, 0x3ECEF6D0, 0xBEEC86EC},
                       Bits3 {0xBD987898, 0xBF6BA7EC, 0xBE6094E0}},
             .joints = {Bits3 {0x41200000, 0x40000000, 0xC0400000}, Bits3 {0x41200000, 0x40000000, 0xC0400000},
                        Bits3 {0x41200000, 0x40000000, 0xC0400000}, Bits3 {0x41200000, 0x40000000, 0xC0400000},
                        Bits3 {0x41200000, 0x40000000, 0xC0400000}, Bits3 {0x41200000, 0x40000000, 0xC0400000},
                        Bits3 {0x41200000, 0x40000000, 0xC0400000}, Bits3 {0x41200000, 0x40000000, 0xC0400000},
                        Bits3 {0x41200000, 0x40000000, 0xC0400000}, Bits3 {0x41200000, 0x40000000, 0xC0400000},
                        Bits3 {0x41200000, 0x40000000, 0xC0400000}, Bits3 {0x41200000, 0x40000000, 0xC0400000},
                        Bits3 {0x41200000, 0x40000000, 0xC0400000}, Bits3 {0x41200000, 0x40000000, 0xC0400000},
                        Bits3 {0x41200000, 0x40000000, 0xC0400000}, Bits3 {0x41200000, 0x40000000, 0xC0400000},
                        Bits3 {0x41200000, 0x40000000, 0xC0400000}, Bits3 {0x41200000, 0x40000000, 0xC0400000},
                        Bits3 {0x41200000, 0x40000000, 0xC0400000}}},
};
} // namespace

TEST(LightningMaths, ArcsRunFromEndToEnd)
{
	const ArcEnds ends {.from = {0.0f, 0.0f, 0.0f},
	                    .to = {4.0f, 0.0f, 0.0f},
	                    .fromNormal = {0.0f, 1.0f, 0.0f},
	                    .toNormal = {0.0f, -1.0f, 0.0f}};
	EXPECT_EQ(ArcLength(ends), 4.0f);
	const auto fromTangent = ArcTangent(ends.fromNormal, glm::vec3(0.0f), 0.2f, 4.0f, 3.0f);
	EXPECT_EQ(fromTangent, glm::vec3(0.0f, 12.0f, 0.0f));
	const std::vector<glm::vec3> still(8, glm::vec3(0.0f));
	const auto joints = ArcJoints(ends, fromTangent, ArcTangent(ends.toNormal, glm::vec3(0.0f), 0.2f, 4.0f, 3.0f), still, 0.1f);
	ASSERT_EQ(joints.size(), 8u);
	EXPECT_EQ(joints.front(), ends.from);
	EXPECT_EQ(joints.back(), ends.to);
	// It leaves upwards along the first normal
	EXPECT_GT(joints[1].y, 0.0f);
	// The jitter moves every joint by its share of the arc's length
	const std::vector<glm::vec3> jitters(8, glm::vec3(1.0f, 0.0f, 0.0f));
	const auto jittered = ArcJoints(ends, fromTangent, glm::vec3(0.0f), jitters, 0.1f);
	EXPECT_NEAR(jittered.front().x, 0.4f, 1e-6f);
}

TEST(LightningMaths, ArcsAreLaidAsTheOriginal)
{
	for (const auto& c : k_LayCases)
	{
		const ArcEnds ends {.from = Vec(c.from), .to = Vec(c.to), .fromNormal = Vec(c.fromNormal), .toNormal = Vec(c.toNormal)};
		const float length = ArcLength(ends);
		const float randomTangents = std::bit_cast<float>(c.randomTangents);
		const float scaleTangents = std::bit_cast<float>(c.scaleTangents);
		ASSERT_EQ(c.draws.size(), c.joints.size() + 2);
		const auto fromTangent = ArcTangent(ends.fromNormal, Vec(c.draws[0]), randomTangents, length, scaleTangents);
		const auto toTangent = ArcTangent(ends.toNormal, Vec(c.draws[1]), randomTangents, length, scaleTangents);
		std::vector<glm::vec3> jitters;
		for (size_t k = 2; k < c.draws.size(); ++k)
		{
			jitters.push_back(Vec(c.draws[k]));
		}
		const auto joints = ArcJoints(ends, fromTangent, toTangent, jitters, std::bit_cast<float>(c.randomFrac));
		ASSERT_EQ(joints.size(), c.joints.size());
		for (size_t k = 0; k < joints.size(); ++k)
		{
			for (int i = 0; i < 3; ++i)
			{
				EXPECT_EQ(std::bit_cast<uint32_t>(joints[k][i]), c.joints[k][static_cast<size_t>(i)]) << k << " " << i;
			}
		}
	}
}

TEST(LightningMaths, AnArcPointGoesThroughItsModelsMatrix)
{
	// The columns are the model's axes: x along z, y up, z along -x, the model 10 m east
	const glm::mat3 axes(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f));
	const glm::vec3 position(10.0f, 0.0f, 0.0f);
	EXPECT_EQ(ArcSurfacePoint(axes, position, {1.0f, 2.0f, 3.0f}), glm::vec3(7.0f, 2.0f, 1.0f));
	// The normal takes the translation too before it is made unit length: it points along the model's position
	EXPECT_EQ(ArcSurfaceNormal(axes, position, {0.0f, 0.0f, 0.0f}), glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_EQ(ArcSurfaceNormal(axes, glm::vec3(0.0f), {0.0f, 0.0f, 0.0f}), glm::vec3(0.0f));
	const auto normal = ArcSurfaceNormal(axes, glm::vec3(0.0f), {0.0f, 3.0f, 0.0f});
	EXPECT_EQ(normal, glm::vec3(0.0f, 1.0f, 0.0f));
}
