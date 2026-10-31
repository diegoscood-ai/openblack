/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "3D/HandPoseBuffers.h"

using openblack::HandPoseBuffers;

namespace
{
/// A one-bone pose: the bone moved along x
std::vector<glm::mat4> PoseAt(float x)
{
	glm::mat4 bone(1.0f);
	bone[3][0] = x;
	return {bone};
}

/// The frame's pose drawn: written into the drawn buffer, as the hand's draw does
void Draw(HandPoseBuffers& buffers, float x)
{
	buffers.Drawn() = PoseAt(x);
}

float DrawnX(const HandPoseBuffers& buffers)
{
	return buffers.Drawn().at(0)[3][0];
}
} // namespace

TEST(HandPoseBuffers, AChangeFadesFromThePoseDrawnLast)
{
	HandPoseBuffers buffers;
	Draw(buffers, 8.0f);
	buffers.Swap();
	Draw(buffers, 16.0f);
	ASSERT_TRUE(buffers.Blend(0.25f));
	// (16 - 8) x 0.25 + 8
	EXPECT_EQ(DrawnX(buffers), 10.0f);
}

TEST(HandPoseBuffers, TwoChangesWithNoPoseDrawnBetweenFadeFromThePoseKeptAtTheChangeBefore)
{
	HandPoseBuffers buffers;
	Draw(buffers, 8.0f);
	buffers.Swap();
	Draw(buffers, 16.0f);
	// two changes in a row, nothing drawn between them: the pose drawn last (16) is not the one faded from
	buffers.Swap();
	buffers.Swap();
	EXPECT_EQ(buffers.Kept().at(0)[3][0], 8.0f);
	Draw(buffers, 24.0f);
	ASSERT_TRUE(buffers.Blend(0.5f));
	// (24 - 8) x 0.5 + 8
	EXPECT_EQ(DrawnX(buffers), 16.0f);
}

TEST(HandPoseBuffers, AHandShownAgainFadesFromWhereItWasHidden)
{
	HandPoseBuffers buffers;
	Draw(buffers, 8.0f);
	buffers.Swap();
	Draw(buffers, 16.0f);
	// hidden: the drawn pose copied into the kept buffer, then the change's swap; nothing drawn while hidden
	buffers.KeepDrawn();
	buffers.Swap();
	// shown again
	buffers.Swap();
	EXPECT_EQ(buffers.Kept().at(0)[3][0], 16.0f);
	Draw(buffers, 32.0f);
	ASSERT_TRUE(buffers.Blend(0.25f));
	// (32 - 16) x 0.25 + 16
	EXPECT_EQ(DrawnX(buffers), 20.0f);
}

TEST(HandPoseBuffers, NoBlendBeforeAPoseIsKept)
{
	HandPoseBuffers buffers;
	Draw(buffers, 8.0f);
	EXPECT_FALSE(buffers.Blend(0.5f));
	EXPECT_EQ(DrawnX(buffers), 8.0f);
}

TEST(HandStatePose, TheFirstPoseAfterTheEnterStartsNoBlend)
{
	openblack::HandStatePose pose;
	pose.Enter();
	EXPECT_FALSE(pose.Change("Cwiggle"));
	EXPECT_FALSE(pose.Change("Cwiggle"));
}

TEST(HandStatePose, AnotherPoseStartsABlendOnce)
{
	openblack::HandStatePose pose;
	EXPECT_FALSE(pose.Change("Cwiggle"));
	EXPECT_TRUE(pose.Change("Cstroke"));
	EXPECT_FALSE(pose.Change("Cstroke"));
	EXPECT_TRUE(pose.Change("Cwiggle"));
}

TEST(HandStatePose, EnteringAgainForgetsTheLastPose)
{
	openblack::HandStatePose pose;
	EXPECT_FALSE(pose.Change("Cwiggle"));
	pose.Enter();
	EXPECT_FALSE(pose.Change("Cstroke"));
}

TEST(HandStateClock, GoesOnByTheFrameAndWrapsAtTheLengthOfTheClipShown)
{
	openblack::HandStateClock clock;
	clock.Enter();
	// the first frame after the Enter shows the clip a frame in
	EXPECT_EQ(clock.Advance(16, 1000), 16u);
	EXPECT_EQ(clock.Advance(16, 1000), 32u);
	// another clip goes on from the same time, wrapped at its own length
	EXPECT_EQ(clock.Advance(16, 1000), 48u);
	EXPECT_EQ(clock.Advance(16, 40), 24u);
	EXPECT_EQ(clock.Advance(16, 1000), 40u);
	// a clip with no length shows its start
	EXPECT_EQ(clock.Advance(16, 0), 0u);
}

TEST(HandStateClock, EnteringAgainStartsItAtNought)
{
	openblack::HandStateClock clock;
	EXPECT_EQ(clock.Advance(500, 1000), 500u);
	clock.Enter();
	EXPECT_EQ(clock.Advance(20, 1000), 20u);
}
