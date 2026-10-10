/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// hand_pick::MissesFromAbove, the hand pick's cheapest reject: every sphere it rejects is one the ray's line misses in
// space by more than the rounding of the pick's own sphere test, so the pick finds the same object with or without it.

#include <cmath>
#include <cstdint>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/HandPickReject.h"

using openblack::ecs::hand_pick::MissesFromAbove;

namespace
{
/// A fixed pseudo-random sequence in [0, 1)
class Sequence
{
public:
	float Next()
	{
		_state = _state * 1664525u + 1013904223u;
		return static_cast<float>(_state >> 8) / 16777216.0f;
	}

private:
	uint32_t _state {12345u};
};

/// The distance in space from the centre to the ray's line, in double precision
double LineDistance(const glm::vec3& origin, const glm::vec3& dir, const glm::vec3& centre)
{
	const glm::dvec3 o(origin);
	const glm::dvec3 d = glm::normalize(glm::dvec3(dir));
	const glm::dvec3 v = glm::dvec3(centre) - o;
	return glm::length(v - d * glm::dot(v, d));
}
} // namespace

TEST(HandPickReject, ARejectedSphereIsMissedInSpace)
{
	Sequence random;
	int rejected = 0;
	int kept = 0;
	for (int i = 0; i < 200000; ++i)
	{
		// a camera up to 1.5 km above and around a 5 km island, looking down at any angle
		const glm::vec3 origin(random.Next() * 5000.0f, 5.0f + random.Next() * 1500.0f, random.Next() * 5000.0f);
		glm::vec3 dir(random.Next() * 2.0f - 1.0f, -random.Next(), random.Next() * 2.0f - 1.0f);
		if (i % 50 == 0)
		{
			dir = glm::vec3(0.0f, -1.0f, 0.0f); // straight down
		}
		dir = glm::normalize(dir);
		const float radius = 0.2f + random.Next() * 40.0f;
		// a centre near the ray half of the time
		glm::vec3 centre(random.Next() * 5000.0f, random.Next() * 200.0f, random.Next() * 5000.0f);
		if (i % 2 == 0)
		{
			const float t = random.Next() * 2000.0f;
			centre =
			    origin + dir * t + glm::vec3(random.Next() - 0.5f, random.Next() - 0.5f, random.Next() - 0.5f) * 4.0f * radius;
		}
		if (MissesFromAbove(glm::vec2(origin.x, origin.z), glm::vec2(dir.x, dir.z), glm::vec2(centre.x, centre.z), radius))
		{
			++rejected;
			ASSERT_GT(LineDistance(origin, dir, centre), static_cast<double>(radius) * 1.05 + 0.5) << i;
			// the pick's own float sphere test agrees
			const glm::vec3 oc = origin - centre;
			const float b = glm::dot(oc, dir);
			const float c = glm::dot(oc, oc) - radius * radius;
			ASSERT_TRUE((c > 0.0f && b > 0.0f) || b * b - c < 0.0f) << i;
		}
		else
		{
			++kept;
		}
	}
	// both outcomes are exercised
	EXPECT_GT(rejected, 1000);
	EXPECT_GT(kept, 1000);
}

TEST(HandPickReject, NearTheLineIsKept)
{
	const glm::vec2 origin(100.0f, 100.0f);
	const glm::vec2 dir(1.0f, 0.0f);
	EXPECT_FALSE(MissesFromAbove(origin, dir, glm::vec2(300.0f, 102.0f), 2.0f));             // on the edge
	EXPECT_FALSE(MissesFromAbove(origin, dir, glm::vec2(300.0f, 103.1f), 2.0f));             // within the margin
	EXPECT_TRUE(MissesFromAbove(origin, dir, glm::vec2(300.0f, 103.3f), 2.0f));              // past 1.1 r + 1 m
	EXPECT_FALSE(MissesFromAbove(origin, glm::vec2(0.0f), glm::vec2(101.0f, 100.0f), 1.0f)); // straight down
	EXPECT_TRUE(MissesFromAbove(origin, glm::vec2(0.0f), glm::vec2(110.0f, 100.0f), 1.0f));
}

TEST(HandPickReject, AnInvisibleSphereIsHitInsideItsScreenCircle)
{
	using openblack::ecs::hand_pick::InvisibleSphereAlong;
	const glm::vec3 origin(0.0f, 10.0f, 0.0f);
	const glm::vec3 forward(0.0f, 0.0f, 1.0f);
	const float nearClip = 0.3f;
	// straight ahead: hit at the centre's view depth
	const auto ahead = InvisibleSphereAlong(origin, forward, forward, nearClip, glm::vec3(0.0f, 10.0f, 20.0f), 1.0f);
	ASSERT_TRUE(ahead.has_value());
	EXPECT_EQ(*ahead, 20.0f);
	// a ray at an angle: its length to the plane at that depth, depth / (ray . forward)
	const glm::vec3 slanted = glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f) + glm::vec3(0.02f, 0.0f, 0.0f));
	const auto aslant = InvisibleSphereAlong(origin, slanted, forward, nearClip, glm::vec3(0.0f, 10.0f, 20.0f), 1.0f);
	ASSERT_TRUE(aslant.has_value());
	EXPECT_EQ(*aslant, 20.0f / glm::dot(slanted, forward));
	// on that plane the ray passes 0.4 m off this centre: hit with a radius above 0.4, missed with one at or below it
	const glm::vec3 off(0.4f, 10.0f, 20.0f);
	const float pass = glm::distance(origin + slanted * (20.0f / glm::dot(slanted, forward)), off);
	EXPECT_TRUE(InvisibleSphereAlong(origin, slanted, forward, nearClip, off, pass + 0.01f).has_value());
	EXPECT_FALSE(InvisibleSphereAlong(origin, slanted, forward, nearClip, off, pass).has_value());
	// a centre short of the near clip, and a ray that does not go forward, are missed
	EXPECT_FALSE(InvisibleSphereAlong(origin, forward, forward, nearClip, glm::vec3(0.0f, 10.0f, 0.2f), 1.0f).has_value());
	EXPECT_FALSE(
	    InvisibleSphereAlong(origin, glm::vec3(1.0f, 0.0f, 0.0f), forward, nearClip, glm::vec3(0.0f, 10.0f, 20.0f), 50.0f)
	        .has_value());
}
