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

#include <optional>
#include <vector>

#include "3D/LandMorph.h"

namespace openblack::ecs::components
{

/// Meshes with this component will have their vertices match terrain height maps: the melting of land_morph
/// (here vs_object_hm_instanced)
///
/// They correspond with the original's 3D type MORPHABLE or CITADEL
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
/// * PhysicalShield (3D type 1; the magic shield itself is a static object), Live
/// * TownCentre
/// * Field
/// * Flowers
/// * the ground marks (ecs/GroundMarks.h): an uprooted tree's crater, an explosion's mark
/// * DesignedWaterFall's ark and dinosaur (morphable, melted every frame)
struct MorphWithTerrain
{
	/// When the original takes the deltas (land_morph::Melting: Snapshot at creation, Live on every draw)
	land_morph::Melting mode {land_morph::Melting::Snapshot};
	/// The raise of each point of its body, taken once and kept, as the original takes them only as the object is made:
	/// from the place it stood when its first body was built, when a creature took it up
	/// (DynamicsSystemInterface::KeepMeltingOf) or just before the land first changed (FreezeMeltings), whichever came
	/// first; empty until then, when the land as it is under its place gives the same raises
	std::vector<float> frozenRaises;
	/// The land's height map it is drawn melted on (LandIslandInterface::GetHeightMapAt): the version of the land before
	/// the land first changed since the object was made, for one that keeps the melting it took then (Snapshot); none
	/// while the land has not changed, and always for one melted on every draw (Live): the land as it is
	std::optional<uint32_t> heightMapVersion;
	/// The place it stood on when a creature first took it up (DynamicsSystemInterface::KeepMeltingOf), where it is drawn
	/// melted once it stands elsewhere, as the original takes the melting only as the object is made; none before
	std::optional<land_morph::MeltPlace> meltPlace;
};

} // namespace openblack::ecs::components
