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
