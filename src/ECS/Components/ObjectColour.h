/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <cstdint>

namespace openblack::ecs::components
{

/// The LH3DObject's +0x4C colour set with SetColour (vt 0x2C, fn_007F9770: edx -> +0x4C, the stack argument -> +0x50)
/// instead of the land light: the power-up bands (DrawSpellGraphic 0x51A3BE, GetPlayerColour 0x64D800; PHandFX
/// Band::Draw 0x68D86D..0x68D8B1). Its alpha goes in components::Alpha. RenderingSystem packs both with
/// lh3d_colour::PackInstanceColour / PackInstanceSpecular like the PSys mesh atoms' (vs_object: the colour x the model
/// light, no haze, plus the specular).
struct ObjectColour
{
	std::array<uint8_t, 3> rgb {255, 255, 255};
	uint32_t specular {0}; ///< +0x50, a D3DCOLOR (its alpha unused)
};

} // namespace openblack::ecs::components
