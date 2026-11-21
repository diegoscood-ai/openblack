/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerInteract.h"

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "Common/GameAngle.h"
#include "Common/GameDistance.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerScript.h"
#include "Enums.h"
#include "Locator.h"

namespace openblack::ecs::villager
{
namespace
{
/// Where an object stands, as a map position (x and z; the altitude 0)
map_coords::MapCoords MapPosOf(entt::entity object)
{
	const auto pos = town_queries::PosOf(object);
	return {pos.x, pos.y, 0.0f};
}
} // namespace

void MakeVillagerFaceObject(entt::entity villager, entt::entity object)
{
	const float angle = town_queries::Get3DAngleFromXZ(town_queries::PosOf(villager), town_queries::PosOf(object));
	SetGameAngle(villager, static_cast<uint16_t>(game_math::ConvertAngle3DToGame(angle)));
}

bool MakeVillagesMeet(entt::entity villager, entt::entity other, VillagerStates state, float distance)
{
	// Close enough: they turn to each other and the first goes on
	if (object::IsTouching(villager, other, distance))
	{
		MakeVillagerFaceObject(villager, other);
		MakeVillagerFaceObject(other, villager);
		SetTopState(villager, state);
		return true;
	}
	// Else each walks towards the other, to stop a gap short of halfway
	const auto here = game_math::GetMidPoint(MapPosOf(villager), MapPosOf(other), distance);
	SetupMoveToPos(villager, map_coords::ToMetres(here), state);
	const auto there = game_math::GetMidPoint(MapPosOf(other), MapPosOf(villager), distance);
	SetupMoveToPos(other, map_coords::ToMetres(there), state);
	SetTargetThing(other, villager);
	SetTargetThing(villager, other);
	return false;
}

} // namespace openblack::ecs::villager
