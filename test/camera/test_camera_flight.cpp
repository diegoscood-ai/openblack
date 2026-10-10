/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A flight's middle point (CameraModel::CharterFlight): halfway between the two origins, raised by their distance
// across the land times the rise, and at least 10 above the land under it. The view a flight goes to
// (camera_flight): the distance shaped, the best heading over the land, the pitch and the arena's look point. The point a
// view is built from, at a distance, heading and pitch, to the bit

#include <cmath>
#include <cstdint>

#include <bit>
#include <limits>
#include <tuple>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Camera/CameraFlight.h"
#include "Camera/CameraModel.h"
#include "Camera/ScriptCamera.h"
#include "support/LandFakes.h"

using namespace openblack;
using openblack::test::HeightFieldIsland;

namespace
{
[[nodiscard]] float Level(glm::vec2 /*xz*/)
{
	return 0.0f;
}

[[nodiscard]] glm::vec3 Via(const LandIslandInterface& land, glm::vec3 origin, glm::vec3 currentOrigin, float rise)
{
	const auto flight = CameraModel::CharterFlight(land, origin, {0.0f, 0.0f, 0.0f}, currentOrigin, rise);
	EXPECT_TRUE(flight.midpoint.has_value());
	return flight.midpoint.value_or(glm::vec3(0.0f));
}
} // namespace

TEST(CameraFlight, TheFlightEndsAtTheNewPlace)
{
	const glm::vec3 origin {100.0f, 50.0f, 200.0f};
	const glm::vec3 focus {150.0f, 0.0f, 260.0f};
	const HeightFieldIsland flat(Level);
	const auto flight = CameraModel::CharterFlight(flat, origin, focus, {0.0f, 20.0f, 0.0f}, 0.3f);
	EXPECT_EQ(flight.origin, origin);
	EXPECT_EQ(flight.focus, focus);
	ASSERT_TRUE(flight.midpoint.has_value());
}

TEST(CameraFlight, OverFlatLandTheViaIsTheOriginalsDoubleClickVia)
{
	// The original's double click flight (rise 0.1) in the DoubleClickFlyTo recording: from the camera's origin at the
	// click to the new origin, its first leg goes to (1040.35498, 37.3544312, 1086.26038)
	const HeightFieldIsland flat(Level);
	const auto via = Via(flat, {1080.70996f, 50.6682434f, 1052.52075f}, {1000.0f, 3.00013423f, 1120.0f}, 0.1f);
	EXPECT_FLOAT_EQ(via.x, 1040.35498f);
	EXPECT_NEAR(via.y, 37.3544312f, 1e-3f);
	EXPECT_FLOAT_EQ(via.z, 1086.26038f);
}

TEST(CameraFlight, OverAHillTheViaIsTenAboveTheLand)
{
	// A hill only around the halfway point: the land under the via is read, not under either end
	const HeightFieldIsland hill(
	    [](glm::vec2 xz) { return glm::distance(xz, glm::vec2(50.0f, 0.0f)) < 10.0f ? 500.0f : 0.0f; });
	EXPECT_FLOAT_EQ(Via(hill, {100.0f, 100.0f, 0.0f}, {0.0f, 100.0f, 0.0f}, 0.1f).y, 510.0f);
	// Land 5 below the raised point: the floor 10 above it wins
	const HeightFieldIsland justBelow([](glm::vec2) { return 105.0f; });
	EXPECT_FLOAT_EQ(Via(justBelow, {100.0f, 100.0f, 0.0f}, {0.0f, 100.0f, 0.0f}, 0.1f).y, 115.0f);
	// Land 11 below it: the raised point stays (100 + 100 x 0.1)
	const HeightFieldIsland wellBelow([](glm::vec2) { return 99.0f; });
	EXPECT_FLOAT_EQ(Via(wellBelow, {100.0f, 100.0f, 0.0f}, {0.0f, 100.0f, 0.0f}, 0.1f).y, 110.0f);
}

TEST(CameraFlight, WithNoRiseTheViaIsTheMiddle)
{
	const HeightFieldIsland flat(Level);
	const auto via = Via(flat, {300.0f, 60.0f, 400.0f}, {0.0f, 40.0f, 0.0f}, 0.0f);
	EXPECT_FLOAT_EQ(via.x, 150.0f);
	EXPECT_FLOAT_EQ(via.y, 50.0f);
	EXPECT_FLOAT_EQ(via.z, 200.0f);
	// Still at least 10 above the land
	const HeightFieldIsland raised([](glm::vec2) { return 45.0f; });
	EXPECT_FLOAT_EQ(Via(raised, {300.0f, 60.0f, 400.0f}, {0.0f, 40.0f, 0.0f}, 0.0f).y, 55.0f);
}

TEST(CameraFlight, TheRiseIsTheDistanceAcrossTheLandTimesTheRise)
{
	const HeightFieldIsland deep([](glm::vec2) { return -100.0f; });
	// 50 across the land (30, 40), times 0.3
	EXPECT_FLOAT_EQ(Via(deep, {30.0f, 0.0f, 40.0f}, {0.0f, 0.0f, 0.0f}, 0.3f).y, 15.0f);
	// The height between the two origins is not counted: halfway up 1000, plus the same 15
	EXPECT_FLOAT_EQ(Via(deep, {30.0f, 1000.0f, 40.0f}, {0.0f, 0.0f, 0.0f}, 0.3f).y, 515.0f);
}

TEST(CameraFlight, TheLandIsReadAsTheHandReadsIt)
{
	// The via's x and z through the map's fixed point, truncated, before the height is read
	const HeightFieldIsland land(Level);
	const auto via = Via(land, {1000.37f, 0.0f, 2000.11f}, {1000.0f, 0.0f, 2000.0f}, 0.0f);
	ASSERT_EQ(land.asked.size(), 1U);
	const auto lookup = [](float m) { return map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(m)); };
	EXPECT_EQ(land.asked[0].x, lookup(via.x));
	EXPECT_EQ(land.asked[0].y, lookup(via.z));
	EXPECT_NE(land.asked[0].x, via.x);
}

// The view a flight goes to

namespace
{
/// pi / 8 and pi / 3, the pitch's bounds
constexpr float k_LowestPitch = 0.392699093f;
constexpr float k_HighestPitch = 1.04719758f;
/// The pitch from the arena's centre to its look point on level land, atan(-0.5)
constexpr float k_ArenaPitch = -0.463647604f;
} // namespace

TEST(CameraFlightView, TheDistanceIsShapedTowards50AndAbove100)
{
	namespace cf = camera_flight;
	// Below 50: most of the way up to it, (50 - 10) x 0.8 + 10
	EXPECT_FLOAT_EQ(cf::ShapeDistance(10.0f), 42.0f);
	EXPECT_FLOAT_EQ(cf::ShapeDistance(30.0f), 46.0f);
	// 50 to 100 as they are, both ends included
	EXPECT_EQ(cf::ShapeDistance(50.0f), 50.0f);
	EXPECT_EQ(cf::ShapeDistance(75.0f), 75.0f);
	EXPECT_EQ(cf::ShapeDistance(100.0f), 100.0f);
	// Above 100: a tenth of the way down to it, (100 - 200) x 0.1 + 200
	EXPECT_FLOAT_EQ(cf::ShapeDistance(200.0f), 190.0f);
	EXPECT_TRUE(std::isnan(cf::ShapeDistance(std::numeric_limits<float>::quiet_NaN())));
}

TEST(CameraFlightView, OnLevelLandTheHeadingStaysAndThePitchIsTheOriginals)
{
	// Every heading sees the same land: the cosine keeps the first. The pitch from atan(-0.5): -0.4636 x 0.2 + the level
	// land's 1.5393804 x 0.5 x 0.2 + 3 pi / 25
	const HeightFieldIsland flat(Level);
	float pitch = k_ArenaPitch;
	EXPECT_EQ(camera_flight::FindBestAngle(flat, 1.0f, 40.0f, {1000.0f, 5.0f, 1000.0f}, pitch), 1.0f);
	EXPECT_NEAR(pitch, 0.4382f, 1e-4f);
	// Five samples on each of the 32 headings, and one more each that is not used
	EXPECT_EQ(flat.asked.size(), 32U * 5U + 32U);
}

TEST(CameraFlightView, OverAHillTheBestHeadingIsTheOpenSide)
{
	// A wall 100 high ahead (+z) and a little to the left, from x = -0.5 z to x = 0.25 z: the headings 0, 1, 30 and 31
	// sixteenths of pi see it; of the open ones the second sixteenth, on the right, has the highest cosine
	const glm::vec2 point {1000.0f, 1000.0f};
	const HeightFieldIsland wall([point](glm::vec2 xz) {
		const auto d = xz - point;
		return d.y > 1.0f && d.x > -0.5f * d.y && d.x < 0.25f * d.y ? 100.0f : 0.0f;
	});
	float pitch = 0.5f;
	EXPECT_FLOAT_EQ(camera_flight::FindBestAngle(wall, 0.0f, 40.0f, {point.x, 0.0f, point.y}, pitch),
	                2.0f * (glm::pi<float>() / 16.0f));
}

TEST(CameraFlightView, ThePitchStaysWithinAnEighthAndAThirdOfPi)
{
	const HeightFieldIsland flat(Level);
	const glm::vec3 point {1000.0f, 0.0f, 1000.0f};
	// Not a number goes to the lower bound, as the original's first comparison sends it
	float pitch = std::numeric_limits<float>::quiet_NaN();
	std::ignore = camera_flight::FindBestAngle(flat, 0.0f, 40.0f, point, pitch);
	EXPECT_EQ(pitch, k_LowestPitch);
	pitch = -3.0f;
	std::ignore = camera_flight::FindBestAngle(flat, 0.0f, 40.0f, point, pitch);
	EXPECT_EQ(pitch, k_LowestPitch);
	pitch = 5.0f;
	std::ignore = camera_flight::FindBestAngle(flat, 0.0f, 40.0f, point, pitch);
	EXPECT_EQ(pitch, k_HighestPitch);
}

TEST(CameraFlightView, TheArenaIsLookedAtFromItsCentresSide)
{
	// The look point of an arena of radius 10: its rim on the +x side, 5 up
	const glm::vec3 centre {1000.0f, 0.0f, 1000.0f};
	const auto look = camera_flight::ArenaLookPoint(centre, 10.0f);
	EXPECT_EQ(look, glm::vec3(1010.0f, 5.0f, 1000.0f));
	// From the centre: heading 3 pi / 2, back towards it; sqrt(125) shaped to 42.236; the pitch 0.4382
	const HeightFieldIsland flat(Level);
	const auto view = camera_flight::ViewOfPoint(flat, centre, look);
	EXPECT_EQ(view.focus, look);
	EXPECT_NEAR(glm::distance(view.origin, look), 42.236f, 1e-3f);
	EXPECT_NEAR(view.origin.x, 971.7545f, 1e-3f);
	EXPECT_NEAR(view.origin.y, 22.9212f, 1e-3f);
	EXPECT_NEAR(view.origin.z, 1000.0f, 1e-3f);
}

// The point at a distance, heading and pitch, as the original builds it: the sines and cosines unrounded, only the
// pitch's cosine kept as a float, every product and sum a float

namespace
{
[[nodiscard]] float FromBits(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

void ExpectBits(const glm::vec3& actual, uint32_t x, uint32_t y, uint32_t z)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(actual.x), x) << actual.x;
	EXPECT_EQ(std::bit_cast<uint32_t>(actual.y), y) << actual.y;
	EXPECT_EQ(std::bit_cast<uint32_t>(actual.z), z) << actual.z;
}
} // namespace

TEST(CameraFlightView, TheDoubleClicksOriginIsTheOriginalsToTheBit)
{
	// The DoubleClickFlyTo recording: 100 from the hand's point (1080.70996, 0, 966.307434), heading 0, at the pitch
	// FindBestAngle gives, 0.53133231. The original's origin is (1080.70996, 50.6682434, 1052.52075); the sine rounded to
	// a float before the product gave a height one step lower, 50.6682396
	const auto origin =
	    script_camera::PointFromDistanceHeadingAndPitch({1080.70996f, 0.0f, 966.307434f}, 100.0f, 0.0f, FromBits(0x3F080565U));
	ExpectBits(origin, 0x448716B8U, 0x424AAC48U, 0x448390AAU);
	EXPECT_EQ(origin, glm::vec3(1080.70996f, 50.6682434f, 1052.52075f));
}

TEST(CameraFlightView, APointFromAnglesKeepsTheSinesUnrounded)
{
	// Worked out with the original's steps; with the sines and cosines rounded to floats first, each of these points
	// is a step or two away in every axis
	// 30 at heading 0.5, pitch 0.25: (13.9356403, 7.42211866, 25.5090199), not (13.9356413, 7.42211914, 25.5090179)
	ExpectBits(script_camera::PointFromDistanceHeadingAndPitch({0.0f, 0.0f, 0.0f}, 30.0f, 0.5f, 0.25f), 0x415EF862U,
	           0x40ED81FFU, 0x41CC1279U);
	// 75 at heading 2, pitch 0.4: (62.8138809, 29.206377, -28.7472458), not (62.8138733, 29.2063751, -28.7472439)
	ExpectBits(script_camera::PointFromDistanceHeadingAndPitch({0.0f, 0.0f, 0.0f}, 75.0f, 2.0f, 0.4f), 0x427B416AU, 0x41E9A6A9U,
	           0xC1E5FA5CU);
	// 1000 from (100, 10, 200) at heading 4, pitch 1.2: (-174.233215, 942.039124, -36.8528137), not (-174.233246,
	// 942.039062, -36.8528290)
	ExpectBits(script_camera::PointFromDistanceHeadingAndPitch({100.0f, 10.0f, 200.0f}, 1000.0f, 4.0f, 1.2f), 0xC32E3BB4U,
	           0x446B8281U, 0xC2136948U);
}
