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

#include "ECS/Components/DetailMesh.h"
#include "ECS/Components/DrawMesh.h"
#include "ECS/Components/Mesh.h"
#include "Graphics/DetailLevel.h"

/// The model an entity's model passes draw. One choice for both the draw layout, which makes a range per drawn model,
/// and the rows written every frame into those ranges, so that each row finds its range. Pure: values only.
namespace openblack::ecs::drawn_mesh
{

/// The drawn model: a partly built building's (components::DrawMesh) when it has one; else, at the top detail level,
/// the high mesh of a villager, an animal or a bird (components::DetailMesh), with the sub-meshes of its Mesh; else
/// its Mesh
[[nodiscard]] constexpr components::Mesh Of(const components::DrawMesh* draw, const components::Mesh& mesh,
                                            const components::DetailMesh* detail, uint8_t detailLevel)
{
	if (draw != nullptr)
	{
		return {draw->id, draw->submeshId, draw->bbSubmeshId};
	}
	if (detail != nullptr && graphics::detail_level::DrawsHighMeshes(detailLevel))
	{
		return {detail->high, mesh.submeshId, mesh.bbSubmeshId};
	}
	return mesh;
}

} // namespace openblack::ecs::drawn_mesh
