/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "3D/AllMeshes.h"

namespace openblack
{
struct GVillagerInfo;
struct GAnimalInfo;
} // namespace openblack

/// The detail meshes of villagers and animals (info: high, std, low). The original gives the LH3DObject the three
/// (GetDetailMesh(2, 1, 0), Villager::SetAge 0x7528C0), but its LevelOfDetail loads are NOPed: always LOD 1, the std
/// mesh (docs/bw1-notes/rendering.md, "LOD"). Mod graphics.hd-people (detail = high): the high mesh, twice the triangles.
namespace openblack::ecs::detail_meshes
{
[[nodiscard]] MeshId Villager(const GVillagerInfo& info, bool child);
[[nodiscard]] MeshId Animal(const GAnimalInfo& info);

/// Once a frame: when the option changed, every villager and animal that shows one of its detail meshes gets the
/// one it should have now
void Update();
} // namespace openblack::ecs::detail_meshes
