/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "3D/LandMorph.h"

namespace openblack::ecs::components
{

/// Meshes with this component will have their vertices match terrain height maps: the melting of land_morph
/// (LH3DObject::UpdateMelting 0x8168F0 and the morphable Draw 0x80E550, here vs_object_hm_instanced)
///
/// They correspond with vanilla Get3DType of MORPHABLE or CITADEL
/// The following Objects should be created with this component:
/// * BigForest
/// * Graveyard
/// * PileFood (not wood)
/// * StoragePit x2 meshes
/// * Wonder (Depends on tribe type but defaults to morphable)
/// * Workshop
/// * CitadelHeart
/// * CitadelPart
/// * Creche
/// * Football
/// * PhysicalShield (Get3DType 0x72CE50 = 1; MagicShield::Get3DType 0x72C340 is the static Object one), Live
/// * TownCentre
/// * Field
/// * the ground marks (fn_00825240, ecs/GroundMarks.h): an uprooted tree's crater, an explosion's mark
/// * DesignedWaterFall's ark and dinosaur (LH3DObject::Create(1), UpdateMelting at 0x5E3C55 / 0x5E3DBE)
struct MorphWithTerrain
{
	/// When the original takes the deltas (land_morph::Melting: Snapshot at creation, Live on every draw)
	land_morph::Melting mode {land_morph::Melting::Snapshot};
};

} // namespace openblack::ecs::components
