/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <limits>
#include <numbers>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/SkeletalAnimation.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"

using namespace openblack;
using namespace openblack::creature_eyes;

namespace
{
constexpr float k_Tolerance = 1e-4f;
constexpr auto k_Pi = std::numbers::pi_v<float>;
constexpr LidAngles k_Lids {.open = 0.45f, .closed = -0.5f, .calm = 0.25f};

/// Always the top of the range, so the wait between blinks is the longest
uint32_t Highest(uint32_t range)
{
	return range - 1;
}
} // namespace

TEST(CreatureEyes, BlinksCloseAndOpenOverTwoHundredMillisecondsEach)
{
	Blink blink;
	blink = AdvanceBlink(blink, 999, Highest);
	EXPECT_EQ(blink.state, Blink::State::Open);
	blink = AdvanceBlink(blink, 1, Highest);
	EXPECT_EQ(blink.state, Blink::State::Closing);
	EXPECT_EQ(blink.timerMs, 200);
	blink = AdvanceBlink(blink, 200, Highest);
	EXPECT_EQ(blink.state, Blink::State::Opening);
	blink = AdvanceBlink(blink, 250, Highest);
	EXPECT_EQ(blink.state, Blink::State::Open);
	// Up to one and a half times the interval until the next
	EXPECT_EQ(blink.timerMs, 4999 + 2500);
}

TEST(CreatureEyes, EachBlinkDrawsItsWaitOnceAsTheEyesOpenAgain)
{
	// The game's local generator from a new campaign's seed, whose draws of 5000 the original gives as 1878 and 4348
	uint32_t seed = game_random::k_InitialSeed;
	int draws = 0;
	const auto local = [&seed, &draws](uint32_t range) {
		++draws;
		return game_random::SeededRandom(range, seed);
	};
	Blink blink;
	blink = AdvanceBlink(blink, 1000, local);
	blink = AdvanceBlink(blink, 200, local);
	EXPECT_EQ(draws, 0);
	blink = AdvanceBlink(blink, 200, local);
	EXPECT_EQ(draws, 1);
	EXPECT_EQ(blink.state, Blink::State::Open);
	EXPECT_EQ(blink.timerMs, 1878 + 2500);
	// the open time runs out, then the next blink
	blink = AdvanceBlink(blink, 1878 + 2500, local);
	blink = AdvanceBlink(blink, 200, local);
	blink = AdvanceBlink(blink, 200, local);
	EXPECT_EQ(draws, 2);
	EXPECT_EQ(blink.timerMs, 4348 + 2500);
	EXPECT_EQ(seed, 0x5BAD6184u);
}

TEST(CreatureEyes, ThePointIsOnTheTriangleAndItsNormalPointsIn)
{
	const auto point = PointOnTriangle({0, 0, 0}, {2, 0, 0}, {0, 2, 0}, 0.25f, 0.5f);
	EXPECT_NEAR(point.position.x, 0.5f, k_Tolerance);
	EXPECT_NEAR(point.position.y, 1.0f, k_Tolerance);
	// The third edge crossed with the second
	EXPECT_NEAR(point.normal.z, -4.0f, k_Tolerance);
}

TEST(CreatureEyes, SmallCreaturesHaveBiggerEyesForTheirSize)
{
	// At size 1: a tenth, by 1.3 for the head and 1.0 for the eyes
	EXPECT_NEAR(EyeSize(1.0f, 1.0f, 0.0f, Mode::Calm), 0.13f, k_Tolerance);
	EXPECT_NEAR(EyeSize(2.0f, 1.0f, 0.0f, Mode::Calm), 0.2f * 0.8f * 0.9f, k_Tolerance);
	EXPECT_NEAR(EyeSize(1.0f, 1.0f, 1.0f, Mode::Calm), 0.13f * 1.05f, k_Tolerance);
	EXPECT_NEAR(EyeSize(1.0f, 1.0f, 0.0f, Mode::Stoned), 0.13f * 1.1f, k_Tolerance);
}

TEST(CreatureEyes, OpenEyesTurnFaster)
{
	EXPECT_NEAR(LookSeconds(0.0f, Mode::Calm), 0.15f, k_Tolerance);
	EXPECT_NEAR(LookSeconds(1.0f, Mode::Calm), 0.1f, k_Tolerance);
	EXPECT_NEAR(LookSeconds(0.0f, Mode::Stoned), 0.75f, k_Tolerance);
}

TEST(CreatureEyes, EyesNeverLookBackIntoTheHead)
{
	const glm::vec3 inward {0.0f, 0.0f, 1.0f};
	// Looking along the surface is pulled a little inwards
	const auto clamped = ClampLook({1.0f, 0.0f, 0.0f}, inward);
	EXPECT_NEAR(glm::length(clamped), 1.0f, k_Tolerance);
	EXPECT_NEAR(clamped.z, 0.1f / std::sqrt(1.01f), k_Tolerance);
	// Looking well inwards is left alone
	const auto ahead = ClampLook({0.0f, 0.6f, 0.8f}, inward);
	EXPECT_NEAR(ahead.y, 0.6f, k_Tolerance);
	EXPECT_NEAR(ahead.z, 0.8f, k_Tolerance);
}

TEST(CreatureEyes, TheEyeballFacesAwayFromWhereItLooks)
{
	const auto frame = EyeballFrame({1, 2, 3}, {0.0f, 0.0f, 1.0f});
	// Turned half round about z
	EXPECT_NEAR(frame.x.x, -1.0f, k_Tolerance);
	EXPECT_NEAR(frame.y.y, -1.0f, k_Tolerance);
	EXPECT_NEAR(frame.z.z, 1.0f, k_Tolerance);
	const auto matrix = ToMatrix(frame, 0.5f);
	// The pupil, at -2 along z of the mesh, ends up half that far along -z from the centre
	const auto pupil = matrix * glm::vec4(0.0f, 0.0f, -2.0f, 1.0f);
	EXPECT_NEAR(pupil.z, 2.0f, k_Tolerance);
	EXPECT_NEAR(pupil.x, 1.0f, k_Tolerance);
}

TEST(CreatureEyes, TheLeftAndRightLidsTurnTowardsTheirAnchors)
{
	const glm::vec3 inward {0.0f, 0.0f, 1.0f};
	const auto left = EyelidFrame({0, 0, 0}, inward, {1, 0, 0}, false);
	const auto right = EyelidFrame({0, 0, 0}, inward, {1, 0, 0}, true);
	EXPECT_NEAR(left.z.z, -1.0f, k_Tolerance);
	// Squared up through the lid's y, which leaves its x pointing away from the anchor on the left
	EXPECT_NEAR(left.x.x, -1.0f, k_Tolerance);
	EXPECT_NEAR(right.x.x, 1.0f, k_Tolerance);
	EXPECT_NEAR(glm::dot(left.x, left.y), 0.0f, k_Tolerance);
}

TEST(CreatureEyes, LidAnglesByMode)
{
	const Blink open;
	EXPECT_NEAR(EyelidAngle(Mode::Wide, k_Lids, 0.0f, 0.0f, open), 0.45f * k_Pi, k_Tolerance);
	EXPECT_NEAR(EyelidAngle(Mode::Closed, k_Lids, 0.0f, 0.0f, open), -0.5f * k_Pi, k_Tolerance);
	EXPECT_NEAR(EyelidAngle(Mode::Calm, k_Lids, 0.0f, 0.0f, open), 0.25f * k_Pi, k_Tolerance);
	// A calm lid follows the pupil and opens with the eyes
	EXPECT_NEAR(EyelidAngle(Mode::Calm, k_Lids, 0.5f, 0.5f, open), (0.25f * k_Pi) - std::asin(0.5f) + 0.15f, k_Tolerance);
}

TEST(CreatureEyes, CalmLidsSwingShutOverABlink)
{
	const auto calm = 0.25f * k_Pi;
	const auto closed = -0.5f * k_Pi;
	const Blink starting {.state = Blink::State::Closing, .timerMs = 200, .intervalMs = 5000};
	const Blink halfShut {.state = Blink::State::Closing, .timerMs = 100, .intervalMs = 5000};
	const Blink shut {.state = Blink::State::Opening, .timerMs = 200, .intervalMs = 5000};
	EXPECT_NEAR(EyelidAngle(Mode::Calm, k_Lids, 0.0f, 0.0f, starting), calm, k_Tolerance);
	EXPECT_NEAR(EyelidAngle(Mode::Calm, k_Lids, 0.0f, 0.0f, halfShut), (calm + closed) / 2.0f, k_Tolerance);
	EXPECT_NEAR(EyelidAngle(Mode::Calm, k_Lids, 0.0f, 0.0f, shut), closed, k_Tolerance);
	// Wide open eyes don't blink
	EXPECT_NEAR(EyelidAngle(Mode::Wide, k_Lids, 0.0f, 0.0f, halfShut), 0.45f * k_Pi, k_Tolerance);
}

TEST(CreatureEyes, TurningALidKeepsItsAxesSquare)
{
	const auto lid = Turned(EyelidFrame({0, 0, 0}, {0, 0, 1}, {1, 0, 0}, false), 0.7f);
	EXPECT_NEAR(glm::dot(lid.y, lid.z), 0.0f, k_Tolerance);
	EXPECT_NEAR(glm::length(lid.y), 1.0f, k_Tolerance);
	EXPECT_NEAR(LidPitch(lid, EyeballFrame({0, 0, 0}, lid.y)), 1.0f, k_Tolerance);
}

TEST(CreatureEyes, YoungCreaturesDrawTheirHeadsHandsFeetAndEyesBigger)
{
	// a creature made at the start size 0.22: its head 1.69 times bigger, its hands and feet 1.445, its eyes 1.356
	const auto young = creature_morph::PartScalesAt(0.22f);
	EXPECT_EQ(young.head, 1.8f - (0.5f * 0.22f));
	EXPECT_EQ(young.limbs, 1.5f - (0.25f * 0.22f));
	EXPECT_EQ(young.eyes, static_cast<float>(static_cast<double>(1.4f) - (static_cast<double>(0.2f) * 0.22f)));
	EXPECT_NEAR(young.head, 1.69f, k_Tolerance);
	EXPECT_NEAR(young.limbs, 1.445f, k_Tolerance);
	EXPECT_NEAR(young.eyes, 1.356f, k_Tolerance);
	// from size 2 on they are no smaller; at size 0, or a size that is not a number, they are at their biggest
	for (const float size : {2.0f, 4.0f})
	{
		const auto grown = creature_morph::PartScalesAt(size);
		EXPECT_FLOAT_EQ(grown.head, 0.8f);
		EXPECT_FLOAT_EQ(grown.limbs, 1.0f);
		EXPECT_FLOAT_EQ(grown.eyes, 1.0f);
	}
	for (const float size : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN()})
	{
		const auto least = creature_morph::PartScalesAt(size);
		EXPECT_EQ(least.head, 1.8f);
		EXPECT_EQ(least.limbs, 1.5f);
		EXPECT_EQ(least.eyes, 1.4f);
	}
}

TEST(CreatureEyes, TheEyesAreSizedByTheHeadTheyGrowWith)
{
	// an eye is the head's growth times a tenth of the size, by the eyes' own factor: the head they sit in grows by the
	// same, so they keep their proportion to it at every size
	for (const float size : {0.22f, 0.5f, 1.0f, 2.0f})
	{
		const auto head = creature_morph::PartScalesAt(size).head;
		EXPECT_NEAR(EyeSize(size, 1.0f, 0.0f, Mode::Calm), head * (1.1f - (0.1f * size)) * size * 0.1f, k_Tolerance);
	}
}

TEST(CreatureEyes, ADrawnSizeGrowsTheCollarTheHandsTheFeetAndTheEyesInTurn)
{
	creature::CreatureRig rig {};
	rig.leashBone = 1;
	rig.rightEye = 4;
	rig.actionPoints = creature::CreatureRig::ActionPoints {
	    .rightHand = 2,
	    .rightFoot = 3,
	    .rightArmpit = 0,
	    .belly = 0,
	    .head = 0,
	    .groin = 0,
	    .pickUpMs = 0.0f,
	    .destroyMs = 0.0f,
	    .discardMs = 0.0f,
	    .eatMs = 0.0f,
	    .throwMs = 0.0f,
	    .putDownMs = 0.0f,
	    .catchMs = 0.0f,
	    .takeFromHandMs = 0.0f,
	    .kickMs = 0.0f,
	};
	// the mirror table ends the file: the right hand's mirror is bone 5, the foot and the eye are their own
	rig.fileTail = {7, 7, 0, 1, 5, 3, 4, 2};
	const auto scales = creature_morph::PartScalesAt(0.22f);
	const auto joints = creature::GrownJoints(rig, 6, 0.22f);
	const std::vector<std::pair<uint32_t, float>> expected {
	    {1, scales.head},  {2, scales.limbs}, {5, scales.limbs}, {3, scales.limbs},
	    {3, scales.limbs}, {4, scales.eyes},  {4, scales.eyes},
	};
	ASSERT_EQ(joints.size(), expected.size());
	for (size_t i = 0; i < joints.size(); ++i)
	{
		EXPECT_EQ(joints[i].joint, expected[i].first);
		EXPECT_EQ(joints[i].scale, expected[i].second);
	}
	// a bone the body lacks is left out, and without the bones named nothing grows
	EXPECT_EQ(creature::GrownJoints(rig, 2, 0.22f).size(), 1u);
	EXPECT_TRUE(creature::GrownJoints(creature::CreatureRig {}, 6, 0.22f).empty());
}

TEST(CreatureEyes, AGrownBoneGrowsAllThatHangsFromItAboutItsJoint)
{
	creature::CreatureRig rig {};
	rig.leashBone = 1;
	// a root, the collar a unit above it, and the head a unit above the collar
	const skeletal_animation::Pose still {.rotation = skeletal_animation::k_Identity, .translation = glm::vec3(0.0f)};
	std::vector<skeletal_animation::Pose> poses(3, still);
	poses[1].translation = {0.0f, 1.0f, 0.0f};
	poses[2].translation = {0.0f, 1.0f, 0.0f};
	const std::vector<uint32_t> parents {skeletal_animation::k_NoParent, 0, 1};

	creature::GrowParts(poses, rig, 0.22f);
	const auto head = creature_morph::PartScalesAt(0.22f).head;
	EXPECT_EQ(poses[1].rotation[1][1], head);
	EXPECT_EQ(poses[1].rotation[0][1], 0.0f);
	const auto bones = skeletal_animation::ComposeBoneMatrices(poses, parents);
	// the collar stays where it is, and the head moves away from it and grows by the head's scale
	EXPECT_NEAR(bones[1][3].y, 1.0f, k_Tolerance);
	EXPECT_NEAR(bones[2][3].y, 1.0f + head, k_Tolerance);
	EXPECT_NEAR(glm::length(glm::vec3(bones[2][0])), head, k_Tolerance);
	EXPECT_NEAR(glm::length(glm::vec3(bones[0][0])), 1.0f, k_Tolerance);
}
