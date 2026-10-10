/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <limits>
#include <optional>

#include <gtest/gtest.h>

#include "Common/GUtilsDistance.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreaturePhysiology.h"

using namespace openblack;
using namespace openblack::creature_physiology;

namespace
{
constexpr float k_Tolerance = 1e-6f;

/// A made-up species with round numbers
Species Fake()
{
	return {.startEnergy = 1.0f,
	        .startWarmth = 0.0f,
	        .comfortTemperature = 15.0f,
	        .secondsPerAgeTick = 10,
	        .growUpMinutes = 120.0f,
	        .lowEnergyThreshold = 0.2f,
	        .exhaustionRate = 1e-3f,
	        .secondsToDehydrate = 100.0f,
	        .strengthDecay = 1.0f,
	        .carryStrengthMinutes = 5.0f,
	        .energyDrain = 0.01f,
	        .fatBurn = 0.001f,
	        .overeatFatFactor = 0.1f,
	        .sleepHeal = 0.01f,
	        .sleepRecover = 0.02f,
	        .sleepLength = 0.5f,
	        .foodToEnergy = 1000.0f,
	        .pooPerEnergy = 0.5f};
}

Turn Standing(float temperature = 15.0f)
{
	return {.moving = false,
	        .action = std::nullopt,
	        .resting = false,
	        .phase = 13,
	        .carriedWeight = std::nullopt,
	        .temperature = temperature,
	        .turnsPerSecond = 10.0f};
}
} // namespace

TEST(CreaturePhysiology, ANewBodyStartsFromItsSpecies)
{
	auto species = Fake();
	species.startEnergy = 0.75f;
	species.startWarmth = -0.25f;
	const auto needs = Start(species);
	EXPECT_FLOAT_EQ(needs.energy, 0.75f);
	EXPECT_FLOAT_EQ(needs.warmth, -0.25f);
	EXPECT_FLOAT_EQ(Hunger(needs), 0.25f);
	EXPECT_FLOAT_EQ(needs.life, 1.0f);
	EXPECT_EQ(needs.age, 0u);
}

TEST(CreaturePhysiology, ItAgesOncePerTickOfGameTime)
{
	auto needs = Start(Fake());
	Shape shape;
	// Ten seconds of ten turns each
	for (int i = 0; i < 99; ++i)
	{
		TickTurn(needs, shape, Fake(), Standing());
	}
	EXPECT_EQ(needs.age, 0u);
	TickTurn(needs, shape, Fake(), Standing());
	EXPECT_EQ(needs.age, 1u);
}

TEST(CreaturePhysiology, ItsAgeTickCountsWholeTurnsASecond)
{
	auto needs = Start(Fake());
	Shape shape;
	// 30 ms a turn: 33 whole turns a second, so a ten-second tick is 330 turns, not 333
	auto turn = Standing();
	turn.turnsPerSecond = 1000.0f / 30.0f;
	needs.turns = 328;
	TickTurn(needs, shape, Fake(), turn);
	EXPECT_EQ(needs.age, 0u);
	TickTurn(needs, shape, Fake(), turn);
	EXPECT_EQ(needs.turns, 330u);
	EXPECT_EQ(needs.age, 1u);
	needs.turns = 332;
	TickTurn(needs, shape, Fake(), turn);
	EXPECT_EQ(needs.age, 1u);
}

TEST(CreaturePhysiology, OnlyItsTurnsAreCountedBeforeTheBodyStage)
{
	auto needs = Start(Fake());
	Shape shape;
	auto turn = Standing();
	turn.phase = 0;
	TickTurn(needs, shape, Fake(), turn);
	EXPECT_FLOAT_EQ(needs.energy, 1.0f);
	EXPECT_EQ(needs.turns, 1u);
	EXPECT_FLOAT_EQ(shape.size, 1.0f);
}

TEST(CreaturePhysiology, TurnsBeforeTheBodyStageCountTowardsItsAge)
{
	auto needs = Start(Fake());
	Shape shape;
	auto turn = Standing();
	turn.phase = 0;
	// A tick of game time is 100 turns: one passes before the body stage and ages it nothing
	for (int i = 0; i < 199; ++i)
	{
		TickTurn(needs, shape, Fake(), turn);
	}
	EXPECT_EQ(needs.turns, 199u);
	EXPECT_EQ(needs.age, 0u);
	// The 200th turn, at the body stage, ends the second tick
	turn.phase = 1;
	TickTurn(needs, shape, Fake(), turn);
	EXPECT_EQ(needs.age, 1u);
}

TEST(CreaturePhysiology, YoungCreaturesGrowFastThenSlowly)
{
	const auto species = Fake();
	Needs needs;
	needs.energy = 1.0f;
	needs.exhaustion = 0.0f;
	// Twice as fast as one full growing up spread over its time, until 7/8 of it
	EXPECT_NEAR(Growth(needs, species, false, 10.0f), 2.0f / (120.0f * 600.0f), k_Tolerance);
	// Asleep, three times as fast
	EXPECT_NEAR(Growth(needs, species, true, 10.0f), 3.0f * 2.0f / (120.0f * 600.0f), k_Tolerance);
	// Grown up, at the slowest
	needs.age = 2;
	EXPECT_FLOAT_EQ(Growth(needs, species, false, 10.0f), k_MinGrowth);
	// Without energy to spare, at the slowest too
	needs.age = 0;
	needs.exhaustion = 1.0f;
	EXPECT_FLOAT_EQ(Growth(needs, species, false, 10.0f), k_MinGrowth);
}

TEST(CreaturePhysiology, ItGrowsStandingStillOrMovingAndUpToFullSize)
{
	const auto species = Fake();
	auto needs = Start(species);
	Shape shape {.fatness = 0.5f, .strength = 0.5f, .size = 1.0f};
	TickTurn(needs, shape, species, Standing());
	EXPECT_GT(shape.size, 1.0f);
	const auto grown = shape.size;
	// Moving, it grows by the same turn's growth
	auto moving = Standing();
	moving.moving = true;
	const auto growth = Growth(needs, species, false, 10.0f);
	TickTurn(needs, shape, species, moving);
	EXPECT_FLOAT_EQ(shape.size, grown + growth);
	const auto grownMoving = shape.size;
	// Not grown up enough to grow
	auto young = Standing();
	young.phase = 2;
	TickTurn(needs, shape, species, young);
	EXPECT_FLOAT_EQ(shape.size, grownMoving);
	shape.size = k_MaxGrownSize - 1e-6f;
	TickTurn(needs, shape, species, Standing());
	EXPECT_FLOAT_EQ(shape.size, k_MaxGrownSize);
	shape.size = 1.9999f;
	TickTurn(needs, shape, species, Standing());
	EXPECT_LE(shape.size, k_MaxGrownSize);
	EXPECT_GT(shape.size, 1.9999f);
	// Made bigger by other means, one turn puts it back to its full size, moving or not
	shape.size = 2.4f;
	TickTurn(needs, shape, species, Standing());
	EXPECT_EQ(shape.size, k_MaxGrownSize);
	shape.size = 3.0f;
	TickTurn(needs, shape, species, moving);
	EXPECT_EQ(shape.size, k_MaxGrownSize);
	// Before the stage it grows at, a bigger creature stays as it is
	shape.size = 2.4f;
	TickTurn(needs, shape, species, young);
	EXPECT_FLOAT_EQ(shape.size, 2.4f);
}

TEST(CreaturePhysiology, AGrowthStepKeepsTheSizeWithinNoneAndFullSize)
{
	Shape shape {.fatness = 0.5f, .strength = 0.5f, .size = 1.0f};
	Grow(shape, 0.25f);
	EXPECT_FLOAT_EQ(shape.size, 1.25f);
	Grow(shape, -2.0f);
	EXPECT_EQ(shape.size, 0.0f);
	shape.size = 2.4f;
	Grow(shape, 0.0f);
	EXPECT_EQ(shape.size, k_MaxGrownSize);
}

TEST(CreaturePhysiology, EnergyRunsDownSlowerForBigOrSleepingCreatures)
{
	const auto species = Fake();
	auto small = Start(species);
	Shape smallShape {.fatness = 0.5f, .strength = 0.5f, .size = 0.0f};
	TickTurn(small, smallShape, species, Standing());
	EXPECT_NEAR(small.energy, 1.0f - 0.01f, k_Tolerance);

	auto big = Start(species);
	Shape bigShape {.fatness = 0.5f, .strength = 0.5f, .size = 2.0f};
	TickTurn(big, bigShape, species, Standing());
	EXPECT_NEAR(big.energy, 1.0f - 0.005f, k_Tolerance);

	auto asleep = Start(species);
	Shape asleepShape {.fatness = 0.5f, .strength = 0.5f, .size = 0.0f};
	auto turn = Standing();
	turn.action = actions::k_SleepAtHome;
	TickTurn(asleep, asleepShape, species, turn);
	EXPECT_NEAR(asleep.energy, 1.0f - 0.0025f, k_Tolerance);

	// Resting to get better uses it as slowly; sleeping by something, or on the spot, does not
	auto resting = Start(species);
	Shape restingShape {.fatness = 0.5f, .strength = 0.5f, .size = 0.0f};
	turn.action = actions::k_RestToGetBetter;
	TickTurn(resting, restingShape, species, turn);
	EXPECT_NEAR(resting.energy, 1.0f - 0.0025f, k_Tolerance);
	auto byObject = Start(species);
	Shape byObjectShape {.fatness = 0.5f, .strength = 0.5f, .size = 0.0f};
	turn.action = actions::k_SleepByObject;
	TickTurn(byObject, byObjectShape, species, turn);
	EXPECT_NEAR(byObject.energy, 1.0f - 0.01f, k_Tolerance);
}

TEST(CreaturePhysiology, WhichActionsCountAsAsleep)
{
	constexpr uint32_t k_SleepOnTheSpot = 313;
	EXPECT_TRUE(SlowsEnergy(actions::k_SleepAtHome));
	EXPECT_TRUE(SlowsEnergy(actions::k_RestToGetBetter));
	EXPECT_FALSE(SlowsEnergy(actions::k_SleepByObject));
	EXPECT_FALSE(SlowsEnergy(k_SleepOnTheSpot));
	EXPECT_FALSE(SlowsEnergy(std::nullopt));
	EXPECT_TRUE(GrowsAsleep(actions::k_SleepAtHome));
	EXPECT_TRUE(GrowsAsleep(actions::k_SleepByObject));
	EXPECT_FALSE(GrowsAsleep(actions::k_RestToGetBetter));
	EXPECT_FALSE(GrowsAsleep(k_SleepOnTheSpot));
	EXPECT_FALSE(GrowsAsleep(std::nullopt));
}

TEST(CreaturePhysiology, ItGrowsThreeTimesAsFastSleepingAtHomeOrBySomething)
{
	const auto species = Fake();
	auto needs = Start(species);
	Shape awake {.fatness = 0.5f, .strength = 0.5f, .size = 0.5f};
	auto byObject = awake;
	auto turn = Standing();
	TickTurn(needs, awake, species, turn);
	auto asleep = Start(species);
	turn.action = actions::k_SleepByObject;
	TickTurn(asleep, byObject, species, turn);
	EXPECT_NEAR(byObject.size - 0.5f, 3.0f * (awake.size - 0.5f), k_Tolerance);
}

TEST(CreaturePhysiology, HungryCreaturesBurnFat)
{
	const auto species = Fake();
	auto needs = Start(species);
	Shape shape {.fatness = 0.5f, .strength = 0.5f, .size = 0.0f};
	needs.energy = 0.6f;
	TickTurn(needs, shape, species, Standing());
	EXPECT_FLOAT_EQ(shape.fatness, 0.5f);
	needs.energy = 0.4f;
	TickTurn(needs, shape, species, Standing());
	EXPECT_NEAR(shape.fatness, 0.499f, k_Tolerance);
}

TEST(CreaturePhysiology, MovingTiresYoungAndHungryCreaturesFaster)
{
	const auto species = Fake();
	auto turn = Standing();
	turn.moving = true;
	Shape shape {.fatness = 0.5f, .strength = 0.5f, .size = 0.0f};

	auto young = Start(species);
	TickTurn(young, shape, species, turn);
	EXPECT_NEAR(young.exhaustion, 4.0f * 1e-3f, k_Tolerance);

	auto old = Start(species);
	old.age = 40;
	TickTurn(old, shape, species, turn);
	EXPECT_NEAR(old.exhaustion, 1e-3f, k_Tolerance);

	auto hungry = Start(species);
	hungry.age = 40;
	hungry.energy = 0.1f;
	TickTurn(hungry, shape, species, turn);
	EXPECT_NEAR(hungry.exhaustion, 1.3f * 1e-3f, k_Tolerance);

	auto still = Start(species);
	TickTurn(still, shape, species, Standing());
	EXPECT_FLOAT_EQ(still.exhaustion, 0.0f);
}

TEST(CreaturePhysiology, ThirstBuildsUpOnceGrownEnough)
{
	const auto species = Fake();
	auto needs = Start(species);
	Shape shape;
	TickTurn(needs, shape, species, Standing());
	EXPECT_NEAR(needs.dehydration, 1.0f / 1000.0f, k_Tolerance);
	Drink(needs);
	EXPECT_FLOAT_EQ(needs.dehydration, 0.0f);
}

TEST(CreaturePhysiology, TheTurnsToDehydrateAreCutToWholeNumbers)
{
	auto species = Fake();
	Shape shape;
	// 10 turns a second x 100.06 s is 1000.6 turns, cut to 1000
	species.secondsToDehydrate = 100.06f;
	auto needs = Start(species);
	TickTurn(needs, shape, species, Standing());
	EXPECT_FLOAT_EQ(needs.dehydration, 1.0f / 1000.0f);
	// 33.3 turns a second count as 33: 3300 turns
	species.secondsToDehydrate = 100.0f;
	needs = Start(species);
	auto turn = Standing();
	turn.turnsPerSecond = 1000.0f / 30.0f;
	TickTurn(needs, shape, species, turn);
	EXPECT_FLOAT_EQ(needs.dehydration, 1.0f / 3300.0f);
	// under a whole turn to dehydrate in, it is parched at once
	species.secondsToDehydrate = 0.05f;
	needs = Start(species);
	TickTurn(needs, shape, species, Standing());
	EXPECT_FLOAT_EQ(needs.dehydration, 1.0f);
	// a product out of the int32 range, or not a number, counts as INT32_MIN: 2^31 turns
	for (const auto seconds : {1.0e30f, -1.0e30f, std::numeric_limits<float>::quiet_NaN()})
	{
		species.secondsToDehydrate = seconds;
		needs = Start(species);
		TickTurn(needs, shape, species, Standing());
		EXPECT_FLOAT_EQ(needs.dehydration, 1.0f / 2147483648.0f);
	}
	// a negative one wraps round as an unsigned number: -5 turns are 2^32 - 5
	species.secondsToDehydrate = -0.5f;
	needs = Start(species);
	TickTurn(needs, shape, species, Standing());
	EXPECT_FLOAT_EQ(needs.dehydration, 1.0f / 4294967291.0f);
	// not before it has grown up enough
	needs = Start(species);
	turn = Standing();
	turn.phase = k_GrowingPhase - 1;
	TickTurn(needs, shape, species, turn);
	EXPECT_FLOAT_EQ(needs.dehydration, 0.0f);
}

TEST(CreaturePhysiology, WarmthFollowsTheSigmoidOfTheTemperature)
{
	const auto species = Fake();
	Shape shape;
	// 40 degrees too hot is a whole step of the table past its threshold
	auto hot = Start(species);
	TickTurn(hot, shape, species, Standing(55.0f));
	EXPECT_FLOAT_EQ(hot.warmth, gutils::SigmoidThreshold(0.6f, 1.0f));
	EXPECT_GT(hot.warmth, 0.99f);
	// 24 degrees too cold is right at the threshold, half a step
	auto cold = Start(species);
	TickTurn(cold, shape, species, Standing(-9.0f));
	EXPECT_NEAR(cold.warmth, -0.5f, 1e-3f);
	// At its comfort, it drifts ever so slightly colder
	auto comfortable = Start(species);
	TickTurn(comfortable, shape, species, Standing(15.0f));
	EXPECT_LE(comfortable.warmth, 0.0f);
	EXPECT_GT(comfortable.warmth, -1e-4f);
}

TEST(CreaturePhysiology, CarryingMakesItStronger)
{
	const auto species = Fake();
	auto needs = Start(species);
	Shape shape {.fatness = 0.5f, .strength = 0.5f, .size = 1.0f};
	auto turn = Standing();
	turn.moving = true;
	turn.carriedWeight = 2.0f;
	TickTurn(needs, shape, species, turn);
	// Half its strength in five minutes of carrying its own weight or more
	EXPECT_NEAR(shape.strength, 0.5f + (0.5f / 3000.0f), k_Tolerance);
}

TEST(CreaturePhysiology, ActionsCostEnergyAndBuildStrength)
{
	auto needs = Start(Fake());
	Shape shape {.fatness = 0.5f, .strength = 0.5f, .size = 1.0f};
	ApplyActionCost(needs, shape, {.strengthGain = 0.012f, .energyCost = 0.03f, .exhaustionCost = 0.16f}, 13);
	EXPECT_NEAR(shape.strength, 0.512f, k_Tolerance);
	EXPECT_NEAR(needs.energy, 1.0f - 0.015f, k_Tolerance);
	EXPECT_NEAR(needs.exhaustion, 0.08f, k_Tolerance);
}

TEST(CreaturePhysiology, EatingFillsItUpFattensAndBuildsPoo)
{
	const auto species = Fake();
	auto needs = Start(species);
	needs.energy = 0.5f;
	Shape shape {.fatness = 0.5f, .strength = 0.5f, .size = 1.0f};
	// A creature of size 0.8 or more counts as 0.8 for a meal: 250 food is 250 / 800 of a meal
	const auto gained = Eat(needs, shape, species, 250.0f);
	EXPECT_FLOAT_EQ(gained, 0.3125f);
	EXPECT_FLOAT_EQ(needs.energy, 0.8125f);
	EXPECT_FLOAT_EQ(shape.fatness, 0.5f);
	EXPECT_FLOAT_EQ(needs.poo, 0.15625f);
	EXPECT_EQ(needs.meals, 1u);

	// A big meal when full: energy up to its size, and the excess goes to fat
	needs.energy = 1.0f;
	shape.size = 1.5f;
	EXPECT_FLOAT_EQ(Eat(needs, shape, species, 1500.0f), 1.875f);
	EXPECT_FLOAT_EQ(needs.energy, 1.5f);
	EXPECT_NEAR(shape.fatness, 0.6875f, k_Tolerance);
	// Smaller creatures fill up on less
	needs.energy = 0.0f;
	shape.size = 0.2f;
	EXPECT_FLOAT_EQ(Eat(needs, shape, species, 100.0f), 0.5f);
	EXPECT_FLOAT_EQ(needs.energy, 0.5f);
	Poo(needs);
	EXPECT_FLOAT_EQ(needs.poo, 0.0f);
}

TEST(CreaturePhysiology, SleepHealsAndRestsThenWakes)
{
	const auto species = Fake();
	auto needs = Start(species);
	needs.exhaustion = 0.5f;
	needs.life = 0.5f;
	constexpr float k_Day = 1700.0f;
	EXPECT_FALSE(SleepTurn(needs, species, 1.0f, 1, false, k_Day));
	EXPECT_NEAR(needs.exhaustion, 0.48f, k_Tolerance);
	EXPECT_NEAR(needs.life, 0.51f, k_Tolerance);
	// Rested, it still sleeps out its first turns
	needs.exhaustion = 0.0f;
	EXPECT_FALSE(SleepTurn(needs, species, 1.0f, k_MinSleepTurns, false, k_Day));
	EXPECT_TRUE(SleepTurn(needs, species, 1.0f, k_MinSleepTurns + 1, false, k_Day));
	// Nearly rested, it wakes even at night once it has slept long enough for its size and the day's length
	needs.exhaustion = 0.09f;
	EXPECT_FALSE(SleepTurn(needs, species, 1.0f, k_MinSleepTurns + 1, true, k_Day));
	needs.exhaustion = 0.09f;
	EXPECT_FALSE(SleepTurn(needs, species, 1.0f, 4249, true, k_Day));
	needs.exhaustion = 0.09f;
	EXPECT_TRUE(SleepTurn(needs, species, 1.0f, 4250, true, k_Day));
	// Still tired, it sleeps on
	needs.exhaustion = 0.5f;
	EXPECT_FALSE(SleepTurn(needs, species, 1.0f, 10000, false, k_Day));
}

TEST(CreaturePhysiology, HowLongItSleepsFollowsItsSizeAndTheDaysLength)
{
	auto species = Fake();
	species.sleepLength = 0.1f;
	// A size 2 creature of sleep length 0.1 sleeps 1700 turns of the default day
	EXPECT_FLOAT_EQ(LongEnoughAsleep(species, 2.0f, 1700.0f), 1700.0f);
	EXPECT_FLOAT_EQ(LongEnoughAsleep(species, 1.0f, 1700.0f), 850.0f);
	EXPECT_FLOAT_EQ(LongEnoughAsleep(species, 2.0f, 850.0f), 850.0f);
}

TEST(CreaturePhysiology, SittingRestsAFifthAsMuchAsSleep)
{
	const auto species = Fake();
	auto needs = Start(species);
	needs.exhaustion = 0.5f;
	needs.life = 0.5f;
	SitTurn(needs, species);
	EXPECT_NEAR(needs.exhaustion, 0.5f - (0.2f * 0.02f), k_Tolerance);
	// It does not heal
	EXPECT_FLOAT_EQ(needs.life, 0.5f);
	needs.exhaustion = 0.001f;
	SitTurn(needs, species);
	EXPECT_FLOAT_EQ(needs.exhaustion, 0.0f);
}

TEST(CreaturePhysiology, FaintingOnlyForGrownUpOwnedCreatures)
{
	Needs needs;
	needs.exhaustion = 1.0f;
	const FaintGates owned {.ownedByPlayer = true};
	EXPECT_EQ(ShouldFaint(needs, 13, owned), Faint::Exhausted);
	EXPECT_FALSE(ShouldFaint(needs, 13, {}).has_value());
	EXPECT_FALSE(ShouldFaint(needs, 4, owned).has_value());
	EXPECT_EQ(ShouldFaint(needs, k_FaintingPhase, owned), Faint::Exhausted);
	// a computer player's creature, and one a script controls, never faint
	EXPECT_FALSE(ShouldFaint(needs, 13, {.ownedByPlayer = true, .computerPlayer = true}).has_value());
	EXPECT_FALSE(ShouldFaint(needs, 13, {.ownedByPlayer = true, .scriptControlled = true}).has_value());
	needs.energy = 0.0f;
	EXPECT_EQ(ShouldFaint(needs, 13, owned), Faint::Starving);
	needs.life = 0.0f;
	EXPECT_EQ(ShouldFaint(needs, 13, owned), Faint::OutOfLife);
	EXPECT_FALSE(ShouldFaint(needs, 13, {.ownedByPlayer = true, .scriptControlled = true}).has_value());
	WakeFromFaint(needs);
	EXPECT_FLOAT_EQ(needs.exhaustion, 0.9f);
	EXPECT_FLOAT_EQ(needs.energy, 0.1f);
}

TEST(CreaturePhysiology, TheBodyDrivesItsDesireSources)
{
	namespace sources = creature_desires::sources;
	Needs needs;
	needs.energy = 0.3f;
	needs.poo = 0.4f;
	needs.exhaustion = 0.5f;
	needs.dehydration = 0.6f;
	needs.life = 0.25f;
	needs.warmth = -0.5f;
	needs.itchiness = 0.2f;
	EXPECT_FLOAT_EQ(*SourceValue(sources::k_HungerFromLowEnergy, needs, false), 0.7f);
	EXPECT_FLOAT_EQ(*SourceValue(sources::k_PooFromAmountOfPoo, needs, false), 0.4f);
	EXPECT_FLOAT_EQ(*SourceValue(sources::k_TirednessFromExhaustion, needs, false), 0.5f);
	EXPECT_FLOAT_EQ(*SourceValue(sources::k_TirednessFromNight, needs, true), 1.0f);
	EXPECT_FLOAT_EQ(*SourceValue(sources::k_FearFromDark, needs, false), 0.0f);
	EXPECT_FLOAT_EQ(*SourceValue(sources::k_WaterFromDehydration, needs, false), 0.6f);
	EXPECT_FLOAT_EQ(*SourceValue(sources::k_RestoreHealthFromLife, needs, false), 0.75f);
	EXPECT_FLOAT_EQ(*SourceValue(sources::k_GetWarmer, needs, false), 0.5f);
	EXPECT_FLOAT_EQ(*SourceValue(sources::k_GetColder, needs, false), 0.0f);
	EXPECT_FLOAT_EQ(*SourceValue(sources::k_Scratch, needs, false), 0.2f);
	EXPECT_FALSE(SourceValue(sources::k_Sadness, needs, false).has_value());
}

TEST(CreaturePhysiology, LeftAloneItsBodyDrivesItsHungerThirstAndPoo)
{
	namespace sources = creature_desires::sources;
	using creature_desires::Desire;
	const auto species = Fake();
	auto needs = Start(species);
	needs.poo = 0.6f;
	Shape shape {.fatness = 0.5f, .strength = 0.5f, .size = 1.0f};

	// Each need's desire grows from its source once the source is well past its threshold
	std::array<creature_desires::DesireSetup, creature_desires::k_DesireCount> setup {};
	const auto need = [&setup](Desire desire, uint32_t source) {
		setup.at(static_cast<size_t>(desire)) = {.max = 2.0f,
		                                         .decayMin = 0.9f,
		                                         .decayMax = 0.9f,
		                                         .increaseSeconds = 2.0f,
		                                         .sources = {{.type = source, .threshold = 0.5f}}};
	};
	need(Desire::Hunger, sources::k_HungerFromLowEnergy);
	need(Desire::Water, sources::k_WaterFromDehydration);
	need(Desire::Poo, sources::k_PooFromAmountOfPoo);
	auto desires = creature_desires::Create(setup, [](float low, float) { return low; });
	for (const auto desire : {Desire::Hunger, Desire::Water, Desire::Poo})
	{
		desires[desire].activated = true;
	}
	const auto read = [&needs](uint32_t type, const creature_desires::Desires&) { return SourceValue(type, needs, false); };

	// Nothing holds the body: a second of turns at a time, it runs down by itself and its desires follow
	constexpr float k_ActOnNeed = 0.3f;
	std::optional<float> hungryAtEnergy;
	std::optional<float> thirstyAtDehydration;
	bool wantsToPoo = false;
	for (int turn = 0; turn < 1000; ++turn)
	{
		TickTurn(needs, shape, species, Standing());
		creature_desires::UpdateSources(desires, read);
		creature_desires::UpdateDesires(desires, 10.0f);
		if (!hungryAtEnergy.has_value() && desires[Desire::Hunger].value >= k_ActOnNeed)
		{
			hungryAtEnergy = needs.energy;
		}
		if (!thirstyAtDehydration.has_value() && desires[Desire::Water].value >= k_ActOnNeed)
		{
			thirstyAtDehydration = needs.dehydration;
		}
		wantsToPoo = wantsToPoo || desires[Desire::Poo].value >= k_ActOnNeed;
	}
	// Hungry enough to eat while it still has some energy, but not while it is full; the same for thirst
	ASSERT_TRUE(hungryAtEnergy.has_value());
	EXPECT_GT(*hungryAtEnergy, 0.0f);
	EXPECT_LT(*hungryAtEnergy, 0.9f);
	ASSERT_TRUE(thirstyAtDehydration.has_value());
	EXPECT_GT(*thirstyAtDehydration, 0.1f);
	EXPECT_LT(*thirstyAtDehydration, 1.0f);
	// The poo inside it, which only meals add to, drives its desire to go
	EXPECT_TRUE(wantsToPoo);
	EXPECT_FLOAT_EQ(needs.poo, 0.6f);
}
