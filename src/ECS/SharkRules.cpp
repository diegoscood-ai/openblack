/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SharkRules.h"

#include <glm/vec3.hpp>

#include "3D/ObjectMatrix.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
constexpr float k_WakeGrowth = 10.0f;
/// The ring is half as deep as it is wide
constexpr float k_WakeAspect = 0.5f;
constexpr float k_WakeRate = 0.5f;
constexpr uint8_t k_WakeCell = 0x31;
constexpr uint32_t k_WakeColour = 0x90FFFFFFu;
} // namespace

float shark_rules::Heading(const glm::vec3& turnStart, const glm::vec3& position, float facing)
{
	if (position.x == turnStart.x && position.z == turnStart.z)
	{
		return facing;
	}
	// The angle of the move across the land, as the game works it out, stored as a float
	return static_cast<float>(affine::GetYAngleOfXZ(glm::vec3(position.x - turnStart.x, 0.0f, position.z - turnStart.z)));
}

glm::vec3 shark_rules::Drawn(const glm::vec3& turnStart, float startHeight, const glm::vec3& position, float endHeight,
                             float turnFraction)
{
	const float before = 1.0f - turnFraction;
	return {before * turnStart.x + turnFraction * position.x, before * startHeight + turnFraction * endHeight,
	        before * turnStart.z + turnFraction * position.z};
}

std::optional<water_rings::Ring> shark_rules::WakeRing(int32_t& timer, const glm::vec3& point, float heading,
                                                       int32_t frameMilliseconds)
{
	if (timer <= k_WakeInterval)
	{
		timer += frameMilliseconds;
		return std::nullopt;
	}
	timer = (timer % k_WakeInterval) + frameMilliseconds;
	return water_rings::Ring {.position = glm::vec3(point.x, 0.0f, point.z),
	                          .age = 0,
	                          .growth = k_WakeGrowth,
	                          .angle = heading,
	                          .aspect = k_WakeAspect,
	                          .rate = k_WakeRate,
	                          .cell = k_WakeCell,
	                          .argb = k_WakeColour};
}
