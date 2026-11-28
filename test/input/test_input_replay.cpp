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
#include <iterator>
#include <optional>
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
		static_cast<void>(replay::TakeRecording());
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

	/// The recording a file holds now
	[[nodiscard]] static std::optional<recording::Recording> ReadFile(const std::filesystem::path& path)
	{
		std::ifstream in(path, std::ios::binary);
		const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		return recording::Read(bytes);
	}

	/// A real mouse move to (x, y), as the window gives it
	[[nodiscard]] static SDL_Event Move(int32_t x, int32_t y)
	{
		SDL_Event event {};
		event.type = SDL_MOUSEMOTION;
		event.motion.x = x;
		event.motion.y = y;
		return event;
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

// A recording being made is in its file from its start (the header), and its events go there every k_FlushFrames of
// its frames, so a game that stops without ending keeps them up to the last time
TEST_F(InputReplayTest, ARecordingGoesIntoItsFileEverySoManyFrames)
{
	const auto path = folder.Path() / "recorded.obin";
	replay::StartRecording(path, 0, 1024, 768);
	auto inFile = ReadFile(path);
	ASSERT_TRUE(inFile.has_value());
	EXPECT_TRUE(inFile->events.empty());
	EXPECT_EQ(inFile->window, glm::ivec2(1024, 768));
	ASSERT_TRUE(replay::TakeRealEvent(Move(1, 2), 5));
	replay::FlushIfDue(5);
	EXPECT_TRUE(ReadFile(path)->events.empty());
	EXPECT_EQ(replay::CurrentStatus().flushed, 0u);
	replay::FlushIfDue(replay::k_FlushFrames);
	inFile = ReadFile(path);
	ASSERT_EQ(inFile->events.size(), 1u);
	EXPECT_EQ(inFile->events[0].frame, 5u);
	EXPECT_EQ(replay::CurrentStatus().flushed, 1u);
	// the next flush is k_FlushFrames later, and adds only what came since
	ASSERT_TRUE(replay::TakeRealEvent(Move(3, 4), replay::k_FlushFrames + 1));
	replay::FlushIfDue((2 * replay::k_FlushFrames) - 1);
	EXPECT_EQ(ReadFile(path)->events.size(), 1u);
	replay::FlushIfDue(2 * replay::k_FlushFrames);
	inFile = ReadFile(path);
	ASSERT_EQ(inFile->events.size(), 2u);
	EXPECT_EQ(inFile->events[1].values[0], 3);
	// the end writes the rest and stops
	ASSERT_TRUE(replay::TakeRealEvent(Move(5, 6), (2 * replay::k_FlushFrames) + 1));
	EXPECT_TRUE(replay::FinishRecording());
	EXPECT_FALSE(replay::IsRecording());
	EXPECT_EQ(ReadFile(path)->events.size(), 3u);
}

// A game started again records on into the same recording: the earlier game's events stay in the file, and the next
// game's frames count on from the frame after the last game's last, as a playback of it counts them
TEST_F(InputReplayTest, ARecordingTakenOutOfAGameRecordsOnInTheNext)
{
	const auto path = folder.Path() / "recorded.obin";
	EXPECT_FALSE(replay::TakeRecording().has_value());
	replay::StartRecording(path, 0, 1024, 768);
	ASSERT_TRUE(replay::TakeRealEvent(Move(1, 2), 40));
	// the game ends at its frame 99 (the run's end writes what it has)
	ASSERT_TRUE(replay::FlushRecording());
	replay::CountAgainFrom(100);
	auto taken = replay::TakeRecording();
	ASSERT_TRUE(taken.has_value());
	EXPECT_FALSE(replay::IsRecording());
	Locator::inputState::emplace<ecs::systems::InputState>();
	replay::ResumeRecording(std::move(*taken));
	ASSERT_TRUE(replay::IsRecording());
	ASSERT_TRUE(replay::TakeRealEvent(Move(7, 8), 3));
	EXPECT_TRUE(replay::FinishRecording());
	const auto inFile = ReadFile(path);
	ASSERT_TRUE(inFile.has_value());
	ASSERT_EQ(inFile->events.size(), 2u);
	EXPECT_EQ(inFile->events[0].frame, 40u);
	EXPECT_EQ(inFile->events[1].frame, 103u);
	// played back from the start, the second move comes at the next game's frame 3 again
	ASSERT_TRUE(replay::StartPlayback(path, 0));
	ASSERT_EQ(replay::DueEvents(40).size(), 1u);
	replay::CountAgainFrom(100);
	EXPECT_TRUE(replay::DueEvents(2).empty());
	ASSERT_EQ(replay::DueEvents(3).size(), 1u);
}

// --resume-input: the recording plays with the real input held back, lets go of what it left held one frame after its
// last events, then gives the input back; the new recording starts with its events and those ups, then the player's
TEST_F(InputReplayTest, AResumedRecordingLetsGoOfWhatItHeldThenTheRecordingGoesOnLive)
{
	const recording::Recording recorded {
	    .window = {1024, 768},
	    .events =
	        {
	            {.frame = 0, .kind = recording::Kind::MouseMotion, .values = {100, 200, 0, 0, 0, 0}},
	            {.frame = 4, .kind = recording::Kind::MouseButton, .values = {SDL_BUTTON_LEFT, 1, 1, 110, 210, 0}},
	        },
	};
	const auto old = WriteRecording(recorded);
	const auto next = folder.Path() / "resumed.obin";
	ASSERT_TRUE(replay::StartResume(old, 0));
	replay::StartRecording(next, 0, 1024, 768, replay::PlaybackEvents());
	EXPECT_TRUE(replay::CurrentStatus().resuming);
	// the new file holds the old events from the start
	EXPECT_EQ(ReadFile(next)->events, recorded.events);
	// while it plays the real mouse is held back and not recorded
	ASSERT_EQ(replay::DueEvents(0).size(), 1u);
	EXPECT_FALSE(replay::TakeRealEvent(Move(900, 900), 2));
	ASSERT_EQ(replay::DueEvents(4).size(), 1u);
	replay::EndIfPlayedOut(4);
	// the button it left down is let go at the next frame, still as the playback's
	EXPECT_TRUE(replay::IsPlaying());
	EXPECT_FALSE(replay::TakeHandOver().has_value());
	EXPECT_FALSE(replay::TakeRealEvent(Move(900, 900), 5));
	const auto ups = replay::DueEvents(5);
	ASSERT_EQ(ups.size(), 1u);
	EXPECT_EQ(ups[0].type, SDL_MOUSEBUTTONUP);
	EXPECT_EQ(ups[0].button.button, SDL_BUTTON_LEFT);
	EXPECT_EQ(ups[0].button.x, 110);
	replay::EndIfPlayedOut(5);
	EXPECT_FALSE(replay::IsPlaying());
	// the hand-over, once
	const auto handOver = replay::TakeHandOver();
	ASSERT_TRUE(handOver.has_value());
	EXPECT_EQ(handOver->gameFrame, 5u);
	EXPECT_EQ(handOver->recordingFrame, 5u);
	EXPECT_EQ(handOver->events, 3u);
	EXPECT_FALSE(replay::TakeHandOver().has_value());
	EXPECT_TRUE(replay::CurrentStatus().handOver.has_value());
	// the player's own input now comes in and is recorded after the ups
	EXPECT_TRUE(replay::TakeRealEvent(Move(300, 400), 6));
	EXPECT_TRUE(replay::FinishRecording());
	const auto inFile = ReadFile(next);
	ASSERT_TRUE(inFile.has_value());
	ASSERT_EQ(inFile->events.size(), 4u);
	EXPECT_EQ(inFile->events[0], recorded.events[0]);
	EXPECT_EQ(inFile->events[1], recorded.events[1]);
	EXPECT_EQ(inFile->events[2].kind, recording::Kind::MouseButton);
	EXPECT_EQ(inFile->events[2].frame, 5u);
	EXPECT_EQ(inFile->events[2].values[1], 0);
	EXPECT_EQ(inFile->events[3].frame, 6u);
	EXPECT_EQ(inFile->events[3].values[0], 300);
}

// A resumed recording that leaves nothing held gives the input back right after its last events; a plain playback
// has no hand-over
TEST_F(InputReplayTest, AResumedRecordingWithNothingHeldHandsOverAtItsEnd)
{
	const recording::Recording recorded {
	    .window = {1024, 768},
	    .events = {{.frame = 2, .kind = recording::Kind::MouseMotion, .values = {5, 6, 0, 0, 0, 0}}},
	};
	const auto old = WriteRecording(recorded);
	ASSERT_TRUE(replay::StartPlayback(old, 0));
	ASSERT_EQ(replay::DueEvents(2).size(), 1u);
	replay::EndIfPlayedOut(2);
	EXPECT_FALSE(replay::IsPlaying());
	EXPECT_FALSE(replay::TakeHandOver().has_value());
	ASSERT_TRUE(replay::StartResume(old, 0));
	ASSERT_EQ(replay::DueEvents(2).size(), 1u);
	replay::EndIfPlayedOut(2);
	EXPECT_FALSE(replay::IsPlaying());
	const auto handOver = replay::TakeHandOver();
	ASSERT_TRUE(handOver.has_value());
	EXPECT_EQ(handOver->gameFrame, 2u);
	EXPECT_EQ(handOver->events, 1u);
}
