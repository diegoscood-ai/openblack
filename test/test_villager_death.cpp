/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A villager's death (docs/wiki/beings/villager-hand-and-death.md#death):
// the pure layer (the help table, the dying time, the clips, the dead tick, the soul, the alignment, the mourning
// turns), VillagerDead's guards, order and counters, the states 14 / 15, CannotExitState, the mourning states and the
// orphans, with the fixture of test_villager_food.cpp (a fake state table in the Locator and scripted draws). There is
// no landscape here: MapCoords::IsWater is true everywhere (ecs::sea_cells), so DEAD makes no soul in these tests;
// only the landing of a thrown corpse puts dry land under it.
// Last, an effect that kills (effects::ApplyEffect): the kill comes before the crushed reaction, which every living
// thing gets and nothing else does. A villager that burnt to death dies by the fire's player.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "Common/EventManager.h"
#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/AltarOffering.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/DetailMesh.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDeaths.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLastInteraction.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Events/CreatureMimicEvents.h"
#include "ECS/Events/VillagerDeathEvents.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/LastDropped.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerPhysics.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerStores.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/Implementations/VillagerWorshipCheck.h"
#include "ECS/Systems/Implementations/WorldEffects.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerMourning.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerSoul.h"
#include "ECS/VillagerAge.h"
#include "ECS/VillagerDrowning.h"
#include "Enums.h"
#include "Game/GameStats.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Resources/ResourcesInterface.h"
#include "support/LandFakes.h"
#include "support/LivingActionFakes.h"
#include "support/MapFakes.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/VillagerFakes.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager = openblack::ecs::villager;
namespace mourning = openblack::ecs::villager_mourning;
namespace soul = openblack::ecs::villager_soul;
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

/// A state table where each row records its calls and accepts every entry and exit
class FakeStateTable final: public test::LivingActionFakeBase
{
public:
	mutable std::vector<std::string> calls;
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
	uint32_t VillagerCallEntryState(LivingAction&, VillagerStates row, VillagerStates, VillagerStates) const override
	{
		calls.push_back("entry " + std::to_string(static_cast<uint32_t>(row)));
		return 1;
	}
	uint32_t VillagerCallExitState(LivingAction&, VillagerStates row, VillagerStates next) const override
	{
		calls.push_back("exit " + std::to_string(static_cast<uint32_t>(row)) + " " +
		                std::to_string(static_cast<uint32_t>(next)));
		return 1;
	}
	int VillagerCallOutOfAnimation(LivingAction&, Index) const override { return -1; }
	bool VillagerCallValidate(LivingAction&, Index) const override { return false; }
};

constexpr auto S(uint32_t n)
{
	return static_cast<VillagerStates>(n);
}

class VillagerDeathTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		// VillagerDrowning logs to the "game" logger without a null test (as test_water.cpp / test_take_resource.cpp)
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		auto info = std::make_unique<InfoConstants>();
		auto& v = info->villager.at(0);
		v.processChecksEvery = 8;
		v.grownUpAge = 13;
		v.sex = SexType::Male;
		v.life = 1.0f;
		v.moveState = LivingStates::LivingMoveToPos;
		v.speedGroup.speedDefault = static_cast<SpeedState>(100);
		v.dyingTimeWithoutGraveyard = 600;
		v.dyingTimeWithGraveyard = 120;
		v.isReacting.isReactingToDeath = 1;
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1;
		}
		auto& table = info->villagerStateTable;
		for (const uint32_t s : {13u, 14u, 15u, 36u, 131u, 163u, 205u, 206u, 207u, 208u})
		{
			table.at(s).isFinalState = 1;
		}
		table.at(163).field0xa8 = 1;            // IsVillagerAvailable
		table.at(163).availableForReaction = 1; // IsAvailableForReaction
		// the player info's dealthReason
		info->player.dealthReason = {0.0f, -0.85f, -0.9f, -0.75f, -0.5f, -0.1f, -0.9f, -1.0f, -0.6f, 0.0f};
		info->town.populationUnderWhichHelpSpritesWarn = 5;
		auto& death = info->reaction.at(static_cast<size_t>(Reaction::ReactToDeath));
		death.priority = 100;
		death.maxReactionDistance = 60.0f;
		death.maxDistanceToRunAwayFromObject = 4.0f;
		death.whetherReactionFinishesIfInitiatorInHand = 1;
		death.numGameTurnsForNormalThingsBeforeReactingAgain = 200;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::rng::emplace<TestRng>();
		Locator::livingActionSystem::emplace<FakeStateTable>();
		game_clock::SetTurn(1000);
		// The villager services the code under test reaches: the game's, unless faked here
		Locator::villagerRules::emplace<ecs::systems::VillagerRules>();
		Locator::villagerWorldQueries::emplace<ecs::systems::VillagerWorldQueries>();
		Locator::villagerWorshipCheck::emplace<ecs::systems::VillagerWorshipCheck>();
		Locator::villagerChildFactory::emplace<ecs::systems::VillagerChildFactory>();
		Locator::villagerDiscipleJobs::emplace<ecs::systems::VillagerDiscipleJobs>();
		game_random::testing::SetGameRand([this](uint32_t) { return _rand; }, [this](float) { return _floatRand; });
		// A fresh event manager per test: its handlers capture this fixture
		Locator::events::emplace<EventManager>();
		auto& events = Locator::events::value();
		events.AddHandler<ecs::events::VillagerDeathHelp>([this](const ecs::events::VillagerDeathHelp& event) {
			const auto& help = event.help;
			_help.killingPeople |= help.killingPeople;
			_help.deathInVillage |= help.deathInVillage;
			_help.worshippersDying |= help.worshippersDying;
			_help.losingVillagers |= help.losingVillagers;
			_help.lowOnPeople |= help.lowOnPeople;
		});
		events.AddHandler<ecs::events::VillagerDeadEffects>(
		    [this](const ecs::events::VillagerDeadEffects& event) { _effects.push_back(event.effects); });
		test::FakeVillagerStores stores(std::make_shared<ecs::systems::VillagerStores>());
		stores.temporaryStore = [](entt::entity, const map_coords::MapCoords& from, ResourceType) {
			return ecs::town_stores::TemporaryStore {entt::null, from};
		};
		Locator::villagerStores::emplace<test::FakeVillagerStores>(stores);
		ecs::effects::reactions::Clear();
		mourning::Clear();
	}

	void TearDown() override
	{
		mourning::Clear();
		ecs::effects::reactions::Clear();
		Locator::events::reset();
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

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity MakeVillager(glm::vec2 at = {100.0f, 130.0f}, uint32_t top = 163)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		ecs::object_index::Assign(e);
		auto& v = registry.Assign<Villager>(e);
		v.life = 1.0f;
		v.town = entt::null;
		v.abode = entt::null;
		v.food = 1.0f;
		v.lastCheckTurn = 1000;
		v.birthTurn = ecs::villager_age::BirthTurnFor(1000, 30);
		registry.Assign<LivingAction>(e, S(top), static_cast<uint16_t>(0));
		registry.Assign<Transform>(e, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<WallHug>(e, glm::vec2(), glm::vec2(), 0.0f, 0.1f);
		registry.Assign<Mobile>(e);
		registry.Assign<Mesh>(e, static_cast<entt::id_type>(1), static_cast<int8_t>(0), static_cast<int8_t>(0));
		return e;
	}

	static entt::entity MakeTown(PlayerNames owner, uint32_t id = 1)
	{
		auto& registry = Reg();
		const auto town = registry.Create();
		auto& t = registry.Assign<Town>(town, id);
		t.owner = owner;
		registry.Assign<Tribe>(town, Tribe::CELTIC);
		registry.Assign<Transform>(town, glm::vec3(50.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Context().towns[id] = town;
		return town;
	}

	static LivingAction& Action(entt::entity e) { return Reg().Get<LivingAction>(e); }
	static Villager& V(entt::entity e) { return Reg().Get<Villager>(e); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }

	/// LookAtPos until it faces `at` (the mourning states then see it at once)
	static void Face(entt::entity e, entt::entity at)
	{
		for (int i = 0; i < 32 && villager::LookAtPos(e, ecs::town_queries::PosOf(at), 1) == 0; ++i)
		{
		}
	}

	uint32_t _rand {0};
	float _floatRand {0.0f};
	villager::DeathHelp _help;
	std::vector<villager::DeadEffects> _effects;
};
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

TEST(VillagerDeathPure, HelpTable)
{
	using R = DeathReason;
	// the killer local: table A (SPELL, PLAYER_INTERACTION, DROWN)
	EXPECT_TRUE(villager::HelpFor(R::Spell, true, false, false, false, 0, 5).killingPeople);
	EXPECT_TRUE(villager::HelpFor(R::PlayerInteraction, true, false, false, false, 0, 5).killingPeople);
	EXPECT_TRUE(villager::HelpFor(R::PlayerInteractionDrown, true, false, false, false, 0, 5).killingPeople);
	EXPECT_FALSE(villager::HelpFor(R::Starving, true, true, true, false, 0, 5).killingPeople);
	// ... and then no "death in village", even when the owner is local too
	EXPECT_FALSE(villager::HelpFor(R::Starving, true, true, true, false, 0, 5).deathInVillage);
	// the owner local: table C
	for (const auto r : {R::Starving, R::Spell, R::Animal, R::Chant, R::Sacrifice, R::Exhaustion})
	{
		EXPECT_TRUE(villager::HelpFor(r, false, true, false, false, 0, 5).deathInVillage) << static_cast<int>(r);
	}
	for (const auto r : {R::None, R::PlayerInteraction, R::PlayerInteractionDrown, R::OldAge})
	{
		EXPECT_FALSE(villager::HelpFor(r, false, true, false, false, 0, 5).deathInVillage) << static_cast<int>(r);
	}
	// the town part: CHANT -> worshippers dying (owner local), whatever the adults
	EXPECT_TRUE(villager::HelpFor(R::Chant, false, true, false, true, 50, 5).worshippersDying);
	EXPECT_FALSE(villager::HelpFor(R::Chant, false, true, false, true, 50, 5).losingVillagers);
	EXPECT_FALSE(villager::HelpFor(R::Chant, false, false, false, true, 2, 5).lowOnPeople);
	// adults above the threshold: B, the killer not the owner, the owner local
	EXPECT_TRUE(villager::HelpFor(R::Animal, false, true, false, true, 6, 5).losingVillagers);
	EXPECT_FALSE(villager::HelpFor(R::Animal, false, true, true, true, 6, 5).losingVillagers);
	EXPECT_FALSE(villager::HelpFor(R::Starving, false, true, false, true, 6, 5).losingVillagers);
	// at or below it: low on people without the B test
	EXPECT_TRUE(villager::HelpFor(R::Starving, false, true, false, true, 5, 5).lowOnPeople);
	EXPECT_FALSE(villager::HelpFor(R::Starving, false, false, false, true, 5, 5).lowOnPeople);
	// no town: nothing of the town part
	const auto none = villager::HelpFor(R::Starving, false, true, false, false, 0, 5);
	EXPECT_FALSE(none.lowOnPeople || none.losingVillagers || none.worshippersDying);
}

TEST(VillagerDeathPure, DyingTimeAndClips)
{
	GVillagerInfo info {};
	info.dyingTimeWithoutGraveyard = 600;
	info.dyingTimeWithGraveyard = 120;
	EXPECT_EQ(villager::DyingTime(false, info), 600u);
	EXPECT_EQ(villager::DyingTime(true, info), 120u);
	EXPECT_EQ(villager::DyingClip(true, 2), 283);
	EXPECT_EQ(villager::DyingClip(false, 2), 246);
	for (const uint8_t lt : {0, 1, 3})
	{
		EXPECT_EQ(villager::DyingClip(false, lt), 253);
		EXPECT_EQ(villager::DeadClip(false, lt), 243);
	}
	EXPECT_EQ(villager::DeadClip(true, 0), 249);
	EXPECT_EQ(villager::DeadClip(false, 2), 246);
}

TEST(VillagerDeathPure, DeadTick)
{
	// 2 -> 1 -> 0 -> vanish at the third call (the counter wraps)
	auto t = ecs::living::DeadTick(2, false, DeathReason::OldAge);
	EXPECT_EQ(t.counter, 1u);
	EXPECT_FALSE(t.vanish);
	t = ecs::living::DeadTick(t.counter, false, DeathReason::OldAge);
	EXPECT_EQ(t.counter, 0u);
	EXPECT_FALSE(t.vanish);
	t = ecs::living::DeadTick(t.counter, false, DeathReason::OldAge);
	EXPECT_TRUE(t.vanish);
	// controlled by a script: the counter stays, no vanish; SACRIFICE vanishes at once either way
	t = ecs::living::DeadTick(0, true, DeathReason::OldAge);
	EXPECT_EQ(t.counter, 0u);
	EXPECT_FALSE(t.vanish);
	EXPECT_TRUE(ecs::living::DeadTick(50, true, DeathReason::Sacrifice).vanish);
	EXPECT_TRUE(ecs::living::DeadTick(50, false, DeathReason::Sacrifice).vanish);
}

TEST(VillagerDeathPure, Soul)
{
	EXPECT_EQ(soul::SoulClip(false, false, 49.9f), 244);
	EXPECT_EQ(soul::SoulClip(false, false, 50.0f), 245);
	EXPECT_EQ(soul::SoulClip(true, false, 10.0f), 247);
	EXPECT_EQ(soul::SoulClip(true, false, 90.0f), 248);
	EXPECT_EQ(soul::SoulClip(false, true, 90.0f), 244);
	EXPECT_EQ(soul::SoulClip(true, true, 90.0f), 247);
	EXPECT_EQ(soul::SoulAlpha(0, 2000), 105);
	EXPECT_EQ(soul::SoulAlpha(1500, 2000), 105);
	// 250 ms into the last 500: (1 - 250 x 0.002) x 105 = 52.5 -> 52
	EXPECT_EQ(soul::SoulAlpha(1750, 2000), 52);
	// 100 ms in: (1 - 0.2f) x 105 in float steps (24-bit x87) = 84 (83.99999 in double would give 83)
	EXPECT_EQ(soul::SoulAlpha(1600, 2000), 84);
	EXPECT_FALSE(soul::SoulExpired(1890, 2000));
	EXPECT_TRUE(soul::SoulExpired(1891, 2000));
}

TEST(VillagerDeathPure, AlignmentAndMourningTurns)
{
	GPlayerInfo info {};
	info.dealthReason = {0.0f, -0.85f, -0.9f, -0.75f, -0.5f, -0.1f, -0.9f, -1.0f, -0.6f, 0.0f};
	EXPECT_FLOAT_EQ(ecs::effects::alignment::DeathAlignmentChange(info, DeathReason::Starving, false, false), -0.85f);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::DeathAlignmentChange(info, DeathReason::Starving, true, false), -1.7f);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::DeathAlignmentChange(info, DeathReason::Starving, false, true), -0.425f);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::DeathAlignmentChange(info, DeathReason::OldAge, true, false), 0.0f);
	EXPECT_EQ(mourning::PointTurns(0.0f), 20);
	EXPECT_EQ(mourning::PointTurns(19.99f), 219);
	EXPECT_EQ(mourning::MournTurns(0.0f), 40);
	EXPECT_EQ(mourning::MournTurns(2.95f), 69);
}

// ---- VillagerDead ------------------------------------------------------------------------------------------------

TEST_F(VillagerDeathTest, Guards)
{
	// already dead: status 1, or TOP 15
	auto a = MakeVillager();
	V(a).status = Villager::k_StatusDead;
	villager::VillagerDead(a, DeathReason::Starving, std::nullopt, 0.0f, 1);
	EXPECT_EQ(Top(a), 163u);
	EXPECT_EQ(V(a).deathReason, DeathReason::None);
	auto b = MakeVillager({100.0f, 130.0f}, 15);
	villager::VillagerDead(b, DeathReason::Starving, std::nullopt, 0.0f, 1);
	EXPECT_EQ(V(b).status, 0u);
	EXPECT_EQ(V(b).deathReason, DeathReason::None);
}

TEST_F(VillagerDeathTest, OrderAndFields)
{
	auto a = MakeVillager();
	villager::VillagerDead(a, DeathReason::Starving, std::nullopt, 0.5f, 1);
	EXPECT_EQ(V(a).status, static_cast<uint16_t>(Villager::k_StatusDead | Villager::k_StatusLandTypeMask));
	EXPECT_FLOAT_EQ(V(a).life, 0.0f);
	EXPECT_EQ(Top(a), 14u);
	EXPECT_EQ(V(a).deathReason, DeathReason::Starving);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 600u);
	EXPECT_TRUE(villager::IsDead(a));
	EXPECT_TRUE(villager::IsCountedOut(a));
	// with a functional graveyard in its town: 120
	const auto town = MakeTown(PlayerNames::NEUTRAL);
	test::FakeVillagerWorldQueries world(std::make_shared<ecs::systems::VillagerWorldQueries>());
	world.graveyard = [town](entt::entity t) { return t == town ? std::optional<bool>(true) : std::nullopt; };
	Locator::villagerWorldQueries::emplace<test::FakeVillagerWorldQueries>(world);
	auto b = MakeVillager();
	V(b).town = town;
	villager::VillagerDead(b, DeathReason::OldAge, std::nullopt, 1.0f, 1);
	EXPECT_EQ(Action(b).turnsUntilStateChange, 120u);
	// SACRIFICE: the counter 0
	auto c = MakeVillager();
	villager::VillagerDead(c, DeathReason::Sacrifice, PlayerNames::PLAYER_ONE, 1.0f, 1);
	EXPECT_EQ(Action(c).turnsUntilStateChange, 0u);
	EXPECT_EQ(villager::GetDeathReason(c), DeathReason::Sacrifice);
}

TEST_F(VillagerDeathTest, OwnerKillerAndCounters)
{
	const auto town = MakeTown(PlayerNames::PLAYER_TWO);
	Reg().Get<Town>(town).buildPulsePrevious = 1;
	auto a = MakeVillager();
	V(a).town = town;
	villager::VillagerDead(a, DeathReason::Starving, std::nullopt, 0.0f, 1);
	// the owner's alignment, the town's counters (killer none -> the neutral player's slot), the pulse
	EXPECT_FLOAT_EQ(ecs::effects::alignment::Of(PlayerNames::PLAYER_TWO).pending, -0.85f);
	const auto& deaths = Reg().Get<TownDeaths>(town);
	EXPECT_EQ(deaths.byReason.at(1), 1u);
	EXPECT_EQ(deaths.byPlayer.at(static_cast<size_t>(PlayerNames::NEUTRAL)), 1u);
	EXPECT_EQ(deaths.count, 1u);
	EXPECT_EQ(deaths.total38, 1u);
	EXPECT_EQ(deaths.total5C.at(0), 1u);
	EXPECT_EQ(deaths.lastDeathTurn, 1000u);
	EXPECT_EQ(Reg().Get<Town>(town).buildPulse, 1u);
	EXPECT_EQ(Reg().Get<Town>(town).buildPulsePrevious, 0u);
	// out of its town
	EXPECT_TRUE(V(a).town == entt::null);
	EXPECT_FALSE(villager::GetPlayerOf(a).has_value());
	// a child of the town killed by player one: twice the change, its slot
	auto b = MakeVillager();
	V(b).town = town;
	V(b).flags = Villager::k_FlagChild;
	villager::VillagerDead(b, DeathReason::Starving, PlayerNames::PLAYER_ONE, 0.0f, 1);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::Of(PlayerNames::PLAYER_TWO).pending, -0.85f - 1.7f);
	EXPECT_EQ(Reg().Get<TownDeaths>(town).byPlayer.at(0), 1u);
	// no town: no alignment change, no counters
	auto c = MakeVillager();
	villager::VillagerDead(c, DeathReason::Starving, std::nullopt, 0.0f, 1);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::Of(PlayerNames::NEUTRAL).pending, 0.0f);
}

TEST_F(VillagerDeathTest, ReportsToGuidance)
{
	// a villager of the local player's small town killed by the local player with the hand
	const auto town = MakeTown(PlayerNames::PLAYER_ONE);
	auto a = MakeVillager();
	V(a).town = town;
	villager::VillagerDead(a, DeathReason::PlayerInteraction, PlayerNames::PLAYER_ONE, 0.0f, 1);
	EXPECT_TRUE(_help.killingPeople);
	EXPECT_FALSE(_help.deathInVillage);
	EXPECT_TRUE(_help.lowOnPeople);
	// a worshipper's death
	_help = {};
	auto b = MakeVillager();
	V(b).town = town;
	villager::VillagerDead(b, DeathReason::Chant, PlayerNames::PLAYER_ONE, 0.0f, 1);
	EXPECT_TRUE(_help.worshippersDying);
	EXPECT_FALSE(_help.lowOnPeople);
}

TEST_F(VillagerDeathTest, WorldPopulation)
{
	auto a = MakeVillager();
	auto b = MakeVillager();
	EXPECT_EQ(magic::players::WorldPopulation(), 2u);
	villager::VillagerDead(a, DeathReason::OldAge, std::nullopt, 1.0f, 1);
	EXPECT_EQ(magic::players::WorldPopulation(), 1u);
	// SetDying again (row 13) does not count it out twice
	villager::SetDying(a);
	EXPECT_EQ(magic::players::WorldPopulation(), 1u);
	// Delete of a live one: gone (the villager's removal takes off one not counted out); no death reason
	villager::Delete(b);
	EXPECT_FALSE(Reg().Valid(b));
	EXPECT_EQ(magic::players::WorldPopulation(), 0u);
	// Delete of the counted-out corpse: no change
	villager::Delete(a);
	EXPECT_EQ(magic::players::WorldPopulation(), 0u);
}

// ---- the states --------------------------------------------------------------------------------------------------

TEST_F(VillagerDeathTest, Dying)
{
	// reason 7: straight to 15
	auto a = MakeVillager({100.0f, 130.0f}, 14);
	V(a).deathReason = DeathReason::Sacrifice;
	villager::Dying(Action(a));
	EXPECT_EQ(Top(a), 15u);
	// else 23 WAIT_FOR_ANIMATION with FINAL 15, and one REACT_TO_DEATH
	auto b = MakeVillager({100.0f, 130.0f}, 14);
	V(b).deathReason = DeathReason::OldAge;
	villager::Dying(Action(b));
	EXPECT_EQ(Top(b), 23u);
	EXPECT_EQ(Final(b), 15u);
	EXPECT_NE(ecs::effects::reactions::GetReactionOfTypeInitiatedBy(b, Reaction::ReactToDeath), 0u);
	// at home: no reaction
	auto c = MakeVillager({100.0f, 130.0f}, 14);
	Reg().Assign<AtHome>(c);
	villager::Dying(Action(c));
	EXPECT_EQ(ecs::effects::reactions::GetReactionOfTypeInitiatedBy(c, Reaction::ReactToDeath), 0u);
}

TEST_F(VillagerDeathTest, Dead)
{
	const auto skeleton = resources::HashIdentifier(MeshId::PersonSkeletonMale);
	// the first DEAD turn: smoke and the skeleton (in the water here: no soul)
	auto a = MakeVillager({100.0f, 130.0f}, 15);
	Action(a).turnsUntilStateChange = 2;
	EXPECT_EQ(villager::Dead(Action(a)), 1u);
	ASSERT_EQ(_effects.size(), 1u);
	EXPECT_TRUE(_effects.at(0).smoke);
	EXPECT_TRUE(_effects.at(0).skeleton);
	EXPECT_FALSE(_effects.at(0).soul);
	EXPECT_EQ(Reg().Get<Mesh>(a).id, skeleton);
	// lying dead it is no longer shadowed: no ground blobs
	EXPECT_FALSE(V(a).humanShadowed);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 1u);
	// the next turns: nothing more until the vanish (the third call), then a puff and it is gone
	EXPECT_EQ(villager::Dead(Action(a)), 1u);
	EXPECT_EQ(_effects.size(), 1u);
	EXPECT_EQ(villager::Dead(Action(a)), 5u);
	EXPECT_EQ(_effects.size(), 2u);
	EXPECT_FALSE(Reg().Valid(a));
	// controlled by a script: no smoke, no skeleton, the counter stays
	auto b = MakeVillager({100.0f, 130.0f}, 15);
	ecs::script_held::SetControlledByScript(b, true);
	Action(b).turnsUntilStateChange = 0;
	EXPECT_EQ(villager::Dead(Action(b)), 1u);
	EXPECT_EQ(_effects.size(), 2u);
	EXPECT_NE(Reg().Get<Mesh>(b).id, skeleton);
	EXPECT_TRUE(V(b).humanShadowed);
	EXPECT_EQ(Action(b).turnsUntilStateChange, 0u);
	// ... except SACRIFICE, which vanishes at once
	V(b).deathReason = DeathReason::Sacrifice;
	EXPECT_EQ(villager::Dead(Action(b)), 5u);
}

TEST_F(VillagerDeathTest, TheSkeletonIsAlsoItsHighMesh)
{
	// the original sets all three of its meshes to the skeleton: the top graphics detail draws it too
	const auto skeleton = resources::HashIdentifier(MeshId::PersonSkeletonMale);
	auto a = MakeVillager({100.0f, 130.0f}, 15);
	Reg().Assign<ecs::components::DetailMesh>(a, static_cast<entt::id_type>(2));
	Action(a).turnsUntilStateChange = 2;
	EXPECT_EQ(villager::Dead(Action(a)), 1u);
	EXPECT_EQ(Reg().Get<Mesh>(a).id, skeleton);
	EXPECT_EQ(Reg().Get<ecs::components::DetailMesh>(a).high, skeleton);
	// controlled by a script: neither changes
	auto b = MakeVillager({100.0f, 130.0f}, 15);
	Reg().Assign<ecs::components::DetailMesh>(b, static_cast<entt::id_type>(2));
	ecs::script_held::SetControlledByScript(b, true);
	EXPECT_EQ(villager::Dead(Action(b)), 1u);
	EXPECT_EQ(Reg().Get<ecs::components::DetailMesh>(b).high, 2u);
}

TEST_F(VillagerDeathTest, CannotExitState)
{
	auto a = MakeVillager({100.0f, 130.0f}, 15);
	EXPECT_EQ(villager::CannotExitState(Action(a), S(24)), 1u);
	EXPECT_EQ(villager::CannotExitState(Action(a), S(10)), 1u);
	EXPECT_EQ(villager::CannotExitState(Action(a), S(15)), 1u);
	EXPECT_EQ(villager::CannotExitState(Action(a), S(163)), 0u);
	EXPECT_EQ(villager::CannotExitState(Action(a), S(13)), 0u);
	EXPECT_EQ(villager::CannotExitState(Action(a), S(14)), 0u);
}

TEST_F(VillagerDeathTest, DeleteDependants)
{
	// alive: SET_DYING through the exits, out of the vagrants
	auto a = MakeVillager({100.0f, 130.0f}, 163);
	auto& table = static_cast<FakeStateTable&>(Locator::livingActionSystem::value());
	table.calls.clear();
	villager::DeleteDependants(a);
	EXPECT_EQ(Top(a), 13u);
	EXPECT_NE(std::find(table.calls.begin(), table.calls.end(), "exit 163 13"), table.calls.end());
	// in a town (no abode): out of it
	const auto town = MakeTown(PlayerNames::NEUTRAL);
	auto b = MakeVillager();
	V(b).town = town;
	villager::DeleteDependants(b);
	EXPECT_TRUE(V(b).town == entt::null);
}

// ---- the mourning ------------------------------------------------------------------------------------------------

TEST_F(VillagerDeathTest, Orphans)
{
	const auto town = MakeTown(PlayerNames::NEUTRAL);
	auto mother = MakeVillager();
	V(mother).town = town;
	auto child = MakeVillager();
	V(child).town = town;
	V(child).mother = mother;
	ecs::town_villagers::AddToHomelessList(town, child);
	mourning::FindChildrenAndOrphanThem(mother);
	EXPECT_EQ(Top(child), 131u);
	EXPECT_TRUE(V(child).mother == entt::null);
	// not her child: nothing
	auto other = MakeVillager();
	EXPECT_EQ(mourning::MakeChildOrphaned(other, mother), 0u);
	EXPECT_EQ(Top(other), 163u);
}

TEST_F(VillagerDeathTest, MourningPriorityAndSetup)
{
	auto dead = MakeVillager({100.0f, 130.0f}, 15);
	const auto reaction = ecs::effects::reactions::CreateReaction(dead, Reaction::ReactToDeath, PlayerNames::NEUTRAL, false);
	ASSERT_NE(reaction, 0u);
	auto a = MakeVillager({103.0f, 130.0f});
	EXPECT_EQ(mourning::ReactToDeathPriority(a, reaction), 100u);
	// the dead one itself: 0
	EXPECT_EQ(mourning::ReactToDeathPriority(dead, reaction), 0u);
	// a villager of a town with a functional graveyard: 0
	const auto town = MakeTown(PlayerNames::NEUTRAL);
	test::FakeVillagerWorldQueries world(std::make_shared<ecs::systems::VillagerWorldQueries>());
	world.graveyard = [town](entt::entity t) { return t == town ? std::optional<bool>(true) : std::nullopt; };
	Locator::villagerWorldQueries::emplace<test::FakeVillagerWorldQueries>(world);
	auto g = MakeVillager();
	V(g).town = town;
	EXPECT_EQ(mourning::ReactToDeathPriority(g, reaction), 0u);
	// roll 0 -> 205 with the counter 0; roll 1 -> 206
	_rand = 0;
	Action(a).turnsUntilStateChange = 77;
	mourning::SetupReactToDeath(a, dead, reaction);
	EXPECT_EQ(Top(a), 205u);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 0u);
	EXPECT_TRUE(mourning::IsReacting(a));
	EXPECT_EQ(mourning::ReactionObject(a), dead);
	_rand = 1;
	auto b = MakeVillager({97.0f, 130.0f});
	mourning::SetupReactToDeath(b, dead, reaction);
	EXPECT_EQ(Top(b), 206u);
	// ten took it: the eleventh gets 0
	for (int i = 0; i < 8; ++i)
	{
		mourning::SetupReactToDeath(MakeVillager({100.0f, 133.0f}), dead, reaction);
	}
	EXPECT_EQ(mourning::ReactToDeathPriority(MakeVillager({100.0f, 127.0f}), reaction), 0u);
}

TEST_F(VillagerDeathTest, AMourningEndsAfterItsTurnsToReactOrWhenItIsNoLongerAvailable)
{
	// every turn, before its state, a REACT_TO_DEATH ends as any reaction does: past the standard turns to react at its
	// distance to the dead it goes back to what it was doing; a reaction no longer available only lets it go
	namespace reactions = ecs::effects::reactions;
	auto dead = MakeVillager({100.0f, 130.0f}, 15);
	const auto reaction = reactions::CreateReaction(dead, Reaction::ReactToDeath, PlayerNames::NEUTRAL, false);
	ASSERT_NE(reaction, 0u);
	const auto type = static_cast<uint8_t>(Reaction::ReactToDeath);
	const auto start = [dead, reaction, type](entt::entity v) {
		// as the spread does: the record of the type, then the setup
		ASSERT_TRUE(reactions::Records(v, type, 0, reactions::Turn()));
		mourning::SetupReactToDeath(v, dead, reaction);
		ASSERT_TRUE(mourning::IsReacting(v));
	};
	_rand = 1;
	auto a = MakeVillager({103.0f, 130.0f});
	start(a);
	const auto started = reactions::RecordTurn(a, type);
	const float distance = ecs::town_queries::GetDistanceInMetres(ecs::town_queries::PosOf(a), ecs::town_queries::PosOf(dead));
	const auto turns = reactions::StandardTurnsToReact(type, distance);
	game_clock::SetTurn(started + turns);
	mourning::ProcessReaction(a);
	EXPECT_TRUE(mourning::IsReacting(a));
	game_clock::SetTurn(started + turns + 1);
	mourning::ProcessReaction(a);
	EXPECT_FALSE(mourning::IsReacting(a));

	auto b = MakeVillager({97.0f, 130.0f});
	start(b);
	const auto top = Top(b);
	reactions::SetAvailable(dead, false);
	mourning::ProcessReaction(b);
	EXPECT_FALSE(mourning::IsReacting(b));
	EXPECT_EQ(Top(b), top);
}

TEST_F(VillagerDeathTest, MourningStates)
{
	auto dead = MakeVillager({100.0f, 130.0f}, 15);
	const auto reaction = ecs::effects::reactions::CreateReaction(dead, Reaction::ReactToDeath, PlayerNames::NEUTRAL, false);
	auto a = MakeVillager({103.0f, 130.0f});
	_rand = 0;
	mourning::SetupReactToDeath(a, dead, reaction);
	Face(a, dead);
	// 205: GameFloatRand(20) = 0 -> 20 facing turns, the 21st goes to 206
	_floatRand = 0.0f;
	for (int i = 0; i < 20; ++i)
	{
		mourning::PointAtDeadPerson(Action(a));
	}
	EXPECT_EQ(Top(a), 205u);
	mourning::PointAtDeadPerson(Action(a));
	EXPECT_EQ(Top(a), 206u);
	// 206 within 1.2 R (3 m < 4.8 m): 207
	mourning::GoTowardsDeadPerson(Action(a));
	EXPECT_EQ(Top(a), 207u);
	// 207 facing: 208, counter 0
	Action(a).turnsUntilStateChange = 9;
	mourning::LookAtDeadPerson(Action(a));
	EXPECT_EQ(Top(a), 208u);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 0u);
	// 208: GameFloatRand(3) = 0 -> 40 turns, then StopReactingAndSetState
	for (int i = 0; i < 40; ++i)
	{
		mourning::MournDeadPerson(Action(a));
	}
	EXPECT_TRUE(mourning::IsReacting(a));
	mourning::MournDeadPerson(Action(a));
	EXPECT_FALSE(mourning::IsReacting(a));
	// 206 far away: a walk of the whole metres of d - R towards the dead one (30.5 - 4 -> 26 m)
	auto b = MakeVillager({130.5f, 130.0f});
	_rand = 1;
	mourning::SetupReactToDeath(b, dead, reaction);
	mourning::GoTowardsDeadPerson(Action(b));
	EXPECT_EQ(Top(b), 1u); // MOVE_TO_POS
	EXPECT_EQ(Final(b), 206u);
	EXPECT_NEAR(Reg().Get<WallHug>(b).goal.x, 104.5f, 0.1f);
	// the dead one gone: the validate pops
	villager::Delete(dead);
	EXPECT_FALSE(mourning::ReactionValidate(Action(b)));
}

// ---- the takers of a REACT_TO_DEATH, the deletion, the water --------------------------------------------------------

TEST_F(VillagerDeathTest, MourningTakersCountTheCurrentOnes)
{
	auto dead = MakeVillager({100.0f, 130.0f}, 15);
	const auto reaction = ecs::effects::reactions::CreateReaction(dead, Reaction::ReactToDeath, PlayerNames::NEUTRAL, false);
	ASSERT_NE(reaction, 0u);
	std::vector<entt::entity> mourners;
	for (int i = 0; i < 10; ++i)
	{
		mourners.push_back(MakeVillager({100.0f, 133.0f}));
		mourning::SetupReactToDeath(mourners.back(), dead, reaction);
	}
	// villager_reactions::IsReacting knows the mourning (a villager has one reaction slot)
	EXPECT_TRUE(ecs::villager_reactions::IsReacting(mourners.front()));
	const auto next = MakeVillager({100.0f, 127.0f});
	EXPECT_EQ(mourning::ReactToDeathPriority(next, reaction), 0u);
	// StopReacting takes one off the reaction's takers: one stops (through villager_reactions), the next one may take
	// it
	ecs::villager_reactions::StopReacting(mourners.front());
	EXPECT_FALSE(mourning::IsReacting(mourners.front()));
	EXPECT_FALSE(ecs::villager_reactions::IsReacting(mourners.front()));
	EXPECT_EQ(mourning::ReactToDeathPriority(next, reaction), 100u);
	// a second stop of the same villager changes nothing
	ecs::villager_reactions::StopReacting(mourners.front());
	mourning::SetupReactToDeath(next, dead, reaction);
	EXPECT_EQ(mourning::ReactToDeathPriority(MakeVillager({100.0f, 127.0f}), reaction), 0u);
	// villager::Delete stops the reaction (ecs::ToBeDeleted's villager branch)
	villager::Delete(mourners.at(1));
	EXPECT_FALSE(mourning::IsReacting(mourners.at(1)));
	EXPECT_EQ(mourning::ReactToDeathPriority(MakeVillager({100.0f, 127.0f}), reaction), 100u);
}

TEST_F(VillagerDeathTest, DeleteDependantsOfAWoman)
{
	// a mother (sex 1 in her info): her children (mother == her) go to 131 and lose her
	const_cast<GVillagerInfo&>(Locator::infoConstants::value().villager.at(0)).sex = SexType::Female;
	const auto town = MakeTown(PlayerNames::NEUTRAL);
	auto mother = MakeVillager();
	V(mother).town = town;
	auto child = MakeVillager();
	V(child).town = town;
	V(child).mother = mother;
	ecs::town_villagers::AddToHomelessList(town, child);
	villager::DeleteDependants(mother);
	EXPECT_EQ(Top(mother), 13u);
	EXPECT_TRUE(V(mother).town == entt::null);
	EXPECT_EQ(Top(child), 131u);
	EXPECT_TRUE(V(child).mother == entt::null);
}

TEST_F(VillagerDeathTest, HasSunk)
{
	// HasSunk: the status bit -> SetTopState(14) and dyingTimeWithoutGraveyard
	auto a = MakeVillager({100.0f, 130.0f}, 15);
	V(a).status = Villager::k_StatusDead;
	EXPECT_TRUE(ecs::HasSunk(a));
	EXPECT_EQ(Top(a), 14u);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 600u);
	// TOP 15 without the bit (villager::IsDead would say dead): not the corpse branch
	auto b = MakeVillager({100.0f, 130.0f}, 15);
	EXPECT_TRUE(ecs::HasSunk(b));
	EXPECT_NE(Top(b), 14u);
	// not available (the final state 14 DYING): not sunk, nothing changes
	auto c = MakeVillager({100.0f, 130.0f}, 14);
	V(c).status = Villager::k_StatusDead;
	Action(c).turnsUntilStateChange = 7;
	EXPECT_FALSE(ecs::HasSunk(c));
	EXPECT_EQ(Top(c), 14u);
	EXPECT_EQ(Action(c).turnsUntilStateChange, 7u);
}

TEST_F(VillagerDeathTest, HasSunkTellsTheCreatureOfWhoThrewIt)
{
	// a villager that can still be reached reports the player whose interface last let go of it, before the rest of
	// its sinking; one that cannot, or that no hand let go of, reports nothing, even when a thrower last touched it
	std::vector<ecs::events::PlayerDeedForMimic> deeds;
	Locator::events::value().AddHandler<ecs::events::PlayerDeedForMimic>(
	    [&deeds](const ecs::events::PlayerDeedForMimic& event) { deeds.push_back(event); });
	Reg().Assign<Player>(Reg().Create(), PlayerNames::PLAYER_ONE);
	auto thrown = MakeVillager({100.0f, 130.0f}, 15);
	ecs::last_dropped::RecordDrop(Reg(), PlayerNames::PLAYER_ONE, thrown);
	EXPECT_TRUE(ecs::HasSunk(thrown));
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.front().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(deeds.front().deed, creature_watching::Deed::ThrowInTheSea);
	EXPECT_EQ(deeds.front().object, thrown);
	EXPECT_FALSE(deeds.front().magic.has_value());
	deeds.clear();
	auto unreachable = MakeVillager({100.0f, 130.0f}, 14);
	V(unreachable).status = Villager::k_StatusDead;
	ecs::last_dropped::RecordDrop(Reg(), PlayerNames::PLAYER_ONE, unreachable);
	EXPECT_FALSE(ecs::HasSunk(unreachable));
	auto walked = MakeVillager({100.0f, 130.0f}, 15);
	EXPECT_TRUE(ecs::HasSunk(walked));
	auto struck = MakeVillager({100.0f, 130.0f}, 15);
	Reg().AssignState<VillagerLastInteraction>(struck, PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(ecs::HasSunk(struck));
	EXPECT_TRUE(deeds.empty());
}

TEST_F(VillagerDeathTest, EndPhysicsInWater)
{
	// the end of the physics in the water at 0 life: dead -> SetTopState(14), the counter 600
	auto a = MakeVillager({100.0f, 130.0f}, 15);
	V(a).status = Villager::k_StatusDead;
	V(a).life = 0.0f;
	ecs::VillagerEndPhysicsInWater(a);
	EXPECT_EQ(Top(a), 14u);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 600u);
	EXPECT_EQ(V(a).deathReason, DeathReason::None);
	// not dead: VillagerDead(6 PLAYER_INTERACTION_DROWN, ..., 0.01, 1)
	auto b = MakeVillager();
	V(b).life = 0.0f;
	ecs::VillagerEndPhysicsInWater(b);
	EXPECT_EQ(V(b).deathReason, DeathReason::PlayerInteractionDrown);
	EXPECT_EQ(Top(b), 14u);
	EXPECT_TRUE(villager::IsDead(b));
}

TEST_F(VillagerDeathTest, AnIndestructibleVillagerNeverFinishesDrowning)
{
	// each drowning turn counts down; an indestructible villager's count is set back to 10 first, so it never gets to
	// 0 and he never drowns
	auto a = MakeVillager({100.0f, 130.0f}, static_cast<uint32_t>(VillagerStates::Drowning));
	Reg().Assign<Indestructible>(a);
	Action(a).turnsUntilStateChange = 1;
	EXPECT_EQ(ecs::VillagerDrowningState(Action(a)), 1u);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 9u);
	for (int turn = 0; turn < 50; ++turn)
	{
		EXPECT_EQ(ecs::VillagerDrowningState(Action(a)), 1u);
	}
	EXPECT_EQ(Action(a).turnsUntilStateChange, 9u);
	EXPECT_FALSE(villager::IsDead(a));
	EXPECT_EQ(V(a).deathReason, DeathReason::None);
	// any other villager drowns when his count runs out: VillagerDead(6 PLAYER_INTERACTION_DROWN)
	auto b = MakeVillager({100.0f, 130.0f}, static_cast<uint32_t>(VillagerStates::Drowning));
	Action(b).turnsUntilStateChange = 1;
	EXPECT_EQ(ecs::VillagerDrowningState(Action(b)), 1u);
	EXPECT_TRUE(villager::IsDead(b));
	EXPECT_EQ(V(b).deathReason, DeathReason::PlayerInteractionDrown);
}

TEST_F(VillagerDeathTest, AThrownCorpseLandsDeadAgainWithItsCounterKept)
{
	// a body of a villager comes to rest on dry land, upright (land type 0), thrown by player one's hand. With no life
	// left: already dead, it goes back to DEAD with what was left of its counter; not dead yet, it dies of the player's
	// interaction (VillagerDead 5) there
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {1}); // dry land everywhere
	ecs::map_cells::Clear();
	ecs::PhysicsEntry po;
	po.throwerPlayer = PlayerNames::PLAYER_ONE;
	const auto corpse = MakeVillager({100.0f, 130.0f}, static_cast<uint32_t>(VillagerStates::Flying));
	V(corpse).status = Villager::k_StatusDead;
	V(corpse).life = 0.0f;
	Action(corpse).turnsUntilStateChange = 321;
	po.entity = corpse;
	EXPECT_EQ(ecs::villager_physics::VillagerEndPhysicsOfClass(corpse, po), entt::entity {entt::null});
	EXPECT_EQ(Top(corpse), static_cast<uint32_t>(VillagerStates::Dead));
	EXPECT_EQ(Action(corpse).turnsUntilStateChange, 321u);
	EXPECT_EQ(V(corpse).deathReason, DeathReason::None);
	EXPECT_EQ((V(corpse).status & Villager::k_StatusLandTypeMask) >> Villager::k_LandTypeShift, 0);

	const auto alive = MakeVillager({100.0f, 130.0f}, static_cast<uint32_t>(VillagerStates::Flying));
	V(alive).life = 0.0f;
	po.entity = alive;
	EXPECT_EQ(ecs::villager_physics::VillagerEndPhysicsOfClass(alive, po), entt::entity {entt::null});
	EXPECT_TRUE(villager::IsDead(alive));
	EXPECT_EQ(V(alive).deathReason, DeathReason::PlayerInteraction);
	ecs::map_cells::Clear();
}

// ---- an effect that kills (effects::ApplyEffect) ------------------------------------------------------------------

namespace
{
/// An effect that crushes by `crush`, with no applier, for a player or none
ecs::effects::EffectValues Crushing(float crush, std::optional<PlayerNames> player = std::nullopt)
{
	ecs::effects::EffectValues values;
	values.numbers.at(static_cast<size_t>(ecs::effects::EffectValues::Number::Crush)) = crush;
	values.player = player;
	return values;
}

/// The constants with the villagers' crush counted whole, and a combustion temperature so that no burn harm is made
void CrushableVillagers()
{
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	for (auto& row : info->villager)
	{
		row.defenceMultiplierCrush = 1.0f;
		row.combustionTemperature = 1000.0f;
	}
	Locator::infoConstants::reset(info.release());
}

/// The crushed reactions the thing started
std::vector<ecs::effects::reactions::Reaction> CrushedReactionsOf(entt::entity initiator)
{
	std::vector<ecs::effects::reactions::Reaction> crushed;
	std::ranges::copy_if(ecs::effects::reactions::All(), std::back_inserter(crushed), [initiator](const auto& reaction) {
		return reaction.initiator == initiator && reaction.type == Reaction::ReactToObjectCrushed;
	});
	return crushed;
}
} // namespace

TEST_F(VillagerDeathTest, AnEffectKillsBeforeItMakesTheCrushedReaction)
{
	CrushableVillagers();
	const auto town = MakeTown(PlayerNames::PLAYER_TWO);
	auto a = MakeVillager();
	V(a).town = town;
	ASSERT_EQ(villager::GetPlayerOf(a).value_or(PlayerNames::NEUTRAL), PlayerNames::PLAYER_TWO);

	auto values = Crushing(2.0f, PlayerNames::PLAYER_ONE);
	ecs::effects::ApplyEffect(a, values);

	// killed by the miracle, which took it out of its town
	EXPECT_TRUE(villager::IsDead(a));
	EXPECT_EQ(V(a).deathReason, DeathReason::Spell);
	EXPECT_TRUE(V(a).town == entt::null);
	// only then crushed: the reaction, started by the villager itself as nothing applied the effect, is for the player
	// it has now. With no town it has none (the original's villager gives a null player), not its town's owner
	const auto crushed = CrushedReactionsOf(a);
	ASSERT_EQ(crushed.size(), 1u);
	EXPECT_EQ(crushed.front().player, std::nullopt);
}

// A fire that burnt the last of a villager's life destroys it with the fire's player (and 0, not the burn): a spell death
// the town counts against that player
TEST_F(VillagerDeathTest, ABurntVillagerDiesByTheFiresPlayer)
{
	const auto town = MakeTown(PlayerNames::PLAYER_ONE);
	auto a = MakeVillager();
	V(a).town = town;
	ecs::fire::traits::DestroyedByEffect(a, PlayerNames::PLAYER_TWO, 0.0f);
	EXPECT_TRUE(villager::IsDead(a));
	EXPECT_EQ(V(a).deathReason, DeathReason::Spell);
	const auto& deaths = Reg().Get<TownDeaths>(town);
	EXPECT_EQ(deaths.byPlayer.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)), 1u);
	EXPECT_EQ(deaths.byPlayer.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)), 0u);
	EXPECT_EQ(deaths.count, 1u);
}

TEST_F(VillagerDeathTest, TheHarmAPlayersEffectDoesIsKeptAgainstThatPlayer)
{
	const openblack::test::RestoreService<Locator::playerSystem> restore;
	auto& players = Locator::playerSystem::emplace<ecs::systems::PlayerSystem>();
	CrushableVillagers();
	const auto town = MakeTown(PlayerNames::PLAYER_TWO);
	auto a = MakeVillager();
	V(a).town = town;
	const auto harmOf = [&](PlayerNames player, PlayerNames by) {
		return players.Rivalry(player).harmFrom.at(static_cast<size_t>(by));
	};
	// a crush that does not kill: its town's player keeps the harm against the caster
	auto values = Crushing(0.25f, PlayerNames::PLAYER_ONE);
	ecs::effects::ApplyEffect(a, values);
	EXPECT_FALSE(villager::IsDead(a));
	EXPECT_EQ(harmOf(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE), 0.25f);
	auto again = Crushing(0.5f, PlayerNames::PLAYER_ONE);
	ecs::effects::ApplyEffect(a, again);
	EXPECT_EQ(harmOf(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE), 0.75f);
	// an effect of no player keeps nothing
	auto nobody = Crushing(0.125f);
	ecs::effects::ApplyEffect(a, nobody);
	for (size_t p = 0; p < static_cast<size_t>(PlayerNames::_COUNT); ++p)
	{
		EXPECT_EQ(players.Rivalry(PlayerNames::PLAYER_TWO).harmFrom.at(p), p == 0 ? 0.75f : 0.0f) << p;
	}
	// a villager of no town is no player's: nothing is kept
	auto b = MakeVillager();
	auto loose = Crushing(0.25f, PlayerNames::PLAYER_THREE);
	ecs::effects::ApplyEffect(b, loose);
	for (size_t p = 0; p < static_cast<size_t>(PlayerNames::_COUNT); ++p)
	{
		EXPECT_EQ(players.Rivalry(static_cast<PlayerNames>(p)).harmFrom.at(static_cast<size_t>(PlayerNames::PLAYER_THREE)),
		          0.0f)
		    << p;
	}
}

TEST_F(VillagerDeathTest, TheHarmToARockADeadTreeAnIconOrAPileIsKeptByThePlayerItWasMadeFor)
{
	auto& registry = Reg();
	using ecs::effects::HarmedPlayerOf;
	const auto is = [](PlayerNames player) { return std::optional<PlayerNames> {player}; };
	// a rock from a player's reward chest is that player's; a land's rock the neutral player's
	const auto rock = registry.Create();
	registry.Assign<MobileStatic>(rock,
	                              MobileStatic {.type = static_cast<MobileStaticInfo>(0), .owner = PlayerNames::PLAYER_THREE});
	EXPECT_EQ(HarmedPlayerOf(rock), is(PlayerNames::PLAYER_THREE));
	const auto landRock = registry.Create();
	registry.Assign<MobileStatic>(landRock, MobileStatic {});
	EXPECT_EQ(HarmedPlayerOf(landRock), is(PlayerNames::NEUTRAL));
	// a tree a player's hand threw, landed dead
	const auto dead = registry.Create();
	registry.Assign<DeadTree>(dead, DeadTree {.type = TreeInfo::Pine, .owner = PlayerNames::PLAYER_ONE});
	EXPECT_EQ(HarmedPlayerOf(dead), is(PlayerNames::PLAYER_ONE));
	// a town's spell icon is its town's player's, no one's once it has no town
	const auto town = MakeTown(PlayerNames::PLAYER_TWO);
	const auto townIcon = registry.Create();
	registry.Assign<SpellIcon>(townIcon).player = PlayerNames::PLAYER_TWO;
	registry.Assign<TownCentreSpellIcon>(townIcon).town = town;
	EXPECT_EQ(HarmedPlayerOf(townIcon), is(PlayerNames::PLAYER_TWO));
	registry.Get<TownCentreSpellIcon>(townIcon).town = entt::null;
	EXPECT_FALSE(HarmedPlayerOf(townIcon).has_value());
	// a worship site's icon is its site's player's, the neutral player's without a site
	const auto siteIcon = registry.Create();
	registry.Assign<SpellIcon>(siteIcon).player = PlayerNames::PLAYER_FOUR;
	registry.Assign<WorshipSpellIcon>(siteIcon).site = registry.Create();
	EXPECT_EQ(HarmedPlayerOf(siteIcon), is(PlayerNames::PLAYER_FOUR));
	registry.Get<WorshipSpellIcon>(siteIcon).site = entt::null;
	EXPECT_EQ(HarmedPlayerOf(siteIcon), is(PlayerNames::NEUTRAL));
	// a pile of no store: its own player
	const auto pile = registry.Create();
	registry.Assign<Pot>(pile).owner = PlayerNames::PLAYER_FIVE;
	EXPECT_EQ(HarmedPlayerOf(pile), is(PlayerNames::PLAYER_FIVE));
}

TEST_F(VillagerDeathTest, EveryLivingThingAndNothingElseIsCrushedAboveAHundredth)
{
	auto& registry = Reg();
	const auto animal = registry.Create();
	registry.Assign<Animal>(animal).type = AnimalInfo::Sheep;
	registry.Assign<Transform>(animal, glm::vec3(100.0f, 0.0f, 130.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto rock = registry.Create();
	registry.Assign<MobileObject>(rock, MobileObjectInfo::Ball);
	registry.Assign<Transform>(rock, glm::vec3(120.0f, 0.0f, 130.0f), glm::mat3(1.0f), glm::vec3(1.0f));

	// a crush of exactly a hundredth crushes nothing
	auto hundredth = Crushing(0.01f);
	ecs::effects::ApplyEffect(animal, hundredth);
	EXPECT_TRUE(CrushedReactionsOf(animal).empty());
	// just above it an animal is crushed, as a villager is, for the neutral player
	auto more = Crushing(0.02f);
	ecs::effects::ApplyEffect(animal, more);
	const auto crushed = CrushedReactionsOf(animal);
	ASSERT_EQ(crushed.size(), 1u);
	EXPECT_EQ(crushed.front().player, PlayerNames::NEUTRAL);
	// one that has started a reaction already is not crushed again
	auto again = Crushing(0.5f);
	ecs::effects::ApplyEffect(animal, again);
	EXPECT_EQ(CrushedReactionsOf(animal).size(), 1u);
	// a thing that is not living is never crushed
	auto onTheRock = Crushing(0.5f);
	ecs::effects::ApplyEffect(rock, onTheRock);
	EXPECT_TRUE(CrushedReactionsOf(rock).empty());
	// a miracle's wolf is crushed for its spell's player
	const auto wolf = registry.Create();
	auto& wolfAnimal = registry.Assign<Animal>(wolf);
	wolfAnimal.type = AnimalInfo::SpellWolf;
	wolfAnimal.player = static_cast<int32_t>(PlayerNames::PLAYER_TWO);
	registry.Assign<Transform>(wolf, glm::vec3(140.0f, 0.0f, 130.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto onTheWolf = Crushing(0.02f);
	ecs::effects::ApplyEffect(wolf, onTheWolf);
	const auto wolfCrushed = CrushedReactionsOf(wolf);
	ASSERT_EQ(wolfCrushed.size(), 1u);
	EXPECT_EQ(wolfCrushed.front().player, std::optional(PlayerNames::PLAYER_TWO));
	// with an applier (the casting creature) the reaction is the applier's, still for the crushed thing's player
	const auto goat = registry.Create();
	registry.Assign<Animal>(goat).type = AnimalInfo::Sheep;
	registry.Assign<Transform>(goat, glm::vec3(160.0f, 0.0f, 130.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto caster = registry.Create();
	registry.Assign<Transform>(caster, glm::vec3(170.0f, 0.0f, 130.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto byTheCaster = Crushing(0.02f);
	byTheCaster.appliedBy = caster;
	ecs::effects::ApplyEffect(goat, byTheCaster);
	EXPECT_TRUE(CrushedReactionsOf(goat).empty());
	const auto casterCrushed = CrushedReactionsOf(caster);
	ASSERT_EQ(casterCrushed.size(), 1u);
	EXPECT_EQ(casterCrushed.front().player, std::optional(PlayerNames::NEUTRAL));
}

namespace
{
/// An altar's mesh
constexpr entt::id_type k_AltarMesh = 77;

/// The particles as a sacrifice sees them: each spot visual started and the player each is given
class AltarLightParticles final: public test::InertParticleSystem
{
public:
	struct Started
	{
		SpotVisualType type;
		glm::vec3 position;
		std::optional<int> turns;
		entt::entity owner;
		float magnitude;
	};
	std::vector<Started> started;
	std::vector<std::pair<entt::entity, int>> players;

	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> turns, entt::entity owner,
	                             float magnitude) override
	{
		started.push_back({type, position, turns, owner, magnitude});
		return type == SpotVisualType::ObjectAppear ? k_Visual : entt::entity {entt::null};
	}
	void SetSpotVisualPlayer(entt::entity visual, int player) override { players.emplace_back(visual, player); }

	/// The object an object-appear visual it starts is (no other is made)
	static constexpr entt::entity k_Visual {7};
};
} // namespace

// A villager given to a worship site's altar is sacrificed, and a light rises from the top of the altar: the
// object-appear spot visual at full strength and with no owner, at the altar's point raised by its height, given the
// site's player (docs/wiki/magic/worship.md#a-sacrifice-at-the-altar)
TEST_F(VillagerDeathTest, AVillagerSacrificedAtTheAltarRaisesALightFromTheTopOfTheAltarForTheSitesPlayer)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	const test::RestoreService<Locator::meshBoxProvider> restoreMeshBoxes;
	auto& particles = static_cast<AltarLightParticles&>(Locator::particleSystem::emplace<AltarLightParticles>());
	// the altar's mesh: 6 m tall
	test::FakeMeshBoxProvider meshBoxes;
	meshBoxes.meshBox = [](entt::id_type mesh) {
		return mesh == k_AltarMesh ? std::optional(AxisAlignedBoundingBox {{-2.0f, 0.0f, -2.0f}, {2.0f, 6.0f, 2.0f}})
		                           : std::nullopt;
	};
	Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>(meshBoxes);

	auto& registry = Reg();
	// player two's built site, its altar standing 20 m up
	const auto site = registry.Create();
	registry.Assign<WorshipSite>(site).player = PlayerNames::PLAYER_TWO;
	registry.Assign<CitadelPartBuild>(site);
	const auto altar = registry.Create();
	registry.Assign<Transform>(altar, glm::vec3(100.0f, 20.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mesh>(altar, k_AltarMesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
	registry.Assign<WorshipTotem>(altar, site);

	// given by player one, with no hand at this machine
	const auto villager = MakeVillager();
	EXPECT_EQ(ecs::altar_offering::Offer(altar, villager, PlayerNames::PLAYER_ONE, std::nullopt),
	          ecs::altar_offering::Answer::Sacrificed);
	// one light, whatever else the death starts
	std::vector<AltarLightParticles::Started> lights;
	std::ranges::copy_if(particles.started, std::back_inserter(lights),
	                     [](const auto& visual) { return visual.type == SpotVisualType::ObjectAppear; });
	ASSERT_EQ(lights.size(), 1u);
	const auto& light = lights.front();
	EXPECT_FLOAT_EQ(light.position.x, 100.0f);
	EXPECT_FLOAT_EQ(light.position.y, 26.0f);
	EXPECT_FLOAT_EQ(light.position.z, 200.0f);
	EXPECT_EQ(light.turns, std::nullopt);
	EXPECT_EQ(light.owner, entt::entity {entt::null});
	EXPECT_FLOAT_EQ(light.magnitude, 1.0f);
	const auto given = std::ranges::find_if(particles.players,
	                                        [](const auto& player) { return player.first == AltarLightParticles::k_Visual; });
	ASSERT_NE(given, particles.players.end());
	EXPECT_EQ(given->second, static_cast<int>(PlayerNames::PLAYER_TWO));
}

// An animal given to a worship site's altar counts one sacrifice in its giver's game statistics, as a villager's death
// as a sacrifice does; a tree counts none (docs/wiki/magic/worship.md#a-sacrifice-at-the-altar)
TEST_F(VillagerDeathTest, AnAnimalSacrificedAtTheAltarCountsOneSacrificeForItsGiverAndATreeNone)
{
	auto& registry = Reg();
	// player two's site, not built: an animal and a tree are taken all the same
	const auto site = registry.Create();
	registry.Assign<WorshipSite>(site).player = PlayerNames::PLAYER_TWO;
	const auto altar = registry.Create();
	registry.Assign<Transform>(altar, glm::vec3(100.0f, 20.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<WorshipTotem>(altar, site);
	const auto sacrificesOf = [](PlayerNames player) { return game_stats::Of(static_cast<size_t>(player)).sacrifices; };
	ASSERT_EQ(sacrificesOf(PlayerNames::PLAYER_ONE), 0u);

	const auto animal = registry.Create();
	registry.Assign<Transform>(animal, glm::vec3(102.0f, 20.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Animal>(animal).type = AnimalInfo::Sheep;
	EXPECT_EQ(ecs::altar_offering::Offer(altar, animal, PlayerNames::PLAYER_ONE, std::nullopt),
	          ecs::altar_offering::Answer::Sacrificed);
	// the giver's, not the site's player's
	EXPECT_EQ(sacrificesOf(PlayerNames::PLAYER_ONE), 1u);
	EXPECT_EQ(sacrificesOf(PlayerNames::PLAYER_TWO), 0u);

	const auto tree = registry.Create();
	registry.Assign<Transform>(tree, glm::vec3(104.0f, 20.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Tree>(tree);
	EXPECT_EQ(ecs::altar_offering::Offer(altar, tree, PlayerNames::PLAYER_ONE, std::nullopt),
	          ecs::altar_offering::Answer::Sacrificed);
	EXPECT_EQ(sacrificesOf(PlayerNames::PLAYER_ONE), 1u);
}

TEST(VillagerSoulList, TheSoulsAreListedNewestFirstAndLeaveTheListWhenFreed)
{
	// a registry and the modules' state of their own; with no clip loaded a soul lasts no time
	const bool registry = Locator::entitiesRegistry::has_value();
	const bool effects = Locator::worldEffects::has_value();
	if (!registry)
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
	}
	if (!effects)
	{
		Locator::worldEffects::emplace<ecs::systems::WorldEffects>();
	}
	auto& entities = Locator::entitiesRegistry::value();
	const auto villager = entities.Create();
	entities.Assign<ecs::components::Transform>(villager, glm::vec3(1.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto before = soul::NewestFirst().size();
	soul::Create(villager, MeshId::Dummy, false);
	const auto first = soul::NewestFirst().front();
	soul::Create(villager, MeshId::Dummy, false);
	const auto souls = soul::NewestFirst();
	ASSERT_EQ(souls.size(), before + 2);
	// the second made comes first
	EXPECT_NE(souls[0], first);
	EXPECT_EQ(souls[1], first);
	soul::Update(1);
	EXPECT_EQ(soul::NewestFirst().size(), before);
	// with the land's entities, the list goes: none is left to name a new land's entity
	soul::Create(villager, MeshId::Dummy, false);
	soul::Clear();
	EXPECT_TRUE(soul::NewestFirst().empty());
	if (!effects)
	{
		Locator::worldEffects::reset();
	}
	if (!registry)
	{
		Locator::entitiesRegistry::reset();
	}
}
