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
