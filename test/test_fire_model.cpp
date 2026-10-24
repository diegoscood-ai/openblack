/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fire's pure formulas (Fire/FireModel): how hot things get, how they burn, cool and heat each other.

#include <cstdint>

#include <array>
#include <bit>
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

/// A rock's table row in the game's info.dat: combustion 500, heat capacity 4000
Material Rock()
{
	return {.combustion = 500.0f, .capacity = 4000.0f, .radius = 1.0f, .height = 1.0f, .fireRadius = 1.0f};
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

TEST(FireModel, AFireReachesOnlyWhatIsStrictlyWithinBothRadii)
{
	// Across the land: the distance strictly under the target's own fire radius plus the fire's
	EXPECT_FALSE(Reaches(5.0f, 2.0f, 3.0f));
	EXPECT_TRUE(Reaches(4.99f, 2.0f, 3.0f));
	EXPECT_FALSE(Reaches(5.01f, 2.0f, 3.0f));
	// In height, once either is 3 m up or more: the target's height plus the flames' strictly over how far the fire is
	// above it
	EXPECT_FALSE(ReachesHeight(5.0f, 0.0f, 1.0f, 3.9f));
	EXPECT_FALSE(ReachesHeight(5.0f, 0.0f, 1.0f, 4.0f));
	EXPECT_TRUE(ReachesHeight(5.0f, 0.0f, 1.0f, 4.1f));
	EXPECT_TRUE(ReachesHeight(0.0f, 5.0f, 0.0f, 0.0f)); // a fire below its target
	// both under 3 m up: no height test at all
	EXPECT_TRUE(ReachesHeight(2.9f, 0.0f, 0.0f, 0.0f)); // 2.9 m above a target with no height
	EXPECT_FALSE(ReachesHeight(3.0f, 0.0f, 0.0f, 0.0f));
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

TEST(FireModel, AToyWarmsButIsNeverHurt)
{
	// every toy's table row: combustion 1000, heat capacity 5000 and no defence against burning
	const Material toy {.combustion = 1000.0f, .capacity = 5000.0f, .defence = 0.0f};
	EXPECT_FLOAT_EQ(BurnDamage(2000.0f, toy), 0.0f);
	EXPECT_FLOAT_EQ(BurnDamage(1500.0f, toy), 0.0f);
	// a fire next to it warms it by ten for each degree between them, over its capacity
	const auto warmed = HeatTransfer(220.0f, Tree(), k_AmbientTemperature, toy);
	EXPECT_NEAR(warmed.targetTemperature, k_AmbientTemperature + 10.0f * (220.0f - k_AmbientTemperature) / 5000.0f, 0.01f);
	// past its combustion it glows, but with no defence it never counts as burning: it does not heat itself, it cools
	// as anything not burning does, and it loses no life
	State state {.temperature = 1500.0f, .previous = 1500.0f};
	const auto outcome = Step(state, toy, {.life = 1.0f});
	EXPECT_FLOAT_EQ(outcome.damage, 0.0f);
	EXPECT_NEAR(state.temperature, 1500.0f - (1500.0f + 10.0f - k_AmbientTemperature) * 4.0f * 0.1f / 5000.0f, 1e-3f);
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
	// rain counts as cooling, so a hot thing in the rain steams (FireGraphic)
	EXPECT_NE(state.flags & k_Cooling, 0);
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

TEST(FireModel, ARockFlamesFromFourHundredFullAtAThousandAndBurnsAtFiveHundred)
{
	// The share of its fiercest runs from 0.8 of its combustion temperature to twice it: from 400 to 1000 for a rock
	const auto rock = Rock();
	EXPECT_FLOAT_EQ(FireFraction(300.0f, rock, 1.0f), 0.0f);
	EXPECT_FLOAT_EQ(FireFraction(400.0f, rock, 1.0f), 0.0f);
	EXPECT_FLOAT_EQ(FireFraction(700.0f, rock, 1.0f), 0.5f);
	EXPECT_FLOAT_EQ(FireFraction(1000.0f, rock, 1.0f), 1.0f);
	EXPECT_FLOAT_EQ(FireFraction(1500.0f, rock, 1.0f), 1.0f);
	// It is on fire from its combustion temperature on, that one included
	EXPECT_FALSE(IsOnFire(499.9f, rock));
	EXPECT_TRUE(IsOnFire(500.0f, rock));
	EXPECT_FLOAT_EQ(MaxTemperature(rock), 1000.0f);
}

TEST(FireModel, ARockMakesPeopleReactFromAHundredDegrees)
{
	// The living react to anything at 100 degrees or more, or to anything on fire
	const auto rock = Rock();
	EXPECT_FALSE(IsAboveReactionTemperature(99.9f, rock));
	EXPECT_TRUE(IsAboveReactionTemperature(100.0f, rock));
	EXPECT_TRUE(IsAboveReactionTemperature(400.0f, rock));
	// something that burns below 100 makes them react once it is on fire
	const Material low {.combustion = 60.0f, .capacity = 10.0f};
	EXPECT_FALSE(IsAboveReactionTemperature(59.9f, low));
	EXPECT_TRUE(IsAboveReactionTemperature(60.0f, low));
}

namespace
{
/// A case of the will to fight a fire, as the original game decides it: the group's burning radius, the villager's
/// radius, the fire fighters there, the group's burning priority and the town modifier (as float bits), and whether it
/// goes to beat the fire
struct FightFireCase
{
	uint32_t groupRadius;
	uint32_t villagerRadius;
	uint32_t fireFighters;
	uint32_t priority;
	uint32_t townModifier;
	bool fights;
};

// Hand-picked first (the 0.1 threshold, no room, a full room, more fire fighters than room), then a spread, then cases
// next to the threshold where only the original's rounding order gives the right answer
constexpr auto k_FightFireCases = std::to_array<FightFireCase>({
    {0x41200000, 0x3F800000, 0, 0x3F800000, 0x3F800000, true},   {0x41200000, 0x3F800000, 0, 0x3DCCCCCD, 0x3F800000, false},
    {0x41200000, 0x3F800000, 0, 0x3DCCCCDA, 0x3F800000, true},   {0x00000000, 0x3F800000, 0, 0x40A00000, 0x3F800000, false},
    {0x3F800000, 0x3F800000, 2, 0x40A00000, 0x3F800000, false},  {0x3F22F984, 0x3F800000, 1, 0x40A00000, 0x3F800000, false},
    {0x40000000, 0x3F800000, 3, 0x3F800000, 0x3F800000, false},  {0x40000000, 0x3F800000, 4, 0x3F800000, 0x3F800000, false},
    {0x40A00000, 0x3F000000, 10, 0x3F000000, 0x3F000000, false}, {0x40400000, 0x3F400000, 0, 0x3E4CCCCD, 0x3F000000, false},
    {0x40400000, 0x3F400000, 0, 0x3E4CCCCD, 0x3F000002, true},   {0x40400000, 0x3F400000, 1, 0x3E4CCCCD, 0x3F000000, false},
    {0x4129BFE1, 0x3EC533F4, 5, 0x3FAF7598, 0x3F3EBAD1, true},   {0x420844F8, 0x3FF0E9D3, 16, 0x3F93A0DA, 0x3F5A099D, true},
    {0x419038CA, 0x3FC648E9, 14, 0x3FE6828C, 0x3EF2CF8E, true},  {0x420D8D98, 0x3EC7427E, 20, 0x3F647F5D, 0x3F280EF0, true},
    {0x415CBEE0, 0x3EB2C7D1, 18, 0x3FBF6259, 0x3F5A0E0C, true},  {0x41A106F2, 0x3F28DAEF, 30, 0x3EB61CB7, 0x3F405CB1, false},
    {0x4190A1DE, 0x3F9EEA31, 18, 0x3F672DFB, 0x3F050B00, true},  {0x420399C5, 0x3FC20167, 24, 0x3EF62A0D, 0x3F33CF61, true},
    {0x41D5B483, 0x3FB6AE44, 3, 0x3E14A1D9, 0x3F44698E, false},  {0x41DBF26A, 0x3FFC3F0C, 6, 0x3E7DE8B7, 0x3F0E2AD6, false},
    {0x4109483C, 0x3F967549, 7, 0x3E8A6718, 0x3F7324F7, true},   {0x41CE1CBE, 0x3EEAE437, 59, 0x3F02AE59, 0x3F176E70, true},
    {0x42003C4A, 0x3F8D1096, 29, 0x3F70A4B5, 0x3E9515C7, false}, {0x40F87BAD, 0x3FDC6275, 6, 0x40293B86, 0x3E7D04A0, false},
    {0x41E56296, 0x3FEF61B8, 18, 0x3F7DDCBA, 0x3ECC6800, true},  {0x418775D3, 0x3F93A25D, 15, 0x3E9FA347, 0x3F6AE728, false},
    {0x41BB1AB9, 0x3F2C9C41, 24, 0x3EE64C0D, 0x3ECB7068, true},
});
} // namespace

TEST(FireModel, AVillagerFightsAFireAsTheOriginalGameDecides)
{
	for (const auto& c : k_FightFireCases)
	{
		EXPECT_EQ(WillFightFire(std::bit_cast<float>(c.groupRadius), std::bit_cast<float>(c.villagerRadius), c.fireFighters,
		                        std::bit_cast<float>(c.priority), std::bit_cast<float>(c.townModifier)),
		          c.fights)
		    << std::hex << c.groupRadius << " " << c.villagerRadius << " " << c.fireFighters << " " << c.priority << " "
		    << c.townModifier;
	}
}

TEST(FireModel, NoRoomRoundAFireMeansNoWillToFightIt)
{
	EXPECT_EQ(FightFireWill(0.0f, 1.0f, 0, 5.0f, 1.0f), 0.0f);
	// a 10 m group burning round, 1 m villagers: about 15.7 fit; with none there the will is the priority itself
	EXPECT_FLOAT_EQ(FightFireWill(10.0f, 1.0f, 0, 0.5f, 1.0f), 0.5f);
	// more fire fighters than fit: the will goes below nothing
	EXPECT_LT(FightFireWill(2.0f, 1.0f, 4, 1.0f, 1.0f), 0.0f);
	// a will of exactly 0.1 is not enough
	EXPECT_FALSE(WillFightFire(10.0f, 1.0f, 0, 0.1f, 1.0f));
}
