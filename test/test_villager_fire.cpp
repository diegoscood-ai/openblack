/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The villagers and fire (docs/wiki/world/fire.md#villagers-and-fire): a villager the heat reaches runs (ON_FIRE), the
// fire's reaction (its priority and how far it reaches), a worshipper on its way going round a fire instead of fighting
// it, and the water states giving up. The fixture of test_villager_reactions.cpp (a fake state table in the Locator, the
// turn and the draws scripted), with the REACT_TO_FIRE row of info.dat. The fires are put on fields by hand, at a
// temperature: a field's radius is fixed and its info row is the fields' one, so the fire's sizes need no mesh.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "3D/AxisAlignedBoundingBox.h"
#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerFire.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerStores.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/Implementations/VillagerWorshipCheck.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "Enums.h"
#include "Fire/FireModel.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/LivingActionFakes.h"
#include "support/MapFakes.h"
#include "support/TestServices.h"
#include "support/VillagerFakes.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager = openblack::ecs::villager;
namespace villager_fire = openblack::ecs::villager_fire;
namespace vr = openblack::ecs::villager_reactions;
namespace reactions = openblack::ecs::effects::reactions;
namespace fire_effect = openblack::ecs::fire;
namespace model = openblack::fire;
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
	[[nodiscard]] bool VillagerIsReadyForNewAnimation(const LivingAction& /*action*/, uint32_t /*times*/) const override
	{
		return false;
	}
};

constexpr auto S(uint32_t n)
{
	return static_cast<VillagerStates>(n);
}

// The REACT_TO_FIRE row of info.dat (reaction 10): priority 210, 100000 turns to react and none before reacting again,
// finishes when its initiator is picked up, reaches 35 m, the distance does not matter to its score, stealth 5, runs
// away 10 to 10 m
constexpr uint32_t k_FirePriority = 210;
constexpr float k_FireReach = 35.0f;
constexpr float k_RunAway = 10.0f;
// The fields' combustion temperature in these tests: a fire's share of its fiercest goes from 0.8 of it (160) to
// twice it (400)
constexpr float k_FieldCombustion = 200.0f;
// A villager's own fire: far above anything's combustion temperature, and below the lowest any thing has (40)
constexpr float k_Burning = 5000.0f;
constexpr float k_NotBurning = 30.0f;
// The villagers' mesh is a box 1 m wide: their radius is 0.5 m
constexpr float k_VillagerHalfWidth = 0.5f;

class VillagerFire: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		test::FakeMeshBoxProvider boxes;
		boxes.meshBox = [](entt::id_type /*id*/) -> std::optional<AxisAlignedBoundingBox> {
			return AxisAlignedBoundingBox {{-k_VillagerHalfWidth, 0.0f, -k_VillagerHalfWidth},
			                               {k_VillagerHalfWidth, 1.8f, k_VillagerHalfWidth}};
		};
		Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>(boxes);
		auto info = std::make_unique<InfoConstants>();
		auto& v = info->villager.at(0);
		v.processChecksEvery = 8;
		v.pauseForASecondChance = 0.01f;
		v.grownUpAge = 13;
		v.life = 1.0f;
		v.moveState = LivingStates::LivingMoveToPos;
		v.speedGroup.speedDefault = static_cast<SpeedState>(100);
		v.lifeWhenCrawlsWounded = 0.15f;
		auto& r = info->reaction.at(static_cast<size_t>(Reaction::ReactToFire));
		r.priority = k_FirePriority;
		r.numGameTurnsForNormalThingsToReact = 100000;
		r.numGameTurnsForNormalThingsBeforeReactingAgain = 0;
		r.numGameTurnsForCreatureToReact = 100000;
		r.numGameTurnsForCreatureBeforeReactingAgain = 0;
		r.whetherItImpresses = 0;
		r.whetherReactionFinishesIfInitiatorInHand = 1;
		r.maxReactionDistance = k_FireReach;
		r.howImportantIsDistance = 0.0f;
		r.stealthRandomChance = 5;
		r.minDistanceToRunAwayFromObject = k_RunAway;
		r.maxDistanceToRunAwayFromObject = k_RunAway;
		auto& field = info->fieldType.at(0);
		field.combustionTemperature = k_FieldCombustion;
		field.burningPriority = 1.0f;
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1;
		}
		auto& table = info->villagerStateTable;
		for (const uint32_t s : {163u, 215u})
		{
			table.at(s).isFinalState = 1;
		}
		table.at(163).availableForReaction = 1;
		table.at(163).field0xa8 = 1; // available
		table.at(163).resumeState = 163;
		table.at(215).field0xa8 = 4; // not available
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::rng::emplace<TestRng>();
		Locator::livingActionSystem::emplace<FakeStateTable>();
		reactions::Clear();
		vr::Register();
		game_clock::SetTurn(1000);
		Locator::villagerRules::emplace<ecs::systems::VillagerRules>();
		Locator::villagerWorldQueries::emplace<ecs::systems::VillagerWorldQueries>();
		Locator::villagerWorshipCheck::emplace<ecs::systems::VillagerWorshipCheck>();
		Locator::villagerChildFactory::emplace<ecs::systems::VillagerChildFactory>();
		Locator::villagerDiscipleJobs::emplace<ecs::systems::VillagerDiscipleJobs>();
		// GameFloatRand(x) gives the answer set for x (0 for any other), and every x asked is kept
		game_random::testing::SetGameRand([](uint32_t) { return 0u; },
		                                  [this](float x) {
			                                  _floatAsked.push_back(x);
			                                  for (const auto& [asked, answer] : _floatAnswers)
			                                  {
				                                  if (std::abs(asked - x) < 1e-4f)
				                                  {
					                                  return answer;
				                                  }
			                                  }
			                                  return 0.0f;
		                                  });
		test::FakeVillagerStores stores(std::make_shared<ecs::systems::VillagerStores>());
		stores.temporaryStore = [](entt::entity, const MapCoords& from, ResourceType) {
			return ecs::town_stores::TemporaryStore {entt::null, from};
		};
		Locator::villagerStores::emplace<test::FakeVillagerStores>(stores);
	}

	void TearDown() override
	{
		villager_fire::Clear();
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

	std::vector<std::pair<float, float>> _floatAnswers;
	std::vector<float> _floatAsked;

	/// Whether GameFloatRand was asked for x
	[[nodiscard]] bool Asked(float x) const
	{
		for (const float asked : _floatAsked)
		{
			if (std::abs(asked - x) < 1e-4f)
			{
				return true;
			}
		}
		return false;
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity MakeVillager(glm::vec2 at, uint32_t top = 163)
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
		registry.Assign<Mesh>(e, static_cast<entt::id_type>(1), static_cast<int8_t>(0), static_cast<int8_t>(0));
		return e;
	}

	static entt::entity MakeTown(glm::vec2 at)
	{
		auto& registry = Reg();
		const auto town = registry.Create();
		registry.Assign<Town>(town, 1u);
		registry.Assign<Tribe>(town, Tribe::CELTIC);
		registry.Assign<Transform>(town, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Context().towns[1] = town;
		return town;
	}

	/// A field (radius 5 m, the fields' info row)
	static entt::entity MakeField(glm::vec2 at)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		registry.Assign<Transform>(e, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		auto& field = registry.Assign<Field>(e);
		field.town = -1;
		return e;
	}

	/// The object has a fire at a temperature
	static fire_effect::FireEffect& Burn(entt::entity object, float temperature)
	{
		auto& fires = Locator::fireSystem::value();
		auto made = std::make_unique<fire_effect::FireEffect>();
		made->id = fires.TakeId();
		made->object = object;
		made->temperature = temperature;
		made->previous = temperature;
		return fires.Insert(std::move(made));
	}

	/// LookAtPos until it faces `at`
	static void Face(entt::entity e, entt::entity at)
	{
		for (int i = 0; i < 32 && villager::LookAtPos(e, ecs::town_queries::PosOf(at), 1) == 0; ++i)
		{
		}
	}

	static glm::vec2 Where(entt::entity e)
	{
		const auto& position = Reg().Get<const Transform>(e).position;
		return {position.x, position.z};
	}

	static LivingAction& Action(entt::entity e) { return Reg().Get<LivingAction>(e); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }
	static uint32_t Previous(entt::entity e) { return Action(e).states.at(2); }
	static glm::vec2 Goal(entt::entity e) { return Reg().Get<const WallHug>(e).goal; }
};
} // namespace

// ---- ON_FIRE (219) ----------------------------------------------------------------------------------------------

// A villager whose own fire burns runs about at random until it goes out: GameFloatRand(2 pi) for the direction first,
// then GameFloatRand(6) + 4 metres, from where it is, to ON_FIRE. No state on this path fetches water (217, 218)
TEST_F(VillagerFire, ABurningVillagerRunsFourToTenMetresAtRandom)
{
	const glm::vec2 start {100.0f, 130.0f};
	const auto v = MakeVillager(start);
	Reg().Get<WallHug>(v).goal = {140.0f, 150.0f};
	Burn(v, k_Burning);
	villager_fire::SetupOnFire(v, 0);
	ASSERT_EQ(Top(v), 219u);
	ASSERT_EQ(Previous(v), 163u);
	EXPECT_EQ(villager_fire::FireOf(v), 0u);

	_floatAnswers = {{glm::two_pi<float>(), 1.0f}, {6.0f, 3.0f}};
	EXPECT_EQ(villager_fire::OnFire(Action(v)), 1u);
	// the angle drawn first, then the distance: 1 radian, 3 + 4 = 7 m, x by the cosine and z by the sine
	std::vector<float> fireDraws;
	for (const float x : _floatAsked)
	{
		if (std::abs(x - glm::two_pi<float>()) < 1e-4f || std::abs(x - 6.0f) < 1e-4f)
		{
			fireDraws.push_back(x);
		}
	}
	ASSERT_EQ(fireDraws.size(), 2u);
	EXPECT_FLOAT_EQ(fireDraws.at(0), glm::two_pi<float>());
	EXPECT_FLOAT_EQ(fireDraws.at(1), 6.0f);
	const auto goal = Goal(v);
	EXPECT_NEAR(goal.x, start.x + 7.0f * std::cos(1.0f), 1e-3f);
	EXPECT_NEAR(goal.y, start.y + 7.0f * std::sin(1.0f), 1e-3f);
	// walking there (MOVE_TO_POS) to ON_FIRE, what it was doing still kept
	EXPECT_EQ(Top(v), 1u);
	EXPECT_EQ(Final(v), 219u);
	EXPECT_TRUE(villager_fire::IsRunningOnFire(v));
	EXPECT_EQ(Previous(v), 163u);

	// with nothing kept, DECIDE_WHAT_TO_DO is kept to come back to
	Action(v).states.at(2) = 0;
	EXPECT_EQ(villager_fire::OnFire(Action(v)), 1u);
	EXPECT_EQ(Final(v), 219u);
	EXPECT_EQ(Previous(v), 163u);
}

// Its fire burning no more, it goes back where it was going and to what it was doing
TEST_F(VillagerFire, ItsFireGoingOutSendsItBackToWhatItWasDoing)
{
	const glm::vec2 before {140.0f, 150.0f};
	const auto v = MakeVillager({100.0f, 130.0f});
	Reg().Get<WallHug>(v).goal = before;
	auto& own = Burn(v, k_Burning);
	villager_fire::SetupOnFire(v, 0);
	ASSERT_EQ(villager_fire::OnFire(Action(v)), 1u);
	ASSERT_NE(Goal(v), before);

	own.temperature = k_NotBurning;
	EXPECT_EQ(villager_fire::OnFire(Action(v)), 0u);
	EXPECT_EQ(Top(v), 163u);
	EXPECT_EQ(Previous(v), 0u);
	EXPECT_EQ(Goal(v), before);
	EXPECT_FALSE(villager_fire::IsRunningOnFire(v));

	// with no fire of its own at all, the same
	const auto other = MakeVillager({60.0f, 130.0f});
	Reg().Get<WallHug>(other).goal = before;
	villager_fire::SetupOnFire(other, 0);
	ASSERT_EQ(Top(other), 219u);
	EXPECT_EQ(villager_fire::OnFire(Action(other)), 0u);
	EXPECT_EQ(Top(other), 163u);
	EXPECT_EQ(Goal(other), before);
}

// Heated by another thing's fire it runs from that thing: burning, to the fire's largest radius + GameFloatRand(10);
// not burning, only while within that radius, to the fire's radius now + GameFloatRand(largest - now + 1); past it, it
// goes back. Always on the line from the thing to the villager
TEST_F(VillagerFire, AVillagerTheHeatReachesRunsFromTheFire)
{
	const glm::vec2 fieldAt {100.0f, 120.0f};
	const auto field = MakeField(fieldAt);
	auto& blaze = Burn(field, 300.0f);
	const float largest = blaze.MaxFireRadius();
	const float now = blaze.FireRadius();
	ASSERT_GT(largest, now);

	// burning, 10 m away
	const auto burning = MakeVillager({100.0f, 130.0f});
	const glm::vec2 before {140.0f, 150.0f};
	Reg().Get<WallHug>(burning).goal = before;
	Burn(burning, k_Burning);
	villager_fire::SetupOnFire(burning, blaze.id);
	EXPECT_EQ(villager_fire::FireOf(burning), blaze.id);
	EXPECT_EQ(Top(burning), 219u);
	EXPECT_EQ(Previous(burning), 163u);
	_floatAnswers = {{10.0f, 2.0f}};
	EXPECT_EQ(villager_fire::OnFire(Action(burning)), 1u);
	EXPECT_TRUE(Asked(10.0f));
	EXPECT_NEAR(Goal(burning).x, fieldAt.x, 1e-3f);
	EXPECT_NEAR(Goal(burning).y, fieldAt.y + largest + 2.0f, 1e-3f);
	EXPECT_EQ(Final(burning), 219u);

	// not burning, 5 m away: inside the largest radius
	const auto near = MakeVillager({100.0f, 125.0f});
	Burn(near, k_NotBurning);
	villager_fire::SetupOnFire(near, blaze.id);
	const float span = largest - now + 1.0f;
	_floatAnswers = {{span, 1.0f}};
	_floatAsked.clear();
	EXPECT_EQ(villager_fire::OnFire(Action(near)), 1u);
	EXPECT_TRUE(Asked(span));
	EXPECT_NEAR(Goal(near).x, fieldAt.x, 1e-3f);
	EXPECT_NEAR(Goal(near).y, fieldAt.y + now + 1.0f, 1e-3f);

	// not burning, 10 m away: past it, back to what it was doing
	const auto far = MakeVillager({100.0f, 130.0f});
	Reg().Get<WallHug>(far).goal = before;
	Burn(far, k_NotBurning);
	villager_fire::SetupOnFire(far, blaze.id);
	EXPECT_EQ(villager_fire::OnFire(Action(far)), 0u);
	EXPECT_EQ(Top(far), 163u);
	EXPECT_EQ(Goal(far), before);
}

// ---- REACT_TO_FIRE (215) ----------------------------------------------------------------------------------------

// A fire reacts from 100 degrees, or from its combustion temperature when that is lower
TEST_F(VillagerFire, AFireReactsFromAHundredDegrees)
{
	const model::Material hut {.combustion = k_FieldCombustion};
	EXPECT_FALSE(model::IsAboveReactionTemperature(99.99f, hut));
	EXPECT_TRUE(model::IsAboveReactionTemperature(100.0f, hut));
	const model::Material grass {.combustion = 60.0f};
	EXPECT_TRUE(model::IsAboveReactionTemperature(60.0f, grass));
	EXPECT_FALSE(model::IsAboveReactionTemperature(59.99f, grass));
}

// The priority is the row's 210 x (1 + 0.5 x the fire's radius / its largest), at most 255, truncated
TEST_F(VillagerFire, AFiresReactionIsTwoHundredAndTenToTwoHundredAndFiftyFive)
{
	const auto field = MakeField({100.0f, 100.0f});
	auto& blaze = Burn(field, 0.0f);
	const auto id = reactions::CreateReaction(field, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	const auto v = MakeVillager({130.0f, 100.0f});

	// the radius a share of the largest: the temperature from 0.8 to 2 times the combustion temperature
	blaze.temperature = 160.0f; // 0
	EXPECT_EQ(villager_fire::ReactToFirePriority(v, id, 0), 210u);
	blaze.temperature = 220.0f; // a quarter: 210 x 1.125 = 236.25
	EXPECT_EQ(villager_fire::ReactToFirePriority(v, id, 0), 236u);
	blaze.temperature = 400.0f; // all of it: 315
	EXPECT_EQ(villager_fire::ReactToFirePriority(v, id, 0), 255u);

	// a villager running on fire does not react
	Action(v).states.at(0) = 1;
	Action(v).states.at(1) = 219;
	EXPECT_EQ(villager_fire::ReactToFirePriority(v, id, 0), 0u);
}

// A villager fighting a fire of the same blaze does not react to it; one fighting a fire up to 2 x (10 + 35) = 90 m
// away takes the new fire into its blaze instead; farther off it reacts
TEST_F(VillagerFire, AFiremanTakesANearFireIntoItsBlaze)
{
	const auto field = MakeField({100.0f, 100.0f});
	auto& blaze = Burn(field, 220.0f);
	const auto id = reactions::CreateReaction(field, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	const auto near = MakeField({180.0f, 100.0f});
	auto& nearBlaze = Burn(near, 220.0f);
	const auto v = MakeVillager({130.0f, 100.0f});
	// beating the fire 80 m away
	Action(v).states.at(0) = 1;
	Action(v).states.at(1) = 216;
	Reg().AssignState<VillagerFireState>(v).fire = nearBlaze.id;
	ASSERT_EQ(blaze.root, &blaze);
	EXPECT_EQ(villager_fire::ReactToFirePriority(v, id, 0), 0u);
	EXPECT_EQ(blaze.root, &nearBlaze);
	// now of its blaze: nothing new
	EXPECT_EQ(villager_fire::ReactToFirePriority(v, id, 0), 0u);

	// fighting one 100 m away: it reacts at the fire's priority
	const auto far = MakeField({200.0f, 100.0f});
	auto& farBlaze = Burn(far, 220.0f);
	const auto farId = reactions::CreateReaction(far, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	const auto other = MakeVillager({170.0f, 100.0f});
	Action(other).states.at(0) = 1;
	Action(other).states.at(1) = 216;
	Reg().AssignState<VillagerFireState>(other).fire = blaze.id;
	EXPECT_EQ(villager_fire::ReactToFirePriority(other, farId, 0), 236u);
	EXPECT_EQ(farBlaze.root, &farBlaze);
}

// The reaction reaches a villager whose distance, half the x and z distances added, is at most 35 m; it then reacts.
// The fire's shut-down sends it back to what it was doing
TEST_F(VillagerFire, OnlyVillagersWithinThirtyFiveMetresReact)
{
	const auto field = MakeField({100.0f, 100.0f});
	Burn(field, 220.0f);
	const auto id = reactions::CreateReaction(field, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	const auto reaction = *reactions::Find(id);
	const auto atEdge = MakeVillager({170.0f, 100.0f});   // 0.5 x 70 = 35
	const auto diagonal = MakeVillager({135.0f, 135.0f}); // 0.5 x (35 + 35) = 35, 49.5 m in a straight line
	const auto beyond = MakeVillager({171.0f, 100.0f});   // 35.5
	for (const auto v : {atEdge, diagonal, beyond})
	{
		villager_fire::ApplyReaction(v, reaction);
	}
	for (const auto v : {atEdge, diagonal})
	{
		EXPECT_TRUE(villager_fire::IsReacting(v));
		EXPECT_EQ(villager_fire::ReactionObject(v), field);
		EXPECT_EQ(Top(v), 215u);
		EXPECT_EQ(Previous(v), 163u);
	}
	EXPECT_FALSE(villager_fire::IsReacting(beyond));
	EXPECT_EQ(Top(beyond), 163u);

	villager_fire::ShutDownReaction(id);
	for (const auto v : {atEdge, diagonal})
	{
		EXPECT_FALSE(villager_fire::IsReacting(v));
		EXPECT_EQ(Top(v), 163u);
	}
}

// A villager of a town may go to beat the fire; one on its way to worship never does, it goes round the fire towards
// where it was going
TEST_F(VillagerFire, AWorshipperGoesRoundAFireInsteadOfFightingIt)
{
	const auto town = MakeTown({50.0f, 50.0f});
	const auto field = MakeField({80.0f, 50.0f});
	Burn(field, 300.0f);
	const auto id = reactions::CreateReaction(field, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);

	const auto fighter = MakeVillager({60.0f, 50.0f});
	const auto worshipper = MakeVillager({60.0f, 52.0f});
	Reg().Assign<WorshipVillager>(worshipper).onWay = true;
	for (const auto v : {fighter, worshipper})
	{
		Reg().Get<Villager>(v).town = town;
		Face(v, field);
	}
	// the fighter reacts, then goes round the fire to beat it
	villager_fire::SetupReactToFire(fighter, field, id);
	ASSERT_EQ(Top(fighter), 215u);
	ASSERT_EQ(Previous(fighter), 163u);
	EXPECT_EQ(villager_fire::ReactToFire(Action(fighter)), 1u);
	EXPECT_EQ(Top(fighter), 220u);
	EXPECT_EQ(Previous(fighter), 216u);
	// the worshipper goes round the fire at once, and on to what it was doing
	villager_fire::SetupReactToFire(worshipper, field, id);
	EXPECT_EQ(Top(worshipper), 220u);
	EXPECT_EQ(Previous(worshipper), 163u);
}

// ---- the water states (217, 218) ------------------------------------------------------------------------------

TEST_F(VillagerFire, TheWaterStatesGiveUpAtOnce)
{
	for (const uint32_t state : {217u, 218u})
	{
		const auto v = MakeVillager({100.0f, 130.0f}, state);
		EXPECT_EQ(villager_fire::PutOutFireWithWater(Action(v)), 1u) << state;
		EXPECT_EQ(Top(v), 163u) << state;
	}
}
