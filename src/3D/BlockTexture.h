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

namespace openblack
{
class LandIslandInterface;

namespace lnd
{
struct LNDCountry;
}

/// The original's block textures (ARGB4444, 16 x 16 texels per cell), built once per block by fn_00873790 per cell:
/// the texel builder fn_008732C0 (SSE fn_007AB4B0) for the cell's country, and for a cell whose four corners are not
/// all of one country, one build per corner country blended with the cone weights (fn_00871850). Colour and coast
/// alpha together; the alpha nibble is coast::CoastAlphaNibble. No render dependencies.
namespace block_texture
{
/// The LND material textures, each 256 x 256 raw B5G5R5 words [x * 256 + z] (LNDMaterial::texels), one after another
struct Materials
{
	std::span<const uint16_t> texels;
	size_t count;
};

/// fn_008732C0 for one texel of weighted altitude h (255 x altitude on a flat cell) with one country's table
/// (0x873358..0x873685): 0 below h = 0x100; else the table entry mat = min((h >> 8) + noise, 255), whose colour is
/// indices[0] x coefficient + indices[1] x (256 - coefficient) (or indices[0] alone when they are equal) at the texel,
/// times bump >> 8 into 4 bits per channel (clamped at 15), and the coast alpha nibble. texelIndex = x * 256 + z in
/// the block, the same index for the materials, the noise and the bump map.
uint16_t CountryTexel(const lnd::LNDCountry& country, int32_t h, uint8_t noise, uint8_t bump, const Materials& materials,
                      size_t texelIndex);

/// fn_00871850: the four corner builds of a texel blended per 4-bit channel (alpha too) with its cone weights,
/// floor(sum(channel_k w_k) / 255)
uint16_t BlendCorners(const std::array<uint16_t, 4>& texels, const std::array<uint8_t, 4>& weights);

/// The whole island, RGBA8 (each nibble x 17), rows along z and columns along x (row 0 = the lowest z of the block
/// extent), texelsPerBlock texels per block side like CoastAlpha (with fewer than 256 each texel takes the original's
/// texel under its corner). Cells with bit 0x02 of their flags byte are 0 (0x8739F8). noise and bump: the LND maps,
/// 256 x 256, [x * 256 + z], the same for every block ([0xFA7694] / [0xFA7698]).
std::vector<uint8_t> BuildIslandBlockTexture(const LandIslandInterface& island, glm::u16vec2 extentIndexMin,
                                             glm::u16vec2 indexSize, uint16_t texelsPerBlock, std::span<const uint8_t> noise,
                                             std::span<const uint8_t> bump, const Materials& materials);
} // namespace block_texture
} // namespace openblack
