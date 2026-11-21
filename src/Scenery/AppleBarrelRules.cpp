/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AppleBarrelRules.h"

#include <algorithm>

#include "Common/GameAngle.h"

using namespace openblack;

map_coords::MapCoords apple_barrel::MeasuredFrom(const std::optional<map_coords::MapCoords>& temple)
{
	return temple.value_or(k_NoTemplePlace);
}

std::optional<size_t> apple_barrel::NearestStore(std::span<const float> distances)
{
	std::optional<size_t> nearest;
	float nearestDistance = k_FarthestStore;
	for (size_t i = 0; i < distances.size(); ++i)
	{
		if (distances[i] < nearestDistance)
		{
			nearestDistance = distances[i];
			nearest = i;
		}
	}
	return nearest;
}

map_coords::MapCoords apple_barrel::BarrelPlace(const map_coords::MapCoords& store, float yAngle, float radius)
{
	auto place = store + game_math::GetPosFromAngle(yAngle, radius);
	place.altitude = 0.0f;
	return place;
}

map_coords::MapCoords apple_barrel::GroundCoords(const map_coords::MapCoords& place)
{
	return {.x = map_coords::MetresToFixedForHandLookup(map_coords::ToMetres(place.x)),
	        .z = map_coords::MetresToFixedForHandLookup(map_coords::ToMetres(place.z)),
	        .altitude = 0.0f};
}

glm::vec3 apple_barrel::WormSpot(const glm::vec3& barrel, const glm::vec3& modelPoint)
{
	return barrel + modelPoint;
}

uint32_t apple_barrel::StepWorm(uint32_t place, uint32_t elapsed, uint32_t playTime, bool looping)
{
	if (playTime == 0)
	{
		return 0;
	}
	const auto next = static_cast<uint64_t>(place) + elapsed;
	if (looping)
	{
		return static_cast<uint32_t>(next % playTime);
	}
	return static_cast<uint32_t>(std::min<uint64_t>(next, playTime - 1));
}
