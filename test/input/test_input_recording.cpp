/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "Input/InputRecording.h"

using namespace openblack::input::recording;

namespace
{
/// One of each kind, over three frames
Recording Sample()
{
	return Recording {
	    .window = {1280, 720},
	    .events = {
	        Event {.frame = 0, .kind = Kind::MouseMotion, .values = {640, 360, 3, -2, 1, 0}},
	        Event {.frame = 0, .kind = Kind::MouseButton, .values = {SDL_BUTTON_LEFT, 1, 1, 640, 360, 0}},
	        Event {.frame = 2, .kind = Kind::MouseWheel, .values = {0, -1, SDL_MOUSEWHEEL_NORMAL, 0, 0, 0}},
	        Event {.frame = 5, .kind = Kind::Key, .values = {SDL_SCANCODE_SPACE, SDLK_SPACE, KMOD_LSHIFT, 0, 1, 0}},
	    }};
}
} // namespace

TEST(InputRecording, WrittenAndReadBackIsTheSame)
{
	const auto recording = Sample();
	const auto bytes = Write(recording);
	ASSERT_EQ(bytes.size(), k_HeaderSize + (4 * k_EventSize));
	EXPECT_EQ(bytes[0], 'O');
	EXPECT_EQ(bytes[3], 'N');
	EXPECT_EQ(bytes[4], k_Version);
	const auto read = Read(bytes);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read, recording);
}

TEST(InputRecording, NotARecordingOfThisVersionReadsAsNone)
{
	auto bytes = Write(Sample());
	// another magic, another version, too short, an unknown kind
	auto other = bytes;
	other[0] = 'X';
	EXPECT_FALSE(Read(other).has_value());
	other = bytes;
	other[4] = 2;
	EXPECT_FALSE(Read(other).has_value());
	EXPECT_FALSE(Read(std::span(bytes).first(k_HeaderSize - 1)).has_value());
	other = bytes;
	other[k_HeaderSize + 4] = 9;
	EXPECT_FALSE(Read(other).has_value());
	// a short last event is left out
	bytes.resize(bytes.size() - 1);
	const auto read = Read(bytes);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->events.size(), 3u);
}

TEST(InputRecording, EachEventGoesBackToTheSdlEventItCameFrom)
{
	SDL_Event key {};
	key.type = SDL_KEYUP;
	key.key.state = SDL_RELEASED;
	key.key.keysym.scancode = SDL_SCANCODE_L;
	key.key.keysym.sym = SDLK_l;
	key.key.keysym.mod = KMOD_LCTRL;
	const auto recorded = FromSdl(key, 7);
	ASSERT_TRUE(recorded.has_value());
	EXPECT_EQ(recorded->frame, 7u);
	EXPECT_EQ(recorded->kind, Kind::Key);
	const auto back = ToSdl(*recorded);
	EXPECT_EQ(back.type, SDL_KEYUP);
	EXPECT_EQ(back.key.keysym.scancode, SDL_SCANCODE_L);
	EXPECT_EQ(back.key.keysym.sym, SDLK_l);
	EXPECT_EQ(back.key.keysym.mod, KMOD_LCTRL);

	SDL_Event button {};
	button.type = SDL_MOUSEBUTTONDOWN;
	button.button.button = SDL_BUTTON_RIGHT;
	button.button.clicks = 2;
	button.button.x = 10;
	button.button.y = 20;
	const auto down = ToSdl(*FromSdl(button, 0));
	EXPECT_EQ(down.type, SDL_MOUSEBUTTONDOWN);
	EXPECT_EQ(down.button.button, SDL_BUTTON_RIGHT);
	EXPECT_EQ(down.button.state, SDL_PRESSED);
	EXPECT_EQ(down.button.clicks, 2);
	EXPECT_EQ(down.button.x, 10);
	EXPECT_EQ(down.button.y, 20);

	SDL_Event motion {};
	motion.type = SDL_MOUSEMOTION;
	motion.motion.x = 5;
	motion.motion.y = 6;
	motion.motion.xrel = -1;
	const auto moved = ToSdl(*FromSdl(motion, 0));
	EXPECT_EQ(moved.motion.x, 5);
	EXPECT_EQ(moved.motion.y, 6);
	EXPECT_EQ(moved.motion.xrel, -1);

	SDL_Event window {};
	window.type = SDL_WINDOWEVENT;
	EXPECT_FALSE(FromSdl(window, 0).has_value());
}

TEST(InputRecording, OnlyTheMouseAndTheKeysAreKeptAndHeldBackWhileReplaying)
{
	EXPECT_TRUE(IsRecordable(SDL_MOUSEMOTION));
	EXPECT_TRUE(IsRecordable(SDL_MOUSEWHEEL));
	EXPECT_TRUE(IsRecordable(SDL_KEYDOWN));
	EXPECT_FALSE(IsRecordable(SDL_QUIT));
	EXPECT_FALSE(IsRecordable(SDL_WINDOWEVENT));
	EXPECT_TRUE(DropWhileReplaying(SDL_KEYUP, true));
	EXPECT_FALSE(DropWhileReplaying(SDL_KEYUP, false));
	EXPECT_FALSE(DropWhileReplaying(SDL_QUIT, true));
	// the text a key types, which the name boxes read, is kept and held back too
	EXPECT_TRUE(IsRecordable(SDL_TEXTINPUT));
	EXPECT_TRUE(DropWhileReplaying(SDL_TEXTINPUT, true));
}

TEST(InputRecording, TypedTextIsKeptAndGoesBackToTheSameText)
{
	SDL_Event typed {};
	typed.type = SDL_TEXTINPUT;
	SDL_strlcpy(typed.text.text, "Test", sizeof(typed.text.text));
	EXPECT_FALSE(FromSdl(typed, 3).has_value());
	const auto pieces = TextFromSdl(typed, 3);
	ASSERT_EQ(pieces.size(), 1u);
	EXPECT_EQ(pieces[0].frame, 3u);
	EXPECT_EQ(pieces[0].kind, Kind::Text);
	const auto back = ToSdl(pieces[0]);
	EXPECT_EQ(back.type, SDL_TEXTINPUT);
	EXPECT_STREQ(back.text.text, "Test");
	// written and read back the same
	const Recording recording {.window = {1024, 768}, .events = pieces};
	const auto read = Read(Write(recording));
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read, recording);
	// none for no text or another event
	typed.text.text[0] = '\0';
	EXPECT_TRUE(TextFromSdl(typed, 0).empty());
	SDL_Event key {};
	key.type = SDL_KEYDOWN;
	EXPECT_TRUE(TextFromSdl(key, 0).empty());
}

TEST(InputRecording, ALongTextIsCutBetweenCharacters)
{
	// 23 letters then a two-byte character: the first piece stops before the character, which goes whole in the second
	SDL_Event typed {};
	typed.type = SDL_TEXTINPUT;
	SDL_strlcpy(typed.text.text, "abcdefghijklmnopqrstuvw\xC3\xA9z", sizeof(typed.text.text));
	const auto pieces = TextFromSdl(typed, 0);
	ASSERT_EQ(pieces.size(), 2u);
	EXPECT_STREQ(ToSdl(pieces[0]).text.text, "abcdefghijklmnopqrstuvw");
	EXPECT_STREQ(ToSdl(pieces[1]).text.text, "\xC3\xA9z");
}

TEST(InputRecording, EventsPlayAtTheirFrameFromTheStart)
{
	const auto recording = Sample();
	Playhead playhead {.startFrame = 10};
	EXPECT_TRUE(Due(recording.events, playhead, 9).empty());
	EXPECT_EQ(Due(recording.events, playhead, 10).size(), 2u);
	EXPECT_TRUE(Due(recording.events, playhead, 11).empty());
	// a frame skipped plays what it held at the next one
	const auto late = Due(recording.events, playhead, 20);
	ASSERT_EQ(late.size(), 2u);
	EXPECT_EQ(late[0].kind, Kind::MouseWheel);
	EXPECT_EQ(late[1].kind, Kind::Key);
	EXPECT_TRUE(Due(recording.events, playhead, 21).empty());
	EXPECT_EQ(playhead.next, recording.events.size());
}

// A game started again (and the skirmish box's own frames) count the frames from 0 again: the recording plays on from
// where it is, its next frame at the new count's 0, never again from its start
TEST(InputRecording, ARecordingPlaysOnWhenTheFrameCountStartsAgain)
{
	const auto recording = Sample();
	Playhead playhead {.startFrame = 0};
	EXPECT_EQ(Due(recording.events, playhead, 0).size(), 2u);
	// the game ends after its frame 1: recording frame 2 is the new count's 0
	CountAgainFrom(playhead, 2);
	EXPECT_EQ(playhead.played, 2u);
	EXPECT_EQ(playhead.startFrame, 0u);
	const auto wheel = Due(recording.events, playhead, 0);
	ASSERT_EQ(wheel.size(), 1u);
	EXPECT_EQ(wheel[0].kind, Kind::MouseWheel);
	// a box's two frames, then the next game: recording frame 5 is that game's frame 1
	CountAgainFrom(playhead, 2);
	EXPECT_EQ(playhead.played, 4u);
	EXPECT_TRUE(Due(recording.events, playhead, 0).empty());
	const auto key = Due(recording.events, playhead, 1);
	ASSERT_EQ(key.size(), 1u);
	EXPECT_EQ(key[0].kind, Kind::Key);
	EXPECT_EQ(playhead.next, recording.events.size());
}

// A playback due to start later (the debug bar's, at the next frame) starts that much sooner on the new count
TEST(InputRecording, APlaybackNotYetStartedStartsSoonerOnTheNewCount)
{
	const auto recording = Sample();
	Playhead playhead {.startFrame = 10};
	CountAgainFrom(playhead, 4);
	EXPECT_EQ(playhead.startFrame, 6u);
	EXPECT_EQ(playhead.played, 0u);
	EXPECT_TRUE(Due(recording.events, playhead, 5).empty());
	EXPECT_EQ(Due(recording.events, playhead, 6).size(), 2u);
}

// A file written as a header and then its events in pieces, as a recording being made is appended to, is the file the
// whole recording makes
TEST(InputRecording, AHeaderAndItsEventsInPiecesAreTheWholeFile)
{
	const auto recording = Sample();
	auto bytes = WriteHeader(recording.window);
	EXPECT_EQ(bytes.size(), k_HeaderSize);
	const std::span events(recording.events);
	for (const auto piece : {events.first(1), events.subspan(1, 0), events.subspan(1, 2), events.subspan(3)})
	{
		const auto more = WriteEvents(piece);
		bytes.insert(bytes.end(), more.begin(), more.end());
	}
	EXPECT_EQ(bytes, Write(recording));
	// a header with no events yet is a recording with none
	const auto empty = Read(WriteHeader(recording.window));
	ASSERT_TRUE(empty.has_value());
	EXPECT_TRUE(empty->events.empty());
	EXPECT_EQ(empty->window, recording.window);
}

// A file cut in the middle of an event (a game that stopped while it was written) reads as the events before it
TEST(InputRecording, AFileCutInTheMiddleOfAnEventKeepsTheEventsBefore)
{
	const auto recording = Sample();
	const auto bytes = Write(recording);
	const auto cut = Read(std::span(bytes).first(k_HeaderSize + (2 * k_EventSize) + 7));
	ASSERT_TRUE(cut.has_value());
	ASSERT_EQ(cut->events.size(), 2u);
	EXPECT_EQ(cut->events[1], recording.events[1]);
}

// The recording's frame a game's frame stands for: none before the start, then counted on through a new count
TEST(InputRecording, TheRecordingsFrameCountsOnThroughANewCount)
{
	Playhead clock {.startFrame = 3};
	EXPECT_FALSE(FrameAt(clock, 2).has_value());
	EXPECT_EQ(FrameAt(clock, 3), 0u);
	EXPECT_EQ(FrameAt(clock, 10), 7u);
	// the game ends after its frame 10: the new count's 0 is the recording's 8
	CountAgainFrom(clock, 11);
	EXPECT_EQ(FrameAt(clock, 0), 8u);
	EXPECT_EQ(FrameAt(clock, 4), 12u);
}

// What a recording leaves held is let go at its end: a button still down at the pointer's last place, a key still down
// with its key code; what was let go, and the wheel, give nothing
TEST(InputRecording, TheButtonsAndKeysLeftHeldAreLetGo)
{
	const std::vector<Event> events {
	    {.frame = 0, .kind = Kind::MouseButton, .values = {SDL_BUTTON_RIGHT, 1, 1, 10, 20, 0}},
	    {.frame = 1, .kind = Kind::MouseButton, .values = {SDL_BUTTON_LEFT, 1, 1, 30, 40, 0}},
	    {.frame = 1, .kind = Kind::Key, .values = {SDL_SCANCODE_W, SDLK_w, KMOD_LSHIFT, 0, 1, 0}},
	    {.frame = 2, .kind = Kind::Key, .values = {SDL_SCANCODE_A, SDLK_a, 0, 0, 1, 0}},
	    {.frame = 3, .kind = Kind::Key, .values = {SDL_SCANCODE_A, SDLK_a, 0, 0, 0, 0}},
	    {.frame = 3, .kind = Kind::MouseButton, .values = {SDL_BUTTON_RIGHT, 0, 1, 50, 60, 0}},
	    {.frame = 4, .kind = Kind::MouseWheel, .values = {0, 1, SDL_MOUSEWHEEL_NORMAL, 0, 0, 0}},
	    {.frame = 4, .kind = Kind::MouseMotion, .values = {70, 80, 20, 20, SDL_BUTTON_LMASK, 0}},
	};
	const auto ups = ReleasesOfHeld(events, 5);
	ASSERT_EQ(ups.size(), 2u);
	EXPECT_EQ(ups[0], (Event {.frame = 5, .kind = Kind::MouseButton, .values = {SDL_BUTTON_LEFT, 0, 1, 70, 80, 0}}));
	EXPECT_EQ(ups[1], (Event {.frame = 5, .kind = Kind::Key, .values = {SDL_SCANCODE_W, SDLK_w, 0, 0, 0, 0}}));
	EXPECT_TRUE(ReleasesOfHeld({}, 0).empty());
	// as SDL events, they are ups
	EXPECT_EQ(ToSdl(ups[0]).type, SDL_MOUSEBUTTONUP);
	EXPECT_EQ(ToSdl(ups[1]).type, SDL_KEYUP);
}

// The wheel's notches as turned, a fraction of one from a fine wheel, which the game's zoom reads, come back as they
// were; a wheel event recorded before they were kept, or crafted, holds none, and its whole notches stand for them
TEST(InputRecording, TheWheelsNotchesAsTurnedComeBack)
{
	SDL_Event wheel {};
	wheel.type = SDL_MOUSEWHEEL;
	wheel.wheel.y = -1;
	wheel.wheel.preciseY = -1.25f;
	wheel.wheel.preciseX = 0.5f;
	wheel.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
	const auto back = ToSdl(*FromSdl(wheel, 3));
	EXPECT_EQ(back.type, SDL_MOUSEWHEEL);
	EXPECT_EQ(back.wheel.y, -1);
	EXPECT_EQ(back.wheel.preciseY, -1.25f);
	EXPECT_EQ(back.wheel.preciseX, 0.5f);
	EXPECT_EQ(back.wheel.direction, static_cast<uint32_t>(SDL_MOUSEWHEEL_FLIPPED));

	const auto crafted = ToSdl({.frame = 0, .kind = Kind::MouseWheel, .values = {0, 2, SDL_MOUSEWHEEL_NORMAL, 0, 0, 0}});
	EXPECT_EQ(crafted.wheel.preciseY, 2.0f);
	EXPECT_EQ(crafted.wheel.preciseX, 0.0f);
}
