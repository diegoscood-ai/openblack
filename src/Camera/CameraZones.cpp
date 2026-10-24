/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraZones.h"

#include <cmath>

#include <optional>
#include <utility>

using namespace openblack;
using namespace openblack::camera_zones;

namespace
{
/// A point this close to a corner, squared, is on the fence; a side shorter than this, squared, is skipped
constexpr float k_OnCorner = 1e-8f;
/// A line this near parallel to a side doesn't cross it, and a crossing this near the point is the point
constexpr float k_Parallel = 1e-4f;
/// The camera has hardly moved this frame when it is this near, squared, across the ground to where it started
constexpr float k_HardlyMoved = 1e-4f;
/// The search round the crossing turns by a 64th of a turn, up to 31 of them either way
constexpr float k_SearchStep = 0.0981747732f;
constexpr int k_SearchSteps = 32;
/// The original maps' cells a side, and their disc's middle and radius
constexpr float k_OriginalCellsPerSide = 512.0f;
constexpr float k_OriginalDiscMiddle = 2560.0f;
constexpr float k_OriginalDiscRadius = 5120.0f;
/// The height of the disc's middle, which the camera's heights are measured from and put back to as well
constexpr float k_DiscMiddleHeight = 0.0f;

/// Zero, or not a number, as a compare that is equal or unordered reads it
bool ZeroOrNotANumber(float value)
{
	return value == 0.0f || std::isnan(value);
}

float DistanceSquaredAcross(const glm::vec3& a, const glm::vec3& b)
{
	const float x = a.x - b.x;
	const float z = a.z - b.z;
	return x * x + z * z;
}
} // namespace

Crossing camera_zones::CrossFence(std::span<const glm::vec3> fence, bool fenceOn, const glm::vec3& point,
                                  const glm::vec3& direction)
{
	Crossing crossing {.inside = true, .closest = point};
	if (!fenceOn || fence.size() <= 2)
	{
		return crossing;
	}
	uint32_t ahead = 0;
	float nearestAhead = 1e20f;
	float nearestBehind = 1e20f;
	std::optional<std::pair<glm::vec3, glm::vec3>> hitAhead;
	std::optional<std::pair<glm::vec3, glm::vec3>> hitBehind;
	const auto* previous = &fence.back();
	for (const auto& corner : fence)
	{
		const float sideX = previous->x - corner.x;
		const float sideZ = previous->z - corner.z;
		const float sideSquared = sideZ * sideZ + sideX * sideX;
		if (DistanceSquaredAcross(point, corner) < k_OnCorner)
		{
			return crossing;
		}
		if (k_OnCorner < sideSquared)
		{
			const float across = -sideX;
			const float facing = direction.x * sideZ + direction.z * across;
			if (k_Parallel < std::abs(facing))
			{
				// How far along the line it meets the side, and how far along the side from this corner
				const float along = (((corner.z * across + corner.x * sideZ) - point.x * sideZ) - point.z * across) / facing;
				const glm::vec3 hit {along * direction.x + point.x, direction.y * along + point.y,
				                     along * direction.z + point.z};
				const float onSide = ((hit.z - corner.z) * sideZ + (hit.x - corner.x) * sideX) / sideSquared;
				if (0.0f <= onSide && onSide < 1.0f)
				{
					if (std::abs(along) < k_Parallel)
					{
						return crossing;
					}
					const glm::vec3 normal {sideZ, 0.0f, across};
					if (along > 0.0f)
					{
						++ahead;
						if (along < nearestAhead)
						{
							nearestAhead = along;
							hitAhead = {hit, normal};
						}
					}
					else if (-along < nearestBehind)
					{
						nearestBehind = -along;
						hitBehind = {hit, normal};
					}
				}
			}
		}
		previous = &corner;
	}
	bool found = false;
	if (const auto& hit = hitAhead.has_value() ? hitAhead : hitBehind; hit.has_value())
	{
		crossing.closest = hit->first;
		crossing.normal = hit->second;
		found = true;
	}
	// A corner nearer than the crossing is nearer still
	float nearest = DistanceSquaredAcross(crossing.closest, point);
	for (const auto& corner : fence)
	{
		const float distance = DistanceSquaredAcross(corner, point);
		if (distance < nearest || !found)
		{
			crossing.closest = corner;
			found = true;
			nearest = distance;
		}
	}
	crossing.inside = (ahead & 1u) != 0u;
	return crossing;
}

camera_zones::FenceKeep camera_zones::PushInsideFence(std::span<const glm::vec3> fence, bool fenceOn,
                                                      const glm::vec3& originAtStart, const glm::vec3& origin,
                                                      const glm::vec3& focus, float searchRadius)
{
	if (!fenceOn)
	{
		return {.inclusionDistance = k_NoFenceDistance};
	}
	// Hardly moved, or a distance that is not a number: the line goes towards what it looks at
	const auto towards = !(DistanceSquaredAcross(originAtStart, origin) >= k_HardlyMoved) ? focus : originAtStart;
	const auto direction = towards - origin;
	const auto crossing = CrossFence(fence, fenceOn, origin, direction);
	if (crossing.inside)
	{
		const float dx = crossing.closest.x - origin.x;
		const float dz = crossing.closest.z - origin.z;
		return {.inclusionDistance = std::sqrt(dz * dz + dx * dx)};
	}
	// The way across the ground from the camera to the crossing, of length 1 unless every part of it is 0 (or not a
	// number)
	glm::vec3 way {crossing.closest.x - origin.x, 0.0f, crossing.closest.z - origin.z};
	if (!(ZeroOrNotANumber(way.x) && ZeroOrNotANumber(way.z)))
	{
		way *= 1.0f / std::sqrt((way.z * way.z + way.y * way.y) + way.x * way.x);
	}
	auto target = crossing.closest;
	for (int step = 0; step < k_SearchSteps; ++step)
	{
		for (const int turn : {-step, step})
		{
			// The angle is a single precision value; its sine and cosine are taken at full precision and rounded once as
			// the radius scales them
			const float angle = static_cast<float>(turn) * k_SearchStep;
			const auto sine = static_cast<float>(std::sin(static_cast<double>(angle)) * static_cast<double>(searchRadius));
			const auto cosine = static_cast<float>(std::cos(static_cast<double>(angle)) * static_cast<double>(searchRadius));
			const glm::vec3 candidate =
			    crossing.closest + glm::vec3(sine * -way.z + cosine * way.x, 0.0f, sine * way.x + cosine * way.z);
			if (CrossFence(fence, fenceOn, candidate, direction).inside)
			{
				return {.target = candidate, .inclusionDistance = 0.0f};
			}
		}
	}
	return {.target = target, .inclusionDistance = 0.0f};
}

MapDisc camera_zones::MapDiscFor(uint16_t cellsPerSide)
{
	const float scale = static_cast<float>(cellsPerSide) / k_OriginalCellsPerSide;
	return {.centre = glm::vec2(k_OriginalDiscMiddle * scale), .radius = k_OriginalDiscRadius * scale};
}

float camera_zones::AcrossFromMiddle(const MapDisc& disc, const glm::vec3& point)
{
	const float dx = point.x - disc.centre.x;
	const float dz = point.z - disc.centre.y;
	return std::sqrt(dz * dz + dx * dx);
}

MapDiscKeep camera_zones::KeepInsideMapDisc(const MapDisc& disc, const glm::vec3& origin, const glm::vec3& focus)
{
	// Both points are taken from the middle and put back afterwards, each step rounded to single precision
	const auto fromMiddle = [&disc](const glm::vec3& point) {
		return glm::vec3(point.x - disc.centre.x, point.y - k_DiscMiddleHeight, point.z - disc.centre.y);
	};
	const auto backFromMiddle = [&disc](const glm::vec3& point) {
		return glm::vec3(disc.centre.x + point.x, k_DiscMiddleHeight + point.y, disc.centre.y + point.z);
	};
	auto originFromMiddle = fromMiddle(origin);
	const auto focusFromMiddle = fromMiddle(focus);
	// Measured across the ground, z first; only x and z are brought in (equal or not a number stays)
	const float distance2 = originFromMiddle.z * originFromMiddle.z + originFromMiddle.x * originFromMiddle.x;
	const bool beyond = distance2 > disc.radius * disc.radius;
	if (beyond)
	{
		const float shrink = disc.radius / std::sqrt(distance2);
		originFromMiddle.x = shrink * originFromMiddle.x;
		originFromMiddle.z = shrink * originFromMiddle.z;
	}
	return {.origin = backFromMiddle(originFromMiddle), .focus = backFromMiddle(focusFromMiddle), .broughtBack = beyond};
}
