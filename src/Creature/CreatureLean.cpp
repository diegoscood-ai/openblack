/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureLean.h"

#include "3D/ObjectMatrix.h"

using namespace openblack;

creature_lean::Heights creature_lean::Sample(glm::vec3 position, float radius, bool carried,
                                             const std::function<float(glm::vec2)>& ground)
{
	if (carried)
	{
		return {.xAhead = position.y, .xBehind = position.y, .zAhead = position.y, .zBehind = position.y};
	}
	return {
	    .xAhead = ground(glm::vec2(position.x + radius, position.z)),
	    .xBehind = ground(glm::vec2(position.x - radius, position.z)),
	    .zAhead = ground(glm::vec2(position.x, position.z + radius)),
	    .zBehind = ground(glm::vec2(position.x, position.z - radius)),
	};
}

creature_lean::Lean creature_lean::FromHeights(const Heights& heights, float radius)
{
	const float across = radius + radius;
	const auto limit = [](float lean) {
		if (lean >= k_Limit)
		{
			lean = k_Limit;
		}
		// not a number fails this test too, and leans the lowest
		if (!(lean > -k_Limit))
		{
			lean = -k_Limit;
		}
		return lean;
	};
	return {
	    .x = limit((heights.xAhead - heights.xBehind) * k_Share / across),
	    .z = limit((heights.zAhead - heights.zBehind) * k_Share / across),
	};
}

glm::mat3 creature_lean::Leaned(const glm::mat3& rotation, Lean lean)
{
	glm::mat3 leaned;
	for (int i = 0; i < 3; ++i)
	{
		const auto& axis = rotation[i];
		leaned[i] = glm::vec3(((axis.z * 0.0f) + (axis.y * 0.0f)) + axis.x, ((axis.x * lean.x) + (axis.z * lean.z)) + axis.y,
		                      ((axis.x * 0.0f) + axis.z) + (axis.y * 0.0f));
	}
	return leaned;
}

glm::mat4 creature_lean::Model(glm::vec3 position, const glm::mat3& rotation, glm::vec3 scale, Lean lean)
{
	return affine::Model(position, Leaned(rotation, lean), scale);
}
