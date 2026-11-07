/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TreeArchetype.h"

#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unlit.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectPlacement.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/TreeAngles.h"
#include "ECS/Trees.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Utils.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

TreeArchetype::Growth TreeArchetype::NewGrowth(TreeInfo type, float maxSize, float scale)
{
	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(type));
	const bool growing = maxSize != scale;
	return {.growing = growing,
	        .counter = static_cast<uint16_t>(growing ? game_random::GameRand(info.growsAfterNumGameTurns) : 0u)};
}

entt::entity TreeArchetype::Create(uint32_t forestId, const glm::vec3& position, TreeInfo type, bool isNonScenic,
                                   float yAngleRadians, float maxSize, float scale)
{
	return Create(forestId, position, type, isNonScenic, yAngleRadians, maxSize, scale, NewGrowth(type, maxSize, scale));
}

entt::entity TreeArchetype::Create(uint32_t forestId, const glm::vec3& position, TreeInfo type, bool isNonScenic,
                                   float yAngleRadians, float maxSize, float scale, Growth growth)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);

	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(type));

	// the angle it keeps: within a turn of zero, on its own side
	const float yAngle = tree_angles::StoredYAngle(yAngleRadians);
	const auto& transform = registry.Assign<Transform>(entity, position, affine::AngleY(yAngle), glm::vec3(scale));
	// its model's own placement, made upright as the game makes it at the creation (ECS/ObjectPlacement.h)
	ecs::object_placement::PlaceYaw(registry, entity, position, yAngle, scale);
	const auto [point, radius] = GetFixedObstacleBoundingCircle(info.normal, transform);
	registry.Assign<Fixed>(entity, point, radius);
	// the wind slot comes from the angle, so that trees facing the same way sway together
	const auto slot = tree_angles::WindSlotOf(yAngle);
	registry.Assign<Tree>(entity, type, maxSize, forestId, isNonScenic, growth.growing, growth.counter, slot);
	// a tree is not shaded by the model light; a dead tree made from it keeps that
	registry.Assign<Unlit>(entity);
	const auto resourceId = resources::HashIdentifier(info.normal);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(-1));
	// at the head of its cell's fixed list
	ecs::map_cells::InsertMapObject(entity);

	// the thing keeps the angle it was placed at
	placed_turn::KeepAngles(registry, entity, glm::vec3(0.0f, yAngle, 0.0f));
	return entity;
}
