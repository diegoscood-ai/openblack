/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The physics' turn and pair rules (Physics/TurnRules, Physics/PairRules), on plain values. The cases follow the
// upstream physics tests where their expected values hold for these rules; the adapted ones say how.

#include <cstdint>

#include <array>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Physics/PairRules.h"
#include "Physics/TurnRules.h"

using namespace openblack;
using openblack::physics::PairMember;
using openblack::physics::TestsPoints;

namespace
{
entt::entity Id(uint32_t id)
{
	return static_cast<entt::entity>(id);
}
} // namespace

TEST(PhysicsTurn, ImpactIsTheMeanForceAndIgnoresTheSmallest)
{
	// A force too small to count gives an impact of 0 (upstream: no value)
	EXPECT_EQ(physics::turn::Impact(glm::vec3(0.01f, 0.0f, 0.0f)), 0.0f);
	EXPECT_FLOAT_EQ(physics::turn::Impact(glm::vec3(0.0f, 200.0f, 0.0f)), 10.0f);
	EXPECT_FLOAT_EQ(physics::turn::GLoad(98.1f, 10.0f), 1.0f);
}

TEST(PhysicsTurnRules, CountedImpactIsLeftAloneBelowTheThreshold)
{
	// |F|^2 of 0.0001 or less does not count: no impact is written
	EXPECT_FALSE(physics::turn::CountedImpact(glm::vec3(0.01f, 0.0f, 0.0f)).has_value());
	EXPECT_FALSE(physics::turn::CountedImpact(glm::vec3(0.0f)).has_value());
	const auto above = physics::turn::CountedImpact(glm::vec3(0.0101f, 0.0f, 0.0f));
	ASSERT_TRUE(above.has_value());
	EXPECT_FLOAT_EQ(*above, 0.0101f * 0.05f);
	EXPECT_FLOAT_EQ(physics::turn::CountedImpact(glm::vec3(0.0f, 200.0f, 0.0f)).value_or(0.0f), 10.0f);
}

TEST(PhysicsTurnRules, LookUpHitter)
{
	using physics::turn::HitterLookup;
	using physics::turn::LookUpHitter;
	// it touched nothing: no hitter, whether the list grew or not
	EXPECT_EQ(LookUpHitter(false, false), HitterLookup::None);
	EXPECT_EQ(LookUpHitter(false, true), HitterLookup::None);
	// it touched a body, but the list grew during the walk: the hitter stays as it was
	EXPECT_EQ(LookUpHitter(true, true), HitterLookup::Keep);
	// it touched a body: that body's place is its hitter
	EXPECT_EQ(LookUpHitter(true, false), HitterLookup::Slot);
}

TEST(PhysicsTurn, KnocksSoundWhenHitOrHardEnough)
{
	EXPECT_FALSE(physics::turn::WantsCollisionSound(true, true, 1000.0f, 1.0f));
	EXPECT_TRUE(physics::turn::WantsCollisionSound(false, true, 0.0f, 1.0f));
	EXPECT_FALSE(physics::turn::WantsCollisionSound(false, false, 4.9f, 1.0f));
	EXPECT_TRUE(physics::turn::WantsCollisionSound(false, false, 4.91f, 1.0f));
}

TEST(PhysicsTurn, ASoundIsSoftMediumOrHardByTheKnockInWeights)
{
	using physics::turn::CollisionSoundLevel;
	using physics::turn::GLoad;
	EXPECT_EQ(CollisionSoundLevel(1.2499f, false), 3);
	EXPECT_EQ(CollisionSoundLevel(1.25f, false), 2);
	EXPECT_EQ(CollisionSoundLevel(3.0f, false), 2);
	EXPECT_EQ(CollisionSoundLevel(3.0001f, false), 1);
	EXPECT_EQ(CollisionSoundLevel(3.0001f, true), 2);
	EXPECT_EQ(CollisionSoundLevel(1.0f, true), 3);
	// a weight of 0 is not guarded: a knock is then infinitely hard, and no knock is not a number, so soft
	EXPECT_EQ(CollisionSoundLevel(GLoad(10.0f, 0.0f), false), 1);
	EXPECT_EQ(CollisionSoundLevel(GLoad(0.0f, 0.0f), false), 3);
}

TEST(PhysicsTurn, ASoundsKeyTakesTheHittersAAndTheHitThingsBCode)
{
	using physics::turn::CollisionSoundKey;
	using Key = std::array<int32_t, 5>;
	// the two code tables as the original keeps them
	EXPECT_EQ(CollisionSoundKey(3, 0, 0), (Key {3, 0, 21, 11, 75}));
	EXPECT_EQ(CollisionSoundKey(1, 20, 32), (Key {1, 0, 35, 24, 75}));
	EXPECT_EQ(CollisionSoundKey(2, 13, 27), (Key {2, 0, 22, 16, 75}));
	EXPECT_EQ(CollisionSoundKey(2, 32, 29), (Key {2, 0, 42, 17, 75}));
	EXPECT_EQ(CollisionSoundKey(2, 18, 7), (Key {2, 0, 30, 9, 75}));
}

TEST(PhysicsTurn, APairStaysQuietForTwoTurns)
{
	physics::turn::SoundPairs pairs;
	EXPECT_TRUE(pairs.MaySound(Id(1), Id(2)));
	pairs.Add(Id(1), Id(2));
	EXPECT_FALSE(pairs.MaySound(Id(1), Id(2)));
	EXPECT_FALSE(pairs.MaySound(Id(2), Id(1)));
	EXPECT_TRUE(pairs.MaySound(Id(1), Id(3)));
	pairs.EndTurn();
	EXPECT_FALSE(pairs.MaySound(Id(1), Id(2)));
	pairs.EndTurn();
	EXPECT_TRUE(pairs.MaySound(Id(1), Id(2)));
	for (uint32_t i = 0; i < physics::turn::SoundPairs::k_MostPairs; ++i)
	{
		pairs.Add(Id(100 + i), Id(0));
	}
	EXPECT_EQ(pairs.Size(), physics::turn::SoundPairs::k_MostPairs);
	EXPECT_FALSE(pairs.MaySound(Id(5), Id(6)));
}

TEST(PhysicsTurn, FastThingsWhooshPastTheCamera)
{
	// The squared distance now against the one kept from the last step: from 11 m to 9 m, and from 9.5 m to 9 m
	EXPECT_TRUE(physics::turn::PassesCamera(81.0f, 121.0f, glm::vec3(-21.0f, 0, 0)));
	EXPECT_FALSE(physics::turn::PassesCamera(81.0f, 121.0f, glm::vec3(-19.0f, 0, 0)));
	EXPECT_FALSE(physics::turn::PassesCamera(81.0f, 90.25f, glm::vec3(-30.0f, 0, 0)));
}

TEST(PhysicsTurn, SinkingAndBobbing)
{
	// Out of sight only once every point is under: the centre a whole radius or more down
	EXPECT_FALSE(physics::turn::SunkOutOfSight(-0.99f, 1.0f));
	EXPECT_TRUE(physics::turn::SunkOutOfSight(-1.0f, 1.0f));
	EXPECT_FALSE(physics::turn::SunkOutOfSight(0.5f, 1.0f));
}

TEST(PhysicsRest, AtTheTurnsStartOnlyMovingBodiesAreAwake)
{
	// A resting body leaves the physics at the next turn's start unless something moving wakes it
	EXPECT_TRUE(physics::turn::AwakeAtTurnStart(false));
	EXPECT_FALSE(physics::turn::AwakeAtTurnStart(true));
}

TEST(PhysicsPairs, WhoTestsAgainstWhom)
{
	const PairMember thrown {.centre = {0, 0, 0}, .radius = 1.0f, .object = Id(1), .thrower = Id(2)};
	const PairMember thrower {.centre = {1, 0, 0}, .radius = 1.0f, .object = Id(2)};
	EXPECT_FALSE(TestsPoints(thrown, thrower));
	EXPECT_FALSE(TestsPoints(thrower, thrown));

	// A building's entry is marked as hitting no objects, so its points are never tested
	PairMember stone {.centre = {0, 0, 0}, .radius = 1.0f, .object = Id(3)};
	PairMember house {.centre = {1.5f, 0, 0}, .radius = 1.0f, .resting = true, .noObjectCollision = true, .object = Id(4)};
	EXPECT_TRUE(TestsPoints(stone, house));
	EXPECT_FALSE(TestsPoints(house, stone));
	house.centre = {2.0f, 0, 0};
	EXPECT_FALSE(TestsPoints(stone, house)) << "touching spheres don't count";

	stone.resting = true;
	house.centre = {1.0f, 0, 0};
	EXPECT_FALSE(TestsPoints(stone, house));
	stone.justSetUp = true;
	EXPECT_TRUE(TestsPoints(stone, house));

	const PairMember villager {.radius = 1.0f, .isVillager = true, .object = Id(5)};
	const PairMember pushed {.radius = 1.0f, .pushedByLiving = true, .object = Id(6)};
	EXPECT_FALSE(TestsPoints(villager, pushed));
	EXPECT_FALSE(TestsPoints(pushed, villager));
	const PairMember load {.radius = 1.0f, .noObjectCollision = true, .object = Id(7)};
	EXPECT_FALSE(TestsPoints(load, stone));
	EXPECT_TRUE(TestsPoints(PairMember {.radius = 1.0f, .object = Id(8)}, load));
}

TEST(PhysicsTurn, ADustInTheSnowTakesTheLandsLightByTheSnowsDepth)
{
	// measured on the original game's tint of a dust colour (each channel moved by the depth in 256ths, rounding down)
	EXPECT_EQ(physics::turn::BlendColour(0x50806040u, 0x00FFFFFFu, 0), 0x50806040u);
	EXPECT_EQ(physics::turn::BlendColour(0x50806040u, 0x00FFFFFFu, 64), 0x509F876Fu);
	EXPECT_EQ(physics::turn::BlendColour(0x50806040u, 0x00FFFFFFu, 128), 0x50BFAF9Fu);
	EXPECT_EQ(physics::turn::BlendColour(0x50806040u, 0x00FFFFFFu, 255), 0x50FEFEFEu);
	// towards a darker light, rounding down as well
	EXPECT_EQ(physics::turn::BlendColour(0x50806040u, 0x00102030u, 128), 0x50484038u);
}

TEST(PhysicsTurn, AFelledTreeToppledOnceItsUpAxisTipsPastUpright)
{
	EXPECT_FALSE(physics::turn::FelledTreeToppled(1.0f));
	EXPECT_FALSE(physics::turn::FelledTreeToppled(0.98f));
	EXPECT_TRUE(physics::turn::FelledTreeToppled(0.97f));
	EXPECT_TRUE(physics::turn::FelledTreeToppled(0.0f));
}

TEST(PhysicsTurn, OnlyAFelledTreeTallerThanTenMakesTheSoundOfATreeThatFalls)
{
	EXPECT_FALSE(physics::turn::FelledTreeFallSounds(5.0f));
	EXPECT_FALSE(physics::turn::FelledTreeFallSounds(10.0f));
	EXPECT_TRUE(physics::turn::FelledTreeFallSounds(10.5f));
}

TEST(PhysicsTurn, ARaisedBodyGoesOverEveryBodyButTheVillagerAndPushedPairs)
{
	// any body in the physics, whatever its class (a physical shield, a way out of a land, a rock)
	EXPECT_TRUE(physics::turn::RaisedOver(false, false, false, false));
	EXPECT_TRUE(physics::turn::RaisedOver(true, false, true, false));
	EXPECT_TRUE(physics::turn::RaisedOver(false, true, false, true));
	// a villager is not raised over what a Living pushed, nor that over a villager
	EXPECT_FALSE(physics::turn::RaisedOver(true, false, false, true));
	EXPECT_FALSE(physics::turn::RaisedOver(false, true, true, false));
}
