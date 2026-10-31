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

#include <array>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The static shadows the game bakes into a block's own texture: each object's mesh projected along the sun onto the
/// ground and drawn into a mask of 8 samples a texel (4 across z, 2 across x, one bit each), which then darkens the
/// texels by how much of each it covers (block_texture::ApplyShadow;
/// docs/wiki/rendering/shadows-and-reflections.md#shadows-three-systems-in-the-original). No render dependencies.
namespace openblack::block_texture
{

/// The sun the game casts the static shadows from: a point far away up and to the side
constexpr glm::vec3 k_ShadowSun {-500000.0f, 500000.0f, -500000.0f};

/// Where the mesh's vertices fall on the ground (y = 0) seen from `sun`: each vertex placed by the object's matrix (the
/// 12 floats: right, up, forward, position), its height taken from the object's own (`objectHeight`, the matrix's
/// position y) and kept at 0 or above, then moved along the line from the sun to the ground. Every step rounds to a
/// single float, in the game's order. Out: each vertex's ground (x, z)
[[nodiscard]] std::vector<glm::vec2> ProjectShadow(std::span<const glm::vec3> vertices, const std::array<float, 12>& matrix,
                                                   float objectHeight, glm::vec3 sun = k_ShadowSun);

/// A run of a skinned part's vertices that one bone moves
struct BoneRun
{
	uint16_t count {0};
	uint16_t bone {0};
};

/// As ProjectShadow for a skinned part: each run of vertices placed by its bone's matrix (12 floats), raised by the
/// object's height the same way, and not kept above the ground
[[nodiscard]] std::vector<glm::vec2> ProjectSkinnedShadow(std::span<const glm::vec3> vertices, std::span<const BoneRun> runs,
                                                          std::span<const std::array<float, 12>> bones, float objectHeight,
                                                          glm::vec3 sun = k_ShadowSun);

/// A ground point in the shadow mask's samples for the block whose first corner is `blockOrigin` (x, z): across the
/// mask (its x) the world z, 4 samples a texel; down it (its y) the world x, 2 samples a texel
[[nodiscard]] glm::vec2 ShadowMaskPoint(glm::vec2 ground, glm::vec2 blockOrigin, int32_t size);

/// The shadow rasteriser: for each sample row, the first and the last sample the triangle's edges reach, and the rows
/// the triangle spans. The edge tables keep their values from one triangle to the next, as the game's do
struct ShadowRaster
{
	static constexpr size_t k_Rows = 513;
	int32_t clipX {0};
	int32_t clipY {0};
	int32_t minY {0};
	int32_t maxY {0};
	std::array<int32_t, k_Rows> ends {};
	std::array<int32_t, k_Rows> starts {};

	/// For a texture `size` texels a side: 4 x size samples across, 2 x size down
	explicit ShadowRaster(int32_t size);
};

/// One mesh part's triangles drawn into `mask` (one byte a texel, `pitch` texels a row, with 4 bytes to spare past
/// its end): the sample row of each corner taken whole (truncated), its column kept as a float. A one-sided part
/// (neither `doubleSided` nor a temple's) draws only the triangles that face up; each triangle covers the samples
/// from its left edge's column (in) to its right edge's (out) on each row from its top (in) to its bottom (out).
/// A nonzero `oddRowsOnly` (the shadow's colour) draws only the odd sample rows
void DrawShadowTriangles(ShadowRaster& raster, std::span<uint8_t> mask, int32_t pitch, std::span<const glm::vec2> points,
                         std::span<const std::array<uint16_t, 3>> triangles, bool doubleSided, bool temple,
                         uint32_t oddRowsOnly = 0);

/// A part of a mesh whose texture's alpha casts the shadow (a tree's, a forest's): its vertices, their texture
/// coordinates, its triangles and the texture's alpha at 64 x 64 (the alpha nibble in each byte's high nibble)
struct TextureShadowPart
{
	std::span<const glm::vec3> positions;
	std::span<const glm::vec2> uvs;
	std::span<const std::array<uint16_t, 3>> triangles;
	std::span<const uint8_t> alpha;
};

/// The shadow of an object whose texture's alpha casts it, into the block's mask (one byte a texel, `size` a row):
/// each vertex placed by the object's matrix moved to the block (its height dropped) and kept at or above the ground,
/// moved along the line from the sun and taken to texels (1.6 a unit); each triangle's edges walked from texel to
/// texel (truncated), each row filled from its nearest to its farthest texel with the alpha map's byte at the
/// texture coordinates there (x 63, nearest, wrapping) ORed in: so the texture's alpha sets the mask's high nibble
void DrawTextureShadow(std::span<uint8_t> mask, int32_t size, glm::uvec2 block, const std::array<float, 12>& placement,
                       std::span<const TextureShadowPart> parts, glm::vec3 sun = k_ShadowSun);

} // namespace openblack::block_texture
