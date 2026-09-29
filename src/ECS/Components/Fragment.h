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
	entt::id_type intactMesh {0};    ///< the building's own mesh (its physics body keeps using it)
	entt::id_type generatedMesh {0}; ///< the drawn FragMesh
	bool morphed {false};            ///< it had MorphWithTerrain (the FragMesh bakes the morph in)
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
