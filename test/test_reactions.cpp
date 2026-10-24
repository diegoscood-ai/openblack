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

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/ReactionTurnRules.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ReactionSystem.h"
#include "GameClock.h"
#include "Locator.h"
#include "Magic/Objects/MagicTree.h"
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

TEST_F(Reactions, SetPlayerOfNewestTakesTheLastOfTheInitiator)
{
	Add(_tree, Reaction::ReactToFire);
	Add(_pot, Reaction::ReactToFood);
	Add(_tree, Reaction::ReactToBurningObjectInHand);

	_system.SetPlayerOfNewest(_tree, PlayerNames::PLAYER_TWO);

	ASSERT_EQ(_system.GetReactions().size(), 3u);
	EXPECT_EQ(_system.GetReactions()[0].player, std::optional(PlayerNames::PLAYER_ONE));
	EXPECT_EQ(_system.GetReactions()[1].player, std::optional(PlayerNames::PLAYER_ONE));
	EXPECT_EQ(_system.GetReactions()[2].player, std::optional(PlayerNames::PLAYER_TWO));
}

TEST_F(Reactions, RemoveFromAnInitiatorShutsEachDownBeforeTheyGo)
{
	_system.SetShutDownHandler(LivingClass::Villager, &RecordShutDown);
	Add(_tree, Reaction::ReactToFire);
	Add(_pot, Reaction::ReactToFood);
	Add(_tree, Reaction::ReactToBurningObjectInHand);

	_system.RemoveFrom(_tree);

	// newest first, as the original's walk from its list's head; each still listed but no longer available when its
	// followers are told
	ASSERT_EQ(g_calls.size(), 2u);
	EXPECT_EQ(g_calls[0].id, 3u);
	EXPECT_EQ(g_calls[1].id, 1u);
	EXPECT_TRUE(g_calls[0].listed);
	EXPECT_FALSE(g_calls[0].available);
	EXPECT_TRUE(g_calls[1].listed);
	EXPECT_FALSE(g_calls[1].available);
	ASSERT_EQ(_system.GetReactions().size(), 1u);
	EXPECT_EQ(_system.GetReactions()[0].id, 2u);
	EXPECT_FALSE(_system.HasReaction(_tree));
	EXPECT_TRUE(_system.HasReaction(_pot));
}

TEST_F(Reactions, RemoveFromAnInitiatorOfOneKindShutsTheNewestDownFirst)
{
	_system.SetShutDownHandler(LivingClass::Villager, &RecordShutDown);
	Add(_tree, Reaction::ReactToFire);
	Add(_pot, Reaction::ReactToFire);
	Add(_tree, Reaction::ReactToFire);

	_system.RemoveFrom(_tree, Reaction::ReactToFire);

	ASSERT_EQ(g_calls.size(), 2u);
	EXPECT_EQ(g_calls[0].id, 3u);
	EXPECT_EQ(g_calls[1].id, 1u);
	ASSERT_EQ(_system.GetReactions().size(), 1u);
	EXPECT_EQ(_system.GetReactions()[0].id, 2u);
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

TEST_F(Reactions, TheCursorMovesToTheNextOlderReactionWhenItsReactionGoes)
{
	Add(_tree, Reaction::ReactToFire);
	Add(_pot, Reaction::ReactToFood);
	Add(_tree, Reaction::ReactToBurningObjectInHand);
	_system.SetCursor(3);

	_system.RemoveFrom(_tree);
	EXPECT_EQ(_system.Cursor(), 2u);

	// none older is left: the next turn starts again at the newest
	_system.Remove(2u);
	EXPECT_EQ(_system.Cursor(), 0u);
}

TEST_F(Reactions, AReactionThatGoesLeavesAnotherCursorAlone)
{
	Add(_tree, Reaction::ReactToFire);
	Add(_pot, Reaction::ReactToFood);
	_system.SetCursor(2);
	_system.Remove(1u);
	EXPECT_EQ(_system.Cursor(), 2u);
}

TEST_F(Reactions, ResetClearsTheCursor)
{
	Add(_tree, Reaction::ReactToFire);
	_system.SetCursor(1);
	_system.Reset();
	EXPECT_EQ(_system.Cursor(), 0u);
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

ecs::effects::reactions::CellWalk RecordOffer(entt::entity living, const Record& reaction, float distance)
{
	g_offers.push_back({.living = living, .reaction = reaction.id, .distance = distance});
	return ecs::effects::reactions::CellWalk::Next;
}

/// As RecordOffer, each Living then ending its cell's walk
ecs::effects::reactions::CellWalk RecordOfferAndLeave(entt::entity living, const Record& reaction, float distance)
{
	RecordOffer(living, reaction, distance);
	return ecs::effects::reactions::CellWalk::Leave;
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

TEST_F(ReactionSpread, ALivingThatEndsItsCellLeavesTheRestOfTheCellOut)
{
	// two cows in one cell and one in the next cell, all within the 15 m reach; each Living offered it ends its cell
	ecs::effects::reactions::SetLivingReactionHandler(LivingClass::Animal, &RecordOfferAndLeave);
	const auto first = CowAt({50.0f, 0.0f, 50.0f});
	const auto second = CowAt({53.0f, 0.0f, 52.0f});
	const auto next = CowAt({62.0f, 0.0f, 50.0f});
	const auto id = Locator::reactionSystem::value().Add(
	    {.initiator = first, .type = Reaction::ReactToFire, .player = PlayerNames::PLAYER_ONE, .radius = 15.0f});

	ecs::effects::reactions::SpreadReaction(id);

	// one of the first cell's two, whichever comes first in its list, and the next cell's one
	EXPECT_EQ(g_offers.size(), 2u);
	EXPECT_NE(OfferTo(first) == nullptr, OfferTo(second) == nullptr);
	EXPECT_NE(OfferTo(next), nullptr);
}

TEST_F(ReactionSpread, AThingJustMadeIsNotReachedByTheSpreadOfItsOwnReactionAtItsCreation)
{
	// a creature's reaction made as it is made, before it is in its cell: even filed already (as a spread outside the
	// turn files every new thing first), the one that starts it is not offered it; the others are
	const auto starter = CowAt({50.0f, 0.0f, 50.0f});
	const auto other = CowAt({53.0f, 0.0f, 52.0f});
	const auto id =
	    ecs::effects::reactions::CreateReactionBeforeFiling(starter, Reaction::ReactToFire, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(id, 0u);
	EXPECT_EQ(OfferTo(starter), nullptr);
	ASSERT_NE(OfferTo(other), nullptr);
	// spread again later, it is offered to its starter like the others
	g_offers.clear();
	ecs::effects::reactions::SpreadReaction(id);
	EXPECT_NE(OfferTo(starter), nullptr);
	EXPECT_NE(OfferTo(other), nullptr);
}

TEST_F(ReactionSpread, EachTurnOffersOneReactionAgainNewestFirst)
{
	// two cows far apart, each the only one in reach of its own reaction
	const auto first = CowAt({50.0f, 0.0f, 50.0f});
	const auto second = CowAt({120.0f, 0.0f, 120.0f});
	auto& system = Locator::reactionSystem::value();
	const auto older =
	    system.Add({.initiator = first, .type = Reaction::ReactToFire, .player = PlayerNames::PLAYER_ONE, .radius = 5.0f});
	const auto newer =
	    system.Add({.initiator = second, .type = Reaction::ReactToFire, .player = PlayerNames::PLAYER_ONE, .radius = 5.0f});

	std::vector<uint32_t> offered;
	for (int i = 0; i < 3; ++i)
	{
		g_offers.clear();
		ecs::effects::reactions::ProcessTurn();
		ASSERT_EQ(g_offers.size(), 1u);
		offered.push_back(g_offers[0].reaction);
	}
	EXPECT_EQ(offered, (std::vector<uint32_t> {newer, older, newer}));
	EXPECT_EQ(system.Cursor(), newer);
	EXPECT_EQ(OfferTo(second)->distance, 0.0f);
}

TEST_F(ReactionSpread, AnInactiveReactionTakesItsTurnWithoutBeingOffered)
{
	const auto first = CowAt({50.0f, 0.0f, 50.0f});
	const auto second = CowAt({120.0f, 0.0f, 120.0f});
	auto& system = Locator::reactionSystem::value();
	const auto older =
	    system.Add({.initiator = first, .type = Reaction::ReactToFire, .player = PlayerNames::PLAYER_ONE, .radius = 5.0f});
	const auto newer = system.Add({.initiator = second,
	                               .type = Reaction::ReactToFire,
	                               .player = PlayerNames::PLAYER_ONE,
	                               .radius = 5.0f,
	                               .available = false});

	ecs::effects::reactions::ProcessTurn();
	EXPECT_EQ(system.Cursor(), newer);
	EXPECT_TRUE(g_offers.empty());

	ecs::effects::reactions::ProcessTurn();
	EXPECT_EQ(system.Cursor(), older);
	ASSERT_EQ(g_offers.size(), 1u);
	EXPECT_EQ(g_offers[0].living, first);
}

TEST_F(ReactionSpread, TheReactionAnObjectStartedIsItsNewest)
{
	const auto cow = CowAt({50.0f, 0.0f, 50.0f});
	auto& list = Locator::reactionSystem::value();
	static_cast<void>(list.Add({.initiator = cow, .type = Reaction::ReactToFire, .player = PlayerNames::PLAYER_ONE}));
	const auto newest = list.Add({.initiator = cow, .type = Reaction::ReactToFood, .player = PlayerNames::PLAYER_ONE});
	EXPECT_EQ(ecs::effects::reactions::GetReactionInitiatedBy(cow), newest);
	EXPECT_EQ(ecs::effects::reactions::GetReactionInitiatedBy(entt::entity {99}), 0u);
}

TEST_F(ReactionSpread, AnEmptyListOffersNothing)
{
	ecs::effects::reactions::ProcessTurn();
	EXPECT_EQ(Locator::reactionSystem::value().Cursor(), 0u);
	EXPECT_TRUE(g_offers.empty());
}

TEST_F(ReactionSpread, AMagicTreeOnFireIsNoLongerLookedAtUntilItsFireIsOut)
{
	const auto count = [](entt::entity initiator, Reaction type) {
		return std::ranges::count_if(Locator::reactionSystem::value().GetReactions(), [&](const Record& reaction) {
			return reaction.initiator == initiator && reaction.type == type;
		});
	};
	const auto tree = Reg().Create();
	ecs::object_index::Assign(tree);
	Reg().Assign<ecs::components::Transform>(tree, glm::vec3(50.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<ecs::components::MagicTree>(tree).player = PlayerNames::PLAYER_TWO;
	auto& list = Locator::reactionSystem::value();
	static_cast<void>(list.Add({.initiator = tree, .type = Reaction::ReactToMagicTree, .player = PlayerNames::PLAYER_TWO}));
	static_cast<void>(list.Add({.initiator = tree, .type = Reaction::ReactToFire, .player = PlayerNames::PLAYER_TWO}));
	game_clock::SetTurn(50);

	// catching fire takes away only its own "come and look at the magic tree"
	magic::magic_tree::StartOnFire(tree);
	EXPECT_EQ(count(tree, Reaction::ReactToMagicTree), 0);
	EXPECT_EQ(count(tree, Reaction::ReactToFire), 1);
	// once the fire is out it makes one again, for its player and not stamped with the turn
	magic::magic_tree::EndOnFire(tree);
	ASSERT_EQ(count(tree, Reaction::ReactToMagicTree), 1);
	const auto reactions = list.GetReactions();
	const auto made = std::ranges::find(reactions, Reaction::ReactToMagicTree, &Record::type);
	ASSERT_NE(made, reactions.end());
	EXPECT_EQ(made->player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(made->turnCreated, 0u);
	// a tree that is no magic tree makes none
	const auto plain = Reg().Create();
	Reg().Assign<ecs::components::Transform>(plain, glm::vec3(60.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	magic::magic_tree::EndOnFire(plain);
	EXPECT_EQ(count(plain, Reaction::ReactToMagicTree), 0);
	game_clock::SetTurn(0);
}

// ---- the reaction offered each turn

namespace
{
namespace turn = openblack::ecs::effects::reaction_turn;

/// The cursor after each of `turns` steps over a list that does not change
std::vector<uint32_t> Walk(std::span<const uint32_t> ids, uint32_t cursor, int turns)
{
	std::vector<uint32_t> seen;
	for (int i = 0; i < turns; ++i)
	{
		cursor = turn::NextCursor(ids, cursor);
		seen.push_back(cursor);
	}
	return seen;
}

/// A reaction the step takes: its initiator there, active, started at no turn
turn::StepFacts Facts()
{
	return {.initiatorAvailable = true, .turn = 1000, .turnsToReact = 50, .active = true, .radius = 25.0f};
}
} // namespace

TEST(ReactionTurnRules, TheCursorWalksFromTheNewestToTheOldestAndWraps)
{
	// the original's list R0 -> R1 -> R2 from its head (the newest) took R0 R1 R2 R0 R1 R2 R0 over seven turns; ours is
	// in creation order, so R0 is id 3
	constexpr std::array<uint32_t, 3> k_Ids {1, 2, 3};
	EXPECT_EQ(Walk(k_Ids, 0, 7), (std::vector<uint32_t> {3, 2, 1, 3, 2, 1, 3}));
}

TEST(ReactionTurnRules, AnEmptyListHasNoCursor)
{
	EXPECT_EQ(turn::NextCursor({}, 0), 0u);
	EXPECT_EQ(turn::NextCursor({}, 5), 0u);
}

TEST(ReactionTurnRules, AStaleCursorStartsAtTheNewest)
{
	constexpr std::array<uint32_t, 3> k_Ids {1, 2, 3};
	EXPECT_EQ(turn::NextCursor(k_Ids, 9), 3u);
}

TEST(ReactionTurnRules, AReactionMadeMidWalkIsReachedAfterTheWrap)
{
	constexpr std::array<uint32_t, 3> k_Before {1, 2, 3};
	const auto cursor = turn::NextCursor(k_Before, 2);
	EXPECT_EQ(cursor, 1u);
	// 4 is made now: the walk goes on to the oldest first, then wraps to 4
	constexpr std::array<uint32_t, 4> k_After {1, 2, 3, 4};
	EXPECT_EQ(turn::NextCursor(k_After, cursor), 4u);
}

TEST(ReactionTurnRules, RemovingTheCursorsReactionMovesItToTheNextOlderSurvivor)
{
	constexpr std::array<uint32_t, 4> k_Ids {1, 2, 3, 4};
	constexpr std::array<uint32_t, 2> k_Gone {3, 2};
	constexpr std::array<uint32_t, 2> k_GoneOtherOrder {2, 3};
	EXPECT_EQ(turn::CursorAfterRemoval(k_Ids, k_Gone, 3), 1u);
	EXPECT_EQ(turn::CursorAfterRemoval(k_Ids, k_GoneOtherOrder, 3), 1u);
	// a cursor that stays is left alone
	EXPECT_EQ(turn::CursorAfterRemoval(k_Ids, k_Gone, 4), 4u);
	EXPECT_EQ(turn::CursorAfterRemoval(k_Ids, k_Gone, 0), 0u);

	// the oldest goes while it is the cursor: none is older, so the next turn starts at the newest
	constexpr std::array<uint32_t, 2> k_Left {1, 4};
	constexpr std::array<uint32_t, 1> k_Oldest {1};
	const auto cursor = turn::CursorAfterRemoval(k_Left, k_Oldest, 1);
	EXPECT_EQ(cursor, 0u);
	constexpr std::array<uint32_t, 1> k_Survivors {4};
	EXPECT_EQ(turn::NextCursor(k_Survivors, cursor), 4u);
}

TEST(ReactionTurnRules, TheReactionAfterARemovedCursorIsSkippedOnce)
{
	// as in the original: the cursor moves to the next older one when its reaction goes, and the next turn steps past it
	constexpr std::array<uint32_t, 4> k_Ids {1, 2, 3, 4};
	constexpr std::array<uint32_t, 1> k_Gone {3};
	const auto cursor = turn::CursorAfterRemoval(k_Ids, k_Gone, 3);
	EXPECT_EQ(cursor, 2u);
	constexpr std::array<uint32_t, 3> k_Left {1, 2, 4};
	EXPECT_EQ(turn::NextCursor(k_Left, cursor), 1u);
}

TEST(ReactionTurnRules, AnInactiveReactionIsNotSpread)
{
	auto facts = Facts();
	facts.active = false;
	const auto step = turn::Decide(facts);
	EXPECT_EQ(step.action, turn::StepAction::Nothing);
	EXPECT_EQ(step.radius, 25.0f);
	EXPECT_EQ(turn::Decide(Facts()).action, turn::StepAction::Spread);
}

TEST(ReactionTurnRules, AGrowingReactionWidensEachTimeItIsProcessed)
{
	// the original, 0.5 a time from 1: 1.5, 2.0, 2.5, 3.0
	auto facts = Facts();
	facts.grows = true;
	facts.growth = 0.5f;
	facts.radius = 1.0f;
	std::vector<float> radii;
	for (int i = 0; i < 4; ++i)
	{
		const auto step = turn::Decide(facts);
		EXPECT_EQ(step.action, turn::StepAction::Spread);
		radii.push_back(step.radius);
		facts.radius = step.radius;
	}
	EXPECT_EQ(radii, (std::vector<float> {1.5f, 2.0f, 2.5f, 3.0f}));

	// the info's 0.1 m steps (fainting, confused) add in float, one at a time
	facts.growth = 0.1f;
	facts.radius = 1.0f;
	float expected = 1.0f;
	for (int i = 0; i < 10; ++i)
	{
		expected = expected + 0.1f;
		facts.radius = turn::Decide(facts).radius;
		EXPECT_EQ(facts.radius, expected);
	}
	// an inactive one grows too, before the test of its state
	facts.active = false;
	EXPECT_EQ(turn::Decide(facts).radius, expected + 0.1f);
}

TEST(ReactionTurnRules, ATimeLimitedReactionShutsDownPastItsTurns)
{
	// the original: started 900, turn 1000, 50 turns to react -> shut down; 150 -> spread
	auto facts = Facts();
	facts.timeLimited = true;
	facts.turnStarted = 900;
	facts.turnsToReact = 50;
	EXPECT_EQ(turn::Decide(facts).action, turn::StepAction::ShutDown);
	facts.turnsToReact = 150;
	EXPECT_EQ(turn::Decide(facts).action, turn::StepAction::Spread);
	// strictly more turns than its limit
	facts.turnsToReact = 100;
	EXPECT_EQ(turn::Decide(facts).action, turn::StepAction::Spread);
	// not started yet: never
	facts.turnsToReact = 0;
	facts.turnStarted = 0;
	EXPECT_EQ(turn::Decide(facts).action, turn::StepAction::Spread);
	// a kind that is not time-limited is never shut down by the step
	facts.turnStarted = 900;
	facts.timeLimited = false;
	EXPECT_EQ(turn::Decide(facts).action, turn::StepAction::Spread);
	// a time-limited one that grows has grown when it is shut down
	facts.timeLimited = true;
	facts.grows = true;
	facts.growth = 0.5f;
	const auto step = turn::Decide(facts);
	EXPECT_EQ(step.action, turn::StepAction::ShutDown);
	EXPECT_EQ(step.radius, 25.5f);
}

TEST(ReactionTurnRules, TheTimeLimitedTypesAreTheOriginals)
{
	constexpr std::array<uint8_t, 20> k_Original {1,  15, 16, 17, 18, 19, 21, 22, 23, 24,
	                                              25, 26, 27, 29, 31, 32, 34, 35, 38, 40};
	for (uint8_t type = 0; type <= 40; ++type)
	{
		EXPECT_EQ(turn::IsTimeLimited(type), std::ranges::find(k_Original, type) != k_Original.end()) << "type " << +type;
	}
	EXPECT_FALSE(turn::IsTimeLimited(0));
	EXPECT_FALSE(turn::IsTimeLimited(10));
	EXPECT_FALSE(turn::IsTimeLimited(20));
	EXPECT_FALSE(turn::IsTimeLimited(28));
}

TEST(ReactionTurnRules, UnavailableInitiatorRemovesItsReactionsBeforeAnythingElse)
{
	auto facts = Facts();
	facts.initiatorAvailable = false;
	facts.grows = true;
	facts.growth = 0.5f;
	facts.timeLimited = true;
	facts.turnStarted = 900;
	const auto step = turn::Decide(facts);
	EXPECT_EQ(step.action, turn::StepAction::RemoveAllOfInitiator);
	// not grown: the step stops first
	EXPECT_EQ(step.radius, 25.0f);
}
