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

TEST(CameraZoneSystem, AFilesZonesAreKeptToWhateverItSays)
{
	player_camera::Zone file;
	file.flag9CE6B0 = 0;
	file.drawForceField = 0;
	file.flagC5E14C = 1;
	file.limit9CE6AC = 62.5f;
	file.limit9CE6A8 = 80.0f;
	file.forceFieldPoints = k_Square;
	// a cylinder at (10, 20, 30), 7 across and 4 high, as the game keeps one: two links and an id first
	player_camera::Exclusion exclusion;
	const std::array<float, 5> numbers {10.0f, 20.0f, 30.0f, 7.0f, 4.0f};
	std::memcpy(exclusion.bytes.data() + 12, numbers.data(), sizeof(numbers));
	const uint32_t cylinder = 1;
	std::memcpy(exclusion.bytes.data() + 32, &cylinder, sizeof(cylinder));
	file.exclusions.push_back(exclusion);
	const auto zones = ecs::systems::CameraZoneSystem::FromFile(file);
	EXPECT_TRUE(zones.exclusionsOn);
	EXPECT_TRUE(zones.fenceOn);
	EXPECT_TRUE(zones.useMaxAltitude);
	EXPECT_FALSE(zones.useHeightAboveLand);
	EXPECT_EQ(zones.maxAltitude, 62.5f);
	EXPECT_EQ(zones.heightAboveLand, 80.0f);
	EXPECT_EQ(zones.fence, k_Square);
	ASSERT_EQ(zones.exclusions.size(), 1u);
	EXPECT_EQ(zones.exclusions[0].position, glm::vec3(10.0f, 20.0f, 30.0f));
	EXPECT_EQ(zones.exclusions[0].radius, 7.0f);
	EXPECT_EQ(zones.exclusions[0].height, 4.0f);
	EXPECT_EQ(zones.exclusions[0].kind, Exclusion::Kind::Cylinder);
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
