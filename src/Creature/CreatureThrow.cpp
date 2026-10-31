/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureThrow.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsX87.h"

using namespace openblack;
using namespace openblack::creature_throw;

namespace
{
constexpr float k_Tiny = 1e-4f;
} // namespace

float creature_throw::GroundDistance(const glm::vec3& from, const glm::vec3& to)
{
	const float dx = from.x - to.x;
	const float dz = from.z - to.z;
	return std::sqrt((dz * dz) + (dx * dx));
}

float creature_throw::ThrowFlightTime(float groundDistance)
{
	return std::sqrt(groundDistance * k_FlightSecondsSquaredPerDistance);
}

glm::vec3 creature_throw::ThrowVelocity(const glm::vec3& target, const glm::vec3& release, float seconds)
{
	// The fall is worked out as half the time squared, then times gravity; the way there is divided by the time as a
	// multiplication by its reciprocal
	const float half = (seconds * seconds) * 0.5f;
	const float fall = half * -k_Gravity;
	const float reciprocal = 1.0f / seconds;
	const glm::vec3 way {target.x - release.x, (target.y - release.y) - fall, target.z - release.z};
	return {way.x * reciprocal, way.y * reciprocal, way.z * reciprocal};
}

creature_throw::ThrowAim creature_throw::AimAhead(const glm::vec3& creature, const glm::vec3& thing,
                                                  const std::function<glm::vec3(float)>& whereAfter)
{
	const float first = ThrowFlightTime(GroundDistance(creature, thing));
	const auto ahead = whereAfter(first);
	const float seconds = ThrowFlightTime(GroundDistance(creature, ahead));
	return {.point = whereAfter(seconds), .seconds = seconds};
}

glm::vec3 creature_throw::MiddleOf(const glm::vec3& place, float ground, float height)
{
	return {place.x, ground + (height * 0.5f), place.z};
}

creature_throw::ThrowAim creature_throw::AimAtStillThing(const glm::vec3& creature, const glm::vec3& place, float ground,
                                                         float height)
{
	const auto middle = MiddleOf(place, ground, height);
	return AimAhead(creature, place, [&middle](float) { return middle; });
}

glm::vec3 creature_throw::VillagerAfter(const glm::vec3& middle, glm::ivec2 step, bool topStateFinal, uint32_t turnsPerSecond,
                                        float seconds)
{
	if (topStateFinal)
	{
		return middle;
	}
	// The whole turns: the turns a second times the time, rounded to a float, then truncated
	const float turnsInTime = static_cast<float>(turnsPerSecond) * seconds;
	const auto turns = static_cast<float>(map_coords::FtoL(turnsInTime));
	// The step in metres: whole map units times the metres of a cell, then over a cell's map units
	const float cellsX = static_cast<float>(step.x) * map_coords::k_CellSize;
	const float cellsZ = static_cast<float>(step.y) * map_coords::k_CellSize;
	const float stepX = cellsX * (1.0f / static_cast<float>(map_coords::k_FixedPerCell));
	const float stepZ = cellsZ * (1.0f / static_cast<float>(map_coords::k_FixedPerCell));
	const float leadX = turns * stepX;
	const float leadZ = turns * stepZ;
	return {leadX + middle.x, middle.y, leadZ + middle.z};
}

glm::vec3 creature_throw::CreatureAfter(const glm::vec3& body, float heading, float speed, float height, float seconds,
                                        const std::function<float(glm::vec2)>& groundAt)
{
	namespace x87 = openblack::gutils::x87;
	// The game turns by the opposite angle: its yaw faces (sin, -cos), the heading here (-sin, -cos)
	const double distance = x87::Mul(static_cast<double>(speed), static_cast<double>(seconds));
	const auto [sine, cosine] = x87::SinCos(-static_cast<double>(heading));
	const float aheadX = x87::Store(x87::Mul(sine, distance));
	const double aheadZ = -x87::Mul(cosine, distance);
	const float x = x87::Store(x87::Add(static_cast<double>(aheadX), static_cast<double>(body.x)));
	const float z = x87::Store(x87::Add(aheadZ, static_cast<double>(body.z)));
	const glm::vec2 truncated {map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(x)),
	                           map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(z))};
	const float y = x87::Store(x87::Add(static_cast<double>(groundAt(truncated)), static_cast<double>(height)));
	return {x, y, z};
}

bool creature_throw::FarEnoughToThrow(float groundDistance, float size)
{
	return groundDistance >= size * k_HeightAtSizeOne * k_MinThrowHeightShare;
}

float creature_throw::HighThrowWeight(float targetSlope, float flatSlope, float highSlope)
{
	const auto span = highSlope - flatSlope;
	if (std::abs(span) < k_Tiny)
	{
		return 0.0f;
	}
	return std::clamp((targetSlope - flatSlope) / span, 0.0f, 1.0f);
}

glm::vec3 creature_throw::HandVelocity(const glm::vec3& atRelease, const glm::vec3& before, float spanMs)
{
	return spanMs > 0.0f ? (atRelease - before) * (1000.0f / spanMs) : glm::vec3(0.0f);
}

glm::vec3 creature_throw::TossVelocity(const glm::vec3& handVelocity, bool mirrored, const glm::mat3& rotation, float share)
{
	auto local = handVelocity * share;
	if (mirrored)
	{
		local.x = -local.x;
	}
	return rotation * local;
}

std::optional<creature_throw::ThrowStand> creature_throw::WhereToThrowFrom(glm::vec2 creature, glm::vec2 target, float height,
                                                                           float targetRadius, float keep)
{
	const float distance = glm::distance(creature, target);
	const float nearest = keep + targetRadius + k_ThrowStandHeights * height;
	const float furthest = k_ThrowReachHeights * height;
	if (distance < nearest)
	{
		if (distance <= 0.0f)
		{
			return std::nullopt;
		}
		// Back along the line from the thing to where it stands, to the nearest it may stand
		return ThrowStand {.kind = ThrowStand::Kind::BackOff,
		                   .point = target - (target - creature) * (nearest / distance),
		                   .minDistance = 0.0f,
		                   .maxDistance = k_ThrowBackOffHeights * height};
	}
	if (distance > furthest)
	{
		return ThrowStand {.kind = ThrowStand::Kind::WalkUp,
		                   .point = target,
		                   .minDistance = nearest,
		                   .maxDistance = furthest > nearest ? furthest : nearest + k_ThrowWalkUpSpare};
	}
	return ThrowStand {};
}
