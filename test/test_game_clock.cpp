/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game clock (GameClock.h): the turn scheduler of LocalTimerSaysDoATurn 0x54C4A0 / ProcessNetworkPackets 0x54CC30,
// StartTurn 0x54E507, the frame clock of GGame::Loop 0x54D2B2..0x54D3A6, PauseGame 0x54AE20, SetSpeed 0x5537F0 and
// NumGameTicksPerSecond 0x711630, driven by a fake GetTickCount with frames of 33 ms.

#include <vector>

#include <gtest/gtest.h>

#include "GameClock.h"

namespace gc = openblack::game_clock;

namespace
{
uint32_t g_Now = 0;

uint32_t FakeTicks()
{
	return g_Now;
}

class GameClockTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		g_Now = 5000;
		gc::SetTickSource(&FakeTicks);
		gc::Reset();
		gc::SetTurn(0);
		gc::OnLoad();
		gc::Start(false);
	}
	void TearDown() override
	{
		gc::SetTickSource(nullptr);
		gc::Reset();
	}

	/// One loop of GGame::Loop: the turns (ProcessNetworkPackets), then the frame clock; true if a turn ran
	static bool Frame(uint32_t ms)
	{
		g_Now += ms;
		bool turned = false;
		while (gc::TurnDue())
		{
			gc::StartTurn();
			turned = true;
		}
		gc::UpdateFrameClock();
		return turned;
	}
};
} // namespace

TEST_F(GameClockTest, FirstTurnRightAwayThenEveryHundredMs)
{
	// local 0 >= turn 0 * 100: the first loop already plays turn 1, and it is counted at its start
	EXPECT_TRUE(Frame(0));
	EXPECT_EQ(gc::Turn(), 1u);
	// the frame clock jumps one turn ahead: visual = 1 * 100 + 0
	EXPECT_EQ(gc::VisualMs(), 100u);
	EXPECT_EQ(gc::FrameGameMs(), 100u);
	EXPECT_FLOAT_EQ(gc::TurnFraction(), 0.0f);
}

TEST_F(GameClockTest, ThirtyFpsKeepsTheLeftover)
{
	// 33 ms frames: with the leftover kept, 3000 ms make 30 turns (not 22, as when each turn restarts the count)
	Frame(0);
	std::vector<uint32_t> turnFrames;
	for (uint32_t i = 1; i <= 91; ++i) // 91 * 33 = 3003 ms
	{
		if (Frame(33))
		{
			turnFrames.push_back(i);
		}
	}
	EXPECT_EQ(gc::Turn(), 31u);
	ASSERT_EQ(turnFrames.size(), 30u);
	// turn 2 is due at local 100: frame 4 (132 ms), turn 3 at 200: frame 7 (231 ms), turn 4 at 300: frame 10 (330)
	EXPECT_EQ(turnFrames[0], 4u);
	EXPECT_EQ(turnFrames[1], 7u);
	EXPECT_EQ(turnFrames[2], 10u);
	// turn 5 at 400: frame 13 (429), and so on: the gaps are 3 or 4 frames, 100 ms on average
	for (size_t i = 1; i < turnFrames.size(); ++i)
	{
		const auto gap = turnFrames[i] - turnFrames[i - 1];
		EXPECT_TRUE(gap == 3u || gap == 4u) << i;
	}
}

TEST_F(GameClockTest, RemainderFractionAndFrameMs)
{
	Frame(0); // turn 1, visual 100
	Frame(33);
	EXPECT_EQ(gc::FrameGameMs(), 33u);
	EXPECT_FLOAT_EQ(gc::TurnFraction(), 33 * 0.01f);
	Frame(33);
	Frame(33); // local 99, still no turn 2: the remainder is 99
	EXPECT_EQ(gc::Turn(), 1u);
	EXPECT_EQ(gc::VisualMs(), 199u);
	EXPECT_FLOAT_EQ(gc::TurnFraction(), 99 * 0.01f);
	Frame(33); // local 132: turn 2; remainder 99 + 33 - 100 = 32
	EXPECT_EQ(gc::Turn(), 2u);
	EXPECT_EQ(gc::VisualMs(), 232u);
	EXPECT_EQ(gc::FrameGameMs(), 33u);
	EXPECT_FLOAT_EQ(gc::TurnFraction(), 32 * 0.01f);
}

TEST_F(GameClockTest, RemainderClampedAt99AndFrameMsAtMost199)
{
	Frame(0);
	// a long frame (250 ms): one turn only (1 a frame); the remainder is clamped at 99
	EXPECT_TRUE(Frame(250));
	EXPECT_EQ(gc::Turn(), 2u);
	EXPECT_EQ(gc::VisualMs(), 299u);
	EXPECT_EQ(gc::FrameGameMs(), 199u);
	EXPECT_FLOAT_EQ(gc::TurnFraction(), 0.99f);
	// the next frame catches up one more turn
	EXPECT_TRUE(Frame(1));
	EXPECT_EQ(gc::Turn(), 3u);
}

TEST_F(GameClockTest, MoreThanTwoSecondsBehindDropsTheLag)
{
	Frame(0);
	Frame(5000); // local 5000, target 100: reset to turn * 100 from now; still one turn this frame
	EXPECT_EQ(gc::Turn(), 2u);
	// the reset set the timer to 1 * 100 (the turn when it was reset): turn 3 waits until it reaches 200
	EXPECT_FALSE(Frame(50));
	EXPECT_TRUE(Frame(50));
	EXPECT_EQ(gc::Turn(), 3u);
}

TEST_F(GameClockTest, PauseStopsTheTimerKeepsTheFractionAndNoExtraTurn)
{
	Frame(0);
	Frame(33);
	Frame(33); // remainder 66
	EXPECT_FLOAT_EQ(gc::TurnFraction(), 0.66f);
	gc::Pause(true);
	for (int i = 0; i < 100; ++i) // 3.3 s paused
	{
		EXPECT_FALSE(Frame(33));
		EXPECT_EQ(gc::FrameGameMs(), 0u);
		EXPECT_FLOAT_EQ(gc::TurnFraction(), 0.66f); // kept, not 0
	}
	EXPECT_EQ(gc::Turn(), 1u);
	gc::Pause(false);
	// the paused time does not count: the timer goes on from 66
	EXPECT_FALSE(Frame(0));
	EXPECT_FALSE(Frame(33)); // 99
	EXPECT_EQ(gc::FrameGameMs(), 33u);
	EXPECT_TRUE(Frame(33)); // 132: turn 2
	EXPECT_EQ(gc::Turn(), 2u);
}

TEST_F(GameClockTest, SpeedRebasesTheTimer)
{
	Frame(0);
	Frame(50); // local 50 at speed 1
	gc::SetSpeed(2.0f);
	EXPECT_FLOAT_EQ(gc::Speed(), 2.0f);
	// the 50 ms gone keep their speed: 25 real ms more make local 100
	EXPECT_FALSE(Frame(24));
	EXPECT_TRUE(Frame(1));
	EXPECT_EQ(gc::Turn(), 2u);
	// the frame's game ms follow the speed too
	Frame(10);
	EXPECT_EQ(gc::FrameGameMs(), 20u);
	// paused, the speed is only kept and comes back with the unpause
	gc::Pause(true);
	gc::SetSpeed(0.5f);
	gc::Pause(false);
	Frame(0);
	Frame(40);
	EXPECT_EQ(gc::FrameGameMs(), 20u);
}

TEST_F(GameClockTest, OnLoadSetsTheVisualClockToTheTurn)
{
	gc::SetTurn(1234);
	gc::OnLoad();
	EXPECT_EQ(gc::VisualMs(), 123400u);
	EXPECT_EQ(gc::FrameGameMs(), 0u);
	EXPECT_FLOAT_EQ(gc::TurnFraction(), 0.0f);
	gc::Start(false);
	// ResetLocalGameTimer: the timer at 1234 * 100, so a turn is due right away
	EXPECT_TRUE(Frame(0));
	EXPECT_EQ(gc::Turn(), 1235u);
}

TEST_F(GameClockTest, TicksForSeconds)
{
	// NumGameTicksPerSecond 0x711630: ftol(1000 / 100 * s), truncated
	EXPECT_EQ(gc::TicksForSeconds(1.0f), 10);
	EXPECT_EQ(gc::TicksForSeconds(2.55f), 25);
	EXPECT_EQ(gc::TicksForSeconds(0.09f), 0);
	// SET_GAME_TICK_TIME changes the logic's ms per turn (integer division: 1000 / 300 = 3)
	gc::SetMsPerTurn(300);
	EXPECT_EQ(gc::MsPerTurn(), 300u);
	EXPECT_EQ(gc::TicksForSeconds(2.0f), 6);
}

TEST_F(GameClockTest, RealClockAndSelectors)
{
	gc::UpdateRealClock();
	g_Now += 40;
	gc::UpdateRealClock();
	EXPECT_EQ(gc::FrameRealMs(), 40u);
	gc::UpdateRealClock(); // same tick: at least 1
	EXPECT_EQ(gc::FrameRealMs(), 1u);
	g_Now += 900;
	gc::UpdateRealClock();
	EXPECT_EQ(gc::CameraFrameMs(), 900u);
	EXPECT_EQ(gc::ClampedFrameMs(true), 500u);
	// the engine timer (LH3DTech::g_timer) runs at speed 1 from tick 0: the wall clock itself
	EXPECT_EQ(gc::EngineMs(), static_cast<int32_t>(g_Now));
	Frame(0);
	EXPECT_EQ(gc::CameraFrameMs(true), gc::FrameGameMs());
	EXPECT_EQ(gc::ClampedFrameMs(), gc::FrameGameMs());
	EXPECT_EQ(gc::FrameGameMs(), 199u);
}
