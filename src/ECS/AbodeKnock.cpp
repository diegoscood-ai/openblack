/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AbodeKnock.h"

#include <algorithm>

namespace abode_knock = openblack::ecs::abode_knock;

namespace
{
/// The steps go up or down by this many for every millisecond of growing or shrinking
constexpr float k_StepsPerMs = 0.255f;
/// The share of a step
constexpr float k_StepShare = 1.0f / 255.0f;
} // namespace

int32_t abode_knock::Knock(int32_t remaining)
{
	if (remaining == 0)
	{
		return k_ReadoutMs;
	}
	if (remaining > k_ReadoutMs - k_ReadoutGrowMs)
	{
		// Still growing in
		return remaining;
	}
	if (remaining < k_ReadoutShrinkMs)
	{
		// Shrinking away: grow back from the size it has shrunk to
		return k_ReadoutMs - remaining;
	}
	return k_ReadoutMs - k_ReadoutGrowMs;
}

int32_t abode_knock::Tick(int32_t remaining, uint32_t frameMs)
{
	// the time is taken off as a whole number and the result goes no lower than 0
	const auto left = static_cast<int32_t>(static_cast<uint32_t>(remaining) - frameMs);
	return left < 0 ? 0 : left;
}

int32_t abode_knock::ReadoutSteps(int32_t remaining)
{
	if (remaining > k_ReadoutMs - k_ReadoutGrowMs)
	{
		// the product and the difference are floats, the fraction cut off
		const float grown = static_cast<float>(remaining - (k_ReadoutMs - k_ReadoutGrowMs)) * k_StepsPerMs;
		return static_cast<int32_t>(static_cast<float>(k_ReadoutSteps) - grown);
	}
	if (remaining <= k_ReadoutShrinkMs)
	{
		return static_cast<int32_t>(static_cast<float>(remaining) * k_StepsPerMs);
	}
	return k_ReadoutSteps;
}

float abode_knock::StepsScale(int32_t steps)
{
	return static_cast<float>(steps) * k_StepShare;
}

float abode_knock::ReadoutScale(int32_t remaining)
{
	return StepsScale(ReadoutSteps(remaining));
}

std::vector<abode_knock::Marker> abode_knock::Readout(int32_t places, int32_t adults, float scale)
{
	std::vector<Marker> markers;
	if (places <= 0)
	{
		return markers;
	}
	markers.reserve(static_cast<size_t>(places));
	for (int32_t i = 0; i < places; ++i)
	{
		Marker marker {.along = static_cast<float>(i) / k_MarkerDivisor};
		if (adults == k_HeldVillagersHome)
		{
			// the held villager's home: full size, the scale not applied
			marker.colour = k_HeldVillagersHomeColour;
			marker.size = 1.0f / k_MarkerDivisor;
		}
		else
		{
			marker.colour = i < adults ? k_LivedInColour : k_FreePlaceColour;
			marker.size = scale / k_MarkerDivisor;
		}
		marker.size = std::max(marker.size, k_SmallestMarker);
		markers.push_back(marker);
	}
	return markers;
}

glm::vec3 abode_knock::RowPoint(const glm::vec3& position, float halfHeight)
{
	// twice the half height, then the position, then the rise, each sum a float
	return {position.x, ((halfHeight + halfHeight) + position.y) + k_RaisedAbove, position.z};
}

glm::vec3 abode_knock::RowStart(const glm::vec3& point, const glm::vec3& right, int32_t places)
{
	const float half = static_cast<float>(-places) * 0.5f;
	return point + right * half;
}

glm::vec3 abode_knock::MarkerPoint(const glm::vec3& start, const glm::vec3& right, float along)
{
	return start + right * along;
}
