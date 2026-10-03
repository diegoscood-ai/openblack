/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FieldArchetype.h"

#include <algorithm>
#include <cstdlib>

#include "AbodeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Town.h"
#include "Common/GameRandom.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "EngineConfig.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity FieldArchetype::Create(int townId, const glm::vec3& position, FieldTypeInfo type, float yAngleRadians)
{
	auto& registry = Locator::entitiesRegistry::value();

	[[maybe_unused]] const auto& info = Locator::infoConstants::value().fieldType.at(static_cast<size_t>(type));

	auto townTribe = registry.Get<Tribe>(registry.Context().towns[townId]);
	auto abodeInfo = GAbodeInfo::Find(townTribe, AbodeNumber::Field);

	auto entity = AbodeArchetype::Create(townId, position, abodeInfo, yAngleRadians, 1.0f, 0, 0);
	if (entity == entt::null)
	{
		return entity;
	}
	auto& field = registry.Assign<Field>(entity, townId);
	// The constructor (0x527DD0) starts the field empty and the town's farmers sow it. Mod world.crops (no villager
	// jobs yet): it starts sown and ripe, as the fields look when a land starts.
	if (Locator::config::value().fieldsWithoutFarmers)
	{
		field.crops = Field::k_TimesToSow;
		field.growth = Field::k_AgeRecolt;
		field.food = Field::k_TotalFood;
	}
	// the ctor: GameRand(10) at 0x527E90 into +0x11C
	field.turnOffset = static_cast<uint8_t>(game_random::GameRand(10));
	// test hook: every field starts at this growth, with the food it would have (OPENBLACK_TEST_FIELD_GROWTH=0..1200)
	if (const char* growth = std::getenv("OPENBLACK_TEST_FIELD_GROWTH"); growth != nullptr)
	{
		field.growth = std::clamp(std::strtof(growth, nullptr), 0.0f, Field::k_AgeRecolt);
		field.food = field.growth * Field::k_TotalFood / Field::k_AgeRecolt;
	}
	// (openblack) AbodeArchetype::Create inserted it with an Abode's type; the field's (GFieldTypeInfo, 0x12 FIELD)
	// comes with the Field component: in again, at the head of the same cells (nothing came in between), which is the
	// original's single InsertMapObject (Field's CallVirtualFunctionsForCreation, MultiMapFixed 0x52E890+0x184)
	ecs::map_cells::RemoveMapObject(entity);
	ecs::map_cells::InsertMapObject(entity);

	return entity;
}
