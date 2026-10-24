/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The villagers' reactions 7 REACT_TO_FOOD and 12 REACT_TO_WOOD and the shared reaction slot
// (docs/wiki/beings/villager-jobs.md#reactions-7-and-12): the pure layer, the effects::reactions additions
// (SetReactionDoneWhen, the standard turns, the same-type flag), villager::IsAvailableForReaction, the slot's whole life (the
// score, the records, AddReaction, ProcessReaction, the same-type refusal, ShutDown), the states 19-22, and the miracles a
// villager runs from (3) or stops to watch (21, 37): the flee point, the priorities, the states 6, 7 and 30; and a thing thrown
// from the hand (9): whether a villager runs or points, the pointing clip's draws, the state 162 and its end; the love states'
// exit and validate and the hand's release of a partner; the breeder's call (30) and its job; and the fire reactions
// (215, 216, 220). The fixture of
// test_villager_build.cpp (a fake state table in the Locator, the turn and the draws scripted). The reactions are made
// with no map: CreateReaction does not spread them, the test hands them to the villager handler itself.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "3D/MapCoords.h"
#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/ReactionRecords.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/FleeGeometry.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/LivingFootpath.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/DynamicsState.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerAnimate.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerCover.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerFire.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Systems/Implementations/VillagerResourceReactions.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerShield.h"
#include "ECS/Systems/Implementations/VillagerStores.h"
#include "ECS/Systems/Implementations/VillagerTeleport.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/Implementations/VillagerWorshipCheck.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerBreeding.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerMourning.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerScript.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Physics/LivingRules.h"
#include "support/LivingActionFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/VillagerFakes.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager = openblack::ecs::villager;
namespace vr = openblack::ecs::villager_reactions;
namespace vrr = openblack::ecs::villager_resource_reactions;
namespace reactions = openblack::ecs::effects::reactions;
namespace tq = openblack::ecs::town_queries;
using openblack::map_coords::MapCoords;

namespace
{
class TestRng final: public RandomNumberManagerInterface
{
	std::mt19937 _generator {1};
	std::mt19937& Generator() override { return _generator; }
	uint32_t _runSeed {0};
	[[nodiscard]] uint32_t GetRunSeed() const override { return _runSeed; }
	void SetRunSeed(uint32_t seed) override { _runSeed = seed; }
	[[nodiscard]] std::mt19937& Stream(RandomStream /*stream*/) override { return _generator; }
	std::optional<std::reference_wrapper<std::mutex>> LockAccess() override { return std::nullopt; }
};

class FakeStateTable final: public test::LivingActionFakeBase
{
public:
	void Update() override {}
	[[nodiscard]] VillagerStates VillagerGetState(const LivingAction& action, LivingAction::Index index) const override
	{
		return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	}
	[[nodiscard]] VillagerStates VillagerGetFinalState(const LivingAction& /*action*/) const override
	{
		return VillagerStates::InvalidState;
	}
	void VillagerSetState(LivingAction&, LivingAction::Index, VillagerStates, bool) const override {}
	uint32_t VillagerCallState(LivingAction&, LivingAction::Index) const override { return 0; }
	uint32_t VillagerCallEntryState(LivingAction&, VillagerStates, VillagerStates, VillagerStates) const override { return 1; }
	uint32_t VillagerCallExitState(LivingAction&, VillagerStates, VillagerStates) const override { return 1; }
	int VillagerCallOutOfAnimation(LivingAction&, LivingAction::Index) const override { return -1; }
	bool VillagerCallValidate(LivingAction&, LivingAction::Index) const override { return false; }
	/// Whether the clip is ready for a new one, and the plays each ask was about
	bool readyForNewClip {false};
	mutable std::vector<uint32_t> readyAsked;
	[[nodiscard]] bool VillagerIsReadyForNewAnimation(const LivingAction& /*action*/, uint32_t times) const override
	{
		readyAsked.push_back(times);
		return readyForNewClip;
	}
};

constexpr auto S(uint32_t n)
{
	return static_cast<VillagerStates>(n);
}

class VillagerReactionsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		auto& v = info->villager.at(0);
		v.hungryForFood = 0.5f;
		v.processChecksEvery = 8;
		v.pauseForASecondChance = 0.01f;
		v.grownUpAge = 13;
		v.life = 1.0f;
		v.moveState = LivingStates::LivingMoveToPos;
		v.speedGroup.speedDefault = static_cast<SpeedState>(100);
		v.maxFoodCarried = 150;
		v.maxWoodCarried = 250;
		v.lifeWhenCrawlsWounded = 0.15f;
		v.damageThresholdToGoHome = 0.3f;
		v.maxDistCarryFoodPit = 100.0f;
		reinterpret_cast<uint32_t*>(&v.isReacting)[7] = 1;
		reinterpret_cast<uint32_t*>(&v.isReacting)[12] = 1;
		// ReactionInfo 7 / 12
		for (const auto [t, priority] : {std::pair {7u, 60u}, std::pair {12u, 55u}})
		{
			auto& r = info->reaction.at(t);
			r.priority = priority;
			r.numGameTurnsForNormalThingsToReact = 2000;
			r.numGameTurnsForNormalThingsBeforeReactingAgain = 0;
			r.whetherItImpresses = 1;
			r.whetherReactionFinishesIfInitiatorInHand = 1;
			r.maxReactionDistance = 35.0f;
			r.howImportantIsDistance = 1.0f;
			r.maxDistanceToRunAwayFromObject = 100.0f;
		}
		info->townDesire.at(0).desireTriggersVillagerAction = 0.01f;
		info->townDesire.at(1).desireTriggersVillagerAction = 0.01f;
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1;
		}
		auto& table = info->villagerStateTable;
		for (const uint32_t s : {19u, 20u, 21u, 22u, 23u, 31u, 32u, 117u, 163u, 215u})
		{
			table.at(s).isFinalState = 1;
			table.at(s).availableForReaction = 1; // available for reactions
		}
		for (const uint32_t s : {19u, 20u, 21u, 22u})
		{
			table.at(s).isReactionState = 1; // reactive
		}
		// info.dat's rows: 19..23 available (field0xa8 = 1) with food /
		// wood interest 0.01 (field0xc8 / field0xcc); 163 available with 0.2 / 0.2
		for (const uint32_t s : {19u, 20u, 21u, 22u, 23u})
		{
			table.at(s).field0xa8 = 1;
			table.at(s).field0xc8 = 0.01f;
			table.at(s).field0xcc = 0.01f;
		}
		table.at(163).field0xa8 = 1;     // GetVillagerAvailableState & 1
		table.at(163).resumeState = 163; // the resume state
		table.at(163).field0xc8 = 0.2f;
		table.at(163).field0xcc = 0.2f;
		// 215 REACT_TO_FIRE: 4 (not available: bit 0 clear), interest 0.01 / 0.01
		table.at(215).field0xa8 = 4;
		table.at(215).field0xc8 = 0.01f;
		table.at(215).field0xcc = 0.01f;
		table.at(1).field0x14 = 1;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::rng::emplace<TestRng>();
		_states = &static_cast<FakeStateTable&>(Locator::livingActionSystem::emplace<FakeStateTable>());
		reactions::Clear();
		vr::Register();
		game_clock::SetTurn(1000);
		// The villager services the code under test reaches: the game's, unless faked here
		Locator::villagerRules::emplace<ecs::systems::VillagerRules>();
		Locator::villagerWorldQueries::emplace<ecs::systems::VillagerWorldQueries>();
		Locator::villagerWorshipCheck::emplace<ecs::systems::VillagerWorshipCheck>();
		Locator::villagerChildFactory::emplace<ecs::systems::VillagerChildFactory>();
		Locator::villagerDiscipleJobs::emplace<ecs::systems::VillagerDiscipleJobs>();
		game_random::testing::SetGameRand([](uint32_t) { return 0u; }, [](float) { return 0.0f; });
		test::FakeVillagerStores stores(std::make_shared<ecs::systems::VillagerStores>());
		stores.temporaryStore = [](entt::entity, const MapCoords& from, ResourceType) {
			return ecs::town_stores::TemporaryStore {entt::null, from};
		};
		Locator::villagerStores::emplace<test::FakeVillagerStores>(stores);
	}

	void TearDown() override
	{
		ClearMiracleMaps();
		reactions::Clear();
		game_random::testing::SetGameRand({}, {});
		Locator::villagerDiscipleJobs::reset();
		Locator::villagerChildFactory::reset();
		Locator::villagerWorshipCheck::reset();
		Locator::villagerWorldQueries::reset();
		Locator::villagerRules::reset();
		Locator::livingActionSystem::reset();
		Locator::rng::reset();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	FakeStateTable* _states {nullptr};

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static void ClearMiracleMaps()
	{
		ecs::villager_fire::Clear();
		ecs::villager_teleport::Clear();
		ecs::villager_shield::Clear();
		ecs::villager_mourning::Clear();
	}

	static entt::entity MakeVillager(glm::vec2 at = {100.0f, 130.0f}, uint32_t top = 163)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		auto& v = registry.Assign<Villager>(e);
		v.life = 1.0f;
		v.town = entt::null;
		v.abode = entt::null;
		v.food = 1.0f;
		v.lastCheckTurn = 1000;
		registry.Assign<LivingAction>(e, S(top), static_cast<uint16_t>(0));
		registry.Assign<Transform>(e, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<WallHug>(e, glm::vec2(), glm::vec2(), 0.0f, 0.1f);
		registry.Assign<Mobile>(e);
		return e;
	}

	static entt::entity MakeTown(uint32_t id = 1)
	{
		auto& registry = Reg();
		const auto town = registry.Create();
		registry.Assign<Town>(town, id);
		registry.Assign<Tribe>(town, Tribe::CELTIC);
		registry.Assign<Transform>(town, glm::vec3(50.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Context().towns[id] = town;
		return town;
	}

	/// A log on the ground (a DeadTree, not in the physics)
	static entt::entity MakeLog(glm::vec2 at)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		registry.Assign<Transform>(e, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<DeadTree>(e, TreeInfo::Pine);
		return e;
	}

	static void Spread(entt::entity villager, uint32_t id, float d)
	{
		const auto* r = reactions::Find(id);
		ASSERT_NE(r, nullptr);
		const auto copy = *r;
		vr::HandleReaction(villager, copy, d);
	}

	static LivingAction& Action(entt::entity e) { return Reg().Get<LivingAction>(e); }
	static Villager& V(entt::entity e) { return Reg().Get<Villager>(e); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }
	static uint32_t Previous(entt::entity e) { return Action(e).states.at(2); }
};
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

TEST(VillagerReactionsPure, Scores)
{
	// IsInterestedInFoodObject: boost x frac x want x s x d + interact, > 0.25 (strict)
	EXPECT_EQ(vrr::FoodInterestScore(1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f), 1.0f);
	EXPECT_EQ(vrr::FoodInterestScore(2.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f), 0.25f);
	EXPECT_EQ(vrr::FoodInterestScore(1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f), 0.0f); // a full villager
	// SET_INTERACT_DESIRE's value is added last: a full villager is then interested in food, and a town's want too
	EXPECT_EQ(vrr::FoodInterestScore(1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f), 0.5f);
	EXPECT_FLOAT_EQ(vrr::TownInterestScore(0.5f, 0.2f, 0.5f, 1.0f, 0.5f), 0.55f);
	// the town term (GetDesire / GetRawDesire x frac x s x d + interact) and the drop-off (> 0.1)
	EXPECT_FLOAT_EQ(vrr::TownInterestScore(0.5f, 0.2f, 0.5f, 1.0f, 0.0f), 0.05f);
	EXPECT_FLOAT_EQ(vrr::FoodDropOffScore(1.0f, 0.5f, 0.2f), 0.1f);
	EXPECT_EQ(vrr::CapacityFraction(75, 150), 0.5f);
	EXPECT_LT(vrr::CapacityFraction(-20, 150), 0.0f);
	// 22's falling log: ftol((1000 / ms) x 100) turns
	EXPECT_EQ(vrr::ReactionFallLimit(100), 1000u);
	EXPECT_EQ(vrr::ReactionFallLimit(50), 2000u);
	// 47..50 and 52
	for (const uint32_t s : {47u, 48u, 49u, 50u, 52u})
	{
		EXPECT_TRUE(vrr::IsForesterWorkState(S(s))) << s;
	}
	for (const uint32_t s : {46u, 51u, 53u, 163u})
	{
		EXPECT_FALSE(vrr::IsForesterWorkState(S(s))) << s;
	}
}

// ---- effects::reactions additions --------------------------------------------------------------------------------

TEST_F(VillagerReactionsTest, StandardTurnsAndFlags)
{
	// ftol((1 + 0.5 x (R - d) / R x distImp) x 2000), R = 45
	EXPECT_EQ(reactions::StandardTurnsToReact(7, 0.0f), 3000u);
	EXPECT_EQ(reactions::StandardTurnsToReact(7, 35.0f), 2222u);
	EXPECT_EQ(reactions::StandardTurnsBeforeReactingAgain(12, 10.0f), 0u);
	// the table's same-type switch flag
	for (const uint8_t t : {10, 28, 35})
	{
		EXPECT_TRUE(reactions::SameTypeSwitch(t)) << static_cast<int>(t);
	}
	for (const uint8_t t : {7, 12, 13, 20, 23})
	{
		EXPECT_FALSE(reactions::SameTypeSwitch(t)) << static_cast<int>(t);
	}
}

TEST_F(VillagerReactionsTest, SetReactionDoneWhen)
{
	const auto v = MakeVillager();
	reactions::SetReactionDoneWhen(v, 7, 100);
	EXPECT_EQ(reactions::RecordTurn(v, 7), 100u);
	reactions::SetReactionDoneWhen(v, 7, 200); // refreshed, not added
	EXPECT_EQ(reactions::RecordTurn(v, 7), 200u);
	EXPECT_EQ(Reg().Get<ReactionRecords>(v).count, 1u);
	reactions::SetReactionDoneWhen(v, 12, 300);
	reactions::SetReactionDoneWhen(v, 13, 400);
	// a fourth: the oldest (the head, 7) goes; no 1800-turn pruning
	reactions::SetReactionDoneWhen(v, 20, 5000);
	EXPECT_EQ(reactions::RecordTurn(v, 7), 0u);
	EXPECT_EQ(reactions::RecordTurn(v, 12), 300u);
	EXPECT_EQ(reactions::RecordTurn(v, 20), 5000u);
}

TEST_F(VillagerReactionsTest, IsAvailableForReaction)
{
	const auto v = MakeVillager();
	EXPECT_TRUE(villager::IsAvailableForReaction(v, Reaction::ReactToWood));
	// at or below LifeWhenCrawlsWounded: only REACT_TO_FOOD
	V(v).life = 0.15f;
	EXPECT_FALSE(villager::IsAvailableForReaction(v, Reaction::ReactToWood));
	EXPECT_TRUE(villager::IsAvailableForReaction(v, Reaction::ReactToFood));
	V(v).life = 1.0f;
	// at the worship site, or playing football
	V(v).flags = Villager::k_FlagAtWorshipSite;
	EXPECT_FALSE(villager::IsAvailableForReaction(v, Reaction::ReactToFood));
	V(v).flags = Villager::k_FlagFootball;
	EXPECT_FALSE(villager::IsAvailableForReaction(v, Reaction::ReactToFood));
	V(v).flags = 0;
	// TOP 31: a final state with field0xec 1 takes one; TOP 56, not final: GetFinalState is FINAL 0, whose field0xec
	// is 0
	Action(v).states.at(0) = 31;
	EXPECT_TRUE(villager::IsAvailableForReaction(v, Reaction::ReactToFood));
	Action(v).states.at(0) = 56; // not final: FINAL 0, whose field0xec is 0
	EXPECT_FALSE(villager::IsAvailableForReaction(v, Reaction::ReactToFood));
}

// ---- the slot ----------------------------------------------------------------------------------------------------

TEST_F(VillagerReactionsTest, WoodReactionLife)
{
	const auto town = MakeTown();
	const auto v = MakeVillager();
	V(v).town = town;
	const auto log = MakeLog({105.0f, 130.0f});
	const auto id = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	// the dead-tree shortcut: priority 55, score ftol(55 x (1 + 0.5 x 30 / 35)) = 78
	EXPECT_EQ(vrr::ReactToWoodPriority(v, id, 0), 55u);
	EXPECT_EQ(vr::ScoreOf(v, id, Reaction::ReactToWood, 5.0f, 0), 78u);
	EXPECT_EQ(vr::ScoreOf(v, id, Reaction::ReactToWood, 36.0f, 0), 0u); // beyond maxReactionDistance
	// the spread: score > 0, Records(12, 0) new -> MarkStarted, SetupReactToWood: AddReaction(r, 21), the slot
	// object = the log
	Spread(v, id, 5.0f);
	EXPECT_EQ(Top(v), 21u);
	EXPECT_EQ(Previous(v), 163u);
	EXPECT_EQ(vr::SlotReaction(v), id);
	EXPECT_EQ(vr::SlotObject(v), log);
	EXPECT_TRUE(vr::IsReacting(v));
	EXPECT_EQ(reactions::RecordTurn(v, 12), 1000u);
	EXPECT_EQ(reactions::Find(id)->turnCreated, 1000u);
	// another log's reaction 12: the same type never switches (its flag is 0)
	const auto log2 = MakeLog({102.0f, 130.0f});
	const auto id2 = reactions::CreateReaction(log2, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	Spread(v, id2, 2.0f);
	EXPECT_EQ(vr::SlotReaction(v), id);
	// 21: the walk to the log's working point with FINAL 22
	EXPECT_EQ(vrr::GotoWoodReaction(Action(v)), 1u);
	EXPECT_EQ(Top(v), 1u);
	EXPECT_EQ(Final(v), 22u);
	const auto wp = ecs::object::GetWorkingPos(log, v);
	EXPECT_EQ(Reg().Get<WallHug>(v).goal, tq::ToMetres({wp.x, wp.z}));
	// ProcessReaction: within the standard turns nothing; past them StopReactingAndSetState (the record gets the turn)
	vr::ProcessSlotReaction(v);
	EXPECT_EQ(vr::SlotReaction(v), id);
	game_clock::SetTurn(1000 + 4000);
	vr::ProcessSlotReaction(v);
	EXPECT_FALSE(vr::SlotHeld(v));
	EXPECT_FALSE(Reg().AllOf<LivingReaction>(v));
	EXPECT_EQ(reactions::RecordTurn(v, 12), 5000u);
	EXPECT_EQ(Top(v), 163u); // PopFromPrevious -> 163's resume
	// ShutDown: the log's reactions removed -> its follower's StopReactingAndSetState at once
	game_clock::SetTurn(6000);
	const auto id3 = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	Spread(v, id3, 5.0f);
	ASSERT_EQ(vr::SlotReaction(v), id3);
	reactions::RemoveAllReactionsInitiatedByObject(log);
	EXPECT_EQ(reactions::Find(id3), nullptr);
	EXPECT_FALSE(vr::SlotHeld(v));
	// the reaction's initiator deleted: ProcessReaction's first test -> StopReacting (no state change)
	game_clock::SetTurn(7000);
	Spread(v, id2, 2.0f);
	ASSERT_EQ(vr::SlotReaction(v), id2);
	Reg().Destroy(log2);
	vr::ProcessSlotReaction(v);
	EXPECT_FALSE(vr::SlotHeld(v));
	EXPECT_EQ(Top(v), 21u);
}

TEST_F(VillagerReactionsTest, WoodReactionGates)
{
	const auto town = MakeTown();
	const auto v = MakeVillager();
	const auto log = MakeLog({105.0f, 130.0f});
	const auto id = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	// no town -> 0
	EXPECT_EQ(vrr::ReactToWoodPriority(v, id, 0), 0u);
	V(v).town = town;
	EXPECT_EQ(vrr::ReactToWoodPriority(v, id, 0), 55u);
	// capacity 0 -> 0; 125 held is no longer below the capacity: the town term (raw wood desire 0) -> 0
	V(v).resourceHeld.at(1) = 250;
	EXPECT_FALSE(vrr::IsInterestedInWoodObject(v, log));
	V(v).resourceHeld.at(1) = 124;
	EXPECT_TRUE(vrr::IsInterestedInWoodObject(v, log));
	V(v).resourceHeld.at(1) = 125;
	EXPECT_FALSE(vrr::IsInterestedInWoodObject(v, log));
	V(v).resourceHeld.at(1) = 0;
	// at or below DamageThresholdToGoHome (0.3) -> 0
	V(v).life = 0.3f;
	EXPECT_FALSE(vrr::IsInterestedInWoodObject(v, log));
	V(v).life = 1.0f;
	// a villager not available for reactions is skipped by the spread
	Action(v).states.at(0) = 56;
	Spread(v, id, 5.0f);
	EXPECT_FALSE(vr::SlotHeld(v));
	EXPECT_EQ(reactions::RecordTurn(v, 12), 0u);
}

TEST_F(VillagerReactionsTest, MiracleTypeWithoutSlotIsUnchanged)
{
	// a REACT_TO_MAGIC_SHIELD whose initiator is no SpellShield: the shield's own gates refuse it, nothing is held
	const auto v = MakeVillager();
	const auto thing = MakeLog({101.0f, 130.0f});
	const auto id = reactions::CreateReaction(thing, Reaction::ReactToMagicShield, PlayerNames::NEUTRAL, false);
	if (id != 0)
	{
		Spread(v, id, 1.0f);
	}
	EXPECT_FALSE(vr::SlotHeld(v));
	EXPECT_FALSE(vr::IsReacting(v));
	EXPECT_EQ(Top(v), 163u);
	// StopReacting with nothing held changes nothing
	vr::StopReacting(v);
	EXPECT_FALSE(Reg().AllOf<LivingReaction>(v));
}

TEST_F(VillagerReactionsTest, OnlyThreeTypesOnlyImpress)
{
	for (uint8_t t = 0; t <= static_cast<uint8_t>(Reaction::ReactToFightWon); ++t)
	{
		EXPECT_EQ(reactions::BeliefButNotReaction(t), t == 34 || t == 39 || t == 40) << static_cast<int>(t);
	}
}

TEST_F(VillagerReactionsTest, AFirePutOutByMagicWaterIsOnlyRecordedAndEndsTheCell)
{
	// the watchers of a fire put out by water: the villager takes nothing up, its record of the type is made, and
	// the spread leaves the cell
	const auto v = MakeVillager();
	const auto spell = MakeLog({101.0f, 130.0f});
	const auto id = reactions::CreateReaction(spell, Reaction::ReactToMagicWaterPuttingOutFire, PlayerNames::PLAYER_ONE, true);
	ASSERT_NE(id, 0u);
	const auto copy = *reactions::Find(id);
	EXPECT_EQ(vr::HandleReaction(v, copy, 1.0f), reactions::CellWalk::Leave);
	EXPECT_EQ(reactions::RecordTurn(v, 34), 1000u);
	EXPECT_FALSE(vr::SlotHeld(v));
	EXPECT_FALSE(vr::IsReacting(v));
	EXPECT_EQ(Top(v), 163u);
	// again in the same turn: not before the turns before reacting again (0 here), still the end of the cell
	game_clock::SetTurn(1000);
	EXPECT_EQ(vr::HandleReaction(v, copy, 1.0f), reactions::CellWalk::Leave);
	EXPECT_EQ(reactions::RecordTurn(v, 34), 1000u);
	// a turn later it is recorded again
	game_clock::SetTurn(1001);
	EXPECT_EQ(vr::HandleReaction(v, copy, 1.0f), reactions::CellWalk::Leave);
	EXPECT_EQ(reactions::RecordTurn(v, 34), 1001u);
	EXPECT_EQ(Top(v), 163u);
}

TEST_F(VillagerReactionsTest, AVillagerNotAvailableLetsTheOnlyImpressingReactionGoOn)
{
	// at the worship site: not available for any reaction, so the next Living of the cell is offered it
	const auto v = MakeVillager();
	V(v).flags = Villager::k_FlagAtWorshipSite;
	const auto spell = MakeLog({101.0f, 130.0f});
	const auto id = reactions::CreateReaction(spell, Reaction::ReactToMagicWaterPuttingOutFire, PlayerNames::PLAYER_ONE, true);
	ASSERT_NE(id, 0u);
	const auto copy = *reactions::Find(id);
	EXPECT_EQ(vr::HandleReaction(v, copy, 1.0f), reactions::CellWalk::Next);
	EXPECT_EQ(reactions::RecordTurn(v, 34), 0u);
}

TEST_F(VillagerReactionsTest, OnlyImpressedWhenTheTypeOnlyImpressesOrTheVillagerCannotBeReached)
{
	EXPECT_FALSE(vr::IsAvailableForBeliefButNotReaction(false, true));
	EXPECT_TRUE(vr::IsAvailableForBeliefButNotReaction(false, false));
	EXPECT_TRUE(vr::IsAvailableForBeliefButNotReaction(true, true));
	EXPECT_TRUE(vr::IsAvailableForBeliefButNotReaction(true, false));
}

TEST_F(VillagerReactionsTest, ABurningThingInTheHandIsReactedToAgainOnlyFarAndStill)
{
	// at least the run-away distance away and no faster than 3 m a second: the standard turns, else at once
	EXPECT_EQ(vr::BurningObjectTurnsBeforeReactingAgain(10.0f, 5.0f, 3.0f, 7), 7u);
	EXPECT_EQ(vr::BurningObjectTurnsBeforeReactingAgain(5.0f, 5.0f, 0.0f, 7), 7u);
	EXPECT_EQ(vr::BurningObjectTurnsBeforeReactingAgain(4.99f, 5.0f, 0.0f, 7), 0u);
	EXPECT_EQ(vr::BurningObjectTurnsBeforeReactingAgain(10.0f, 5.0f, 3.01f, 7), 0u);
}

TEST_F(VillagerReactionsTest, AShieldIsReactedToAgainByItsTownsLastAttack)
{
	namespace vs = ecs::villager_shield;
	// a town not attacked lately: forever, whatever the shield
	EXPECT_EQ(vs::TurnsBeforeReactingToShieldAgain(true, false, true, false, 7), 0x7FFFFFFFu);
	EXPECT_EQ(vs::TurnsBeforeReactingToShieldAgain(true, false, false, false, 7), 0x7FFFFFFFu);
	// no town, or attacked lately: a shield not over the villager's goal at once, else the standard turns
	EXPECT_EQ(vs::TurnsBeforeReactingToShieldAgain(false, false, true, false, 7), 0u);
	EXPECT_EQ(vs::TurnsBeforeReactingToShieldAgain(true, true, true, false, 7), 0u);
	EXPECT_EQ(vs::TurnsBeforeReactingToShieldAgain(true, true, true, true, 7), 7u);
	EXPECT_EQ(vs::TurnsBeforeReactingToShieldAgain(false, false, false, false, 7), 7u);
}

TEST_F(VillagerReactionsTest, AVillagerAtHomeIsOnlyImpressedByAnyReactionAndEndsTheCell)
{
	const auto town = MakeTown();
	const auto outside = MakeVillager();
	V(outside).town = town;
	const auto home = MakeVillager();
	V(home).town = town;
	Reg().Assign<AtHome>(home);
	const auto log = MakeLog({105.0f, 130.0f});
	const auto id = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	const auto copy = *reactions::Find(id);
	// the one outside takes it up as before, and the walk goes on
	EXPECT_EQ(vr::HandleReaction(outside, copy, 5.0f), reactions::CellWalk::Next);
	EXPECT_EQ(vr::SlotReaction(outside), id);
	EXPECT_EQ(Top(outside), 21u);
	// the one at home is only impressed: its record is made, it takes nothing up, and the cell ends
	EXPECT_EQ(vr::HandleReaction(home, copy, 5.0f), reactions::CellWalk::Leave);
	EXPECT_EQ(reactions::RecordTurn(home, 12), 1000u);
	EXPECT_FALSE(vr::SlotHeld(home));
	EXPECT_EQ(Top(home), 163u);
	// a fire too: no fire reaction taken up
	const auto fire = reactions::CreateReaction(log, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	ASSERT_NE(fire, 0u);
	const auto fireCopy = *reactions::Find(fire);
	EXPECT_EQ(vr::HandleReaction(home, fireCopy, 1.0f), reactions::CellWalk::Leave);
	EXPECT_EQ(reactions::RecordTurn(home, 10), 1000u);
	EXPECT_FALSE(ecs::villager_fire::IsReacting(home));
	EXPECT_EQ(Top(home), 163u);
}

TEST_F(VillagerReactionsTest, AVillagerAtHomeNotAvailableLetsTheReactionGoOn)
{
	// at home and at the worship site: not available, so nothing is recorded and the walk goes on
	const auto home = MakeVillager();
	Reg().Assign<AtHome>(home);
	V(home).flags = Villager::k_FlagAtWorshipSite;
	const auto log = MakeLog({105.0f, 130.0f});
	const auto id = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	const auto copy = *reactions::Find(id);
	EXPECT_EQ(vr::HandleReaction(home, copy, 5.0f), reactions::CellWalk::Next);
	EXPECT_EQ(reactions::RecordTurn(home, 12), 0u);
}

// ---- the states --------------------------------------------------------------------------------------------------

TEST_F(VillagerReactionsTest, ArrivesAtFoodReaction)
{
	const auto town = MakeTown();
	const auto v = MakeVillager({100.0f, 130.0f}, 20);
	V(v).town = town;
	// not a PileFood (a plain object): nothing taken; nothing held -> 163
	const auto thing = MakeLog({101.0f, 130.0f});
	const auto id = reactions::CreateReaction(thing, Reaction::ReactToFood, PlayerNames::NEUTRAL, false);
	Reg().Assign<LivingReaction>(v, LivingReaction {id, Reaction::ReactToFood, thing, 1});
	EXPECT_EQ(vrr::ArrivesAtFoodReaction(Action(v)), 1u);
	EXPECT_EQ(Top(v), 163u);
	EXPECT_EQ(V(v).resourceHeld.at(0), 0);
	// food held and hungry -> 117 EAT_FOOD
	Reg().AssignOrReplace<LivingReaction>(v, LivingReaction {id, Reaction::ReactToFood, thing, 1});
	Action(v).states.at(0) = 20;
	V(v).resourceHeld.at(0) = 60;
	V(v).food = 0.2f;
	EXPECT_EQ(vrr::ArrivesAtFoodReaction(Action(v)), 1u);
	EXPECT_EQ(Top(v), 117u);
	// the object gone: StopReactingAndSetState
	Reg().AssignOrReplace<LivingReaction>(v, LivingReaction {id, Reaction::ReactToFood, thing, 1});
	Reg().Destroy(thing);
	Action(v).states.at(0) = 20;
	EXPECT_EQ(vrr::ArrivesAtFoodReaction(Action(v)), 1u);
	EXPECT_FALSE(Reg().AllOf<LivingReaction>(v));
}

TEST_F(VillagerReactionsTest, WoodStatesObjectGone)
{
	const auto v = MakeVillager({100.0f, 130.0f}, 22);
	const auto log = MakeLog({101.0f, 130.0f});
	const auto id = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	Reg().Assign<LivingReaction>(v, LivingReaction {id, Reaction::ReactToWood, log, 1});
	Reg().Destroy(log);
	// 22: SetTopState(163) (ExitReaction stops the reaction then: 163 is not reactive)
	EXPECT_EQ(vrr::ArrivesAtWoodReaction(Action(v)), 1u);
	EXPECT_EQ(Top(v), 163u);
	// 21: StopReactingAndSetState
	Action(v).states.at(0) = 21;
	EXPECT_EQ(vrr::GotoWoodReaction(Action(v)), 1u);
	EXPECT_FALSE(Reg().AllOf<LivingReaction>(v));
}

// ---- the miracle reactions stay identical ----------------------------------------------------------------------

namespace
{
/// What a reaction left on a villager
struct Outcome
{
	uint32_t top {0};
	uint32_t finalState {0};
	uint32_t previous {0};
	bool reacting {false};
	bool slot {false};
	uint32_t record {0};
};
} // namespace

TEST_F(VillagerReactionsTest, MiracleTypesWithoutSlotGoStraightToTheirCode)
{
	// with no slot reaction held the dispatcher runs the miracle ApplyReaction and nothing else (the only path before
	// the slot): the same reaction through vr::HandleReaction and through the miracle function itself, each in a fresh
	// world, leaves the same states, records and maps (fire, teleport, shield, mourning)
	using Apply = void (*)(entt::entity, const reactions::Reaction&);
	const std::array<std::pair<Reaction, Apply>, 4> types {{
	    {Reaction::ReactToFire, &ecs::villager_fire::ApplyReaction},
	    {Reaction::ReactToTeleport, &ecs::villager_teleport::ApplyReaction},
	    {Reaction::ReactToMagicShield, &ecs::villager_shield::ApplyReaction},
	    {Reaction::ReactToDeath, &ecs::villager_mourning::ApplyReaction},
	}};
	const auto run = [](Reaction type, Apply direct) {
		ClearMiracleMaps();
		reactions::Clear();
		Locator::entitiesRegistry::reset();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		const auto v = MakeVillager();
		const auto initiator = MakeLog({101.0f, 130.0f});
		const auto id = reactions::CreateReaction(initiator, type, PlayerNames::NEUTRAL, false);
		if (const auto* r = reactions::Find(id); r != nullptr)
		{
			const auto copy = *r;
			if (direct != nullptr)
			{
				direct(v, copy);
			}
			else
			{
				vr::HandleReaction(v, copy, 1.0f);
			}
		}
		return Outcome {Top(v),
		                Final(v),
		                Previous(v),
		                vr::IsReacting(v),
		                Reg().AllOf<LivingReaction>(v),
		                reactions::RecordTurn(v, static_cast<uint8_t>(type))};
	};
	for (const auto& [type, direct] : types)
	{
		const auto viaDispatcher = run(type, nullptr);
		const auto viaMiracle = run(type, direct);
		const auto t = static_cast<int>(type);
		EXPECT_EQ(viaDispatcher.top, viaMiracle.top) << t;
		EXPECT_EQ(viaDispatcher.finalState, viaMiracle.finalState) << t;
		EXPECT_EQ(viaDispatcher.previous, viaMiracle.previous) << t;
		EXPECT_EQ(viaDispatcher.reacting, viaMiracle.reacting) << t;
		EXPECT_EQ(viaDispatcher.record, viaMiracle.record) << t;
		EXPECT_FALSE(viaDispatcher.slot) << t;
		EXPECT_FALSE(viaMiracle.slot) << t;
	}
}

TEST_F(VillagerReactionsTest, MiracleTypeMeetingSlotHolder)
{
	const auto town = MakeTown();
	const auto v = MakeVillager();
	V(v).town = town;
	const auto log = MakeLog({105.0f, 130.0f});
	const auto wood = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	Spread(v, wood, 5.0f);
	ASSERT_EQ(vr::SlotReaction(v), wood);
	const auto before = Reg().Get<const LivingReaction>(v);
	const auto thing = MakeLog({101.0f, 130.0f});
	for (const auto type :
	     {Reaction::ReactToFire, Reaction::ReactToTeleport, Reaction::ReactToMagicShield, Reaction::ReactToDeath})
	{
		const auto id = reactions::CreateReaction(thing, type, PlayerNames::NEUTRAL, false);
		if (id == 0)
		{
			continue;
		}
		// a miracle reaction that does not take the villager (its own priority refuses a plain object): the slot is
		// kept as it was and no miracle map holds the villager
		Spread(v, id, 1.0f);
		const auto t = static_cast<int>(type);
		ASSERT_TRUE(Reg().AllOf<LivingReaction>(v)) << t;
		const auto& after = Reg().Get<const LivingReaction>(v);
		EXPECT_EQ(after.reaction, before.reaction) << t;
		EXPECT_EQ(after.object, before.object) << t;
		EXPECT_EQ(after.joinOrder, before.joinOrder) << t;
		EXPECT_EQ(Top(v), 21u) << t;
		EXPECT_FALSE(ecs::villager_fire::IsReacting(v)) << t;
		EXPECT_FALSE(ecs::villager_teleport::IsReacting(v)) << t;
		EXPECT_FALSE(ecs::villager_shield::IsReacting(v)) << t;
		EXPECT_FALSE(ecs::villager_mourning::IsReacting(v)) << t;
	}
	// IsAvailableForReaction(type) first, for every type: a wounded slot holder (life at
	// LifeWhenCrawlsWounded) is refused anything but REACT_TO_FOOD before the switch rule is looked at
	V(v).life = 0.15f;
	EXPECT_FALSE(villager::IsAvailableForReaction(v, Reaction::ReactToFire));
	const auto fire = reactions::CreateReaction(thing, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	if (fire != 0)
	{
		Spread(v, fire, 1.0f);
	}
	EXPECT_EQ(vr::SlotReaction(v), wood);
	EXPECT_FALSE(ecs::villager_fire::IsReacting(v));
}

TEST_F(VillagerReactionsTest, FoodToWoodSwitchAfterTenSeconds)
{
	const auto town = MakeTown();
	const auto v = MakeVillager({100.0f, 130.0f}, 19);
	V(v).town = town;
	// it follows a food reaction (the slot as SetupReactToFood leaves it) done at turn 905, 10 turns a second
	const auto pile = MakeLog({110.0f, 130.0f});
	const auto food = reactions::CreateReaction(pile, Reaction::ReactToFood, PlayerNames::NEUTRAL, false);
	ASSERT_NE(food, 0u);
	Reg().Assign<LivingReaction>(v, LivingReaction {food, Reaction::ReactToFood, pile, 1});
	reactions::SetReactionDoneWhen(v, 7, 905);
	const auto log = MakeLog({105.0f, 130.0f});
	const auto wood = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	ASSERT_NE(wood, 0u);
	// (1000 - 905) / 10 = 9 s < max(10, cur / new x 20 - 10) = 10 -> kept
	Spread(v, wood, 5.0f);
	EXPECT_EQ(vr::SlotReaction(v), food);
	EXPECT_EQ(Top(v), 19u);
	// 10 s -> the switch: SetReactionDoneWhen(12, now) and the wood setup (AddReaction(r, 21))
	game_clock::SetTurn(1005);
	Spread(v, wood, 5.0f);
	EXPECT_EQ(vr::SlotReaction(v), wood);
	EXPECT_EQ(vr::SlotObject(v), log);
	EXPECT_EQ(Top(v), 21u);
	EXPECT_EQ(reactions::RecordTurn(v, 12), 1005u);
	EXPECT_EQ(reactions::RecordTurn(v, 7), 905u); // the switch unlinks with no record refresh
}

TEST_F(VillagerReactionsTest, ShutDownWithTwoFollowers)
{
	const auto town = MakeTown();
	const auto older = MakeVillager({100.0f, 130.0f});
	const auto newer = MakeVillager({101.0f, 130.0f});
	V(older).town = town;
	V(newer).town = town;
	const auto log = MakeLog({105.0f, 130.0f});
	const auto id = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	Spread(older, id, 5.0f);
	Spread(newer, id, 4.0f);
	ASSERT_EQ(vr::SlotReaction(older), id);
	ASSERT_EQ(vr::SlotReaction(newer), id);
	// the head of the followers is the newest (AddReaction links at the head); ShutDown stops the
	// head first, then the next one, until none follows
	EXPECT_GT(Reg().Get<const LivingReaction>(newer).joinOrder, Reg().Get<const LivingReaction>(older).joinOrder);
	reactions::RemoveAllReactionsInitiatedByObject(log);
	EXPECT_EQ(reactions::Find(id), nullptr);
	for (const auto v : {older, newer})
	{
		EXPECT_FALSE(vr::SlotHeld(v));
		EXPECT_FALSE(Reg().AllOf<LivingReaction>(v));
		EXPECT_EQ(Top(v), 163u); // StopReactingAndSetState: PopFromPrevious -> 163's resume
	}
}

TEST_F(VillagerReactionsTest, AddReactionFromAMiracleMap)
{
	const auto town = MakeTown();
	const auto v = MakeVillager();
	V(v).town = town;
	// the villager follows a fire reaction (VillagerFire's map: SetupReactToFire stores 163 and goes to 215)
	const auto thing = MakeLog({101.0f, 130.0f});
	const auto fire = reactions::CreateReaction(thing, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	ASSERT_NE(fire, 0u);
	ecs::villager_fire::SetupReactToFire(v, thing, fire);
	ASSERT_TRUE(ecs::villager_fire::IsReacting(v));
	ASSERT_EQ(Top(v), 215u);
	ASSERT_EQ(Previous(v), 163u);
	// a wood reaction above it (the fire scores 0 for an object with no fire, and it has no fire record: past 10 s):
	// the switch, then AddReaction: no StorePreviousState (the previous state is held), SetTopState(21), the fire map
	// dropped, the slot. In 215 the villager is not available (field0xa8 = 4), so the dead-tree shortcut is off and the
	// log is wanted only through the town: raw wood desire 10 x capacity 1 x interest 0.01 x the distance term > the
	// trigger 0.01
	Reg().Get<Town>(town).desire.raw.at(1) = 10.0f;
	const auto log = MakeLog({105.0f, 130.0f});
	const auto wood = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	Spread(v, wood, 5.0f);
	EXPECT_FALSE(ecs::villager_fire::IsReacting(v));
	EXPECT_EQ(vr::SlotReaction(v), wood);
	EXPECT_EQ(vr::SlotObject(v), log);
	EXPECT_EQ(Top(v), 21u);
	EXPECT_EQ(Previous(v), 163u);
	EXPECT_EQ(reactions::RecordTurn(v, 12), 1000u);
}

// ---- the fire reactions (215 REACT_TO_FIRE, 216 PUT_OUT_FIRE_BY_BEATING, 220 MOVE_AROUND_FIRE) --------------------

namespace
{
/// A fire burning hot on the thing, listed in the game's fire system as a new fire is
ecs::fire::FireEffect& Burn(entt::entity thing)
{
	auto& fires = Locator::fireSystem::value();
	auto fire = std::make_unique<ecs::fire::FireEffect>();
	fire->id = fires.TakeId();
	fire->object = thing;
	fire->temperature = 1000.0f;
	fire->previous = 1000.0f;
	return fires.Insert(std::move(fire));
}
} // namespace

TEST_F(VillagerReactionsTest, AFireReactionWithNoTownWalksRoundTheFireTowardsItsGoal)
{
	// facing the burning thing, with no town to fight the fire for: the walk round it (220) towards where it was going,
	// and the state it had stored after it
	const auto v = MakeVillager();
	Reg().Get<WallHug>(v).goal = glm::vec2(160.0f, 130.0f);
	const auto thing = MakeLog({120.0f, 130.0f});
	const auto id = reactions::CreateReaction(thing, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	const auto& fire = Burn(thing);
	ecs::villager_fire::SetupReactToFire(v, thing, id);
	ASSERT_EQ(Top(v), 215u);
	villager::SetGameAngle(v, tq::GetAngleFromXZ(tq::PosOf(v), tq::PosOf(thing)));
	EXPECT_EQ(ecs::villager_fire::ReactToFire(Action(v)), 1u);
	EXPECT_EQ(Top(v), 220u);
	EXPECT_EQ(Previous(v), 163u);
	const auto& state = Reg().Get<const VillagerFireState>(v);
	EXPECT_EQ(state.fire, fire.id);
	EXPECT_EQ(state.savedDestination, glm::vec2(160.0f, 130.0f));
}

namespace
{
/// How far the villager is from facing the thing, the short way, in 2048ths of a turn
int TurnLeft(entt::entity villager, entt::entity thing)
{
	const int target = tq::GetAngleFromXZ(tq::PosOf(villager), tq::PosOf(thing));
	const int d = std::abs(target - static_cast<int>(villager::GetGameAngle(villager))) & 0x7FF;
	return d > 0x400 ? 0x800 - d : d;
}

/// The villager faces straight away from the thing
void FaceAway(entt::entity villager, entt::entity thing)
{
	const auto target = tq::GetAngleFromXZ(tq::PosOf(villager), tq::PosOf(thing));
	villager::SetGameAngle(villager, static_cast<uint16_t>((target + 0x400) & 0x7FF));
}
} // namespace

TEST_F(VillagerReactionsTest, AFireReactionTurnsToTheFireAStepATurnBeforeItDecides)
{
	// no town, so once it faces the fire it walks round it (220) towards where it was going
	const auto v = MakeVillager();
	const auto thing = MakeLog({120.0f, 130.0f});
	const auto id = reactions::CreateReaction(thing, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	const auto& fire = Burn(thing);
	ecs::villager_fire::SetupReactToFire(v, thing, id);
	ASSERT_EQ(Top(v), 215u);
	FaceAway(v, thing);
	ASSERT_EQ(TurnLeft(v, thing), 0x400);
	// half a turn away: four turns of an eighth each, the state returning 1 with nothing else done, the fire noted
	for (int turn = 1; turn <= 4; ++turn)
	{
		EXPECT_EQ(ecs::villager_fire::ReactToFire(Action(v)), 1u) << turn;
		EXPECT_EQ(TurnLeft(v, thing), 0x400 - turn * 0x100) << turn;
		EXPECT_EQ(Top(v), 215u) << turn;
		EXPECT_EQ(Reg().Get<const VillagerFireState>(v).fire, fire.id) << turn;
	}
	// facing it, the next turn decides
	EXPECT_EQ(ecs::villager_fire::ReactToFire(Action(v)), 1u);
	EXPECT_EQ(TurnLeft(v, thing), 0);
	EXPECT_EQ(Top(v), 220u);
	EXPECT_EQ(Previous(v), 163u);
}

TEST_F(VillagerReactionsTest, AFireReactionFacingTheFireWithinAStepDecidesAtOnce)
{
	const auto v = MakeVillager();
	const auto thing = MakeLog({120.0f, 130.0f});
	const auto id = reactions::CreateReaction(thing, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	Burn(thing);
	ecs::villager_fire::SetupReactToFire(v, thing, id);
	// less than a step (0x100) away: it snaps to face it and decides the same turn
	const auto target = tq::GetAngleFromXZ(tq::PosOf(v), tq::PosOf(thing));
	villager::SetGameAngle(v, static_cast<uint16_t>((target + 0xFF) & 0x7FF));
	EXPECT_EQ(ecs::villager_fire::ReactToFire(Action(v)), 1u);
	EXPECT_EQ(TurnLeft(v, thing), 0);
	EXPECT_EQ(Top(v), 220u);
}

TEST_F(VillagerReactionsTest, BeatingAFireTurnsToItBeforeTheFirstBlow)
{
	const auto v = MakeVillager();
	const auto thing = MakeLog({120.0f, 130.0f});
	const auto id = reactions::CreateReaction(thing, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	auto& fire = Burn(thing);
	ecs::villager_fire::SetupReactToFire(v, thing, id);
	Reg().Get<VillagerFireState>(v).fire = fire.id;
	Action(v).states.at(0) = 216;
	// beside the fire: a metre beyond the villager's and the fire's reach, inside the 2 m band
	const float keep = std::max(fire.SafeFireRadius(), ecs::object::GetRadius(thing));
	const float reach = ecs::object::GetRadius(v) + keep;
	Reg().Get<Transform>(v).position = glm::vec3(120.0f - reach - 1.0f, 0.0f, 130.0f);
	FaceAway(v, thing);
	// half a turn away: four turns of an eighth each, with no blow
	for (int turn = 1; turn <= 4; ++turn)
	{
		EXPECT_EQ(ecs::villager_fire::PutOutFireByBeating(Action(v)), 1u) << turn;
		EXPECT_EQ(TurnLeft(v, thing), 0x400 - turn * 0x100) << turn;
		EXPECT_EQ(fire.temperature, 1000.0f) << turn;
		EXPECT_EQ(Top(v), 216u) << turn;
	}
	// facing it, the blow (a burn of -8) cools the fire
	EXPECT_EQ(ecs::villager_fire::PutOutFireByBeating(Action(v)), 1u);
	EXPECT_EQ(TurnLeft(v, thing), 0);
	EXPECT_LT(fire.temperature, 1000.0f);
	EXPECT_EQ(Top(v), 216u);
}

namespace
{
/// A fake of the teleport stones that answers whether a villager reacts and records what the villager side asks of it
class FakeTeleportStones final: public ecs::systems::TeleportSystemInterface
{
public:
	struct Asked
	{
		entt::entity stone {entt::null};
		entt::entity living {entt::null};
	};

	[[nodiscard]] bool CanPlaceStone(glm::vec3 /*point*/) const override { return false; }
	[[nodiscard]] bool ShouldReact(entt::entity stone, entt::entity living) const override
	{
		shouldReact = Asked {stone, living};
		return reacts;
	}
	bool DoTeleport(entt::entity stone, entt::entity living, bool forced) override
	{
		teleported = Asked {stone, living};
		teleportForced = forced;
		return true;
	}
	bool DropOnStone(entt::entity /*villager*/, entt::entity /*stone*/, PlayerNames /*dropper*/) override { return false; }
	[[nodiscard]] std::optional<entt::entity> RouteStoneFor(PlayerNames /*player*/, glm::vec3 /*worshipper*/,
	                                                        glm::vec3 /*site*/, float /*maxDistance*/) const override
	{
		return std::nullopt;
	}
	void ProcessTurn() override {}
	void Reset() override {}
	[[nodiscard]] bool CanDropOnStone(entt::entity /*villager*/, entt::entity /*stone*/) const override { return false; }
	void RegisterDestination(entt::entity stone, entt::entity living, glm::vec3 destination) override
	{
		registered = Asked {stone, living};
		registeredDestination = destination;
	}
	[[nodiscard]] uint32_t NextStoneSerial() override { return 0; }
	void Update(float /*seconds*/) override {}
	void RunDebugHooks() override {}

	bool reacts {false};
	mutable std::optional<Asked> shouldReact;
	std::optional<Asked> registered;
	std::optional<glm::vec3> registeredDestination;
	std::optional<Asked> teleported;
	std::optional<bool> teleportForced;
};
} // namespace

TEST_F(VillagerReactionsTest, TeleportReactionAsksTheStones)
{
	// the villager side of REACT_TO_TELEPORT goes to the teleport stones (Locator::teleportSystem): whether it reacts,
	// the destination the stone keeps, and the jump, each with the same stone and villager
	test::RestoreService<Locator::teleportSystem> restore;
	auto& fake = static_cast<FakeTeleportStones&>(Locator::teleportSystem::emplace<FakeTeleportStones>());
	// the fixture's info is made here, not const: its REACT_TO_TELEPORT row gets a priority of its own
	const_cast<ReactionInfo&>(Locator::infoConstants::value().reaction.at(static_cast<size_t>(Reaction::ReactToTeleport)))
	    .priority = 70;
	const auto v = MakeVillager();
	Reg().Get<WallHug>(v).goal = glm::vec2(140.0f, 170.0f);
	const auto stone = Reg().Create();
	Reg().Assign<Transform>(stone, glm::vec3(900.0f, 0.0f, 900.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<TeleportStone>(stone);
	const auto id = reactions::CreateReaction(stone, Reaction::ReactToTeleport, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);

	// the priority: none unless the stones say the villager reacts, then the reaction's own
	fake.reacts = false;
	EXPECT_EQ(ecs::villager_teleport::ReactToTeleportPriority(v, id), 0u);
	ASSERT_TRUE(fake.shouldReact.has_value());
	EXPECT_EQ(fake.shouldReact->stone, stone);
	EXPECT_EQ(fake.shouldReact->living, v);
	fake.reacts = true;
	EXPECT_EQ(ecs::villager_teleport::ReactToTeleportPriority(v, id), 70u);

	// taking it up: the stone is told where the villager was going (its wall hug's goal)
	ecs::villager_teleport::SetupReactToTeleport(v, stone, id);
	ASSERT_TRUE(fake.registered.has_value());
	EXPECT_EQ(fake.registered->stone, stone);
	EXPECT_EQ(fake.registered->living, v);
	ASSERT_TRUE(fake.registeredDestination.has_value());
	EXPECT_EQ(*fake.registeredDestination, glm::vec3(140.0f, 0.0f, 170.0f));
	EXPECT_EQ(ecs::villager_teleport::ReactionObject(v), stone);

	// at the stone: the jump, not forced
	EXPECT_EQ(ecs::villager_teleport::TeleportReaction(Reg().Get<LivingAction>(v)), 1u);
	ASSERT_TRUE(fake.teleported.has_value());
	EXPECT_EQ(fake.teleported->stone, stone);
	EXPECT_EQ(fake.teleported->living, v);
	ASSERT_TRUE(fake.teleportForced.has_value());
	EXPECT_FALSE(*fake.teleportForced);
}

TEST_F(VillagerReactionsTest, FinalDestinationOnAFootpathIsItsEndNodeInTheWalksDirection)
{
	const auto v = MakeVillager();
	Reg().Get<WallHug>(v).goal = glm::vec2(140.0f, 170.0f);
	// a footpath of three nodes along x, the last one hidden
	const auto path = Reg().Create();
	auto& footpath = Reg().Assign<Footpath>(path);
	for (const auto& [x, flags] : std::array<std::pair<float, uint8_t>, 3> {{{10.0f, 1}, {20.0f, 1}, {30.0f, 4}}})
	{
		footpath.nodes.push_back(
		    {glm::vec3(x, 0.0f, 50.0f), map_coords::FromMetres(glm::vec2(x, 50.0f)), flags, footpath.nextNodeId++});
	}
	// no node: the wall hug's goal
	auto& walk = Reg().Assign<LivingFootpath>(v, LivingFootpath {.footpath = path});
	EXPECT_EQ(ecs::villager_teleport::FinalDestination(v), glm::vec3(140.0f, 0.0f, 170.0f));
	// on a node, away from the head: the last node that is not hidden
	walk.node = 0;
	const auto last = ecs::villager_teleport::FinalDestination(v);
	EXPECT_FLOAT_EQ(last.x, map_coords::ToMetres(map_coords::FromMetres(glm::vec2(20.0f, 50.0f)).x));
	EXPECT_FLOAT_EQ(last.z, map_coords::ToMetres(map_coords::FromMetres(glm::vec2(20.0f, 50.0f)).z));
	// toward the head: the first
	walk.reverse = true;
	EXPECT_FLOAT_EQ(ecs::villager_teleport::FinalDestination(v).x,
	                map_coords::ToMetres(map_coords::FromMetres(glm::vec2(10.0f, 50.0f)).x));
}

TEST(VillagerTeleport, MovedSinceTurnStartComparesTheMapsWholeCoordinatesInXAndZ)
{
	using ecs::villager_teleport::MovedSinceTurnStart;
	const glm::vec3 start {10.0f, 3.0f, 20.0f};
	EXPECT_FALSE(MovedSinceTurnStart(start, start));
	// the height does not count
	EXPECT_FALSE(MovedSinceTurnStart(start, {10.0f, 9.0f, 20.0f}));
	// one whole unit (65536 to 65537) in x, or in z, does
	EXPECT_TRUE(MovedSinceTurnStart(start, {10.0002f, 3.0f, 20.0f}));
	EXPECT_TRUE(MovedSinceTurnStart(start, {10.0f, 3.0f, 19.9998f}));
	// a move within one unit (65536.06 truncated to 65536) does not
	EXPECT_EQ(map_coords::ToFixed(10.00001f), map_coords::ToFixed(10.0f));
	EXPECT_FALSE(MovedSinceTurnStart(start, {10.00001f, 3.0f, 20.0f}));
}

TEST_F(VillagerReactionsTest, IsMovingIsWhetherItMovedSinceTheStartOfItsTurn)
{
	const auto v = MakeVillager();
	auto& transform = Reg().Get<Transform>(v);
	// before its first turn: not moving
	EXPECT_FALSE(ecs::villager_teleport::IsMoving(v));
	// its turn starts where it stands: walking, with a speed, it has not moved yet
	Reg().Assign<DrawPosition>(v, DrawPosition {.turnStart = transform.position, .started = true});
	Reg().Assign<MoveStateLinearTag>(v);
	EXPECT_FALSE(ecs::villager_teleport::IsMoving(v));
	// moved during the turn, in any state
	Reg().Remove<MoveStateLinearTag>(v);
	transform.position.x += 1.0f;
	EXPECT_TRUE(ecs::villager_teleport::IsMoving(v));
	// put somewhere by a script: its turn starts there, so it has not moved
	transform.position.z += 5.0f;
	ecs::SnapTurnStart(Reg(), v);
	EXPECT_FALSE(ecs::villager_teleport::IsMoving(v));
}

// ---- the miracles a villager runs from or watches ----------------------------------------------------------------

namespace flee = openblack::ecs::effects::flee;

TEST(VillagerFleeGeometry, FleeingPositionFromStill)
{
	// straight away from it, ten metres in the plane
	EXPECT_EQ(flee::FleeingPositionFromStill({0.0f, 0.0f}, {3.0f, 4.0f}, 10.0f), glm::vec2(-6.0f, -8.0f));
	EXPECT_EQ(flee::FleeingPositionFromStill({100.0f, 130.0f}, {110.0f, 130.0f}, 10.0f), glm::vec2(90.0f, 130.0f));
	// standing on it: it stays
	EXPECT_EQ(flee::FleeingPositionFromStill({5.0f, 5.0f}, {5.0f, 5.0f}, 10.0f), glm::vec2(5.0f, 5.0f));
}

TEST(VillagerFleeGeometry, FleeingPositionFromMoving)
{
	// a miracle going towards -x along z = 130: ten metres across its way on the villager's side, each axis its draw
	// (1 for x, then 3 for z) less 4
	EXPECT_EQ(flee::FleeingPositionFromMoving({100.0f, 135.0f}, {130.0f, 130.0f}, {-1.0f, 0.0f}, 10.0f, 1.0f, 3.0f),
	          glm::vec2(103.0f, 146.0f));
	// the villager on the other side of its way runs the other way
	EXPECT_EQ(flee::FleeingPositionFromMoving({100.0f, 125.0f}, {130.0f, 130.0f}, {-1.0f, 0.0f}, 10.0f, 1.0f, 3.0f),
	          glm::vec2(97.0f, 114.0f));
	// the way's length does not count
	EXPECT_EQ(flee::FleeingPositionFromMoving({100.0f, 135.0f}, {130.0f, 130.0f}, {-5.0f, 0.0f}, 10.0f, 1.0f, 3.0f),
	          glm::vec2(103.0f, 146.0f));
	// straight up or down: no side, only the random shift
	EXPECT_EQ(flee::FleeingPositionFromMoving({100.0f, 135.0f}, {130.0f, 130.0f}, {0.0f, 0.0f}, 10.0f, 1.0f, 3.0f),
	          glm::vec2(97.0f, 134.0f));
}

TEST(VillagerFleeGeometry, ComingTowards)
{
	const glm::vec3 me(0.0f, 0.0f, 0.0f);
	const glm::vec3 at(10.0f, 0.0f, 0.0f);
	EXPECT_TRUE(flee::ComingTowards(me, at, {-1.0f, 0.0f, 0.0f}));
	// within the cosine 0.8 (2 / sqrt(5)), not beyond it (1 / sqrt(2))
	EXPECT_TRUE(flee::ComingTowards(me, at, {-2.0f, 0.0f, 1.0f}));
	EXPECT_FALSE(flee::ComingTowards(me, at, {-1.0f, 0.0f, 1.0f}));
	EXPECT_FALSE(flee::ComingTowards(me, at, {0.0f, 0.0f, 1.0f}));
	EXPECT_FALSE(flee::ComingTowards(me, at, {1.0f, 0.0f, 0.0f}));
	// still, or standing on it: not coming
	EXPECT_FALSE(flee::ComingTowards(me, at, {0.0f, 0.0f, 0.0f}));
	EXPECT_FALSE(flee::ComingTowards(me, me, {-1.0f, 0.0f, 0.0f}));
	// the heights count: high above, flying level, it passes over
	EXPECT_FALSE(flee::ComingTowards(me, {10.0f, 20.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}));
	EXPECT_TRUE(flee::ComingTowards(me, {10.0f, 20.0f, 0.0f}, {-1.0f, -2.0f, 0.0f}));
}

TEST(VillagerFleeGeometry, FleeFromSpellPriority)
{
	// the priority, plus 100 x (600000 - d) / 600000 within 600000 whole units, truncated
	EXPECT_EQ(flee::FleeFromSpellPriority(150, 0), 250u);
	EXPECT_EQ(flee::FleeFromSpellPriority(150, 65536), 239u);
	EXPECT_EQ(flee::FleeFromSpellPriority(150, 300000), 200u);
	EXPECT_EQ(flee::FleeFromSpellPriority(150, 599999), 150u);
	EXPECT_EQ(flee::FleeFromSpellPriority(150, 600000), 150u);
	EXPECT_EQ(flee::FleeFromSpellPriority(150, 2000000), 150u);
	// kept to a byte
	EXPECT_EQ(flee::FleeFromSpellPriority(200, 0), 44u);
}

namespace
{
/// The info.dat rows of the three miracle reactions, the villager reacting to them and the states 6, 7 and 30
void SetUpMiracleRows()
{
	auto& info = const_cast<InfoConstants&>(Locator::infoConstants::value());
	struct Row
	{
		Reaction type;
		uint32_t priority;
		uint32_t turns;
		uint32_t again;
		float reach;
		float importance;
		uint32_t finishesInHand;
	};
	for (const auto& row : {Row {Reaction::FleeFromSpell, 150, 80, 0, 50.0f, 0.1f, 0},
	                        Row {Reaction::LookAtNiceSpell, 120, 80, 15, 60.0f, 0.25f, 1},
	                        Row {Reaction::ReactToImpressiveSpell, 120, 50, 100, 60.0f, 0.2f, 1}})
	{
		auto& r = info.reaction.at(static_cast<size_t>(row.type));
		r.priority = row.priority;
		r.numGameTurnsForNormalThingsToReact = row.turns;
		r.numGameTurnsForNormalThingsBeforeReactingAgain = row.again;
		r.maxReactionDistance = row.reach;
		r.howImportantIsDistance = row.importance;
		r.whetherReactionFinishesIfInitiatorInHand = row.finishesInHand;
		r.minDistanceToRunAwayFromObject = 50.0f;
		r.maxDistanceToRunAwayFromObject = 100.0f;
		reinterpret_cast<uint32_t*>(&info.villager.at(0).isReacting)[static_cast<size_t>(row.type)] = 1;
	}
	for (const uint32_t s : {6u, 7u, 30u})
	{
		auto& state = info.villagerStateTable.at(s);
		state.isFinalState = 1;
		state.availableForReaction = 1;
		state.isReactionState = 1;
	}
}

/// A miracle with no Transform, where it is (y its altitude above the land) and going along `movement`
entt::entity MakeMiracle(glm::vec3 at, glm::vec3 movement)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto e = registry.Create();
	auto& spell = registry.Assign<Spell>(e);
	spell.position = at;
	spell.movementDirection = movement;
	return e;
}
} // namespace

TEST_F(VillagerReactionsTest, FleeFromSpellPriorityByDistance)
{
	SetUpMiracleRows();
	const auto v = MakeVillager({100.0f, 130.0f});
	// 10 m away (65536 whole units): 150 + 89
	const auto near = MakeMiracle({110.0f, 0.0f, 130.0f}, {});
	const auto nearId = reactions::CreateReaction(near, Reaction::FleeFromSpell, PlayerNames::NEUTRAL, false);
	ASSERT_NE(nearId, 0u);
	EXPECT_EQ(vr::FleeFromSpellPriority(v, nearId, 0), 239u);
	// 100 m away: the priority alone
	const auto far = MakeMiracle({200.0f, 0.0f, 130.0f}, {});
	const auto farId = reactions::CreateReaction(far, Reaction::FleeFromSpell, PlayerNames::NEUTRAL, false);
	ASSERT_NE(farId, 0u);
	EXPECT_EQ(vr::FleeFromSpellPriority(v, farId, 0), 150u);
}

TEST_F(VillagerReactionsTest, NiceSpellPriorityNotForItsMaker)
{
	SetUpMiracleRows();
	const auto maker = MakeVillager({100.0f, 130.0f});
	const auto other = MakeVillager({101.0f, 130.0f});
	const auto spell = MakeMiracle({110.0f, 0.0f, 130.0f}, {});
	Reg().Get<Spell>(spell).creator = SpellCreator {SpellCreator::Kind::Thing, PlayerNames::NEUTRAL, maker};
	for (const auto type : {Reaction::LookAtNiceSpell, Reaction::ReactToImpressiveSpell})
	{
		const auto id = reactions::CreateReaction(spell, type, PlayerNames::NEUTRAL, false);
		ASSERT_NE(id, 0u);
		EXPECT_EQ(vr::LookAtNiceSpellPriority(maker, id, 0), 0u);
		EXPECT_EQ(vr::LookAtNiceSpellPriority(other, id, 0), 120u);
	}
}

TEST_F(VillagerReactionsTest, NoMiracleReactionOutOfReach)
{
	SetUpMiracleRows();
	const auto v = MakeVillager({100.0f, 130.0f});
	const auto spell = MakeMiracle({110.0f, 0.0f, 130.0f}, {});
	// beyond each type's maxReactionDistance (50, 60, 60) it scores nothing and takes nothing
	const auto refuses = [&](Reaction type, float beyond) {
		const auto id = reactions::CreateReaction(spell, type, PlayerNames::NEUTRAL, false);
		ASSERT_NE(id, 0u);
		EXPECT_EQ(vr::ScoreOf(v, id, type, beyond, 0), 0u) << static_cast<int>(type);
		Spread(v, id, beyond);
		EXPECT_FALSE(vr::SlotHeld(v)) << static_cast<int>(type);
		EXPECT_EQ(Top(v), 163u) << static_cast<int>(type);
	};
	refuses(Reaction::FleeFromSpell, 50.5f);
	refuses(Reaction::LookAtNiceSpell, 60.5f);
	refuses(Reaction::ReactToImpressiveSpell, 60.5f);
}

TEST_F(VillagerReactionsTest, FleeFromAStillMiracle)
{
	SetUpMiracleRows();
	const auto v = MakeVillager({100.0f, 130.0f});
	const auto spell = MakeMiracle({110.0f, 0.0f, 130.0f}, {});
	const auto id = reactions::CreateReaction(spell, Reaction::FleeFromSpell, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	// within reach: it takes the reaction, keeps the miracle and starts running (6)
	Spread(v, id, 10.0f);
	EXPECT_EQ(vr::SlotReaction(v), id);
	EXPECT_EQ(vr::SlotObject(v), spell);
	EXPECT_EQ(Top(v), 6u);
	EXPECT_EQ(Previous(v), 163u);
	// 10 m from it, nearer than 50: ten metres straight away, no draw, then it watches from there (FINAL 30)
	uint32_t draws = 0;
	game_random::testing::SetGameRand([](uint32_t) { return 0u; },
	                                  [&draws](float) {
		                                  ++draws;
		                                  return 0.0f;
	                                  });
	EXPECT_EQ(vr::Fleeing(Action(v)), 1u);
	EXPECT_EQ(draws, 0u);
	EXPECT_EQ(Top(v), 1u);
	EXPECT_EQ(Final(v), 30u);
	EXPECT_EQ(Reg().Get<WallHug>(v).goal, glm::vec2(90.0f, 130.0f));
	EXPECT_EQ(vr::SlotReaction(v), id);
}

TEST_F(VillagerReactionsTest, FleeAcrossAMovingMiracle)
{
	SetUpMiracleRows();
	const auto v = MakeVillager({100.0f, 135.0f});
	// 60 m away, beyond the near distance, but coming at it along z = 130: it runs across the way
	const auto spell = MakeMiracle({160.0f, 0.0f, 130.0f}, {-1.0f, 0.0f, 0.0f});
	const auto id = reactions::CreateReaction(spell, Reaction::FleeFromSpell, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	Spread(v, id, 10.0f);
	ASSERT_EQ(Top(v), 6u);
	// two game draws of 8, x first (1) then z (3)
	std::vector<float> asked;
	game_random::testing::SetGameRand([](uint32_t) { return 0u; },
	                                  [&asked](float x) {
		                                  asked.push_back(x);
		                                  return asked.size() == 1 ? 1.0f : 3.0f;
	                                  });
	EXPECT_EQ(vr::Fleeing(Action(v)), 1u);
	EXPECT_EQ(asked, (std::vector<float> {8.0f, 8.0f}));
	EXPECT_EQ(Top(v), 1u);
	EXPECT_EQ(Final(v), 30u);
	EXPECT_EQ(Reg().Get<WallHug>(v).goal, glm::vec2(103.0f, 146.0f));
}

TEST_F(VillagerReactionsTest, FleeTurnsToWatchBeyondTheNearDistance)
{
	SetUpMiracleRows();
	const auto v = MakeVillager({100.0f, 130.0f});
	const auto twin = MakeVillager({100.0f, 130.0f});
	// 60 m away, between 50 and 100, and still: it turns to watch (30), facing it as one turning step does
	const auto spell = MakeMiracle({160.0f, 0.0f, 130.0f}, {});
	const auto id = reactions::CreateReaction(spell, Reaction::FleeFromSpell, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	Spread(v, id, 10.0f);
	ASSERT_EQ(Top(v), 6u);
	EXPECT_EQ(vr::Fleeing(Action(v)), 1u);
	EXPECT_EQ(Top(v), 30u);
	EXPECT_EQ(vr::SlotReaction(v), id);
	villager::LookAtPos(twin, tq::PosOf(v) + glm::ivec2(map_coords::ToFixed(60.0f), 0), 1);
	EXPECT_EQ(villager::GetGameAngle(v), villager::GetGameAngle(twin));
}

TEST_F(VillagerReactionsTest, FleeGivesUpBeyondTheFarDistance)
{
	SetUpMiracleRows();
	const auto v = MakeVillager({100.0f, 130.0f});
	const auto spell = MakeMiracle({205.0f, 0.0f, 130.0f}, {-1.0f, 0.0f, 0.0f});
	const auto id = reactions::CreateReaction(spell, Reaction::FleeFromSpell, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	Spread(v, id, 10.0f);
	ASSERT_EQ(Top(v), 6u);
	// 105 m away: back to what it was doing, the reaction dropped
	EXPECT_EQ(vr::Fleeing(Action(v)), 1u);
	EXPECT_EQ(Top(v), 163u);
	EXPECT_FALSE(vr::SlotHeld(v));
}

TEST_F(VillagerReactionsTest, WatchANiceMiracle)
{
	SetUpMiracleRows();
	for (const auto type : {Reaction::LookAtNiceSpell, Reaction::ReactToImpressiveSpell})
	{
		const auto v = MakeVillager({100.0f, 130.0f});
		const auto twin = MakeVillager({100.0f, 130.0f});
		const auto spell = MakeMiracle({100.0f, 0.0f, 150.0f}, {});
		const auto id = reactions::CreateReaction(spell, type, PlayerNames::NEUTRAL, false);
		ASSERT_NE(id, 0u);
		// within reach it stops to watch (7), keeping the miracle
		Spread(v, id, 20.0f);
		EXPECT_EQ(vr::SlotReaction(v), id) << static_cast<int>(type);
		EXPECT_EQ(vr::SlotObject(v), spell) << static_cast<int>(type);
		EXPECT_EQ(Top(v), 7u) << static_cast<int>(type);
		// each turn one turning step towards it, as LookAtPos with mode 1
		for (int turn = 0; turn < 3; ++turn)
		{
			EXPECT_EQ(vr::Watching(Action(v)), 1u);
			villager::LookAtPos(twin, tq::PosOf(v) + glm::ivec2(0, map_coords::ToFixed(20.0f)), 1);
			EXPECT_EQ(villager::GetGameAngle(v), villager::GetGameAngle(twin)) << static_cast<int>(type);
			EXPECT_EQ(Top(v), 7u);
		}
		// the miracle 99 m away: still watching; 101 m (beyond 100): it gives up and goes back to what it was doing
		Reg().Get<Spell>(spell).position = {100.0f, 0.0f, 229.0f};
		EXPECT_EQ(vr::Watching(Action(v)), 1u);
		EXPECT_EQ(Top(v), 7u);
		Reg().Get<Spell>(spell).position = {100.0f, 0.0f, 231.0f};
		EXPECT_EQ(vr::Watching(Action(v)), 1u);
		EXPECT_EQ(Top(v), 163u);
		EXPECT_FALSE(vr::SlotHeld(v));
	}
}

TEST_F(VillagerReactionsTest, WatchEndsWhenTheMiracleGoes)
{
	SetUpMiracleRows();
	const auto v = MakeVillager({100.0f, 130.0f}, 163);
	const auto spell = MakeMiracle({100.0f, 0.0f, 150.0f}, {});
	const auto id = reactions::CreateReaction(spell, Reaction::LookAtNiceSpell, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	Spread(v, id, 20.0f);
	ASSERT_EQ(Top(v), 7u);
	// the miracle ends: its reaction is shut down, the watcher goes back to what it was doing
	reactions::RemoveAllReactionsInitiatedByObject(spell);
	EXPECT_FALSE(vr::SlotHeld(v));
	EXPECT_EQ(Top(v), 163u);
	// a watcher whose miracle went without its reaction being shut down stops at its next turn
	const auto w = MakeVillager({100.0f, 130.0f}, 163);
	const auto again = MakeMiracle({100.0f, 0.0f, 150.0f}, {});
	const auto againId = reactions::CreateReaction(again, Reaction::LookAtNiceSpell, PlayerNames::NEUTRAL, false);
	ASSERT_NE(againId, 0u);
	Spread(w, againId, 20.0f);
	ASSERT_EQ(Top(w), 7u);
	Reg().Destroy(again);
	EXPECT_EQ(vr::Watching(Action(w)), 1u);
	EXPECT_EQ(Top(w), 163u);
	EXPECT_FALSE(vr::SlotHeld(w));
}

// ---- a thing thrown from the hand (9) ----------------------------------------------------------------------------

namespace
{
namespace living_rules = openblack::physics::living;

/// The info.dat row of 9 REACT_TO_FLYING_OBJECT, the villager reacting to it, and the states 6, 30 and 162
void SetUpFlyingObjectRows()
{
	auto& info = const_cast<InfoConstants&>(Locator::infoConstants::value());
	auto& r = info.reaction.at(static_cast<size_t>(Reaction::ReactToFlyingObject));
	r.priority = 142;
	r.numGameTurnsForNormalThingsToReact = 80;
	r.numGameTurnsForNormalThingsBeforeReactingAgain = 0;
	r.maxReactionDistance = 25.0f;
	r.howImportantIsDistance = 1.0f;
	r.whetherReactionFinishesIfInitiatorInHand = 1;
	r.minDistanceToRunAwayFromObject = 50.0f;
	r.maxDistanceToRunAwayFromObject = 100.0f;
	reinterpret_cast<uint32_t*>(&info.villager.at(0).isReacting)[static_cast<size_t>(Reaction::ReactToFlyingObject)] = 1;
	for (const uint32_t s : {6u, 30u, 162u})
	{
		auto& state = info.villagerStateTable.at(s);
		state.isFinalState = 1;
		state.availableForReaction = 1;
		state.isReactionState = 1;
	}
}

/// A rock in the physics where it is, flying at `velocity`
entt::entity MakeThrownRock(glm::vec2 at, glm::vec3 velocity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto rock = registry.Create();
	registry.Assign<Transform>(rock, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
	auto entry = std::make_unique<ecs::PhysicsEntry>();
	entry->entity = rock;
	entry->body.velocity = velocity;
	entry->body.resting = false;
	Locator::dynamicsSystem::value().GetState().objects.push_back(std::move(entry));
	return rock;
}

ecs::PhysicsEntry& EntryOf(entt::entity rock)
{
	auto* entry = Locator::dynamicsSystem::value().Find(rock);
	EXPECT_NE(entry, nullptr);
	return *entry;
}

void ClearPhysics()
{
	Locator::dynamicsSystem::value().GetState().objects.clear();
}

/// GameRand answers in turn from `answers` (0 once they run out); each n asked is written down
void ScriptGameRand(std::vector<uint32_t> answers, std::vector<uint32_t>& asked)
{
	asked.clear();
	game_random::testing::SetGameRand(
	    [answers = std::move(answers), &asked](uint32_t n) {
		    asked.push_back(n);
		    return asked.size() <= answers.size() ? answers.at(asked.size() - 1) : 0u;
	    },
	    [](float) { return 0.0f; });
}
} // namespace

TEST(VillagerFlyingObjectRules, ItRunsOnlyWhenStrictlyNearerThanTwoSecondsOfFlight)
{
	using living_rules::FlyingObjectResponse;
	EXPECT_EQ(living_rules::k_RunSeconds, 2.0f);
	// exactly two seconds of flight away: it points; the next float nearer: it runs; further: it points
	EXPECT_EQ(living_rules::RespondToFlyingObject(10.0f, 5.0f), FlyingObjectResponse::Point);
	EXPECT_EQ(living_rules::RespondToFlyingObject(std::nextafter(10.0f, 0.0f), 5.0f), FlyingObjectResponse::Run);
	EXPECT_EQ(living_rules::RespondToFlyingObject(std::nextafter(10.0f, 20.0f), 5.0f), FlyingObjectResponse::Point);
	// a still thing, or a value that compares with nothing: it points
	EXPECT_EQ(living_rules::RespondToFlyingObject(0.0f, 0.0f), FlyingObjectResponse::Point);
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(living_rules::RespondToFlyingObject(nan, 5.0f), FlyingObjectResponse::Point);
	EXPECT_EQ(living_rules::RespondToFlyingObject(1.0f, nan), FlyingObjectResponse::Point);
	// the body's speed
	EXPECT_EQ(living_rules::FlyingSpeed({1.0f, 2.0f, 2.0f}), 3.0f);
	EXPECT_EQ(living_rules::FlyingSpeed({0.0f, -4.0f, 3.0f}), 5.0f);
	EXPECT_EQ(living_rules::FlyingSpeed({0.0f, 0.0f, 0.0f}), 0.0f);
}

TEST(VillagerFlyingObjectRules, AThingIsInTheAirWhileFasterThanThreeOrHigherThanAFifth)
{
	EXPECT_EQ(living_rules::k_AirborneSpeed, 3.0f);
	EXPECT_EQ(living_rules::k_AirborneHeight, 0.2f);
	// on the land: in the air only faster than 3 m/s (strict)
	EXPECT_FALSE(living_rules::IsActuallyInTheAir(3.0f, 10.0f, 10.0f, 0.0f));
	EXPECT_TRUE(living_rules::IsActuallyInTheAir(std::nextafter(3.0f, 4.0f), 10.0f, 10.0f, 0.0f));
	// slow: in the air only when its centre less the land, plus its radius, is above 0.2 (strict)
	EXPECT_FALSE(living_rules::IsActuallyInTheAir(0.0f, 0.5f, 0.5f, 0.2f));
	EXPECT_TRUE(living_rules::IsActuallyInTheAir(0.0f, 0.5f, 0.5f, 0.25f));
	EXPECT_TRUE(living_rules::IsActuallyInTheAir(0.0f, 1.0f, 0.75f, 0.0f));
	EXPECT_FALSE(living_rules::IsActuallyInTheAir(0.0f, 1.0f, 1.25f, 0.25f));
	// a speed that compares with nothing counts as slow
	EXPECT_FALSE(living_rules::IsActuallyInTheAir(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f));
}

TEST(VillagerFlyingObjectRules, AShelteredVillagerSeesOnlyWhatIsStrictlyInsideTheShield)
{
	EXPECT_TRUE(living_rules::ShelterSeesFlyer(4.0f, 5.0f));
	EXPECT_FALSE(living_rules::ShelterSeesFlyer(5.0f, 5.0f));
	EXPECT_FALSE(living_rules::ShelterSeesFlyer(6.0f, 5.0f));
	EXPECT_FALSE(living_rules::ShelterSeesFlyer(std::numeric_limits<float>::quiet_NaN(), 5.0f));
}

TEST_F(VillagerReactionsTest, AFlyingThingIsNotNoticedWhileTheVillagerFliesOrLands)
{
	SetUpFlyingObjectRows();
	const auto rock = MakeThrownRock({110.0f, 130.0f}, {5.0f, 0.0f, 0.0f});
	const auto id = reactions::CreateReaction(rock, Reaction::ReactToFlyingObject, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(id, 0u);
	EXPECT_EQ(vr::ReactToFlyingObjectPriority(MakeVillager({100.0f, 130.0f}, 163), id, 0), 142u);
	EXPECT_EQ(vr::ReactToFlyingObjectPriority(MakeVillager({100.0f, 130.0f}, 10), id, 0), 0u);
	EXPECT_EQ(vr::ReactToFlyingObjectPriority(MakeVillager({100.0f, 130.0f}, 11), id, 0), 0u);
	ClearPhysics();
}

TEST_F(VillagerReactionsTest, ADancingVillagerNoticesAFlyingThingOnlyWhenItComesAtIt)
{
	SetUpFlyingObjectRows();
	const auto away = MakeThrownRock({110.0f, 130.0f}, {5.0f, 0.0f, 0.0f});
	const auto coming = MakeThrownRock({110.0f, 130.0f}, {-5.0f, 0.0f, 1.0f});
	const auto awayId = reactions::CreateReaction(away, Reaction::ReactToFlyingObject, PlayerNames::PLAYER_ONE, false);
	const auto comingId = reactions::CreateReaction(coming, Reaction::ReactToFlyingObject, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(awayId, 0u);
	ASSERT_NE(comingId, 0u);
	const auto dancer = MakeVillager({100.0f, 130.0f}, 163);
	Reg().Assign<Dancer>(dancer);
	EXPECT_EQ(vr::ReactToFlyingObjectPriority(dancer, awayId, 0), 0u);
	EXPECT_EQ(vr::ReactToFlyingObjectPriority(dancer, comingId, 0), 142u);
	// one that isn't dancing notices both
	const auto watcher = MakeVillager({100.0f, 130.0f}, 163);
	EXPECT_EQ(vr::ReactToFlyingObjectPriority(watcher, awayId, 0), 142u);
	EXPECT_EQ(vr::ReactToFlyingObjectPriority(watcher, comingId, 0), 142u);
	// out of the physics, a dancer takes no notice
	ClearPhysics();
	EXPECT_EQ(vr::ReactToFlyingObjectPriority(dancer, comingId, 0), 0u);
}

TEST_F(VillagerReactionsTest, AFlyingThingSplitsRunnersFromPointersAtTwiceItsSpeed)
{
	SetUpFlyingObjectRows();
	const auto v = MakeVillager({100.0f, 130.0f});
	const auto w = MakeVillager({100.0f, 130.0f});
	V(v).sex = Villager::Sex::MALE;
	V(v).lifeStage = Villager::LifeStage::Adult;
	V(w).sex = Villager::Sex::MALE;
	V(w).lifeStage = Villager::LifeStage::Adult;
	// the distance across the map, as the setup measures it, and a speed of exactly half of it
	const auto rock = MakeThrownRock({110.0f, 130.0f}, {0.0f, 0.0f, 0.0f});
	const float d = tq::GetDistanceInMetres(tq::PosOf(v), tq::PosOf(rock));
	EntryOf(rock).body.velocity = {d / 2.0f, 0.0f, 0.0f};
	const auto id = reactions::CreateReaction(rock, Reaction::ReactToFlyingObject, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(id, 0u);
	// at exactly two seconds of its flight it points (162), keeping the thing
	Spread(v, id, 5.0f);
	EXPECT_EQ(vr::SlotReaction(v), id);
	EXPECT_EQ(vr::SlotObject(v), rock);
	EXPECT_EQ(Top(v), 162u);
	EXPECT_EQ(Previous(v), 163u);
	// a little faster and the same distance is nearer than that: it runs (6)
	EntryOf(rock).body.velocity = {std::nextafter(d / 2.0f, d), 0.0f, 0.0f};
	Spread(w, id, 5.0f);
	EXPECT_EQ(vr::SlotReaction(w), id);
	EXPECT_EQ(Top(w), 6u);
	// a thing with no physics body is taken up by nobody
	ClearPhysics();
	const auto x = MakeVillager({100.0f, 130.0f});
	Spread(x, id, 5.0f);
	EXPECT_FALSE(vr::SlotHeld(x));
	EXPECT_EQ(Top(x), 163u);
}

TEST_F(VillagerReactionsTest, PointingDrawsTheClipAsTheStateIsEntered)
{
	SetUpFlyingObjectRows();
	struct Case
	{
		Villager::Sex sex;
		Villager::LifeStage stage;
		std::vector<uint32_t> answers;
		std::vector<uint32_t> asked;
		int32_t clip;
	};
	// a man draws once, 0 / 1 / 2 -> 310 LOOKING_FOR_SOMETHING / 385 STAND / 395 TALKING_AND_POINTING; a woman or a
	// child first draws whether it is scared stiff (0 -> 355), and only if not the same second draw
	const std::array<Case, 6> cases {
	    Case {Villager::Sex::MALE, Villager::LifeStage::Adult, {0}, {3}, 310},
	    Case {Villager::Sex::MALE, Villager::LifeStage::Adult, {2}, {3}, 395},
	    Case {Villager::Sex::FEMALE, Villager::LifeStage::Adult, {0}, {3}, 355},
	    Case {Villager::Sex::FEMALE, Villager::LifeStage::Adult, {1, 0}, {3, 3}, 310},
	    Case {Villager::Sex::MALE, Villager::LifeStage::Child, {0}, {3}, 355},
	    Case {Villager::Sex::MALE, Villager::LifeStage::Child, {2, 1}, {3, 3}, 385},
	};
	const auto rock = MakeThrownRock({100.0f, 150.0f}, {1.0f, 0.0f, 0.0f});
	const auto id = reactions::CreateReaction(rock, Reaction::ReactToFlyingObject, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(id, 0u);
	std::vector<uint32_t> asked;
	for (size_t i = 0; i < cases.size(); ++i)
	{
		const auto& c = cases.at(i);
		const auto v = MakeVillager({100.0f, 130.0f});
		V(v).sex = c.sex;
		V(v).lifeStage = c.stage;
		ScriptGameRand(c.answers, asked);
		Spread(v, id, 10.0f);
		EXPECT_EQ(Top(v), 162u) << i;
		EXPECT_EQ(asked, c.asked) << i;
		ASSERT_TRUE(Reg().AllOf<SkeletalAnimation>(v)) << i;
		EXPECT_EQ(Reg().Get<SkeletalAnimation>(v).clipIndex, c.clip) << i;
		// the clip function is the state's: asked again it draws again, the same way
		ScriptGameRand(c.answers, asked);
		EXPECT_EQ(ecs::villager_animate::StateClip(v), c.clip) << i;
		EXPECT_EQ(asked, c.asked) << i;
	}
	ClearPhysics();
}

TEST_F(VillagerReactionsTest, WatchingAFlyingThingLooksUpWhileItIsHigherThanThreeMetres)
{
	SetUpFlyingObjectRows();
	const auto rock = MakeThrownRock({100.0f, 150.0f}, {1.0f, 0.0f, 0.0f});
	const auto id = reactions::CreateReaction(rock, Reaction::ReactToFlyingObject, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(id, 0u);
	const auto v = MakeVillager({100.0f, 130.0f});
	Spread(v, id, 10.0f);
	ASSERT_EQ(vr::SlotObject(v), rock);
	// 161 WATCH_FLYING_OBJECT_REACTION, come from 162; no land here, so the height is the rock's own
	Action(v).states.at(0) = 161;
	Action(v).states.at(2) = 162;
	auto& position = Reg().Get<Transform>(rock).position;
	std::vector<uint32_t> asked;
	ScriptGameRand({}, asked);
	position.y = 3.5f;
	EXPECT_EQ(ecs::villager_animate::StateClip(v), 311);
	position.y = std::nextafter(3.0f, 4.0f);
	EXPECT_EQ(ecs::villager_animate::StateClip(v), 311);
	// exactly 3 m is not more than 3 m
	position.y = 3.0f;
	EXPECT_EQ(ecs::villager_animate::StateClip(v), 385);
	position.y = 0.5f;
	EXPECT_EQ(ecs::villager_animate::StateClip(v), 385);
	// a previous state whose clip hides the villager keeps it hidden
	auto& info = const_cast<InfoConstants&>(Locator::infoConstants::value());
	info.villagerStateTable.at(162).animation = static_cast<uint32_t>(-4);
	position.y = 3.5f;
	EXPECT_EQ(ecs::villager_animate::StateClip(v), -4);
	info.villagerStateTable.at(162).animation = 385;
	// with nothing to watch it stands
	vr::SetSlotObject(v, entt::null);
	EXPECT_EQ(ecs::villager_animate::StateClip(v), 385);
	// the clip draws nothing
	EXPECT_TRUE(asked.empty());
	ClearPhysics();
}

TEST_F(VillagerReactionsTest, PointingEndsOnceTheThingIsNoLongerInTheAir)
{
	SetUpFlyingObjectRows();
	const auto v = MakeVillager({100.0f, 130.0f});
	V(v).sex = Villager::Sex::MALE;
	V(v).lifeStage = Villager::LifeStage::Adult;
	// 20 m away, falling at 4 m/s: it points
	const auto rock = MakeThrownRock({100.0f, 150.0f}, {0.0f, -4.0f, 0.0f});
	const auto id = reactions::CreateReaction(rock, Reaction::ReactToFlyingObject, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(id, 0u);
	Spread(v, id, 10.0f);
	ASSERT_EQ(Top(v), 162u);
	// faster than 3 m/s it is still in the air: it keeps pointing
	EXPECT_EQ(vr::PointAtFlyingObject(Action(v)), 1u);
	EXPECT_EQ(Top(v), 162u);
	EXPECT_EQ(vr::SlotReaction(v), id);
	// at 3 m/s, lying on the land with no radius: no longer in the air, it goes back to what it was doing
	EntryOf(rock).body.velocity = {0.0f, -3.0f, 0.0f};
	EXPECT_EQ(vr::PointAtFlyingObject(Action(v)), 1u);
	EXPECT_EQ(Top(v), 163u);
	EXPECT_FALSE(vr::SlotHeld(v));
	ClearPhysics();
}

TEST_F(VillagerReactionsTest, APointerRunsOnceTheThingIsNearerThanTwoSecondsOfItsFlight)
{
	SetUpFlyingObjectRows();
	const auto v = MakeVillager({100.0f, 130.0f});
	V(v).sex = Villager::Sex::MALE;
	V(v).lifeStage = Villager::LifeStage::Adult;
	const auto rock = MakeThrownRock({100.0f, 150.0f}, {4.0f, 0.0f, 0.0f});
	const auto id = reactions::CreateReaction(rock, Reaction::ReactToFlyingObject, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(id, 0u);
	Spread(v, id, 10.0f);
	ASSERT_EQ(Top(v), 162u);
	// the thing out of the physics: nothing changes
	ClearPhysics();
	EXPECT_EQ(vr::PointAtFlyingObject(Action(v)), 1u);
	EXPECT_EQ(Top(v), 162u);
	// back in the physics, 20 m away at 12 m/s: it runs (6), still following the thing
	auto entry = std::make_unique<ecs::PhysicsEntry>();
	entry->entity = rock;
	entry->body.velocity = {12.0f, 0.0f, 0.0f};
	Locator::dynamicsSystem::value().GetState().objects.push_back(std::move(entry));
	EXPECT_EQ(vr::PointAtFlyingObject(Action(v)), 1u);
	EXPECT_EQ(Top(v), 6u);
	EXPECT_EQ(vr::SlotReaction(v), id);
	// running from a thing that flies: across its way, two game draws of 8 for the flee point
	std::vector<float> floats;
	game_random::testing::SetGameRand([](uint32_t) { return 0u; },
	                                  [&floats](float x) {
		                                  floats.push_back(x);
		                                  return 4.0f;
	                                  });
	EXPECT_EQ(vr::Fleeing(Action(v)), 1u);
	EXPECT_EQ(floats, (std::vector<float> {8.0f, 8.0f}));
	EXPECT_EQ(Top(v), 1u);
	EXPECT_EQ(Final(v), 30u);
	ClearPhysics();
}

// ---- love out of doors (VillagerBreeding.cpp) --------------------------------------------------------------------

TEST_F(VillagerReactionsTest, ExitSexFreesAPartnerStillInTheSameState)
{
	auto& table = const_cast<InfoConstants&>(Locator::infoConstants::value()).villagerStateTable;
	table.at(123).isFinalState = 1;
	table.at(124).isFinalState = 1;
	const auto a = MakeVillager({100.0f, 130.0f}, 123);
	const auto b = MakeVillager({101.0f, 130.0f}, 123);
	V(a).targetThing = b;
	V(b).targetThing = a;
	// on to another of the love states (the same exit): nothing changes
	EXPECT_EQ(villager::ExitSex(Action(a), VillagerStates::StopHavingSex), 1u);
	EXPECT_EQ(V(a).targetThing, b);
	EXPECT_EQ(V(b).targetThing, a);
	// out of them: both let go, and the partner, still kissing, stops
	EXPECT_EQ(villager::ExitSex(Action(a), VillagerStates::DecideWhatToDo), 1u);
	EXPECT_EQ(V(a).targetThing, entt::entity(entt::null));
	EXPECT_EQ(V(b).targetThing, entt::entity(entt::null));
	EXPECT_EQ(Top(b), 124u);
	// a partner in another state is only let go of
	V(a).targetThing = b;
	V(b).targetThing = a;
	EXPECT_EQ(villager::ExitSex(Action(a), VillagerStates::DecideWhatToDo), 1u);
	EXPECT_EQ(V(a).targetThing, entt::entity(entt::null));
	EXPECT_EQ(V(b).targetThing, a);
	EXPECT_EQ(Top(b), 124u);
}

TEST_F(VillagerReactionsTest, SexValidateDropsAPartnerWhoIsGone)
{
	const auto a = MakeVillager({100.0f, 130.0f}, 122);
	const auto b = MakeVillager({110.0f, 130.0f}, 122);
	V(a).targetThing = b;
	EXPECT_TRUE(villager::SexValidate(Action(a)));
	EXPECT_EQ(V(a).targetThing, b);
	Reg().Destroy(b);
	EXPECT_TRUE(villager::SexValidate(Action(a)));
	EXPECT_EQ(V(a).targetThing, entt::entity(entt::null));
}

TEST_F(VillagerReactionsTest, TheHandFreesThePartnerOnlyWhenTheStoredFinalIsHavingSex)
{
	const auto a = MakeVillager({100.0f, 130.0f}, 123);
	const auto b = MakeVillager({101.0f, 130.0f}, 123);
	V(a).targetThing = b;
	V(b).targetThing = a;
	// HAVING_SEX on top, but not stored as the final state: nothing
	villager::LetPartnerGo(a);
	EXPECT_EQ(V(a).targetThing, b);
	EXPECT_EQ(V(b).targetThing, a);
	// stored as the final state: both let go, the partner decides afresh
	Action(a).states.at(1) = 123;
	villager::LetPartnerGo(a);
	EXPECT_EQ(V(a).targetThing, entt::entity(entt::null));
	EXPECT_EQ(V(b).targetThing, entt::entity(entt::null));
}

// ---- the breeder disciple (VillagerBreeding.cpp) -----------------------------------------------------------------

TEST_F(VillagerReactionsTest, ABreedersCallIsAnsweredByOneOfTheOtherSexAtATime)
{
	auto& info = const_cast<InfoConstants&>(Locator::infoConstants::value());
	info.reaction.at(static_cast<size_t>(Reaction::ReactToBreeder)).priority = 40;
	// row 0 a woman (a housewife), row 1 a man (a forester), both of an age from 13 to 100
	info.villager.at(1) = info.villager.at(0);
	info.villager.at(0).sex = SexType::Female;
	info.villager.at(0).villagerNumber = VillagerNumber::Housewife;
	info.villager.at(1).sex = SexType::Male;
	info.villager.at(1).villagerNumber = VillagerNumber::Forester;
	for (size_t row = 0; row < 2; ++row)
	{
		info.villager.at(row).startHavingSexAge = 13;
		info.villager.at(row).stopHavingSexAge = 100;
	}
	const auto breeder = MakeVillager({100.0f, 130.0f}, 225);
	const auto man = MakeVillager({105.0f, 130.0f});
	const auto woman = MakeVillager({106.0f, 130.0f});
	for (const auto [e, row] : {std::pair {breeder, 0u}, std::pair {man, 1u}, std::pair {woman, 0u}})
	{
		V(e).tribe = info.villager.at(row).tribeType;
		V(e).number = info.villager.at(row).villagerNumber;
		villager::SetAgeBirthTurn(e, 20, 1000);
	}
	const auto id = reactions::CreateReaction(breeder, Reaction::ReactToBreeder, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	EXPECT_EQ(villager::ReactToBreederPriority(man, id, 0), 40);
	// the same sex, or the breeder itself: no
	EXPECT_EQ(villager::ReactToBreederPriority(woman, id, 0), 0);
	EXPECT_EQ(villager::ReactToBreederPriority(breeder, id, 0), 0);
	// under age: no
	villager::SetAgeBirthTurn(man, 5, 1000);
	EXPECT_EQ(villager::ReactToBreederPriority(man, id, 0), 0);
	villager::SetAgeBirthTurn(man, 20, 1000);
	// under a script: no
	ecs::script_held::SetControlledByScript(man, true);
	EXPECT_EQ(villager::ReactToBreederPriority(man, id, 0), 0);
	ecs::script_held::SetControlledByScript(man, false);
	EXPECT_EQ(villager::ReactToBreederPriority(man, id, 0), 40);
	// once one villager follows the call, nobody else answers it
	Reg().Assign<LivingReaction>(woman, LivingReaction {id, Reaction::ReactToBreeder, breeder, 1});
	EXPECT_EQ(villager::FollowersOf(id), 1u);
	EXPECT_EQ(villager::ReactToBreederPriority(man, id, 0), 0);
	// the setup: it follows the call in 227, the breeder its object
	Reg().Remove<LivingReaction>(woman);
	villager::SetupReactToBreeder(man, breeder, id);
	EXPECT_EQ(Top(man), 227u);
	EXPECT_EQ(vr::SlotReaction(man), id);
	EXPECT_EQ(vr::SlotObject(man), breeder);
}

TEST_F(VillagerReactionsTest, ABreedersJobWithNoHomeNearIsBesideItsTownsFood)
{
	auto& info = const_cast<InfoConstants&>(Locator::infoConstants::value());
	info.town.amountOverCapacityThatDiscipleBreedersBreedUntil = 2.0f;
	const auto v = MakeVillager({100.0f, 130.0f}, 163);
	// no town: no job
	EXPECT_EQ(villager::SetupBreederDisciple(v), 0u);
	const auto town = MakeTown();
	V(v).town = town;
	V(v).targetThing = town;
	// its town too far over full: no job
	Reg().Get<Town>(town).stats.children = 2;
	EXPECT_EQ(villager::SetupBreederDisciple(v), 0u);
	Reg().Get<Town>(town).stats.children = 0;
	// no home near: a spot three metres (the draws at nought) from the food drop-off, walked to into 225
	EXPECT_EQ(villager::SetupBreederDisciple(v), 1u);
	EXPECT_EQ(Final(v), 225u);
	EXPECT_EQ(V(v).targetThing, entt::entity(entt::null));
	const auto dropoff = villager::GetResourceDropoffPos(v, ResourceType::Food);
	const auto& area = V(v).wanderArea;
	EXPECT_NEAR(tq::GetDistanceInMetres(dropoff, glm::ivec2(area.x, area.z)), 3.0f, 0.05f);
}

TEST_F(VillagerReactionsTest, AVillagerAScriptPutInACrowdDrawsACrowdClipAgainEachTimeItsClipIsReady)
{
	const auto v = MakeVillager({100.0f, 130.0f}, 244);
	auto& clip = Reg().Assign<SkeletalAnimation>(v);
	Action(v).turnsSinceStateChange = 7;
	std::vector<uint32_t> asked;
	// not ready: nothing drawn, no clip, the turns kept
	ScriptGameRand({0}, asked);
	EXPECT_EQ(villager::ScriptInCrowd(Action(v)), 1u);
	EXPECT_EQ(_states->readyAsked, std::vector<uint32_t> {1});
	EXPECT_TRUE(asked.empty());
	EXPECT_FALSE(clip.hasClip);
	EXPECT_EQ(Action(v).turnsSinceStateChange, 7);
	// ready: a crowd clip drawn out of 25 (0 is PAmbient2), from the start, and the turns from 0
	_states->readyForNewClip = true;
	EXPECT_EQ(villager::ScriptInCrowd(Action(v)), 1u);
	EXPECT_EQ(asked, std::vector<uint32_t> {25});
	EXPECT_TRUE(clip.hasClip);
	EXPECT_EQ(clip.clipIndex, static_cast<int32_t>(AnimId::PAmbient2));
	EXPECT_EQ(Action(v).turnsSinceStateChange, 0);
	// ready again and the same clip drawn: it plays on where it is, the turns from 0 again
	clip.time = 1.5f;
	Action(v).turnsSinceStateChange = 5;
	ScriptGameRand({0}, asked);
	EXPECT_EQ(villager::ScriptInCrowd(Action(v)), 1u);
	EXPECT_EQ(clip.clipIndex, static_cast<int32_t>(AnimId::PAmbient2));
	EXPECT_EQ(clip.time, 1.5f);
	EXPECT_EQ(Action(v).turnsSinceStateChange, 0);
	// a different clip drawn (6 and up is PStand) starts from the start
	ScriptGameRand({6}, asked);
	EXPECT_EQ(villager::ScriptInCrowd(Action(v)), 1u);
	EXPECT_EQ(clip.clipIndex, static_cast<int32_t>(AnimId::PStand));
	EXPECT_EQ(clip.time, 0.0f);
	EXPECT_EQ(_states->readyAsked, (std::vector<uint32_t> {1, 1, 1, 1}));
}

TEST_F(VillagerReactionsTest, AShieldsFallPanicsAVillagerAboutWhatItWasWatchingWhateverThatWas)
{
	SetUpFlyingObjectRows();
	auto& info = const_cast<InfoConstants&>(Locator::infoConstants::value());
	auto& row = info.reaction.at(static_cast<size_t>(Reaction::ReactToMagicShieldDestroyed));
	row.minDistanceToRunAwayFromObject = 20.0f;
	row.maxDistanceToRunAwayFromObject = 40.0f;
	// it watches a flying rock: the slot holds the rock's reaction and the rock
	const auto rock = MakeThrownRock({100.0f, 150.0f}, {1.0f, 0.0f, 0.0f});
	const auto watching = reactions::CreateReaction(rock, Reaction::ReactToFlyingObject, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(watching, 0u);
	const auto v = MakeVillager({100.0f, 130.0f});
	std::vector<uint32_t> asked;
	ScriptGameRand({0}, asked);
	Spread(v, watching, 10.0f);
	ASSERT_EQ(vr::SlotReaction(v), watching);
	ASSERT_EQ(vr::FollowedReactionOf(v).object, rock);
	ASSERT_EQ(vr::FollowedReactionOf(v).kind, Reaction::ReactToFlyingObject);
	// a shield falls nearby; with no town to hide in, it runs in panic and goes on watching the rock, as the original
	// keeps the old reaction's object (not the shield's, which it was never amazed by)
	const auto shield = MakeMiracle({100.0f, 0.0f, 110.0f}, {});
	const auto fall = reactions::CreateReaction(shield, Reaction::ReactToMagicShieldDestroyed, PlayerNames::PLAYER_ONE, false);
	ASSERT_NE(fall, 0u);
	ecs::villager_cover::SetupReactToMagicShieldDestroyed(v, shield, fall);
	EXPECT_EQ(vr::SlotReaction(v), fall);
	EXPECT_EQ(vr::SlotObject(v), rock);
	EXPECT_EQ(Final(v), static_cast<uint32_t>(VillagerStates::PanicReaction));
	ClearPhysics();
}

// The burning thing in the hand: a walker's speed a turn becomes metres a second at ten turns a second, as the
// original's walkers convert it (the values are the original's)
TEST_F(VillagerReactionsTest, AWalkersSpeedAMetreATurnIsTenMetresASecond)
{
	// a speed a turn in metres, as the original's walkers hold it, and the metres a second the original gives for it
	struct Case
	{
		uint32_t metresPerTurn;
		uint32_t metresPerSecond;
	};
	constexpr std::array<Case, 6> k_Cases {{
	    {0x39200000, 0x3AC80000}, // the least speed: 0.00015 m a turn, 0.0015 m a second
	    {0x3E4CD800, 0x40000700}, // 2.0004 m a second
	    {0x3E99AC00, 0x40401700}, // 3.0014 m a second: just too fast to count as still
	    {0x3F200000, 0x40C80000}, // 6.25 m a second
	    {0x3F800200, 0x41200280}, // 10.0006 m a second
	    {0x411FFF60, 0x42C7FF38}, // the greatest speed: 99.998 m a second
	}};
	for (const auto& c : k_Cases)
	{
		const float speed = vr::WalkerSpeedInMetresPerSecond(std::bit_cast<float>(c.metresPerTurn));
		EXPECT_EQ(std::bit_cast<uint32_t>(speed), c.metresPerSecond);
	}
	// a burning walker going 0.3 m a turn is too fast to be still, so it is reacted to again at once
	EXPECT_EQ(vr::BurningObjectTurnsBeforeReactingAgain(10.0f, 5.0f,
	                                                    vr::WalkerSpeedInMetresPerSecond(std::bit_cast<float>(0x3E99AC00u)), 7),
	          0u);
	// and 0.29999 m a turn still counts as still
	EXPECT_EQ(vr::BurningObjectTurnsBeforeReactingAgain(10.0f, 5.0f,
	                                                    vr::WalkerSpeedInMetresPerSecond(std::bit_cast<float>(0x3E999800u)), 7),
	          7u);
}

// ---- a villager on its way to worship meets a fire ---------------------------------------------------------------

TEST_F(VillagerReactionsTest, OnItsWayToWorshipAVillagerWalksRoundAFireAndGoesOn)
{
	// walking (1, not final) to the worship site's arrive point, its final state 59 ARRIVES_AT_WORSHIP_SITE_FOR_WORSHIP
	const auto v = MakeVillager({100.0f, 130.0f}, 1);
	Action(v).states.at(1) = 59;
	Reg().Get<WallHug>(v).goal = glm::vec2(160.0f, 130.0f);
	Reg().Assign<WorshipVillager>(v).onWay = true;
	const auto thing = MakeLog({120.0f, 130.0f});
	const auto id = reactions::CreateReaction(thing, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	const auto& fire = Burn(thing);
	ecs::villager_fire::SetupReactToFire(v, thing, id);
	// the reaction is taken as by anyone, then at once the walk round the fire (220) towards the arrive point, and 59
	// after it
	EXPECT_TRUE(ecs::villager_fire::IsReacting(v));
	EXPECT_EQ(Top(v), 220u);
	EXPECT_EQ(Previous(v), 59u);
	const auto& state = Reg().Get<const VillagerFireState>(v);
	EXPECT_EQ(state.fire, fire.id);
	EXPECT_EQ(state.object, thing);
	EXPECT_EQ(state.savedDestination, glm::vec2(160.0f, 130.0f));
}

TEST_F(VillagerReactionsTest, OnItsWayToWorshipWithAPassingFinalStateItGoesOnToTheStoredOne)
{
	// the final state's row is never stored as previous (a passing one): the state it had stored, 59, is the one after,
	// so once round the fire it is counted on its town's way to worship again (as only a walk round going on to 59 is)
	const_cast<GVillagerStateTableInfo&>(Locator::infoConstants::value().villagerStateTable.at(58)).keepsPreviousState = 1;
	const auto town = MakeTown();
	const auto v = MakeVillager({100.0f, 130.0f}, 1);
	V(v).town = town;
	Action(v).states.at(1) = 58;
	Action(v).states.at(2) = 59;
	auto& worshipper = Reg().Assign<WorshipVillager>(v);
	worshipper.onWay = true;
	const auto thing = MakeLog({120.0f, 130.0f});
	const auto id = reactions::CreateReaction(thing, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	Burn(thing);
	ecs::villager_fire::SetupReactToFire(v, thing, id);
	EXPECT_EQ(Top(v), 220u);
	EXPECT_EQ(Previous(v), 59u);
	EXPECT_TRUE(Reg().Get<const WorshipVillager>(v).onWayInTown);
}

TEST_F(VillagerReactionsTest, OnItsWayToWorshipAThingNotBurningOnlyTakesTheReaction)
{
	const auto v = MakeVillager({100.0f, 130.0f}, 1);
	Action(v).states.at(1) = 59;
	Reg().Assign<WorshipVillager>(v).onWay = true;
	const auto thing = MakeLog({120.0f, 130.0f});
	const auto id = reactions::CreateReaction(thing, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	ecs::villager_fire::SetupReactToFire(v, thing, id);
	EXPECT_EQ(Top(v), 215u);
	EXPECT_EQ(Reg().Get<const VillagerFireState>(v).fire, 0u);
}

TEST_F(VillagerReactionsTest, NotOnItsWayToWorshipAVillagerStopsAtAFire)
{
	// the same walk, not to worship: only the reaction (215), deciding later
	const auto v = MakeVillager({100.0f, 130.0f}, 1);
	Action(v).states.at(1) = 59;
	const auto thing = MakeLog({120.0f, 130.0f});
	const auto id = reactions::CreateReaction(thing, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	Burn(thing);
	ecs::villager_fire::SetupReactToFire(v, thing, id);
	EXPECT_EQ(Top(v), 215u);
	EXPECT_EQ(Previous(v), 59u);
}
