/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LivingAngle.h"

#include <cmath>

#include <algorithm>
#include <utility>

#include "3D/CreatureBody.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Common/GUtilsDistance.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectPlacement.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/VillagerSpeed.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs::living
{

void SetYAngle(entt::entity entity, float radians)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<components::Villager>(entity))
	{
		villager::SetYAngle(entity, radians);
	}
	else if (registry.AllOf<components::Animal>(entity))
	{
		animal_ai::SetYAngle(entity, radians);
	}
}

bool SetFocus(entt::entity entity, const glm::vec3& target)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const components::Transform>(entity);
	if (transform == nullptr || !registry.AnyOf<components::Villager, components::Animal>(entity))
	{
		return false;
	}
	// its position in map coordinates, in metres; the angle does not read y. (inferred) openblack's Transform position
	// stands for it, through map coordinates
	const glm::vec3 from(map_coords::Quantise(transform->position.x), transform->position.y,
	                     map_coords::Quantise(transform->position.z));
	// the angle from the position to the target, rounded to a float, then SetYAngle
	SetYAngle(entity, static_cast<float>(affine::GetYAngleBetween(from, target)));
	return true;
}

std::optional<float> SpeedProperty(entt::entity entity)
{
	// in the physics is the InPhysics mark: a moving body, a carried thing or one left hanging, not a body come to rest
	return SpeedProperty(entity, std::as_const(Locator::entitiesRegistry::value()).AllOf<components::InPhysics>(entity),
	                     Locator::dynamicsSystem::value().Find(entity));
}

std::optional<float> SpeedProperty(entt::entity entity, bool inPhysics, const PhysicsEntry* po)
{
	// any object in the physics with its PhysicsEntry
	if (inPhysics && po != nullptr)
	{
		const auto& v = po->body.velocity;
		return std::sqrt((v.z * v.z + v.y * v.y) + v.x * v.x);
	}
	auto& registry = Locator::entitiesRegistry::value();
	const bool villager = registry.AllOf<components::Villager>(entity);
	const bool animal = registry.AllOf<components::Animal>(entity);
	if (!villager && !animal)
	{
		const auto* body = std::as_const(registry).TryGet<const components::Creature>(entity);
		if (body == nullptr)
		{
			return std::nullopt;
		}
		// a creature, in the physics without a PhysicsEntry or out of it: openblack keeps no dead creature
		const auto& kinds = Locator::infoConstants::value().creature;
		const auto row = openblack::creature::InfoRow(body->species);
		return CreatureMadeSpeedInMetres(row < kinds.size() ? static_cast<int32_t>(kinds.at(row).speedGroup.speedDefault) : 0);
	}
	const auto speedInMetres = [&]() { return villager ? VillagerSpeedInMetres(entity) : animal_ai::GetSpeedInMetres(entity); };
	if (inPhysics)
	{
		return speedInMetres(); // no dead test
	}
	// dead -> 0
	const bool dead = villager ? villager::IsDead(entity) : animal_ai::IsDead(entity);
	return dead ? 0.0f : speedInMetres();
}

float CreatureMadeSpeedInMetres(int32_t wholeSpeed)
{
	// stored as an unsigned 16-bit speed: below 0 is 0, above 65535 is 65535
	return gutils::ConvertWholeDistanceToMeters(std::clamp(wholeSpeed, 0, 0xFFFF));
}

std::optional<float> AgeProperty(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<components::Villager>(entity))
	{
		return static_cast<float>(villager::GetAge(entity));
	}
	if (registry.AllOf<components::Animal>(entity))
	{
		return static_cast<float>(animal_ai::GetAge(entity));
	}
	return std::nullopt;
}

bool SetThingScale(entt::entity entity, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* transform = registry.Valid(entity) ? registry.TryGet<components::Transform>(entity) : nullptr;
	if (transform == nullptr)
	{
		return false;
	}
	if (transform->scale != glm::vec3(scale))
	{
		transform->scale = glm::vec3(scale);
		// its model is placed again at the new scale
		object_placement::PlaceAgain(registry, entity);
		map_cells::OnAnglesOrScaleChanged(entity);
	}
	return true;
}

} // namespace openblack::ecs::living
