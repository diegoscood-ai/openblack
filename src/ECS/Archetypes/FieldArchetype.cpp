/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FieldArchetype.h"

#include <cstdlib>

#include <algorithm>

#include "AbodeArchetype.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Town.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownPlacement.h"
#include "ECS/Town/TownQueries.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity FieldArchetype::Create(int townId, const glm::vec3& position, FieldTypeInfo type, float yAngleRadians)
{
	auto& registry = Locator::entitiesRegistry::value();

	[[maybe_unused]] const auto& info = Locator::infoConstants::value().fieldType.at(static_cast<size_t>(type));

	// the field row of the town's tribe; a field of no town takes the Celtic field row
	const auto fieldTown = ecs::town_queries::TownByKey(static_cast<uint32_t>(townId));
	const auto abodeInfo =
	    fieldTown != entt::null ? GAbodeInfo::Find(registry.Get<Tribe>(fieldTown), AbodeNumber::Field) : AbodeInfo::CelticField;
	// (openblack, guard) no field row for the tribe: no field
	if (abodeInfo == AbodeInfo::None)
	{
		return entt::null;
	}

	// a field of no town stays of no town
	auto entity = AbodeArchetype::Create(townId, position, abodeInfo, yAngleRadians, 1.0f, 0, 0, false, false);
	if (entity == entt::null)
	{
		return entity;
	}
	auto& field = registry.Assign<Field>(entity, townId);
	// the field type info the field keeps
	field.type = type;
	// at the head of the town's fields, then the town area grows by the field's own radius (5.0), which the abode
	// part had as 0
	if (const auto town = ecs::town_queries::TownByKey(registry.Get<Abode>(entity).townId); town != entt::null)
	{
		town_placement::ExtendTownArea(town, entity);
	}
	// The original starts the field empty and the town's farmers sow it
	// a random turn offset, GameRand(10)
	field.turnOffset = static_cast<uint8_t>(game_random::GameRand(10));
	// test hook: every field starts at this growth, with the food it would have (OPENBLACK_TEST_FIELD_GROWTH=0..1200)
	if (const char* growth = std::getenv("OPENBLACK_TEST_FIELD_GROWTH"); growth != nullptr)
	{
		field.growth = std::clamp(std::strtof(growth, nullptr), 0.0f, Field::k_AgeRecolt);
		field.food = field.growth * Field::k_TotalFood / Field::k_AgeRecolt;
	}
	// (openblack) AbodeArchetype::Create inserted it with an Abode's type; the field's (FIELD) comes with the Field
	// component: in again, at the head of the same cells (nothing came in between), which is the original's single
	// insert into the map
	ecs::map_cells::RemoveMapObject(entity);
	ecs::map_cells::InsertMapObject(entity);

	return entity;
}
