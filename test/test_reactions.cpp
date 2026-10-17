/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The reaction service on its own: ids in creation order, the removals by initiator and by kind with each class's
// shut-down before the reactions leave the list, and a new land. Then the spread over the map's cells: the living that
// started a reaction is offered it like every other living in reach, as the original's spread has no test against the
// reaction's source

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <array>
#include <span>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ReactionSystem.h"
#include "Locator.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

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

class Reactions: public ::testing::Test
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

TEST_F(Reactions, IdsGoUpInCreationOrder)
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

TEST_F(Reactions, RemoveFromAnInitiatorShutsEachDownBeforeTheyGo)
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

TEST_F(Reactions, RemoveFromAnInitiatorOfOneKindLeavesItsOthers)
{
	Add(_tree, Reaction::ReactToFire);
	Add(_tree, Reaction::ReactToBurningObjectInHand);

	_system.RemoveFrom(_tree, Reaction::ReactToFire);

	ASSERT_EQ(_system.GetReactions().size(), 1u);
	EXPECT_EQ(_system.GetReactions()[0].type, Reaction::ReactToBurningObjectInHand);
	EXPECT_TRUE(_system.HasReaction(_tree));
}

TEST_F(Reactions, RemoveOneById)
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

TEST_F(Reactions, HasReactionCountsAnUnavailableOne)
{
	Add(_tree, Reaction::ReactToFire);
	_system.List()[0].available = false;
	EXPECT_TRUE(_system.HasReaction(_tree));
}

TEST_F(Reactions, ResetEmptiesTheListAndStartsTheIdsAgain)
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

namespace
{
/// The map the spread asks to sync its cells outside the turn: the cells are filed by the test itself
class FakeMap final: public ecs::MapInterface
{
public:
	[[nodiscard]] std::span<const entt::entity> GetFixedInGridCell(const CellId& /*cellId*/) const override { return {}; }
	[[nodiscard]] std::span<const entt::entity> GetFixedInGridCell(const glm::vec3& /*pos*/) const override { return {}; }
	[[nodiscard]] std::span<const entt::entity> GetMobileInGridCell(const CellId& /*cellId*/) const override { return {}; }
	[[nodiscard]] std::span<const entt::entity> GetMobileInGridCell(const glm::vec3& /*pos*/) const override { return {}; }
	[[nodiscard]] std::vector<entt::entity> GetAllInCell(glm::ivec2 /*cell*/) const override { return {}; }
	void Sync() override {}
	void Refile(entt::entity /*entity*/) override {}
};

/// A living the spread handed a reaction to, and its distance from the reaction
struct Offer
{
	entt::entity living;
	uint32_t reaction;
	float distance;
};

std::vector<Offer> g_offers;

void RecordOffer(entt::entity living, const Record& reaction, float distance)
{
	g_offers.push_back({.living = living, .reaction = reaction.id, .distance = distance});
}

/// Animals in the map's cells of a 16 x 16 cell island, with the world lists; the animals' handler records what it is
/// offered
class ReactionSpread: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {1});
		Locator::entitiesMap::emplace<FakeMap>();
		test::EmplaceMapAndVillagerDefaults();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		ecs::map_cells::Clear();
		ecs::object_index::OnLoadMap();
		ecs::effects::reactions::SetLivingReactionHandler(LivingClass::Animal, &RecordOffer);
		g_offers.clear();
	}
	void TearDown() override
	{
		g_offers.clear();
		ecs::map_cells::Clear();
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A cow standing at a point, in its cell's mobile list
	static entt::entity CowAt(const glm::vec3& position)
	{
		const auto entity = Reg().Create();
		ecs::object_index::Assign(entity);
		Reg().Assign<ecs::components::Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<ecs::components::Animal>(entity).type = AnimalInfo::Cow;
		ecs::map_cells::InsertMapObject(entity);
		return entity;
	}

	static const Offer* OfferTo(entt::entity living)
	{
		const auto found = std::ranges::find(g_offers, living, &Offer::living);
		return found != g_offers.end() ? &*found : nullptr;
	}

private:
	test::RestoreService<Locator::terrainSystem> _restoreTerrain;
	test::RestoreService<Locator::entitiesMap> _restoreMap;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};
} // namespace

TEST_F(ReactionSpread, TheLivingThatStartsAReactionIsOfferedItLikeTheOthers)
{
	// two cows in one cell; the first starts a reaction whose reach, 5 m, is that one cell
	const auto starter = CowAt({50.0f, 0.0f, 50.0f});
	const auto other = CowAt({53.0f, 0.0f, 52.0f});
	const auto id = Locator::reactionSystem::value().Add(
	    {.initiator = starter, .type = Reaction::ReactToFire, .player = PlayerNames::PLAYER_ONE, .radius = 5.0f});

	ecs::effects::reactions::SpreadReaction(id);

	// each once, the one that started it at no distance, the other at (|dz| + |dx|) / 2
	ASSERT_EQ(g_offers.size(), 2u);
	const auto* own = OfferTo(starter);
	ASSERT_NE(own, nullptr);
	EXPECT_EQ(own->reaction, id);
	EXPECT_EQ(own->distance, 0.0f);
	const auto* neighbour = OfferTo(other);
	ASSERT_NE(neighbour, nullptr);
	EXPECT_EQ(neighbour->reaction, id);
	EXPECT_EQ(neighbour->distance, 2.5f);
}
