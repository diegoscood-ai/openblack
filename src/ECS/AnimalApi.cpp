/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include "Common/GameDistance.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/AnimalAnimations.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/WalkArrival.h"
#include "InfoConstants.h"
#include "Locator.h"

/// The animals' functions the spells and scripts use (ECS/AnimalAI.h).
namespace openblack::ecs::animal_ai
{
using components::Alpha;
using components::Animal;
using components::AnimalBrain;
using components::Transform;

entt::entity CreateAnimal(const glm::vec3& position, AnimalInfo type, entt::entity flock, uint32_t age, int32_t player)
{
	const auto entity = archetypes::AnimalArchetype::Create(position, type, entt::null, flock, age);
	if (entity != entt::null)
	{
		Locator::entitiesRegistry::value().Get<Animal>(entity).player = player;
	}
	return entity;
}

void MoveTo(entt::entity entity, glm::vec2 position, float altitude, AnimalState final)
{
	auto* brain = detail::BrainOf(entity);
	if (brain == nullptr)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& animal = registry.Get<Animal>(entity);
	detail::Context ctx {entity, animal, *brain, registry.Get<Transform>(entity), detail::InfoOf(animal)};
	brain->goalAltitude = altitude;
	detail::SetupMoveToPos(ctx, position, final);
}

void SetState(entt::entity entity, AnimalState state)
{
	if (auto* brain = detail::BrainOf(entity); brain != nullptr)
	{
		detail::SetTopState(entity, *brain, state);
	}
}

void SetStateRaw(entt::entity entity, AnimalState state)
{
	if (auto* brain = detail::BrainOf(entity); brain != nullptr)
	{
		brain->topState = static_cast<uint8_t>(state);
		brain->turnsSinceStateChange = 0;
	}
}

void SetFinalDestination(entt::entity entity, glm::vec2 position)
{
	if (auto* brain = detail::BrainOf(entity); brain != nullptr)
	{
		brain->finalDestination = position;
	}
}

std::optional<glm::vec3> Destination(entt::entity entity)
{
	const auto* brain = detail::BrainOf(entity);
	if (brain == nullptr)
	{
		return std::nullopt;
	}
	return glm::vec3(brain->goal.x, brain->goalAltitude, brain->goal.y);
}

void SetSpeedInMetresPerSecond(entt::entity entity, float metresPerSecond)
{
	SetSpeedInMetres(entity, metresPerSecond / 10.0f);
}

uint16_t GameAngle(entt::entity entity)
{
	const auto* brain = detail::BrainOf(entity);
	return brain != nullptr ? brain->angle : uint16_t {0};
}

void TurnTowards(entt::entity entity, uint16_t angle)
{
	auto* brain = detail::BrainOf(entity);
	if (brain == nullptr)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& animal = registry.Get<Animal>(entity);
	detail::Context ctx {entity, animal, *brain, registry.Get<Transform>(entity), detail::InfoOf(animal)};
	// the turning circle is measured to the target it has, else to its goal
	const auto at = object::MapCoordsOf(entity);
	const auto towards = brain->target != entt::null && registry.Valid(brain->target) ? object::MapCoordsOf(brain->target)
	                                                                                  : map_coords::FromMetres(brain->goal);
	detail::SetTowardsAngle(ctx, angle, game_math::GetDistanceInMetres(at, towards));
	detail::FaceAngle(ctx.transform, brain->angle);
	registry.SetDirty();
}

bool AreWeThere(entt::entity entity, float extra)
{
	const auto* brain = detail::BrainOf(entity);
	if (brain == nullptr)
	{
		return false;
	}
	return walk_arrival::AreWeThere(object::MapCoordsOf(entity), map_coords::FromMetres(brain->goal), brain->speed, extra);
}

void SetSpeciesDying(AnimalInfo type, SpeciesDying dying)
{
	detail::Shared().SetSpeciesDying(static_cast<size_t>(type), std::move(dying));
}

void Kill(entt::entity entity)
{
	if (auto* brain = detail::BrainOf(entity); brain != nullptr)
	{
		detail::SetDying(entity, *brain);
	}
}

void Remove(entt::entity entity)
{
	if (ecs::IsAvailable(entity))
	{
		detail::Delete(entity);
	}
}

bool IsFlyingSpecies(AnimalInfo type)
{
	return detail::IsBird(type);
}

bool IsDoveClass(AnimalInfo type)
{
	return IsFlyingSpecies(type) || type == AnimalInfo::Vulture;
}

void SetAlpha(entt::entity entity, float alpha)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return;
	}
	if (alpha >= 1.0f)
	{
		registry.Remove<Alpha>(entity);
	}
	else
	{
		registry.AssignOrReplace<Alpha>(entity, alpha);
	}
	registry.SetDirty();
}

} // namespace openblack::ecs::animal_ai
