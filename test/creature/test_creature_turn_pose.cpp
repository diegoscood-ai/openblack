/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature's body posed for the turn, on made-up bones: a new pose and the one before, the copy kept in the hand,
// the turn's animation step, the drawn blend, the bones seen from the body's drawn matrix and which bones are drawn, the
// file's mirror table and the mean of the bones at a branch's ends

#include <cstdint>

#include <array>
#include <bit>
#include <limits>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureTurnPose.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureTurnPose.h"

using namespace openblack;
using openblack::ecs::components::CreatureAnimation;
using openblack::ecs::components::CreatureTurnPose;

namespace
{
constexpr uint32_t k_NoParent = std::numeric_limits<uint32_t>::max();

glm::mat4 At(float x, float y, float z)
{
	return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
}

/// Three bones all at one place, which tells the poses apart
std::vector<glm::mat4> Pose(float x)
{
	return {At(x, 0.0f, 0.0f), At(x, 1.0f, 0.0f), At(x, 2.0f, 0.0f)};
}

/// A body's drawn matrix on Land 1's pen ramp: far from the land's origin, turned, and small
glm::mat4 RampRow(float scale)
{
	return glm::scale(glm::rotate(At(2546.0f, 17.0f, 2605.0f), 0.7f, glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(scale));
}

/// Three bones turned and placed about the mesh, as an animation leaves them
std::vector<glm::mat4> Turned()
{
	return {glm::rotate(At(0.0f, 1.0f, 0.0f), 0.3f, glm::vec3(1.0f, 0.0f, 0.0f)),
	        glm::rotate(At(0.5f, 6.0f, -1.0f), -1.1f, glm::vec3(0.0f, 0.0f, 1.0f)),
	        glm::rotate(At(-2.0f, 12.0f, 3.0f), 2.0f, glm::vec3(0.0f, 1.0f, 0.0f))};
}

void ExpectNear(const glm::mat4& got, const glm::mat4& want, float tolerance)
{
	for (glm::length_t column = 0; column < 4; ++column)
	{
		for (glm::length_t row = 0; row < 4; ++row)
		{
			EXPECT_NEAR(got[column][row], want[column][row], tolerance) << column << ", " << row;
		}
	}
}
} // namespace

TEST(CreatureTurnPoseStep, TheTurnTimesTheRateOfTheSizeCutTowardsZero)
{
	// 1.6 - (1 x 0.5) x 0.85 = 1.175, x 100 = 117.5 in floats, cut to 117 rather than rounded to 118
	EXPECT_EQ(creature_turn_pose::Step(100, 1.0f, false, false), 117);
	EXPECT_EQ(creature_turn_pose::Step(100, 0.5f, false, false), 138); // 138.75
	EXPECT_EQ(creature_turn_pose::Step(100, 2.0f, false, false), 75);
	EXPECT_EQ(creature_turn_pose::Step(100, 3.0f, false, false), 32); // 32.499992 in floats
	// past a size of about 3.76 the step goes back, still cut towards zero: -3.3 is -3
	EXPECT_EQ(creature_turn_pose::Step(33, 4.0f, false, false), -3);
}

TEST(CreatureTurnPoseStep, AtTheRealSpeedTheTurnsOwnAndDoubledTwice)
{
	EXPECT_EQ(creature_turn_pose::Step(100, 1.0f, true, false), 100);
	EXPECT_EQ(creature_turn_pose::Step(100, 1.0f, false, true), 234);
	EXPECT_EQ(creature_turn_pose::Step(100, 1.0f, true, true), 200);
}

TEST(CreatureTurnPoseLivedMs, TheTurnAtTheScaleCutTowardsZero)
{
	EXPECT_EQ(creature_turn_pose::LivedMs(1.0f, 100), 100);
	EXPECT_EQ(creature_turn_pose::LivedMs(0.5f, 100), 50);
	EXPECT_EQ(creature_turn_pose::LivedMs(0.333f, 100), 33); // 33.3
	EXPECT_EQ(creature_turn_pose::LivedMs(0.0f, 100), 0);
}

TEST(CreatureTurnPoseLivedMs, TheScaleIsHeldToZeroToOne)
{
	EXPECT_EQ(creature_turn_pose::LivedMs(1.5f, 100), 100);
	EXPECT_EQ(creature_turn_pose::LivedMs(-0.5f, 100), 0);
}

TEST(CreatureTurnPoseClockFor, TheMillisecondsTheirSecondsAndTheStep)
{
	const auto normal = creature_turn_pose::ClockFor(100, 1.0f, false, false);
	EXPECT_EQ(normal.ms, 100);
	EXPECT_EQ(normal.seconds, 100.0f * 0.001f);
	EXPECT_EQ(normal.step, 117);

	// frozen to a third at size 2: 33 x 0.75 = 24.75, cut to 24
	const auto frozen = creature_turn_pose::ClockFor(33, 2.0f, false, false);
	EXPECT_EQ(frozen.ms, 33);
	EXPECT_EQ(frozen.seconds, 33.0f * 0.001f);
	EXPECT_EQ(frozen.step, 24);

	EXPECT_EQ(creature_turn_pose::ClockFor(100, 1.0f, true, false).step, 100);
	EXPECT_EQ(creature_turn_pose::ClockFor(100, 1.0f, false, true).step, 234);
}

// The numbers below are the original's own: its turn clock run on a spread of turns, sizes and switches
// (docs/wiki/creature/pose-and-animation.md#the-turns-clocks)
TEST(CreatureTurnPoseStep, TheOriginalsStepsOverSizesAndTurns)
{
	struct Case
	{
		int32_t ms;
		float size;
		int32_t step;
	};
	constexpr std::array k_Cases {
	    Case {.ms = 1, .size = 0.05f, .step = 1},     Case {.ms = 7, .size = 0.05f, .step = 11},
	    Case {.ms = 100, .size = 0.05f, .step = 157}, Case {.ms = 200, .size = 0.05f, .step = 315},
	    Case {.ms = 199, .size = 0.1f, .step = 309},  Case {.ms = 117, .size = 0.75f, .step = 149},
	    Case {.ms = 50, .size = 0.7f, .step = 65},    Case {.ms = 99, .size = 1.3f, .step = 103},
	    Case {.ms = 66, .size = 1.3f, .step = 69},    Case {.ms = 1, .size = 1.5f, .step = 0},
	    Case {.ms = 200, .size = 1.5f, .step = 192},  Case {.ms = 100, .size = 1.75f, .step = 85},
	    Case {.ms = 117, .size = 2.5f, .step = 62},   Case {.ms = 199, .size = 3.0f, .step = 64},
	    Case {.ms = 200, .size = 3.0f, .step = 64},   Case {.ms = 117, .size = 4.0f, .step = -11},
	    Case {.ms = 0, .size = 1.0f, .step = 0},
	};
	for (const auto& c : k_Cases)
	{
		EXPECT_EQ(creature_turn_pose::Step(c.ms, c.size, false, false), c.step) << c.ms << " ms at size " << c.size;
	}
	// doubled after the cut: 65 x 2 and 10 x 2
	EXPECT_EQ(creature_turn_pose::Step(50, 0.7f, false, true), 130);
	EXPECT_EQ(creature_turn_pose::Step(33, 3.0f, false, true), 20);
}

TEST(CreatureTurnPoseClockFor, TheSecondsAreTheOriginalsFloats)
{
	struct Case
	{
		int32_t ms;
		uint32_t secondsBits;
	};
	constexpr std::array k_Cases {
	    Case {.ms = 1, .secondsBits = 0x3A83126Fu},
	    Case {.ms = 10, .secondsBits = 0x3C23D70Bu},
	    Case {.ms = 33, .secondsBits = 0x3D072B02u},
	    Case {.ms = 66, .secondsBits = 0x3D872B02u},
	    Case {.ms = 75, .secondsBits = 0x3D99999Au},
	    Case {.ms = 90, .secondsBits = 0x3DB851ECu},
	    Case {.ms = 100, .secondsBits = 0x3DCCCCCDu},
	    Case {.ms = 199, .secondsBits = 0x3E4BC6A9u},
	    Case {.ms = 0, .secondsBits = 0u},
	};
	for (const auto& c : k_Cases)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(creature_turn_pose::ClockFor(c.ms, 1.0f, false, false).seconds), c.secondsBits)
		    << c.ms << " ms";
	}
}

TEST(CreatureTurnPoseClockOfTurn, AFrozenTurnLivesOneLessTheFrozenAmount)
{
	// the 100 ms turn at the scale 1 less the frozen amount, as a float, cut towards zero (0.9 frozen: 10.000002 is 10);
	// then its step at the size
	struct Case
	{
		float frozen;
		float size;
		int32_t ms;
		int32_t step;
	};
	constexpr std::array k_Cases {
	    Case {.frozen = 0.0f, .size = 1.0f, .ms = 100, .step = 117},
	    Case {.frozen = 0.1f, .size = 1.0f, .ms = 90, .step = 105}, // 0.89999998 in floats, x 100 rounds to 90
	    Case {.frozen = 0.25f, .size = 0.5f, .ms = 75, .step = 104},
	    Case {.frozen = 0.333f, .size = 1.0f, .ms = 66, .step = 77},
	    Case {.frozen = 0.5f, .size = 2.0f, .ms = 50, .step = 37},
	    Case {.frozen = 0.9f, .size = 2.0f, .ms = 10, .step = 7},
	    Case {.frozen = 0.99f, .size = 1.0f, .ms = 0, .step = 0},
	    Case {.frozen = 1.0f, .size = 1.0f, .ms = 0, .step = 0},
	};
	for (const auto& c : k_Cases)
	{
		const auto clock = creature_turn_pose::ClockOfTurn(1.0f - c.frozen, 100, c.size, false, false);
		EXPECT_EQ(clock.ms, c.ms) << "frozen " << c.frozen;
		EXPECT_EQ(clock.step, c.step) << "frozen " << c.frozen;
		EXPECT_EQ(clock.seconds, static_cast<float>(c.ms) * 0.001f) << "frozen " << c.frozen;
	}
	EXPECT_EQ(creature_turn_pose::ClockOfTurn(1.0f, 100, 1.0f, true, false).step, 100);
	EXPECT_EQ(creature_turn_pose::ClockOfTurn(1.0f, 100, 1.0f, false, true).step, 234);
	EXPECT_EQ(creature_turn_pose::ClockOfTurn(1.0f, 100, 1.0f, true, true).step, 200);
}

TEST(CreatureTurnPoseAdvance, NoPoseYetThenTheFirstIsAlsoThePrevious)
{
	CreatureTurnPose pose;
	EXPECT_FALSE(creature_turn_pose::HasPose(pose));
	EXPECT_TRUE(creature_turn_pose::Safe(pose).empty());
	EXPECT_TRUE(creature_turn_pose::Drawn(pose, 0.5f).empty());

	creature_turn_pose::Advance(pose, Pose(1.0f));
	EXPECT_TRUE(creature_turn_pose::HasPose(pose));
	EXPECT_EQ(pose.current, Pose(1.0f));
	EXPECT_EQ(pose.previous, Pose(1.0f));
}

TEST(CreatureTurnPoseAdvance, EachPoseMovesTheCurrentToThePrevious)
{
	CreatureTurnPose pose;
	creature_turn_pose::Advance(pose, Pose(1.0f));
	creature_turn_pose::Advance(pose, Pose(2.0f));
	EXPECT_EQ(pose.previous, Pose(1.0f));
	EXPECT_EQ(pose.current, Pose(2.0f));
	// a second pose in the same turn moves it on again
	creature_turn_pose::Advance(pose, Pose(3.0f));
	EXPECT_EQ(pose.previous, Pose(2.0f));
	EXPECT_EQ(pose.current, Pose(3.0f));
}

TEST(CreatureTurnPoseSafe, TheKeptPoseWhileHeldThenTheCurrent)
{
	CreatureTurnPose pose;
	creature_turn_pose::Advance(pose, Pose(1.0f));
	EXPECT_EQ(creature_turn_pose::Safe(pose).data(), pose.current.data());

	// held before the turn's pose: the game keeps reading the turn before's while the current moves on
	creature_turn_pose::Freeze(pose);
	creature_turn_pose::Advance(pose, Pose(2.0f));
	const auto held = creature_turn_pose::Safe(pose);
	EXPECT_EQ(std::vector<glm::mat4>(held.begin(), held.end()), Pose(1.0f));
	EXPECT_EQ(pose.current, Pose(2.0f));

	// taken again by the hand, it keeps the current one
	creature_turn_pose::Freeze(pose);
	const auto again = creature_turn_pose::Safe(pose);
	EXPECT_EQ(std::vector<glm::mat4>(again.begin(), again.end()), Pose(2.0f));

	creature_turn_pose::Reconnect(pose);
	EXPECT_FALSE(pose.frozen.has_value());
	EXPECT_EQ(creature_turn_pose::Safe(pose).data(), pose.current.data());
}

TEST(CreatureTurnPoseBlend, TheFirstAssignsTheNextAdd)
{
	std::vector<glm::mat4> dst(2, glm::mat4(5.0f));
	std::vector<glm::mat4> src(2, glm::mat4(2.0f));
	src[1][3] = glm::vec4(4.0f, 8.0f, 12.0f, 2.0f);
	creature_turn_pose::BlendInto(dst, src, 0.25f, true);
	EXPECT_EQ(dst[0][0], glm::vec4(0.5f, 0.0f, 0.0f, 0.0f));
	creature_turn_pose::BlendInto(dst, src, 0.5f, false);
	EXPECT_EQ(dst[1][2], glm::vec4(0.0f, 0.0f, 1.5f, 0.0f));
	// the place too, not only the diagonal
	EXPECT_EQ(glm::vec3(dst[1][3]), glm::vec3(3.0f, 6.0f, 9.0f));
	// the bottom row is not blended
	EXPECT_EQ(dst[1][3][3], 1.0f);
	EXPECT_EQ(dst[1][0][3], 0.0f);
}

TEST(CreatureTurnPoseBlend, OnlyTheBonesBothHave)
{
	std::vector<glm::mat4> dst(3, glm::mat4(1.0f));
	const std::vector<glm::mat4> src(2, glm::mat4(2.0f));
	creature_turn_pose::BlendInto(dst, src, 1.0f, true);
	// blended but for the bottom row, which stays that of an affine matrix
	auto blended = glm::mat4(2.0f);
	blended[3][3] = 1.0f;
	EXPECT_EQ(dst[1], blended);
	EXPECT_EQ(dst[2], glm::mat4(1.0f));
}

TEST(CreatureTurnPoseDrawn, BetweenThePreviousAndTheCurrent)
{
	CreatureTurnPose pose;
	creature_turn_pose::Advance(pose, Pose(2.0f));
	creature_turn_pose::Advance(pose, Pose(4.0f));
	EXPECT_EQ(creature_turn_pose::Drawn(pose, 0.0f), Pose(2.0f));
	EXPECT_EQ(creature_turn_pose::Drawn(pose, 1.0f), Pose(4.0f));
	EXPECT_EQ(creature_turn_pose::Drawn(pose, 0.5f), Pose(3.0f));
}

TEST(CreatureTurnPoseDrawn, PastTheTurnItGoesOnUpToTwice)
{
	CreatureTurnPose pose;
	creature_turn_pose::Advance(pose, Pose(2.0f));
	creature_turn_pose::Advance(pose, Pose(4.0f));
	// 1.5 x 4 + (1 - 1.5) x 2
	EXPECT_EQ(creature_turn_pose::Drawn(pose, 1.5f), Pose(5.0f));
	// taken as 2: 2 x 4 - 2
	EXPECT_EQ(creature_turn_pose::Drawn(pose, 3.0f), Pose(6.0f));
}

TEST(CreatureTurnPoseWorld, TheBodysMatrixBeforeEachBone)
{
	const auto world = At(10.0f, 0.0f, 0.0f) * glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
	const auto placed = creature_turn_pose::InWorld(Pose(1.0f), world);
	ASSERT_EQ(placed.size(), 3u);
	EXPECT_EQ(glm::vec3(placed[2][3]), glm::vec3(12.0f, 4.0f, 0.0f));
	EXPECT_TRUE(creature_turn_pose::InWorld({}, world).empty());
}

TEST(CreatureTurnPoseRelative, SeenFromTheRowTheBonesComeBackFromTheWorld)
{
	const auto row = glm::scale(glm::rotate(At(10.0f, 2.0f, -5.0f), 0.7f, glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(0.5f));
	const auto bones = Turned();

	const auto relative = creature_turn_pose::RelativeTo(row, creature_turn_pose::InWorld(bones, row));

	ASSERT_EQ(relative.size(), bones.size());
	for (size_t bone = 0; bone < bones.size(); ++bone)
	{
		ExpectNear(relative[bone], bones[bone], 1e-5f);
	}
}

TEST(CreatureTurnPoseRelative, FarFromTheOriginTheRowDrawsTheWorldsBonesAgain)
{
	// kilometres out and small, the row times each bone is the world's bone to a tenth of a millimetre
	const auto row = RampRow(0.0947f);
	const auto world = creature_turn_pose::InWorld(Turned(), row);

	const auto relative = creature_turn_pose::RelativeTo(row, world);

	ASSERT_EQ(relative.size(), world.size());
	for (size_t bone = 0; bone < world.size(); ++bone)
	{
		ExpectNear(glm::mat4(glm::dmat4(row) * glm::dmat4(relative[bone]) - glm::dmat4(world[bone])), glm::mat4(0.0f), 1e-4f);
	}
}

TEST(CreatureTurnPoseRelative, OnlyTheRowsTopThreeRowsAreRead)
{
	auto row = RampRow(0.5f);
	const auto world = creature_turn_pose::InWorld(Turned(), row);
	const auto relative = creature_turn_pose::RelativeTo(row, world);
	// an instance row's bottom row carries no placement
	row[0][3] = 7.0f;
	row[3][3] = 3.0f;
	EXPECT_EQ(creature_turn_pose::RelativeTo(row, world), relative);
}

TEST(CreatureTurnPoseRelative, NothingFromARowThatCannotBeInverted)
{
	EXPECT_TRUE(creature_turn_pose::RelativeTo(glm::mat4(0.0f), Pose(1.0f)).empty());
	EXPECT_TRUE(creature_turn_pose::RelativeTo(RampRow(1.0f), {}).empty());
}

TEST(CreatureTurnPoseRelative, OnThePenRampTheDrawnSizeGrowsWithinTheTurn)
{
	// two turns of Land 1's ramp: the body placed at the pen's size of each
	CreatureTurnPose pose;
	creature_turn_pose::Advance(pose, creature_turn_pose::InWorld(Turned(), RampRow(0.09469f)));
	creature_turn_pose::Advance(pose, creature_turn_pose::InWorld(Turned(), RampRow(0.29814f)));
	const auto row = RampRow(0.09469f);

	// at the turn's start the body is the turn before's, half way it is half way between them
	const auto start = creature_turn_pose::RelativeTo(row, creature_turn_pose::Drawn(pose, 0.0f));
	ExpectNear(start[0], Turned()[0], 5e-3f);
	const auto half = creature_turn_pose::Drawn(pose, 0.5f);
	EXPECT_FLOAT_EQ(glm::length(glm::vec3(half[0][0])), 0.5f * 0.09469f + 0.5f * 0.29814f);
	EXPECT_EQ(creature_turn_pose::Drawn(pose, 0.0f), pose.previous);
}

TEST(CreatureTurnPoseBonesDrawn, TheBlendedBonesWhenThereAreAnyElseTheFramesPose)
{
	CreatureAnimation animation;
	animation.boneMatrices = Pose(1.0f);
	EXPECT_EQ(&creature_turn_pose::BonesDrawn(animation), &animation.boneMatrices);
	animation.drawnBones = Pose(2.0f);
	EXPECT_EQ(&creature_turn_pose::BonesDrawn(animation), &animation.drawnBones);
	animation.drawnBones.clear();
	EXPECT_EQ(&creature_turn_pose::BonesDrawn(animation), &animation.boneMatrices);
}

TEST(CreatureTurnPoseMirror, TheFilesLastNumberForEachBone)
{
	// two numbers before the bones' table, then bone 0 <-> 2 and bone 1 its own
	const std::vector<int32_t> tail {7, 9, 2, 1, 0};
	EXPECT_EQ(creature_turn_pose::MirrorBone(tail, 3, 0), 2u);
	EXPECT_EQ(creature_turn_pose::MirrorBone(tail, 3, 1), 1u);
	EXPECT_EQ(creature_turn_pose::MirrorBone(tail, 3, 2), 0u);
	EXPECT_FALSE(creature_turn_pose::MirrorBone(tail, 3, 3).has_value());
	// a table shorter than the bones, or naming no bone
	EXPECT_FALSE(creature_turn_pose::MirrorBone(tail, 6, 0).has_value());
	const std::vector<int32_t> none {-1, 5, 0};
	EXPECT_FALSE(creature_turn_pose::MirrorBone(none, 3, 0).has_value());
	EXPECT_FALSE(creature_turn_pose::MirrorBone(none, 3, 1).has_value());
}

TEST(CreatureTurnPoseLeafMean, TheFingertipsUnderTheHand)
{
	// root, arm, hand; under the hand a finger of two bones (3, then its tip 6) and two tips (4, 5)
	const std::vector<uint32_t> parents {k_NoParent, 0, 1, 2, 2, 2, 3};
	const std::vector<glm::mat4> bones {At(0, 0, 0), At(1, 0, 0), At(2, 0, 0), At(3, 0, 0),
	                                    At(4, 1, 0), At(5, 2, 0), At(9, 3, 0)};
	const auto mean = creature_turn_pose::LeafMean(parents, bones, 2);
	ASSERT_TRUE(mean.has_value());
	// tips 6, 4 and 5: (9 + 4 + 5, 3 + 1 + 2) x 1/3, the finger's base 3 not counted
	const float third = 1.0f / 3.0f;
	EXPECT_EQ(*mean, glm::vec3(third * 18.0f, third * 6.0f, 0.0f));
	// a tip is its own mean
	EXPECT_EQ(creature_turn_pose::LeafMean(parents, bones, 6), glm::vec3(9.0f, 3.0f, 0.0f));
}

TEST(CreatureTurnPoseLeafMean, TheBonesAfterTheStartUnderItsParentCountToo)
{
	// the walk goes on from the start bone along the bones after it under the same parent: from tip 4, tip 5 as well
	const std::vector<uint32_t> parents {k_NoParent, 0, 1, 2, 2, 2, 3};
	const std::vector<glm::mat4> bones {At(0, 0, 0), At(1, 0, 0), At(2, 0, 0), At(3, 0, 0),
	                                    At(4, 1, 0), At(6, 2, 0), At(9, 3, 0)};
	EXPECT_EQ(creature_turn_pose::LeafMean(parents, bones, 4), glm::vec3(0.5f * 10.0f, 0.5f * 3.0f, 0.0f));
}

TEST(CreatureTurnPoseLeafMean, TheSumIsRoundedAfterEachAdd)
{
	// 1e8 + 4 is 1e8 in floats, and + 4 again still 1e8; as (4 + 4) + 1e8 it would be 1e8 + 8
	const std::vector<uint32_t> parents {k_NoParent, 0, 0, 0};
	const std::vector<glm::mat4> bones {At(0, 0, 0), At(1e8f, 0, 0), At(4, 0, 0), At(4, 0, 0)};
	const auto mean = creature_turn_pose::LeafMean(parents, bones, 0);
	ASSERT_TRUE(mean.has_value());
	EXPECT_EQ(mean->x, (1.0f / 3.0f) * 1e8f);
	EXPECT_NE(mean->x, (1.0f / 3.0f) * (1e8f + 8.0f));
}

TEST(CreatureTurnPoseLeafMean, NoneWithoutTheBones)
{
	const std::vector<uint32_t> parents {k_NoParent, 0};
	EXPECT_FALSE(creature_turn_pose::LeafMean(parents, std::vector<glm::mat4>(2), 2).has_value());
	EXPECT_FALSE(creature_turn_pose::LeafMean(parents, std::vector<glm::mat4>(1), 0).has_value());
}
