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

#include <glm/vec2.hpp>

/// What the game prints into a block's own texture once its texels are painted: the footprints of the buildings,
/// fields and river beds over it (docs/wiki/world/terrain.md#the-prints-the-16th-step). The texture is ARGB 4444 as the
/// game keeps it, `size` texels a side and one row per texel along the block's x: texel (x, z) is
/// `texels[size * x + z]`. No render dependencies.
namespace openblack::block_texture
{

/// One triangle of a footprint: its corners on the ground in the footprint's own frame (x, z), and the texel each
/// corner takes from the footprint's picture (column, row, in texels)
struct PrintTriangle
{
	std::array<glm::vec2, 3> corners;
	std::array<glm::vec2, 3> texels;
};

/// A footprint's picture (ARGB 4444, `width` texels a row) and its triangles
struct PrintPicture
{
	int32_t width {0};
	int32_t height {0};
	std::span<const uint16_t> texels;
	std::span<const PrintTriangle> triangles;
};

/// How a footprint goes into the texture. Colour: each colour nibble moves towards the picture's by the picture's
/// alpha, (s (255 - a) + f a) / 255 rounded down with a = 16 x the alpha nibble (so even 15 leaves a little of the
/// land), the land's alpha kept. Alpha: the colour kept and the alpha lowered to the picture's, raised first by the
/// strength's high nibble (the river beds' channels, which the sea shows through)
enum class PrintMode : uint8_t
{
	Colour,
	Alpha,
};

/// The strength the alpha mode reads when no other is given, and the one the game goes back to after each print
constexpr uint32_t k_DefaultPrintStrength = 0xFF;

/// Prints `picture` into the texture of the block at (blockX, blockZ) in the land's block grid (its first texel at
/// 160 x the index), placed by `placement`, the 12 floats of the object's matrix (its right, up and forward axes and
/// its position; only the ground's x and z are read). Each triangle on its own: its corners in texels, (world - block)
/// x size / 160, their edges walked from texel to texel rounding to the nearest (ties to even), each texel row along x
/// keeping the nearest and farthest z an edge reached and the picture's texel there; then each row filled from the one
/// to the other, the picture's texel stepped along as a float and taken at the nearest whole texel inside the picture.
/// Rows and texels outside the texture are left out. Overlapping triangles print twice.
void PrintFootprint(std::span<uint16_t> texels, int32_t size, glm::uvec2 block, const PrintPicture& picture,
                    const std::array<float, 12>& placement, PrintMode mode, uint32_t strength = k_DefaultPrintStrength);

/// How much a static shadow darkens a texel, by its coverage: the mask's byte holds one bit for each of its 8
/// samples, and the table gives 255 x the share covered, rounded down (0, 31, 63 ... 255); halved again with the
/// low-resolution textures, so that their shadows are half as dark
[[nodiscard]] std::array<uint8_t, 256> ShadowTable(bool lowResolution);

/// Darkens each texel by its byte of the shadow mask (the same index as the texel): each colour nibble x
/// (255 - table[coverage] / 2) / 256, rounded down, the alpha kept; a texel the shadows didn't reach is left as it is
void ApplyShadow(std::span<uint16_t> texels, std::span<const uint8_t> mask, const std::array<uint8_t, 256>& table);

/// The snow lying on the land, as its cells are kept: 128 x 128 cells of 40 units (4 x 4 to a block), depth[z * 128 +
/// x], and the two 256 x 256 noise maps that make its edges ragged and its white uneven (snowmap.raw and the whites made
/// from it, 14 or 15 a nibble)
struct SnowLayer
{
	static constexpr int32_t k_CellsPerSide = 128;
	std::span<const float> depths;
	std::span<const uint8_t> noise;
	std::span<const uint8_t> whites;
};

/// Whitens the texels of the block at (blockX, blockZ) under the snow lying on them. Cell by cell, a cell left alone when
/// none of its four corners is deeper than 5: its corners' depths in 16.16 fixed point (truncated), stepped across the
/// cell's texels in whole steps (each division truncated, along x at the cell's two edges and then along z between
/// them); at each texel the level d = ((depth >> 16) - noise) >> 3 against the noise under it: none at 0 or below,
/// (white, white, 15) above 15, else each colour nibble (16 - d) / 16 of the land's and d / 16 of the white, each part
/// rounded down; the alpha kept
void PrintSnow(std::span<uint16_t> texels, int32_t size, glm::uvec2 block, const SnowLayer& snow);

} // namespace openblack::block_texture
