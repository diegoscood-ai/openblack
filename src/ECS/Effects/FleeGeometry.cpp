/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FleeGeometry.h"

#include <cmath>

using namespace openblack::ecs::effects;

glm::vec2 flee::FleeingPositionFromStill(glm::vec2 me, glm::vec2 object, float distance)
{
	const glm::vec2 away = me - object;
	if (away.x == 0.0f && away.y == 0.0f)
	{
		return me;
	}
	// scaled to the distance by the length in the plane
	const float scale = distance / std::sqrt(away.y * away.y + away.x * away.x);
	return {me.x + away.x * scale, me.y + away.y * scale};
}

glm::vec2 flee::FleeingPositionFromMoving(glm::vec2 me, glm::vec2 object, glm::vec2 movement, float distance, float randomX,
                                          float randomZ)
{
	// across the way: the way turned a quarter, made one long in the plane
	glm::vec2 across(-movement.y, movement.x);
	if (across.x != 0.0f || across.y != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt(across.x * across.x + across.y * across.y);
		across.x *= inverse;
		across.y *= inverse;
	}
	// the random draw plus the run across the way, less half the jitter: x first, then z
	glm::vec2 f;
	f.x = randomX;
	f.x += across.x * distance;
	f.x -= k_FleeJitter * 0.5f;
	f.y = randomZ;
	f.y += across.y * distance;
	f.y -= k_FleeJitter * 0.5f;
	// on the side of the way the living being stands on
	const float side = across.y * (me.y - object.y) + across.x * (me.x - object.x);
	if (side < 0.0f || std::isnan(side))
	{
		return me - f;
	}
	return me + f;
}

bool flee::ComingTowards(glm::vec3 me, glm::vec3 object, glm::vec3 movement)
{
	// the line from the thing to the living being, made one long
	glm::vec3 to = me - object;
	if (to.x != 0.0f || to.y != 0.0f || to.z != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt((to.z * to.z + to.y * to.y) + to.x * to.x);
		to *= inverse;
	}
	// its way, made one long
	glm::vec3 way = movement;
	if (way.x != 0.0f || way.y != 0.0f || way.z != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt((way.z * way.z + way.y * way.y) + way.x * way.x);
		way *= inverse;
	}
	const float cosine = (way.z * to.z + way.y * to.y) + way.x * to.x;
	return cosine >= k_ComingTowardsCosine;
}

uint8_t flee::FleeFromSpellPriority(uint32_t priority, int32_t fastDistance)
{
	if (fastDistance < k_FleeUrgencyUnits)
	{
		const int32_t urgency = (k_FleeUrgencyUnits - fastDistance) * 100 / k_FleeUrgencyUnits;
		return static_cast<uint8_t>(static_cast<uint32_t>(urgency) + priority);
	}
	return static_cast<uint8_t>(priority);
}
