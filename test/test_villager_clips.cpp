/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <limits>
#include <optional>

#include <ANMFile.h>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "3D/L3DAnim.h"
#include "ECS/VillagerClips.h"

using namespace openblack;
namespace villager_clips = openblack::ecs::villager_clips;

namespace
{
/// An animation file's header as the couple's kiss has it in the game's pack: 5133 ms of play in 56036 bytes of data
anm::ANMFile KissHeader()
{
	anm::ANMFile file;
	auto& header = file.GetHeader();
	header = {};
	header.unknown0x20 = 5133;
	header.frameCount = 0;
	header.animationDataSize = 56036;
	header.unknown0x50 = 0x416;
	return file;
}
} // namespace

TEST(VillagerClips, AClipLastsItsPlayTimeNotTheSizeOfItsData)
{
	L3DAnim clip;
	clip.Load(KissHeader());
	EXPECT_EQ(clip.GetPlayTime(), 5133u);
	EXPECT_EQ(clip.GetDataSize(), 56036u);
}

TEST(VillagerClips, AClipHasPlayedOnceItsTurnsCoverItsPlayTime)
{
	// The kiss of 5133 ms has played after 52 turns of 100 ms, not after 51
	EXPECT_FALSE(villager_clips::HasPlayed(51, 100, 5133, 1));
	EXPECT_TRUE(villager_clips::HasPlayed(52, 100, 5133, 1));
	// Twice through takes twice as long
	EXPECT_FALSE(villager_clips::HasPlayed(102, 100, 5133, 2));
	EXPECT_TRUE(villager_clips::HasPlayed(103, 100, 5133, 2));
	// A clip of no length has always played
	EXPECT_TRUE(villager_clips::HasPlayed(0, 100, 0, 1));
}

TEST(VillagerClips, AClipPlaysByTheGroundOnlyWithAStrideAndWithoutTheFlag)
{
	// The game sets the flag 0x200 on a clip whose stride is under 0.05 m either way, and such a clip plays by the clock
	EXPECT_TRUE(villager_clips::PlaysByGround(1.16f, 0x116));
	EXPECT_TRUE(villager_clips::PlaysByGround(0.05f, 0));
	EXPECT_FALSE(villager_clips::PlaysByGround(0.0499f, 0));
	EXPECT_TRUE(villager_clips::PlaysByGround(-1.16f, 0));
	EXPECT_FALSE(villager_clips::PlaysByGround(-0.0499f, 0));
	EXPECT_FALSE(villager_clips::PlaysByGround(0.0f, 0));
	EXPECT_FALSE(villager_clips::PlaysByGround(std::numeric_limits<float>::quiet_NaN(), 0));
	// The flag in the file keeps a clip off the ground whatever its stride
	EXPECT_FALSE(villager_clips::PlaysByGround(1.16f, 0x316));
}

TEST(VillagerClips, TheGroundCoveredPlaysWholeMillisecondsOfTheClip)
{
	// A man at 1475 map units a turn (2.25 m/s) on a 1.16 m stride of 1000 ms: 33 ms cover 0.0743 m, 64.03 ms of clip
	EXPECT_EQ(villager_clips::MovingPlay(1475, 33, 1.0f, 1.16f, 1000), 64);
	EXPECT_EQ(villager_clips::MovingPlay(1475, 100, 1.0f, 1.16f, 1000), 194);
	EXPECT_EQ(villager_clips::MovingPlay(1966, 16, 1.0f, 1.16f, 1000), 41);
	// A smaller villager takes more steps for the same ground
	EXPECT_EQ(villager_clips::MovingPlay(2621, 17, 0.9f, 1.3f, 800), 46);
	// No time or no speed plays nothing
	EXPECT_EQ(villager_clips::MovingPlay(1475, 0, 1.0f, 1.16f, 1000), 0);
	EXPECT_EQ(villager_clips::MovingPlay(0, 33, 1.0f, 1.16f, 1000), 0);
	// Less than a millisecond of clip is none: the fraction is not kept for the next frame
	EXPECT_EQ(villager_clips::MovingPlay(10, 16, 1.0f, 1.16f, 1000), 0);
	// A stride backwards plays the clip backwards, truncated towards zero
	EXPECT_EQ(villager_clips::MovingPlay(1475, 33, 1.0f, -1.16f, 1000), -64);
}

TEST(VillagerClips, TheGroundCoveredIsRoundedToSinglePrecisionAtEveryStep)
{
	// 1699 x 199 / 655350 is 0.51590909... m; to single precision that, over 0.5 and times 1100, rounds to 1135.0, which
	// the game keeps, where the exact quotient or one in double precision (1134.99999...) would truncate to 1134
	EXPECT_EQ(villager_clips::MovingPlay(1699, 199, 1.0f, 0.5f, 1100), 1135);
	// The largest speed over the longest frame on a tiny villager: 477600000 in single precision, not 477599985
	EXPECT_EQ(villager_clips::MovingPlay(65535, 199, 0.05f, 0.05f, 60000), 477600000);
	// A villager of no size covers an endless distance, which the game's conversion turns into the lowest whole number
	EXPECT_EQ(villager_clips::MovingPlay(1475, 33, 0.0f, 1.16f, 1000), std::numeric_limits<int32_t>::min());
	EXPECT_EQ(villager_clips::MovingPlay(0, 33, 0.0f, 1.16f, 1000), std::numeric_limits<int32_t>::min());
}

TEST(VillagerClips, AFramePlaysAVillagersClipByTheGroundInAMovingStateAndByTimeOtherwise)
{
	// In a moving state, a clip with a stride plays by the ground covered
	EXPECT_EQ(villager_clips::FramePlay(uint16_t {1475}, 33, 1.0f, 1.16f, 0x116, 1000),
	          villager_clips::MovingPlay(1475, 33, 1.0f, 1.16f, 1000));
	// Out of a moving state it plays by the frame's milliseconds, whatever its stride
	EXPECT_EQ(villager_clips::FramePlay(std::nullopt, 33, 1.0f, 1.16f, 0x116, 1000), 33);
	// So does a clip that does not play by the ground, in a moving state too
	EXPECT_EQ(villager_clips::FramePlay(uint16_t {1475}, 33, 1.0f, 1.16f, 0x316, 1000), 33);
	EXPECT_EQ(villager_clips::FramePlay(uint16_t {1475}, 33, 1.0f, 0.0f, 0, 1000), 33);
	// A paused frame plays nothing
	EXPECT_EQ(villager_clips::FramePlay(std::nullopt, 0, 1.0f, 1.16f, 0, 1000), 0);
}

TEST(VillagerClips, ByTimeAOneShotClipHeldAtItsEndPlaysItselfAgainEveryFrame)
{
	// The frame that reaches the end plays to it, then the whole clip once more
	const auto ends =
	    villager_clips::AdvanceClip(980, villager_clips::FramePlay(std::nullopt, 33, 1.0f, 0.0f, 0, 1000), 1000, false);
	EXPECT_EQ(ends.place, 1000);
	EXPECT_EQ(ends.played[0].from, 980);
	EXPECT_EQ(ends.played[0].to, 1000);
	EXPECT_EQ(ends.played[1].from, 0);
	EXPECT_EQ(ends.played[1].to, 1000);
	// Every frame after, a paused one included, it stays there and plays the whole clip again
	for (const uint32_t milliseconds : {33u, 16u, 0u})
	{
		const auto held = villager_clips::AdvanceClip(
		    1000, villager_clips::FramePlay(std::nullopt, milliseconds, 1.0f, 0.0f, 0, 1000), 1000, false);
		EXPECT_EQ(held.place, 1000);
		EXPECT_GE(held.played[0].from, held.played[0].to);
		EXPECT_EQ(held.played[1].from, 0);
		EXPECT_EQ(held.played[1].to, 1000);
	}
	// A looping clip by time goes on from its start by what is left over
	const auto loops =
	    villager_clips::AdvanceClip(980, villager_clips::FramePlay(std::nullopt, 33, 1.0f, 0.0f, 0, 1000), 1000, true);
	EXPECT_EQ(loops.place, 13);
	EXPECT_EQ(loops.played[1].to, 13);
}

TEST(VillagerClips, AClipShortOfItsEndMovesOnAndPlaysTheStretch)
{
	const auto step = villager_clips::AdvanceClip(200, 64, 1000, true);
	EXPECT_EQ(step.place, 264);
	EXPECT_EQ(step.played[0].from, 200);
	EXPECT_EQ(step.played[0].to, 264);
	EXPECT_EQ(step.played[1].to, 0);
	// Nothing played: the clip stays where it is
	EXPECT_EQ(villager_clips::AdvanceClip(200, 0, 1000, true).place, 200);
	// The milliseconds are whole: ten frames of less than one leave the clip where it was
	int32_t place = 0;
	for (int frame = 0; frame < 10; ++frame)
	{
		place = villager_clips::AdvanceClip(place, villager_clips::MovingPlay(10, 16, 1.0f, 1.16f, 1000), 1000, true).place;
	}
	EXPECT_EQ(place, 0);
}

TEST(VillagerClips, ALoopingClipGoesOnFromItsStart)
{
	const auto step = villager_clips::AdvanceClip(950, 64, 1000, true);
	EXPECT_EQ(step.place, 14);
	EXPECT_EQ(step.played[0].from, 950);
	EXPECT_EQ(step.played[0].to, 1000);
	EXPECT_EQ(step.played[1].from, 0);
	EXPECT_EQ(step.played[1].to, 14);
	// Exactly at the end it is at the start, and only the stretch to the end plays
	const auto atEnd = villager_clips::AdvanceClip(936, 64, 1000, true);
	EXPECT_EQ(atEnd.place, 0);
	EXPECT_EQ(atEnd.played[0].to, 1000);
	EXPECT_EQ(atEnd.played[1].to, 0);
	// More than a whole play in one frame leaves what is over the last whole play
	EXPECT_EQ(villager_clips::AdvanceClip(500, 2700, 1000, true).place, 200);
}

TEST(VillagerClips, AOneShotClipStaysAtItsEndAndPlaysAgainFromItsStart)
{
	const auto step = villager_clips::AdvanceClip(950, 64, 1000, false);
	EXPECT_EQ(step.place, 1000);
	EXPECT_EQ(step.played[0].from, 950);
	EXPECT_EQ(step.played[0].to, 1000);
	// The game plays the whole clip's stretch once more as it holds the end, frame after frame
	EXPECT_EQ(step.played[1].from, 0);
	EXPECT_EQ(step.played[1].to, 1000);
	const auto held = villager_clips::AdvanceClip(1000, 64, 1000, false);
	EXPECT_EQ(held.place, 1000);
	EXPECT_EQ(held.played[0].from, 1000);
	EXPECT_EQ(held.played[0].to, 1000);
	EXPECT_EQ(held.played[1].to, 1000);
}

TEST(VillagerClips, AClipPlayedBackPastItsStartIsDrawnFromItsStart)
{
	// Backwards inside the clip it goes back
	EXPECT_EQ(villager_clips::AdvanceClip(100, -64, 1000, true).place, 36);
	// Past the start it is drawn from 0, and nothing plays
	const auto step = villager_clips::AdvanceClip(30, -64, 1000, true);
	EXPECT_EQ(step.place, 0);
	EXPECT_GE(step.played[0].from, step.played[0].to);
	// The lowest whole number of a villager of no size wraps the sum, which is drawn from 0
	EXPECT_EQ(villager_clips::AdvanceClip(500, std::numeric_limits<int32_t>::min(), 1000, true).place, 0);
	// (openblack) a clip of no length stays at its start
	EXPECT_EQ(villager_clips::AdvanceClip(500, 64, 0, true).place, 0);
}

TEST(VillagerClips, SettingTheSameClipAgainPlaysOn)
{
	using villager_clips::ClipPlace;
	EXPECT_EQ(villager_clips::PlaceOnSetClip(true, true, false), ClipPlace::Keep);
	EXPECT_EQ(villager_clips::PlaceOnSetClip(true, false, false), ClipPlace::Keep);
	EXPECT_EQ(villager_clips::PlaceOnSetClip(true, true, true), ClipPlace::Keep);
}

TEST(VillagerClips, ADifferentClipStartsOverOnlyWhenAskedAndNotDancing)
{
	using villager_clips::ClipPlace;
	EXPECT_EQ(villager_clips::PlaceOnSetClip(false, true, false), ClipPlace::Restart);
	EXPECT_EQ(villager_clips::PlaceOnSetClip(false, false, false), ClipPlace::Keep);
	// A dancer's new clip takes over in time with the dance
	EXPECT_EQ(villager_clips::PlaceOnSetClip(false, true, true), ClipPlace::Keep);
}

TEST(VillagerClips, ADanceMoveStartsTheClipOverOnceItHasPlayedThrough)
{
	// A 1366 ms dance clip, 20 turns of 100 ms since the dancer's state changed: it starts over and the count restarts
	const auto first = villager_clips::OnDanceMove(20, 100, 1366);
	EXPECT_TRUE(first.restart);
	EXPECT_EQ(first.turnsSinceStateChange, 0u);
	// Halfway through it plays on, keeping its count
	const auto halfway = villager_clips::OnDanceMove(7, 100, 1366);
	EXPECT_FALSE(halfway.restart);
	EXPECT_EQ(halfway.turnsSinceStateChange, 7u);
}

TEST(VillagerClips, AMoveStartedOnFiveTurnsStartsTheClipOverOnce)
{
	// The dance starts a move on five turns running; the clip starts over on the first only, then plays on
	uint32_t turns = 30;
	int restarts = 0;
	for (int turn = 0; turn < 5; ++turn)
	{
		const auto move = villager_clips::OnDanceMove(turns, 100, 1366);
		restarts += move.restart ? 1 : 0;
		turns = move.turnsSinceStateChange + 1;
	}
	EXPECT_EQ(restarts, 1);
	EXPECT_EQ(turns, 5u);
}
