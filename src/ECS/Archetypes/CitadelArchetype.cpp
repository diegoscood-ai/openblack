/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CitadelArchetype.h"

#include <algorithm>
#include <cmath>

#include <spdlog/spdlog.h>

#include <entt/fwd.hpp>

#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "3D/LandIslandInterface.h"
#include "Locator.h"
#include "Worship/Citadel.h"
#include "Worship/SpecialPoints.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
/// CitadelHeart's creation (0x882730) flattens the land under the temple: the 17 x 17 cell corners around its cell
/// take that cell's altitude, all of it within 35 units of the centre and blending back to their own by 70
/// (fn_008826C0), truncated to whole altitude units. The temple keeps the y it had before (and stays rigid once
/// built: the finished temple is drawn static, 0x882A40).
void FlattenLandUnderTemple(const glm::vec3& position)
{
	if (!Locator::terrainSystem::has_value() || Locator::terrainSystem::value().GetMaterialInfo().empty())
	{
		return; // no island loaded (tests)
	}
	auto& island = Locator::terrainSystem::value();
	const glm::ivec2 centre(static_cast<int>(position.x * 0.1f), static_cast<int>(position.z * 0.1f));
	const int last = island.GetCellsPerSide() - 1;
	if (centre.x < 0 || centre.y < 0 || centre.x > last || centre.y > last)
	{
		return;
	}
	const auto centreAltitude = static_cast<float>(island.GetCellAltitude(island.GetCell(glm::u16vec2(centre))));
	int changed = 0;
	float largest = 0.0f;
	for (int dx = -8; dx <= 8; ++dx)
	{
		for (int dz = -8; dz <= 8; ++dz)
		{
			const auto cell = centre + glm::ivec2(dx, dz);
			if (cell.x < 0 || cell.y < 0 || cell.x > last || cell.y > last)
			{
				continue;
			}
			const float distance = 10.0f * std::sqrt(static_cast<float>(dx * dx + dz * dz));
			const float t = distance < 35.0f ? 0.0f : distance > 70.0f ? 1.0f : (distance - 35.0f) / 35.0f;
			const auto altitude = static_cast<float>(island.GetCellAltitude(island.GetCell(glm::u16vec2(cell))));
			const auto flattened = static_cast<uint16_t>(std::max(0, static_cast<int>(altitude * t + centreAltitude * (1.0f - t))));
			if (static_cast<float>(flattened) != altitude)
			{
				island.SetCellAltitude(glm::u16vec2(cell), flattened);
				++changed;
				largest = std::max(largest, std::abs(static_cast<float>(flattened) - altitude));
			}
		}
	}
	island.RebuildAltitudes();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Temple at ({:.0f}, {:.0f}): land flattened to {} ({} cells changed, up to {} units)",
	                    position.x, position.z, centreAltitude, changed, largest);
}
} // namespace

entt::entity CitadelArchetype::Create(const glm::vec3& position, PlayerNames playerOwner, const glm::mat4& rotation,
                                      const glm::vec3& size)
{
	FlattenLandUnderTemple(position);

	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, rotation, size);
	registry.Assign<Temple>(entity, playerOwner);
	const auto meshId = entt::hashed_string("temple/b_first_temple_l3d");
	registry.Assign<Mesh>(entity, meshId, static_cast<int8_t>(0), static_cast<int8_t>(0));
	// CitadelHeart's creation also gives the citadel its worship part: six free slots around the heart's Y angle, which
	// Citadel::AddTown fills with a WorshipSite per tribe (Worship/Citadel.cpp). (inferido, openblack deviation) A
	// planned citadel gets it too, because openblack draws it as a finished temple and has no building sites.
	worship::citadel::Initialise(entity, worship::YAngleOf(glm::mat3(rotation)));
	// CitadelHeart's CallVirtualFunctionsForCreation (MultiMapFixed 0x52E890+0x184): InsertMapObject (vt +0x544,
	// 0x52E650). (aproximado) its own collide shape (CreateCollideData 0x468FB0) is not ported: map_cells takes the
	// mesh's. (inferido) after the worship part above: their order is not read
	ecs::map_cells::InsertMapObject(entity);
	return entity;
}

/// CREATE_PLANNED_CITADEL (0x715E91) makes, in the original, only a PlannedTownCitadelHeart on the town's planned list
/// (invisible, no land flattening). The temple appears when the plan is converted
/// (PlannedTownCitadelHeart::CreatePlannedNoFixedCheck 0x467EF0) by BUILD_BUILDING in Land 1 or by the villagers'
/// civic building requests in the other lands, at 0 % built and then built by the villagers. None of that exists in
/// openblack yet (town desires, building sites, partial drawing), so, not to lose the temple, it is made here already
/// built. What does follow the conversion: the owner is the town's player (town->GetPlayer), not the script's, and the
/// temple is drawn at scale 1 (CallVirtualFunctionsForCreation 0x4675A0 pushes 1.0; the plan's scale is only the
/// heart's).
entt::entity CitadelArchetype::CreatePlan(entt::entity town, const glm::vec3& position, const glm::mat4& rotation)
{
	const auto owner = Locator::entitiesRegistry::value().Get<Town>(town).owner;
	return Create(position, owner, rotation, glm::vec3(1.0f));
}
