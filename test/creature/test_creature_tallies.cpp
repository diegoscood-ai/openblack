/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "Creature/CreatureEating.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureTallies.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace stats = openblack::ecs::player_statistics;

TEST(CreatureTallies, TheTalliesAreMadeTheFirstTimeACreatureCountsSomething)
{
	ecs::Registry registry;
	const auto creature = registry.Create();
	registry.Assign<Creature>(creature);
	EXPECT_EQ(registry.TryGet<const CreatureTallies>(creature), nullptr);

	stats::CountCreaturePoo(registry, creature);
	stats::CountCreaturePoo(registry, creature);
	stats::CountCreatureMushroom(registry, creature);
	stats::CountCreatureMeal(registry, creature, creature_eating::k_VillagerSort);
	stats::CountCreatureMeal(registry, creature, creature_eating::k_AnimalSort);
	stats::CountCreatureMeal(registry, creature, creature_eating::k_CreatureSort);

	const auto* tallies = registry.TryGet<const CreatureTallies>(creature);
	ASSERT_NE(tallies, nullptr);
	EXPECT_EQ(tallies->poos, 2u);
	EXPECT_EQ(tallies->mushroomsEaten, 1u);
	EXPECT_EQ(tallies->meals.peopleKilled, 1u);
	EXPECT_EQ(tallies->meals.animalsKilled, 1u);
	EXPECT_EQ(tallies->meals.creaturesKilled, 1u);
}

TEST(CreatureTallies, AMealThatIsNoKillMakesNoTallies)
{
	ecs::Registry registry;
	const auto creature = registry.Create();
	registry.Assign<Creature>(creature);
	// a mobile object eaten is no kill
	stats::CountCreatureMeal(registry, creature, 3);
	EXPECT_EQ(registry.TryGet<const CreatureTallies>(creature), nullptr);
}

TEST(CreatureTallies, OnlyACreatureCounts)
{
	ecs::Registry registry;
	const auto villager = registry.Create();
	stats::CountCreaturePoo(registry, villager);
	stats::CountCreatureMushroom(registry, villager);
	stats::CountCreatureMeal(registry, villager, creature_eating::k_VillagerSort);
	EXPECT_EQ(registry.TryGet<const CreatureTallies>(villager), nullptr);
	// nor a thing that is gone
	stats::CountCreaturePoo(registry, entt::null);
}
