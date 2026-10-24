/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The villager core (docs/wiki/beings/villager-states.md#summary): the constructor's draws and water rule, CREATED,
// SetTopState's return codes and pause, SetState and the town's modifiers, the periodic checks and the life's wear, the
// pick-up's and the flight's change of state through the old state's exit, with a fake state table in the Locator and scripted
// draws.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdlib>

#include <array>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/ScriptAnimation.h"
#include "ECS/Components/ScriptStateArguments.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/LivingConstruct.h"
#include "ECS/MapCells.h"
#include "ECS/PhysicsGameHooks.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/Systems/Implementations/VillagerAnimate.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerPhysics.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerScript.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/Implementations/VillagerWorshipCheck.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerTrace.h"
#include "ECS/VillagerAnimations.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/DynamicsFakes.h"
#include "support/LivingActionFakes.h"
#include "support/TestServices.h"
#include "support/VillagerFakes.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager = openblack::ecs::villager;
namespace villager_script = openblack::ecs::villager_script;
using Index = LivingAction::Index;

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

/// A state table where each row records its calls and returns what the test says (default: no function = 1)
class FakeStateTable final: public test::LivingActionFakeBase
{
public:
	struct Row
	{
		std::optional<uint32_t> entry;
		std::optional<uint32_t> exit;
		std::function<uint32_t(LivingAction&)> state;
		/// The row's exit function itself, in place of `exit`
		std::function<uint32_t(LivingAction&, VillagerStates)> exitFunction;
	};
	mutable std::map<uint32_t, Row> rows;
	mutable std::vector<std::string> calls;
	/// A top state set from outside the state logic: as the game's table, through SetTopState unless it skips the
	/// transition, else set straight. Off: nothing is set
	bool setsTopStates {false};

	void Update() override {}
	[[nodiscard]] VillagerStates VillagerGetState(const LivingAction& action, Index index) const override
	{
		return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	}
	[[nodiscard]] VillagerStates VillagerGetFinalState(const LivingAction& /*action*/) const override
	{
		return VillagerStates::InvalidState;
	}
	void VillagerSetState(LivingAction& action, Index index, VillagerStates state, bool skipTransition) const override
	{
		if (!setsTopStates || index != Index::Top)
		{
			return;
		}
		const auto entity = Locator::entitiesRegistry::value().ToEntity(action);
		calls.push_back(std::string(skipTransition ? "set " : "set with exit ") + std::to_string(static_cast<uint32_t>(state)));
		if (skipTransition)
		{
			villager::SetState(entity, Index::Top, state);
			return;
		}
		villager::SetTopState(entity, state);
	}
	uint32_t VillagerCallState(LivingAction& action, Index index) const override
	{
		const auto s = action.states.at(static_cast<size_t>(index));
		calls.push_back("state " + std::to_string(s));
		const auto& row = rows[s];
		return row.state ? row.state(action) : 0;
	}
	uint32_t VillagerCallEntryState(LivingAction&, VillagerStates row, VillagerStates final, VillagerStates next) const override
	{
		calls.push_back("entry " + std::to_string(static_cast<uint32_t>(row)) + " " +
		                std::to_string(static_cast<uint32_t>(final)) + " " + std::to_string(static_cast<uint32_t>(next)));
		return rows[static_cast<uint32_t>(row)].entry.value_or(1);
	}
	uint32_t VillagerCallExitState(LivingAction& action, VillagerStates row, VillagerStates next) const override
	{
		calls.push_back("exit " + std::to_string(static_cast<uint32_t>(row)) + " " +
		                std::to_string(static_cast<uint32_t>(next)));
		const auto& fake = rows[static_cast<uint32_t>(row)];
		return fake.exitFunction ? fake.exitFunction(action, next) : fake.exit.value_or(1);
	}
	int VillagerCallOutOfAnimation(LivingAction&, Index) const override { return -1; }
	bool VillagerCallValidate(LivingAction&, Index) const override { return false; }

	[[nodiscard]] size_t Count(const std::string& call) const
	{
		return static_cast<size_t>(std::count(calls.begin(), calls.end(), call));
	}
};

constexpr auto S(uint32_t n)
{
	return static_cast<VillagerStates>(n);
}

class VillagerCoreTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		auto& v = info->villager.at(0);
		v.hungryForFood = 0.5f;
		v.processChecksEvery = 8;
		v.damageThresholdToGoHome = 0.3f;
		v.pauseForASecondChance = 0.01f;
		v.grownUpAge = 13;
		v.sex = SexType::Female;
		v.life = 1.0f;
		v.moveState = LivingStates::LivingMoveToPos; // 1 in every villager row of info.dat
		// the rows of info.dat the tests use; every row: no out-of clip
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1;
		}
		auto& table = info->villagerStateTable;
		// 0 INVALID: checks 1, goes home when hurt 1
		table.at(0).field0xe4 = 1;
		table.at(0).field0xe8 = 1;
		// 1 MOVE_TO_POS: moving, pause 1, drain 2e-6
		table.at(1).field0x14 = 1;
		table.at(1).canPauseForASecond = 1;
		table.at(1).field0xf8 = 2e-6f;
		// 10 FLYING / 24 IN_HAND: final, not kept as PREVIOUS, no checks
		for (const uint32_t s : {10u, 24u})
		{
			table.at(s).isFinalState = 1;
			table.at(s).keepsPreviousState = 1;
		}
		// 16 DROWNING: final, not PREVIOUS
		table.at(16).isFinalState = 1;
		table.at(16).keepsPreviousState = 1;
		// 19 GOTO_FOOD_REACTION: final, desire 0, checks 1, goes home 1
		table.at(19).isFinalState = 1;
		table.at(19).field0x4 = 0;
		table.at(19).field0xe4 = 1;
		table.at(19).field0xe8 = 1;
		// 36 GO_HOME: final, desire 8 (1), pause 1, checks 1
		table.at(36).isFinalState = 1;
		table.at(36).field0x4 = 8;
		table.at(36).canPauseForASecond = 1;
		table.at(36).field0xe4 = 1;
		// 85 CREATED / 163 DECIDE: final, no checks
		table.at(85).isFinalState = 1;
		table.at(163).isFinalState = 1;
		// 248 GO_HOME_FROM_WORSHIP: final, checks 1
		table.at(248).isFinalState = 1;
		table.at(248).field0xe4 = 1;
		// 219 ON_FIRE: final
		table.at(219).isFinalState = 1;
		// 239 PAUSE: not final, no pause, no checks, speed group 4
		table.at(239).speedIndex = 4;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::rng::emplace<TestRng>();
		Locator::livingActionSystem::emplace<FakeStateTable>();
		game_clock::SetTurn(100);
		// The villager services the code under test reaches: the game's, unless faked here
		Locator::villagerRules::emplace<ecs::systems::VillagerRules>();
		Locator::villagerWorldQueries::emplace<ecs::systems::VillagerWorldQueries>();
		Locator::villagerWorshipCheck::emplace<ecs::systems::VillagerWorshipCheck>();
		Locator::villagerChildFactory::emplace<ecs::systems::VillagerChildFactory>();
		Locator::villagerDiscipleJobs::emplace<ecs::systems::VillagerDiscipleJobs>();
		SetDraws({}, {});
	}

	void TearDown() override
	{
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

	/// Scripted draws: GameRand and GameFloatRand take the next value of their list (0 when it is empty) and log the call
	void SetDraws(std::deque<uint32_t> ints, std::deque<float> floats)
	{
		_ints = std::move(ints);
		_floats = std::move(floats);
		_draws.clear();
		game_random::testing::SetGameRand(
		    [this](uint32_t n) {
			    _draws.push_back("R" + std::to_string(n));
			    if (_ints.empty())
			    {
				    return 0u;
			    }
			    const auto v = _ints.front();
			    _ints.pop_front();
			    return v;
		    },
		    [this](float x) {
			    _draws.push_back("F" + std::to_string(x).substr(0, 3));
			    if (_floats.empty())
			    {
				    return 0.0f;
			    }
			    const auto v = _floats.front();
			    _floats.pop_front();
			    return v;
		    });
	}

	static FakeStateTable& Table() { return static_cast<FakeStateTable&>(Locator::livingActionSystem::value()); }

	static entt::entity MakeVillager(uint32_t top = 163, uint32_t final = 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto e = registry.Create();
		auto& v = registry.Assign<Villager>(e);
		v.life = 1.0f;
		v.town = entt::null;
		v.abode = entt::null;
		v.food = 1.0f;
		auto& action = registry.Assign<LivingAction>(e, S(top), static_cast<uint16_t>(0));
		action.states.at(1) = static_cast<uint8_t>(final);
		return e;
	}

	static entt::entity MakeTown()
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto town = registry.Create();
		registry.Assign<Town>(town, 1u);
		return town;
	}

	static LivingAction& Action(entt::entity e) { return Locator::entitiesRegistry::value().Get<LivingAction>(e); }
	static Villager& V(entt::entity e) { return Locator::entitiesRegistry::value().Get<Villager>(e); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }

	std::deque<uint32_t> _ints;
	std::deque<float> _floats;
	std::vector<std::string> _draws;
};
} // namespace

TEST_F(VillagerCoreTest, ConstructorFood)
{
	// the adult's scale first: (0.05 - 0.2) + 1 is below 0.9, one draw. Then 0.3 + 0.5 < 1 -> a second draw kept
	// unclamped: 0.55 + 0.5 = 1.05
	const auto& info = Locator::infoConstants::value().villager.at(0);
	auto a = MakeVillager(0);
	SetDraws({}, {0.2f, 0.3f, 0.55f});
	villager::Construct(a, info, 25, 100, false);
	EXPECT_FLOAT_EQ(V(a).food, 1.05f);
	// a first draw of 0.55 -> 1.0, one draw
	auto b = MakeVillager(0);
	SetDraws({}, {0.2f, 0.55f});
	villager::Construct(b, info, 25, 100, false);
	EXPECT_FLOAT_EQ(V(b).food, 1.0f);
	EXPECT_EQ(std::count(_draws.begin(), _draws.end(), "F0.6"), 1);
}

TEST_F(VillagerCoreTest, ConstructorLastCheckTurn)
{
	const auto& info = Locator::infoConstants::value().villager.at(0);
	// turn 100, GameRand 3 (< 100) then 5 -> 95
	auto a = MakeVillager(0);
	SetDraws({3, 5, 0}, {0.2f, 0.6f});
	villager::Construct(a, info, 25, 100, false);
	EXPECT_EQ(V(a).lastCheckTurn, 95u);
	// turn 2, GameRand 5 (not < 2) -> 2 - 2 = 0, one draw
	auto b = MakeVillager(0);
	SetDraws({5, 0}, {0.2f, 0.6f});
	villager::Construct(b, info, 25, 2, false);
	EXPECT_EQ(V(b).lastCheckTurn, 0u);
	EXPECT_EQ(std::count(_draws.begin(), _draws.end(), "R8"), 1);
}

TEST_F(VillagerCoreTest, ConstructorCounterStateAndWater)
{
	const auto& info = Locator::infoConstants::value().villager.at(0);
	auto a = MakeVillager(0);
	SetDraws({0, 0, 0}, {0.2f, 0.6f});
	villager::Construct(a, info, 25, 100, false);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 1);
	EXPECT_EQ(Top(a), 85u);
	EXPECT_EQ(Final(a), 0u);
	EXPECT_EQ(Action(a).turnsSinceStateChange, 0);
	// in the water: 16 DROWNING with the same counter, and no entry function
	auto b = MakeVillager(0);
	SetDraws({0, 0, 41}, {0.2f, 0.6f});
	Table().calls.clear();
	villager::Construct(b, info, 25, 100, true);
	EXPECT_EQ(Top(b), 16u);
	EXPECT_EQ(Action(b).turnsUntilStateChange, 42);
	EXPECT_TRUE(Table().calls.empty());
	// adult: max(age, 18); child flag and birth turn
	auto c = MakeVillager(0);
	EXPECT_EQ(villager::Construct(c, info, 5, 100, false), 5u);
	EXPECT_NE(V(c).flags & Villager::k_FlagChild, 0);
	EXPECT_EQ(V(c).birthTurn, 100u - 5u * 1500u);
	auto d = MakeVillager(0);
	EXPECT_EQ(villager::Construct(d, info, 14, 100, false), 18u);
	EXPECT_EQ(V(d).flags & Villager::k_FlagChild, 0);
	EXPECT_EQ(V(d).birthTurn, 100u - 18u * 1500u);
}

TEST_F(VillagerCoreTest, ConstructorDrawOrder)
{
	// Create's GameRand(10), the living part's age (none: this info's start age is 0), SetAge's scale (1 or 2: (0.05 -
	// 0.1) + 1 is above an adult's 0.9, so a second), food (1 or 2), lastCheck (1 or 2), the counter, then the skeleton
	// setting's scale for the age (the draws run out: 0, so 1.05 is above 0.85 and a second). Create's 2 makes no special
	// villager
	const auto& info = Locator::infoConstants::value().villager.at(0);
	auto a = MakeVillager(0);
	Locator::entitiesRegistry::value().Assign<Transform>(a, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	SetDraws({2, 3, 5, 0}, {0.1f, 0.2f});
	EXPECT_FALSE(villager::RollSpecialVillager(info).has_value());
	villager::Construct(a, info, 25, 100, false);
	const std::vector<std::string> expected = {"R10", "F0.1", "F0.1", "F0.6", "F0.6", "R8", "R8", "R500", "F0.1", "F0.1"};
	EXPECT_EQ(_draws, expected);
	// the last scale for the age is the villager's: its second draw's size, on the villager's Transform
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<Transform>(a).scale, glm::vec3((0.05f - 0.0f) + 1.0f));
}

// The try: GameRand(10) of 0 or 1 draws the name among the 48 rows; the villager's own sex chooses; a name given is not
// given again on this land
TEST_F(VillagerCoreTest, SpecialVillagerRollPicksAFreeNameOfTheVillagersSex)
{
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	for (size_t i = 0; i < info->specialVillager.size(); ++i)
	{
		auto& row = info->specialVillager.at(i);
		row.name = {'N', static_cast<char>('0' + i % 10), '\0'};
		row.sex = (i & 1) != 0 ? SexType::Female : SexType::Male;
		row.age = 30;
	}
	Locator::infoConstants::reset(info.release());
	// the fixture's row 0 is a woman
	const auto& villagerInfo = Locator::infoConstants::value().villager.at(0);
	SetDraws({2}, {});
	EXPECT_EQ(villager::RollSpecialVillager(villagerInfo), std::nullopt);
	EXPECT_EQ(_draws, std::vector<std::string>({"R10"}));
	SetDraws({1, 4}, {});
	EXPECT_EQ(villager::RollSpecialVillager(villagerInfo), std::optional<uint8_t>(5));
	EXPECT_EQ(_draws, std::vector<std::string>({"R10", "R48"}));
	// row 5 given to a villager: the next woman from 4 is 7
	auto a = MakeVillager(0);
	Locator::entitiesRegistry::value().Assign<Transform>(a, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	SetDraws({}, {});
	villager::MakeSpecialVillager(a, villagerInfo, 5, 25, 100);
	EXPECT_TRUE(Locator::entitiesRegistry::value().Context().namedVillagersUsed.at(5));
	SetDraws({0, 4}, {});
	EXPECT_EQ(villager::RollSpecialVillager(villagerInfo), std::optional<uint8_t>(7));
}

// The special villager after its construction: its name's row on it, the name's age (a newborn keeps 1), and the scale
// for that age drawn again
TEST_F(VillagerCoreTest, SpecialVillagerTakesItsNamesAge)
{
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->specialVillager.at(3).age = 40;
	Locator::infoConstants::reset(info.release());
	const auto& villagerInfo = Locator::infoConstants::value().villager.at(0);
	auto& registry = Locator::entitiesRegistry::value();
	auto a = MakeVillager(0);
	registry.Assign<Transform>(a, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	villager::Construct(a, villagerInfo, 25, 100, false);
	// an adult starts at 0.9, below (0.05 - 0.1) + 1, so the grown scale is a second draw
	SetDraws({}, {0.1f, 0.02f});
	EXPECT_EQ(villager::MakeSpecialVillager(a, villagerInfo, 3, 25, 100), 40u);
	EXPECT_EQ(_draws, std::vector<std::string>({"F0.1", "F0.1"}));
	EXPECT_EQ(registry.Get<NamedVillager>(a).row, 3);
	EXPECT_EQ(V(a).birthTurn, 100u - 40u * 1500u);
	EXPECT_EQ(registry.Get<Transform>(a).scale, glm::vec3((0.05f - 0.02f) + 1.0f));
	// a newborn keeps its age 1
	auto b = MakeVillager(0);
	registry.Assign<Transform>(b, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	villager::Construct(b, villagerInfo, 1, 100, false);
	EXPECT_EQ(villager::MakeSpecialVillager(b, villagerInfo, 3, 1, 100), 1u);
	EXPECT_EQ(V(b).birthTurn, 100u - 1500u);
}

// The living part's age: the draw below half the start age plus three quarters of it, as whole numbers; no draw for a
// start age below 2
TEST_F(VillagerCoreTest, TheLivingPartsAgeDraw)
{
	GVillagerInfo info {};
	info.startAge = 25;
	SetDraws({7}, {});
	EXPECT_EQ(ecs::living::ConstructorAgeDraw(info), 7u + 25u - 6u);
	EXPECT_EQ(_draws, std::vector<std::string>({"R12"}));
	info.startAge = 1;
	SetDraws({}, {});
	EXPECT_EQ(ecs::living::ConstructorAgeDraw(info), 1u);
	EXPECT_TRUE(_draws.empty());
}

TEST_F(VillagerCoreTest, ConstructorDrawsTheLivingPartsAgeFirst)
{
	// the living part's age: GameRand(start age / 2), before SetAge's scale, its value not kept
	auto& info = const_cast<GVillagerInfo&>(Locator::infoConstants::value().villager.at(0));
	const auto startAge = info.startAge;
	info.startAge = 25;
	auto a = MakeVillager(0);
	SetDraws({7}, {0.1f, 0.2f});
	EXPECT_EQ(villager::Construct(a, info, 30, 100, false), 30u);
	ASSERT_GE(_draws.size(), 2u);
	EXPECT_EQ(_draws.at(0), "R12");
	EXPECT_EQ(_draws.at(1), "F0.1");
	info.startAge = startAge;
}

TEST_F(VillagerCoreTest, CreatedCountsDown)
{
	auto a = MakeVillager(85);
	Action(a).turnsUntilStateChange = 2;
	villager::VillagerCreated(Action(a));
	villager::VillagerCreated(Action(a));
	EXPECT_EQ(Top(a), 85u);
	villager::VillagerCreated(Action(a));
	EXPECT_EQ(Top(a), 163u);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 0);
}

TEST_F(VillagerCoreTest, ExitRefused)
{
	auto a = MakeVillager(163);
	Action(a).turnsSinceStateChange = 7;
	Table().rows[163].exit = 0;
	EXPECT_EQ(villager::SetTopState(a, S(163)), villager::k_ExitRefused);
	EXPECT_EQ(Top(a), 163u);
	EXPECT_EQ(Action(a).turnsSinceStateChange, 7);
	EXPECT_EQ(Table().Count("entry 163 163 163"), 0u);
}

TEST_F(VillagerCoreTest, ExitRefusedByFinal)
{
	// TOP 1 (not final, no exit), FINAL 36 whose exit refuses
	auto a = MakeVillager(1, 36);
	Table().rows[36].exit = 0;
	SetDraws({}, {0.9f}); // 163 has no pause anyway
	EXPECT_EQ(villager::SetTopState(a, S(163)), villager::k_ExitRefused);
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 36u);
	// TOP == FINAL (36 is final): its exit runs once
	auto b = MakeVillager(36);
	Table().rows[36].exit = 1;
	Table().calls.clear();
	villager::SetTopState(b, S(163));
	EXPECT_EQ(Table().Count("exit 36 163"), 1u);
}

TEST_F(VillagerCoreTest, EntryRefused)
{
	auto a = MakeVillager(163);
	Table().rows[19].entry = 0;
	EXPECT_EQ(villager::SetTopState(a, S(19)), villager::k_EntryRefused);
	EXPECT_EQ(Top(a), 163u); // CallEntryStateFunction(163) after the refusal
	EXPECT_EQ(Final(a), 0u);
	EXPECT_EQ(Table().Count("entry 163 163 163"), 1u);
}

TEST_F(VillagerCoreTest, EntrySetsItself)
{
	auto a = MakeVillager(163);
	Table().rows[19].entry = 0x23;
	EXPECT_EQ(villager::SetTopState(a, S(19)), villager::k_Done);
	EXPECT_EQ(Top(a), 163u);
}

TEST_F(VillagerCoreTest, SettingTopClearsFinalWithTheTown)
{
	auto a = MakeVillager(1, 36);
	V(a).town = MakeTown();
	villager::SetState(a, Index::Top, S(163));
	EXPECT_EQ(Final(a), 0u);
	const auto& town = Locator::entitiesRegistry::value().Get<Town>(V(a).town);
	EXPECT_FLOAT_EQ(town.desire.doingNow.at(8), -1.0f);
	EXPECT_FLOAT_EQ(town.desire.doingNowCount.at(8), -1.0f);
}

TEST_F(VillagerCoreTest, PreviousRule)
{
	auto a = MakeVillager(163);
	V(a).town = MakeTown();
	villager::SetState(a, Index::Previous, S(24));
	EXPECT_EQ(Action(a).states.at(2), 0u);
	villager::SetState(a, Index::Previous, S(36));
	EXPECT_EQ(Action(a).states.at(2), 36u);
	const auto& town = Locator::entitiesRegistry::value().Get<Town>(V(a).town);
	EXPECT_FLOAT_EQ(town.desire.doingNowCount.at(8), 1.0f); // the original's quirk: PREVIOUS counts too
}

TEST_F(VillagerCoreTest, AdjustTownModifier)
{
	auto a = MakeVillager(163);
	villager::AdjustTownModifier(a, S(36), true); // no town: nothing
	V(a).town = MakeTown();
	const auto& town = Locator::entitiesRegistry::value().Get<Town>(V(a).town);
	villager::AdjustTownModifier(a, S(163), true); // desire -1: nothing
	EXPECT_FLOAT_EQ(town.desire.doingNowCount.at(8), 0.0f);
	villager::AdjustTownModifier(a, S(36), true);
	EXPECT_FLOAT_EQ(town.desire.doingNow.at(8), 1.0f);
	EXPECT_FLOAT_EQ(town.desire.doingNowCount.at(8), 1.0f);
	villager::AdjustTownModifier(a, S(36), false);
	EXPECT_FLOAT_EQ(town.desire.doingNow.at(8), 0.0f);
	EXPECT_FLOAT_EQ(town.desire.doingNowCount.at(8), 0.0f);
}

TEST_F(VillagerCoreTest, PauseForASecondChance)
{
	// life 0.5: x = 0.5, x^3 = 0.125: rand - 0.0625 < 0.01
	auto a = MakeVillager(163);
	V(a).life = 0.5f;
	SetDraws({}, {0.0724f});
	EXPECT_EQ(villager::SetTopState(a, S(36)), villager::k_Done);
	EXPECT_EQ(Top(a), 239u);
	EXPECT_EQ(Final(a), 36u);
	EXPECT_EQ(Table().Count("entry 36 163 36"), 1u); // the entry of s runs when the pause starts
	auto b = MakeVillager(163);
	V(b).life = 0.5f;
	SetDraws({}, {0.0726f});
	villager::SetTopState(b, S(36));
	EXPECT_EQ(Top(b), 36u);
	// poisoned: x = 1 - 0.25, threshold 0.01 + 0.2109
	auto c = MakeVillager(163);
	V(c).life = 0.5f;
	Locator::entitiesRegistry::value().Assign<Poisoned>(c);
	SetDraws({}, {0.2208f});
	villager::SetTopState(c, S(36));
	EXPECT_EQ(Top(c), 239u);
	auto d = MakeVillager(163);
	V(d).life = 0.5f;
	Locator::entitiesRegistry::value().Assign<Poisoned>(d);
	SetDraws({}, {0.2210f});
	villager::SetTopState(d, S(36));
	EXPECT_EQ(Top(d), 36u);
	// TOP 239 or a state without the pause flag: no draw
	auto e = MakeVillager(239, 36);
	SetDraws({}, {});
	villager::SetTopState(e, S(36));
	auto f = MakeVillager(163);
	villager::SetTopState(f, S(19));
	EXPECT_TRUE(_draws.empty());
}

TEST_F(VillagerCoreTest, PauseEndsInTheFinalState)
{
	auto a = MakeVillager(163);
	V(a).life = 0.5f;
	SetDraws({}, {0.0f});
	villager::SetTopState(a, S(36));
	ASSERT_EQ(Top(a), 239u);
	_draws.clear();
	// no clip resources in the tests: the clip is over
	villager::PauseForASecond(Action(a));
	EXPECT_EQ(Top(a), 36u);
	EXPECT_EQ(Final(a), 0u);
	EXPECT_EQ(Table().Count("entry 36 163 36"), 1u);
	EXPECT_EQ(Table().Count("entry 36 36 36"), 1u); // the entry again, told the final state of the pause
	EXPECT_TRUE(_draws.empty());                    // TOP was 239: no second pause
}

TEST_F(VillagerCoreTest, PeriodicChecksEveryNineTurns)
{
	auto a = MakeVillager(36);
	V(a).lastCheckTurn = 95;
	std::vector<uint32_t> checks;
	for (uint32_t turn = 96; turn <= 125; ++turn)
	{
		const auto before = V(a).lastCheckTurn;
		villager::CheckEveryTime(a, turn);
		if (V(a).lastCheckTurn != before)
		{
			checks.push_back(turn);
		}
	}
	const std::vector<uint32_t> expected = {104, 113, 122};
	EXPECT_EQ(checks, expected);
	// a state without checks (163): none
	auto b = MakeVillager(163);
	V(b).lastCheckTurn = 95;
	for (uint32_t turn = 96; turn <= 125; ++turn)
	{
		villager::CheckEveryTime(b, turn);
	}
	EXPECT_EQ(V(b).lastCheckTurn, 95u);
}

TEST_F(VillagerCoreTest, LifeWear)
{
	auto a = MakeVillager(1, 36);
	V(a).lastCheckTurn = 100;
	villager::CheckEveryTime(a, 100);
	EXPECT_FLOAT_EQ(V(a).life, 1.0f - 2e-6f);
	auto b = MakeVillager(36);
	villager::CheckEveryTime(b, 100);
	EXPECT_FLOAT_EQ(V(b).life, 1.0f);
}

TEST_F(VillagerCoreTest, DeathReasons)
{
	auto a = MakeVillager(36);
	V(a).life = 0.0f;
	Table().calls.clear();
	villager::ProcessState(a, 100);
	EXPECT_EQ(villager::GetDeathReason(a), DeathReason::Exhaustion);
	EXPECT_EQ(Table().Count("state 36"), 0u); // the death left 36 (SetDying: TOP 14) before CallState
	auto b = MakeVillager(36);
	V(b).life = 0.0f;
	V(b).flags = Villager::k_FlagAtWorshipSite;
	villager::CheckEveryTime(b, 100);
	EXPECT_EQ(villager::GetDeathReason(b), DeathReason::Chant);
	// the bit as the worship code keeps it today: WorshipVillager::atSite
	auto d = MakeVillager(36);
	V(d).life = 0.0f;
	Locator::entitiesRegistry::value().Assign<WorshipVillager>(d).atSite = true;
	villager::CheckEveryTime(d, 100);
	EXPECT_EQ(villager::GetDeathReason(d), DeathReason::Chant);
	// GetFinalState 248 GO_HOME_FROM_WORSHIP
	auto c = MakeVillager(248);
	V(c).life = 0.0f;
	villager::CheckEveryTime(c, 100);
	EXPECT_EQ(villager::GetDeathReason(c), DeathReason::Chant);
}

TEST_F(VillagerCoreTest, HurtGoesHome)
{
	// switched off (on in the game): the villager stays
	test::FakeVillagerRules rules;
	rules.goHomeEnabled = [] { return false; };
	Locator::villagerRules::emplace<test::FakeVillagerRules>(rules);
	auto off = MakeVillager(1, 0);
	V(off).life = 0.29f;
	V(off).lastCheckTurn = 0;
	villager::CheckEveryTime(off, 100);
	EXPECT_EQ(Top(off), 1u);
	Locator::villagerRules::emplace<ecs::systems::VillagerRules>();
	// TOP 1 (moving) with FINAL 0: the checks are row 0's (checks 1, goes home 1)
	auto a = MakeVillager(1, 0);
	V(a).life = 0.29f;
	V(a).lastCheckTurn = 0;
	SetDraws({}, {0.9f}); // 36 may pause: no
	villager::CheckEveryTime(a, 100);
	EXPECT_EQ(Top(a), 36u);
	// reacting to food (19): only when food > hungryForFood
	auto b = MakeVillager(19);
	V(b).life = 0.29f;
	V(b).food = 0.5f;
	villager::CheckEveryTime(b, 100);
	EXPECT_EQ(Top(b), 19u);
	auto c = MakeVillager(19);
	V(c).life = 0.29f;
	V(c).food = 0.6f;
	SetDraws({}, {0.9f});
	villager::CheckEveryTime(c, 100);
	EXPECT_EQ(Top(c), 36u);
	// downed by a predator: no
	auto d = MakeVillager(1, 0);
	V(d).life = 0.29f;
	Locator::entitiesRegistry::value().Assign<DownedVillager>(d);
	villager::CheckEveryTime(d, 100);
	EXPECT_EQ(Top(d), 1u);
}

// What cannot be destroyed cannot be eaten either: one flag for both, which the land-to-land vortex gives what it
// throws out and takes back as it goes
TEST_F(VillagerCoreTest, WhatCannotBeDestroyedCannotBeEaten)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto thrown = MakeVillager(1, 0);
	EXPECT_FALSE(ecs::script_held::CannotBeEaten(thrown));
	registry.Assign<Indestructible>(thrown);
	EXPECT_TRUE(ecs::script_held::CannotBeEaten(thrown));
	registry.Remove<Indestructible>(thrown);
	EXPECT_FALSE(ecs::script_held::CannotBeEaten(thrown));
	// marked not to be eaten, it cannot be destroyed either
	const auto marked = MakeVillager(1, 0);
	ecs::script_held::SetCannotBeEaten(marked);
	EXPECT_TRUE(registry.AllOf<Indestructible>(marked));
	ecs::script_held::SetCannotBeEaten(marked);
	EXPECT_TRUE(ecs::script_held::CannotBeEaten(marked));
	EXPECT_FALSE(ecs::script_held::CannotBeEaten(entt::entity {entt::null}));
}

TEST_F(VillagerCoreTest, SetupMoveToWithHugKeepsFinal)
{
	auto a = MakeVillager(163);
	Locator::entitiesRegistry::value().Assign<WallHug>(a, glm::vec2(), glm::vec2(), 0.0f, 0.1f);
	EXPECT_EQ(villager::SetupMoveToWithHug(a, glm::vec2(10.0f, 20.0f), S(219)), 1u);
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 219u);
	EXPECT_TRUE(Locator::entitiesRegistry::value().AllOf<MoveStateLinearTag>(a));
	// the entry of d refused: back to 163, no walk
	auto b = MakeVillager(163);
	Locator::entitiesRegistry::value().Assign<WallHug>(b, glm::vec2(), glm::vec2(), 0.0f, 0.1f);
	Table().rows[219].entry = 0;
	EXPECT_EQ(villager::SetupMoveToWithHug(b, glm::vec2(10.0f, 20.0f), S(219)), 0u);
	EXPECT_EQ(Top(b), 163u);
	EXPECT_FALSE(Locator::entitiesRegistry::value().AllOf<MoveStateLinearTag>(b));
}

// ---- the script's villager (VillagerScript.h, both: MOVE_GAME_THING, SET_SCRIPT_STATE / _ULONG, PLAYED) ---------

namespace
{
/// A walker in the map cells, as a villager is from its creation; one the hand holds (top 24 IN_HAND) is out of them
entt::entity MakeWalker(uint32_t top, glm::vec2 at, float speed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto e = registry.Create();
	auto& v = registry.Assign<Villager>(e);
	v.life = 1.0f;
	v.town = entt::null;
	v.abode = entt::null;
	registry.Assign<LivingAction>(e, static_cast<VillagerStates>(top), static_cast<uint16_t>(7));
	registry.Assign<WallHug>(e, glm::vec2(), glm::vec2(), 0.0f, speed);
	auto& transform = registry.Assign<Transform>(e);
	transform.position = glm::vec3(at.x, 0.0f, at.y);
	// a script's villager: setting a state keeps its speed (the script-controlled flag)
	ecs::script_held::SetControlledByScript(e, true);
	if (top != static_cast<uint32_t>(VillagerStates::InHand))
	{
		ecs::map_cells::InsertMapObject(e);
	}
	return e;
}
} // namespace

TEST_F(VillagerCoreTest, ScriptAreWeThereIsStrict)
{
	// AreWeThere: d^2 < (speed + r)^2 in whole units; 0.5 m a turn is 3276 units, 0.4 m is 2621 and 0.5 m is 3276
	const auto a = MakeWalker(163, glm::vec2(0.0f), 0.5f);
	EXPECT_TRUE(villager::AreWeThere(a, glm::vec2(0.4f, 0.0f), 0.0f));
	EXPECT_FALSE(villager::AreWeThere(a, glm::vec2(0.5f, 0.0f), 0.0f));
	EXPECT_TRUE(villager::AreWeThere(a, glm::vec2(0.5f, 0.0f), 0.5f));
	// r is in map units, not metres: 0.6 m (3932 units) is out of reach with r = 0.5
	EXPECT_FALSE(villager::AreWeThere(a, glm::vec2(0.6f, 0.0f), 0.5f));
}

TEST_F(VillagerCoreTest, ScriptSetupMoveToPosStepsThrough)
{
	// SetupMoveToPos -> SetupMobileMoveToPos: TOP MOVE_TO_POS, FINAL IN_SCRIPT, STEP_THROUGH
	auto& registry = Locator::entitiesRegistry::value();
	const auto a = MakeWalker(163, glm::vec2(0.0f), 0.1f);
	EXPECT_EQ(villager::SetupMoveToPos(a, glm::vec2(10.0f, 0.0f), VillagerStates::InScript), 1u);
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 4u);
	EXPECT_TRUE(registry.AllOf<MoveStateStepThroughTag>(a));
	EXPECT_FALSE(registry.AllOf<MoveStateLinearTag>(a));
	const auto& wallHug = registry.Get<WallHug>(a);
	EXPECT_EQ(wallHug.goal, glm::vec2(10.0f, 0.0f));
	// InitStepsXZ: a = 0 (+x), speed 0.1 m = 655 (ConvertMetersToWholeDistance), x = (cos 0 = 65536 x (655 >> 4)) >> 12
	// = 640 MapCoords = 0.09765625 m, z = sin 0 = 0
	EXPECT_EQ(wallHug.step, glm::ivec2(640, 0));
	EXPECT_EQ(wallHug.stepMetres.x, 0.09765625f);
	EXPECT_EQ(wallHug.stepMetres.y, 0.0f);
	EXPECT_EQ(wallHug.gameAngle, 0u);
	// the step is aimed again on the walk's first turn
	EXPECT_EQ(wallHug.turnsUntilStepRebuild, 1);
	// there already: ARRIVED
	const auto b = MakeWalker(163, glm::vec2(5.0f, 5.0f), 0.1f);
	EXPECT_EQ(villager::SetupMoveToPos(b, glm::vec2(5.05f, 5.0f), VillagerStates::InScript), 1u);
	EXPECT_TRUE(registry.AllOf<MoveStateArrivedTag>(b));
	// the exit refused: no walk, 0
	const auto c = MakeWalker(163, glm::vec2(0.0f), 0.1f);
	Table().rows[163].exit = 0;
	EXPECT_EQ(villager::SetupMoveToPos(c, glm::vec2(10.0f, 0.0f), VillagerStates::InScript), 0u);
	EXPECT_FALSE(registry.AllOf<MoveStateStepThroughTag>(c));
}

TEST_F(VillagerCoreTest, ScriptSetupMoveToPosResetsTheHugOnlyWhenNotThere)
{
	// SetupMobileMoveToPos(pos): an entry distance kept apart (0xFFFF or more) goes back to 0 first; there already, the
	// circle held is kept (ARRIVED); not there, the circle-hug info is reset (STEP_THROUGH)
	auto& registry = Locator::entitiesRegistry::value();
	const auto hold = [&registry](entt::entity villager, uint32_t entryDistance) {
		WallHugObjectReference held {};
		held.stepsAway = 7;
		held.entity = registry.Create();
		held.entryDistance = entryDistance;
		registry.Assign<WallHugObjectReference>(villager, held);
	};
	const auto there = MakeWalker(163, glm::vec2(5.0f, 5.0f), 0.1f);
	hold(there, 40);
	EXPECT_EQ(villager::SetupMoveToPos(there, glm::vec2(5.05f, 5.0f), VillagerStates::InScript), 1u);
	EXPECT_TRUE(registry.AllOf<MoveStateArrivedTag>(there));
	ASSERT_TRUE(registry.AllOf<WallHugObjectReference>(there));
	EXPECT_EQ(registry.Get<WallHugObjectReference>(there).stepsAway, 7u);
	EXPECT_EQ(registry.Get<WallHugObjectReference>(there).entryDistance, 40u);
	const auto keptApart = MakeWalker(163, glm::vec2(5.0f, 5.0f), 0.1f);
	hold(keptApart, 0x7FFFFFFF);
	EXPECT_EQ(villager::SetupMoveToPos(keptApart, glm::vec2(5.05f, 5.0f), VillagerStates::InScript), 1u);
	ASSERT_TRUE(registry.AllOf<WallHugObjectReference>(keptApart));
	EXPECT_EQ(registry.Get<WallHugObjectReference>(keptApart).entryDistance, 0u);
	const auto notThere = MakeWalker(163, glm::vec2(0.0f), 0.1f);
	hold(notThere, 40);
	EXPECT_EQ(villager::SetupMoveToPos(notThere, glm::vec2(10.0f, 0.0f), VillagerStates::InScript), 1u);
	EXPECT_TRUE(registry.AllOf<MoveStateStepThroughTag>(notThere));
	EXPECT_FALSE(registry.AllOf<WallHugObjectReference>(notThere));
}

TEST_F(VillagerCoreTest, ScriptSetScriptStateKeepsTheWalk)
{
	// SetScriptState: StorePreviousState, exit, entry, SetAnim(1), turns until the state change = 0; the walk it
	// leaves is kept as it was, to go on with when it comes back to it
	auto& registry = Locator::entitiesRegistry::value();
	const auto a = MakeWalker(163, glm::vec2(0.0f), 0.1f);
	ASSERT_EQ(villager::SetupMoveToPos(a, glm::vec2(10.0f, 0.0f), VillagerStates::InScript), 1u);
	Table().calls.clear();
	villager_script::SetScriptState(a, VillagerStates::ScriptPlayAnim);
	EXPECT_EQ(Top(a), 200u);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 0u);
	EXPECT_EQ(Table().Count("exit 1 200"), 1u);
	EXPECT_EQ(Table().Count("entry 200 4 200"), 1u);
	EXPECT_TRUE(registry.AllOf<MoveStateStepThroughTag>(a));
	EXPECT_EQ(registry.Get<WallHug>(a).goal, glm::vec2(10.0f, 0.0f));
	// in the hand: nothing (IsObjectInMap)
	const auto b = MakeWalker(24, glm::vec2(0.0f), 0.1f);
	villager_script::SetScriptState(b, VillagerStates::ScriptPlayAnim);
	EXPECT_EQ(Top(b), 24u);
}

TEST_F(VillagerCoreTest, AReactionsStateChangeKeepsTheWalk)
{
	// the reactions' SetTopState is the villager's: the walk a change leaves keeps its move state and goal; a change
	// the exit refuses changes nothing either
	auto& registry = Locator::entitiesRegistry::value();
	const auto a = MakeWalker(163, glm::vec2(0.0f), 0.1f);
	ASSERT_EQ(villager::SetupMoveToPos(a, glm::vec2(10.0f, 0.0f), VillagerStates::InScript), 1u);
	static_cast<void>(ecs::villager_reactions::SetTopState(a, VillagerStates::DecideWhatToDo));
	EXPECT_EQ(Top(a), 163u);
	EXPECT_TRUE(registry.AllOf<MoveStateStepThroughTag>(a));
	EXPECT_EQ(registry.Get<WallHug>(a).goal, glm::vec2(10.0f, 0.0f));
	const auto b = MakeWalker(163, glm::vec2(0.0f), 0.1f);
	ASSERT_EQ(villager::SetupMoveToPos(b, glm::vec2(10.0f, 0.0f), VillagerStates::InScript), 1u);
	Table().rows[1].exit = 0;
	EXPECT_EQ(ecs::villager_reactions::SetTopState(b, VillagerStates::DecideWhatToDo), villager::k_ExitRefused);
	EXPECT_EQ(Top(b), 1u);
	EXPECT_TRUE(registry.AllOf<MoveStateStepThroughTag>(b));
}

TEST_F(VillagerCoreTest, ScriptRefusesAVillagerOutOfTheMap)
{
	// the in-map flag is the map cells' one, not the state: a villager out of the cells (thrown, in flight) in any
	// state is refused by SET_SCRIPT_STATE and MOVE_GAME_THING, and taken again once it lands
	auto& registry = Locator::entitiesRegistry::value();
	const auto a = MakeWalker(163, glm::vec2(0.0f), 0.1f);
	ASSERT_TRUE(villager::IsObjectInMap(a));
	ecs::map_cells::RemoveMapObject(a);
	EXPECT_FALSE(villager::IsObjectInMap(a));
	EXPECT_FALSE(villager_script::CanBeDirected(a));
	villager_script::SetScriptState(a, VillagerStates::ScriptPlayAnim);
	EXPECT_EQ(Top(a), 163u);
	villager_script::MoveTo(a, glm::vec2(10.0f, 0.0f));
	EXPECT_EQ(Top(a), 163u);
	EXPECT_FALSE(registry.AllOf<MoveStateStepThroughTag>(a));
	// landed: in the map again
	ecs::map_cells::InsertMapObject(a);
	EXPECT_TRUE(villager_script::CanBeDirected(a));
	villager_script::SetScriptState(a, VillagerStates::ScriptPlayAnim);
	EXPECT_EQ(Top(a), 200u);
}

TEST_F(VillagerCoreTest, ScriptMoveToWalksOrWaits)
{
	// MOVE_GAME_THING: not there yet -> SetupMoveToPos(IN_SCRIPT); there -> SetScriptState(IN_SCRIPT)
	auto& registry = Locator::entitiesRegistry::value();
	const auto a = MakeWalker(163, glm::vec2(0.0f), 0.1f);
	villager_script::MoveTo(a, glm::vec2(10.0f, 0.0f));
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 4u);
	EXPECT_TRUE(registry.AllOf<MoveStateStepThroughTag>(a));
	const auto b = MakeWalker(163, glm::vec2(5.0f, 5.0f), 0.1f);
	villager_script::MoveTo(b, glm::vec2(5.05f, 5.0f));
	EXPECT_EQ(Top(b), 4u);
	EXPECT_FALSE(registry.AllOf<MoveStateStepThroughTag>(b));
	// in the hand: nothing (IsObjectInMap)
	const auto c = MakeWalker(24, glm::vec2(0.0f), 0.1f);
	villager_script::MoveTo(c, glm::vec2(10.0f, 0.0f));
	EXPECT_EQ(Top(c), 24u);
}

TEST_F(VillagerCoreTest, ScriptPlayAnimCountsDown)
{
	// ScriptPlayAnim and HasPlayedScriptAnimation
	const auto a = MakeWalker(200, glm::vec2(0.0f), 0.1f);
	villager_script::SetScriptAnimation(a, 224, 2);
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<ScriptAnimation>(a).clip, static_cast<AnimId>(224));
	EXPECT_EQ(villager_script::ScriptClip(a), 224);
	EXPECT_FALSE(villager_script::HasPlayedScriptAnimation(a)); // TOP 200, 2 left
	villager_script::ScriptPlayAnim(Action(a));
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<ScriptAnimation>(a).playsLeft, 1u);
	EXPECT_EQ(Top(a), 23u); // PlayAnimThenSetState: WAIT_FOR_ANIMATION, then 200 again
	EXPECT_EQ(Final(a), 200u);
	EXPECT_FALSE(villager_script::HasPlayedScriptAnimation(a));
	// the clip ends (no animation here: ready at once): SetTopStateToFinal
	EXPECT_EQ(villager::WaitForAnimation(Action(a)), 0u);
	EXPECT_EQ(Top(a), 200u);
	villager_script::ScriptPlayAnim(Action(a));
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<ScriptAnimation>(a).playsLeft, 0u);
	EXPECT_EQ(Top(a), 23u);
	EXPECT_EQ(Final(a), 4u); // the last time goes back to IN_SCRIPT
	EXPECT_EQ(villager::WaitForAnimation(Action(a)), 0u);
	EXPECT_EQ(Top(a), 4u);
	EXPECT_TRUE(villager_script::HasPlayedScriptAnimation(a));
	// TOP 200 with no times left: complete, and the state does nothing
	const auto b = MakeWalker(200, glm::vec2(0.0f), 0.1f);
	EXPECT_TRUE(villager_script::HasPlayedScriptAnimation(b));
	EXPECT_EQ(villager_script::ScriptPlayAnim(Action(b)), 1u);
	EXPECT_EQ(Top(b), 200u);
}

TEST_F(VillagerCoreTest, ScriptWanderAroundPositionSetsOffThenStands)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto a = MakeWalker(199, glm::vec2(0.0f), 0.1f);
	Action(a).turnsUntilStateChange = 0;
	// The script's place, its distance, and its fewest and most turns (the pair a script's clip is kept in)
	villager_script::SetScriptPlace(a, glm::vec2(100.0f, 200.0f));
	villager_script::SetScriptNumber(a, 8.0f);
	villager_script::SetScriptAnimation(a, 4, 20);
	ASSERT_TRUE(registry.AllOf<ScriptStateArguments>(a));
	EXPECT_EQ(registry.Get<const ScriptStateArguments>(a).place, glm::vec2(100.0f, 200.0f));
	EXPECT_FLOAT_EQ(registry.Get<const ScriptStateArguments>(a).number, 8.0f);
	// A new place keeps the number, and a new number the place
	villager_script::SetScriptPlace(a, glm::vec2(110.0f, 210.0f));
	EXPECT_FLOAT_EQ(registry.Get<const ScriptStateArguments>(a).number, 8.0f);
	villager_script::SetScriptNumber(a, 6.0f);
	EXPECT_EQ(registry.Get<const ScriptStateArguments>(a).place, glm::vec2(110.0f, 210.0f));
	// No turns left: the bearing from a whole turn, the distance from the script's, the walk, then the pause from the
	// fewest plus a draw of most - fewest
	SetDraws({5}, {0.0f, 3.0f});
	EXPECT_EQ(villager_script::ScriptWanderAroundPosition(Action(a)), 1u);
	EXPECT_EQ(_draws, (std::vector<std::string> {"F6.2", "F6.0", "R16"}));
	EXPECT_EQ(Top(a), 1u); // MOVE_TO_POS, coming back into the wander
	EXPECT_EQ(Final(a), 199u);
	EXPECT_TRUE(registry.AllOf<MoveStateLinearTag>(a));
	EXPECT_EQ(Action(a).turnsUntilStateChange, 9u);
	// With turns left it stands, one fewer a turn, and draws nothing
	SetDraws({}, {});
	EXPECT_EQ(villager_script::ScriptWanderAroundPosition(Action(a)), 1u);
	EXPECT_TRUE(_draws.empty());
	EXPECT_EQ(Action(a).turnsUntilStateChange, 8u);
	// A villager is the only thing that keeps them
	const auto thing = registry.Create();
	villager_script::SetScriptPlace(thing, glm::vec2(1.0f, 2.0f));
	villager_script::SetScriptNumber(thing, 3.0f);
	EXPECT_FALSE(registry.AllOf<ScriptStateArguments>(thing));
}

TEST_F(VillagerCoreTest, ScriptExitInScript)
{
	// ExitInScript: a script state next -> 1; else ExitNoChangeState
	// the test's own InfoConstants (SetUp), written through the Locator's const view
	auto& table = const_cast<InfoConstants&>(Locator::infoConstants::value()).villagerStateTable;
	table.at(200).isScriptState = 1;
	const auto a = MakeWalker(4, glm::vec2(0.0f), 0.1f);
	EXPECT_EQ(villager_script::ExitInScript(Action(a), VillagerStates::ScriptPlayAnim), 1u);
	// 24 IN_HAND: IsStateForInterface -> 1
	EXPECT_EQ(villager_script::ExitInScript(Action(a), VillagerStates::InHand), 1u);
	// 36 GO_HOME (final, another exit than the final state's, not script-interruptable): 0
	EXPECT_EQ(villager_script::ExitInScript(Action(a), VillagerStates::GoHome), 0u);
	table.at(36).isScriptInterruptableState = 1;
	EXPECT_EQ(villager_script::ExitInScript(Action(a), VillagerStates::GoHome), 1u);
}

TEST_F(VillagerCoreTest, ScriptExitInScriptLetsGoOfTheCircle)
{
	// ExitInScript first lets go of the circle held (the circle, its turns to it, its entry distance), whatever it
	// returns; the walk itself (goal, move state) is kept
	auto& registry = Locator::entitiesRegistry::value();
	const auto hold = [&registry](entt::entity villager) {
		WallHugObjectReference held {};
		held.stepsAway = 7;
		held.entity = registry.Create();
		held.entryDistance = 0x10000;
		registry.Assign<WallHugObjectReference>(villager, held);
	};
	const auto letsGo = MakeWalker(4, glm::vec2(0.0f), 0.1f);
	hold(letsGo);
	registry.Assign<MoveStateOrbitTag>(letsGo, MoveStateClockwise::Clockwise, glm::vec2(1.0f, 2.0f));
	EXPECT_EQ(villager_script::ExitInScript(Action(letsGo), VillagerStates::InHand), 1u);
	EXPECT_FALSE(registry.AllOf<WallHugObjectReference>(letsGo));
	EXPECT_TRUE(registry.AllOf<MoveStateOrbitTag>(letsGo));
	// 36 GO_HOME (final, another exit than the final state's, not script-interruptable): 0, and the circle is let go of
	// all the same
	const auto refused = MakeWalker(4, glm::vec2(0.0f), 0.1f);
	hold(refused);
	EXPECT_EQ(villager_script::ExitInScript(Action(refused), VillagerStates::GoHome), 0u);
	EXPECT_FALSE(registry.AllOf<WallHugObjectReference>(refused));
	// Holding none, nothing else changes
	const auto none = MakeWalker(4, glm::vec2(0.0f), 0.1f);
	EXPECT_EQ(villager_script::ExitInScript(Action(none), VillagerStates::InHand), 1u);
	EXPECT_FALSE(registry.AllOf<WallHugObjectReference>(none));
}

// ---- a knocked resting proxy (DynamicsSystem::ProcessTurn -> the class's InitialisePhysics) -----------------------

namespace
{
/// The game's PhysicsClassHooks::InitialisePhysicsKnocked for the entity's class: true = started (the body is woken)
bool Knock(entt::entity entity)
{
	ecs::PhysicsGameHooks hooks;
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Locator::entitiesRegistry::value()));
	ecs::PhysicsEntry po;
	po.entity = entity;
	return hooks.InitialisePhysicsKnocked(dynamics, po);
}

entt::entity MakeAnimal(uint8_t top, uint8_t final, uint8_t previous)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto e = registry.Create();
	registry.Assign<Animal>(e).type = AnimalInfo::Sheep;
	registry.Assign<Transform>(e, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& brain = registry.Assign<AnimalBrain>(e);
	brain.topState = top;
	brain.finalState = final;
	brain.previousState = previous;
	return e;
}
} // namespace

TEST_F(VillagerCoreTest, KnockedVillagerFlies)
{
	// walking (TOP 1 MOVE_TO_POS, FINAL 36 GO_HOME): InitialisePhysics stores the final state as PREVIOUS
	// (StorePreviousState), SetTopState(FLYING) takes -> started
	const auto walking = MakeVillager(1, 36);
	EXPECT_TRUE(Knock(walking));
	EXPECT_EQ(Top(walking), 10u);
	EXPECT_EQ(Action(walking).states.at(2), 36u);
	// deciding (TOP 163, final): PREVIOUS = 163
	const auto deciding = MakeVillager(163);
	EXPECT_TRUE(Knock(deciding));
	EXPECT_EQ(Top(deciding), 10u);
	EXPECT_EQ(Action(deciding).states.at(2), 163u);
}

TEST_F(VillagerCoreTest, KnockedVillagerInHandKeepsPrevious)
{
	// IN_HAND: no StorePreviousState (24 made storable here, so a store would write 24)
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->villagerStateTable.at(24).keepsPreviousState = 0;
	Locator::infoConstants::reset(info.release());
	const auto held = MakeVillager(24);
	Action(held).states.at(2) = 163;
	EXPECT_TRUE(Knock(held));
	EXPECT_EQ(Top(held), 10u);
	EXPECT_EQ(Action(held).states.at(2), 163u);
}

TEST_F(VillagerCoreTest, KnockedVillagerNotStarted)
{
	// SetTopState(FLYING) refused (its entry function says 0): not started, the body is not woken
	Table().rows[10].entry = 0;
	const auto refused = MakeVillager(163);
	EXPECT_FALSE(Knock(refused));
	EXPECT_EQ(Top(refused), 163u);
	// carried by a particle system: ProcessTurn does not call InitialisePhysics at all
	Table().rows[10].entry = 1;
	const auto carried = MakeVillager(1, 36);
	// carried as the tornado's take makes it: the carried mark and in the physics with no body
	Locator::entitiesRegistry::value().Assign<CarriedByTornado>(carried);
	Locator::entitiesRegistry::value().AssignOrReplaceState<InPhysics>(carried);
	EXPECT_FALSE(Knock(carried));
	EXPECT_EQ(Top(carried), 1u);
	EXPECT_EQ(Action(carried).states.at(2), 0u);
}

// ---- picked up or thrown: the old state's exit ------------------------------------------------------------------

namespace
{
/// A villager running from a thing (6 FLEEING_FROM_OBJECT_REACTION, final and reactive as in info.dat), following a
/// slot reaction; the row's exit is the shared reaction exit, as in the game's table
entt::entity MakeFleeingVillager(FakeStateTable& table, entt::entity entity)
{
	auto& rows = const_cast<InfoConstants&>(Locator::infoConstants::value()).villagerStateTable;
	rows.at(6).isFinalState = 1;
	rows.at(6).isReactionState = 1;
	table.rows[6].exitFunction = ecs::villager_reactions::ExitReaction;
	Locator::entitiesRegistry::value().Assign<LivingReaction>(
	    entity, LivingReaction {5, openblack::Reaction::ReactToFood, entt::null, 1});
	return entity;
}
} // namespace

TEST_F(VillagerCoreTest, PickedUpVillagerLeavesItsReaction)
{
	Table().setsTopStates = true;
	const auto held = MakeFleeingVillager(Table(), MakeVillager(6));
	ASSERT_TRUE(ecs::villager_reactions::IsReacting(held));
	// IN_HAND through 6's exit, which ends the reaction (24 is not a reactive state); no pause roll
	ecs::villager_physics::InterfaceSetInMagicHand(held);
	EXPECT_EQ(Top(held), 24u);
	EXPECT_EQ(Table().Count("set with exit 24"), 1u);
	EXPECT_EQ(Table().Count("exit 6 24"), 1u);
	EXPECT_FALSE(ecs::villager_reactions::IsReacting(held));
	EXPECT_FALSE(Locator::entitiesRegistry::value().AllOf<LivingReaction>(held));
	EXPECT_TRUE(_draws.empty());
}

TEST_F(VillagerCoreTest, ThrownVillagerLeavesItsReaction)
{
	Table().setsTopStates = true;
	const auto thrown = MakeFleeingVillager(Table(), MakeVillager(6));
	// the start of a flight from the hand's throw (the testbed's throws take a villager the hand does not hold): FLYING
	// through 6's exit, which ends the reaction (10 is not a reactive state)
	ecs::PhysicsEntry po;
	po.entity = thrown;
	ecs::villager_physics::VillagerStartPhysics(thrown, po, true);
	EXPECT_EQ(Top(thrown), 10u);
	EXPECT_EQ(Table().Count("exit 6 10"), 1u);
	EXPECT_FALSE(ecs::villager_reactions::IsReacting(thrown));
	EXPECT_FALSE(Locator::entitiesRegistry::value().AllOf<LivingReaction>(thrown));
	EXPECT_TRUE(_draws.empty());
}

TEST_F(VillagerCoreTest, PickedUpAndThrownVillagerWithNoExitIsUnchanged)
{
	Table().setsTopStates = true;
	// 163 DECIDE_WHAT_TO_DO has no exit function: IN_HAND as before, 163 kept as PREVIOUS (StorePreviousState first),
	// no reaction made or lost
	const auto deciding = MakeVillager(163);
	Action(deciding).states.at(2) = 36;
	ecs::villager_physics::InterfaceSetInMagicHand(deciding);
	EXPECT_EQ(Top(deciding), 24u);
	EXPECT_EQ(Final(deciding), 0u);
	EXPECT_EQ(Action(deciding).states.at(2), 163u);
	EXPECT_EQ(Table().Count("exit 163 24"), 1u);
	EXPECT_FALSE(ecs::villager_reactions::IsReacting(deciding));
	// picked up again while held: nothing asked
	Table().calls.clear();
	ecs::villager_physics::InterfaceSetInMagicHand(deciding);
	EXPECT_TRUE(Table().calls.empty());
	// thrown from the hand: FLYING through IN_HAND's exit; once flying, a start of physics asks nothing more
	ecs::PhysicsEntry po;
	po.entity = deciding;
	ecs::villager_physics::VillagerStartPhysics(deciding, po, true);
	EXPECT_EQ(Top(deciding), 10u);
	EXPECT_EQ(Table().Count("exit 24 10"), 1u);
	Table().calls.clear();
	ecs::villager_physics::VillagerStartPhysics(deciding, po, true);
	EXPECT_EQ(Top(deciding), 10u);
	EXPECT_TRUE(Table().calls.empty());
	EXPECT_EQ(Action(deciding).states.at(2), 163u);
	EXPECT_TRUE(_draws.empty());
}

TEST_F(VillagerCoreTest, VillagerThrownInTheWorldKeepsItsStateAsPrevious)
{
	// walking (TOP 1, FINAL 36): thrown by a script or a creature, not the hand, the Living's start (before the body)
	// stores the final state as PREVIOUS, then FLYING takes
	const auto walking = MakeVillager(1, 36);
	EXPECT_TRUE(ecs::villager_physics::VillagerStartPhysicsInWorld(walking, glm::vec3(0.0f), glm::vec3(0.0f)));
	EXPECT_EQ(Top(walking), 10u);
	EXPECT_EQ(Action(walking).states.at(2), 36u);
	// already flying: taken, nothing more is stored
	Action(walking).states.at(2) = 163;
	EXPECT_TRUE(ecs::villager_physics::VillagerStartPhysicsInWorld(walking, glm::vec3(0.0f), glm::vec3(0.0f)));
	EXPECT_EQ(Action(walking).states.at(2), 163u);
	// held (IN_HAND, made storable here so a store would write 24): PREVIOUS is kept
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->villagerStateTable.at(24).keepsPreviousState = 0;
	Locator::infoConstants::reset(info.release());
	const auto held = MakeVillager(24);
	Action(held).states.at(2) = 163;
	EXPECT_TRUE(ecs::villager_physics::VillagerStartPhysicsInWorld(held, glm::vec3(0.0f), glm::vec3(0.0f)));
	EXPECT_EQ(Top(held), 10u);
	EXPECT_EQ(Action(held).states.at(2), 163u);
}

TEST_F(VillagerCoreTest, VillagerThrownInTheWorldRefusedStartsNoPhysics)
{
	// SetTopState(FLYING) refused (its entry function says 0): the start is refused before any body is made
	Table().rows[10].entry = 0;
	const auto refused = MakeVillager(163);
	EXPECT_FALSE(ecs::villager_physics::VillagerStartPhysicsInWorld(refused, glm::vec3(0.0f), glm::vec3(0.0f)));
	EXPECT_EQ(Top(refused), 163u);
}

TEST_F(VillagerCoreTest, VillagerThrownFromAHandKeepsItsStateAsPreviousUnlessHeld)
{
	// not IN_HAND (a creature's hold, or a thing the testbed throws): the final state is stored as PREVIOUS
	const auto walking = MakeVillager(1, 36);
	ecs::villager_physics::VillagerBeforeStartFromHand(walking);
	EXPECT_EQ(Action(walking).states.at(2), 36u);
	EXPECT_EQ(Top(walking), 1u);
	// IN_HAND (made storable here, so a store would write 24): PREVIOUS is kept
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->villagerStateTable.at(24).keepsPreviousState = 0;
	Locator::infoConstants::reset(info.release());
	const auto held = MakeVillager(24);
	Action(held).states.at(2) = 163;
	ecs::villager_physics::VillagerBeforeStartFromHand(held);
	EXPECT_EQ(Action(held).states.at(2), 163u);
}

TEST_F(VillagerCoreTest, AnimalThrownFromAHandKeepsItsStateAsPreviousUnlessHeld)
{
	const auto wandering = MakeAnimal(32, 32, 0);
	ecs::animal_ai::BeforeFlightFromHand(wandering);
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const AnimalBrain>(wandering).previousState, 32u);
	const auto held = MakeAnimal(24, 24, 33);
	ecs::animal_ai::BeforeFlightFromHand(held);
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const AnimalBrain>(held).previousState, 33u);
}

TEST_F(VillagerCoreTest, ADancerThrownFromAHandLeavesItsDance)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto dance = registry.Create();
	auto& d = registry.Assign<Dance>(dance);
	d.groups.all.resize(1);
	const auto villager = MakeVillager(5);
	const auto animal = MakeAnimal(5, 5, 0);
	for (const auto dancer : {villager, animal})
	{
		d.groups.all[0].dancers.push_back(dancer);
		registry.Assign<Dancer>(dancer, Dancer {.dance = dance, .group = 0});
	}
	d.dancers = 2;
	ecs::villager_physics::VillagerBeforeStartFromHand(villager);
	EXPECT_FALSE(registry.AllOf<Dancer>(villager));
	ecs::animal_ai::BeforeFlightFromHand(animal);
	EXPECT_FALSE(registry.AllOf<Dancer>(animal));
	const auto& left = registry.Get<const Dance>(dance);
	EXPECT_TRUE(left.groups.all[0].dancers.empty());
	EXPECT_EQ(left.dancers, 0u);
	// not dancing: nothing to leave
	const auto walking = MakeVillager(1, 36);
	ecs::villager_physics::VillagerBeforeStartFromHand(walking);
	EXPECT_FALSE(registry.AllOf<Dancer>(walking));
}

TEST_F(VillagerCoreTest, PickedUpVillagerOrAnimalKeepsItsStateAsPrevious)
{
	Table().setsTopStates = true;
	// walking (TOP 1, FINAL 36): the pick-up stores the final state as PREVIOUS, then IN_HAND
	const auto walking = MakeVillager(1, 36);
	ecs::villager_physics::InterfaceSetInMagicHand(walking);
	EXPECT_EQ(Top(walking), 24u);
	EXPECT_EQ(Action(walking).states.at(2), 36u);
	// an animal wandering (32): PREVIOUS = 32, then IN_HAND
	const auto wandering = MakeAnimal(32, 32, 0);
	ecs::villager_physics::InterfaceSetInMagicHand(wandering);
	const auto& brain = Locator::entitiesRegistry::value().Get<const AnimalBrain>(wandering);
	EXPECT_EQ(brain.previousState, 32u);
	EXPECT_EQ(brain.topState, static_cast<uint8_t>(ecs::animal_ai::AnimalState::InHand));
}

TEST_F(VillagerCoreTest, KnockedAnimalFlies)
{
	using ecs::animal_ai::AnimalState;
	// wandering (32): StorePreviousState keeps its final state, FLYING -> started
	const auto wandering = MakeAnimal(32, 32, 0);
	EXPECT_TRUE(Knock(wandering));
	const auto& brain = Locator::entitiesRegistry::value().Get<const AnimalBrain>(wandering);
	EXPECT_EQ(brain.topState, static_cast<uint8_t>(AnimalState::Flying));
	EXPECT_EQ(brain.previousState, 32u);
	// IN_HAND (24): PREVIOUS is kept
	const auto held = MakeAnimal(24, 24, 33);
	EXPECT_TRUE(Knock(held));
	const auto& heldBrain = Locator::entitiesRegistry::value().Get<const AnimalBrain>(held);
	EXPECT_EQ(heldBrain.topState, static_cast<uint8_t>(AnimalState::Flying));
	EXPECT_EQ(heldBrain.previousState, 33u);
}

TEST_F(VillagerCoreTest, OutOfClipIntoAStateWithNoIntoClip)
{
	// the out-of clip is over and the new state's into function gives no clip (the arrive-at-resource clip choice on
	// entry): the state's own clip starts and the wait ends on that turn. No
	// resources here: every clip lasts 0 ms, so the out-of clip is over on the first turn
	auto& table = const_cast<InfoConstants&>(Locator::infoConstants::value()).villagerStateTable;
	// info.dat: 20 ARRIVES_AT_FOOD_REACTION allows an out-of clip, its clip is 385 P_STAND
	table.at(20).field0xf0 = 0;
	const auto a = MakeVillager(22);
	const auto anim = [a]() -> const SkeletalAnimation& {
		return Locator::entitiesRegistry::value().Get<const SkeletalAnimation>(a);
	};
	// leaving 22 ARRIVES_AT_WOOD_REACTION for 20: the out-of clip 340 PICK_UP_STICKS, flags 0x800 and 0x1000
	EXPECT_EQ(ecs::VillagerCallOutOfAnimation(a, S(20)), 340);
	Action(a).states.at(0) = 20;
	ecs::villager_animate::SetAnim(a, 340, true);
	EXPECT_EQ(anim().transitionFlags, 0x1800);
	Table().calls.clear();
	// the turn the out-of clip ends: the state's clip, the wait flag clear, the state logic still skipped
	EXPECT_EQ(villager::ProcessState(a, 100), 1u);
	EXPECT_EQ(anim().clipIndex, 385);
	EXPECT_EQ(anim().transitionFlags, 0x1000);
	EXPECT_EQ(Action(a).turnsSinceStateChange, 0);
	EXPECT_EQ(Table().Count("state 20"), 0u);
	// the next turn runs the state
	villager::ProcessState(a, 101);
	EXPECT_EQ(Table().Count("state 20"), 1u);
	EXPECT_EQ(anim().clipIndex, 385);
}

TEST_F(VillagerCoreTest, AVillagerIsMadeWithItsBornStatesClip)
{
	// info.dat: 85 CREATED plays 385 P_STAND, 16 DROWNING 252 P_DROWNING, both straight from the table
	auto& table = const_cast<InfoConstants&>(Locator::infoConstants::value()).villagerStateTable;
	table.at(85).animation = 385;
	table.at(16).animation = 252;
	const auto& info = Locator::infoConstants::value().villager.at(0);
	auto& registry = Locator::entitiesRegistry::value();
	const auto onLand = MakeVillager(0);
	const auto inWater = MakeVillager(0);
	SetDraws({0, 0, 0, 0, 0, 0}, {0.2f, 0.6f, 0.2f, 0.6f});
	villager::Construct(onLand, info, 25, 100, false);
	villager::Construct(inWater, info, 25, 100, true);
	ASSERT_EQ(Top(onLand), 85u);
	ASSERT_EQ(Top(inWater), 16u);
	EXPECT_FALSE(registry.AllOf<SkeletalAnimation>(onLand));
	SetDraws({}, {});
	ecs::villager_animate::SetFirstClip(onLand);
	ecs::villager_animate::SetFirstClip(inWater);
	// the clip component and the clip from the start, with nothing drawn
	EXPECT_TRUE(_draws.empty());
	const auto& standing = registry.Get<const SkeletalAnimation>(onLand);
	EXPECT_TRUE(standing.hasClip);
	EXPECT_EQ(standing.clipIndex, 385);
	EXPECT_EQ(standing.time, 0.0f);
	EXPECT_EQ(ecs::villager_animate::CurrentClip(inWater), 252);
	EXPECT_EQ(registry.Get<const SkeletalAnimation>(inWater).time, 0.0f);
	// a clip it already plays is left where it is
	registry.Get<SkeletalAnimation>(onLand).time = 0.5f;
	ecs::villager_animate::SetFirstClip(onLand);
	EXPECT_EQ(registry.Get<const SkeletalAnimation>(onLand).time, 0.5f);
	// a born state with no clip of its own sets none
	table.at(85).animation = static_cast<uint32_t>(-4);
	const auto noClip = MakeVillager(85);
	ecs::villager_animate::SetFirstClip(noClip);
	EXPECT_TRUE(registry.AllOf<SkeletalAnimation>(noClip));
	EXPECT_EQ(ecs::villager_animate::CurrentClip(noClip), -1);
}

TEST_F(VillagerCoreTest, AScriptStateInTheCreatingTurnPlaysItsClip)
{
	// SET_SCRIPT_STATE right after the villager is made: its SetAnim(1) finds the first clip and puts on the new
	// state's; without a first clip nothing would be set until the frame
	auto& table = const_cast<InfoConstants&>(Locator::infoConstants::value()).villagerStateTable;
	table.at(85).animation = 385;
	table.at(4).animation = 389;
	auto& registry = Locator::entitiesRegistry::value();
	const auto made = MakeWalker(85, glm::vec2(0.0f), 0.1f);
	ecs::villager_animate::SetFirstClip(made);
	registry.Get<SkeletalAnimation>(made).time = 0.25f;
	villager_script::SetScriptState(made, VillagerStates::InScript);
	ASSERT_EQ(Top(made), 4u);
	EXPECT_EQ(ecs::villager_animate::CurrentClip(made), 389);
	EXPECT_EQ(registry.Get<const SkeletalAnimation>(made).time, 0.0f);
	// with no clip component the same call sets none
	const auto bare = MakeWalker(85, glm::vec2(0.0f), 0.1f);
	villager_script::SetScriptState(bare, VillagerStates::InScript);
	EXPECT_FALSE(registry.AllOf<SkeletalAnimation>(bare));
}

namespace
{
/// A trace argument that counts how often it is formatted
struct CountedArgument
{
	int* formatted;
};
} // namespace

template <>
struct fmt::formatter<CountedArgument>: fmt::formatter<int>
{
	auto format(const CountedArgument& argument, fmt::format_context& context) const
	{
		++*argument.formatted;
		return fmt::formatter<int>::format(*argument.formatted, context);
	}
};

TEST(VillagerTrace, NothingIsFormattedForAVillagerNobodyTraces)
{
	if (std::getenv("OPENBLACK_VILLAGER_TRACE") != nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_VILLAGER_TRACE is set";
	}
	int evaluated = 0;
	int formatted = 0;
	const auto argument = [&] {
		++evaluated;
		return CountedArgument {&formatted};
	};
	villager::TraceFormatted(entt::entity {1}, "state {} -> {}", argument(), 3);
	// the arguments are evaluated as they were in the fmt::format call; the text is not made
	EXPECT_EQ(evaluated, 1);
	EXPECT_EQ(formatted, 0);
	// the same argument formats once when the text is made
	EXPECT_EQ(fmt::format("{}", CountedArgument {&formatted}), "1");
}
