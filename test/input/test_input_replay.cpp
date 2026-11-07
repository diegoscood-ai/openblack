/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A recording played back: its last events are given while it still plays, so that a last mouse move is taken as the
// recording's, and the playback ends only after them

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Systems/Implementations/InputState.h"
#include "FileSystem/DefaultFileSystem.h"
#include "Input/InputRecording.h"
#include "Input/InputReplay.h"
#include "Locator.h"
#include "TestTempFolder.h"

using namespace openblack;
using namespace openblack::input;

namespace
{
/// An input state and a file system of the test's own for the playback, the ones from before put back after
class InputReplayTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the playback logs its start and its end at info level (SPDLOG_LOGGER_INFO needs the logger)
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		_previousInput = Locator::inputState::handle();
		_previousFiles = Locator::filesystem::handle();
		Locator::inputState::emplace<ecs::systems::InputState>();
		Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
	}
	void TearDown() override
	{
		replay::StopPlayback();
		Locator::filesystem::reset(_previousFiles);
		Locator::inputState::reset(_previousInput);
	}

	/// The recording written to a file of the test's folder
	[[nodiscard]] std::filesystem::path WriteRecording(const recording::Recording& recorded) const
	{
		const auto path = folder.Path() / "replay.obin";
		const auto bytes = recording::Write(recorded);
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		return path;
	}

	test::TestTempFolder folder;

private:
	Locator::inputState::node_type _previousInput;
	Locator::filesystem::node_type _previousFiles;
};
} // namespace

TEST_F(InputReplayTest, ALastMouseMoveIsGivenWhileThePlaybackStillPlays)
{
	const recording::Recording recorded {
	    .window = {1024, 768},
	    .events =
	        {
	            {.frame = 0, .kind = recording::Kind::MouseButton, .values = {SDL_BUTTON_LEFT, 1, 1, 100, 200, 0}},
	            {.frame = 3, .kind = recording::Kind::MouseMotion, .values = {300, 400, 200, 200, 0, 0}},
	        },
	};
	ASSERT_TRUE(replay::StartPlayback(WriteRecording(recorded), 10));
	ASSERT_TRUE(replay::IsPlaying());
	// the button at the start frame, nothing in between
	ASSERT_EQ(replay::DueEvents(10).size(), 1u);
	EXPECT_TRUE(replay::DueEvents(12).empty());
	EXPECT_TRUE(replay::IsPlaying());
	// the last frame gives the move, and the playback still plays while it is dispatched
	const auto last = replay::DueEvents(13);
	ASSERT_EQ(last.size(), 1u);
	EXPECT_EQ(last.front().type, SDL_MOUSEMOTION);
	EXPECT_EQ(last.front().motion.x, 300);
	EXPECT_EQ(last.front().motion.y, 400);
	EXPECT_TRUE(replay::IsPlaying());
	// the next call has nothing, and the playback has ended
	EXPECT_TRUE(replay::DueEvents(14).empty());
	EXPECT_FALSE(replay::IsPlaying());
}

TEST_F(InputReplayTest, ThePlaybackEndsOnceItsLastEventsAreDispatched)
{
	const recording::Recording recorded {
	    .window = {1024, 768},
	    .events = {{.frame = 0, .kind = recording::Kind::MouseMotion, .values = {5, 6, 0, 0, 0, 0}}},
	};
	ASSERT_TRUE(replay::StartPlayback(WriteRecording(recorded), 0));
	// nothing has been given yet: it keeps playing
	replay::EndIfPlayedOut(0);
	EXPECT_TRUE(replay::IsPlaying());
	ASSERT_EQ(replay::DueEvents(0).size(), 1u);
	EXPECT_TRUE(replay::IsPlaying());
	replay::EndIfPlayedOut(0);
	EXPECT_FALSE(replay::IsPlaying());
	EXPECT_TRUE(replay::DueEvents(1).empty());
}

TEST_F(InputReplayTest, APlaybackTakenOutOfAGamePlaysOnInTheNext)
{
	const recording::Recording recorded {
	    .window = {1024, 768},
	    .events =
	        {
	            {.frame = 0, .kind = recording::Kind::MouseMotion, .values = {1, 2, 0, 0, 0, 0}},
	            {.frame = 445, .kind = recording::Kind::MouseMotion, .values = {512, 344, 0, 0, 0, 0}},
	            {.frame = 600, .kind = recording::Kind::MouseMotion, .values = {862, 634, 0, 0, 0, 0}},
	        },
	};
	EXPECT_FALSE(replay::TakePlayback().has_value());
	ASSERT_TRUE(replay::StartPlayback(WriteRecording(recorded), 0));
	ASSERT_EQ(replay::DueEvents(0).size(), 1u);
	// the game ends at its frame 443; the next game's input state has no playback until it is given back
	replay::CountAgainFrom(444);
	auto taken = replay::TakePlayback();
	ASSERT_TRUE(taken.has_value());
	EXPECT_FALSE(replay::IsPlaying());
	Locator::inputState::emplace<ecs::systems::InputState>();
	replay::ResumePlayback(std::move(*taken));
	ASSERT_TRUE(replay::IsPlaying());
	// recording frame 445 is the new count's 1
	EXPECT_TRUE(replay::DueEvents(0).empty());
	const auto move = replay::DueEvents(1);
	ASSERT_EQ(move.size(), 1u);
	EXPECT_EQ(move.front().motion.x, 512);
	// a box's 100 frames, then recording frame 600 is the next game's frame 56
	replay::CountAgainFrom(100);
	EXPECT_TRUE(replay::DueEvents(55).empty());
	ASSERT_EQ(replay::DueEvents(56).size(), 1u);
}
