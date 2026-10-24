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

#include <numbers>

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

glm::vec3 NormaliseUp(glm::vec3 up)
{
	// a part counts as zero when it equals zero or is not a number (an unordered compare)
	const auto isZero = [](float v) { return !(v < 0.0f) && !(v > 0.0f); };
	if (isZero(up.x) && isZero(up.y) && isZero(up.z))
	{
		return up;
	}
	const float scale = 1.0f / std::sqrt((up.z * up.z + up.y * up.y) + up.x * up.x);
	return {up.x * scale, up.y * scale, up.z * scale};
}

glm::mat3 HandRotation(float heading, glm::vec3 up)
{
	// half a turn on from the heading, a float sum; its sine and its negated cosine taken wider, each rounded once
	const float turn = heading + std::numbers::pi_v<float>;
	const auto s = static_cast<float>(std::sin(static_cast<double>(turn)));
	const auto c = -static_cast<float>(std::cos(static_cast<double>(turn)));
	// d x up with d = (s, 0, c), each product rounded to a float, the zero part of d not multiplied
	glm::vec3 side {-(c * up.y), c * up.x - s * up.z, s * up.y};
	// a part counts as zero when it equals zero or is not a number (an unordered compare)
	const auto isZero = [](float v) { return !(v < 0.0f) && !(v > 0.0f); };
	if (!isZero(side.x) || !isZero(side.y) || !isZero(side.z))
	{
		const float scale = 1.0f / std::sqrt(side.x * side.x + side.y * side.y + side.z * side.z);
		side = {side.x * scale, side.y * scale, side.z * scale};
	}
	// side x up, worked as the negation of up x side's terms
	const glm::vec3 across {-(side.z * up.y - side.y * up.z), -(side.x * up.z - side.z * up.x),
	                        -(side.y * up.x - side.x * up.y)};
	return {side, across, -up};
}

} // namespace openblack::hand_orientation
