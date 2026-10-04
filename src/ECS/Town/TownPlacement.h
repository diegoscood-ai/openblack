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

#include <entt/entity/entity.hpp>
#include <entt/fwd.hpp>

#include "ECS/MapCoords.h"

// Where a town may put a fixed building (MapCoords.cpp / Town.cpp of runblack.exe W120; spec
// dev\documentacion\edificios\V6_pending.md §1): the town rectangle Town +0x728..+0x73A, the fixed check
// MapCoords::IsSuitableForFixedAbodeInTown 0x603860 and its wrapper GAbodeInfo::IsOkToCreateAtPos 0x404B10. Not
// GObjectInfo::IsOkToCreateAtPos 0x638C40 (map_collide::IsOkToCreateAtPos, the CREATE_* commands' rule).

namespace openblack
{
struct GAbodeInfo;
}

namespace openblack::ecs::town_placement
{
/// IsSuitableForFixedAbodeInTown's "inside the nearest other town's rectangle" result (0x603880..0x60388C)
constexpr uint32_t k_InsideOtherTown = 0xE;

/// Town::SetTownArea 0x73AAF0: min = 0x7FFFFFFF, max = 0 (0x73AB00..0x73AB1B), then fn_73AC90 for every abode
/// (+0x754) and every field (+0x780; openblack's fields are abodes). (approximate) the original keeps it up to date at
/// AddStructureToTown 0x739A0A / RemoveStructureFromTown 0x739BB6 / Field::DeleteDependancys 0x5281C3 / the Field ctor
/// 0x527E82; openblack recomputes it whenever it is read (the same values for the same abodes)
void SetTownArea(entt::entity town);
/// SetTownArea for every town (map_cells::ForEachTown), before a query reads the rectangles (approximate, as above)
void SetAllTownAreas();
/// fn_0073AE10 0x73AE10: the rectangle's centre as it is (SetTownArea first), x = ftol((maxX x 10 / 65536 + minX x 10
/// / 65536) x 0.5 x 65536 / 10), z the same with +0x738 / +0x72C, altitude 0 (Town::MakeScenicForest 0x741B8C). No
/// town: {0, 0, 0}
[[nodiscard]] map_coords::MapCoords GetTownAreaCentre(entt::entity town);
/// MapCoords::IsSuitableForFixed(pos, mesh, angle, scale) 0x6038B0 -> fn_604020: the mesh's NewCollideDescriptor cells
/// (map_cells::DescriptorCells, reach scale x half diagonal + 1); for each: the position's own cell on the map and not
/// water, and no MultiMapFixed of the cell's fixed list with d < R_obj + scale x max(hx, hz) (strict). No mesh: 0
[[nodiscard]] bool IsSuitableForFixed(const map_coords::MapCoords& pos, entt::id_type meshId, float yAngle,
                                      float scale);
/// MapCoords::IsSuitableForFixedAbodeInTown(pos, mesh, town, angle, scale) 0x603860: with a town, the nearest OTHER
/// town (GetNearestTown 0x601F90, tribe -1) whose rectangle is within 4 cells -> 0xE (accepted WITHOUT the fixed tests:
/// literal); else IsSuitableForFixed (0 / 1)
[[nodiscard]] uint32_t IsSuitableForFixedAbodeInTown(const map_coords::MapCoords& pos, entt::id_type meshId,
                                                     entt::entity town, float yAngle, float scale);
/// GAbodeInfo::IsOkToCreateAtPos(pos, angle, scale, town) 0x404B10: IsSuitableForFixedAbodeInTown(pos, info's mesh
/// (vt +0x2C), town, angle, scale) != 0 (`neg; sbb; neg` 0x404B2E..0x404B32)
[[nodiscard]] bool IsOkToCreateAtPos(const GAbodeInfo& info, const map_coords::MapCoords& pos, float yAngle,
                                     float scale, entt::entity town);
} // namespace openblack::ecs::town_placement
