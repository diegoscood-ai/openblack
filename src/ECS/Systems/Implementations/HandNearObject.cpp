/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The object near the action's point when the click hit none (GInterface::FindObjectNearMapCoord fn_005D39E0) and the
// point it searches around, the land behind the hand seen from the camera (GLandscape::Draw 0x5E4848..0x5E4870).
// Research: dev\documentacion\hand\fonmc\README.md. Wiki: docs/bw1-notes/hand-and-interface.md, "Object under the
// cursor".

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"
#include "HandSystemDetail.h"

#include <limits>

#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Transform.h"
#include "ECS/FishShoals.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

void HandSystem::UpdatePointBehindHand() noexcept
{
	// GLandscape::Draw 0x5E4848..0x5E4870, once a drawn frame: the box (min / max) of the hand's bones 1..n-1 (its
	// TransformedMatrices +0x80, element 1 first: 0x5E46DC..0x5E47CD, the root left out), the camera's ray through its
	// centre onto the land (LH3DIsland::RayCast fn_00802550, with the plane y = 0 within 7500 m when the land is missed);
	// g_0xD2017C = hit, g_0xD1A3A0 = (x, 0, z)
	_pointBehindHand.reset();
	const auto* bones = GetBoneMatrices();
	if (bones == nullptr || bones->empty() || !Locator::camera::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const glm::mat4 model = GetHandMatrix();
	glm::vec3 lo(std::numeric_limits<float>::max());
	glm::vec3 hi(std::numeric_limits<float>::lowest());
	for (size_t i = 1; i < bones->size(); ++i)
	{
		const glm::vec3 joint = glm::vec3(model * (*bones)[i][3]);
		lo = glm::min(lo, joint);
		hi = glm::max(hi, joint);
	}
	const glm::vec3 centre = (lo + hi) * 0.5f;
	const glm::vec3 camera = Locator::camera::value().GetOrigin();
	glm::vec2 hit;
	if (Locator::terrainSystem::value().RayCast(camera, centre, hit, camera))
	{
		_pointBehindHand = glm::vec3(hit.x, 0.0f, hit.y);
	}
}

std::optional<entt::entity> HandSystem::FindObjectNearMapCoord(glm::vec3 at) const noexcept
{
	namespace map_coords = ecs::map_coords;
	// fn_005D39E0: nothing without the point behind the hand (g_0xD2017C) or with it at (0, 0, 0)
	if (!_pointBehindHand)
	{
		return std::nullopt;
	}
	// MapCoords c(LHPoint 0xD1A3A0): x, z = ftol(m x 6553.6), altitude = 0 - GetAltitude (so 0 only on a cell at
	// height 0)
	const glm::vec3 c = *_pointBehindHand;
	if (map_coords::ToFixed(c.x) == 0 && map_coords::ToFixed(c.z) == 0 &&
	    Locator::terrainSystem::value().GetHeightAt(glm::vec2(c.x, c.z)) == 0.0f)
	{
		return std::nullopt;
	}
	// c on the water: the shoal near the action's point (fn_00824B10 on this+0x3F0, xz distance^2 < 4), then the fish
	// farm of that shoal (g_game +0x205C0C, +0x88 == shoal). (approximate) ecs::FindFishFarmAt walks the farms, not the
	// shoal list then the farm: they only differ where shoals overlap
	// (inferred) !IsLand stands for MapCoords::IsWater 0x6035B0 (0x5D3A2F)
	if (!hand_detail::IsLand(c))
	{
		if (const auto farm = ecs::FindFishFarmAt(at); farm)
		{
			return farm;
		}
	}
	// the cells of the square +-5 m (0x40A00000 at 0x5D3B1B) around c's MapCoords value (fild x 10 / 65536,
	// 0x5D3AC9..0x5D3B9C; ftol((c -+ 5) x 65536 / 10), signed high words), x outer, z inner, the fixed list then the
	// mobile one; every object but the fragments (0x5D3C29), at its 2D distance from c (GUtils::GetDistanceInMetres
	// 0x74CD70), the nearest under 5 ([0x8AB6E4]; strict, the first of a tie)
	const auto& registry = Locator::entitiesRegistry::value();
	const float cx = map_coords::Quantise(c.x);
	const float cz = map_coords::Quantise(c.z);
	const int16_t minX = map_coords::SignedCellOf(map_coords::ToFixedGUtils(cx - 5.0f));
	const int16_t maxX = map_coords::SignedCellOf(map_coords::ToFixedGUtils(cx + 5.0f));
	const int16_t minZ = map_coords::SignedCellOf(map_coords::ToFixedGUtils(cz - 5.0f));
	const int16_t maxZ = map_coords::SignedCellOf(map_coords::ToFixedGUtils(cz + 5.0f));
	std::optional<entt::entity> best;
	float bestDistance = 5.0f;
	for (int32_t i = minX; i <= maxX; ++i)
	{
		for (int32_t j = minZ; j <= maxZ; ++j)
		{
			const glm::ivec2 cell(i, j);
			if (!map_coords::InBounds(cell))
			{
				continue;
			}
			ecs::map_cells::ForEachInCell(cell, [&](entt::entity object) {
				if (!registry.Valid(object) || registry.AllOf<Fragment>(object))
				{
					return true;
				}
				const auto* transform = registry.TryGet<const Transform>(object);
				if (transform == nullptr)
				{
					return true;
				}
				const float distance = gutils::GetDistanceInMetres(c, transform->position);
				if (distance < bestDistance)
				{
					bestDistance = distance;
					best = object;
				}
				return true;
			});
		}
	}
	// g_0xBF3578 = the distance from the action's point to c: the nearest counts when it is not farther (<=)
	if (best && bestDistance <= gutils::GetDistanceInMetres(at, c))
	{
		return best;
	}
	return std::nullopt;
}
