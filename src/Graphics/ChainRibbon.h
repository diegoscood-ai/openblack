/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <span>
#include <vector>

#include <glm/vec3.hpp>

/// The corners of a chain ribbon (the lightning bolt and the storm's forks, the gesture trail, the beams): a strip
/// through the chain's joints that faces the camera, built on the CPU as the original builds it
namespace openblack::graphics::chain_ribbon
{

/// Four corners to each segment: head + side, head - side, tail + side, tail - side
inline constexpr uint32_t k_CornersPerSegment = 4;

/// One joint of the chain: where it is, and its scale, which is the ribbon's half width there
struct Joint
{
	glm::vec3 position;
	float scale;
};

/// The ribbon's corners seen from `eye`, four to each segment between two joints that follow each other. Each end of a
/// segment is moved to either side by cross(eye - joint, segment) x the joint's scale / its length, and not at all where
/// that cross product is exactly zero (a zero-length segment, or the eye on the segment's line). Where two segments
/// meet, both pairs of corners move to their midpoints. Nothing for fewer than two joints
[[nodiscard]] std::vector<glm::vec3> Corners(std::span<const Joint> joints, const glm::vec3& eye);

} // namespace openblack::graphics::chain_ribbon
