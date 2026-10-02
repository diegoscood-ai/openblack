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

#include <memory>
#include <string>
#include <vector>

#include "3D/FrameAnim.h"
#include "PSys/PSys.h"
#include "PSys/PSysManager.h"

// Landscape light maps: ParticleLightMapCreator (0x6A9D80, GetBitmap 0x6A9D40, ParticleLightMap::DrawAt 0x67B220) — the
// bright splash a fireball, a lightning fork, a storm or the beam leaves on the ground. DrawAt puts a record in the list
// 0xD4EDB8 and PSysLightMaps::AddDrawing 0x6CA6E0 -> fn_006CA540 -> fn_006CA280 stamps each into the land's cells every
// frame (fn_0086CFF0, land_light::AddStamp): the light in the cells' colour, which the land and the models standing
// there take as their specular. Report: tmp_dis\psys\part_render.md §8; wiki docs/bw1-notes/rendering.md.

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
	float shiftX {0.0f}; ///< +0x80 / +0x84 (DrawAt 0x67B220 and fn_006CA280 do not read them)
	float shiftZ {0.0f};
	/// +0x34 the GJBitmap of GetBitmap 0x6A9D40: LoadBitmapFromFile(TextureFileName, Pitch, 3, NumFramesInFile,
	/// NumFramesInUse) (land_light::LoadBitmapFile); none when the file is missing or of another size
	std::shared_ptr<const graphics::frame_anim::StackedFrames> bitmap;

	/// CreateParticleLightMap 0x6A9D80: the frame animation of a new atom (not a sprite, so PSys.cpp does not set it)
	void InitAtom(Effect& effect, Atom& atom) const override;
};

namespace light_map_atoms
{
/// ParticleLightMap::DrawAt 0x67B220 for every light map atom of this frame, then PSysLightMaps::AddDrawing's
/// fn_006CA280: Stamp of manager::Collect, once between two land_light::ClearStamps
void SubmitFrame();
/// One stamp per light map atom of `drawables` (land_light::AddStamp); returns how many went into the list
int Stamp(const std::vector<manager::Drawable>& drawables);
} // namespace light_map_atoms

} // namespace openblack::psys
