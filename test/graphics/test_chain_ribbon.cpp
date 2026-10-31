/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// graphics::chain_ribbon (src/Graphics/ChainRibbon.h): the chain ribbons' corners, pinned bit for bit on synthetic
// joints. The expected bits are those of the corner maths as the renderer had it before it became this function,
// computed in single precision in the same order: no fused multiply-adds, the square root and the divide correctly
// rounded

#include <cstddef>
#include <cstdint>

#include <array>
#include <bit>
#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Graphics/ChainRibbon.h"

namespace chain_ribbon = openblack::graphics::chain_ribbon;
using chain_ribbon::Joint;

namespace
{

/// The bits of a corner's x, y and z
using Bits = std::array<uint32_t, 3>;

void ExpectCorners(std::span<const Joint> joints, const glm::vec3& eye, std::span<const Bits> expected)
{
	const auto corners = chain_ribbon::Corners(joints, eye);
	ASSERT_EQ(corners.size(), expected.size());
	for (size_t i = 0; i < corners.size(); ++i)
	{
		for (int axis = 0; axis < 3; ++axis)
		{
			EXPECT_EQ(std::bit_cast<uint32_t>(corners[i][axis]), expected[i][static_cast<size_t>(axis)])
			    << "corner " << i << ", axis " << axis;
		}
	}
}

constexpr std::array k_StraightJoints {Joint {{0.1f, 0.2f, 0.3f}, 0.5f}, Joint {{1.1f, 1.2f, 1.3f}, 0.6f},
                                       Joint {{2.1f, 2.2f, 2.3f}, 0.7f}};
constexpr glm::vec3 k_StraightEye {10.3f, -4.7f, 8.9f};
constexpr std::array<Bits, 8> k_StraightCorners {{
    {0xBE6DCA9Au, 0x3E247B25u, 0x3F2BED78u},
    {0x3EDD4BB3u, 0x3E751E75u, 0xBD929EF0u},
    {0x3F338AE6u, 0x3F938D5Au, 0x3FDF7A00u},
    {0x3FBFD427u, 0x3F9FA5DAu, 0x3F5AA598u},
    {0x3F338AE6u, 0x3F938D5Au, 0x3FDF7A00u},
    {0x3FBFD427u, 0x3F9FA5DAu, 0x3F5AA598u},
    {0x3FD14438u, 0x400945A7u, 0x40347EA2u},
    {0x40242AB0u, 0x401053F3u, 0x3FE3CF88u},
}};

constexpr std::array k_OneSegmentJoints {Joint {{1.25f, -0.75f, 3.5f}, 1.5f}, Joint {{-2.125f, 0.625f, 7.0625f}, 0.875f}};
constexpr glm::vec3 k_OneSegmentEye {6.4f, 9.1f, -3.3f};
constexpr std::array<Bits, 4> k_OneSegmentCorners {{
    {0x4016E2DBu, 0xBF22A174u, 0x40902924u},
    {0x3E11D250u, 0xBF5D5E8Cu, 0x401FADB8u},
    {0xBFBD4CACu, 0x3F3121D2u, 0x40F4C2AAu},
    {0xC03159AAu, 0x3F0EDE2Eu, 0x40CF3D56u},
}};

constexpr std::array k_BentJoints {Joint {{0.0f, 0.0f, 0.0f}, 1.0f}, Joint {{3.0f, 1.0f, 0.5f}, 1.25f},
                                   Joint {{4.0f, 4.0f, -1.0f}, 0.8f}, Joint {{7.5f, 4.25f, -2.0f}, 2.0f}};
constexpr glm::vec3 k_BentEye {-20.5f, 15.25f, 30.75f};
constexpr std::array<Bits, 12> k_BentCorners {{
    {0xBE3EA200u, 0x3F533DE4u, 0xBF0888C8u},
    {0x3E3EA200u, 0xBF533DE4u, 0x3F0888C8u},
    {0x4018A9D8u, 0x3FBF2B3Du, 0xBE570068u},
    {0x40675628u, 0x3F01A986u, 0x3F9AE00Du},
    {0x4018A9D8u, 0x3FBF2B3Du, 0xBE570068u},
    {0x40675628u, 0x3F01A986u, 0x3F9AE00Du},
    {0x4066AA09u, 0x408AAA42u, 0xBFB6377Eu},
    {0x408CAAFCu, 0x406AAB7Cu, 0xBF139104u},
    {0x4066AA09u, 0x408AAA42u, 0xBFB6377Eu},
    {0x408CAAFCu, 0x406AAB7Cu, 0xBF139104u},
    {0x40E3AF36u, 0x40BF99C4u, 0xC03A68A4u},
    {0x40FC50CAu, 0x4020CC79u, 0xBF8B2EB8u},
}};

constexpr std::array k_ZeroLengthJoints {Joint {{5.5f, 1.5f, -2.25f}, 0.5f}, Joint {{5.5f, 1.5f, -2.25f}, 0.5f},
                                         Joint {{8.0f, 2.5f, -1.0f}, 0.75f}};
constexpr glm::vec3 k_ZeroLengthEye {0.3f, 12.7f, -6.1f};
constexpr std::array<Bits, 8> k_ZeroLengthCorners {{
    {0x40B00000u, 0x3FC00000u, 0xC0100000u},
    {0x40B00000u, 0x3FC00000u, 0xC0100000u},
    {0x40B3C682u, 0x3FBD5B2Cu, 0xC01E0B4Cu},
    {0x40AC397Eu, 0x3FC2A4D4u, 0xC001F4B4u},
    {0x40B3C682u, 0x3FBD5B2Cu, 0xC01E0B4Cu},
    {0x40AC397Eu, 0x3FC2A4D4u, 0xC001F4B4u},
    {0x4105A9C2u, 0x401C08C3u, 0xBFD443C6u},
    {0x40F4AC7Bu, 0x4023F73Du, 0xBEAEF0E6u},
}};

constexpr std::array k_EyeOnLineJoints {Joint {{2.0f, 3.0f, 0.0f}, 1.5f}, Joint {{2.0f, 3.0f, 5.0f}, 1.5f},
                                        Joint {{4.0f, 3.0f, 9.0f}, 1.0f}};
constexpr glm::vec3 k_EyeOnLineEye {2.0f, 3.0f, -15.0f};
constexpr std::array<Bits, 8> k_EyeOnLineCorners {{
    {0x40000000u, 0x40400000u, 0x00000000u},
    {0x40000000u, 0x40400000u, 0x00000000u},
    {0x40000000u, 0x40100000u, 0x40A00000u},
    {0x40000000u, 0x40700000u, 0x40A00000u},
    {0x40000000u, 0x40100000u, 0x40A00000u},
    {0x40000000u, 0x40700000u, 0x40A00000u},
    {0x40800000u, 0x40000000u, 0x41100000u},
    {0x40800000u, 0x40800000u, 0x41100000u},
}};

constexpr std::array k_LongJoints {Joint {{0.000f, 20.610f, -5.000f}, 0.400f},  Joint {{2.057f, 18.792f, -4.710f}, 0.450f},
                                   Joint {{3.591f, 17.901f, -4.113f}, 0.500f},  Joint {{4.846f, 17.910f, -3.835f}, 0.550f},
                                   Joint {{6.473f, 16.083f, -4.397f}, 0.600f},  Joint {{8.580f, 15.210f, -5.789f}, 0.400f},
                                   Joint {{10.569f, 15.210f, -7.353f}, 0.450f}, Joint {{12.018f, 13.375f, -8.095f}, 0.500f},
                                   Joint {{13.294f, 12.519f, -7.273f}, 0.550f}, Joint {{15.018f, 12.509f, -4.932f}, 0.600f},
                                   Joint {{17.155f, 10.666f, -2.044f}, 0.400f}, Joint {{19.065f, 9.828f, -0.109f}, 0.450f}};
constexpr glm::vec3 k_LongEye {100.125f, 50.5f, -75.25f};
constexpr std::array<Bits, 44> k_LongCorners {{
    {0xBE17812Fu, 0x41A32791u, 0xC0A9AF3Du}, {0x3E17812Fu, 0x41A69AFFu, 0xC09650C3u}, {0x3FF6E248u, 0x4193FFB0u, 0xC0A0A066u},
    {0x400BDAA2u, 0x4198AC58u, 0xC08CD03Eu}, {0x3FF6E248u, 0x4193FFB0u, 0xC0A0A066u}, {0x400BDAA2u, 0x4198AC58u, 0xC08CD03Eu},
    {0x4063E266u, 0x418BCDFCu, 0xC08B2BA6u}, {0x4067C37Eu, 0x41929C82u, 0xC0781F7Cu}, {0x4063E266u, 0x418BCDFCu, 0xC08B2BA6u},
    {0x4067C37Eu, 0x41929C82u, 0xC0781F7Cu}, {0x409710DAu, 0x418CB648u, 0xC084C09Fu}, {0x409F1404u, 0x4191D914u, 0xC061600Au},
    {0x409710DAu, 0x418CB648u, 0xC084C09Fu}, {0x409F1404u, 0x4191D914u, 0xC061600Au}, {0x40C478CCu, 0x417FC57Eu, 0xC09C4FD5u},
    {0x40D9CCD6u, 0x4181713Au, 0xC07A313Au}, {0x40C478CCu, 0x417FC57Eu, 0xC09C4FD5u}, {0x40D9CCD6u, 0x4181713Au, 0xC07A313Au},
    {0x41057590u, 0x417338A8u, 0xC0C375C1u}, {0x410D19CCu, 0x41737FAAu, 0xC0AF0939u}, {0x41057590u, 0x417338A8u, 0xC0C375C1u},
    {0x410D19CCu, 0x41737FAAu, 0xC0AF0939u}, {0x4124D557u, 0x417320CAu, 0xC0F6CD94u}, {0x412D5FE9u, 0x41739788u, 0xC0DFC9FAu},
    {0x4124D557u, 0x417320CAu, 0xC0F6CD94u}, {0x412D5FE9u, 0x41739788u, 0xC0DFC9FAu}, {0x413D7A36u, 0x4152630Du, 0xC1073474u},
    {0x4143193Eu, 0x41599CF3u, 0xC0F7AB94u}, {0x413D7A36u, 0x4152630Du, 0xC1073474u}, {0x4143193Eu, 0x41599CF3u, 0xC0F7AB94u},
    {0x41554312u, 0x4140B32Cu, 0xC0EFCECEu}, {0x41542560u, 0x414FE87Au, 0xC0E1AA06u}, {0x41554312u, 0x4140B32Cu, 0xC0EFCECEu},
    {0x41542560u, 0x414FE87Au, 0xC0E1AA06u}, {0x417154CCu, 0x413F7ED4u, 0xC0A4A4D9u}, {0x416F3EA8u, 0x4150CAE6u, 0xC097010Bu},
    {0x417154CCu, 0x413F7ED4u, 0xC0A4A4D9u}, {0x416F3EA8u, 0x4150CAE6u, 0xC097010Bu}, {0x41893F1Fu, 0x41250A1Cu, 0xC00EFB28u},
    {0x41893BC3u, 0x413045C4u, 0xBFED4D43u}, {0x41893F1Fu, 0x41250A1Cu, 0xC00EFB28u}, {0x41893BC3u, 0x413045C4u, 0xBFED4D43u},
    {0x4198B082u, 0x4116C6BEu, 0xBE9C3396u}, {0x419859BCu, 0x4123B83Cu, 0x3DB2578Eu},
}};

} // namespace

TEST(ChainRibbon, FewerThanTwoJointsHaveNoCorners)
{
	EXPECT_TRUE(chain_ribbon::Corners({}, glm::vec3(0.0f)).empty());
	const std::array joints {Joint {{1.0f, 2.0f, 3.0f}, 1.0f}};
	EXPECT_TRUE(chain_ribbon::Corners(joints, glm::vec3(0.0f)).empty());
}

TEST(ChainRibbon, OneSegmentIsAsWideAsItsJointsScales)
{
	ExpectCorners(k_OneSegmentJoints, k_OneSegmentEye, k_OneSegmentCorners);
	// each end's two corners lie the joint's scale away on either side, so they are twice the scale apart
	const auto corners = chain_ribbon::Corners(k_OneSegmentJoints, k_OneSegmentEye);
	EXPECT_NEAR(glm::distance(corners[0], corners[1]), 2.0f * 1.5f, 1e-5f);
	EXPECT_NEAR(glm::distance(corners[2], corners[3]), 2.0f * 0.875f, 1e-5f);
}

TEST(ChainRibbon, StraightChain)
{
	ExpectCorners(k_StraightJoints, k_StraightEye, k_StraightCorners);
}

TEST(ChainRibbon, BentChainMeetsHalfWayAtItsJoints)
{
	ExpectCorners(k_BentJoints, k_BentEye, k_BentCorners);
	// where two segments meet, the tail corners of one are the head corners of the next
	const auto corners = chain_ribbon::Corners(k_BentJoints, k_BentEye);
	for (size_t segment = 1; segment < k_BentJoints.size() - 1; ++segment)
	{
		const auto head = segment * chain_ribbon::k_CornersPerSegment;
		EXPECT_EQ(corners[head], corners[head - 2]);
		EXPECT_EQ(corners[head + 1], corners[head - 1]);
	}
}

TEST(ChainRibbon, ZeroLengthSegmentCollapsesToItsJoint)
{
	// the first segment's two joints are at one point: its sides are zero, so its head corners are the joint, and its
	// tail corners only move half way to the next segment's
	ExpectCorners(k_ZeroLengthJoints, k_ZeroLengthEye, k_ZeroLengthCorners);
}

TEST(ChainRibbon, EyeOnTheSegmentsLineGivesNoSide)
{
	// the eye is on the first segment's line, so that segment has no side; at the joint it shares with the next
	// segment the corners are half way to the next segment's
	ExpectCorners(k_EyeOnLineJoints, k_EyeOnLineEye, k_EyeOnLineCorners);
}

TEST(ChainRibbon, LongChain)
{
	ExpectCorners(k_LongJoints, k_LongEye, k_LongCorners);
}
