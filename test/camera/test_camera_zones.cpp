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

#include <cmath>
#include <cstdint>
#include <cstring>

#include <array>
#include <bit>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
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

TEST(CameraZoneSystem, TheMapsClearTakesTheFenceAndLimitsAndThePlacesThatGoWithTheMap)
{
	Zones zones {
	    .exclusionsOn = false,
	    .fenceOn = true,
	    .useMaxAltitude = true,
	    .useHeightAboveLand = true,
	    .maxAltitude = 62.5f,
	    .heightAboveLand = 37.75f,
	    .fence = k_Square,
	    .exclusions = {{.position = {10.0f, 0.0f, 10.0f}, .radius = 5.0f, .saved = true},
	                   {.position = {20.0f, 0.0f, 20.0f}, .radius = 6.0f, .saved = false}},
	};
	const auto cleared = AfterMapClear(std::move(zones));
	const Zones start;
	EXPECT_EQ(cleared.exclusionsOn, start.exclusionsOn);
	EXPECT_FALSE(cleared.fenceOn);
	EXPECT_FALSE(cleared.useMaxAltitude);
	EXPECT_FALSE(cleared.useHeightAboveLand);
	EXPECT_EQ(cleared.maxAltitude, k_DefaultLimit);
	EXPECT_EQ(cleared.heightAboveLand, k_DefaultLimit);
	EXPECT_TRUE(cleared.fence.empty());
	// only the place not marked to go with the map is still kept out of
	ASSERT_EQ(cleared.exclusions.size(), 1u);
	EXPECT_EQ(cleared.exclusions[0].position, glm::vec3(20.0f, 0.0f, 20.0f));
	EXPECT_EQ(cleared.exclusions[0].radius, 6.0f);
}

TEST(CameraZoneSystem, AFilesPlacesSayWhetherTheyGoWithTheMap)
{
	player_camera::Zone file;
	player_camera::Exclusion exclusion;
	const uint32_t saved = 1;
	std::memcpy(exclusion.bytes.data() + 36, &saved, sizeof(saved));
	file.exclusions.push_back(exclusion);
	file.exclusions.push_back(player_camera::Exclusion {});
	const auto zones = ecs::systems::CameraZoneSystem::FromFile(file);
	ASSERT_EQ(zones.exclusions.size(), 2u);
	EXPECT_TRUE(zones.exclusions[0].saved);
	EXPECT_FALSE(zones.exclusions[1].saved);
}

TEST(CameraZoneSystem, AClearLeavesNoFileAndTheStartZones)
{
	ecs::systems::CameraZoneSystem system;
	system.ClearMap();
	EXPECT_TRUE(system.GetFileName().empty());
	EXPECT_FALSE(system.GetZones().fenceOn);
	EXPECT_TRUE(system.GetZones().exclusions.empty());
	system.ClearForScriptsRestart();
	EXPECT_TRUE(system.GetFileName().empty());
	EXPECT_TRUE(system.GetZones().exclusionsOn);
	EXPECT_EQ(system.GetZones().maxAltitude, k_DefaultLimit);
}

TEST(CameraZones, TheHeightsComeFromTheLandUnderTheCameraAndTheZonesLimits)
{
	// No limits set: 3 above the land, and the lower of 30000 and 30000 above the land
	const auto open = HeightBoundsAt(10.0f, 3.0f, Zones {});
	EXPECT_EQ(open.lowest, 13.0f);
	EXPECT_EQ(open.highest, 30000.0f);
	EXPECT_EQ(HeightBoundsAt(-10.0f, 3.0f, Zones {}).highest, 29990.0f);
	// The highest and the most above the land: the lower of the two
	Zones zones;
	zones.useMaxAltitude = true;
	zones.maxAltitude = 200.0f;
	zones.useHeightAboveLand = true;
	zones.heightAboveLand = 50.0f;
	EXPECT_EQ(HeightBoundsAt(170.0f, 3.0f, zones).highest, 200.0f);
	EXPECT_EQ(HeightBoundsAt(100.0f, 3.0f, zones).highest, 150.0f);
	// The slide's least rise is the float 0.01 as a double, as the game keeps it
	EXPECT_EQ(std::bit_cast<std::uint64_t>(k_SlideLeastRise), 0x3F847AE140000000ULL);
}

TEST(CameraZones, BelowTheLowestTheCameraAloneIsLifted)
{
	const HeightBounds bounds {.lowest = 13.0f, .highest = 30010.0f};
	const auto kept = KeepBetweenHeights(bounds, true, {5.0f, 11.0f, 7.0f}, {5.0f, 9.0f, 40.0f}, {5.0f, 9.0f, 40.0f});
	EXPECT_TRUE(kept.raised);
	EXPECT_FALSE(kept.slid);
	EXPECT_EQ(kept.origin, glm::vec3(5.0f, 13.0f, 7.0f));
	EXPECT_EQ(kept.focus, glm::vec3(5.0f, 9.0f, 40.0f));
	EXPECT_EQ(kept.turningPoint, glm::vec3(5.0f, 9.0f, 40.0f));
	// A height that is not a number is lifted too
	const auto nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(KeepBetweenHeights(bounds, true, {0.0f, nan, 0.0f}, {}, {}).origin.y, 13.0f);
	// At the lowest, nothing moves
	EXPECT_FALSE(KeepBetweenHeights(bounds, true, {0.0f, 13.0f, 0.0f}, {}, {}).raised);
}

TEST(CameraZones, AboveTheHighestTheCameraSlidesDownItsViewWithWhatMovesWithIt)
{
	const HeightBounds bounds {.lowest = 3.0f, .highest = 100.0f};
	// 20 over, looking down 20 for 40 ahead: everything goes one view along
	const auto kept = KeepBetweenHeights(bounds, true, {0.0f, 120.0f, 0.0f}, {0.0f, 100.0f, 40.0f}, {1.0f, 100.0f, 40.0f});
	EXPECT_TRUE(kept.slid);
	EXPECT_FALSE(kept.raised);
	EXPECT_EQ(kept.origin, glm::vec3(0.0f, 100.0f, 40.0f));
	EXPECT_EQ(kept.focus, glm::vec3(0.0f, 80.0f, 80.0f));
	EXPECT_EQ(kept.turningPoint, glm::vec3(1.0f, 80.0f, 80.0f));
	// A view that hardly falls is left where it is, and still counts
	const auto flat = KeepBetweenHeights(bounds, true, {0.0f, 120.0f, 0.0f}, {0.0f, 119.995f, 40.0f}, {});
	EXPECT_TRUE(flat.slid);
	EXPECT_EQ(flat.origin, glm::vec3(0.0f, 120.0f, 0.0f));
	// Without the exclusions there is no slide
	EXPECT_FALSE(KeepBetweenHeights(bounds, false, {0.0f, 120.0f, 0.0f}, {0.0f, 100.0f, 40.0f}, {}).slid);
}

TEST(CameraZones, WhatTheCameraLooksAtIsSetAgainAlongItsView)
{
	EXPECT_EQ(FocusAlongView({1.0f, 2.0f, 3.0f}, {1.0f, 2.0f, 7.0f}, 10.0f), glm::vec3(1.0f, 2.0f, 13.0f));
	const auto slanted = FocusAlongView({1.0f, 2.0f, 3.0f}, {4.0f, 2.0f, 7.0f}, 10.0f);
	EXPECT_NEAR(slanted.x, 7.0f, 1e-5f);
	EXPECT_EQ(slanted.y, 2.0f);
	EXPECT_NEAR(slanted.z, 11.0f, 1e-5f);
	// No view: what it looks at stays on the camera
	EXPECT_EQ(FocusAlongView({1.0f, 2.0f, 3.0f}, {1.0f, 2.0f, 3.0f}, 10.0f), glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST(CameraZones, TheViewIsMadeAUnitLongWithTheSquaresAddedZYThenX)
{
	// A view whose squares added z, y then x round apart from x, y then z (glm's order): the focus is the game's,
	// bit for bit, one last bit off glm::normalize's in x and z
	const glm::vec3 view {std::bit_cast<float>(0xC1E2B872U), std::bit_cast<float>(0xC0F93A1AU),
	                      std::bit_cast<float>(0xC23C6239U)};
	const auto focus = FocusAlongView({}, view, 10.0f);
	EXPECT_EQ(std::bit_cast<std::uint32_t>(focus.x), 0xC0A35C24U);
	EXPECT_EQ(std::bit_cast<std::uint32_t>(focus.y), 0xBFB3939CU);
	EXPECT_EQ(std::bit_cast<std::uint32_t>(focus.z), 0xC107BCA8U);
	// The distance to the land takes the same order: one last bit from glm::distance's here
	EXPECT_EQ(std::bit_cast<std::uint32_t>(DistanceZYX({}, view)), 0x425E0EB1U);
	// A view of no length, or with parts that are not numbers, is left alone
	const auto nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(FocusAlongView({4.0f, 5.0f, 6.0f}, {4.0f, 5.0f, 6.0f}, 0.1f), glm::vec3(4.0f, 5.0f, 6.0f));
	const auto unnumbered = FocusAlongView({}, {nan, nan, nan}, 10.0f);
	EXPECT_TRUE(std::isnan(unnumbered.x));
}

// The largest move: the player camera's origin goes no further in a frame than 1.6 times its heading distance a second,
// and at least 300, from where it was heading at the frame's start; further, it is brought back along the line to that
// distance, the line made a unit long by one over its length
TEST(LargestMove, AMoveFurtherThanTheLargestIsHeldToItAlongItsLine)
{
	const glm::vec3 from {1000.0f, 50.0f, 1000.0f};
	// a heading distance of 100 over 16 ms gives 2.56, so the least, 300: a move of 200 or of 300 exactly goes on
	const float least = FrameMoveLimit(100.0f, 0.016f);
	EXPECT_EQ(least, 300.0f);
	EXPECT_FALSE(LimitFrameMove(from, from + glm::vec3(200.0f, 0.0f, 0.0f), least).has_value());
	EXPECT_FALSE(LimitFrameMove(from, from + glm::vec3(180.0f, 0.0f, 240.0f), least).has_value());
	// one of 500 is held to 300 along it: (400, 0, 300) times 1 / 500, then 300
	const auto held = LimitFrameMove(from, from + glm::vec3(400.0f, 0.0f, 300.0f), least);
	ASSERT_TRUE(held.has_value());
	EXPECT_EQ(*held, glm::vec3(1240.0f, 50.0f, 1180.0f));
	// far from what it looks at the largest move is longer: 1.6 x 20000 x 0.016 = 512, so 500 goes on
	const float far = FrameMoveLimit(20000.0f, 0.016f);
	EXPECT_EQ(far, 512.0f);
	EXPECT_FALSE(LimitFrameMove(from, from + glm::vec3(400.0f, 0.0f, 300.0f), far).has_value());
	// and a move of 600 straight up is held to 512 of it
	const auto up = LimitFrameMove(from, from + glm::vec3(0.0f, 600.0f, 0.0f), far);
	ASSERT_TRUE(up.has_value());
	EXPECT_EQ(*up, glm::vec3(1000.0f, 562.0f, 1000.0f));
}

// The camera's land probe and its push out of the cylinders (raffclar's cases, refitted): which exclusions are near,
// the cylinders' push across the ground, the domes' roofs as ground, and the sixteen points' pushes
namespace
{
Ground FlatLand(float height)
{
	return {.height = [height](glm::vec2) { return height; }, .normal = [](glm::vec2) { return glm::vec3(0.0f, 1.0f, 0.0f); }};
}
} // namespace

TEST(CameraLandProbe, OnlyExclusionsNearWhereTheCameraWasHeadingCount)
{
	const std::vector<Exclusion> exclusions {
	    {.position = {100.0f, 0.0f, 0.0f}, .radius = 20.0f, .height = 30.0f, .kind = Exclusion::Kind::Dome},
	    {.position = {0.0f, 0.0f, 100.0f}, .radius = 20.0f, .height = 0.0f, .kind = Exclusion::Kind::Cylinder},
	};
	// strictly within twice the margin of a dome's edge, and less than its height and twice the margin above its base
	EXPECT_EQ(NearbyExclusions(exclusions, {74.5f, 35.0f, 0.0f}).size(), 1u);
	EXPECT_EQ(NearbyExclusions(exclusions, {74.0f, 35.0f, 0.0f}).size(), 0u);
	EXPECT_EQ(NearbyExclusions(exclusions, {100.0f, 36.0f, 0.0f}).size(), 0u);
	// a cylinder at any height
	EXPECT_EQ(NearbyExclusions(exclusions, {0.0f, 9000.0f, 75.0f}).size(), 1u);
}

TEST(CameraLandProbe, ACylinderPushesTheCameraStraightOutToTheMargin)
{
	const std::vector<Exclusion> nearby {
	    {.position = {0.0f, 0.0f, 0.0f}, .radius = 20.0f, .height = 0.0f, .kind = Exclusion::Kind::Cylinder},
	    {.position = {500.0f, 0.0f, 0.0f}, .radius = 20.0f, .height = 20.0f, .kind = Exclusion::Kind::Dome},
	};
	const auto pushed = PushOutOfCylinders(nearby, {10.0f, 70.0f, 0.0f});
	EXPECT_EQ(pushed.pushes, 1u);
	EXPECT_EQ(pushed.origin, glm::vec3(23.0f, 70.0f, 0.0f));
	// on its axis, the distance is taken as 0.01, so the camera stays where the axis is
	EXPECT_EQ(PushOutOfCylinders(nearby, {0.0f, 70.0f, 0.0f}).origin, glm::vec3(0.0f, 70.0f, 0.0f));
	// clear of it, or in a dome, it is not pushed
	EXPECT_EQ(PushOutOfCylinders(nearby, {23.5f, 70.0f, 0.0f}).pushes, 0u);
	EXPECT_EQ(PushOutOfCylinders(nearby, {500.0f, 10.0f, 0.0f}).pushes, 0u);
}

TEST(CameraLandProbe, ADomesRoofIsGroundToTheProbeWhileTheExclusionsAreOn)
{
	const std::vector<Exclusion> nearby {
	    {.position = {0.0f, 10.0f, 0.0f}, .radius = 50.0f, .height = 40.0f, .kind = Exclusion::Kind::Dome},
	};
	const auto land = FlatLand(12.0f);
	// half an ellipsoid: 40 over its base at its middle, its base at its edge
	EXPECT_EQ(SurfaceHeight(land, nearby, true, {0.0f, 0.0f}), 50.0f);
	EXPECT_EQ(SurfaceHeight(land, nearby, true, {30.0f, 0.0f}), std::sqrt(1.0f - 900.0f / 2500.0f) * 40.0f + 10.0f);
	// where the land is higher the land, outside the dome the land, and with the exclusions off the land
	EXPECT_EQ(SurfaceHeight(land, nearby, true, {49.99f, 0.0f}), 12.0f);
	EXPECT_EQ(SurfaceHeight(land, nearby, true, {50.0f, 0.0f}), 12.0f);
	EXPECT_EQ(SurfaceHeight(land, nearby, false, {0.0f, 0.0f}), 12.0f);
}

TEST(CameraLandProbe, EachPointBelowTheLandPushesTheCameraUpByAtMostHalfTheMargin)
{
	const auto land = FlatLand(0.0f);
	// well clear of the land, nothing moves
	{
		const auto push = PushOffTheGround(land, {}, true, {0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 50.0f});
		EXPECT_FALSE(push.pushed);
		EXPECT_EQ(push.points, 0u);
		EXPECT_EQ(push.origin, glm::vec3(0.0f, 10.0f, 0.0f));
	}
	// looking level 2 over the land: only the point straight below is in it, 1 deep, and the camera comes up 1; the push
	// has no part away from what it looks at, which stays
	{
		const auto push = PushOffTheGround(land, {}, true, {0.0f, 2.0f, 0.0f}, {0.0f, 2.0f, 50.0f});
		EXPECT_TRUE(push.pushed);
		EXPECT_EQ(push.points, 1u);
		EXPECT_EQ(push.origin.y, 3.0f);
		EXPECT_FALSE(push.focusMoved);
		EXPECT_EQ(push.focus, glm::vec3(0.0f, 2.0f, 50.0f));
	}
	// deep in the land, each point pushes 1.5 at most: the bottom one, the four below the middle and the six round it
	// take it from 20 under to 3.5 under, and the first of the four above it, 2.12 above that, brings it out only to
	// 3 sqrt(2) / 2 under, which leaves the rest of the ball on or above the land
	{
		const auto push = PushOffTheGround(land, {}, true, {0.0f, -20.0f, 0.0f}, {0.0f, -20.0f, 50.0f});
		EXPECT_EQ(push.points, 12u);
		EXPECT_NEAR(push.origin.y, -2.1213203f, 1e-5f);
	}
}

TEST(CameraLandProbe, APushAwayFromWhatTheCameraLooksAtMovesThatToo)
{
	// looking straight down at the land 1 below: the pushes up are away from what it looks at
	const auto land = FlatLand(0.0f);
	const auto push = PushOffTheGround(land, {}, true, {0.0f, 1.0f, 0.0f}, {0.0f, -100.0f, 0.001f});
	EXPECT_TRUE(push.pushed);
	EXPECT_TRUE(push.focusMoved);
	EXPECT_GT(push.focus.y, -100.0f);
}

TEST(CameraLandProbe, ADomesRoofPushesTheCameraOutFromItsMiddle)
{
	const std::vector<Exclusion> nearby {
	    {.position = {0.0f, 0.0f, 0.0f}, .radius = 50.0f, .height = 50.0f, .kind = Exclusion::Kind::Dome},
	};
	const auto land = FlatLand(0.0f);
	// just under the roof, to the side of its middle: pushed outwards, away from the middle
	const glm::vec3 origin {30.0f, 39.0f, 0.0f};
	const auto push = PushOffTheGround(land, nearby, true, origin, origin + glm::vec3(0.0f, 0.0f, 50.0f));
	EXPECT_TRUE(push.pushed);
	EXPECT_GT(push.onDomes, 0u);
	EXPECT_EQ(push.onDomes, push.points);
	EXPECT_GT(glm::length(push.origin), glm::length(origin));
	EXPECT_GT(push.origin.x, origin.x);
	// with the exclusions off the probe feels only the land, and the dome does not push
	const auto off = PushOffTheGround(land, nearby, false, origin, origin + glm::vec3(0.0f, 0.0f, 50.0f));
	EXPECT_FALSE(off.pushed);
	// but a point the land pushes, under a dome, takes the dome's normal whatever the exclusions
	const std::vector<Exclusion> low {
	    {.position = {0.0f, -10.0f, 0.0f}, .radius = 50.0f, .height = 12.0f, .kind = Exclusion::Kind::Dome},
	};
	const auto under = PushOffTheGround(land, low, false, {0.0f, 2.0f, 0.0f}, {0.0f, 2.0f, 50.0f});
	EXPECT_EQ(under.points, 1u);
	EXPECT_EQ(under.onDomes, 1u);
}

// The easing after the frame's limits: twice for what the camera looks at moved, twice again for the camera pushed, so
// four times when both, as the fence and the slide under the highest do
TEST(CameraEase, TheEaseDoublesForEachOfTheTwoWaysTheLimitsMovedTheCamera)
{
	EXPECT_EQ(EaseMicroseconds(false, false), std::chrono::microseconds(300'000));
	EXPECT_EQ(EaseMicroseconds(true, false), std::chrono::microseconds(600'000));
	EXPECT_EQ(EaseMicroseconds(false, true), std::chrono::microseconds(600'000));
	EXPECT_EQ(EaseMicroseconds(true, true), std::chrono::microseconds(1'200'000));
}
