/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <SDL_events.h>
#include <glm/vec2.hpp>

// (openblack, test runs) A recording of the player's raw input, frame by frame: the mouse's motion, buttons and wheel,
// the keyboard's keys and the text they type, as the game's window took them. The original's own recordings
// (Data\HandDemo\*.hnd, and the interface's InterfaceRecord.hnd) keep the interface's messages and the camera, not the input
// (Input/HandDemo.h), so they cannot carry the keys or the wheel; this file is openblack's own, with a version mark. Wiki:
// docs/wiki/engine/input-recording.md.
namespace openblack::input::recording
{

/// "OBIN", the file's first four bytes
constexpr std::array<uint8_t, 4> k_Magic {'O', 'B', 'I', 'N'};
/// The format this code writes and reads
constexpr uint32_t k_Version = 1;
/// The header: magic, version, the window's width and height when recorded
constexpr size_t k_HeaderSize = 16;
/// One event: its frame, its kind and six values
constexpr size_t k_EventSize = 32;

/// The kinds of input a recording keeps
enum class Kind : uint32_t
{
	MouseMotion = 1, ///< values: x, y, xrel, yrel, buttons held
	MouseButton = 2, ///< values: button, down (1) or up (0), clicks, x, y
	MouseWheel = 3,  ///< values: x, y, direction, the precise x and y (their float bits; both zero: x and y stand for them)
	Key = 4,         ///< values: scancode, key code, modifiers, repeat, down (1) or up (0)
	/// Not input: the in-loop logo played before the recording's first frame, so many frames of it (value 0). A replay
	/// shows no logo but takes the same draws from the C runtime's random stream (Game::SpendRecordedLogo)
	Logo = 5,
	/// The text a key typed (SDL's text input, which the name boxes read): up to 24 bytes of its UTF-8 in the six values,
	/// four to a value from the first byte, the rest zero; a longer text is several of these at the same frame
	Text = 6,
};
/// The bytes of text one Text event holds
constexpr size_t k_TextBytes = 24;

/// One recorded event, at a frame counted from the recording's start
struct Event
{
	uint32_t frame {0};
	Kind kind {Kind::MouseMotion};
	std::array<int32_t, 6> values {};

	friend bool operator==(const Event&, const Event&) = default;
};

/// A whole recording
struct Recording
{
	glm::ivec2 window {0};
	std::vector<Event> events; ///< in the order they were taken, by frame

	friend bool operator==(const Recording&, const Recording&) = default;
};

/// The file's bytes for a recording: its header, then its events
[[nodiscard]] std::vector<uint8_t> Write(const Recording& recording);
/// The header's bytes alone, for a file whose events are appended later (WriteEvents)
[[nodiscard]] std::vector<uint8_t> WriteHeader(glm::ivec2 window);
/// The bytes of these events, to append after a header or after other events: the header and the events written in
/// any number of pieces make the same file as Write
[[nodiscard]] std::vector<uint8_t> WriteEvents(std::span<const Event> events);
/// The recording a file's bytes hold; none when they are not one of this version (a short last event is left out)
[[nodiscard]] std::optional<Recording> Read(std::span<const uint8_t> bytes);

/// Whether a window event of this SDL type is input a recording keeps (the mouse and the keys)
[[nodiscard]] constexpr bool IsRecordable(uint32_t type)
{
	return type == SDL_MOUSEMOTION || type == SDL_MOUSEBUTTONDOWN || type == SDL_MOUSEBUTTONUP || type == SDL_MOUSEWHEEL ||
	       type == SDL_KEYDOWN || type == SDL_KEYUP || type == SDL_TEXTINPUT;
}
/// While a recording plays, the real mouse and keys are not dispatched (the window's other events always are)
[[nodiscard]] constexpr bool DropWhileReplaying(uint32_t type, bool replaying)
{
	return replaying && IsRecordable(type);
}

/// The recorded event of an SDL event at `frame`; none for an event a recording does not keep, and for text (TextFromSdl)
[[nodiscard]] std::optional<Event> FromSdl(const SDL_Event& event, uint32_t frame);
/// The Text events of an SDL text input at `frame`, its text cut into pieces of at most k_TextBytes bytes, each cut
/// between two characters; none for an empty text or another event. Pure
[[nodiscard]] std::vector<Event> TextFromSdl(const SDL_Event& event, uint32_t frame);
/// The SDL event a recorded one stands for
[[nodiscard]] SDL_Event ToSdl(const Event& event);

/// Where a playback is in its recording
struct Playhead
{
	size_t next {0};         ///< the next event to play
	uint32_t startFrame {0}; ///< the game's frame the recording's frame 0 plays at (or, with frames played, goes on at)
	uint32_t played {0};     ///< the recording's frames played before the game's frame count last started again
};
/// The ups of the mouse buttons and the keys that these events leave held, at `frame`, so that nothing stays pressed
/// once they are over: a button's up at the pointer's last place, a key's with the key code its down had and no
/// modifiers. Buttons first, then keys, each in increasing order. Pure
[[nodiscard]] std::vector<Event> ReleasesOfHeld(std::span<const Event> events, uint32_t frame);

/// The recording's frame (counted from its start) that the game's frame `frame` stands for; none before the start. Pure
[[nodiscard]] std::optional<uint32_t> FrameAt(const Playhead& playhead, uint32_t frame);
/// The events due at the game's frame `frame`, from the playhead on: every one whose frame (counted from the start) is
/// not later than it, in order. The playhead moves past them. A frame before the start plays nothing. Pure
[[nodiscard]] std::span<const Event> Due(std::span<const Event> events, Playhead& playhead, uint32_t frame);
/// The game's frame count starts again at 0 where it would have come to `frame` (a game started again, or a box's own
/// frames over): the recording plays on from where it is, its next frame at the new count's 0. A playback that has not
/// started by then starts that much sooner. Pure
void CountAgainFrom(Playhead& playhead, uint32_t frame);

} // namespace openblack::input::recording
