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

#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::graphics
{

/// Mod graphics.hd-people (smooth): rounder low-poly boned meshes. Each triangle becomes the cubic PN triangle of its
/// corners and normals (Vlachos et al. 2001, "Curved PN Triangles") and is split into level² triangles.
/// Vertices are in the space of their bone (rigid skin, one bone per vertex, like the L3D vertex groups): the surface is
/// built in the rest pose (restBones = each bone's model matrix) and every new vertex goes back to the bone of the
/// nearest corner, so an animated mesh bends at the same joints as the original's.
/// Corners that share a rest position (UV seams, bone borders) are welded and share one averaged normal, and the points
/// of an edge only depend on its two welded corners, so the surface has no cracks.
struct PnVertex
{
	glm::vec3 position; ///< in its bone's space
	glm::vec2 uv;
	glm::vec3 normal; ///< in its bone's space
	int16_t bone;     ///< -1: no bone (model space)
};

struct PnRange
{
	uint32_t indicesOffset;
	uint32_t indicesCount;
};

/// Replaces vertices, indices and each range's triangles with the tessellated ones (ranges keep their order).
/// Returns false, changing nothing, if the result would not fit in 16-bit indices.
bool TessellatePn(std::vector<PnVertex>& vertices, std::vector<uint16_t>& indices, std::vector<PnRange>& ranges,
                  const std::vector<glm::mat4>& restBones, int level);

} // namespace openblack::graphics
