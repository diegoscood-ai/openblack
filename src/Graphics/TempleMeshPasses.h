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

#include <algorithm>
#include <span>
#include <vector>

/// The passes a temple room's mesh is drawn in, as the game's mesh draw takes its submeshes
namespace openblack::graphics::temple_mesh_passes
{

/// What the passes need of a submesh: whether it is drawn through its lightmap, and whether it is a picture, a submesh
/// drawn through a rectangle of a sheet of its own (the challenge room's and the saved games' pictures)
struct SubMesh
{
	bool lightmapped {false};
	bool picture {false};
};

/// The submeshes in the order the mesh draw takes them: first those with a lightmap, then every other one but the
/// pictures; when the mesh has a picture, a third pass over its submeshes in turn draws the pictures and, once more,
/// every submesh of the second pass, so that a blended submesh without a lightmap is blended twice
[[nodiscard]] inline std::vector<uint32_t> Order(std::span<const SubMesh> subMeshes)
{
	std::vector<uint32_t> order;
	const auto count = static_cast<uint32_t>(subMeshes.size());
	for (uint32_t i = 0; i < count; ++i)
	{
		if (subMeshes[i].lightmapped && !subMeshes[i].picture)
		{
			order.push_back(i);
		}
	}
	for (uint32_t i = 0; i < count; ++i)
	{
		if (!subMeshes[i].lightmapped && !subMeshes[i].picture)
		{
			order.push_back(i);
		}
	}
	if (std::ranges::any_of(subMeshes, &SubMesh::picture))
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			if (subMeshes[i].picture || !subMeshes[i].lightmapped)
			{
				order.push_back(i);
			}
		}
	}
	return order;
}

} // namespace openblack::graphics::temple_mesh_passes
