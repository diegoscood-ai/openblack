/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/HandOnCreature.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerCreatures.h"
#include "ECS/CreatureRemoval.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

/// Writes down each step in turn, and takes the creature out of the registry as the world's last step
class FakeWorld final: public creature_removal::WorldInterface
{
public:
	explicit FakeWorld(Registry& registry)
	    : _registry(registry)
	{
	}

	void LetGoFromHand(entt::entity creature) override { Step("hand", creature); }
	void EndFight(entt::entity creature) override { Step("fight", creature); }
	void TakeOffLeash(entt::entity creature) override { Step("leash", creature); }
	void ReleaseEffects(entt::entity creature) override { Step("effects", creature); }
	void RemoveReactions(entt::entity creature) override { Step("reactions", creature); }
	void StopActing(entt::entity creature) override { Step("actions", creature); }
	void LeaveView(entt::entity creature) override { Step("view", creature); }
	void RemoveFromWorld(entt::entity creature) override
	{
		Step("world", creature);
		_registry.Destroy(creature);
	}
	[[nodiscard]] std::optional<entt::entity> PlayersCreature(PlayerNames owner) const override { return Slot(owner); }
	void LosePlayersCreature(PlayerNames owner) override
	{
		// the removed creature may be gone already
		steps.emplace_back("players creature");
		lost.push_back(owner);
		Slot(owner).reset();
	}

	/// The creature the game keeps for each player, as the player service would
	[[nodiscard]] std::optional<entt::entity>& Slot(PlayerNames owner) { return slots.at(static_cast<size_t>(owner)); }
	[[nodiscard]] const std::optional<entt::entity>& Slot(PlayerNames owner) const
	{
		return slots.at(static_cast<size_t>(owner));
	}

	std::vector<std::string> steps;
	std::vector<PlayerNames> lost;
	std::array<std::optional<entt::entity>, static_cast<size_t>(PlayerNames::_COUNT)> slots {};

private:
	void Step(std::string name, entt::entity creature)
	{
		// Every step comes while the creature is still there
		EXPECT_TRUE(_registry.Valid(creature)) << name;
		steps.push_back(std::move(name));
	}

	Registry& _registry;
};

class CreatureRemovalTest: public ::testing::Test
{
protected:
	entt::entity AddPlayer(PlayerNames name)
	{
		const auto player = registry.Create();
		registry.Assign<Player>(player, Player {.name = name});
		registry.Assign<PlayerCreatures>(player);
		return player;
	}

	entt::entity AddCreature(entt::entity player, PlayerNames owner)
	{
		const auto creature = registry.Create();
		registry.Assign<Creature>(creature, Creature {.owner = owner});
		registry.Get<PlayerCreatures>(player).acquired.push_back(creature);
		return creature;
	}

	Registry registry;
};

} // namespace

TEST_F(CreatureRemovalTest, GoesThroughEveryStepInTheGamesOrder)
{
	const auto player = AddPlayer(PlayerNames::PLAYER_ONE);
	const auto creature = AddCreature(player, PlayerNames::PLAYER_ONE);
	FakeWorld world(registry);
	world.Slot(PlayerNames::PLAYER_ONE) = creature;

	const auto removed = creature_removal::Remove(registry, creature, world);

	ASSERT_TRUE(removed.has_value());
	// its player is left with no creature last, as the creature itself goes once its removal is done
	const std::vector<std::string> expected {"hand",    "fight", "leash", "effects",         "reactions",
	                                         "actions", "view",  "world", "players creature"};
	EXPECT_EQ(world.steps, expected);
	EXPECT_FALSE(registry.Valid(creature));
	EXPECT_TRUE(registry.Get<PlayerCreatures>(player).acquired.empty());
	EXPECT_TRUE(removed->wasPrimary);
	EXPECT_FALSE(removed->alsoLost.has_value());
	EXPECT_EQ(world.lost, std::vector<PlayerNames> {PlayerNames::PLAYER_ONE});
	EXPECT_FALSE(world.Slot(PlayerNames::PLAYER_ONE).has_value());
}

TEST_F(CreatureRemovalTest, NoOtherCreatureTakesThePlaceOfThePlayersCreature)
{
	const auto player = AddPlayer(PlayerNames::PLAYER_ONE);
	const auto first = AddCreature(player, PlayerNames::PLAYER_ONE);
	const auto second = AddCreature(player, PlayerNames::PLAYER_ONE);
	const auto third = AddCreature(player, PlayerNames::PLAYER_ONE);
	FakeWorld world(registry);
	world.Slot(PlayerNames::PLAYER_ONE) = first;

	const auto removed = creature_removal::Remove(registry, first, world);

	ASSERT_TRUE(removed.has_value());
	EXPECT_TRUE(removed->wasPrimary);
	EXPECT_FALSE(removed->alsoLost.has_value());
	// left empty: neither the next one got nor the last one made is theirs now
	EXPECT_FALSE(world.Slot(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_TRUE(registry.Valid(second));
	EXPECT_TRUE(registry.Valid(third));
	EXPECT_EQ(registry.Get<PlayerCreatures>(player).acquired, (std::vector<entt::entity> {second, third}));
}

TEST_F(CreatureRemovalTest, RemovingAnotherOfTheOwnersCreaturesLeavesThemNone)
{
	const auto player = AddPlayer(PlayerNames::PLAYER_ONE);
	const auto kept = AddCreature(player, PlayerNames::PLAYER_ONE);
	const auto spare = AddCreature(player, PlayerNames::PLAYER_ONE);
	FakeWorld world(registry);
	world.Slot(PlayerNames::PLAYER_ONE) = kept;

	const auto removed = creature_removal::Remove(registry, spare, world);

	ASSERT_TRUE(removed.has_value());
	EXPECT_FALSE(removed->wasPrimary);
	EXPECT_EQ(removed->alsoLost, kept);
	EXPECT_EQ(world.lost, std::vector<PlayerNames> {PlayerNames::PLAYER_ONE});
	EXPECT_FALSE(world.Slot(PlayerNames::PLAYER_ONE).has_value());
	// after the whole removal, the spare one's place in the world last
	const std::vector<std::string> expected {"hand",    "fight", "leash", "effects",         "reactions",
	                                         "actions", "view",  "world", "players creature"};
	EXPECT_EQ(world.steps, expected);
	EXPECT_TRUE(registry.Valid(kept));
}

TEST_F(CreatureRemovalTest, OnlyItsOwnersCreatureIsLost)
{
	const auto one = AddPlayer(PlayerNames::PLAYER_ONE);
	const auto two = AddPlayer(PlayerNames::PLAYER_TWO);
	const auto mine = AddCreature(one, PlayerNames::PLAYER_ONE);
	const auto spare = AddCreature(one, PlayerNames::PLAYER_ONE);
	const auto theirs = AddCreature(two, PlayerNames::PLAYER_TWO);
	const auto older = registry.Create();
	registry.Assign<Creature>(older, Creature {.owner = PlayerNames::NEUTRAL});
	const auto newest = registry.Create();
	registry.Assign<Creature>(newest, Creature {.owner = PlayerNames::NEUTRAL});
	const auto nobodys = registry.Create();
	registry.Assign<Creature>(nobodys, Creature {.owner = PlayerNames::_COUNT});
	FakeWorld world(registry);
	world.Slot(PlayerNames::PLAYER_ONE) = mine;
	world.Slot(PlayerNames::PLAYER_TWO) = theirs;
	world.Slot(PlayerNames::NEUTRAL) = newest;

	// a creature of no player leaves every player's alone
	ASSERT_TRUE(creature_removal::Remove(registry, nobodys, world).has_value());
	EXPECT_TRUE(world.lost.empty());
	EXPECT_EQ(world.Slot(PlayerNames::NEUTRAL), newest);

	// the neutral player has one too, and any of theirs going empties it
	const auto neutral = creature_removal::Remove(registry, older, world);
	ASSERT_TRUE(neutral.has_value());
	EXPECT_EQ(neutral->alsoLost, newest);
	EXPECT_FALSE(world.Slot(PlayerNames::NEUTRAL).has_value());

	const auto removed = creature_removal::Remove(registry, spare, world);
	ASSERT_TRUE(removed.has_value());
	EXPECT_EQ(removed->alsoLost, mine);
	EXPECT_EQ(world.lost, (std::vector<PlayerNames> {PlayerNames::NEUTRAL, PlayerNames::PLAYER_ONE}));
	EXPECT_EQ(world.Slot(PlayerNames::PLAYER_TWO), theirs);
	EXPECT_EQ(registry.Get<PlayerCreatures>(two).acquired, std::vector<entt::entity> {theirs});
}

TEST_F(CreatureRemovalTest, WhatPointsAtItLetsGo)
{
	const auto player = AddPlayer(PlayerNames::PLAYER_ONE);
	const auto creature = AddCreature(player, PlayerNames::PLAYER_ONE);
	const auto other = AddCreature(player, PlayerNames::PLAYER_ONE);

	const auto marker = registry.Create();
	registry.Assign<LeashMarker>(marker, LeashMarker {.creature = creature});
	const auto otherMarker = registry.Create();
	registry.Assign<LeashMarker>(otherMarker, LeashMarker {.creature = other});
	const auto carried = registry.Create();
	registry.Assign<HeldByCreature>(carried, HeldByCreature {.creature = creature});
	auto& following = registry.Assign<CreatureLocomotion>(other);
	following.following = creature;
	const auto hand = registry.Create();
	registry.Assign<HandOnCreature>(hand, HandOnCreature {.creature = creature});
	FakeWorld world(registry);

	const auto removed = creature_removal::Remove(registry, creature, world);

	ASSERT_TRUE(removed.has_value());
	EXPECT_EQ(removed->referencesCleared, 4u);
	EXPECT_FALSE(registry.Valid(marker));
	EXPECT_TRUE(registry.Valid(otherMarker));
	EXPECT_FALSE(registry.AllOf<HeldByCreature>(carried));
	EXPECT_TRUE(registry.Valid(carried));
	EXPECT_FALSE(registry.Get<CreatureLocomotion>(other).following.has_value());
	EXPECT_FALSE(registry.AllOf<HandOnCreature>(hand));
}

TEST_F(CreatureRemovalTest, SomethingThatIsntACreatureIsLeftAlone)
{
	const auto thing = registry.Create();
	FakeWorld world(registry);

	EXPECT_FALSE(creature_removal::Remove(registry, thing, world).has_value());
	EXPECT_TRUE(world.steps.empty());
	EXPECT_TRUE(registry.Valid(thing));
}
