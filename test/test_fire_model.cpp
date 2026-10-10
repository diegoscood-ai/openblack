/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fire's pure formulas (Fire/FireModel): how hot things get, how they burn, cool and heat each other.

#include <limits>

#include <gtest/gtest.h>

#include "Fire/FireModel.h"

using namespace openblack;
using namespace openblack::fire;

namespace
{
constexpr float k_Epsilon = 1e-4f;

/// A villager's table row: combustion 120, heat capacity 82.5, burn defence 0.5
Material Villager()
{
	return {.combustion = 120.0f,
	        .capacity = 82.5f,
	        .defence = 0.5f,
	        .burningPriority = 1.0f,
	        .radius = 0.5f,
	        .height = 1.8f,
	        .fireRadius = 0.5f};
}

/// A hut: combustion 150, heat capacity 2000, burn defence 0.01
Material Hut()
{
	return {.combustion = 150.0f,
	        .capacity = 2000.0f,
	        .defence = 0.01f,
	        .burningPriority = 0.5f,
	        .radius = 4.0f,
	        .height = 5.0f,
	        .fireRadius = 4.0f};
}

/// A tree: combustion 110, heat capacity 1000, burn defence 0.01
Material Tree()
{
	return {.combustion = 110.0f,
	        .capacity = 1000.0f,
	        .defence = 0.01f,
	        .burningPriority = 0.5f,
	        .radius = 2.0f,
	        .height = 8.0f,
	        .fireRadius = 2.0f};
}
} // namespace

TEST(FireModel, NothingCatchesBelowFortyDegreesAndHoldsAtLeastOneUnitOfHeat)
{
	Material cold {.combustion = 10.0f, .capacity = 0.0f};
	EXPECT_FLOAT_EQ(CombustionTemperature(cold), 40.0f);
	EXPECT_FLOAT_EQ(MaxTemperature(cold), 80.0f);
	// A creature's table gives a capacity that is next to nothing as a float
	cold.capacity = 2.1e-43f;
	EXPECT_FLOAT_EQ(Capacity(cold), 1.0f);
}

TEST(FireModel, BurningAtTwiceItsCombustionHurtsByItsDefence)
{
	// A villager loses a twentieth of its life a turn, a hut a thousandth
	EXPECT_NEAR(BurnDamage(240.0f, Villager()), 0.05f, k_Epsilon);
	EXPECT_NEAR(BurnDamage(300.0f, Hut()), 0.001f, k_Epsilon);
	EXPECT_FLOAT_EQ(BurnDamage(120.0f, Villager()), 0.0f);
}

TEST(FireModel, TheFireIsFiercestAtTwiceItsCombustionAndWeakAsItsObjectBurnsAway)
{
	const auto hut = Hut();
	EXPECT_FLOAT_EQ(FireFraction(120.0f, hut, 1.0f), 0.0f);
	EXPECT_FLOAT_EQ(FireFraction(300.0f, hut, 1.0f), 1.0f);
	EXPECT_NEAR(FireFraction(210.0f, hut, 1.0f), (210.0f - 120.0f) / 180.0f, k_Epsilon);
	// No more than twice the life left
	EXPECT_NEAR(FireFraction(300.0f, hut, 0.1f), 0.2f, k_Epsilon);
	EXPECT_FLOAT_EQ(FireRadius(hut, 1.0f), 5.0f);
	EXPECT_FLOAT_EQ(MaxFireRadius(hut), 5.0f);
	EXPECT_FLOAT_EQ(SafeFireRadius(2.0f, 5.0f), 3.0f);
	EXPECT_NEAR(FlameHeight(300.0f, hut), 5.0f * 1.25f, k_Epsilon);
	EXPECT_FLOAT_EQ(FlameHeight(k_AmbientTemperature, hut), 0.0f);
}

TEST(FireModel, ABurnPullsTheTemperatureTowardsTheAirsPlusTheBurn)
{
	// Water's -4000 cools something light at once, a hut by about 21 degrees a drop
	Material light {.combustion = 100.0f, .capacity = 10.0f};
	EXPECT_NEAR(ApplyBurn(300.0f, light, -4000.0f), k_AmbientTemperature - 4000.0f, 0.01f);
	EXPECT_NEAR(ApplyBurn(300.0f, Hut(), -4000.0f), 300.0f + 10.0f * (k_AmbientTemperature - 4300.0f) / 2000.0f, 0.01f);
	// A villager's beat cools a burning hut by about 1.4 degrees
	EXPECT_NEAR(300.0f - ApplyBurn(300.0f, Hut(), -8.0f), 1.4165f, 0.001f);
	// Each turn of a blast's 200 heats a villager by ten times the gap over its capacity
	EXPECT_NEAR(ApplyBurn(k_AmbientTemperature, Villager(), 200.0f), k_AmbientTemperature + 2000.0f / 82.5f, 0.01f);
	EXPECT_FLOAT_EQ(SetOnFireTemperature(Hut(), 0.5f), 300.0f * 0.5f + 150.0f);
}

TEST(FireModel, AFireHeatsItsNeighbourByTheDifferenceUpToHalfItsHeat)
{
	const auto tree = Tree();
	const auto hut = Hut();
	// Ten for each degree of difference, which changes the hut by no more than the difference
	const auto transfer = HeatTransfer(220.0f, tree, k_AmbientTemperature, hut);
	EXPECT_NEAR(transfer.targetTemperature, k_AmbientTemperature + 10.0f * (220.0f - k_AmbientTemperature) / 2000.0f, 0.01f);
	EXPECT_FLOAT_EQ(transfer.sourceTemperature, 220.0f);
	// Something hot but not burning, as a fireball below its combustion, loses what it gives
	Material ball {.combustion = 2000.0f, .capacity = 75.0f};
	const auto cooling = HeatTransfer(1500.0f, ball, k_AmbientTemperature, Villager());
	const float heat = 10.0f * (1500.0f - k_AmbientTemperature);
	EXPECT_NEAR(cooling.sourceTemperature, 1500.0f - heat / 75.0f, 0.01f);
	EXPECT_NEAR(cooling.targetTemperature, k_AmbientTemperature + heat / 82.5f, 0.01f);
	// Nothing passes to something hotter
	EXPECT_FLOAT_EQ(HeatTransfer(100.0f, tree, 200.0f, hut).targetTemperature, 200.0f);
}

TEST(FireModel, ABurningThingHeatsItselfSlowlyAndHurts)
{
	const auto hut = Hut();
	State state {.temperature = 150.0f, .previous = 140.0f};
	const auto outcome = Step(state, hut, {.life = 1.0f});
	EXPECT_FALSE(outcome.gone);
	EXPECT_NE(state.flags & k_JustIgnited, 0);
	EXPECT_NEAR(state.temperature, 150.0f + 0.1f * 150.0f / 300.0f, k_Epsilon);
	EXPECT_NEAR(outcome.damage, BurnDamage(state.temperature, hut), k_Epsilon);
	// It heats itself no hotter than twice its combustion
	state = {.temperature = 300.0f, .previous = 300.0f};
	(void)Step(state, hut, {.life = 1.0f});
	EXPECT_FLOAT_EQ(state.temperature, 300.0f);
	EXPECT_EQ(state.flags & k_VeryHot, 0);
}

TEST(FireModel, SomethingNotBurningCoolsBySurfaceOverCapacityFasterInTheWetAndRain)
{
	const auto tree = Tree();
	const float t = 100.0f;
	const float dry = t - (t + 10.0f - k_AmbientTemperature) * (4.0f * 8.0f * 2.0f) * 0.1f / 1000.0f;
	State state {.temperature = t, .previous = t};
	(void)Step(state, tree, {.life = 1.0f});
	EXPECT_NEAR(state.temperature, dry, k_Epsilon);
	state = {.temperature = t, .previous = t};
	(void)Step(state, tree, {.inWater = true, .life = 1.0f});
	EXPECT_NEAR(state.temperature, t - (t - dry) * 50.0f, k_Epsilon);
	state = {.temperature = t, .previous = t};
	const auto outcome = Step(state, tree, {.rain = 127.0f, .life = 1.0f});
	EXPECT_TRUE(outcome.rainedOn);
	EXPECT_NEAR(state.temperature, t - (t - dry) * 2.27f, k_Epsilon);
	// Heated this turn, it doesn't cool
	state = {.temperature = t, .previous = t - 1.0f};
	(void)Step(state, tree, {.life = 1.0f});
	EXPECT_FLOAT_EQ(state.temperature, t);
	// A burning thing cooled below its combustion has just gone out
	state = {.temperature = 111.0f, .previous = 111.0f};
	(void)Step(state, tree, {.inWater = true, .life = 1.0f});
	EXPECT_NE(state.flags & k_JustExtinguished, 0);
}

TEST(FireModel, ItCharsAsItsLifeRunsLowAndGoesOnceColdAndClean)
{
	const auto hut = Hut();
	State state {.temperature = 300.0f, .previous = 300.0f};
	(void)Step(state, hut, {.life = 0.3f});
	EXPECT_NEAR(state.charring, 0.04f, k_Epsilon);
	// No more charred than its lost life allows
	state.charring = 0.6f;
	(void)Step(state, hut, {.life = 0.3f});
	EXPECT_NEAR(state.charring, 0.5f, k_Epsilon);
	// Out, the charring fades
	state = {.temperature = 30.0f, .previous = 30.0f, .charring = 0.3f};
	(void)Step(state, hut, {.life = 0.3f});
	EXPECT_NEAR(state.charring, 0.28f, k_Epsilon);
	state = {.temperature = k_AmbientTemperature + 0.05f, .previous = k_AmbientTemperature, .charring = 0.0f};
	EXPECT_TRUE(Step(state, hut, {.life = 1.0f}).gone);
}

TEST(FireModel, OurGamesOwnComparisonsAreKept)
{
	// A NaN reads as burning, as no share and as no flames, as our fire has always compared them
	const auto hut = Hut();
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_TRUE(IsOnFire(nan, hut));
	EXPECT_EQ(FireFraction(300.0f, hut, nan), 0.0f);
	EXPECT_EQ(FlameHeight(nan, hut), 0.0f);
}
