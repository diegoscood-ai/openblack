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

// Mist particles: ParticleMistCreator (props 0x6B3C00, ctor 0x6AA380, CreateParticleMist 0x6AA610, CreateLH3DMist
// 0x6AA5A0) and RenderParticleMist::DrawAt 0x67A670. Each atom owns an LH3DMist (LH3DObject type 7: the mist.l3d dome
// with the smoke atlas, the same object as the map mists and the storm puffs); here it is handed every frame to mapa's
// mists::Submit (Graphics/Mists.h), which culls, sorts and draws it with Renderer::DrawMist. The water miracle's cloud
// (SF_Water, SF_WaterPU1, the water in the hand and on its holder) and the lightning storm use it. Wiki:
// docs/bw1-notes/magic.md, "Agua".

namespace openblack::psys
{

/// ParticleMistCreator (ParticleCreator + 0x7C bytes)
struct MistCreator: Creator
{
	bool takeRatioFromMatrix {false}; ///< +0x7B: k = M[1][1] / M[0][0] of the draw matrix
	bool isShadowMap {true};          ///< +0x79 (ctor 1)
	bool loadLightMap {true};         ///< +0x7A (ctor 1): with a TextureFileName, a land light / shadow map too
	std::string lightMap;             ///< +0x60 TextureFileName
	int pitch {12};                   ///< +0x54
	int numFramesInFile {1};          ///< +0x58
	int numFramesInUse {1};           ///< +0x5C
	float initialScaleMin {1.0f};     ///< +0x70
	float ratio {0.0f};               ///< +0x74: the mist's k (+0x8C); 0 = 2.5 + LocalFloatRand(2.5) per mist

	/// CreateParticleMist 0x6AA610: the scale, RandomiseScale ? PSysFloatRand(InitialScaleMin, InitialScale) :
	/// InitialScale (atom +0x74)
	void InitAtom(Effect& effect, Atom& atom) const override;
};

namespace mist_atoms
{
/// RenderParticleMist::DrawAt 0x67A670 for every mist atom of the running effects, once per frame (magic::Update):
/// size = the atom's scale, colour = the atom's colour x the land light base colour (alpha x 255 >> 8), the effect
/// branch (+0x80 |= 2) with k = Ratio, then mists::Submit. `milliseconds` advances the atlas counter.
void SubmitFrame(float milliseconds);
/// DrawAt's colour: per channel (c x base) >> 8, the alpha (a x 0xFF) >> 8 (the base's alpha byte is forced to 0xFF)
[[nodiscard]] uint32_t MistColour(uint32_t atomArgb, uint32_t baseArgb);
} // namespace mist_atoms

} // namespace openblack::psys
