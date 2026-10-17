/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A deterministic run's clock: the time system's ticks counted from a restart, the pinned date and the fixed step by
// the time system's names, on a clock of its own driven by a fake tick count; and the machine's clock as the game
// reads it, through a time system or without one, and the night voices' date read through it. And the run's seed
// of openblack's own generators (not the original's): a stream each, the same draws for the same seed.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <ctime>

#include <array>
#include <chrono>
#include <set>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/Services/SpookyVoices.h"
#include "Common/MachineClock.h"
#include "Common/RandomNumberManager.h"
#include "Common/RandomNumberManagerProduction.h"
#include "Common/RandomNumberManagerTesting.h"
#include "ECS/Systems/Implementations/TimeSystem.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack;
using openblack::ecs::systems::TimeSystem;
using openblack::ecs::systems::TimeSystemInterface;

namespace
{
uint32_t g_Now = 0;

uint32_t FakeTicks()
{
	return g_Now;
}

/// A clock of its own on the fake tick count
struct Clock
{
	Clock()
	{
		g_Now = 123456;
		time.SetTickSource(&FakeTicks);
	}
	TimeSystem time;
};
} // namespace

// Until it is restarted, the clock as the game reads it is the clock's tick count, and the date the wall clock's
TEST(RunSeed, UnrestartedTicksAreTheTickCount)
{
	Clock clock;
	EXPECT_EQ(clock.time.GetTicks(), clock.time.TickCount());
	EXPECT_EQ(clock.time.GetTicks(), 123456u);
	EXPECT_FALSE(clock.time.GetPinnedDate().has_value());
	const auto wall =
	    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	EXPECT_NEAR(static_cast<double>(clock.time.GetUnixTime()), static_cast<double>(wall), 2.0);
}

// The ticks count from 0 when the clock is restarted, and wrap as the machine's count does
TEST(RunSeed, TicksCountFromARestart)
{
	Clock clock;
	clock.time.RestartClock(std::nullopt);
	EXPECT_EQ(clock.time.GetTicks(), 0u);
	g_Now += 250;
	EXPECT_EQ(clock.time.GetTicks(), 250u);
	EXPECT_FALSE(clock.time.GetPinnedDate().has_value());

	g_Now = 0xFFFFFFF0u;
	clock.time.RestartClock(std::nullopt);
	g_Now += 0x20u;
	EXPECT_EQ(clock.time.GetTicks(), 0x20u);
}

// With a fixed frame time the ticks move on by it each frame, whatever the wall clock does
TEST(RunSeed, TicksFollowAFixedFrameTime)
{
	Clock clock;
	clock.time.SetFixedFrameTime(std::chrono::milliseconds(16));
	EXPECT_EQ(clock.time.GetFixedFrameTime(), std::chrono::milliseconds(16));
	EXPECT_EQ(clock.time.FixedFrameMs(), 16u);
	clock.time.RestartClock(TimeSystemInterface::k_DeterministicDate);
	for (int frame = 0; frame < 5; ++frame)
	{
		g_Now += 1000 + static_cast<uint32_t>(frame * 37);
		clock.time.AdvanceFixedFrame();
	}
	EXPECT_EQ(clock.time.GetTicks(), 80u);

	clock.time.SetFixedFrameTime(std::nullopt);
	EXPECT_FALSE(clock.time.GetFixedFrameTime().has_value());
	EXPECT_EQ(clock.time.FixedFrameMs(), 0u);
}

// A pinned date moves on with the ticks; without one the date is the wall clock's
TEST(RunSeed, APinnedDateMovesOnWithTheTicks)
{
	Clock clock;
	clock.time.RestartClock(TimeSystemInterface::k_DeterministicDate);
	EXPECT_EQ(clock.time.GetPinnedDate(), 978350400);
	EXPECT_EQ(clock.time.GetUnixTime(), 978350400);
	g_Now += 2500;
	EXPECT_EQ(clock.time.GetUnixTime(), 978350402);
	clock.time.RestartClock(std::nullopt);
	EXPECT_FALSE(clock.time.GetPinnedDate().has_value());
	// The wall clock's date: well after the pinned one
	EXPECT_GT(clock.time.GetUnixTime(), 1'600'000'000);
}

// The pinned date is 1 January 2001 at noon, UTC
TEST(RunSeed, TheDeterministicDateIsTheFirstOf2001)
{
	constexpr int64_t k_DaysTo2001 = 11'323; // 1970 to 2000 inclusive: 31 years, 8 of them leap
	EXPECT_EQ(TimeSystemInterface::k_DeterministicDate, k_DaysTo2001 * 86'400 + 12 * 3'600);
}

// The machine's clock reads the time system it is given: its ticks from the restart and its pinned date
TEST(RunSeed, TheMachineClockReadsTheTimeSystem)
{
	g_Now = 5000;
	auto& time = Locator::time::emplace<TimeSystem>();
	time.SetTickSource(&FakeTicks);
	EXPECT_EQ(machine_clock::Ticks(), 5000u);
	time.RestartClock(TimeSystemInterface::k_DeterministicDate);
	g_Now += 3250;
	EXPECT_EQ(machine_clock::Ticks(), 3250u);
	EXPECT_EQ(machine_clock::UnixTime(), TimeSystemInterface::k_DeterministicDate + 3);
	// a fresh clock back for the services torn down after the test
	Locator::time::emplace<TimeSystem>();
}

// Without a time system the machine's clock is the wall clock's
TEST(RunSeed, TheMachineClockWithoutATimeSystemIsTheWallClocks)
{
	Locator::time::reset();
	const auto ticks = game_clock::WallTicks();
	EXPECT_LE(machine_clock::Ticks() - ticks, 1000u);
	const auto wall =
	    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	EXPECT_NEAR(static_cast<double>(machine_clock::UnixTime()), static_cast<double>(wall), 2.0);
	Locator::time::emplace<TimeSystem>();
}

// The night voices read the date as the game does: the time system's, pinned in a seeded run, unless a test gives a
// clock of its own
TEST(RunSeed, TheNightVoicesReadTheGamesDate)
{
	auto& time = Locator::time::emplace<TimeSystem>();
	time.SetTickSource(&FakeTicks);
	time.RestartClock(TimeSystemInterface::k_DeterministicDate);
	EXPECT_EQ(audio::spooky::UnixTime(), TimeSystemInterface::k_DeterministicDate);
	audio::spooky::SetClock([] { return int64_t {42}; });
	EXPECT_EQ(audio::spooky::UnixTime(), 42);
	audio::spooky::SetClock({});
	Locator::time::emplace<TimeSystem>();
}

namespace
{
constexpr auto k_Streams = static_cast<size_t>(RandomStream::Count);

std::vector<uint32_t> Draws(std::mt19937& generator, int count = 8)
{
	std::vector<uint32_t> draws;
	for (int i = 0; i < count; ++i)
	{
		draws.push_back(generator());
	}
	return draws;
}

std::vector<int> GeneralDraws(RandomNumberManagerInterface& rng, int count = 8)
{
	std::vector<int> draws;
	for (int i = 0; i < count; ++i)
	{
		draws.push_back(rng.NextValue(0, 1'000'000));
	}
	return draws;
}
} // namespace

// Every stream starts from its own seed, made from the run's: the same for the same run seed, different between streams
// and between close run seeds
TEST(RunSeed, EachStreamHasItsOwnSeedFromTheRunsSeed)
{
	std::set<uint32_t> seeds;
	for (size_t stream = 0; stream < k_Streams; ++stream)
	{
		seeds.insert(run_seed::StreamSeed(42, static_cast<RandomStream>(stream)));
		seeds.insert(run_seed::StreamSeed(43, static_cast<RandomStream>(stream)));
	}
	EXPECT_EQ(seeds.size(), 2 * k_Streams);
	static_assert(run_seed::StreamSeed(42, RandomStream::CreatureMind) == run_seed::StreamSeed(42, RandomStream::CreatureMind));
}

// The same seed gives the same draws in every stream and in the general draws, however much was drawn before
TEST(RunSeed, TheSameSeedDrawsTheSameNumbers)
{
	RandomNumberManagerProduction rng;
	rng.SetRunSeed(42);
	EXPECT_EQ(rng.GetRunSeed(), 42u);
	std::array<std::vector<uint32_t>, k_Streams> first;
	for (size_t stream = 0; stream < k_Streams; ++stream)
	{
		first.at(stream) = Draws(rng.Stream(static_cast<RandomStream>(stream)));
	}
	const auto general = GeneralDraws(rng);

	// Drawn differently meanwhile, then seeded again
	Draws(rng.Stream(RandomStream::CreatureFight), 100);
	GeneralDraws(rng, 7);
	rng.SetRunSeed(42);
	for (size_t stream = 0; stream < k_Streams; ++stream)
	{
		EXPECT_EQ(Draws(rng.Stream(static_cast<RandomStream>(stream))), first.at(stream)) << stream;
	}
	EXPECT_EQ(GeneralDraws(rng), general);

	// Streams don't share their draws: one drawing more doesn't move another
	EXPECT_NE(first.at(static_cast<size_t>(RandomStream::CreatureMind)),
	          first.at(static_cast<size_t>(RandomStream::CreatureFight)));
	rng.SetRunSeed(43);
	EXPECT_NE(GeneralDraws(rng), general);
}

// Unless seeded, the run's seed is the time the game started at, as the one generator was seeded before
TEST(RunSeed, TheDefaultRunSeedIsTheStartTime)
{
	const auto before = static_cast<uint32_t>(std::time(nullptr));
	const RandomNumberManagerProduction rng;
	const auto after = static_cast<uint32_t>(std::time(nullptr));
	EXPECT_GE(rng.GetRunSeed(), before);
	EXPECT_LE(rng.GetRunSeed(), after);
}

// The testing manager seeds alike
TEST(RunSeed, TheTestingManagerSeedsAlike)
{
	RandomNumberManagerTesting testing;
	RandomNumberManagerProduction production;
	testing.SetRunSeed(9);
	production.SetRunSeed(9);
	EXPECT_EQ(Draws(testing.Stream(RandomStream::CreatureMind)), Draws(production.Stream(RandomStream::CreatureMind)));
	EXPECT_EQ(GeneralDraws(testing), GeneralDraws(production));
}

// Without the game's random numbers (a system made alone in a test) a stream draws from a generator of its own on the
// standard seed, so the same each time
TEST(RunSeed, AStreamSourceWithoutTheGameDrawsTheStandardSequence)
{
	const auto previous = Locator::rng::handle();
	Locator::rng::reset();
	RandomStreamSource source(RandomStream::CreatureLocomotion);
	std::mt19937 standard;
	for (int i = 0; i < 5; ++i)
	{
		EXPECT_EQ(source(), standard());
	}
	Locator::rng::reset(previous);
}

// With the game's random numbers given, a stream source draws from the run's stream
TEST(RunSeed, AStreamSourceDrawsTheRunsStream)
{
	const auto previous = Locator::rng::handle();
	Locator::rng::emplace<RandomNumberManagerTesting>().SetRunSeed(5);
	RandomNumberManagerTesting expected;
	expected.SetRunSeed(5);
	RandomStreamSource source(RandomStream::CreatureMind);
	for (const auto draw : Draws(expected.Stream(RandomStream::CreatureMind), 5))
	{
		EXPECT_EQ(source(), draw);
	}
	Locator::rng::reset(previous);
}
