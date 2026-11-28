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

#include <optional>
#include <span>
#include <vector>

#include <glm/vec3.hpp>

#include "3D/AffineMatrix.h"

// The boxes a boned body is met through: one a bone, the box about the vertices that bone moves, as the mesh is
// built, placed each pose by the bone's matrix. What is thrown at a creature meets the ellipsoid inside each box. Wiki:
// docs/wiki/physics/physics.md#a-creatures-body.

namespace openblack::physics::collide_boxes
{

/// One bone's box: its corners in the mesh's own space, and the bone's matrix this pose with its inverse
struct Box
{
	/// The box starts at the mesh's origin on both corners, so it always reaches it
	glm::vec3 min {0.0f};
	glm::vec3 max {0.0f};
	affine::AffineMatrix matrix;
	affine::AffineMatrix inverse;
};

/// A run of a primitive's vertices that one bone moves
struct VertexGroup
{
	uint32_t vertexCount {0};
	uint32_t bone {0};
};

/// A mesh's boxes, as many as it has: its bone count (the low byte of its flags) with bones, else one
[[nodiscard]] std::vector<Box> Make(uint32_t count);

/// The boxes grown by one primitive's vertices (in the mesh's space): with bones, each group's vertices into its bone's
/// box, in the group's order; without, every vertex into the first box. A group naming a bone past the boxes is skipped
void AddPrimitive(std::span<Box> boxes, std::span<const glm::vec3> vertices, std::span<const VertexGroup> groups,
                  bool hasBones);

/// Each box takes its bone's matrix this pose (the first bones' matrices, as many as there are boxes) and its inverse
void InitForCollision(std::span<Box> boxes, std::span<const affine::AffineMatrix> bones);

/// Where a line meets a box's ellipsoid: the point, the outward way there (unit) and how far along the line it is
struct Impact
{
	glm::vec3 point {0.0f};
	glm::vec3 normal {0.0f};
	float distance {0.0f};
};

/// Where the line from `start` along `direction` (in the world) first meets the ellipsoid inside the box, its
/// distance counted in the line's steps of `direction`; none for a box flatter than a ten-thousandth on a side, a
/// direction too short in the box's space, or a line that misses it
[[nodiscard]] std::optional<Impact> NearestImpact(const Box& box, const glm::vec3& start, const glm::vec3& direction);

/// The nearest of the boxes' impacts: the first of the nearest in the boxes' order; none when the line meets none
[[nodiscard]] std::optional<Impact> NearestImpact(std::span<const Box> boxes, const glm::vec3& start,
                                                  const glm::vec3& direction);

} // namespace openblack::physics::collide_boxes
