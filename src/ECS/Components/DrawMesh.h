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

#include <entt/fwd.hpp>

namespace openblack::ecs::components
{

/// The model RenderingSystem draws for an entity instead of its Mesh: a building partly built (MultiMapFixed::Draw
/// 0x518090 -> DrawBuilding 0x517F90 -> fn_816AD0, physics::PartialBuild::BuildMesh; abodes::RedrawConstruction). The
/// Mesh component stays the whole model, as the original's 3D object keeps its mesh (Get2DRadius 0x638180, the map
/// cells, the static shadow and the footprint read it); only the model passes take this one
struct DrawMesh
{
	entt::id_type id;
	int8_t submeshId;
	int8_t bbSubmeshId;
};

} // namespace openblack::ecs::components
