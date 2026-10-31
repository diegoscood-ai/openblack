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

// (openblack, test runs) A recording of the player's raw input, frame by frame: the mouse's motion, buttons and wheel
// and the keyboard's keys, as the game's window took them. The original's own recordings (Data\HandDemo\*.hnd, and the
// interface's InterfaceRecord.hnd) keep the interface's messages and the camera, not the input (Input/HandDemo.h), so
// they cannot carry the keys or the wheel; this file is openblack's own, with a version mark.
// Wiki: docs/wiki/engine/input-recording.md.
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
	MouseWheel = 3,  ///< values: x, y, direction
	Key = 4,         ///< values: scancode, key code, modifiers, repeat, down (1) or up (0)
};

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

/// The file's bytes for a recording
[[nodiscard]] std::vector<uint8_t> Write(const Recording& recording);
/// The recording a file's bytes hold; none when they are not one of this version (a short last event is left out)
[[nodiscard]] std::optional<Recording> Read(std::span<const uint8_t> bytes);

/// Whether a window event of this SDL type is input a recording keeps (the mouse and the keys)
[[nodiscard]] constexpr bool IsRecordable(uint32_t type)
{
	return type == SDL_MOUSEMOTION || type == SDL_MOUSEBUTTONDOWN || type == SDL_MOUSEBUTTONUP || type == SDL_MOUSEWHEEL ||
	       type == SDL_KEYDOWN || type == SDL_KEYUP;
}
/// While a recording plays, the real mouse and keys are not dispatched (the window's other events always are)
[[nodiscard]] constexpr bool DropWhileReplaying(uint32_t type, bool replaying)
{
	return replaying && IsRecordable(type);
}

/// The recorded event of an SDL event at `frame`; none for an event a recording does not keep
[[nodiscard]] std::optional<Event> FromSdl(const SDL_Event& event, uint32_t frame);
/// The SDL event a recorded one stands for
[[nodiscard]] SDL_Event ToSdl(const Event& event);

/// Where a playback is in its recording
struct Playhead
{
	size_t next {0};         ///< the next event to play
	uint32_t startFrame {0}; ///< the game's frame the recording's frame 0 plays at
};
/// The events due at the game's frame `frame`, from the playhead on: every one whose frame (counted from the start) is
/// not later than it, in order. The playhead moves past them. A frame before the start plays nothing. Pure
[[nodiscard]] std::span<const Event> Due(std::span<const Event> events, Playhead& playhead, uint32_t frame);

} // namespace openblack::input::recording
