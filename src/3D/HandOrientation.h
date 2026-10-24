/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace openblack::hand_orientation
{

/// The hand's up eases to the land's slope over this many seconds
constexpr float k_UpEaseSeconds = 0.4f;
/// The line of sight must lean more than this from straight down, east-west or north-south, to turn the hand
constexpr float k_MinHeadingLean = 0.01f;

/// Which way the hand faces across the land: along the line of sight through the cursor, flattened. Looking straight
/// down it keeps the way it faced.
[[nodiscard]] glm::vec3 HeadingAlongRay(glm::vec3 rayDirection, glm::vec3 previousHeading);

/// The hand's eased up made a unit vector as the original does it: one over the square root of (z² + y²) + x², each
/// step rounded to a float. An up whose three parts all count as zero (zero or not a number) is left as it is
[[nodiscard]] glm::vec3 NormaliseUp(glm::vec3 up);

/// The hand's rotation from its heading (the yaw of the line of sight) and its up, as the original works it. The
/// facing d = (sin b, 0, -cos b), b = heading + pi, is level and points back towards the camera; side = d x up,
/// normalised unless all three of its parts are zero. The model's X = side, Y = side x up, Z = -up. The up is taken
/// as it is given, not normalised (the caller's up already is, or is meant not to be)
[[nodiscard]] glm::mat3 HandRotation(float heading, glm::vec3 up);

} // namespace openblack::hand_orientation
