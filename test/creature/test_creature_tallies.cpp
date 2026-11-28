/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <array>

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

// The end of a meal counts the kill and the mushroom only while the thing eaten is still there; a meal already gone
// counts nothing
TEST(CreatureTallies, AMealEndCountsOnlyAThingStillThere)
{
	ecs::Registry registry;
	const auto creature = registry.Create();
	registry.Assign<Creature>(creature);
	stats::CountCreatureMealEnd(registry, creature, false, creature_eating::k_VillagerSort, true);
	EXPECT_EQ(registry.TryGet<const CreatureTallies>(creature), nullptr);
	stats::CountCreatureMealEnd(registry, creature, true, creature_eating::k_VillagerSort, false);
	stats::CountCreatureMealEnd(registry, creature, true, 3, true);
	const auto* tallies = registry.TryGet<const CreatureTallies>(creature);
	ASSERT_NE(tallies, nullptr);
	EXPECT_EQ(tallies->meals.peopleKilled, 1u);
	EXPECT_EQ(tallies->mushroomsEaten, 1u);
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

// At the start of the creatures' turn each creature of a player notes its size as its player's creature start size, only
// while none is noted yet: a size it grows to later, or a second creature of the same player, changes nothing
TEST(CreatureStartSize, EachPlayersCreatureNotesItsSizeOnlyWhileNoneIsNoted)
{
	ecs::Registry registry;
	const auto one = registry.Create();
	registry.Assign<Creature>(one).owner = PlayerNames::PLAYER_ONE;
	registry.Get<Creature>(one).size = 0.5f;
	const auto two = registry.Create();
	registry.Assign<Creature>(two).owner = PlayerNames::PLAYER_TWO;
	registry.Get<Creature>(two).size = 0.75f;
	const auto nobodys = registry.Create();
	registry.Assign<Creature>(nobodys).owner = PlayerNames::_COUNT;
	std::array<float, static_cast<size_t>(PlayerNames::_COUNT)> starts {};
	const auto startOf = [&starts](PlayerNames player) -> float& { return starts.at(static_cast<size_t>(player)); };

	stats::NoteCreatureStartSizes(registry, startOf);
	EXPECT_EQ(starts.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)), 0.5f);
	EXPECT_EQ(starts.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)), 0.75f);

	registry.Get<Creature>(one).size = 0.9f;
	const auto another = registry.Create();
	registry.Assign<Creature>(another).owner = PlayerNames::PLAYER_TWO;
	registry.Get<Creature>(another).size = 2.0f;
	stats::NoteCreatureStartSizes(registry, startOf);
	EXPECT_EQ(starts.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)), 0.5f);
	EXPECT_EQ(starts.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)), 0.75f);
	EXPECT_EQ(starts.at(static_cast<size_t>(PlayerNames::PLAYER_THREE)), 0.0f);
}
