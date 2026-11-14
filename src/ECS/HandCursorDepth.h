/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

/// Whether the land or the object takes the hand's cursor. The game compares them by their depth in front of the
/// camera, not by their distance along the cursor's ray, and counts the land 2.3 further than it is.
namespace openblack::ecs::hand_cursor_depth
{

/// How much further the land counts than it is, in depth in front of the camera
constexpr float k_LandMargin = 2.3f;

/// The depth in front of the camera of a point the eye sees along `offset`
[[nodiscard]] inline float ViewDepth(glm::vec3 offset, glm::vec3 forward)
{
	return glm::dot(offset, forward);
}

/// Whether the land, with its margin, lies in front of the object
[[nodiscard]] constexpr bool LandBeforeObject(float landDepth, float objectDepth)
{
	return landDepth + k_LandMargin < objectDepth;
}

} // namespace openblack::ecs::hand_cursor_depth
