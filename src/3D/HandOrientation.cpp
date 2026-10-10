/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandOrientation.h"

#include <cmath>

#include <glm/geometric.hpp>

namespace openblack::hand_orientation
{

glm::vec3 HeadingAlongRay(glm::vec3 rayDirection, glm::vec3 previousHeading)
{
	if (std::abs(rayDirection.x) > k_MinHeadingLean || std::abs(rayDirection.z) > k_MinHeadingLean)
	{
		return glm::normalize(glm::vec3(rayDirection.x, 0.0f, rayDirection.z));
	}
	return previousHeading;
}

} // namespace openblack::hand_orientation
