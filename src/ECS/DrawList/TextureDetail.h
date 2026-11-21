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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "BlockCull.h"

/// The land's texture detail line: beyond it a block shows its low-resolution tile instead of its own texture, and
/// in its band the two are blended by a per-vertex alpha
/// (docs/wiki/world/terrain.md#what-a-block-shows-meanwhile-and-the-far-blocks). Pure functions, no render
/// dependencies.
namespace openblack::ecs::draw_list
{

/// What a block shows of its texture: its own (0), only its low-resolution tile (1), or its own with the tile over it
/// by the vertices' alpha (2)
enum class TextureLevel : uint8_t
{
	Own = 0,
	TileOnly = 1,
	Blended = 2,
};

/// The line before any land is created: 300 ahead, its band 40 wide (20 each side)
constexpr LodLine k_StartTextureLine {.distance = 300.0f, .width = 40.0f, .halfWidth = 20.0f};

/// How many textures the original's texture pool holds on the test machine with the default settings: it fills the
/// pool up to its 256 MB cap at start-up (16 small surfaces, 3 big ones, then 256 x 256 ones until the cap), about
/// 2064 (docs/wiki/world/terrain.md#the-texture-detail-lines-distance). openblack has no pool to count: this stands
/// for the count
constexpr int32_t k_TexturePoolCount = 2064;

/// The line once a land is created, from the detail index and the texture pool's count. At index 4 it is set afresh,
/// `along` (1, 0, 0) and the point 0, 400 ahead with a band of 70 (35 each side), then by the count, compared signed
/// and strictly: above 600, 800 ahead; above 500, 700; above 400, 600; each with a band of 170 (85 each side). At any
/// other index the land's creation leaves the line as it is
[[nodiscard]] LodLine TextureLineAtLandCreation(const LodLine& current, int32_t detailIndex, int32_t poolCount);

/// The line rebuilt for this frame, as RebuildLodLine but in the plane at the height y_ref = min(eye.y, 0.67 x 165)
/// (110.55 when eye.y is above it or NaN), m = -y_ref: the point is (d - m f.y, m f.x, 0) / n.z when the line runs more
/// along z, else (0, -(m f.z), m f.y - d) / n.x, each the reciprocal times the component. Every step rounds to a
/// float, in this order
[[nodiscard]] LodLine RebuildTextureLine(const LodLine& previous, glm::vec3 eye, glm::vec3 viewDirection);

/// A block's texture level from where its 4 ground corners, (x, z), (x, z + 160), (x + 160, z) and (x + 160, z + 160),
/// are against the line: all near, its own texture; all far, its tile only; any in the band or on both sides, both.
/// A block still waiting for its first texture shows its tile only, wherever it is
[[nodiscard]] TextureLevel BlockTextureLevel(glm::vec2 blockMapPos, const LodLine& line, bool pending);

/// The alpha of a blended block's tile at a vertex, from the vertex's place: v = (x - point.x) along.z -
/// (z - point.z) along.x. Beyond the band's near side (v > halfWidth) 0; inside it, (halfWidth - v) times
/// `alphaScale` (255 / width, once per block) rounded to the nearest, ties to even, its low byte; otherwise (at or past
/// the far side, or NaN) 255
[[nodiscard]] uint8_t TileAlpha(const LodLine& line, float alphaScale, float x, float z);

/// 255 / width, the tile alpha's scale for a block
[[nodiscard]] float TileAlphaScale(const LodLine& line);

} // namespace openblack::ecs::draw_list
