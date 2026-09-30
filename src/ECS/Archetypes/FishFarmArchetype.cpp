/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FishFarmArchetype.h"

#include <array>
#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ObjectCreationIndex.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity FishFarmArchetype::Create(const glm::vec3& position)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& island = Locator::terrainSystem::value();
	auto& rng = Locator::rng::value();

	const float y = island.GetHeightAt(glm::vec2(position.x, position.z));
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	registry.Assign<Transform>(entity, glm::vec3(position.x, y, position.z), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& farm = registry.Assign<FishFarm>(entity);

	// Rings of radius 2, 4, ... < 50, 32 directions each: the first direction with sea (altitude 0) at two radii in a
	// row gives the shoal centre, at the second of them
	std::array<uint8_t, 32> seaCount {};
	for (float radius = 2.0f; radius < 50.0f; radius += 2.0f)
	{
		for (size_t i = 0; i < seaCount.size(); ++i)
		{
			const float angle = static_cast<float>(i) * glm::two_pi<float>() * 0.03125f;
			const glm::vec2 point(position.x + std::cos(angle) * radius, position.z + std::sin(angle) * radius);
			// with the sea flattening off ([0xC37BF4] = 0 during the search, 0x52CCD4)
			if (island.GetUnflattenedHeightAt(point) != 0.0f)
			{
				seaCount[i] = 0;
				continue;
			}
			if (++seaCount[i] == 2)
			{
				FishShoal shoal;
				shoal.centre = glm::vec3(point.x, y, point.y);
				shoal.target = shoal.centre;
				// fn_00824740
				for (auto& fish : shoal.fish)
				{
					fish.halfSize = rng.NextValue(0.8f, 1.2f);
					fish.position = shoal.centre + glm::vec3(rng.NextValue(-5.0f, 5.0f), rng.NextValue(-1.0f, 0.0f),
					                                         rng.NextValue(-5.0f, 5.0f));
					fish.heading = rng.NextValue(-glm::pi<float>(), glm::pi<float>());
					fish.speed = rng.NextValue(0.5f, 1.5f);
					fish.turnRate = fish.speed * (1.0f + rng.NextValue(-0.1f, 0.1f)) * 0.6283f;
					fish.frame = 0.0f;
					fish.fleeTime = 0.0f;
				}
				farm.shoal = shoal;
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fish farm at ({}, {}): shoal at ({}, {}, {})", position.x, position.z,
				                    shoal.centre.x, shoal.centre.y, shoal.centre.z);
				return entity;
			}
		}
	}
	return entity;
}
