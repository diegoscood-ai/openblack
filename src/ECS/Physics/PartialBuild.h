/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/L3DSubMesh.h"

namespace openblack::ecs::physics
{
/// The partly built draw of a building (MultiMapFixed::DrawBuilding 0x517F90 -> fn_816AD0), in world space: the main
/// (status 0) mesh clipped at pos.y + pct x H x scale, an inner wall pass pushed in 0.35 (0.2 for two-sided materials),
/// a cap joining both cut outlines, and the scaffold (the highest status) rising (pct < 0.2), whole, or cut from the top
/// (pct > 0.8). A damaged building draws it over its FragMesh, at GetPercentForDrawBuilding.
class PartialBuild
{
public:
	[[nodiscard]] static std::vector<graphics::L3DSubMesh::GeneratedPrimitive> Build(entt::entity building, entt::id_type mesh,
	                                                                                 float percent);
	PartialBuild() = delete;
};
} // namespace openblack::ecs::physics
