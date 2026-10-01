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

#include <string>

#include "PSys/PSys.h"

// Landscape light maps: ParticleLightMapCreator (0x6A9D80, GetBitmap 0x6A9D40, ParticleLightMap::DrawAt 0x67B220) — the
// bright splash a fireball, a lightning fork, a storm or the beam leaves on the ground. The original records them in
// list 0xD4EDB8 and PSysLightMaps::AddDrawing 0x6CA6E0 blits every frame (fn_0086CFF0) into the landscape's dynamic
// light texture. Report: tmp_dis\psys\part_render.md §8; wiki docs/bw1-notes/magic.md.
//
// (deviation, off-nothing: the port has no dynamic landscape light texture, so the atoms are drawn as flat additive
// quads lying on the ground, with the frames of the .raw packed into one 8 x 8 atlas texture. The light therefore does
// not follow the terrain slope and does not tint the objects standing on it.)

namespace openblack::psys
{

/// ParticleLightMapCreator (props 0x6B45E0)
struct LightMapCreator: Creator
{
	int pitch {1};           ///< +0x64 the side of one square frame, pixels
	int numFramesInFile {1}; ///< +0x68
	int numFramesInUse {1};  ///< +0x6C
	float randJitter {0.0f}; ///< +0x70 with UseRandJitter (+0x74): metres of noise per axis
	bool useRandJitter {false};
	float shiftX {0.0f}; ///< +0x80 / +0x84, they compensate the blit's +10 m offset (unused here)
	float shiftZ {0.0f};

	/// ParticleLightMap::DrawAt: the position noise of a new atom
	void InitAtom(Effect& effect, Atom& atom) const override;
};

} // namespace openblack::psys
