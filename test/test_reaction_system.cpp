/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The reaction service on its own: ids in creation order, the removals by initiator and by kind with each class's
// shut-down before the reactions leave the list, and a new land

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/ReactionSystem.h"

using namespace openblack;
using openblack::ecs::effects::reactions::LivingClass;
using openblack::ecs::systems::ReactionSystem;
using Record = openblack::ecs::effects::reactions::Reaction;

namespace
{
/// What the shut-down handlers saw: the reaction and whether it was still listed and available at that moment
struct ShutDownCall
{
	uint32_t id;
	bool listed;
	bool available;
};

ReactionSystem* g_system = nullptr;
std::vector<ShutDownCall> g_calls;

void RecordShutDown(uint32_t id)
{
	const auto* found = g_system->Find(id);
	g_calls.push_back({.id = id, .listed = found != nullptr, .available = found != nullptr && found->available});
}

class ReactionSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		g_system = &_system;
		g_calls.clear();
	}
	void TearDown() override { g_system = nullptr; }

	uint32_t Add(entt::entity initiator, Reaction type)
	{
		return _system.Add({.initiator = initiator, .type = type, .player = PlayerNames::PLAYER_ONE});
	}

	ReactionSystem _system;
	const entt::entity _tree {entt::entity {3}};
	const entt::entity _pot {entt::entity {7}};
};
} // namespace

TEST_F(ReactionSystemTest, IdsGoUpInCreationOrder)
{
	EXPECT_EQ(Add(_tree, Reaction::ReactToFire), 1u);
	EXPECT_EQ(Add(_pot, Reaction::ReactToFood), 2u);
	ASSERT_EQ(_system.GetReactions().size(), 2u);
	EXPECT_EQ(_system.GetReactions()[0].initiator, _tree);
	EXPECT_EQ(_system.GetReactions()[1].initiator, _pot);
	EXPECT_TRUE(_system.IsActive(1));
	EXPECT_FALSE(_system.IsActive(3));
	EXPECT_EQ(_system.Find(0), nullptr);
}

TEST_F(ReactionSystemTest, RemoveFromAnInitiatorShutsEachDownBeforeTheyGo)
{
	_system.SetShutDownHandler(LivingClass::Villager, &RecordShutDown);
	Add(_tree, Reaction::ReactToFire);
	Add(_pot, Reaction::ReactToFood);
	Add(_tree, Reaction::ReactToBurningObjectInHand);

	_system.RemoveFrom(_tree);

	// in list order, each still listed but no longer available when its followers are told
	ASSERT_EQ(g_calls.size(), 2u);
	EXPECT_EQ(g_calls[0].id, 1u);
	EXPECT_EQ(g_calls[1].id, 3u);
	EXPECT_TRUE(g_calls[0].listed);
	EXPECT_FALSE(g_calls[0].available);
	EXPECT_TRUE(g_calls[1].listed);
	EXPECT_FALSE(g_calls[1].available);
	ASSERT_EQ(_system.GetReactions().size(), 1u);
	EXPECT_EQ(_system.GetReactions()[0].id, 2u);
	EXPECT_FALSE(_system.HasReaction(_tree));
	EXPECT_TRUE(_system.HasReaction(_pot));
}

TEST_F(ReactionSystemTest, RemoveFromAnInitiatorOfOneKindLeavesItsOthers)
{
	Add(_tree, Reaction::ReactToFire);
	Add(_tree, Reaction::ReactToBurningObjectInHand);

	_system.RemoveFrom(_tree, Reaction::ReactToFire);

	ASSERT_EQ(_system.GetReactions().size(), 1u);
	EXPECT_EQ(_system.GetReactions()[0].type, Reaction::ReactToBurningObjectInHand);
	EXPECT_TRUE(_system.HasReaction(_tree));
}

TEST_F(ReactionSystemTest, RemoveOneById)
{
	_system.SetShutDownHandler(LivingClass::Animal, &RecordShutDown);
	Add(_tree, Reaction::ReactToFire);
	Add(_pot, Reaction::ReactToFood);

	_system.Remove(2u);

	ASSERT_EQ(g_calls.size(), 1u);
	EXPECT_EQ(g_calls[0].id, 2u);
	EXPECT_FALSE(_system.IsActive(2));
	EXPECT_TRUE(_system.IsActive(1));
}

TEST_F(ReactionSystemTest, HasReactionCountsAnUnavailableOne)
{
	Add(_tree, Reaction::ReactToFire);
	_system.List()[0].available = false;
	EXPECT_TRUE(_system.HasReaction(_tree));
}

TEST_F(ReactionSystemTest, ResetEmptiesTheListAndStartsTheIdsAgain)
{
	_system.SetShutDownHandler(LivingClass::Villager, &RecordShutDown);
	Add(_tree, Reaction::ReactToFire);
	_system.SetInTurn(true);
	const auto joined = _system.NextJoinOrder();

	_system.Reset();

	EXPECT_TRUE(_system.GetReactions().empty());
	EXPECT_FALSE(_system.InTurn());
	EXPECT_TRUE(g_calls.empty()); // a new land shuts nothing down
	EXPECT_EQ(Add(_pot, Reaction::ReactToFood), 1u);
	// the handlers and the join counter stay
	EXPECT_EQ(_system.ShutDownHandlers()[static_cast<std::size_t>(LivingClass::Villager)], &RecordShutDown);
	EXPECT_EQ(_system.NextJoinOrder(), joined + 1);
}
