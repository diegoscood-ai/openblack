/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The world of the tests with villagers in it: the world harness (support/WorldHarness.h) and the game's villager
// services over it (their rules, world queries, worship checks, children, disciples' jobs, the states' actions and the
// random numbers), with villagers made as the game makes them. Each service is put back when the test ends. Include it
// after LOCATOR_IMPLEMENTATIONS.

#include <cstdint>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Common/RandomNumberManagerProduction.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Systems/Implementations/LivingActionSystem.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/Implementations/VillagerWorshipCheck.h"
#include "Enums.h"
#include "Locator.h"
#include "support/RestoreService.h"
#include "support/WorldHarness.h"

namespace openblack::test::world
{
class VillagerWorld: public WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		Locator::rng::emplace<RandomNumberManagerProduction>();
		Locator::livingActionSystem::emplace<ecs::systems::LivingActionSystem>();
		Locator::villagerRules::emplace<ecs::systems::VillagerRules>();
		Locator::villagerWorldQueries::emplace<ecs::systems::VillagerWorldQueries>();
		Locator::villagerWorshipCheck::emplace<ecs::systems::VillagerWorshipCheck>();
		Locator::villagerChildFactory::emplace<ecs::systems::VillagerChildFactory>();
		Locator::villagerDiscipleJobs::emplace<ecs::systems::VillagerDiscipleJobs>();
	}

	/// A grown villager of the kind standing on the land, of no town, as the game makes one
	[[nodiscard]] static entt::entity Villager(VillagerInfo type, glm::vec2 at, uint32_t age = 20)
	{
		const glm::vec3 point(at.x, k_Ground, at.y);
		return ecs::archetypes::VillagerArchetype::Create(point, point, type, age, false);
	}

private:
	// put back in reverse order, before the world's own
	RestoreService<Locator::rng> _restoreRng;
	RestoreService<Locator::livingActionSystem> _restoreLivingActions;
	RestoreService<Locator::villagerRules> _restoreRules;
	RestoreService<Locator::villagerWorldQueries> _restoreWorldQueries;
	RestoreService<Locator::villagerWorshipCheck> _restoreWorshipCheck;
	RestoreService<Locator::villagerChildFactory> _restoreChildFactory;
	RestoreService<Locator::villagerDiscipleJobs> _restoreDiscipleJobs;
};
} // namespace openblack::test::world
