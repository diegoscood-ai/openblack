/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

namespace openblack::ecs::components
{
/// An object whose part under the water is drawn cut by the plane (0, -1, 0, 0) before the sea (LH3DObject vt+0x11C
/// DrawCutByPlane: animated fn_00811C70, static fn_0080C050), like the sharks (fn_00774E30, SetColorSpecular(0xFF303070,
/// 0)), the swimming SuperVillagers (GLandscape::Draw 0x5E4C45) and the fish puzzle's net (fn_00829BC0). Renderer::
/// DrawCutBelowWater draws it into the reflection target, mirrored like everything the sea shows through.
///
/// The part above the water (the same call with the plane (0, 1, 0, 0), the shark's Whale::Draw 0x774E10 in the colour
/// of the land light table[255]) is Renderer::DrawCutByPlane with keep = 1: the owner of the object calls it instead
/// of its normal draw.
struct CutByPlane
{
	uint32_t belowColour {0xFF303070u}; ///< 0xAARRGGBB of SetColorSpecular (the sharks' dark blue)
	/// the owner draws the part above the water too, instead of the whole model (the sharks' Whale::Draw 0x774E10):
	/// the normal pass skips it and Renderer::DrawCutAboveWater draws it with keep = 1 and table[255]
	bool drawAbove {false};
};
} // namespace openblack::ecs::components
