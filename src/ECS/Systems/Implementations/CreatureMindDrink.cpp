/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/LandAvoid.h"
#include "3D/LandIslandInterface.h"
#include "Creature/CreatureDrink.h"
#include "Creature/CreatureExplorationMap.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Components/MapSimData.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace land = openblack::ecs::creature_exploration_land;

std::optional<creature_drink::Place> ecs::systems::mind_detail::DrinkPlaceOf(entt::entity creature)
{
	if (!Locator::terrainSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	const glm::vec2 from {transform->position.x, transform->position.z};
	const auto& terrain = Locator::terrainSystem::value();
	// The point itself when the creature can stand there, else the nearest point where it can stand, if near enough
	const auto standAt = [&terrain](glm::vec2 wanted) {
		const glm::vec3 at {wanted.x, terrain.GetHeightAt(wanted), wanted.y};
		const bool valid = land_avoid::IsPosValid(at, land_avoid::k_CreatureRadius);
		const auto found =
		    valid ? std::nullopt
		          : land_avoid::NearestValid(wanted, land_avoid::k_CreatureRadius, creature_drink::k_StandSpiralReach);
		return creature_drink::StandingPoint(at, valid, found);
	};

	// The water points, in the order that the land made them
	std::vector<std::pair<uint32_t, glm::vec2>> marked;
	registry.Each<const DrinkWaypoint>([&marked](const DrinkWaypoint& point) {
		marked.emplace_back(point.number, glm::vec2(point.position.x, point.position.z));
	});
	std::ranges::sort(marked, {}, &std::pair<uint32_t, glm::vec2>::first);
	std::vector<glm::vec2> points;
	points.reserve(marked.size());
	std::ranges::transform(marked, std::back_inserter(points), &std::pair<uint32_t, glm::vec2>::second);
	if (const auto water = creature_drink::NearestWaterPoint(from, points))
	{
		const auto stand = standAt(*water);
		if (!stand.has_value())
		{
			return std::nullopt;
		}
		return creature_drink::Place {.walkTo = *stand};
	}

	// With no water point near, the nearest coast of its map and the sea nearest to that coast. Where the map has no
	// such place, the creature uses the low corner of the map, as in the game.
	constexpr glm::vec2 k_Corner {0.0f};
	const auto coast = land::FindPlace(creature, creature_exploration::Kind::Coast, from, false).value_or(k_Corner);
	creature_drink::Place place {
	    .sea = land::FindPlace(creature, creature_exploration::Kind::Sea, coast, false).value_or(k_Corner)};
	// A creature that stands in the water already only turns to the sea
	if (!ecs::sea_cells::IsWater(transform->position))
	{
		const auto stand = standAt(coast);
		if (!stand.has_value())
		{
			return std::nullopt;
		}
		place.walkTo = *stand;
	}
	return place;
}
