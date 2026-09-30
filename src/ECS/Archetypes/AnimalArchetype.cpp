/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimalArchetype.h"

#include <glm/vec3.hpp>

#include "3D/LandIslandInterface.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ObjectCreationIndex.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity AnimalArchetype::Create(const glm::vec3& position, AnimalInfo type, int32_t flock, uint32_t age)
{
	if (type == AnimalInfo::None || type >= AnimalInfo::_COUNT)
	{
		return entt::null;
	}
	const auto& info = Locator::infoConstants::value().animal.at(static_cast<size_t>(type));
	// Birds and bats fly at altitudeNormal above the ground; without their flight they would stand on it
	if (info.altitudeNormal != 0.0f)
	{
		return entt::null;
	}
	auto& rng = Locator::rng::value();
	if (age == 0)
	{
		age = rng.NextValue<uint32_t>(0, 19) + 5;
	}
	// InitialiseScale 0x417B20 / SetScaleForAge 0x417A40 (K = 0.75): young ones take their age's scale and a random part
	// of the step to the next; adults 1.05 - FloatRand(0.1)
	float scale;
	const auto& ageToScale = info.ageToScale.values;
	if (age < info.grownUpAge && age >= 1 && age + 1 < ageToScale.size())
	{
		scale = ageToScale[age - 1];
		const float step = 0.75f * (ageToScale[age + 1] - scale);
		scale += step > 0.0f ? rng.NextValue<float>(0.0f, step) : 0.0f;
	}
	else
	{
		scale = 1.05f - rng.NextValue<float>(0.0f, 0.1f);
	}

	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	// no start angle in the Object / Living constructors; standing on the land
	glm::vec3 ground = position;
	if (Locator::terrainSystem::has_value())
	{
		ground.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	}
	registry.Assign<Transform>(entity, ground, glm::mat3(1.0f), glm::vec3(scale));
	registry.Assign<Mobile>(entity);
	registry.Assign<Animal>(entity, type, age, flock, true);
	// LOD 1 of the three detail meshes (the LevelOfDetail loads are NOPed: always the high one)
	registry.Assign<Mesh>(entity, resources::HashIdentifier(info.high), static_cast<int8_t>(0), static_cast<int8_t>(0));
	return entity;
}
