/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

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

} // namespace openblack::hand_orientation
