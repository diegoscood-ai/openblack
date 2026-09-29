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

namespace openblack::graphics
{

/// Mod (EngineConfig::textureMipmaps). The original never set D3DTSS_MIPFILTER and its textures have no
/// mip levels. Decodes the base level of each layer to RGBA8 and appends a box-filtered chain down to 1x1, in the
/// layer-major order bgfx expects (layer 0 lods 0..n, layer 1 lods 0..n, ...).
/// Colour is averaged weighted by alpha so that transparent texels don't bleed dark fringes into cut-outs, and textures
/// with on/off alpha keep their coverage at every level so that leaves and fences don't thin out in the distance.
/// @param bimgFormat a bimg::TextureFormat::Enum (same values as bgfx::TextureFormat::Enum).
std::vector<uint8_t> BuildRgba8MipChain(const void* src, uint16_t width, uint16_t height, uint16_t layers, int bimgFormat);

} // namespace openblack::graphics
