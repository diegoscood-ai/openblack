/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

namespace openblack::ecs::physics
{
class FragMesh;
}

namespace openblack::ecs::components
{
/// Abode +0x90 DestructionMesh: a building a rock knocked pieces out of, drawn from its FragMesh.
struct BuildingDamage
{
	std::shared_ptr<physics::FragMesh> mesh;
	entt::id_type intactMesh {0}; ///< the building's own mesh: its Mesh component, which the damage never changes
	/// the FragMesh's model (FragMesh::BuildMesh, with the partly built draw over it), drawn as the building's
	/// components::DrawMesh while that DrawMesh holds this id. Kept here as well because the DrawMesh can be taken away
	/// without erasing it (abodes::RedrawConstruction's Remove before its on_destroy sink is connected): physics erases
	/// it again (a no-op once OnDrawMeshDestroyed has done it)
	entt::id_type generatedMesh {0};
	bool morphed {false};            ///< it had MorphWithTerrain (the FragMesh bakes the morph in)
	/// the repair site's baseline (site +0x640 = 1.1 x life - 0.1, set by Abode::ReduceLife at every hit):
	/// GetPercentForDrawBuilding = min(PercentBuilt, (life - s) / (1 - s))
	float repairBase {0.0f};
};

/// Fragment (a Rock subclass, 0x76EB20): a piece knocked off a building. It only hits the landscape, cannot be picked
/// up, and vanishes after 100 turns per triangle.
struct Fragment
{
	std::shared_ptr<physics::FragMesh> mesh;
	entt::entity parent {entt::null};
	entt::id_type generatedMesh {0};
	int turnsLeft {0};
	float area {0.0f};
};
} // namespace openblack::ecs::components
