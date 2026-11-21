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

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include <SDL_events.h>

#include "Input/InputRecording.h"

// (openblack, test runs) The player and the recorder of the raw input (Input/InputRecording.h). Both sit where the
// window's events enter the game (Game::Update's dispatch), as OPENBLACK_MOUSE_AT and OPENBLACK_IGNORE_REAL_INPUT do:
// nothing in the game's logic knows of them. Off unless --replay-input, --record-input or the debug bar starts one;
// the game is the same without them. Kept in Locator::inputState.
// Wiki: docs/wiki/engine/input-recording.md.
namespace openblack::input::replay
{

/// A recording being played, with where it is
struct Playback
{
	recording::Recording recording;
	recording::Playhead playhead;
	std::filesystem::path file;
	/// --resume-input: the player takes over once it plays out, the mouse buttons and keys it left held let go first
	bool handsOver {false};
	bool releasesGiven {false};
};

/// Where a resumed playback gave the input back to the player
struct HandOver
{
	uint32_t gameFrame {0};      ///< the game's frame it ended at
	uint32_t recordingFrame {0}; ///< the recording's own frame then
	size_t events {0};           ///< the events it played, the ups it gave at its end included
};

/// A recording being made, with its clock and how much of it is in its file
struct Recorder
{
	recording::Recording recording;
	/// the game's frame its frame 0 was at, and the frames recorded before the game's frame count last started again
	/// (only `startFrame` and `played` are used)
	recording::Playhead clock;
	std::filesystem::path file;
	size_t flushed {0};       ///< the events already in the file
	uint32_t lastFlushAt {0}; ///< the recording's frame of the last flush
	bool headerWritten {false};
};

/// The recording's events are appended to its file every so many of its frames, so a game that stops without ending
/// (a crash) keeps them up to the last time
constexpr uint32_t k_FlushFrames = 300;

/// Plays the recording in `path` from the game's frame `startFrame` on; false (and nothing plays) when the file cannot
/// be read or is not a recording of this version. A recording already playing is dropped
bool StartPlayback(const std::filesystem::path& path, uint32_t startFrame);
/// --resume-input: plays the recording in `path` from the game's frame `startFrame` on, as StartPlayback, and gives the
/// input back to the player once it plays out: one frame after its last events it lets go of the buttons and keys they
/// left held (recording::ReleasesOfHeld), given as its own events and recorded too, then it ends (TakeHandOver)
bool StartResume(const std::filesystem::path& path, uint32_t startFrame);
/// The events of the recording playing; none when none plays
[[nodiscard]] std::span<const recording::Event> PlaybackEvents();
/// Records the input from the game's frame `startFrame` on, into `path`, whose header is written at once, after the
/// events in `first` (a resumed recording's, so that the new file can be resumed again); a recording already being made
/// is dropped
void StartRecording(const std::filesystem::path& path, uint32_t startFrame, int32_t windowWidth, int32_t windowHeight,
                    std::span<const recording::Event> first = {});

/// Whether a recording plays (it plays until its last events have been dispatched: EndIfPlayedOut)
[[nodiscard]] bool IsPlaying();
/// Whether the input is being recorded
[[nodiscard]] bool IsRecording();

/// The window's event as it comes: whether it is dispatched (the real mouse and keys are not while a recording plays)
/// and, when the input is being recorded, kept at `frame`
[[nodiscard]] bool TakeRealEvent(const SDL_Event& event, uint32_t frame);
/// The recorded events due at the game's frame `frame`, to dispatch after the window's own. The playback still plays
/// while its last events are dispatched, so that they are taken as the recording's (a last mouse move moves the game's
/// cursor); a playback with nothing left ends at the next call, with nothing due
[[nodiscard]] std::vector<SDL_Event> DueEvents(uint32_t frame);
/// Once the frame's recorded events are dispatched: a playback with nothing left ends (a resumed one first gives the
/// ups of what it leaves held, at the next frame)
void EndIfPlayedOut(uint32_t frame);
/// The hand-over of a resumed playback that has just ended, once: for the Game to give the player the pointer where
/// the game's cursor is
[[nodiscard]] std::optional<HandOver> TakeHandOver();

/// The game's frame count starts again at 0 where it would have come to `frame` (a game started again, or the skirmish
/// box's own frames over): a recording playing plays on from where it is, its next frame at the new count's 0, and a
/// recording being made counts on the same way
void CountAgainFrom(uint32_t frame);
/// A game started again: the recording playing is taken out of the game that ends (none when none plays), to be given
/// to the next one with ResumePlayback, so that it plays on through the restart
[[nodiscard]] std::optional<Playback> TakePlayback();
/// The next game plays on the recording taken out of the last one
void ResumePlayback(Playback playback);

/// A game started again: the recording being made is taken out of the game that ends (none when none is made), to be
/// given to the next one with ResumeRecording, so that it records on through the restart at the frames the playback
/// would count
[[nodiscard]] std::optional<Recorder> TakeRecording();
/// The next game records on into the recording taken out of the last one
void ResumeRecording(Recorder recorder);

/// The game's frame `frame` is done: the events recorded since the last flush are appended to the file once
/// k_FlushFrames of the recording's frames have gone by
void FlushIfDue(uint32_t frame);
/// Appends the events not yet in the file; false when they could not be written
bool FlushRecording();
/// Appends the events not yet in the file and stops recording; false when they could not be written
bool FinishRecording();
/// Stops the playback
void StopPlayback();

/// What the debug bar shows
struct Status
{
	bool playing {false};
	bool recording {false};
	size_t played {0};   ///< the events given so far
	size_t total {0};    ///< the events of the recording playing
	size_t recorded {0}; ///< the events kept so far
	size_t flushed {0};  ///< of which already in the file
	bool resuming {false};            ///< the playback gives the input back to the player at its end
	std::optional<HandOver> handOver; ///< where it did
	std::filesystem::path file;
};
[[nodiscard]] Status CurrentStatus();

} // namespace openblack::input::replay
