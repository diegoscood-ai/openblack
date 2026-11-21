/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a living being reacts to a predator: the rules of ECS/Effects/PredatorFlee, pinned to the numbers the
// original game's own functions give for the same inputs.

#include <array>
#include <bit>

#include <gtest/gtest.h>

#include "ECS/Effects/PredatorFlee.h"

using namespace openblack;
using namespace openblack::ecs::effects::flee;

namespace
{
struct PriorityCase
{
	bool animal;
	bool atHome;
	uint32_t distance;
	uint32_t topState;
	uint32_t storedFinalState;
	uint32_t finalState;
	uint32_t turnsSinceStateChange;
	uint32_t predatorTopState;
	uint32_t turnReactionStarted;
	bool comingTowards;
	uint32_t speed;
	int32_t walkingSpeed;
	uint32_t priority;
	uint32_t expected;
};

// The original's answers; the minimum distance to run away is 25 metres in every case. Floats are their bits
// clang-format off
constexpr std::array<PriorityCase, 40> k_PriorityCases {{
	{true, false, 0x41200000u, 0u, 0u, 0u, 0u, 0u, 0u, false, 0x40400000u, 2, 150u, 187u},
	{false, false, 0x41200000u, 0u, 0u, 0u, 0u, 0u, 0u, false, 0x40400000u, 2, 150u, 0u},
	{true, true, 0x41200000u, 0u, 0u, 0u, 0u, 0u, 0u, false, 0x40400000u, 2, 150u, 0u},
	{true, false, 0x41200000u, 236u, 0u, 0u, 0u, 0u, 0u, false, 0x40400000u, 2, 150u, 0u},
	{true, false, 0x41200000u, 0u, 236u, 0u, 0u, 0u, 0u, false, 0x40400000u, 2, 150u, 187u},
	{true, false, 0x41F00000u, 0u, 236u, 0u, 0u, 0u, 0u, false, 0x40400000u, 2, 150u, 0u},
	{true, false, 0x41200000u, 0u, 0u, 0u, 0u, 35u, 0u, false, 0x40400000u, 2, 150u, 0u},
	{true, false, 0x41200000u, 0u, 0u, 0u, 0u, 51u, 0u, false, 0x40400000u, 2, 150u, 0u},
	{true, false, 0x41200000u, 0u, 0u, 237u, 20u, 35u, 0u, false, 0x40400000u, 2, 150u, 187u},
	{true, false, 0x41200000u, 0u, 0u, 237u, 19u, 35u, 0u, false, 0x40400000u, 2, 150u, 0u},
	{true, false, 0x41F00000u, 0u, 0u, 0u, 0u, 0u, 5u, false, 0x40400000u, 2, 150u, 0u},
	{true, false, 0x41F00000u, 0u, 0u, 0u, 0u, 0u, 5u, true, 0x40400000u, 2, 150u, 187u},
	{true, false, 0x41F00000u, 0u, 0u, 0u, 0u, 0u, 0u, false, 0x40400000u, 2, 150u, 187u},
	{true, false, 0x41C80000u, 0u, 0u, 0u, 0u, 0u, 5u, false, 0x40400000u, 2, 150u, 187u},
	{true, false, 0x41200000u, 0u, 0u, 0u, 0u, 0u, 0u, false, 0x41100000u, 2, 150u, 156u},
	{true, false, 0x41200000u, 0u, 0u, 0u, 0u, 0u, 0u, false, 0x00000000u, 2, 150u, 75u},
	{true, false, 0x41200000u, 0u, 0u, 0u, 0u, 0u, 0u, false, 0x40E00000u, 3, 97u, 161u},
	{true, false, 0x41200000u, 0u, 0u, 0u, 0u, 0u, 0u, false, 0x3F800000u, 3, 100u, 66u},
	{true, false, 0x3E33E07Au, 0u, 0u, 0u, 0u, 0u, 7u, false, 0x410C7438u, 1, 250u, 198u},
	{true, false, 0x3FEEABC3u, 0u, 0u, 0u, 0u, 0u, 7u, false, 0x40C4E3AFu, 1, 76u, 15u},
	{true, false, 0x40DCD6D3u, 0u, 0u, 0u, 0u, 0u, 0u, false, 0x4137DD17u, 8, 216u, 7u},
	{true, false, 0x40DC9F84u, 0u, 0u, 0u, 0u, 0u, 0u, false, 0x411EB96Au, 6, 219u, 34u},
	{true, false, 0x41AFF739u, 0u, 0u, 0u, 0u, 0u, 7u, false, 0x408986A0u, 1, 237u, 115u},
	{true, false, 0x41BDD27Fu, 0u, 0u, 0u, 0u, 0u, 7u, false, 0x40834412u, 1, 188u, 223u},
	{true, false, 0x421907DFu, 0u, 0u, 0u, 0u, 0u, 7u, true, 0x40E4B18Eu, 1, 232u, 177u},
	{true, false, 0x4192418Eu, 0u, 0u, 0u, 0u, 0u, 0u, true, 0x412E6E13u, 4, 182u, 83u},
	{true, false, 0x41C2C0B6u, 0u, 0u, 0u, 0u, 0u, 7u, true, 0x40B0C292u, 9, 187u, 150u},
	{true, false, 0x40A0FA78u, 0u, 0u, 0u, 0u, 0u, 7u, false, 0x40991026u, 6, 112u, 100u},
	{true, false, 0x40ADB2A1u, 0u, 0u, 0u, 0u, 0u, 0u, true, 0x3E10C931u, 4, 216u, 111u},
	{true, false, 0x4194E421u, 0u, 0u, 0u, 0u, 0u, 0u, false, 0x3FB9E3D5u, 2, 37u, 31u},
	{true, false, 0x41E3AE69u, 0u, 0u, 0u, 0u, 0u, 0u, false, 0x41125621u, 8, 12u, 12u},
	{true, false, 0x420CCE09u, 0u, 0u, 0u, 0u, 0u, 7u, false, 0x3FF18D88u, 3, 163u, 0u},
	{true, false, 0x42144E7Fu, 0u, 0u, 0u, 0u, 0u, 0u, true, 0x3E1F12C9u, 4, 130u, 67u},
	{true, false, 0x41CF80ECu, 0u, 0u, 0u, 0u, 0u, 7u, true, 0x40CA71DEu, 7, 199u, 189u},
	{true, false, 0x40839AB7u, 0u, 0u, 0u, 0u, 0u, 7u, true, 0x413FF617u, 6, 138u, 206u},
	{true, false, 0x41BD0AFEu, 0u, 0u, 0u, 0u, 0u, 0u, true, 0x412B1B19u, 9, 157u, 171u},
	{true, false, 0x41DA85CCu, 0u, 0u, 0u, 0u, 0u, 7u, true, 0x40DBD20Au, 5, 167u, 198u},
	{true, false, 0x42032BE5u, 0u, 0u, 0u, 0u, 0u, 0u, false, 0x3CE469A3u, 1, 140u, 71u},
	{true, false, 0x4218A9C1u, 0u, 0u, 0u, 0u, 0u, 0u, true, 0x405ED804u, 8, 90u, 64u},
	{true, false, 0x40634A55u, 0u, 0u, 0u, 0u, 0u, 0u, false, 0x40EF1324u, 5, 63u, 78u},
}};
// clang-format on

VillagerPredatorFacts FactsOf(const PriorityCase& c)
{
	return {
	    .initiatorIsAnimal = c.animal,
	    .distance = std::bit_cast<float>(c.distance),
	    .minDistanceToRunAway = 25.0f,
	    .atHome = c.atHome,
	    .topState = static_cast<VillagerStates>(c.topState),
	    .storedFinalState = static_cast<VillagerStates>(c.storedFinalState),
	    .finalState = static_cast<VillagerStates>(c.finalState),
	    .turnsSinceStateChange = static_cast<uint16_t>(c.turnsSinceStateChange),
	    .predatorTopState = static_cast<uint8_t>(c.predatorTopState),
	    .turnReactionStarted = c.turnReactionStarted,
	    .predatorComingTowards = c.comingTowards,
	    .predatorSpeed = std::bit_cast<float>(c.speed),
	    .predatorWalkingSpeed = c.walkingSpeed,
	    .priority = c.priority,
	};
}
} // namespace

TEST(PredatorFlee, AVillagersPriorityToRunFromAPredatorIsTheOriginals)
{
	for (size_t i = 0; i < k_PriorityCases.size(); ++i)
	{
		EXPECT_EQ(VillagerFleeFromPredatorPriority(FactsOf(k_PriorityCases[i])), k_PriorityCases[i].expected) << "case " << i;
	}
}

TEST(PredatorFlee, AFastPredatorsPriorityKeepsOnlyItsLowByte)
{
	// 150 x (0.5 + 0.5 x 9 / 2) = 412.5: 412 cut to a byte is 156
	auto facts = FactsOf(k_PriorityCases[0]);
	facts.predatorSpeed = 9.0f;
	EXPECT_EQ(VillagerFleeFromPredatorPriority(facts), 156u);
	// a walking speed of 0 makes the scale infinite: the conversion's out-of-range value, whose low byte is 0
	facts.predatorWalkingSpeed = 0;
	EXPECT_EQ(VillagerFleeFromPredatorPriority(facts), 0u);
}

TEST(PredatorFlee, TheLivingTurnsToReactEndAtOnceForAChasingPredatorItWatches)
{
	constexpr uint32_t k_Standard = 555;
	// the original's answers: only an animal; none while it watches (state 7) a predator faster than 5 m/s coming at it
	EXPECT_EQ(LivingTurnsToReactToPredator(false, 7, 9.0f, true, k_Standard), 0u);
	EXPECT_EQ(LivingTurnsToReactToPredator(true, 7, 5.0f, true, k_Standard), k_Standard);
	EXPECT_EQ(LivingTurnsToReactToPredator(true, 7, 5.0000005f, false, k_Standard), k_Standard);
	EXPECT_EQ(LivingTurnsToReactToPredator(true, 7, 5.0000005f, true, k_Standard), 0u);
	EXPECT_EQ(LivingTurnsToReactToPredator(true, 6, 5.0000005f, true, k_Standard), k_Standard);
}

TEST(PredatorFlee, TheLivingTurnsBeforeReactingAgainLookAtTheTrueDistance)
{
	constexpr uint32_t k_Standard = 777;
	// the original's answers, minimum distance 25: none for a non-animal, for a predator faster than 5 m/s, or nearer
	// than the minimum; the spread distance plays no part here
	EXPECT_EQ(LivingTurnsBeforeReactingAgainToPredator(false, 0.0f, 40.0f, 25.0f, k_Standard), 0u);
	EXPECT_EQ(LivingTurnsBeforeReactingAgainToPredator(true, 0.0f, 24.999998f, 25.0f, k_Standard), 0u);
	EXPECT_EQ(LivingTurnsBeforeReactingAgainToPredator(true, 0.0f, 25.0f, 25.0f, k_Standard), k_Standard);
	EXPECT_EQ(LivingTurnsBeforeReactingAgainToPredator(true, 5.0f, 40.0f, 25.0f, k_Standard), k_Standard);
	EXPECT_EQ(LivingTurnsBeforeReactingAgainToPredator(true, 5.0000005f, 40.0f, 25.0f, k_Standard), 0u);
	EXPECT_EQ(LivingTurnsBeforeReactingAgainToPredator(true, 9.0f, 25.0f, 25.0f, k_Standard), 0u);
}

TEST(PredatorFlee, AVillagerHidingOrLookingAroundKeepsItsReaction)
{
	// the original's answers over the living result
	EXPECT_EQ(VillagerTurnsToReactToPredator(VillagerStates::InvalidState, 1234), 1234u);
	EXPECT_EQ(VillagerTurnsToReactToPredator(VillagerStates::FleeingFromPredatorReaction, 1234), 1234u);
	EXPECT_EQ(VillagerTurnsToReactToPredator(VillagerStates::GoAndHideInNearbyBuilding, 1234), k_TurnsForever);
	EXPECT_EQ(VillagerTurnsToReactToPredator(VillagerStates::LookToSeeIfItIsSafe, 1234), k_TurnsForever);
	EXPECT_EQ(VillagerTurnsBeforeReactingAgainToPredator(VillagerStates::InvalidState, 4321), 4321u);
	EXPECT_EQ(VillagerTurnsBeforeReactingAgainToPredator(VillagerStates::GoAndHideInNearbyBuilding, 4321), 4321u);
	EXPECT_EQ(VillagerTurnsBeforeReactingAgainToPredator(VillagerStates::LookToSeeIfItIsSafe, 4321), 0u);
	EXPECT_EQ(k_TurnsForever, 0x7FFFFFFFu);
}

TEST(PredatorFlee, AVillagerOnItsWayToHideOnlyRestartsItsCount)
{
	EXPECT_FALSE(VillagerStartsFleeingFromPredator(VillagerStates::GoAndHideInNearbyBuilding));
	EXPECT_TRUE(VillagerStartsFleeingFromPredator(VillagerStates::LookToSeeIfItIsSafe));
	EXPECT_TRUE(VillagerStartsFleeingFromPredator(VillagerStates::InvalidState));
}

TEST(PredatorFlee, AVillagerHidesInTheNearestBuildingWithinAHundredMetres)
{
	EXPECT_FALSE(NearestHidingPlace({}).has_value());
	const std::array none {HidingPlace {100.0f, true}, HidingPlace {5.0f, false}};
	EXPECT_FALSE(NearestHidingPlace(none).has_value());
	const std::array some {HidingPlace {60.0f, true}, HidingPlace {30.0f, false}, HidingPlace {40.0f, true},
	                       HidingPlace {40.0f, true}, HidingPlace {99.99f, true}};
	// strictly nearer: of two at the same distance the first in the list
	EXPECT_EQ(NearestHidingPlace(some), 2u);
}

TEST(PredatorFlee, ARunningVillagerStopsHidesOrLeavesItToADraw)
{
	EXPECT_EQ(VillagerPredatorFleeStep(75.5f, 75.0f, true, true, 9.0f, true), PredatorFleeStep::Stop);
	EXPECT_EQ(VillagerPredatorFleeStep(75.0f, 75.0f, true, true, 9.0f, true), PredatorFleeStep::HideAgain);
	EXPECT_EQ(VillagerPredatorFleeStep(10.0f, 75.0f, true, false, 9.0f, true), PredatorFleeStep::LookForHidingPlace);
	EXPECT_EQ(VillagerPredatorFleeStep(10.0f, 75.0f, false, true, 9.0f, true), PredatorFleeStep::LookForHidingPlace);
	EXPECT_EQ(VillagerPredatorFleeStep(10.0f, 75.0f, false, false, 5.0f, true), PredatorFleeStep::Draw);
	EXPECT_EQ(VillagerPredatorFleeStep(10.0f, 75.0f, false, false, 9.0f, false), PredatorFleeStep::Draw);
}

TEST(PredatorFlee, AHidingVillagerWaitsWhileThePredatorIsNear)
{
	EXPECT_EQ(VillagerHideStep(false, true, true, 1.0f, 25.0f), HideStep::Stop);
	EXPECT_EQ(VillagerHideStep(true, false, true, 1.0f, 25.0f), HideStep::WalkOn);
	EXPECT_EQ(VillagerHideStep(true, true, true, 25.0f, 25.0f), HideStep::Wait);
	EXPECT_EQ(VillagerHideStep(true, true, true, 25.5f, 25.0f), HideStep::Count);
	EXPECT_EQ(VillagerHideStep(true, true, false, 1.0f, 25.0f), HideStep::Count);
	// out after more than 150 turns plus the draw
	EXPECT_FALSE(HiddenLongEnough(150, 0));
	EXPECT_TRUE(HiddenLongEnough(151, 0));
	EXPECT_FALSE(HiddenLongEnough(299, 149));
	EXPECT_TRUE(HiddenLongEnough(300, 149));
}

TEST(PredatorFlee, AVillagerLookingAroundGoesBackInWhileThePredatorIsAbout)
{
	EXPECT_EQ(VillagerLookIfSafeStep(true, true, 100.0f, 100.0f, 0, true), LookIfSafeStep::HideAgain);
	EXPECT_EQ(VillagerLookIfSafeStep(true, true, 100.0f, 100.0f, 0, false), LookIfSafeStep::FleeIfComing);
	EXPECT_EQ(VillagerLookIfSafeStep(true, true, 100.5f, 100.0f, 0, true), LookIfSafeStep::Count);
	EXPECT_EQ(VillagerLookIfSafeStep(true, false, 10.0f, 100.0f, 0, true), LookIfSafeStep::Count);
	EXPECT_EQ(VillagerLookIfSafeStep(false, true, 10.0f, 100.0f, 0, true), LookIfSafeStep::Count);
	EXPECT_EQ(VillagerLookIfSafeStep(true, true, 10.0f, 100.0f, k_PredatorAsleep, true), LookIfSafeStep::Count);
	EXPECT_EQ(VillagerLookIfSafeStep(true, true, 10.0f, 100.0f, k_PredatorInLair, true), LookIfSafeStep::Count);
	// it stops after more than 25 turns plus the draw
	EXPECT_FALSE(LookedLongEnough(25, 0));
	EXPECT_TRUE(LookedLongEnough(26, 0));
	EXPECT_TRUE(LookedLongEnough(50, 24));
}
