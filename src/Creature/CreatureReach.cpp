/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureReach.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

using namespace openblack;
using namespace openblack::creature_reach;

namespace
{
/// Spans narrower than this are taken as this, so nothing is divided by nothing
constexpr float k_TinySpan = 1e-4f;

float Ratio(float value, float from, float to)
{
	auto span = to - from;
	if (std::abs(span) < k_TinySpan)
	{
		span = span < 0.0f ? -k_TinySpan : k_TinySpan;
	}
	return (value - from) / span;
}

const glm::vec3& At(const Points& points, Corner corner)
{
	return points.at(static_cast<size_t>(corner));
}
} // namespace

Points creature_reach::Mirrored(const Points& points)
{
	auto mirrored = points;
	for (auto& point : mirrored)
	{
		point.x = -point.x;
	}
	return mirrored;
}

glm::vec3 creature_reach::Centre(const Points& points)
{
	glm::vec3 sum {0.0f};
	for (const auto& point : points)
	{
		sum += point;
	}
	return sum * 0.25f;
}

bool creature_reach::ReachesMirrored(const Points& points, const glm::vec3& target)
{
	const auto centre = Centre(points);
	const glm::vec2 own {centre.x, centre.z};
	const glm::vec2 reflected {-centre.x, centre.z};
	const glm::vec2 at {target.x, target.z};
	return glm::distance(at, reflected) < glm::distance(at, own);
}

Blend creature_reach::Solve(const Points& points, const glm::vec3& target)
{
	const auto& backLeft = At(points, Corner::BackLeft);
	const auto& backRight = At(points, Corner::BackRight);
	const auto& frontLeft = At(points, Corner::FrontLeft);
	const auto& frontRight = At(points, Corner::FrontRight);

	const auto s = Ratio(target.x, backLeft.x, backRight.x);
	const auto t = Ratio(target.x, frontLeft.x, frontRight.x);
	const auto backZ = backLeft.z + (s * (backRight.z - backLeft.z));
	const auto frontZ = frontLeft.z + (t * (frontRight.z - frontLeft.z));
	const auto d = Ratio(target.z, backZ, frontZ);

	const auto within = [](float value, float limit) { return value > -limit && value < 1.0f + limit; };
	return {
	    .acrossBack = s,
	    .acrossFront = t,
	    .backToFront = d,
	    .inRange = within(s, k_SideLimit) && within(t, k_SideLimit) && within(d, k_DepthLimit),
	    .weights = {(1.0f - s) * (1.0f - d), s * (1.0f - d), (1.0f - t) * d, t * d},
	};
}

namespace
{
/// How much of each corner the furthest point takes: the back pair pulled back by 0.8 of the depth shared between them,
/// the front pair stretched by it. The back's weight is a hair short of 0.4, as the game's
constexpr std::array<float, k_CornerCount> k_FurthestWeights {-0.39999998f, -0.39999998f, 0.9f, 0.9f};
static_assert(std::bit_cast<uint32_t>(k_FurthestWeights.at(0)) == 0xBECCCCCCu);

/// The front pair stretched to the furthest depth the blend allows, at a scale: each corner's weight scaled first, and
/// the corners added in their order
glm::vec3 Furthest(const Points& points, float scale)
{
	glm::vec3 furthest {0.0f};
	for (size_t i = 0; i < points.size(); ++i)
	{
		furthest += points.at(i) * (scale * k_FurthestWeights.at(i));
	}
	return furthest;
}
} // namespace

float creature_reach::MaxReach(const Points& points)
{
	const auto furthest = Furthest(points, 1.0f);
	return glm::length(glm::vec2(furthest.x, furthest.z));
}

float creature_reach::AverageReach(const Points& points, float scale)
{
	float reach = 0.0f;
	for (const auto& point : points)
	{
		reach -= scale * point.z;
	}
	return reach * 0.25f;
}

float creature_reach::MaxReach3D(const Points& points, float scale)
{
	return glm::length(Furthest(points, scale));
}
