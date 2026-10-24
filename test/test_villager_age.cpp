/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The villager's age (docs/wiki/beings/villager-life.md#home-food-sleep-homeless-and-age): the child that grows up (234, the
// counts, 18 years), the rescale turns, the old age's r^3, UpdatePregnancy and the scale for the age with the scripted
// GameFloatRand. Then the pure villager_age rules (ECS/VillagerAge.h) on made-up villagers, with raffclar's cases at the
// original's numbers.

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <bit>
#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/Implementations/VillagerWorshipCheck.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/VillagerAge.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/LivingActionFakes.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager = openblack::ecs::villager;
namespace villager_age = openblack::ecs::villager_age;
namespace av = openblack::ecs::abode_villagers;
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

class FakeStateTable final: public test::LivingActionFakeBase
{
public:
	void Update() override {}
	[[nodiscard]] VillagerStates VillagerGetState(const LivingAction& action, Index index) const override
	{
		return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	}
	[[nodiscard]] VillagerStates VillagerGetFinalState(const LivingAction& /*action*/) const override
	{
		return VillagerStates::InvalidState;
	}
	void VillagerSetState(LivingAction&, Index, VillagerStates, bool) const override {}
	uint32_t VillagerCallState(LivingAction&, Index) const override { return 0; }
	uint32_t VillagerCallEntryState(LivingAction&, VillagerStates, VillagerStates, VillagerStates) const override { return 1; }
	uint32_t VillagerCallExitState(LivingAction&, VillagerStates, VillagerStates) const override { return 1; }
	int VillagerCallOutOfAnimation(LivingAction&, Index) const override { return -1; }
	bool VillagerCallValidate(LivingAction&, Index) const override { return false; }
};

constexpr auto S(uint32_t n)
{
	return static_cast<VillagerStates>(n);
}

class VillagerAgeTest: public ::testing::Test
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
		v.grownUpAge = 13;
		v.oldAge = 60;
		v.retirementAge = 100;
		v.sex = SexType::Female;
		v.life = 1.0f;
		v.dancingSpeed = 3;
		v.moveState = LivingStates::LivingMoveToPos;
		for (size_t i = 0; i < v.ageToScale.values.size(); ++i)
		{
			v.ageToScale.values.at(i) = 0.3f + 0.05f * static_cast<float>(i);
		}
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1;
		}
		info->villagerStateTable.at(163).isFinalState = 1;
		info->villagerStateTable.at(234).isFinalState = 1;
		auto& abode = info->abode.at(0);
		abode.abodeNumber = AbodeNumber::A;
		abode.tribeType = Tribe::CELTIC;
		abode.maxVillagersInAbode = 4;
		abode.maxChildrenInAbode = 4;
		abode.percentTooCrowded = 0.9f;
		abode.thresholdForStopBeingFunctional = 0.75f;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::rng::emplace<TestRng>();
		Locator::livingActionSystem::emplace<FakeStateTable>();
		game_clock::SetTurn(30001);
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
			    const auto r = _ints.front();
			    _ints.pop_front();
			    return r;
		    },
		    [this](float x) {
			    _draws.push_back("F" + std::to_string(x).substr(0, 4));
			    if (_floats.empty())
			    {
				    return 0.0f;
			    }
			    const auto r = _floats.front();
			    _floats.pop_front();
			    return r;
		    });
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	static const GVillagerInfo& Info() { return Locator::infoConstants::value().villager.at(0); }

	static entt::entity MakeVillager(uint32_t age, bool child, uint32_t top = 163)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		auto& v = registry.Assign<Villager>(e);
		v.life = 1.0f;
		v.town = entt::null;
		v.abode = entt::null;
		v.food = 1.0f;
		v.lastCheckTurn = 30001;
		v.birthTurn = villager_age::BirthTurnFor(30001, age);
		v.flags = child ? Villager::k_FlagChild : 0;
		registry.Assign<LivingAction>(e, S(top), static_cast<uint16_t>(0));
		registry.Assign<Transform>(e, glm::vec3(60.0f, 0.0f, 60.0f), glm::mat3(1.0f), glm::vec3(0.5f));
		registry.Assign<WallHug>(e, glm::vec2(), glm::vec2(), 0.0f, 0.1f);
		registry.Assign<Mobile>(e);
		return e;
	}

	static entt::entity MakeTown()
	{
		auto& registry = Reg();
		const auto town = registry.Create();
		registry.Assign<Town>(town, 1u);
		registry.Assign<Tribe>(town, Tribe::CELTIC);
		registry.Assign<Transform>(town, glm::vec3(50.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Context().towns[1] = town;
		return town;
	}

	static entt::entity MakeAbode()
	{
		auto& registry = Reg();
		const auto abode = registry.Create();
		ecs::object_index::Assign(abode);
		registry.Assign<Abode>(abode, AbodeNumber::A, 1u, 0u, 0u);
		registry.Assign<Transform>(abode, glm::vec3(60.0f, 0.0f, 60.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Life>(abode, 1.0f);
		return abode;
	}

	static Villager& V(entt::entity e) { return Reg().Get<Villager>(e); }
	static uint32_t Top(entt::entity e) { return Reg().Get<LivingAction>(e).states.at(0); }
	static float Scale(entt::entity e) { return Reg().Get<Transform>(e).scale.x; }

	std::deque<uint32_t> _ints;
	std::deque<float> _floats;
	std::vector<std::string> _draws;
};
} // namespace

TEST_F(VillagerAgeTest, PureLayer)
{
	EXPECT_TRUE(villager_age::IsChildAge(12, Info().grownUpAge));
	EXPECT_FALSE(villager_age::IsChildAge(13, Info().grownUpAge));
	EXPECT_EQ(villager_age::GivenAge(Info().grownUpAge, Info().grownUpAge), 18u);
	EXPECT_TRUE(villager_age::IsGrowthTurn(375 * 80));
	EXPECT_FALSE(villager_age::IsGrowthTurn(375 * 80 + 9));
	// r = 0.9 -> n 29; r = 65534 / 65535 -> 39 (nobody dies of old age before 63)
	const villager_age::Ages ages {.grownUp = 13, .old = 60, .oldest = 100};
	const auto rangeFor = [&ages](float r) {
		uint32_t range = 0;
		static_cast<void>(villager_age::DiesOfOldAge(
		    61, ages, [r](float) { return r; },
		    [&range](uint32_t n) {
			    range = n;
			    return 0u;
		    }));
		return range;
	};
	EXPECT_EQ(rangeFor(0.9f), 29u);
	EXPECT_EQ(rangeFor(65534.0f / 65535.0f), 39u);
	const auto dies = [&ages](uint32_t age, uint32_t d) {
		return villager_age::DiesOfOldAge(
		    age, ages, [](float) { return 0.9f; }, [d](uint32_t) { return d; });
	};
	EXPECT_FALSE(dies(61, 38));
	EXPECT_FALSE(dies(62, 38));
	EXPECT_TRUE(dies(63, 38));
	EXPECT_FALSE(dies(70, 28));
	EXPECT_TRUE(dies(75, 26));
	// InitialiseScale: a child ageToScale[age - 1], at 0 the dword before the table (dancingSpeed) as a float; an adult 0.9
	EXPECT_FLOAT_EQ(villager::InitialScaleForAge(Info(), 5), Info().ageToScale.values.at(4));
	EXPECT_EQ(villager::InitialScaleForAge(Info(), 0), std::bit_cast<float>(3u));
	EXPECT_FLOAT_EQ(villager::InitialScaleForAge(Info(), 30), 0.9f);
}

TEST_F(VillagerAgeTest, ScaleForAge)
{
	// a child of 5 from 0.5: + GameFloatRand((ageToScale[6] - 0.5) x 0.75)
	SetDraws({}, {0.01f});
	const float range = (Info().ageToScale.values.at(6) - 0.5f) * 0.75f;
	EXPECT_FLOAT_EQ(villager::ScaleForAge(Info(), 5, 0.5f), 0.5f + 0.01f);
	ASSERT_EQ(_draws.size(), 1u);
	EXPECT_EQ(_draws.at(0), "F" + std::to_string(range).substr(0, 4));
	// an adult below t: two draws (0.05 - r) + 1
	SetDraws({}, {0.02f, 0.07f});
	EXPECT_FLOAT_EQ(villager::ScaleForAge(Info(), 30, 0.9f), (0.05f - 0.07f) + 1.0f);
	EXPECT_EQ(_draws, std::vector<std::string>({"F0.10", "F0.10"}));
	// an adult already above t: one draw, the scale kept
	SetDraws({}, {0.09f});
	EXPECT_FLOAT_EQ(villager::ScaleForAge(Info(), 30, 1.0f), 1.0f);
	EXPECT_EQ(_draws.size(), 1u);
}

TEST_F(VillagerAgeTest, ChildGrowsUp)
{
	const auto town = MakeTown();
	const auto abode = MakeAbode();
	// 12 years on a turn that is not a rescale turn (30001 % 375 != 0): nothing, no draw
	auto young = MakeVillager(12, true);
	EXPECT_EQ(villager::CheckChildGrownUp(young), 0u);
	EXPECT_TRUE(_draws.empty());
	EXPECT_NE(V(young).flags & Villager::k_FlagChild, 0);
	// on a rescale turn (30000): SetScaleForAge (one draw)
	game_clock::SetTurn(30000);
	auto young2 = MakeVillager(12, true);
	V(young2).birthTurn = villager_age::BirthTurnFor(30000, 12);
	SetDraws({}, {});
	villager::CheckChildGrownUp(young2);
	EXPECT_EQ(_draws.size(), 1u);
	game_clock::SetTurn(30001);
	// 13: flags without 8, the age 18 (birth turn = turn - 27000), the abode's counts, mother 0, TOP 234
	auto c = MakeVillager(13, true);
	av::AddVillagerToAbode(abode, c);
	EXPECT_EQ(Reg().Get<Abode>(abode).childCount, 1);
	V(c).mother = young;
	SetDraws({}, {});
	EXPECT_EQ(villager::CheckChildGrownUp(c), 1u);
	EXPECT_EQ(V(c).flags & Villager::k_FlagChild, 0);
	EXPECT_EQ(V(c).birthTurn, 30001u - 18u * 1500u);
	EXPECT_EQ(villager::GetAge(c), 18u);
	EXPECT_EQ(Reg().Get<Abode>(abode).childCount, 0);
	EXPECT_EQ(Reg().Get<Abode>(abode).adultCount, 1);
	EXPECT_TRUE(V(c).mother == entt::null);
	EXPECT_EQ(Top(c), 234u);
	// the town's adults / children moved (TownStats::ChildToAdult)
	EXPECT_EQ(Reg().Get<Town>(town).stats.adults, 1u);
}

TEST_F(VillagerAgeTest, OldAge)
{
	// 60: never (no draw)
	auto a = MakeVillager(60, false);
	SetDraws({}, {});
	EXPECT_FALSE(villager::CheckDeathFromOldAge(a));
	EXPECT_TRUE(_draws.empty());
	// 63 with r = 65534 / 65535 and d = 38: dies, OLD_AGE
	auto b = MakeVillager(63, false);
	SetDraws({38}, {65534.0f / 65535.0f});
	EXPECT_TRUE(villager::CheckDeathFromOldAge(b));
	EXPECT_EQ(villager::GetDeathReason(b), DeathReason::OldAge);
	// then VillagerDead -> SetDying: SetLife(0) and SetTopState(DYING), which sets the state's speed with no test:
	// life 0 <= lifeWhenCrawlsWounded -> GameFloatRand(0.2)
	EXPECT_EQ(_draws, std::vector<std::string>({"F1.00", "R39", "F0.20"}));
	// 70 with r = 0.9: n 29, d at most 28 -> lives
	auto c = MakeVillager(70, false);
	SetDraws({28}, {0.9f});
	EXPECT_FALSE(villager::CheckDeathFromOldAge(c));
	// 75: d 26 > 25 -> dies
	auto d = MakeVillager(75, false);
	SetDraws({26}, {0.9f});
	EXPECT_TRUE(villager::CheckDeathFromOldAge(d));
}

TEST_F(VillagerAgeTest, UpdatePregnancy)
{
	// pregnant 20, 9 turns since the last check: 11
	auto a = MakeVillager(25, false);
	V(a).pregnancy = 20;
	V(a).lastCheckTurn = 30001 - 9;
	EXPECT_EQ(villager::UpdatePregnancy(a), 0u);
	EXPECT_EQ(V(a).pregnancy, 11);
	// down to 0 or below: HousewifeStartsGivingBirth (VillagerBirth.cpp): the pregnancy ends; 111 with the
	// counter GameRand(98) + 25 (r 0: 25) less HousewifeGivingBirth's first turn; its 1
	V(a).pregnancy = 5;
	EXPECT_EQ(villager::UpdatePregnancy(a), 1u);
	EXPECT_EQ(V(a).pregnancy, 0);
	EXPECT_FALSE(villager::IsPregnant(a));
	EXPECT_EQ(Reg().Get<LivingAction>(a).states.at(0), 111u);
	EXPECT_EQ(Reg().Get<LivingAction>(a).turnsUntilStateChange, 24);
	// not pregnant: nothing
	EXPECT_EQ(villager::UpdatePregnancy(a), 0u);
	EXPECT_EQ(V(a).pregnancy, 0);
}

namespace
{
constexpr std::array<float, 20> k_AgeToScale = {0.41f, 0.45f, 0.5f,  0.54f, 0.58f, 0.63f, 0.67f, 0.71f, 0.76f, 0.8f,
                                                0.84f, 0.89f, 0.93f, 0.97f, 0.97f, 0.97f, 0.97f, 1.15f, 1.19f, 1.19f};
constexpr uint32_t k_GrownUp = 13;
/// What the game finds just before the table, read for a child of age 0
constexpr float k_BeforeTable = 0.37f;

/// Hands out the given draws in turn, recording the limits asked for
struct Draws
{
	std::vector<float> values;
	std::vector<float> limits;
	size_t next {0};
	villager_age::FloatRandom Float()
	{
		return [this](float limit) {
			limits.push_back(limit);
			return values.at(next++);
		};
	}
};
} // namespace

TEST(VillagerAge, AgeCountsWholeYearsOfFifteenHundredTurns)
{
	EXPECT_EQ(villager_age::AgeOf(1499, 0), 0u);
	EXPECT_EQ(villager_age::AgeOf(1500, 0), 1u);
	EXPECT_EQ(villager_age::AgeOf(31500, 1500), 20u);
	// Born before the game began: the turns wrap round
	const auto born = villager_age::BirthTurnFor(100, 30);
	EXPECT_EQ(villager_age::AgeOf(100, born), 30u);
	EXPECT_EQ(villager_age::AgeOf(100 + 1500, born), 31u);
}

TEST(VillagerAge, AdultsAreAtLeastEighteen)
{
	EXPECT_EQ(villager_age::GivenAge(5, k_GrownUp), 5u);
	EXPECT_EQ(villager_age::GivenAge(13, k_GrownUp), 18u);
	EXPECT_EQ(villager_age::GivenAge(40, k_GrownUp), 40u);
	EXPECT_TRUE(villager_age::IsChildAge(12, k_GrownUp));
	EXPECT_FALSE(villager_age::IsChildAge(13, k_GrownUp));
}

TEST(VillagerAge, ChildrenStartAtTheSizeOfTheAgeBeforeTheirsAndAdultsANearlyFullOne)
{
	// The game reads the table one age down: at age 0, the value before the table
	EXPECT_EQ(villager_age::StartScale(0, k_GrownUp, k_AgeToScale, k_BeforeTable), k_BeforeTable);
	EXPECT_EQ(villager_age::StartScale(1, k_GrownUp, k_AgeToScale, k_BeforeTable), 0.41f);
	EXPECT_EQ(villager_age::StartScale(12, k_GrownUp, k_AgeToScale, k_BeforeTable), 0.89f);
	EXPECT_EQ(villager_age::StartScale(30, k_GrownUp, k_AgeToScale, k_BeforeTable), 0.9f);
}

TEST(VillagerAge, AChildGrowsUpToThreeQuartersOfTheWayToTheNextAgesSize)
{
	// The game reads the table one age up: a child of 5 grows towards the size of 6
	Draws draws {.values = {0.03f}};
	const float scale = villager_age::GrownScale(5, k_GrownUp, k_AgeToScale, 0.58f, draws.Float());
	ASSERT_EQ(draws.limits.size(), 1u);
	EXPECT_EQ(draws.limits[0], (0.67f - 0.58f) * 0.75f);
	EXPECT_EQ(scale, 0.58f + 0.03f);
}

TEST(VillagerAge, AnAdultKeepsItsSizeUnlessTheDrawIsBiggerThenDrawsAgain)
{
	// (0.05 - 0.08) + 1 = 0.97 is no bigger than 0.98: it stays
	Draws keep {.values = {0.08f}};
	EXPECT_EQ(villager_age::GrownScale(30, k_GrownUp, k_AgeToScale, 0.98f, keep.Float()), 0.98f);
	EXPECT_EQ(keep.limits.size(), 1u);
	// (0.05 - 0.02) + 1 is bigger than 0.9: a second draw decides
	Draws redraw {.values = {0.02f, 0.09f}};
	const float scale = villager_age::GrownScale(30, k_GrownUp, k_AgeToScale, 0.9f, redraw.Float());
	ASSERT_EQ(redraw.limits.size(), 2u);
	EXPECT_EQ(redraw.limits[1], 0.1f);
	// The draw is taken from 0.05 first and full size added after, as the game does: not the same float as 1.05 - r
	EXPECT_EQ(scale, (0.05f - 0.09f) + 1.0f);
	EXPECT_NE(scale, 1.05f - 0.09f);
}

TEST(VillagerAge, AnAdultSizeThatIsNotANumberDrawsAgain)
{
	Draws redraw {.values = {0.08f, 0.09f}};
	const float scale =
	    villager_age::GrownScale(30, k_GrownUp, k_AgeToScale, std::numeric_limits<float>::quiet_NaN(), redraw.Float());
	EXPECT_EQ(redraw.limits.size(), 2u);
	EXPECT_EQ(scale, (0.05f - 0.09f) + 1.0f);
}

TEST(VillagerAge, ChildrenGrowFourTimesAYear)
{
	EXPECT_TRUE(villager_age::IsGrowthTurn(0));
	EXPECT_FALSE(villager_age::IsGrowthTurn(374));
	EXPECT_TRUE(villager_age::IsGrowthTurn(375));
	EXPECT_TRUE(villager_age::IsGrowthTurn(1500));
}

TEST(VillagerAge, TheOldAreCheckedOnTheirOwnTurnOfEightHundred)
{
	// Its own turn, 10 + 790 = 800, fell within the last 8 turns
	EXPECT_TRUE(villager_age::IsOldAgeCheckDue(790, 10, 8));
	EXPECT_TRUE(villager_age::IsOldAgeCheckDue(797, 10, 8));
	EXPECT_FALSE(villager_age::IsOldAgeCheckDue(798, 10, 8));
}

TEST(VillagerAge, OnlyThePastOldAgeDieOfItAndTheOlderTheLikelier)
{
	const villager_age::Ages ages {.grownUp = 13, .old = 60, .oldest = 100};
	std::vector<uint32_t> intLimits;
	const villager_age::IntRandom extra = [&intLimits](uint32_t limit) {
		intLimits.push_back(limit);
		return limit;
	};
	const villager_age::FloatRandom full = [](float limit) { return limit; };
	EXPECT_FALSE(villager_age::DiesOfOldAge(60, ages, full, extra));
	EXPECT_TRUE(intLimits.empty());
	// The draw at its most adds all forty years: 61 + 40 is past 100
	EXPECT_TRUE(villager_age::DiesOfOldAge(61, ages, full, extra));
	EXPECT_EQ(intLimits.back(), 40u);
	// A half draw cubed is an eighth: five more years, 90 + 5 is not past 100
	const villager_age::FloatRandom half = [](float limit) { return limit * 0.5f; };
	EXPECT_FALSE(villager_age::DiesOfOldAge(90, ages, half, extra));
	EXPECT_EQ(intLimits.back(), 5u);
	EXPECT_TRUE(villager_age::DiesOfOldAge(96, ages, half, extra));
}

TEST(VillagerAge, AScriptMakingAGrownVillagerYoungGivesItTheChildsModel)
{
	// The opening's son is made grown (14) and then given the age 8
	const auto setting = villager_age::SetAge(8, 18, 13);
	EXPECT_EQ(setting.age, 8u);
	EXPECT_TRUE(setting.child);
	ASSERT_TRUE(setting.childModel.has_value());
	EXPECT_TRUE(*setting.childModel);
}

TEST(VillagerAge, AnAgeOnTheSameSideOfGrowingUpKeepsTheModel)
{
	const auto child = villager_age::SetAge(3, 8, 13);
	EXPECT_TRUE(child.child);
	EXPECT_FALSE(child.childModel.has_value());
	// An adult is never younger than the youngest adult's age
	const auto adult = villager_age::SetAge(14, 40, 13);
	EXPECT_EQ(adult.age, 18u);
	EXPECT_FALSE(adult.child);
	EXPECT_FALSE(adult.childModel.has_value());
}

TEST(VillagerAge, AChildGivenAGrownAgeWearsItsAdultModel)
{
	const auto setting = villager_age::SetAge(13, 5, 13);
	EXPECT_EQ(setting.age, 18u);
	EXPECT_FALSE(setting.child);
	ASSERT_TRUE(setting.childModel.has_value());
	EXPECT_FALSE(*setting.childModel);
}

TEST(VillagerAge, AVillagerGivenAnAgeWearsItsKindsModelsForItAtEveryDistance)
{
	const std::array child {1, 2, 3};
	const std::array adult {4, 5, 6};
	EXPECT_EQ(villager_age::ModelsForAge(true, child, adult), child);
	EXPECT_EQ(villager_age::ModelsForAge(false, child, adult), adult);
}

namespace
{
/// Hands out float and whole draws, recording each draw's limit in order
struct DrawStream
{
	std::vector<float> floats;
	std::vector<uint32_t> ints;
	std::vector<std::string> order;
	size_t nextFloat {0};
	size_t nextInt {0};
	villager_age::FloatRandom Float()
	{
		return [this](float limit) {
			order.push_back("f" + std::to_string(limit));
			return floats.at(nextFloat++);
		};
	}
	villager_age::IntRandom Int()
	{
		return [this](uint32_t limit) {
			order.push_back("i" + std::to_string(limit));
			return ints.at(nextInt++);
		};
	}
};
const villager_age::NewbornKind k_Kind {.grownUp = k_GrownUp,
                                        .hungryForFood = 0.5f,
                                        .processChecksEvery = 8,
                                        .ageToScale = k_AgeToScale,
                                        .beforeTable = k_BeforeTable};
} // namespace

TEST(VillagerAge, ANewAdultDrawsItsSizeFoodCheckAndWaitInTheGamesOrder)
{
	DrawStream stream {.floats = {0.02f, 0.08f, 0.3f, 0.45f}, .ints = {5, 3, 41}};
	const auto born = villager_age::MakeNewborn(13, 1000, k_Kind, stream.Float(), stream.Int());
	EXPECT_EQ(born.age, 18u);
	EXPECT_FALSE(born.child);
	// (0.05 - 0.02) + 1 is bigger than 0.9, so the size is a second draw
	EXPECT_EQ(born.scale, (0.05f - 0.08f) + 1.0f);
	// 0.3 + 0.5 is under full, so the food is a second draw
	EXPECT_EQ(born.food, 0.45f + 0.5f);
	// 5 is under the turn, so the check is drawn again
	EXPECT_EQ(born.lastCheckTurn, 997u);
	EXPECT_EQ(born.turnsUntilFirstDecision, 42u);
	const std::vector<std::string> expected = {"f" + std::to_string(0.1f),
	                                           "f" + std::to_string(0.1f),
	                                           "f" + std::to_string(0.6f),
	                                           "f" + std::to_string(0.6f),
	                                           "i8",
	                                           "i8",
	                                           "i500"};
	EXPECT_EQ(stream.order, expected);
}

TEST(VillagerAge, ANewChildIsTheSizeOfTheAgeBeforeItsOwnAndAFullOneDrawsOnce)
{
	DrawStream stream {.floats = {0.0f, 0.55f}, .ints = {5, 0}};
	const auto born = villager_age::MakeNewborn(5, 2, k_Kind, stream.Float(), stream.Int());
	EXPECT_TRUE(born.child);
	EXPECT_EQ(born.age, 5u);
	// The size of 4, grown by nothing towards the size of 6
	EXPECT_EQ(born.scale, 0.58f + 0.0f);
	// 0.55 + 0.5 is full: exactly full, no second draw
	EXPECT_EQ(born.food, 1.0f);
	// The game is younger than the draw: last checked when the game began
	EXPECT_EQ(born.lastCheckTurn, 0u);
	EXPECT_EQ(born.turnsUntilFirstDecision, 1u);
	const std::vector<std::string> expected = {"f" + std::to_string((0.67f - 0.58f) * 0.75f), "f" + std::to_string(0.6f), "i8",
	                                           "i500"};
	EXPECT_EQ(stream.order, expected);
}

TEST(VillagerAge, ANewChildOfNoYearsStartsFromTheValueBeforeTheTable)
{
	DrawStream stream {.floats = {0.01f, 0.55f}, .ints = {5, 0}};
	const auto born = villager_age::MakeNewborn(0, 2, k_Kind, stream.Float(), stream.Int());
	EXPECT_TRUE(born.child);
	EXPECT_EQ(born.scale, k_BeforeTable + 0.01f);
	EXPECT_EQ(stream.order.front(), "f" + std::to_string((0.45f - k_BeforeTable) * 0.75f));
}
