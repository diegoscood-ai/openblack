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

// The widest angle's cosine stays unrounded in the original, as the FPU leaves it, and the dot is compared with it.
// The float nearest that cosine lies above it, so a dot of exactly that float is within the angle; a cosine rounded
// to a float first would leave it out.
TEST(LightningMaths, TheWidestAnglesCosineIsComparedUnrounded)
{
	EXPECT_TRUE(WithinClashAngle(std::bit_cast<float>(0x3F025089u)));
	EXPECT_FALSE(WithinClashAngle(std::bit_cast<float>(0x3F025088u)));
	EXPECT_EQ(std::bit_cast<uint32_t>(static_cast<float>(std::cos(k_ClashWidestAngle))), 0x3F025089u);
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

TEST(LightningMaths, AHandBoltPointsWhereTheCameraLooksEvenStraightDown)
{
	// The original's own numbers for the camera's forward x and z (the bits of each float): straight down, the signs of
	// the zeros pick 0, minus 0, pi or minus pi, never the heading of the step before
	struct Case
	{
		uint32_t x;
		uint32_t z;
		uint32_t heading;
	};
	constexpr std::array<Case, 6> k_Cases = {{
	    {.x = 0x00000000u, .z = 0x00000000u, .heading = 0x00000000u},
	    {.x = 0x80000000u, .z = 0x00000000u, .heading = 0x40490FDBu},
	    {.x = 0x00000000u, .z = 0x80000000u, .heading = 0x80000000u},
	    {.x = 0x80000000u, .z = 0x80000000u, .heading = 0xC0490FDBu},
	    {.x = 0x3F3504F3u, .z = 0x3F3504F3u, .heading = 0x3F490FDBu},
	    {.x = 0x3E99999Au, .z = 0xBF666666u, .heading = 0xBF9FE0BBu},
	}};
	for (const auto& c : k_Cases)
	{
		const glm::vec3 forward(std::bit_cast<float>(c.x), -1.0f, std::bit_cast<float>(c.z));
		EXPECT_EQ(std::bit_cast<uint32_t>(HandBoltHeading(forward)), c.heading) << c.x << " " << c.z;
	}
}

TEST(LightningMaths, ABoltNotFromAHandPointsAlongTheXAxis)
{
	// The storm's and the strikes' bolts: 0 whatever the camera looks at; from the hand, the camera's heading
	const glm::vec3 forward(0.3f, -0.5f, -0.9f);
	EXPECT_EQ(std::bit_cast<uint32_t>(BoltHeading(false, forward)), 0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(BoltHeading(false, glm::vec3(-1.0f, 0.0f, -0.0f))), 0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(BoltHeading(true, forward)), std::bit_cast<uint32_t>(HandBoltHeading(forward)));
}

// Where two bolts meet, as the original's clash test answers (tools/oracle, its link stubbed to a flag): the newer
// bolt's origin, middle and heading, the older one's, the reach; whether they clash, and the meeting point
TEST(LightningMaths, TheClashIsTheOriginals)
{
	struct Case
	{
		std::array<uint32_t, 15> in;
		bool clash;
		std::array<uint32_t, 3> meeting;
	};
	constexpr std::array<Case, 12> k_Cases {{
	    {{0x00000000u, 0x41200000u, 0x00000000u, 0x00000000u, 0x40000000u, 0x42200000u, 0x3fc90fdbu, 0x40a00000u, 0x41200000u,
	      0x00000000u, 0x40a00000u, 0x40000000u, 0x42200000u, 0x3fc90fdbu, 0x42c80000u},
	     true,
	     {0x40a00001u, 0x4099999au, 0x413b905eu}},
	    {{0x00000000u, 0x41200000u, 0x00000000u, 0x00000000u, 0x40000000u, 0x42200000u, 0x3fc90fdbu, 0x40a00000u, 0x41200000u,
	      0x00000000u, 0x40a00000u, 0x40000000u, 0xc2200000u, 0xbfc90fdbu, 0x42c80000u},
	     false,
	     {0u, 0u, 0u}},
	    {{0x00000000u, 0x41200000u, 0x00000000u, 0x00000000u, 0x40000000u, 0x42200000u, 0x3fc90fdbu, 0x40a00000u, 0x41200000u,
	      0x00000000u, 0x40a00000u, 0x40000000u, 0x42200000u, 0x3fc90fdbu, 0x40a00000u},
	     false,
	     {0u, 0u, 0u}},
	    {{0x446ef618u, 0x424794f0u, 0x4556cdc7u, 0x4467c414u, 0x421d47cfu, 0x4558328fu, 0x3ff32142u, 0x446250d3u, 0x4124ecaau,
	      0x455a3c6cu, 0x44529c1bu, 0x41b45b04u, 0x455cc027u, 0x401559dau, 0x439146a6u},
	     true,
	     {0x445a3df2u, 0x419217c7u, 0x455a4e26u}},
	    {{0x4557ce34u, 0x42243c41u, 0x44740cc0u, 0x45579fa9u, 0x42030915u, 0x447adb82u, 0x3fe9ad60u, 0x4558c476u, 0x42137ba7u,
	      0x44800ed0u, 0x45555deau, 0x41394246u, 0x448e72bau, 0x40030858u, 0x432555d6u},
	     true,
	     {0x455745dau, 0x41a3727fu, 0x4483ea52u}},
	    {{0x454e3096u, 0x4197c2edu, 0x44c17a7du, 0x454dc5afu, 0x410b8f0cu, 0x44b21f4eu, 0xbfd8b7a6u, 0x454f07c5u, 0x420e4a57u,
	      0x44bf8cbeu, 0x4547be7cu, 0x4101e769u, 0x44b1a7c2u, 0xc015c4b4u, 0x4353ea48u},
	     true,
	     {0x454abaf9u, 0x418dd272u, 0x44bd9d23u}},
	    {{0x44538533u, 0x3f97f3c0u, 0x45429f21u, 0x4467b403u, 0x4172b765u, 0x4540f853u, 0xbe7e7e61u, 0x44614786u, 0x41d46aaeu,
	      0x4543e652u, 0x446b00aeu, 0x41b6f006u, 0x454526feu, 0x3f0223e6u, 0x4350b739u},
	     true,
	     {0x4463e026u, 0x41c1415au, 0x4544f33du}},
	    {{0x4404329cu, 0x41fb36c9u, 0x422dc10bu, 0x440cc4bbu, 0x41943007u, 0x4333e75du, 0x3f8eaaf1u, 0x4407b6abu, 0x40d8dcabu,
	      0x42471a22u, 0x4409e3c7u, 0x420c78e3u, 0x429aedffu, 0x3f8a03feu, 0x42cc891cu},
	     true,
	     {0x44079077u, 0x41c996dcu, 0x425c1bceu}},
	    {{0x45969e51u, 0x4242af50u, 0x459af96fu, 0x459a5984u, 0x41b126bbu, 0x459a1b7fu, 0xbf1edc3au, 0x459839c5u, 0x426a80f2u,
	      0x459ba42cu, 0x459b7cd0u, 0x421c772fu, 0x459a9c24u, 0xbed99622u, 0x42caf2a4u},
	     true,
	     {0x45995b3eu, 0x4237c766u, 0x459bad99u}},
	    {{0x44d38a36u, 0x42610729u, 0x455ab541u, 0x44cddc81u, 0x41097b82u, 0x455ef195u, 0x3ff51c9bu, 0x44d51b79u, 0x4269b2e9u,
	      0x4557a3c8u, 0x44d41719u, 0x4197b270u, 0x4558ac62u, 0x3ffdc7c9u, 0x43781ea6u},
	     false,
	     {0u, 0u, 0u}},
	    {{0x43019f06u, 0x410fd36du, 0x44a051f9u, 0x4228ef10u, 0x41bdc418u, 0x449ff6e5u, 0x4041649au, 0x42af136fu, 0x419132c6u,
	      0x449d017du, 0x41a6ad6cu, 0x416659bfu, 0x448d6a59u, 0x40849c64u, 0x42b1724du},
	     false,
	     {0u, 0u, 0u}},
	    {{0x44e46a81u, 0x424868d6u, 0x44a9066du, 0x44e72aa4u, 0x42091212u, 0x44aaaefau, 0x3deac658u, 0x44de0442u, 0x426a121fu,
	      0x44a7d2e5u, 0x44e32633u, 0x401db5d2u, 0x44a79a9bu, 0xbda01398u, 0x4249dc1au},
	     false,
	     {0u, 0u, 0u}},
	}};
	const auto f = [](uint32_t v) { return std::bit_cast<float>(v); };
	for (const auto& c : k_Cases)
	{
		const auto& in = c.in;
		const BoltPose newer {
		    .origin = {f(in[0]), f(in[1]), f(in[2])}, .centroid = {f(in[3]), f(in[4]), f(in[5])}, .heading = f(in[6])};
		const BoltPose older {
		    .origin = {f(in[7]), f(in[8]), f(in[9])}, .centroid = {f(in[10]), f(in[11]), f(in[12])}, .heading = f(in[13])};
		const auto meeting = ClashPoint(newer, older, f(in[14]));
		ASSERT_EQ(meeting.has_value(), c.clash) << std::hex << in[0] << " " << in[7];
		if (meeting.has_value())
		{
			EXPECT_EQ((std::array {std::bit_cast<uint32_t>(meeting->x), std::bit_cast<uint32_t>(meeting->y),
			                       std::bit_cast<uint32_t>(meeting->z)}),
			          c.meeting);
		}
	}
}
