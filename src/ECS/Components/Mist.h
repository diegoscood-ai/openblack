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

namespace openblack::ecs::components
{

/// A bank of mist (Mist::Create 0x6063D0 -> CallVirtualFunctionsForCreation 0x606420): an LH3DObject of type 7
/// (LH3DMist, Data\Landscape\mist.l3d with the smoke material) at GetAltitude(pos) + the script's altitude, drawn by
/// fn_007FA300 facing the camera like the sky clouds. Its Transform holds the position.
struct Mist
{
	float size;      ///< LH3DObject +0x88: the mesh scale
	uint32_t colour; ///< ARGB; the alpha (+0x90) is colour >> 24
	/// +0x80 bit 2, set when the script's last value is not 1 (then +0x8C = k): scale size / (1 + (k - 1)
	/// (1 - |dy| / |d|)) and the sky light (from above, ambient 210); without it, the plain size, the colour times the
	/// land light under it (fn_00801C90) and the models' light
	bool edgeShrink;
	float k;          ///< +0x8C (3 by default, LH3DMist ctor 0x7F9560)
	int counter;      ///< +0x84: += int(time_inc * 0.255) modulo 900; the atlas frame is (counter / 20) & 15
	float counterRemainder;
};

} // namespace openblack::ecs::components
