/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fence's crossing test (Camera/CameraZones): his cases, and the same answers as the crossing test it replaced
// (player_camera::InsideInclusion, kept below as it was) on many made-up fences, points and lines. The zones a file
// sets (CameraZoneSystem)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstring>

#include <array>
#include <bit>
#include <limits>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/CameraZones.h"
#include "Camera/PlayerCameraScript.h"
#include "ECS/Systems/Implementations/CameraZoneSystem.h"

using namespace openblack;
using namespace openblack::camera_zones;

namespace
{
/// A square fence 100 across, from (0, 0) to (100, 100)
const std::vector<glm::vec3> k_Square {
    {0.0f, 0.0f, 0.0f},
    {100.0f, 0.0f, 0.0f},
    {100.0f, 0.0f, 100.0f},
    {0.0f, 0.0f, 100.0f},
};

// The crossing test before camera_zones::CrossFence, as it was, on its zone's two fields
struct OldZone
{
	int32_t drawForceField = 0;
	std::vector<glm::vec3> forceFieldPoints;
};
constexpr float k_OldSquaredEpsilon = 1e-08f;
constexpr double k_OldEpsilon = 9.9999997473787516e-05;
constexpr float k_OldFar = 1.0e20f;

bool OldInsideInclusion(const OldZone& zone, const glm::vec3& p, const glm::vec3& dir, glm::vec3* hit, glm::vec3* normal)
{
	if (zone.drawForceField == 0)
	{
		return true;
	}
	if (hit != nullptr)
	{
		*hit = p;
	}
	const auto& points = zone.forceFieldPoints;
	if (points.size() < 3)
	{
		return true;
	}
	int32_t crossings = 0;
	float bestAhead = k_OldFar;
	float bestBehind = k_OldFar;
	bool foundAhead = false;
	bool foundBehind = false;
	glm::vec3 hitAhead = p;
	glm::vec3 hitBehind = p;
	glm::vec2 normalAhead(0.0f, 1.0f);
	glm::vec2 normalBehind(0.0f, 1.0f);
	const glm::vec3* a = &points.back(); // the last point
	for (const auto& b : points)
	{
		const float ex = a->x - b.x;
		const float ez = a->z - b.z;
		const float wx = p.x - b.x;
		const float wz = p.z - b.z;
		const float length2 = ex * ex + ez * ez;
		if (wz * wz + wx * wx < k_OldSquaredEpsilon) // on a point
		{
			return true;
		}
		const glm::vec3* next = &b;
		if (length2 > k_OldSquaredEpsilon)
		{
			const float nex = -ex;
			const float denominator = dir.z * nex + dir.x * ez;
			if (static_cast<double>(std::abs(denominator)) > k_OldEpsilon)
			{
				const float numerator = ((b.x * ez + b.z * nex) - p.x * ez) - p.z * nex;
				const float t = numerator / denominator;
				const glm::vec3 q(t * dir.x + p.x, dir.y * t + p.y, t * dir.z + p.z);
				const float s = ((q.x - b.x) * ex + (q.z - b.z) * ez) / length2;
				// 0 <= s < 1
				if (!(s < 0.0f) && s < 1.0f)
				{
					if (static_cast<double>(std::abs(t)) < k_OldEpsilon) // on the edge
					{
						return true;
					}
					if (t > 0.0f)
					{
						++crossings;
						if (t < bestAhead)
						{
							bestAhead = t;
							hitAhead = q;
							normalAhead = {ez, nex};
							foundAhead = true;
						}
					}
					else if (t < 0.0f && -t < bestBehind)
					{
						bestBehind = -t;
						hitBehind = q;
						normalBehind = {ez, nex};
						foundBehind = true;
					}
				}
			}
		}
		a = next; // the edge's end becomes the next start
	}
	bool found = false;
	if (foundAhead)
	{
		if (hit != nullptr)
		{
			*hit = hitAhead;
		}
		if (normal != nullptr)
		{
			*normal = {normalAhead.x, 0.0f, normalAhead.y};
		}
		found = true;
	}
	else if (foundBehind)
	{
		if (hit != nullptr)
		{
			*hit = hitBehind;
		}
		if (normal != nullptr)
		{
			*normal = {normalBehind.x, 0.0f, normalBehind.y};
		}
		found = true;
	}
	if (hit != nullptr) // the nearest polygon point when nearer in x / z
	{
		const float hx = hit->x - p.x;
		const float hz = hit->z - p.z;
		float best = hx * hx + hz * hz;
		for (const auto& v : points)
		{
			const float vx = v.x - p.x;
			const float vz = v.z - p.z;
			const float d = vz * vz + vx * vx;
			if (d < best || !found)
			{
				best = d;
				*hit = v;
				found = true;
			}
		}
	}
	return (crossings & 1) != 0;
}

/// A made-up number generator, so that the cases are the same every run
struct Cases
{
	uint32_t state = 12345;
	uint32_t Next()
	{
		state = state * 1664525u + 1013904223u;
		return state >> 8;
	}
	/// Mostly whole numbers on a coarse grid, so that points fall on corners and sides, and some fractions
	float Coordinate()
	{
		const auto n = Next();
		const float whole = static_cast<float>(static_cast<int32_t>(n % 21) - 10) * 10.0f;
		return (n & 0x100u) != 0u ? whole : whole + static_cast<float>(n % 997) / 97.0f;
	}
	glm::vec3 Point() { return {Coordinate(), static_cast<float>(Next() % 50), Coordinate()}; }
	glm::vec3 Direction()
	{
		switch (Next() % 4)
		{
		case 0:
			return {1.0f, 0.0f, 0.0f};
		case 1:
			return {0.0f, 0.0f, 1.0f};
		case 2:
			return {1.0f, 0.5f, -1.0f};
		default:
			return {Coordinate(), Coordinate(), Coordinate()};
		}
	}
};
} // namespace

TEST(CameraZones, APointIsInsideTheFenceWhenALineFromItCrossesItAnOddNumberOfTimesAhead)
{
	const auto inside = CrossFence(k_Square, true, {50.0f, 10.0f, 50.0f}, {1.0f, 0.0f, 0.0f});
	EXPECT_TRUE(inside.inside);
	// The crossing ahead is the nearest, unless a corner is nearer
	EXPECT_FLOAT_EQ(inside.closest.x, 100.0f);
	EXPECT_FLOAT_EQ(inside.closest.z, 50.0f);

	const auto outside = CrossFence(k_Square, true, {150.0f, 10.0f, 50.0f}, {-1.0f, 0.0f, 0.0f});
	EXPECT_FALSE(outside.inside);
	EXPECT_FLOAT_EQ(outside.closest.x, 100.0f);
	EXPECT_FLOAT_EQ(outside.closest.y, 10.0f);

	// A line that crosses nowhere ahead takes the nearest crossing behind
	const auto behind = CrossFence(k_Square, true, {150.0f, 0.0f, 50.0f}, {1.0f, 0.0f, 0.0f});
	EXPECT_FALSE(behind.inside);
	EXPECT_FLOAT_EQ(behind.closest.x, 100.0f);
	EXPECT_FLOAT_EQ(behind.closest.z, 50.0f);
	// One that crosses nowhere at all takes the nearest corner
	const auto away = CrossFence(k_Square, true, {150.0f, 0.0f, 150.0f}, {1.0f, 0.0f, -1.0f});
	EXPECT_FALSE(away.inside);
	EXPECT_FLOAT_EQ(away.closest.x, 100.0f);
	EXPECT_FLOAT_EQ(away.closest.z, 100.0f);

	// Off, or with fewer than three corners, everywhere is inside
	EXPECT_TRUE(CrossFence(k_Square, false, {500.0f, 0.0f, 500.0f}, {1.0f, 0.0f, 0.0f}).inside);
	EXPECT_TRUE(CrossFence(std::span(k_Square).first(2), true, {500.0f, 0.0f, 500.0f}, {1.0f, 0.0f, 0.0f}).inside);
	// On a corner it is inside
	EXPECT_TRUE(CrossFence(k_Square, true, {100.0f, 0.0f, 100.0f}, {1.0f, 0.0f, 0.0f}).inside);
}

TEST(CameraZones, ACameraOutsideTheFenceIsPutBackPastWhereItCrossed)
{
	// From the middle out past the right side: tested along the line back to where it started, it crossed at x = 100,
	// and the first point tried, 20 back in along the way it came, is inside
	const glm::vec3 start {50.0f, 30.0f, 50.0f};
	const auto kept =
	    PushInsideFence(k_Square, true, start, {120.0f, 30.0f, 50.0f}, {130.0f, 0.0f, 50.0f}, SearchRadius(false));
	ASSERT_TRUE(kept.target.has_value());
	EXPECT_FLOAT_EQ(kept.target->x, 80.0f);
	EXPECT_FLOAT_EQ(kept.target->z, 50.0f);
	EXPECT_EQ(kept.inclusionDistance, 0.0f);
	// Turning by a drag round the edge of the screen, 50 back in
	const auto turning =
	    PushInsideFence(k_Square, true, start, {120.0f, 30.0f, 50.0f}, {130.0f, 0.0f, 50.0f}, SearchRadius(true));
	ASSERT_TRUE(turning.target.has_value());
	EXPECT_FLOAT_EQ(turning.target->x, 50.0f);
	// Inside, nothing to put back, and the distance across the ground to the fence's nearest point the test gives: the
	// line crosses ahead at (0, 50), but the corner (100, 0) is nearer, so it is sqrt(10^2 + 50^2). The original's test
	// and distance give the same for this square (its crossing test returns the corner, and its distance 0x424BF5F6)
	const auto inside =
	    PushInsideFence(k_Square, true, start, {90.0f, 30.0f, 50.0f}, {95.0f, 0.0f, 50.0f}, SearchRadius(false));
	EXPECT_FALSE(inside.target.has_value());
	EXPECT_EQ(std::bit_cast<uint32_t>(inside.inclusionDistance), 0x424BF5F6u); // 50.990196
	// Hardly moved, the line goes towards what it looks at instead: from (90, 50) towards (95, 50) the fence is 10 ahead
	const auto still = PushInsideFence(k_Square, true, {90.0f, 30.0f, 50.0f}, {90.0f, 30.0f, 50.0f}, {95.0f, 0.0f, 50.0f},
	                                   SearchRadius(false));
	EXPECT_FLOAT_EQ(still.inclusionDistance, 10.0f);
	// Without a fence the scripts read a distance of 1e10
	const auto off = PushInsideFence(k_Square, false, start, {500.0f, 30.0f, 50.0f}, glm::vec3(0.0f), SearchRadius(false));
	EXPECT_FALSE(off.target.has_value());
	EXPECT_EQ(off.inclusionDistance, k_NoFenceDistance);
}

TEST(CameraZones, WhatMovesWithTheCameraKeepsItsPlaceFromIt)
{
	const glm::vec3 moved = MovedWithOrigin({10.0f, 5.0f, 20.0f}, {120.0f, 30.0f, 50.0f}, {80.0f, 30.0f, 50.0f});
	EXPECT_EQ(moved, glm::vec3(-30.0f, 5.0f, 20.0f));
	// The shake and the sound as the fence puts it back
	EXPECT_EQ(k_FenceShakeRadius, 100.0f);
	EXPECT_EQ(k_FenceShakeStrength, 1.0f);
	EXPECT_EQ(k_FenceShakeMilliseconds, 400);
	EXPECT_EQ(k_OutOfBoundsSample, 43);
}

TEST(CameraZones, TheCrossingIsTheOneItReplaced)
{
	Cases cases;
	uint32_t checked = 0;
	for (int fenceNumber = 0; fenceNumber < 400; ++fenceNumber)
	{
		OldZone zone {.drawForceField = fenceNumber % 17 == 0 ? 0 : 1};
		const auto corners = 1 + cases.Next() % 8;
		for (uint32_t i = 0; i < corners; ++i)
		{
			zone.forceFieldPoints.push_back(cases.Point());
		}
		for (int pointNumber = 0; pointNumber < 60; ++pointNumber)
		{
			// a corner, the middle of a side, or anywhere
			auto point = cases.Point();
			const auto& points = zone.forceFieldPoints;
			if (const auto pick = cases.Next() % 6; pick == 0)
			{
				point = points.at(cases.Next() % points.size());
			}
			else if (pick == 1)
			{
				const auto i = cases.Next() % points.size();
				point = (points.at(i) + points.at((i + 1) % points.size())) * 0.5f;
			}
			const auto direction = cases.Direction();
			glm::vec3 hit {-1.0f};
			glm::vec3 normal {-2.0f};
			const bool inside = OldInsideInclusion(zone, point, direction, &hit, &normal);
			const auto crossing = CrossFence(points, zone.drawForceField != 0, point, direction);
			SCOPED_TRACE(::testing::Message() << "fence " << fenceNumber << ", point " << pointNumber);
			ASSERT_EQ(crossing.inside, inside);
			if (zone.drawForceField == 0)
			{
				continue; // the old test left its hit as it was
			}
			ASSERT_EQ(crossing.closest, hit);
			if (normal != glm::vec3(-2.0f))
			{
				ASSERT_EQ(crossing.normal, normal);
			}
			++checked;
		}
	}
	EXPECT_GT(checked, 20000u);
}

TEST(MapDisc, IsTheMapsSideRoundItsMiddleScaledWithTheMap)
{
	const auto disc = MapDiscFor(512);
	EXPECT_EQ(disc.centre, glm::vec2(2560.0f, 2560.0f));
	EXPECT_EQ(disc.radius, 5120.0f);
	const auto twice = MapDiscFor(1024);
	EXPECT_EQ(twice.centre, glm::vec2(5120.0f, 5120.0f));
	EXPECT_EQ(twice.radius, 10240.0f);
	EXPECT_FLOAT_EQ(AcrossFromMiddle(disc, {2563.0f, 900.0f, 2564.0f}), 5.0f);
}

TEST(MapDisc, APointIsInsideOnlyStrictlyWithinItsRadiusAcrossTheGround)
{
	const auto disc = MapDiscFor(512);
	EXPECT_TRUE(InsideMapDisc(disc, {7679.0f, 3000.0f, 2560.0f}));
	EXPECT_FALSE(InsideMapDisc(disc, {7680.0f, 0.0f, 2560.0f})); // on the edge
	EXPECT_FALSE(InsideMapDisc(disc, {std::numeric_limits<float>::quiet_NaN(), 0.0f, 2560.0f}));
}

TEST(CameraZones, APointIsInsideACylinderOrADomeWithTheMarginAndNeverInsideAnotherKind)
{
	// the margin of 3 on the radius (5 + 3 = 8 across) and on a dome's height (5 + 3 = 8 up)
	const std::array exclusions {
	    Exclusion {.position = {0.0f, 10.0f, 0.0f}, .radius = 5.0f, .height = 5.0f, .kind = Exclusion::Kind::Cylinder},
	};
	EXPECT_TRUE(InsideExclusion(exclusions, {7.9f, 1000.0f, 0.0f})); // a cylinder has no top
	EXPECT_FALSE(InsideExclusion(exclusions, {8.0f, 10.0f, 0.0f}));  // on its edge
	const std::array domes {
	    Exclusion {.position = {0.0f, 10.0f, 0.0f}, .radius = 5.0f, .height = 5.0f, .kind = Exclusion::Kind::Dome},
	};
	EXPECT_TRUE(InsideExclusion(domes, {0.0f, 17.9f, 0.0f}));
	EXPECT_FALSE(InsideExclusion(domes, {0.0f, 18.0f, 0.0f})); // on the dome
	EXPECT_FALSE(InsideExclusion(domes, {5.0f, 15.0f, 5.0f})); // within the radius, outside the dome
	EXPECT_TRUE(InsideExclusion(domes, {0.0f, std::numeric_limits<float>::quiet_NaN(), 0.0f}));
	const std::array others {
	    Exclusion {.position = {0.0f, 10.0f, 0.0f}, .radius = 5.0f, .height = 5.0f, .kind = Exclusion::Kind::Other},
	};
	EXPECT_FALSE(InsideExclusion(others, {0.0f, 10.0f, 0.0f}));
	EXPECT_FALSE(InsideExclusion(std::span<const Exclusion> {}, {0.0f, 10.0f, 0.0f}));
}

TEST(MapDisc, MeasuresAcrossTheGroundWhateverTheHeight)
{
	// 5000 across the ground and 3000 up: over 5120 away in space, but inside
	const auto disc = MapDiscFor(512);
	EXPECT_FALSE(KeepInsideMapDisc(disc, {7560.0f, 3000.0f, 2560.0f}, {}).broughtBack);
	EXPECT_FALSE(KeepInsideMapDisc(disc, {2560.0f, 0.0f, 7680.0f}, {}).broughtBack);
	// Exactly on the edge, or not a number, stays
	EXPECT_FALSE(KeepInsideMapDisc(disc, {7680.0f, 0.0f, 2560.0f}, {}).broughtBack);
	EXPECT_FALSE(KeepInsideMapDisc(disc, {std::numeric_limits<float>::quiet_NaN(), 0.0f, 9000.0f}, {}).broughtBack);
}

TEST(MapDisc, BringsTheCameraBackToTheEdgeKeepingItsHeight)
{
	// 10000 across the ground from the middle: brought in to 5120, along the same way, at the same height
	const auto kept = KeepInsideMapDisc(MapDiscFor(512), {8560.0f, 800.0f, 10560.0f}, {8000.0f, 700.0f, 10000.0f});
	ASSERT_TRUE(kept.broughtBack);
	EXPECT_NEAR(kept.origin.x, 5632.0f, 1e-2f);
	EXPECT_EQ(kept.origin.y, 800.0f);
	EXPECT_NEAR(kept.origin.z, 6656.0f, 1e-2f);
	// The focus is only put through the middle, which leaves these as they are
	EXPECT_EQ(kept.focus, glm::vec3(8000.0f, 700.0f, 10000.0f));
}

namespace
{
/// A point's bits before and after the original's round trip through the map's middle (2560, 0, 2560), from the
/// original's own point subtraction and addition
struct MiddleRoundTrip
{
	std::array<uint32_t, 3> in;
	std::array<uint32_t, 3> out;
};
// Inside the disc: below 1280 (a tenth, a thousandth, a hundred) a coordinate is rounded, minus zero becomes zero;
// from 1280 up it stays
constexpr std::array<MiddleRoundTrip, 6> k_InsideRoundTrips {{
    {{0x3DCCCCCD, 0x44611000, 0x3DCCCCCD}, {0x3DCD0000, 0x44611000, 0x3DCD0000}},
    {{0x3A83126F, 0x80000000, 0x449FFCCD}, {0x3A800000, 0x00000000, 0x449FFCCD}},
    {{0x449FFFAE, 0x42153333, 0x44A00000}, {0x449FFFAE, 0x42153333, 0x44A00000}},
    {{0x452001F8, 0x00000000, 0x451FFE08}, {0x452001F8, 0x00000000, 0x451FFE08}},
    {{0x45A00000, 0x3F800000, 0x45A00002}, {0x45A00000, 0x3F800000, 0x45A00002}},
    {{0x45DAC59A, 0x80000000, 0x42C8999A}, {0x45DAC59A, 0x00000000, 0x42C899A0}},
}};
// Beyond the disc, as a focus may be: negative coordinates are rounded too, and a hundred thousandth goes to zero
constexpr std::array<MiddleRoundTrip, 3> k_BeyondRoundTrips {{
    {{0x461C3FFF, 0x4145851F, 0xC2173D71}, {0x461C3FFF, 0x4145851F, 0xC2173D80}},
    {{0xC2F6E979, 0x40800000, 0x46200152}, {0xC2F6E980, 0x40800000, 0x46200152}},
    {{0x469C4057, 0x453B8000, 0x3727C5AC}, {0x469C4057, 0x453B8000, 0x00000000}},
}};

glm::vec3 PointOf(const std::array<uint32_t, 3>& bits)
{
	return {std::bit_cast<float>(bits[0]), std::bit_cast<float>(bits[1]), std::bit_cast<float>(bits[2])};
}

std::array<uint32_t, 3> BitsOf(const glm::vec3& point)
{
	return {std::bit_cast<uint32_t>(point.x), std::bit_cast<uint32_t>(point.y), std::bit_cast<uint32_t>(point.z)};
}
} // namespace

TEST(MapDisc, RoundsTheOriginAndTheFocusThroughTheMiddleAsTheOriginal)
{
	const auto disc = MapDiscFor(512);
	const glm::vec3 middle {2560.0f, 0.0f, 2560.0f};
	for (const auto& trip : k_InsideRoundTrips)
	{
		const auto asOrigin = KeepInsideMapDisc(disc, PointOf(trip.in), middle);
		EXPECT_FALSE(asOrigin.broughtBack);
		EXPECT_EQ(BitsOf(asOrigin.origin), trip.out);
		EXPECT_EQ(asOrigin.focus, middle);
		EXPECT_EQ(BitsOf(KeepInsideMapDisc(disc, middle, PointOf(trip.in)).focus), trip.out);
	}
	for (const auto& trip : k_BeyondRoundTrips)
	{
		EXPECT_EQ(BitsOf(KeepInsideMapDisc(disc, middle, PointOf(trip.in)).focus), trip.out);
	}
}

TEST(CameraZoneSystem, AFilesZonesAreKeptToWhateverItSays)
{
	player_camera::Zone file;
	file.useExclusions = 0;
	file.drawForceField = 0;
	file.useMaxAltitude = 1;
	file.maxAltitude = 62.5f;
	file.heightAboveLand = 80.0f;
	file.forceFieldPoints = k_Square;
	// a cylinder at (10, 20, 30), 7 across and 4 high, as the game keeps one: two links and an id first
	player_camera::Exclusion exclusion;
	const std::array<float, 5> numbers {10.0f, 20.0f, 30.0f, 7.0f, 4.0f};
	std::memcpy(exclusion.bytes.data() + 12, numbers.data(), sizeof(numbers));
	const uint32_t cylinder = 1;
	std::memcpy(exclusion.bytes.data() + 32, &cylinder, sizeof(cylinder));
	file.exclusions.push_back(exclusion);
	// and one of a kind that is neither a dome nor a cylinder
	const uint32_t neither = 2;
	std::memcpy(exclusion.bytes.data() + 32, &neither, sizeof(neither));
	file.exclusions.push_back(exclusion);
	const auto zones = ecs::systems::CameraZoneSystem::FromFile(file);
	EXPECT_TRUE(zones.exclusionsOn);
	EXPECT_TRUE(zones.fenceOn);
	EXPECT_TRUE(zones.useMaxAltitude);
	EXPECT_FALSE(zones.useHeightAboveLand);
	EXPECT_EQ(zones.maxAltitude, 62.5f);
	EXPECT_EQ(zones.heightAboveLand, 80.0f);
	EXPECT_EQ(zones.fence, k_Square);
	ASSERT_EQ(zones.exclusions.size(), 2u);
	EXPECT_EQ(zones.exclusions[0].position, glm::vec3(10.0f, 20.0f, 30.0f));
	EXPECT_EQ(zones.exclusions[0].radius, 7.0f);
	EXPECT_EQ(zones.exclusions[0].height, 4.0f);
	EXPECT_EQ(zones.exclusions[0].kind, Exclusion::Kind::Cylinder);
	EXPECT_EQ(zones.exclusions[1].kind, Exclusion::Kind::Other);
}

TEST(CameraZoneSystem, AFileThatCantBeReadLeavesNoZones)
{
	// a file no land has, or no file system and resources to read it through
	ecs::systems::CameraZoneSystem system;
	EXPECT_FALSE(system.SetZones("NoSuchZone.exc"));
	EXPECT_FALSE(system.GetZones().fenceOn);
	EXPECT_TRUE(system.GetZones().exclusionsOn);
	EXPECT_EQ(system.GetZones().maxAltitude, k_DefaultLimit);
	EXPECT_TRUE(system.GetZones().fence.empty());
	EXPECT_TRUE(system.GetFileName().empty());
}
