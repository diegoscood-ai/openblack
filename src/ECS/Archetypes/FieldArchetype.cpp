/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FieldArchetype.h"

#include "AbodeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Town.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Registry.h"
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
	// The constructor (0x527DD0) starts empty and the town's farmers sow it; openblack has no farmers yet, so the
	// fields start sown and ripe (as they look when a land starts)
	field.crops = Field::k_TimesToSow;
	field.growth = Field::k_AgeRecolt;
	field.food = Field::k_TotalFood;
	field.turnOffset = static_cast<uint8_t>(Locator::rng::value().NextValue<int>(0, 9));

	return entity;
}
