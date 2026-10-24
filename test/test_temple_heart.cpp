/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <vector>

#include <gtest/gtest.h>

#include "Physics/TempleHeart.h"

using namespace openblack::physics::temple_heart;

namespace
{
entt::entity E(uint32_t id)
{
	return static_cast<entt::entity>(id);
}

Building Standing(uint32_t id, float life)
{
	return {.entity = E(id), .available = true, .life = life, .built = 1.0f};
}
} // namespace

TEST(TempleHeart, ASoundBuildingTakesTheBlowAtOnce)
{
	const std::vector<Town> towns {
	    {.buildings = {Standing(1, 0.2f), Standing(2, 0.3f)}},
	    {.buildings = {Standing(3, 1.0f)}},
	};
	const auto target = Choose(towns);
	EXPECT_EQ(target.kind, TargetKind::Building);
	EXPECT_EQ(target.entity, E(2));
}

TEST(TempleHeart, OtherwiseTheFirstStandingBuildingOfAnyTown)
{
	auto field = Standing(1, 1.0f);
	field.field = true;
	auto pitch = Standing(2, 1.0f);
	pitch.footballPitch = true;
	auto unbuilt = Standing(3, 1.0f);
	unbuilt.built = 0.0f;
	auto ruin = Standing(4, 0.0f);
	const std::vector<Town> towns {
	    {.buildings = {field, pitch, unbuilt, ruin}},
	    {.buildings = {Standing(5, 0.25f), Standing(6, 0.1f)}},
	};
	const auto target = Choose(towns);
	EXPECT_EQ(target.kind, TargetKind::Building);
	EXPECT_EQ(target.entity, E(5));
}

TEST(TempleHeart, ThenAHomelessVillagerThenTheHeart)
{
	const std::vector<Town> homeless {
	    {.homeless = {{.entity = E(7), .available = false}}},
	    {.homeless = {{.entity = E(8), .available = true}, {.entity = E(9), .available = true}}},
	};
	const auto villager = Choose(homeless);
	EXPECT_EQ(villager.kind, TargetKind::Villager);
	EXPECT_EQ(villager.entity, E(8));
	EXPECT_EQ(Choose(std::vector<Town> {{}}).kind, TargetKind::Heart);
}

TEST(TempleHeart, TheHeartIsHarmedByTheBlowsMomentumUpToAFifth)
{
	EXPECT_FLOAT_EQ(Harm({0.0f, 3.0f, 4.0f}, 1000.0f), 0.025f);
	EXPECT_FLOAT_EQ(Harm({0.0f, 30.0f, 40.0f}, 1000.0f), 0.2f);
}

// Each step is rounded to a float, as the game's single precision rounds it: worked out at a higher precision and
// rounded once, this would be a unit more
TEST(TempleHeart, TheHarmsStepsAreSingleFloats)
{
	const glm::vec3 velocity {std::bit_cast<float>(0xC11A0370u), std::bit_cast<float>(0xC12A0737u),
	                          std::bit_cast<float>(0x419E9B3Au)};
	EXPECT_EQ(std::bit_cast<uint32_t>(Harm(velocity, std::bit_cast<float>(0x44B102D0u))), 0x3E31658Fu);
}

TEST(TempleHeartStrike, ABlowPassedOnStrikesTheBuildingOrFlingsTheVillager)
{
	EXPECT_EQ(Outcome({.struck = false, .target = TargetKind::Building}), StrikeOutcome::Nothing);
	EXPECT_EQ(Outcome({.struck = true, .target = TargetKind::Building}), StrikeOutcome::BuildingStruck);
	EXPECT_EQ(Outcome({.struck = true, .target = TargetKind::Villager}), StrikeOutcome::VillagerFlung);
	EXPECT_EQ(Outcome({.struck = true, .target = TargetKind::Villager, .villagerInPhysics = true}), StrikeOutcome::Nothing);
}

TEST(TempleHeartStrike, OnlyAThingStillThereThatBreaksBuildingsHarmsTheHeart)
{
	EXPECT_EQ(Outcome({.struck = true, .hitterAvailable = true, .breaksBuildings = true}), StrikeOutcome::HeartHarmed);
	EXPECT_EQ(Outcome({.struck = true, .hitterAvailable = true, .breaksBuildings = false}), StrikeOutcome::Nothing);
	EXPECT_EQ(Outcome({.struck = true, .hitterAvailable = false, .breaksBuildings = true}), StrikeOutcome::Nothing);
}

TEST(TempleHeartStrike, TheHeartKeepsTheTurnABlowThatBreaksBuildingsHarmsIt)
{
	EXPECT_EQ(BreakingBlowTurn(StrikeOutcome::HeartHarmed, 1234, 0), 1234u);
	EXPECT_EQ(BreakingBlowTurn(StrikeOutcome::HeartHarmed, 2000, 1234), 2000u);
	// a blow passed on, or one that harms nothing, leaves the turn it kept
	for (const auto outcome : {StrikeOutcome::Nothing, StrikeOutcome::BuildingStruck, StrikeOutcome::VillagerFlung})
	{
		EXPECT_EQ(BreakingBlowTurn(outcome, 2000, 1234), 1234u);
		EXPECT_EQ(BreakingBlowTurn(outcome, 2000, 0), 0u);
	}
}

TEST(TempleHeartDamage, OnlyAHurtHeartBeamsAndOneWithNoLifeAtItsFiercest)
{
	EXPECT_FALSE(DamageEffectLife(1.0f).has_value());
	EXPECT_FALSE(DamageEffectLife(1.5f).has_value());
	EXPECT_EQ(DamageEffectLife(0.5f), 0.5f);
	EXPECT_EQ(DamageEffectLife(0.0f), 0.0f);
	EXPECT_EQ(DamageEffectLife(-0.5f), 0.0f);
}

TEST(TempleHeartDamage, TheLessLifeTheOftenerItBeams)
{
	// 3 seconds at nearly all its life, 0.4 at none, in whole turns of a tenth of a second, a float a step: 17 turns
	// at half its life (1.7 as a float is a little under, but its product is a float again, 17)
	EXPECT_EQ(DamageInterval(100, 0.0f), 4u);
	EXPECT_EQ(DamageInterval(100, 0.5f), 17u);
	EXPECT_EQ(DamageInterval(100, 0.3f), 11u);
	const auto half = DamageLookAt(0.5f);
	EXPECT_FLOAT_EQ(half.life, 1.7f);
	EXPECT_EQ(half.alpha, 60);
	EXPECT_EQ(DamageLookAt(0.0f).alpha, 100);
	// Due strictly more than the interval after the last
	uint32_t last = 10;
	EXPECT_FALSE(DamageBeamDue(last, 27, 17));
	EXPECT_TRUE(DamageBeamDue(last, 28, 17));
	EXPECT_EQ(last, 28u);
	// Its sound from six tenths of its life down
	EXPECT_TRUE(DamageBeamSounds(0.6f));
	EXPECT_TRUE(DamageBeamSounds(0.0f));
	EXPECT_FALSE(DamageBeamSounds(0.61f));
}

TEST(TempleHeartWounds, TheOriginalsWaitAndSoundForEachLife)
{
	// The original's numbers at a tenth of a second a turn, run in the game itself: no life, less than none or not a
	// number wait 4 turns; the edges where single and double precision steps part by a turn are the 24-bit ones
	const auto wait = [](float life) { return DamageInterval(100, DamageEffectLife(life).value()); };
	EXPECT_EQ(wait(0.0f), 4u);
	EXPECT_EQ(wait(-1.0f), 4u);
	EXPECT_EQ(wait(std::bit_cast<float>(0x7FC00000u)), 4u);
	EXPECT_EQ(wait(0.25f), 10u);
	EXPECT_EQ(wait(0.5f), 17u);
	EXPECT_EQ(wait(0.6f), 19u);
	EXPECT_TRUE(DamageBeamSounds(DamageEffectLife(0.6f).value()));
	// The next float above six tenths waits as long but makes no sound
	const float justAbove = std::bit_cast<float>(0x3F19999Bu);
	EXPECT_EQ(wait(justAbove), 19u);
	EXPECT_FALSE(DamageBeamSounds(DamageEffectLife(justAbove).value()));
	// The last float below all its life
	EXPECT_EQ(wait(std::bit_cast<float>(0x3F7FFFFFu)), 29u);
}

TEST(TempleHeartBeam, EveryTwoSecondsAtTheSameTargetAndAtOnceAtANewOne)
{
	EXPECT_EQ(BeamInterval(100), 20u);
	Beam beam;
	const auto first = static_cast<entt::entity>(1);
	const auto second = static_cast<entt::entity>(2);
	EXPECT_TRUE(BeamAtTargetDue(beam, first, 21, 20));
	EXPECT_EQ(beam.turn, 21u);
	// Strictly more than the interval later
	EXPECT_FALSE(BeamAtTargetDue(beam, first, 41, 20));
	EXPECT_TRUE(BeamAtTargetDue(beam, first, 42, 20));
	// A new target starts the wait afresh, from nothing
	EXPECT_TRUE(BeamAtTargetDue(beam, second, 43, 20));
	// Taking the blow itself forgets the target, and beams at itself afresh too
	EXPECT_TRUE(BeamAtItselfDue(beam, 44, 20));
	EXPECT_TRUE(beam.target == entt::null);
	EXPECT_FALSE(BeamAtItselfDue(beam, 50, 20));
	// Too early in the game for any beam
	Beam early;
	EXPECT_FALSE(BeamAtTargetDue(early, first, 20, 20));
	EXPECT_EQ(early.target, first);
}
