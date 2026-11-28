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

namespace openblack
{
/// The coast alpha of the original's block textures: the alpha nibble that the texel builder (and its SSE copy)
/// writes from the altitude and the noise, so that the land fades into the sea drawn before it (render
/// mode 14, SRCALPHA / INVSRCALPHA). No render dependencies: the island turns the result into a texture.
namespace coast
{
/// Texels per cell side and per block side of the original's block texture (UseHighTexture)
constexpr int k_TexelsPerCell = 16;
constexpr int k_TexelsPerBlock = 256;

/// Weights of the four cell corners for each of the 16 x 16 texels of a cell: w_k = max(0, 14 - d_k)
/// with d_k the distance in texels to the corner, truncated to a sum of 255 and the largest (the last of equals)
/// raised by 1 until they reach it. Index [i * 16 + j], i along x (cell +17), j along z (cell +1); corners [0] (x, z), [1] (x,
/// z + 1), [2] (x + 1, z + 1), [3] (x + 1, z).
using ConeWeights = std::array<std::array<uint8_t, 4>, k_TexelsPerCell * k_TexelsPerCell>;
const ConeWeights& GetConeWeights();

/// The alpha nibble (0..15) of a texel whose weighted altitude is h (255 x altitude on a flat cell) and whose noise
/// byte is n: 0 below 0x100, 15 from 0x400 up, in between from the alpha table (the same in the SSE path) with
/// e = h + 4 * (int8)n.
uint8_t CoastAlphaNibble(int32_t h, uint8_t noise);
} // namespace coast
} // namespace openblack
