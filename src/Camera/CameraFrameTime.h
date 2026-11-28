/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <chrono>

/// The camera's frame time. The game camera's update is given the frame's whole camera milliseconds (the real clock's,
/// or the game's while the recorded interface plays back), and takes them as seconds in single precision: the whole
/// milliseconds times the float 0.001. The camera's own time and its zoomers move on by at most a tenth of a second of it
/// a frame (CappedSeconds), as do the follows, the paths and the script camera's modes; the player's world camera reads
/// it whole (Seconds), its keys included.
namespace openblack::camera_frame
{

/// Seconds in a millisecond, as the game's single precision constant
inline constexpr float k_SecondsPerMs = 0.001f;
/// The camera's frame time is no longer than this, in seconds, for its own time and its zoomers
inline constexpr float k_MaxSeconds = 0.1f;

/// A frame's whole camera milliseconds as seconds: the milliseconds as a float times 0.001, rounded to a float
[[nodiscard]] constexpr float Seconds(uint32_t ms)
{
	return static_cast<float>(ms) * k_SecondsPerMs;
}

/// The same, at most a tenth of a second
[[nodiscard]] constexpr float CappedSeconds(uint32_t ms)
{
	const float seconds = Seconds(ms);
	return k_MaxSeconds < seconds ? k_MaxSeconds : seconds;
}

/// The whole milliseconds of a camera step given as a duration; none for a step that is not forwards
[[nodiscard]] constexpr uint32_t WholeMs(std::chrono::microseconds step)
{
	return step.count() <= 0 ? 0u : static_cast<uint32_t>(step.count() / 1000);
}

/// A camera step given as a duration, as seconds of its whole milliseconds
[[nodiscard]] constexpr float Seconds(std::chrono::microseconds step)
{
	return Seconds(WholeMs(step));
}

/// The same, at most a tenth of a second
[[nodiscard]] constexpr float CappedSeconds(std::chrono::microseconds step)
{
	return CappedSeconds(WholeMs(step));
}

/// The camera step of a frame of whole camera milliseconds, as the camera's updates take it
[[nodiscard]] constexpr std::chrono::microseconds Step(uint32_t ms)
{
	return std::chrono::milliseconds(ms);
}

} // namespace openblack::camera_frame
