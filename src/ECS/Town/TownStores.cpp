/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownStores.h"

#include <optional>

#include "ECS/Components/Town.h"
#include "ECS/GUtilsAngle.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/TownQueries.h"
#include "Locator.h"
#include "Magic/Objects/MagicPiles.h"

namespace openblack::ecs::town_stores
{
TemporaryStore GetTemporaryResourceStorePotOrPos(entt::entity town, const map_coords::MapCoords& from, ResourceType type)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* t = town != entt::null && registry.Valid(town) ? registry.TryGet<components::Town>(town) : nullptr;
	if (t == nullptr || (type != ResourceType::Food && type != ResourceType::Wood))
	{
		return {entt::null, from};
	}
	auto& pot = t->temporaryPots.at(static_cast<size_t>(type));
	// 0x73E90D..0x73E91F: +0x600[type] != NULL && its IsAvailable (vt +0x2C, GameThing 0x401810)
	if (pot == entt::null || !abode_queries::IsAvailable(pot))
	{
		// 0x73E95F..0x73E999: GetCongregationPos 0x7408B0, += GetPosFromAngle 0x74D580(0, WOOD ? 5 : 0) (operator+=
		// 0x605410). (approximate) town_queries::GetCongregationPos gives x / z only: the cache's altitude (+0xF18) is
		// dropped, the pile stands on the land anyway
		const auto congregation = town_queries::GetCongregationPos(town);
		map_coords::MapCoords pos {congregation.x, congregation.y, 0.0f};
		pos += gutils::GetPosFromAngle(0.0f, type == ResourceType::Wood ? k_WoodPotOffset : 0.0f);
		// 0x73E99E..0x73E9D4: FindClearArea 0x7412F0(pos, pos, 45, 1.5, 2, 0x73EA50, NULL): result and start are the same
		// local; the filter is Flags +0x24 bit 1, the MultiMapFixed bit. The bool it returns is not tested
		glm::ivec2 clear {pos.x, pos.z};
		town_queries::FindClearArea(clear, clear, k_ClearAreaA, k_ClearAreaB, k_ClearAreaRadius,
		                            map_cells::IsMultiMapFixedClass, entt::null);
		pos.x = clear.x;
		pos.z = clear.y;
		// 0x73E9D9..0x73EA11: Pot::Create 0x66CF10(pos, GPotInfo 0xD4C660 + (FOOD ? 10 : 9) x 0x144, 0, NULL, this, 0,
		// 0, 1, 0): for those two infos the MagicFood 0x5FA9F0 / MagicWood 0x600E20 constructors (player NULL, amount
		// 0; the town is not passed on), then CallVirtualFunctionsForCreation (vt +0x658); kept in +0x600[type]
		pot = magic::objects::CreateMagicResourcePile(map_coords::ToWorld(pos), std::nullopt, type, 0, true);
		if (pot == entt::null)
		{
			// (openblack) no pile could be made (PotArchetype has no info for it): the point itself
			return {entt::null, pos};
		}
	}
	// 0x73E921..0x73E94E / 0x73EA0C..0x73EA34: out = pot.GetNearestEdgeToPos(from) (vt +0x83C, Object 0x636DA0)
	return {pot, object::GetNearestEdgeToPos(pot, from)};
}
} // namespace openblack::ecs::town_stores
