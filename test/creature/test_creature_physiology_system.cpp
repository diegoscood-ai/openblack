/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature physiology system on a registry of its own: the body's turns by the time scale, eating and finishing an
// action, a poo dropped as a lump of poo on the synced stream, sick sprayed as drops that are no entity and draw on no
// game stream, and the night from the day / night clock

#define LOCATOR_IMPLEMENTATIONS

#include <cstring>

#include <numbers>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/DayNightClock.h"
#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureMorph.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/ScriptHeld.h"
#include "ECS/Components/Transform.h"
#include "ECS/Systems/Implementations/CreaturePhysiologySystem.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "creature/CreatureSystemFakes.h"
#include "creature/CreatureSystemWorld.h"
#include "support/FireFakes.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreaturePhysiologySystem;

namespace
{
class CreaturePhysiologySystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		// noon, as a land opens
		Locator::dayNightClock::emplace<ecs::systems::DayNightClockSystem>().Clock().Reset();
		// no mind: the body goes by no action
		Locator::creatureMindSystem::reset();
		// a year of the body's life an hour, and a species that, nearly rested, sleeps on for 100 x its size x the day's
		// length x 5 turns
		auto& ape = _world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe));
		ape.secondsPerAgeTick = 3600;
		ape.secondsToDehydrate = 5000.0f;
		ape.sleepLength = 100.0f;
		ape.foodToEnergy = 1000.0f;
		ape.startEnergy = 0.5f;
	}
	void TearDown() override { test::ResetMapAndVillagerDefaults(); }

	static CreatureNeeds& NeedsOf(entt::entity creature)
	{
		return test::creature_world::World::Registry().Get<CreatureNeeds>(creature);
	}
	static size_t Things() { return std::as_const(test::creature_world::World::Registry()).Size<Transform>(); }

	test::creature_world::World _world;
	const test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	const test::RestoreService<Locator::dayNightClock> _restoreClock;
	const test::RestoreService<Locator::creatureMindSystem> _restoreMind;
	CreaturePhysiologySystem _system;
};
} // namespace

TEST_F(CreaturePhysiologySystemTest, TheBodysTurnsFollowTheTimeScale)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_system.SetTimeScale(0.5f);
	_system.ProcessTurn();
	EXPECT_TRUE(NeedsOf(creature).started);
	EXPECT_EQ(NeedsOf(creature).needs.turns, 0u);
	_system.ProcessTurn();
	EXPECT_EQ(NeedsOf(creature).needs.turns, 1u);
	_system.SetTimeScale(2.0f);
	_system.ProcessTurn();
	EXPECT_EQ(NeedsOf(creature).needs.turns, 3u);
	// within what can be run in a turn
	_system.SetTimeScale(-1.0f);
	EXPECT_FLOAT_EQ(_system.GetTimeScale(), 0.0f);
	_system.SetTimeScale(1e9f);
	EXPECT_FLOAT_EQ(_system.GetTimeScale(), 3600.0f);
	_system.SetFaintingEnabled(false);
	EXPECT_FALSE(_system.IsFaintingEnabled());
}

TEST_F(CreaturePhysiologySystemTest, EatingAndFinishingAnAction)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_system.Eat(creature, 100.0f);
	EXPECT_TRUE(NeedsOf(creature).started);
	EXPECT_EQ(NeedsOf(creature).needs.meals, 1u);
	EXPECT_GT(NeedsOf(creature).needs.energy, 0.5f);

	auto& row = _world.Info().creatureAction.at(3);
	constexpr char k_Name[] = "HardWork";
	std::memcpy(row.name.data(), k_Name, sizeof(k_Name));
	row.exhaustionCost = 0.25f;
	row.strengthGain = 0.1f;
	const auto exhaustion = NeedsOf(creature).needs.exhaustion;
	const auto strength = test::creature_world::World::Registry().Get<Creature>(creature).strength;
	_system.FinishAction(creature, "HardWork");
	EXPECT_GT(NeedsOf(creature).needs.exhaustion, exhaustion);
	EXPECT_GT(test::creature_world::World::Registry().Get<Creature>(creature).strength, strength);
	// an action the table does not name costs nothing
	const auto after = NeedsOf(creature).needs.exhaustion;
	_system.FinishAction(creature, "Nothing");
	EXPECT_FLOAT_EQ(NeedsOf(creature).needs.exhaustion, after);
}

TEST_F(CreaturePhysiologySystemTest, APooIsALumpOfPooTurnedOnTheSyncedStream)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_world.teleported.clear();
	std::vector<float> floatDraws;
	int draws = 0;
	game_random::testing::SetGameRand(
	    [&draws](uint32_t) {
		    ++draws;
		    return 0u;
	    },
	    [&floatDraws](float x) {
		    floatDraws.push_back(x);
		    return 1.0f;
	    });
	const auto before = Things();
	_system.Poo(creature);
	ASSERT_EQ(floatDraws.size(), 1u);
	EXPECT_FLOAT_EQ(floatDraws.front(), 2.0f * std::numbers::pi_v<float>);
	EXPECT_EQ(draws, 0);
	ASSERT_EQ(Things(), before + 1);
	ASSERT_EQ(_world.teleported.size(), 1u);
	const auto poo = _world.teleported.front();
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	ASSERT_TRUE(registry.AllOf<MobileObject>(poo));
	EXPECT_EQ(registry.Get<MobileObject>(poo).type, MobileObjectInfo::LumpOfPoo);
	EXPECT_EQ(registry.Get<Transform>(poo).rotation, affine::AngleY(1.0f));
}

TEST_F(CreaturePhysiologySystemTest, SickIsDropsOfItsOwnThatGoAfterFourSeconds)
{
	const game_random::testing::ScopedState state;
	const auto creature = test::creature_world::World::MakeCreature();
	const auto seeds = game_random::Current();
	const auto crt = game_random::crt::Seed();
	const auto before = Things();

	_system.Puke(creature);
	ASSERT_EQ(_system.GetPukeDrops().size(), 12u);
	EXPECT_EQ(Things(), before);
	EXPECT_EQ(game_random::Current().synced, seeds.synced);
	EXPECT_EQ(game_random::Current().local, seeds.local);
	EXPECT_EQ(game_random::crt::Seed(), crt);
	// thrown forwards and up from the mouth: the creature faces -z
	for (const auto& drop : _system.GetPukeDrops())
	{
		EXPECT_LT(drop.velocity.z, 0.0f);
		EXPECT_GT(drop.velocity.y, 0.0f);
	}

	for (int second = 0; second < 3; ++second)
	{
		_system.Update(1.0f);
	}
	ASSERT_EQ(_system.GetPukeDrops().size(), 12u);
	// on the land by now, and fading
	EXPECT_FLOAT_EQ(_system.GetPukeDrops().front().position.y, 0.0f);
	EXPECT_LT(_system.GetPukeDrops().front().tint.a, 1.0f);
	_system.Update(1.0f);
	EXPECT_TRUE(_system.GetPukeDrops().empty());
}

TEST_F(CreaturePhysiologySystemTest, TheNightKeepsARestedCreatureAsleep)
{
	auto& clock = Locator::dayNightClock::value();
	const auto creature = test::creature_world::World::MakeCreature();
	auto& needs = NeedsOf(creature);
	needs.rest = CreatureNeeds::Rest::Asleep;
	needs.restTurns = 60;
	_system.ProcessTurn();
	// fully rested after the fifty turns it must sleep: in the day it may wake
	EXPECT_TRUE(NeedsOf(creature).rested);

	clock.Clock().SetScriptTime(0.0f);
	ASSERT_TRUE(clock.Clock().IsVisualNight());
	NeedsOf(creature).rested = false;
	_system.ProcessTurn();
	EXPECT_FALSE(NeedsOf(creature).rested);
}

TEST_F(CreaturePhysiologySystemTest, ABodysNeedsAreTheSpeciesStartUntilSetAndASetIsKept)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& ape = _world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe));
	ape.startWarmth = -0.25f;
	// before its first turn: what the species starts it with, and nothing started
	const auto before = _system.NeedsOf(creature);
	ASSERT_TRUE(before.has_value());
	EXPECT_FLOAT_EQ(before->energy, 0.5f);
	EXPECT_FLOAT_EQ(before->warmth, -0.25f);
	EXPECT_FALSE(NeedsOf(creature).started);

	auto needs = *before;
	needs.exhaustion = 1.0f;
	needs.energy = 0.2f;
	_system.SetNeeds(creature, needs);
	EXPECT_TRUE(NeedsOf(creature).started);
	// its first turn keeps them rather than starting them again
	_system.SetTimeScale(0.0f);
	_system.ProcessTurn();
	EXPECT_FLOAT_EQ(NeedsOf(creature).needs.exhaustion, 1.0f);
	EXPECT_FLOAT_EQ(NeedsOf(creature).needs.energy, 0.2f);
	EXPECT_FLOAT_EQ(_system.NeedsOf(creature)->exhaustion, 1.0f);
}

TEST_F(CreaturePhysiologySystemTest, OnlyACreatureHasNeeds)
{
	auto& registry = test::creature_world::World::Registry();
	const auto thing = registry.Create();
	EXPECT_FALSE(_system.NeedsOf(thing).has_value());
	EXPECT_FALSE(_system.NeedsOf(entt::null).has_value());
	_system.SetNeeds(thing, creature_physiology::Needs {});
	_system.SetNeeds(entt::null, creature_physiology::Needs {});
	EXPECT_FALSE(std::as_const(registry).AllOf<CreatureNeeds>(thing));
}

TEST_F(CreaturePhysiologySystemTest, GrowingNowTakesOneTurnsGrowthUpToFullSize)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe)).growUpMinutes = 240.0f;
	auto& registry = test::creature_world::World::Registry();
	const auto growth = _system.GrowthOf(creature);
	ASSERT_TRUE(growth.has_value());
	EXPECT_GT(*growth, creature_physiology::k_MinGrowth);
	const auto size = registry.Get<Creature>(creature).size;
	_system.GrowNow(creature);
	EXPECT_FLOAT_EQ(registry.Get<Creature>(creature).size, size + *growth);
	// made bigger by other means, it is put back to its full size, and drawn so
	registry.Get<Creature>(creature).size = 2.4f;
	_system.GrowNow(creature);
	EXPECT_EQ(registry.Get<Creature>(creature).size, creature_physiology::k_MaxGrownSize);
	EXPECT_FLOAT_EQ(
	    registry.Get<Transform>(creature).scale.x,
	    ecs::archetypes::CreatureArchetype::DrawnScale(CreatureType::GiantApe, creature_physiology::k_MaxGrownSize));
	// anything but a creature has no growth and is not grown
	const auto thing = registry.Create();
	EXPECT_FALSE(_system.GrowthOf(thing).has_value());
	EXPECT_FALSE(_system.GrowthOf(entt::null).has_value());
	_system.GrowNow(thing);
	EXPECT_FALSE(std::as_const(registry).AllOf<Creature>(thing));
}

TEST_F(CreaturePhysiologySystemTest, TheFatnessShownStepsAfterTheTurnTowardsTheFatness)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<CreatureMorph>(creature).shownFatness = 0.5f;
	registry.Get<Creature>(creature).fatness = 1.0f;
	// the body's own turn leaves the fatness shown as it was
	_system.ProcessTurn();
	EXPECT_EQ(registry.Get<CreatureMorph>(creature).shownFatness, 0.5f);
	// then, after what the creature did, it steps towards the fatness as the turn left it
	const auto fatness = registry.Get<Creature>(creature).fatness;
	_system.ProcessShownFatness();
	EXPECT_FLOAT_EQ(registry.Get<CreatureMorph>(creature).shownFatness, creature_morph::EaseFatness(0.5f, fatness));
	// a fatness set above full is shown full at most
	registry.Get<CreatureMorph>(creature).shownFatness = 0.995f;
	registry.Get<Creature>(creature).fatness = 1.5f;
	_system.ProcessShownFatness();
	EXPECT_EQ(registry.Get<CreatureMorph>(creature).shownFatness, 1.0f);
}

TEST_F(CreaturePhysiologySystemTest, TheActionItCarriesOutDecidesWhetherItGrowsAsAsleep)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe)).growUpMinutes = 240.0f;
	const auto awake = _system.GrowthOf(creature);
	ASSERT_TRUE(awake.has_value());
	// sleeping by something grows it three times as fast; asleep on the spot, its body goes by the action, not the sleep
	NeedsOf(creature).action = creature_physiology::actions::k_SleepByObject;
	EXPECT_FLOAT_EQ(*_system.GrowthOf(creature), 3.0f * *awake);
	NeedsOf(creature).action.reset();
	NeedsOf(creature).rest = CreatureNeeds::Rest::Asleep;
	EXPECT_FLOAT_EQ(*_system.GrowthOf(creature), *awake);
}

TEST_F(CreaturePhysiologySystemTest, HowLongItSleepsFollowsTheDaysLengthOnTheClock)
{
	auto& clock = Locator::dayNightClock::value();
	auto& ape = _world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe));
	ape.sleepLength = 0.01f;
	ape.sleepRecover = 0.0f;
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<Creature>(creature).size = 1.0f;
	// at night, nearly rested, it sleeps on until it has slept long enough: 85 turns of the default day
	clock.Clock().SetScriptTime(0.0f);
	ASSERT_TRUE(clock.Clock().IsVisualNight());
	EXPECT_FLOAT_EQ(*_system.LongEnoughAsleepOf(creature), 1.0f * 0.01f * 1700.0f * 5.0f);
	auto& needs = NeedsOf(creature);
	needs.started = true;
	needs.needs.exhaustion = 0.05f;
	needs.rest = CreatureNeeds::Rest::Asleep;
	needs.restTurns = 60;
	_system.ProcessTurn();
	EXPECT_FALSE(NeedsOf(creature).rested);
	// a shorter day, 50 turns
	clock.Clock().SetCycle(1000.0f, DayNightClock::k_DefaultNight, DayNightClock::k_DefaultChange);
	ASSERT_TRUE(clock.Clock().IsVisualNight());
	EXPECT_NEAR(*_system.LongEnoughAsleepOf(creature), 50.0f, 1e-3f);
	_system.ProcessTurn();
	EXPECT_TRUE(NeedsOf(creature).rested);
	EXPECT_FALSE(_system.LongEnoughAsleepOf(registry.Create()).has_value());
}

TEST_F(CreaturePhysiologySystemTest, SittingRestsItAFifthAsMuchAsSleep)
{
	_world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe)).sleepRecover = 0.01f;
	const auto creature = test::creature_world::World::MakeCreature();
	auto& needs = NeedsOf(creature);
	needs.started = true;
	needs.needs.exhaustion = 0.5f;
	// a turn sitting, as the mind tells it once a turn
	_system.Sit(creature);
	EXPECT_NEAR(NeedsOf(creature).needs.exhaustion, 0.5f - (0.2f * 0.01f), 1e-6f);
	// sitting counts no turns asleep, and the body's own turn does not rest it, sitting or not
	EXPECT_EQ(NeedsOf(creature).restTurns, 0u);
	NeedsOf(creature).sitting = true;
	_system.ProcessTurn();
	EXPECT_NEAR(NeedsOf(creature).needs.exhaustion, 0.5f - (0.2f * 0.01f), 1e-6f);
	// anything but a creature is left alone
	_system.Sit(test::creature_world::World::Registry().Create());
}

TEST_F(CreaturePhysiologySystemTest, ItGoesByTheActionItsMindCarriesOutAtItsOwnTurn)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe)).growUpMinutes = 240.0f;
	auto& mind =
	    static_cast<test::creature_fakes::FakeMind&>(Locator::creatureMindSystem::emplace<test::creature_fakes::FakeMind>());
	// the mind sleeps at home: the body takes it up at its next turn, not before
	mind.action = creature_physiology::actions::k_SleepAtHome;
	EXPECT_FALSE(NeedsOf(creature).action.has_value());
	_system.ProcessTurn();
	EXPECT_EQ(NeedsOf(creature).action, creature_physiology::actions::k_SleepAtHome);
	const auto asleep = _system.GrowthOf(creature);
	ASSERT_TRUE(asleep.has_value());
	EXPECT_GT(*asleep, 0.0f);
	NeedsOf(creature).action.reset();
	EXPECT_FLOAT_EQ(*asleep, 3.0f * *_system.GrowthOf(creature));
	NeedsOf(creature).action = creature_physiology::actions::k_SleepAtHome;
	// it stops: so does the body, at its next turn
	mind.action.reset();
	_system.ProcessTurn();
	EXPECT_FALSE(NeedsOf(creature).action.has_value());
	// without a mind it goes by no action
	NeedsOf(creature).action = creature_physiology::actions::k_SleepAtHome;
	Locator::creatureMindSystem::reset();
	_system.ProcessTurn();
	EXPECT_FALSE(NeedsOf(creature).action.has_value());
}

TEST_F(CreaturePhysiologySystemTest, OnlyACreatureNoScriptControlsAndNoComputerPlaysFaints)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	NeedsOf(creature).needs.exhaustion = 1.0f;
	_system.ProcessTurn();
	EXPECT_EQ(NeedsOf(creature).faint, creature_physiology::Faint::Exhausted);
	// under a script's control it does not
	registry.Assign<ScriptHeld>(creature).controlledByScript = true;
	_system.ProcessTurn();
	EXPECT_FALSE(NeedsOf(creature).faint.has_value());
	registry.Get<ScriptHeld>(creature).controlledByScript = false;
	_system.ProcessTurn();
	EXPECT_EQ(NeedsOf(creature).faint, creature_physiology::Faint::Exhausted);
	// nor when the computer plays its owner
	const auto player = registry.Create();
	registry.Assign<Player>(player, PlayerNames::PLAYER_ONE);
	registry.Assign<PlayerMagic>(player).playerType = 2;
	_system.ProcessTurn();
	EXPECT_FALSE(NeedsOf(creature).faint.has_value());
	// a human's creature does
	registry.Get<PlayerMagic>(player).playerType = 1;
	_system.ProcessTurn();
	EXPECT_EQ(NeedsOf(creature).faint, creature_physiology::Faint::Exhausted);
}

TEST_F(CreaturePhysiologySystemTest, DrinkingQuenchesItAndCoolsItToZero)
{
	const test::RestoreService<Locator::fireSystem> restoreFires;
	auto& fires = static_cast<test::FakeFires&>(Locator::fireSystem::emplace<test::FakeFires>());
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	NeedsOf(creature).needs.dehydration = 0.7f;
	_system.Drink(creature);
	EXPECT_FLOAT_EQ(NeedsOf(creature).needs.dehydration, 0.0f);
	ASSERT_EQ(fires.temperaturesSet.size(), 1u);
	EXPECT_EQ(fires.temperaturesSet.front().first, creature);
	EXPECT_FLOAT_EQ(fires.temperaturesSet.front().second, 0.0f);
	// anything but a creature is left alone
	_system.Drink(test::creature_world::World::Registry().Create());
	EXPECT_EQ(fires.temperaturesSet.size(), 1u);
}
