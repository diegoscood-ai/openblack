/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Pulling a tied leash (Creature/LeashPull.h): the length a pull sets, the rope's pick on the screen stretch by stretch
// and what it takes the press from. The lengths and the stretch test are pinned to the original's own results, run in
// the exe oracle (its pull's release and its stretch test, tools/oracle); the other cases follow its rules

#include <cstdint>

#include <array>
#include <bit>
#include <limits>
#include <optional>
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Creature/LeashPull.h"

using namespace openblack;
using namespace openblack::creature_leash;

namespace
{
/// Where a tied thing stands, as the original's map place holds it: x and z in fixed point, its altitude over the land,
/// and the land's height there
glm::vec3 TiedAt(int32_t x, int32_t z, float altitude, float land)
{
	return TiedPointOf({x, z, altitude}, land);
}

float F(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

glm::vec3 V(uint32_t x, uint32_t y, uint32_t z)
{
	return {F(x), F(y), F(z)};
}

affine::AffineMatrix M(const std::array<uint32_t, 12>& bits)
{
	affine::AffineMatrix m;
	for (size_t i = 0; i < bits.size(); ++i)
	{
		m.m.at(i) = F(bits.at(i));
	}
	return m;
}
} // namespace

// The hand's distance from the tied thing, or twice its radius when that is at least as long, bit for bit as the
// original's release of the pull gives them
TEST(LeashPull, TheLengthIsTheHandsDistanceButNoLessThanTwiceTheRadius)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(PullLength({10.0f, 3.0f, 20.0f}, TiedAt(81920, 163840, 1.5f, 4.0f), 0.5f)), 0x40C3F58Du);
	// nearer than twice the radius: twice the radius
	EXPECT_EQ(std::bit_cast<uint32_t>(PullLength({12.6f, 5.6f, 25.1f}, TiedAt(81920, 163840, 1.5f, 4.0f), 2.0f)), 0x40800000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(PullLength({1245.803389779404f, 222.53609677821882f, 1590.3871311313933f},
	                                             TiedAt(12413438, 10939276, 18.44649993330834f, 8.70156848508442f),
	                                             28.300701509949413f)),
	          0x442A6D77u);
	EXPECT_EQ(std::bit_cast<uint32_t>(PullLength({226.41192930628873f, 140.72071433464913f, 493.1456652396606f},
	                                             TiedAt(9122793, 1710846, 11.478823758562015f, 3.934256876670661f),
	                                             8.38447098033331f)),
	          0x449562B9u);
	EXPECT_EQ(std::bit_cast<uint32_t>(PullLength({319.2084247160765f, 239.14409742936135f, 277.53483679780635f},
	                                             TiedAt(10359134, 7463104, 2.5339846510053943f, 0.5324586607603932f),
	                                             125.67382949707073f)),
	          0x44C1322Fu);
	EXPECT_EQ(std::bit_cast<uint32_t>(PullLength({808.5881623337793f, 164.71978378535425f, 837.6544768539737f},
	                                             TiedAt(6314363, 9705935, 16.82468190924501f, 135.8466760368506f),
	                                             374.22781864634106f)),
	          0x443B1D29u);
	EXPECT_EQ(std::bit_cast<uint32_t>(PullLength({811.7760424588097f, 71.2009811562862f, 1187.920441386594f},
	                                             TiedAt(4197808, 7644791, 13.019328776263785f, 43.68870568806056f),
	                                             382.9946236285025f)),
	          0x443F7F50u);
	EXPECT_EQ(PullSlackLength(24.5f), 12.25f);
}

// The tied thing's point: its place on the map across the ground, at the land's height plus its altitude
TEST(LeashPull, TheTiedThingsPointIsItsPlaceAtTheLandsHeightPlusItsAltitude)
{
	const auto point = TiedPointOf({81920, 163840, 1.5f}, 4.0f);
	EXPECT_EQ(point, glm::vec3(12.5f, 5.5f, 25.0f));
	EXPECT_EQ(TiedPointOf({0, 0, 0.0f}, 0.0f), glm::vec3(0.0f));
}

// A rope point projects as the game projects a point: the clip depth, the pixel truncated, shown in front of the near
// clip whether on the screen or off it, bit for bit as the original's
TEST(LeashPull, ARopePointProjectsAsTheGameProjectsAPoint)
{
	auto point = ProjectRopePoint(M({0x3FC82951, 0x3FF6033C, 0x3F20B2CB, 0x3F43CF98, 0x3EACEF05, 0xBFB8246A, 0xBFEE09ED,
	                                 0xBFF6D692, 0x3FD2075E, 0x3F4DCB16, 0x3FECF053, 0xBFF51D84}),
	                              1.0f, {640, 480}, V(0x4159E52E, 0xBFE361FC, 0x41B865FD));
	EXPECT_TRUE(point.projected);
	EXPECT_EQ(point.pixel, glm::ivec2(169, 326));
	EXPECT_EQ(std::bit_cast<uint32_t>(point.depth), 0x423C0AD3u);
	point = ProjectRopePoint(M({0xBFD59ED5, 0xBE721197, 0x3E4E0965, 0x3F8955E7, 0xBD4DA909, 0xBFF17432, 0x3F9E479E, 0xBFDF33F0,
	                            0x3F99873C, 0xBFA77A23, 0xBF28F4A0, 0x3F936867}),
	                         2.0f, {1024, 768}, V(0xC20FCCC8, 0xC20C87A0, 0x3FD384DA));
	EXPECT_TRUE(point.projected);
	EXPECT_EQ(point.pixel, glm::ivec2(701, 342));
	EXPECT_EQ(std::bit_cast<uint32_t>(point.depth), 0x4278B0CFu);
	// off the screen: still shown
	point = ProjectRopePoint(M({0x3FEA8488, 0x3FB1D296, 0xBFFFB893, 0xBF949FEB, 0x3FD20F2A, 0xBDF5DD3E, 0x3FF5F19B, 0xBED2132A,
	                            0xBFDA9AB8, 0x3F048FD4, 0x3F8E98FA, 0xBF6BBFF3}),
	                         0.5f, {640, 480}, V(0xC225246F, 0xC185EE77, 0x4239A168));
	EXPECT_TRUE(point.projected);
	EXPECT_EQ(point.pixel, glm::ivec2(2804, 5957));
	EXPECT_EQ(std::bit_cast<uint32_t>(point.depth), 0x408A040Eu);
	// behind the near clip: not shown
	point = ProjectRopePoint(M({0xBE430DA8, 0x3E74D3E4, 0x3FD93221, 0xBE0CB283, 0x3D0078AF, 0x3EB2F6D1, 0xBFA17433, 0x3D431C74,
	                            0x3F04FFFA, 0x3F960111, 0xBFCFCF0D, 0xBF49512E}),
	                         0.5f, {1024, 768}, V(0xC223BB56, 0x41F7B733, 0x419AC034));
	EXPECT_FALSE(point.projected);
	EXPECT_EQ(std::bit_cast<uint32_t>(point.depth), 0xC2457B03u);
}

// The whole pick of a stretch, from the rope's points to the depth of the point the mouse is on, bit for bit as the
// original's pick of that rope
TEST(LeashPull, ThePickOfAStretchGivesTheOriginalsDepth)
{
	const auto toClip = M({0x3FDD4C83, 0x3F1B3E04, 0x3E9E0C34, 0xA47124A9, 0x3FF135F4, 0xBF21234B, 0xBF3F977F, 0x3FB3502D,
	                       0x3F368DB9, 0xC09FDA46, 0xC0E16D26, 0x4261BA60});
	const auto toCamera = M({0x3F6AEEDD, 0x3E8006C1, 0x3E9E0C34, 0xA4000000, 0x3F46EC6F, 0xBF21234B, 0xBECB654E, 0x3F13E09D,
	                         0x3F368DB9, 0xC029B37D, 0xC039E808, 0x4261BA60});
	const auto from = V(0x4005DC5F, 0x3FAA6D9A, 0xBFAE7A27);
	const auto to = V(0x40196BA8, 0x3F36552F, 0xBFB13400);
	const std::array points {ProjectRopePoint(toClip, 1.0f, {1024, 768}, from),
	                         ProjectRopePoint(toClip, 1.0f, {1024, 768}, to)};
	EXPECT_EQ(points.at(0).pixel, glm::ivec2(508, 419));
	EXPECT_EQ(points.at(1).pixel, glm::ivec2(513, 426));
	const auto hit = PickRope({513, 419}, points, 1024, F(0x3E19999A));
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->segment, 0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(hit->along), 0x3F14CC07u);
	EXPECT_EQ(std::bit_cast<uint32_t>(RopeHitDepth(toCamera, from, to, hit->along)), 0x425E290Eu);
}

// The stretch test: within either end's round, along the stretch as the cosine the original measures, or nothing, bit
// for bit as the original's
TEST(LeashPull, TheMouseIsOnAStretchWithinItsWidthAsTheOriginalMeasuresIt)
{
	const glm::ivec2 a {90, 95};
	const glm::ivec2 b {140, 110};
	EXPECT_EQ(std::bit_cast<uint32_t>(RopeSegmentHit({100, 100}, a, 3, b, 5).value()), 0x3F7C36D5u);
	EXPECT_EQ(RopeSegmentHit({91, 96}, a, 3, b, 5), std::optional(0.0f));
	EXPECT_EQ(RopeSegmentHit({139, 111}, a, 3, b, 5), std::optional(1.0f));
	EXPECT_FALSE(RopeSegmentHit({120, 130}, a, 3, b, 5).has_value());
	EXPECT_EQ(std::bit_cast<uint32_t>(RopeSegmentHit({118, 104}, a, 2, b, 8).value()), 0x3F7FF37Cu);
	EXPECT_EQ(RopeSegmentHit({200, 200}, {200, 200}, 2, {200, 200}, 2), std::optional(0.0f));
	EXPECT_EQ(std::bit_cast<uint32_t>(RopeSegmentHit({50, 52}, {10, 10}, 2, {90, 90}, 6).value()), 0x3F7FEC83u);
}

// The pick goes stretch by stretch from the first point and takes the first the mouse is on; a stretch with an end off
// the screen or no further than 1 in front of the camera is skipped. Each point is as wide as the rope is drawn there,
// at least 2 pixels
TEST(LeashPull, ThePickTakesTheFirstStretchShownInFrontOfTheCamera)
{
	std::array<RopeScreenPoint, 4> points {{
	    {.pixel = {100, 100}, .depth = 50.0f, .projected = true},
	    {.pixel = {200, 100}, .depth = 50.0f, .projected = true},
	    {.pixel = {300, 100}, .depth = 50.0f, .projected = true},
	    {.pixel = {400, 100}, .depth = 50.0f, .projected = true},
	}};
	// 640 wide, a half width of 0.15: 192 / 50, 3 pixels either side
	auto hit = PickRope({250, 102}, points, 640, 0.15f);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->segment, 1u);
	EXPECT_FALSE(PickRope({250, 104}, points, 640, 0.15f).has_value());
	// the second point off the screen: the first two stretches are skipped
	points.at(1).projected = false;
	EXPECT_FALSE(PickRope({150, 100}, points, 640, 0.15f).has_value());
	hit = PickRope({350, 100}, points, 640, 0.15f);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->segment, 2u);
	// a point no further than 1 in front of the camera is never picked
	points.at(1).projected = true;
	points.at(1).depth = 1.0f;
	EXPECT_FALSE(PickRope({150, 100}, points, 640, 0.15f).has_value());
	// far off, the width is the least: 2 pixels
	points.at(1).depth = 50.0f;
	for (auto& point : points)
	{
		point.depth = 10000.0f;
	}
	EXPECT_TRUE(PickRope({150, 101}, points, 640, 0.15f).has_value());
	EXPECT_FALSE(PickRope({150, 103}, points, 640, 0.15f).has_value());
	EXPECT_FALSE(PickRope({150, 100}, std::span(points).first(1), 640, 0.15f).has_value());
}

// The rope takes the press from an object, a screen object or the land only when it is strictly nearer the camera, and
// never from a creature
TEST(LeashPull, TheRopeTakesThePressWhenNearerButNeverFromACreature)
{
	EXPECT_TRUE(RopeTakesThePick(RopeRival::Object, 20.0f, 10.0f));
	EXPECT_FALSE(RopeTakesThePick(RopeRival::Object, 10.0f, 10.0f));
	EXPECT_FALSE(RopeTakesThePick(RopeRival::Object, 5.0f, 10.0f));
	EXPECT_FALSE(RopeTakesThePick(RopeRival::Creature, 20.0f, 10.0f));
	EXPECT_TRUE(RopeTakesThePick(RopeRival::ScreenObject, 20.0f, 10.0f));
	EXPECT_TRUE(RopeTakesThePick(RopeRival::Land, std::numeric_limits<float>::max(), 10.0f));
	EXPECT_FALSE(RopeTakesThePick(RopeRival::Land, std::numeric_limits<float>::quiet_NaN(), 10.0f));
}
