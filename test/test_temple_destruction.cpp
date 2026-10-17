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

#include "Temple/TempleDestruction.h"

using namespace openblack::temple_destruction;

namespace
{
/// Every turn's events at a tenth of a second a turn, from the first turn on, until the end
std::vector<Events> RunToEnd()
{
	std::vector<Events> turns;
	float clock = 0.0f;
	for (int turn = 0; turn < 400; ++turn)
	{
		const float before = clock;
		clock += 100.0f * 0.001f;
		turns.push_back(Between(before, clock));
		if (turns.back().end)
		{
			break;
		}
	}
	return turns;
}
} // namespace

TEST(TempleDestruction, EachMomentHappensOnceOnTheTurnTheClockPassesIt)
{
	const auto turns = RunToEnd();
	const auto count = [&turns](auto field) {
		int n = 0;
		for (const auto& events : turns)
		{
			n += (events.*field) ? 1 : 0;
		}
		return n;
	};
	EXPECT_EQ(count(&Events::loopStarts), 1);
	EXPECT_EQ(count(&Events::shellShown), 1);
	EXPECT_EQ(count(&Events::shellGoes), 1);
	EXPECT_EQ(count(&Events::glow), 1);
	EXPECT_EQ(count(&Events::explosion), 1);
	EXPECT_EQ(count(&Events::smoke), 1);
	EXPECT_EQ(count(&Events::end), 1);
	// The loop on the first turn, the end with the smoke on the same last turn, about 22 seconds on
	const bool loopsFirst = turns.front().loopStarts;
	const bool smokesLast = turns.back().smoke;
	EXPECT_TRUE(loopsFirst);
	EXPECT_TRUE(smokesLast);
	// The shell goes with the smoke, at the end
	EXPECT_TRUE(turns.back().shellGoes);
	EXPECT_GE(turns.size(), 220u);
	EXPECT_LE(turns.size(), 221u);
}

TEST(TempleDestruction, AMomentIsPassedFromAtItToBeforeItAfter)
{
	EXPECT_TRUE(Passes(0.0f, 0.0f, 0.1f));
	EXPECT_FALSE(Passes(0.0f, 0.1f, 0.1f));
	EXPECT_FALSE(Passes(0.1f, 0.0f, 0.2f));
}

TEST(TempleDestruction, TheHeartFadesFromFourteenSecondsOverFour)
{
	EXPECT_EQ(HeartAlpha(0.0f), 255);
	EXPECT_EQ(HeartAlpha(14.0f), 255);
	EXPECT_EQ(HeartAlpha(16.0f), 127);
	EXPECT_EQ(HeartAlpha(18.0f), 0);
	EXPECT_EQ(HeartAlpha(21.0f), 0);
}

TEST(TempleDestruction, BeamsComeFasterBrighterAndShorterLivedOverFourteenSeconds)
{
	EXPECT_FALSE(Beaming(-0.1f));
	EXPECT_TRUE(Beaming(0.0f));
	EXPECT_TRUE(Beaming(13.9f));
	EXPECT_FALSE(Beaming(14.0f));
	EXPECT_EQ(BeamShare(-1.0f), 0.0f);
	EXPECT_EQ(BeamShare(7.0f), 0.5f);
	EXPECT_EQ(BeamShare(20.0f), 1.0f);
	EXPECT_FLOAT_EQ(NextBeam(1.0f, 0.0f), 1.4f);
	EXPECT_FLOAT_EQ(NextBeam(1.0f, 0.5f), 1.3f);
	EXPECT_FLOAT_EQ(NextBeam(1.0f, 1.0f), 1.2f);
	const auto first = BeamLookAt(0.0f);
	EXPECT_EQ(first.life, 3.0f);
	EXPECT_EQ(first.speed, 1.0f);
	EXPECT_EQ(first.alpha, 50);
	const auto last = BeamLookAt(1.0f);
	EXPECT_FLOAT_EQ(last.life, 0.7f);
	EXPECT_EQ(last.speed, 1.5f);
	EXPECT_EQ(last.alpha, 200);
	// The alpha is cut down to a whole number
	EXPECT_EQ(BeamLookAt(0.999f).alpha, 199);
}

// Each step is rounded to a float, as the game's single precision rounds it: worked out at a higher precision and
// rounded once, these two would each be one unit apart
TEST(TempleDestruction, TheBeamsStepsAreSingleFloats)
{
	const auto share = std::bit_cast<float>(0x3F55F4B4u);
	EXPECT_EQ(std::bit_cast<uint32_t>(NextBeam(std::bit_cast<float>(0x40C1E131u), share)), 0x40C954ACu);
	EXPECT_EQ(std::bit_cast<uint32_t>(BeamLookAt(std::bit_cast<float>(0x3F4324CAu)).life), 0x3F9F95B2u);
}

TEST(TempleDestruction, TheLocalHeartbeatQuickensAlongItsThreePoints)
{
	// Before the first point and from the last on, their own pace
	EXPECT_EQ(HeartbeatPace(-1.0f), k_HeartbeatPace[0]);
	EXPECT_EQ(HeartbeatPace(0.0f), k_HeartbeatPace[0]);
	EXPECT_EQ(HeartbeatPace(25.0f), k_HeartbeatPace[2]);
	EXPECT_EQ(HeartbeatPace(30.0f), k_HeartbeatPace[2]);
	// At a point, its pace; between two, drawn straight along, a float a step
	EXPECT_EQ(HeartbeatPace(19.0f), k_HeartbeatPace[1]);
	EXPECT_EQ(std::bit_cast<uint32_t>(HeartbeatPace(0.1f)), 0x3F23D519u);
	EXPECT_EQ(std::bit_cast<uint32_t>(HeartbeatPace(9.5f)), 0x3F747644u);
	EXPECT_EQ(std::bit_cast<uint32_t>(HeartbeatPace(21.5f)), 0x4019A338u);
	EXPECT_EQ(std::bit_cast<uint32_t>(HeartbeatPace(22.0f)), 0x40281150u);
}

TEST(TempleDestruction, TheGlowBeatsFromAQuarterToWhole)
{
	EXPECT_EQ(GlowPower(0.0f), 0.25f);
	EXPECT_EQ(GlowPower(0.5f), 0.625f);
	EXPECT_EQ(GlowPower(1.0f), 1.0f);
}

TEST(TempleDestruction, SpotVisualsLastTheirSecondsInTurns)
{
	EXPECT_EQ(TurnsFor(100, k_GlowSeconds), 60);
	EXPECT_EQ(TurnsFor(100, k_ExplosionSeconds), 150);
	EXPECT_EQ(SmokeTurns(100, 0.0f), 60);
	EXPECT_EQ(SmokeTurns(100, 0.4f), 90);
	EXPECT_EQ(SmokeTurns(100, 0.123f), 69);
}

TEST(TempleDestruction, OnlyTheLocalPlayersLossOutsideSkirmishesAndOnlineGamesEndsTheGameOnce)
{
	EXPECT_TRUE(GameOverStarts({.localTempleDestroying = true}));
	EXPECT_FALSE(GameOverStarts({}));
	EXPECT_FALSE(GameOverStarts({.over = true, .localTempleDestroying = true}));
	EXPECT_FALSE(GameOverStarts({.skirmish = true, .localTempleDestroying = true}));
	EXPECT_FALSE(GameOverStarts({.multiplayer = true, .localTempleDestroying = true}));
}
