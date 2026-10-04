/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownPlacement.h"

#include <optional>

#include <glm/vec2.hpp>

#include "ECS/Components/Town.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Town/TownStats.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

// MapCoords.cpp / Town.cpp of runblack.exe W120 (TownPlacement.h)

namespace openblack::ecs::town_placement
{
using namespace components;

namespace
{
/// SetTownArea's start values (0x73AB00..0x73AB1B): min 0x7FFFFFFF, max 0
constexpr int32_t k_AreaMinStart = 0x7FFFFFFF;
constexpr int32_t k_AreaMaxStart = 0;
/// NewCollideDescriptor::Init 0x46AB10: reach = scale x mesh +0x30 + 1 (as map_cells' descriptor, MapCells.cpp)
constexpr float k_DescriptorReachAdd = 1.0f;

/// fn_73AC90 0x73AC90 (the town, an object): its GetRadius (vt +0x60, the 3D radius, not Get2DRadius) around its
/// position, in metres, widening the rectangle
void ExtendTownArea(Town& town, entt::entity object)
{
	const float r = object::GetRadius(object);
	const auto at = object::MapCoordsOf(object);
	// 0x73AC95..0x73AD3D on x: obj.x x 10 x 2^-16 (approximate: map_coords::ToMetres, one product) - r; below the
	// minimum in metres (test ah, 1) -> min.x = ftol(v x 65536 / 10)
	const auto widen = [r](int32_t position, int32_t& minimum, int32_t& maximum) {
		const float low = map_coords::ToMetres(position) - r;
		if (low < map_coords::ToMetres(minimum))
		{
			minimum = map_coords::ToFixedGUtils(low);
		}
		// position + r above the maximum (test ah, 0x41 jne: skipped when <=)
		const float high = map_coords::ToMetres(position) + r;
		if (high > map_coords::ToMetres(maximum))
		{
			maximum = map_coords::ToFixedGUtils(high);
		}
	};
	widen(at.x, town.areaMin.x, town.areaMax.x);
	// 0x73AD40..0x73ADFA: the same on z with +0x72C / +0x738
	widen(at.z, town.areaMin.y, town.areaMax.y);
}
} // namespace

void SetTownArea(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* t = registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr;
	if (t == nullptr)
	{
		return;
	}
	t->areaMin = {k_AreaMinStart, k_AreaMinStart};
	t->areaMax = {k_AreaMaxStart, k_AreaMaxStart};
	// 0x73AB32: the abodes (+0x754, next +0x9C); 0x73AB51: the fields (+0x780), which openblack keeps as abodes of
	// the town: AbodesOf has them once (a second pass over them would not move the rectangle)
	for (const auto abode : town_stats::AbodesOf(town))
	{
		ExtendTownArea(*t, abode);
	}
	// (then a camera point at +0x598 from the centre: not V6, not ported)
}

void SetAllTownAreas()
{
	map_cells::ForEachTown([](entt::entity town) {
		SetTownArea(town);
		return true;
	});
}

bool IsSuitableForFixed(const map_coords::MapCoords& pos, entt::id_type meshId, float yAngle, float scale)
{
	// 0x6038B0..0x603AF6: the static Game3DObject g_tmp (0xD38330) with the mesh, angle and scale at the position;
	// openblack builds its NewCollide shape directly (map_collide::FromMesh, the shape NewCollideDescriptor 0x46AAD0
	// takes). No mesh: g_tmp has no 3D object and 0x603B10 returns 0
	map_collide::Shape shape;
	if (!map_collide::FromMesh(meshId, map_coords::ToMetres(pos), yAngle, scale, shape))
	{
		return false;
	}
	const float reach = scale * object::MeshHalfDiagonal(meshId) + k_DescriptorReachAdd;
	// fn_604020 0x604037..0x604087: R = scale x max(mesh +0x24, +0x2C), the inline Get2DRadius
	const float radius = object::MeshRadius2D(meshId, scale);
	// GetNext 0x46AD80 returns NULL at a marked cell off the map and the loop then ends with 1 (0x46ADEC..0x46AE0A):
	// DescriptorCells is cut there the same way; fn_601E00 can never fail (dead test)
	for (const auto cell : map_cells::DescriptorCells(shape, reach))
	{
		// 0x6040A9..0x604125: the POSITION's landscape cell, re-tested for every descriptor cell: in 0..0x1FF, a block,
		// not water (lc +6 & 0x10). sea_cells::IsWater: no landscape cell is water (1) too
		if (!map_coords::InBounds(pos) || sea_cells::IsWater(map_coords::ToWorld(pos)))
		{
			return false;
		}
		// 0x60412E..0x604170: the cell's fixed list (+4, GetMapChild vt +0x53C), unfiltered; (+0x24 & 2) &&
		// IsSolidToNewAbode (vt +0x7D8) is exactly the MultiMapFixed class (V6_pending §1.3)
		for (auto obj = map_cells::FirstFixed(cell); obj != entt::null; obj = map_cells::GetMapChild(obj, cell))
		{
			if (!map_cells::IsMultiMapFixedClass(obj))
			{
				continue;
			}
			// fn_605CD0 -> GetDistanceInMetres 0x74CD70; Get2DRadius vt +0x64 (Field / FishFarm 5.0); reject when
			// R_obj + R > d (fcomp; test ah, 0x41; je): d == R_obj + R is accepted
			const float d = gutils::GetDistanceInMetres(pos, object::MapCoordsOf(obj));
			if (object::Get2DRadius(obj) + radius > d)
			{
				return false;
			}
		}
	}
	return true;
}

uint32_t IsSuitableForFixedAbodeInTown(const map_coords::MapCoords& pos, entt::id_type meshId, entt::entity town,
                                       float yAngle, float scale)
{
	// 0x603865: only with a town
	if (town != entt::null)
	{
		// 0x60387B GetNearestTown(&near, &dist, excluded = town, tribe = -1): the nearest OTHER town by the octagonal
		// cell distance and its rectangle's code (map_cells::GetNearestTownCells; the rectangles recomputed first)
		SetAllTownAreas();
		// 0x603880..0x60388C: code 1 -> 0xE, which 0x404B10 turns into "accepted" (literal: no fixed tests)
		if (map_cells::GetNearestTownCells(pos, town, std::nullopt).code == 1)
		{
			return k_InsideOtherTown;
		}
	}
	// 0x6038A0
	return IsSuitableForFixed(pos, meshId, yAngle, scale) ? 1u : 0u;
}

bool IsOkToCreateAtPos(const GAbodeInfo& info, const map_coords::MapCoords& pos, float yAngle, float scale,
                       entt::entity town)
{
	// 0x404B10: the info's mesh (vt +0x2C), != 0 (neg; sbb; neg)
	return IsSuitableForFixedAbodeInTown(pos, resources::HashIdentifier(info.meshId), town, yAngle, scale) != 0;
}
} // namespace openblack::ecs::town_placement
