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

/// Mod (graphics.terrain-x2): doubles the resolution of RGBA8 images with a separable Lanczos-3 filter (edges
/// clamped), each of the layers on its own. Returns layers of (2 * width) x (2 * height) RGBA8 texels.
std::vector<uint8_t> UpscaleRgba8Lanczos2x(const uint8_t* src, uint16_t width, uint16_t height, uint16_t layers);

} // namespace openblack::graphics
