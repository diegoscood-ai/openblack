/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <optional>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureHold.h"
#include "Creature/CreatureObjectActions.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Villager.h"
#include "ECS/HoldLowering.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
float Bits(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

glm::vec3 Row(uint32_t x, uint32_t y, uint32_t z)
{
	return {Bits(x), Bits(y), Bits(z)};
}

void ExpectBits(const glm::vec3& value, const glm::vec3& expected)
{
	for (glm::length_t i = 0; i < 3; ++i)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(value[i]), std::bit_cast<uint32_t>(expected[i])) << "component " << i;
	}
}

// A right hand's bone, its rows of lengths other than one and not square to each other, as a blended pose leaves them
const glm::mat3 k_RightHand {glm::vec3(1.731f, 0.0213f, -0.4471f), glm::vec3(0.3319f, 1.502f, 0.9917f),
                             glm::vec3(-0.0441f, -1.1093f, 1.4087f)};
// A left hand's, whose rows' lengths come out apart when summed in another order
const glm::mat3 k_LeftHand {glm::vec3(-0.0072f, 2.3184f, 0.6031f), glm::vec3(-1.9921f, 0.0108f, -0.7713f),
                            glm::vec3(0.4462f, 0.5527f, -2.0214f)};
} // namespace

// The rows made of unit length are the original's to the bit (its own matrix normalisation run on these rows), then the
// up and forward rows swapped and the side and forward rows turned back
TEST(CreatureHold, TheRightHandsTurnIsTheBonesRowsOfUnitLengthSwappedAndTurnedBack)
{
	const auto turn = creature_hold::HeldTurn(k_RightHand, false);
	ExpectBits(turn[0], Row(0xBF77D896u, 0xBC432F23u, 0x3E800846u));
	ExpectBits(turn[1], Row(0xBCC969E9u, 0xBF1E5332u, 0x3F490E90u));
	ExpectBits(turn[2], Row(0xBE39B2CEu, 0xBF5217BBu, 0xBF0AB6DFu));
}

// In the left hand the new up row is turned back instead of the forward one
TEST(CreatureHold, TheLeftHandTurnsBackTheUpRowInstead)
{
	const auto turn = creature_hold::HeldTurn(k_LeftHand, true);
	ExpectBits(turn[0], Row(0x3B44F7B8u, 0xBF77BF95u, 0xBE80E58Eu));
	ExpectBits(turn[1], Row(0xBE554024u, 0xBE841327u, 0x3F718515u));
	ExpectBits(turn[2], Row(0xBF6EBA41u, 0x3BA5A9B1u, 0xBEB8DC5Bu));
}

// The thing hangs below the grasp point along its up row at its scale, by its height times its lowering (a mobile
// object 4 high, 0.7; a villager 3 high, 0.65)
TEST(CreatureHold, TheThingHangsBelowTheGraspPointByItsHeightTimesItsLowering)
{
	const auto right = creature_hold::Place(k_RightHand, glm::vec3(2561.3f, 30.25f, 2519.75f), false, 1.25f, 4.0f * 0.7f);
	ExpectBits(right.position, Row(0x452015E7u, 0x41FFDAA2u, 0x451D58D0u));
	EXPECT_EQ(right.turn, creature_hold::HeldTurn(k_RightHand, false));
	const auto left = creature_hold::Place(k_LeftHand, glm::vec3(10.0f, 2.0f, -4.0f), true, 0.5f, 3.0f * 0.65f);
	ExpectBits(left.position, Row(0x41267F5Eu, 0x40203188u, 0xC0BADEF4u));
}

// With no lowering, or an up row of no length, the thing is at the grasp point
TEST(CreatureHold, WithNoLoweringOrNoUpRowItIsAtTheGraspPoint)
{
	const glm::vec3 grasp {5.0f, 6.0f, 7.0f};
	ExpectBits(creature_hold::Place(k_RightHand, grasp, false, 1.0f, 0.0f).position, grasp);
	ExpectBits(creature_hold::HungBelow(grasp, glm::vec3(0.0f), 2.0f), grasp);
}

// Its rows at its scale are the turn's rows times the scale, as its drawn matrix multiplies them
TEST(CreatureHold, ItsScaledRowsAreTheTurnsRowsTimesItsScale)
{
	const auto turn = creature_hold::HeldTurn(k_RightHand, false);
	const auto rows = creature_hold::ScaledRows(turn, 1.25f);
	for (glm::length_t r = 0; r < 3; ++r)
	{
		ExpectBits(rows[r], turn[r] * 1.25f);
	}
}

// How far below the hand a thing hangs goes by its class, as the hand's hold table has it: a pot or a mobile object 0.7,
// a villager 0.65, a lump of poo -0.3 (above the hand point), anything else not at all
TEST(CreatureHold, TheLoweringGoesByTheThingsClass)
{
	ecs::Registry registry;
	const auto pot = registry.Create();
	registry.Assign<Pot>(pot, 100u, 600u, PotInfo::WheatInHand);
	const auto villager = registry.Create();
	registry.Assign<Villager>(villager);
	const auto poo = registry.Create();
	registry.Assign<MobileObject>(poo, MobileObjectInfo::LumpOfPoo);
	const auto other = registry.Create();
	EXPECT_EQ(ecs::hold_lowering::MultiplierOf(registry, pot), 0.7f);
	EXPECT_EQ(ecs::hold_lowering::MultiplierOf(registry, villager), 0.65f);
	EXPECT_EQ(ecs::hold_lowering::MultiplierOf(registry, poo), -0.3f);
	EXPECT_EQ(ecs::hold_lowering::MultiplierOf(registry, other), 0.0f);
}

// The arm shows the hold pose unless the body plays an animation that moves the held thing itself
TEST(CreatureHold, TheHoldPoseShowsUnlessTheBodyPlaysWithWhatItHolds)
{
	using creature_object_actions::Kind;
	using creature_object_actions::ShowsHoldPose;
	EXPECT_TRUE(ShowsHoldPose(std::nullopt));
	for (const auto kind :
	     {Kind::PickUp, Kind::PutDown, Kind::Discard, Kind::Lob, Kind::Eat, Kind::Keep, Kind::Throw, Kind::TakeFromHand})
	{
		EXPECT_FALSE(ShowsHoldPose(kind)) << creature_object_actions::Name(kind);
	}
	for (const auto kind : {Kind::Destroy, Kind::Point, Kind::Kick})
	{
		EXPECT_TRUE(ShowsHoldPose(kind)) << creature_object_actions::Name(kind);
	}
}
