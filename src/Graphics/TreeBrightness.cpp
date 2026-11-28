/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TreeBrightness.h"

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

using namespace openblack;

int tree_brightness::Factor(const glm::vec3& cameraFocus, const glm::vec3& cameraForward, const glm::vec3& light)
{
	// 200 + 55 x (the view's heading on the ground . the light's direction past the focus), never under 200
	const auto toFocus = cameraFocus - light;
	const glm::vec2 heading(cameraForward.x, cameraForward.z);
	// Full brightness when either has no direction
	if (!(glm::length(toFocus) > 1e-4f && glm::length(heading) > 1e-4f))
	{
		return 255;
	}
	const auto d = glm::normalize(toFocus);
	const auto v = glm::normalize(heading);
	const float dot = v.x * d.x + v.y * d.z;
	// Truncated
	return dot < 0.0f ? 200 : std::min(255, static_cast<int>(200.0f + 55.0f * dot));
}
