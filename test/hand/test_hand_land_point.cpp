/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>
#include <limits>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/LandLine.h"
#include "ECS/HandLandPoint.h"

namespace land_line = openblack::land_line;
using namespace openblack::ecs::hand_land_point;

namespace
{
constexpr float k_NaN = std::numeric_limits<float>::quiet_NaN();

/// Land of the same height everywhere on the map
land_line::CellLookup Flat(uint16_t height)
{
	return [height](int32_t, int32_t) -> std::optional<land_line::CellHeights> {
		return land_line::CellHeights {.here = height, .acrossX = height, .acrossZ = height, .acrossBoth = height};
	};
}

/// No land anywhere: only the sea
std::optional<land_line::CellHeights> Sea(int32_t /*unused*/, int32_t /*unused*/)
{
	return std::nullopt;
}

/// The land's height is the same everywhere
std::function<float(glm::vec2)> HeightOf(float height)
{
	return [height](glm::vec2) { return height; };
}

/// The hand's inputs with nothing under the cursor, the hand `handPosition` away and a reach of 300
Inputs NothingUnder(glm::vec3 eye, glm::vec3 handPosition)
{
	return {
	    .eye = eye,
	    .nearPoint = {eye.x, eye.y - 1.0f, eye.z},
	    .hitXZ = std::nullopt,
	    .handPosition = handPosition,
	    .holdingWithSpring = false,
	    .subtractHand = true,
	    .handScale = 1.0f,
	    .handSize = 1.0f,
	    .reach = 300.0f,
	};
}

/// The same with the land found at x, z
Inputs LandAt(glm::vec3 eye, glm::vec2 hit)
{
	auto inputs = NothingUnder(eye, {-1000.0f, -1000.0f, -1000.0f});
	inputs.hitXZ = hit;
	return inputs;
}
} // namespace

TEST(HandLandPoint, TheLengthSumsZThenYThenX)
{
	// sqrt((z^2 + y^2) + x^2) and sqrt((x^2 + y^2) + z^2) round apart here (checked with float32 arithmetic)
	const glm::vec3 v(0.3f, 0.7f, 1.1f);
	EXPECT_EQ(std::bit_cast<uint32_t>(LengthZyx(v)), 0x3fab4099u);
	const float xFirst = std::sqrt((v.x * v.x + v.y * v.y) + v.z * v.z);
	EXPECT_EQ(std::bit_cast<uint32_t>(xFirst), 0x3fab4098u);
	EXPECT_EQ(LengthZyx({0.0f, -4.0f, 3.0f}), 5.0f);
}

TEST(HandLandPoint, TheDistanceSumsXThenZThenY)
{
	// sqrt((x^2 + z^2) + y^2) and sqrt((x^2 + y^2) + z^2) round apart here (checked with float32 arithmetic)
	const glm::vec3 from(40.143f, -46.941f, -47.455f);
	const glm::vec3 to(4.141f, 43.915f, -11.88f);
	EXPECT_EQ(std::bit_cast<uint32_t>(DistanceXzy(from, to)), 0x42d00153u);
	EXPECT_EQ(std::bit_cast<uint32_t>(DistanceXzy(to, from)), 0x42d00153u);
	EXPECT_EQ(DistanceXzy({1.0f, 2.0f, 3.0f}, {4.0f, 2.0f, -1.0f}), 5.0f);
}

TEST(HandLandPoint, TheViewDistanceIsKeptBetweenTwoAndTheReach)
{
	EXPECT_EQ(ClampedViewDistance(50.0f, 300.0f), 50.0f);
	EXPECT_EQ(ClampedViewDistance(1.0f, 300.0f), 2.0f);
	EXPECT_EQ(ClampedViewDistance(2.0f, 300.0f), 2.0f);
	EXPECT_EQ(ClampedViewDistance(400.0f, 300.0f), 300.0f);
	EXPECT_EQ(ClampedViewDistance(k_NaN, 300.0f), 2.0f);
	// a reach that is not a number gives the reach
	EXPECT_TRUE(std::isnan(ClampedViewDistance(50.0f, k_NaN)));
}

TEST(HandLandPoint, TheShiftMovesBothEndsByTheDepthAlongTheUnitDirection)
{
	// Along an axis, every step is exact
	const auto axis = ShiftedAlongView({10.0f, 50.0f, 20.0f}, {10.0f, 50.0f, 24.0f}, 11.0f, 1.0f);
	EXPECT_EQ(axis.from, glm::vec3(10.0f, 50.0f, 31.0f));
	EXPECT_EQ(axis.to, glm::vec3(10.0f, 50.0f, 35.0f));

	// A 3-4-5 direction: 1 / length, each axis times it, then times the depth, then added to each end
	const glm::vec3 eye(100.0f, 50.0f, 200.0f);
	const glm::vec3 nearPoint(103.0f, 46.0f, 200.0f);
	const auto line = ShiftedAlongView(eye, nearPoint, 11.0f, 1.0f);
	const float inverse = 1.0f / 5.0f;
	const glm::vec3 d((3.0f * inverse) * 11.0f, (-4.0f * inverse) * 11.0f, (0.0f * inverse) * 11.0f);
	EXPECT_EQ(line.from, glm::vec3(d.x + eye.x, d.y + eye.y, d.z + eye.z));
	EXPECT_EQ(line.to, glm::vec3(d.x + nearPoint.x, d.y + nearPoint.y, d.z + nearPoint.z));
	EXPECT_NEAR(LengthZyx(line.from - eye), 11.0f, 1e-4f);
}

TEST(HandLandPoint, AZeroDirectionIsLeftUnnormalised)
{
	const glm::vec3 eye(100.0f, 50.0f, 200.0f);
	const auto line = ShiftedAlongView(eye, eye, 11.0f, 1.0f);
	EXPECT_EQ(line.from, eye);
	EXPECT_EQ(line.to, eye);
	EXPECT_EQ(AlongMouseRay(eye, eye, 40.0f), eye);
}

TEST(HandLandPoint, ADepthNotAboveZeroTakesTheNearClip)
{
	const glm::vec3 eye(100.0f, 50.0f, 200.0f);
	const glm::vec3 nearPoint(100.6f, 49.3f, 200.4f);
	const auto atNear = ShiftedAlongView(eye, nearPoint, 2.5f, 2.5f);
	for (const float depth : {0.0f, -3.0f, k_NaN})
	{
		const auto line = ShiftedAlongView(eye, nearPoint, depth, 2.5f);
		EXPECT_EQ(line.from, atNear.from) << depth;
		EXPECT_EQ(line.to, atNear.to) << depth;
	}
}

TEST(HandLandPoint, LandNearerThanTheShiftIsSkipped)
{
	// Flat land 1 below the eye, the view down at 45 degrees along x, a near clip of 1
	constexpr uint16_t k_Height = 30;
	const float ground = static_cast<float>(k_Height) * land_line::k_HeightUnit;
	const glm::vec3 eye(2565.0f, ground + 1.0f, 2563.0f);
	constexpr float k_NearClip = 1.0f;
	const glm::vec3 nearPoint(eye.x + 0.70710677f, eye.y - 0.70710677f, eye.z);

	// The line from the eye meets the land 1 along x
	const auto unshifted = land_line::LandOrSeaAlong(eye, nearPoint, eye, Flat(k_Height));
	ASSERT_TRUE(unshifted.has_value());
	EXPECT_NEAR(unshifted->x, eye.x + 1.0f, 0.05f);

	// Moved on by the near clip plus 1 it starts under the land, and goes on down to the sea's level
	const auto line = ShiftedAlongView(eye, nearPoint, k_NearClip + 1.0f, k_NearClip);
	EXPECT_LT(line.from.y, ground);
	const auto shifted = land_line::LandOrSeaAlong(line.from, line.to, eye, Flat(k_Height));
	ASSERT_TRUE(shifted.has_value());
	EXPECT_NEAR(shifted->x, eye.x + eye.y, 0.05f);
}

TEST(HandLandPoint, TheNearSeaIsFoundAndTheFarSeaIsNot)
{
	const glm::vec3 eye(2560.0f, 100.0f, 2560.0f);
	// down at 45 degrees: the sea's level 100 along x
	const auto down = ShiftedAlongView(eye, {eye.x + 0.70710677f, eye.y - 0.70710677f, eye.z}, 2.0f, 1.0f);
	const auto nearSea = land_line::LandOrSeaAlong(down.from, down.to, eye, Sea);
	ASSERT_TRUE(nearSea.has_value());
	EXPECT_NEAR(nearSea->x, eye.x + 100.0f, 0.05f);
	EXPECT_NEAR(nearSea->y, eye.z, 0.05f);

	// nearly level: the sea's level 10000 along x, past the 7500 it counts within
	const auto shallow = ShiftedAlongView(eye, {eye.x + 1.0f, eye.y - 0.01f, eye.z}, 2.0f, 1.0f);
	EXPECT_FALSE(land_line::LandOrSeaAlong(shallow.from, shallow.to, eye, Sea).has_value());
}

TEST(HandLandPoint, NothingUnderTheCursorGivesTheHandsPositionUnshortened)
{
	const glm::vec3 eye(0.0f, 10.0f, 0.0f);
	const glm::vec3 hand(3.0f, 4.0f, 4.0f);
	auto inputs = NothingUnder(eye, hand);
	inputs.handScale = 1.1f;
	inputs.handSize = 1.3f;
	const auto result = HandLandPoint(inputs, HeightOf(0.0f));
	EXPECT_FALSE(result.found);
	EXPECT_EQ(result.point, hand);
	// subtractHand is on, but with nothing found the hand's own size is not taken off
	EXPECT_EQ(result.viewDistance, LengthZyx({0.0f - 3.0f, 10.0f - 4.0f, 0.0f - 4.0f}));
}

TEST(HandLandPoint, HoldingWithTheSpringOnTakesTheReachAlongTheMouseRay)
{
	const glm::vec3 eye(0.0f, 100.0f, 0.0f);
	auto inputs = NothingUnder(eye, {-1000.0f, -1000.0f, -1000.0f});
	inputs.nearPoint = {0.3f, 99.2f, 0.5f};
	inputs.holdingWithSpring = true;
	inputs.reach = 50.0f;
	const auto result = HandLandPoint(inputs, HeightOf(0.0f));
	EXPECT_TRUE(result.found);

	// length / |d| first, then each axis of d times it, added to the eye
	const glm::vec3 d(0.3f - 0.0f, 99.2f - 100.0f, 0.5f - 0.0f);
	const float scale = 50.0f / LengthZyx(d);
	const glm::vec3 expected(eye.x + scale * d.x, eye.y + scale * d.y, eye.z + scale * d.z);
	EXPECT_EQ(result.point, expected);
	EXPECT_EQ(AlongMouseRay(eye, inputs.nearPoint, 50.0f), expected);

	// it is a found point, so the hand's size comes off
	const float length = LengthZyx({eye.x - expected.x, eye.y - expected.y, eye.z - expected.z});
	EXPECT_EQ(result.viewDistance, length - (3.2f * 1.0f) * 1.0f);
}

TEST(HandLandPoint, TheHandsDepthIsItsScaleTimes3Point2ThenTimesItsSize)
{
	const glm::vec3 eye(0.0f, 10.0f, 0.0f);
	auto inputs = LandAt(eye, {3.0f, 4.0f});
	inputs.handScale = 1.1f;
	inputs.handSize = 1.3f;
	std::optional<glm::vec2> asked;
	const auto result = HandLandPoint(inputs, [&asked](glm::vec2 xz) {
		asked = xz;
		return 4.0f;
	});
	ASSERT_TRUE(asked.has_value());
	EXPECT_EQ(*asked, glm::vec2(3.0f, 4.0f));
	EXPECT_TRUE(result.found);
	EXPECT_EQ(result.point, glm::vec3(3.0f, 4.0f, 4.0f));

	// (3.2 scale) size and 3.2 (scale size) round apart here (checked with float32 arithmetic)
	const float length = LengthZyx({-3.0f, 6.0f, -4.0f});
	EXPECT_EQ(result.viewDistance, length - (3.2f * 1.1f) * 1.3f);
	EXPECT_EQ(std::bit_cast<uint32_t>(result.viewDistance), 0x404efdf2u);
	EXPECT_EQ(std::bit_cast<uint32_t>(length - 3.2f * (1.1f * 1.3f)), 0x404efdf4u);
}

TEST(HandLandPoint, LowLandKeepsTheFullDistance)
{
	const glm::vec3 eye(0.0f, 10.0f, 0.0f);
	const auto distanceAt = [&eye](float height, bool subtractHand) {
		auto inputs = LandAt(eye, {3.0f, 4.0f});
		inputs.subtractHand = subtractHand;
		return HandLandPoint(inputs, HeightOf(height)).viewDistance;
	};
	const auto full = [&eye](float height) { return LengthZyx({eye.x - 3.0f, eye.y - height, eye.z - 4.0f}); };
	// the sea, a lake at 0, and anything under 0.1
	EXPECT_EQ(distanceAt(0.0f, true), full(0.0f));
	EXPECT_EQ(distanceAt(-2.0f, true), full(-2.0f));
	EXPECT_EQ(distanceAt(0.09f, true), full(0.09f));
	// at 0.1 the hand's size comes off
	EXPECT_EQ(distanceAt(0.1f, true), full(0.1f) - 3.2f);
	// and never without subtractHand
	EXPECT_EQ(distanceAt(0.1f, false), full(0.1f));
}

TEST(HandLandPoint, TheDistanceIsKeptBetween2AndTheReach)
{
	const glm::vec3 eye(0.0f, 0.0f, 0.0f);
	const auto distanceTo = [&eye](float z, float reach = 300.0f) {
		auto inputs = NothingUnder(eye, {0.0f, 0.0f, z});
		inputs.reach = reach;
		return HandLandPoint(inputs, HeightOf(0.0f)).viewDistance;
	};
	EXPECT_EQ(distanceTo(0.0f), 2.0f);
	EXPECT_EQ(distanceTo(1.0f), 2.0f);
	EXPECT_EQ(distanceTo(2.0f), 2.0f);
	EXPECT_EQ(distanceTo(k_NaN), 2.0f);
	EXPECT_EQ(distanceTo(2.5f), 2.5f);
	EXPECT_EQ(distanceTo(50.0f), 50.0f);
	EXPECT_EQ(distanceTo(300.0f), 300.0f);
	EXPECT_EQ(distanceTo(500.0f), 300.0f);
	// a NaN reach keeps the distance
	EXPECT_EQ(distanceTo(50.0f, k_NaN), 50.0f);

	// the hand's size taken off can bring it under 2: 3.8 - 3.2
	const auto inputs = LandAt({0.0f, 4.0f, 0.0f}, {0.0f, 0.0f});
	EXPECT_EQ(HandLandPoint(inputs, HeightOf(0.2f)).viewDistance, 2.0f);
}

TEST(HandLandPoint, ScaleForViewDistance)
{
	// kept between 2 and the reach first; the original's bits (oracle)
	EXPECT_EQ(std::bit_cast<uint32_t>(ScaleForViewDistance(0.0f, 300.0f)), 0x3E8D48C9U);
	EXPECT_EQ(std::bit_cast<uint32_t>(ScaleForViewDistance(5.0f, 300.0f)), 0x3F13088CU);
	EXPECT_EQ(ScaleForViewDistance(10.0f, 300.0f), 1.0f);
	EXPECT_EQ(ScaleForViewDistance(150.0f, 300.0f), 1.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(ScaleForViewDistance(500.0f, 300.0f)), 0x3FF904A8U);
}
