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

#include <vector>

// The mouse messages that the gesture samples come from, and their times. The game's system library sends one for each
// move of the mouse, timed by the system's clock; while the mouse stays still its input thread sends one of its own at
// the cursor's position each time it has waited 40 ms for a move (only while the game's window is active, else it looks
// again every 300 ms). Each message gives the mouse handler the ms since the message before it; the handler adds them
// up and, once the sum is past 28 ms, sends the cursor's position on, which is a gesture sample. So a still mouse still
// gives samples, and 70 of them on the same pixel forget the path.
// Wiki: docs/wiki/magic/gestures.md#the-samples.

namespace openblack::magic::gestures::mouse_messages
{
/// How long the input thread waits for a move before it sends a message of its own
inline constexpr float k_StillWaitMs = 40.0f;
/// How long it sleeps before it looks again while it may not send one (the window is not active)
inline constexpr float k_InactiveSleepMs = 300.0f;
/// The handler sends the position on once its sum of ms is past this
inline constexpr float k_SampleMs = 28.0f;

/// What the input thread is doing
enum class Thread : uint8_t
{
	Waiting,  ///< waiting for a move, at most k_StillWaitMs
	Sleeping, ///< sleeping k_InactiveSleepMs before it looks again
};

/// The messages' clock and the handler's sum, kept from frame to frame
struct Clock
{
	bool anyMessage {false};   ///< the first message has none before it: it gives 0 ms
	float sinceMessage {0.0f}; ///< ms since the last message, a move or one of the input thread's
	Thread thread {Thread::Waiting};
	float threadElapsed {0.0f}; ///< ms since the thread began to wait or to sleep
	float handlerMs {0.0f};     ///< the handler's sum since it last sent the position on
};

/// One frame of `frameMs`: the ms each mouse message of the frame gives, in order, appended to `messages`. The input
/// thread runs through the frame first, sending a message each time its wait ends while `threadMaySend` (the window is
/// active); a frame in which the cursor moved then ends with the move's message, which starts the thread's wait again
/// unless it is sleeping. (approximate: a move stands at the end of its frame)
void Frame(Clock& clock, float frameMs, bool moved, bool threadMaySend, std::vector<float>& messages);

/// The handler on one message of `ms`: nothing for 0 ms or less; else the sum grows by `ms` and, once past
/// k_SampleMs, goes back to 0 (nothing is taken off) and the position is sent on (true)
[[nodiscard]] bool Handle(Clock& clock, float ms);
} // namespace openblack::magic::gestures::mouse_messages
