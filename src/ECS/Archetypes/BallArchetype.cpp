/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BallArchetype.h"

#include <limits>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/TownPlaythings.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity BallArchetype::Create(const glm::vec3& position)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// a football is one of the world's objects: it takes the next creation number
	ecs::object_index::Assign(entity);
	const auto& info = Locator::infoConstants::value().ball;

	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mobile>(entity);
	registry.Assign<Ball>(entity);
	registry.Assign<Mesh>(entity, resources::HashIdentifier(info.meshId), static_cast<int8_t>(0), static_cast<int8_t>(1));
	// A ball no side plays with belongs to no one: the neutral player's call to come and play, unstamped
	ecs::effects::reactions::CreateReaction(entity, Reaction::ReactToBall, PlayerNames::NEUTRAL, false);
	// a mobile object: at the head of its cell's mobile list
	ecs::map_cells::InsertMapObject(entity);
	return entity;
}

entt::entity BallArchetype::CreateForNearestTown(const glm::vec3& position)
{
	const auto ball = Create(position);
	// any tribe, any abode, however far
	const auto town = ecs::map_cells::GetNearestTownToPos(map_coords::FromWorld(position), std::nullopt,
	                                                      ecs::map_cells::k_AnyAbodeType, std::numeric_limits<float>::max());
	if (town == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Ball Created but unable to add it to a town.");
		return ball;
	}
	ecs::town_playthings::GiveBall(Locator::entitiesRegistry::value(), town, ball);
	return ball;
}
