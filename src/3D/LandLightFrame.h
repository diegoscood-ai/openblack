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

namespace openblack
{

/// The land's light of this frame at a level of luminosity, 0xRRGGBB: what things made now and coloured by the light,
/// such as a ring on the water, keep. From the table the renderer built last (land_light::CurrentTable); white before
/// the first build.
[[nodiscard]] uint32_t FrameLandLight(uint8_t level);

} // namespace openblack
