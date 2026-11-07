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
};

/// Plays the recording in `path` from the game's frame `startFrame` on; false (and nothing plays) when the file cannot
/// be read or is not a recording of this version. A recording already playing is dropped
bool StartPlayback(const std::filesystem::path& path, uint32_t startFrame);
/// Records the input from the game's frame `startFrame` on, for `path`; a recording already being made is dropped
void StartRecording(const std::filesystem::path& path, uint32_t startFrame, int32_t windowWidth, int32_t windowHeight);

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
/// Once the frame's recorded events are dispatched: a playback with nothing left ends
void EndIfPlayedOut(uint32_t frame);

/// The game's frame count starts again at 0 where it would have come to `frame` (a game started again, or the skirmish
/// box's own frames over): a recording playing plays on from where it is, its next frame at the new count's 0
void CountAgainFrom(uint32_t frame);
/// A game started again: the recording playing is taken out of the game that ends (none when none plays), to be given
/// to the next one with ResumePlayback, so that it plays on through the restart
[[nodiscard]] std::optional<Playback> TakePlayback();
/// The next game plays on the recording taken out of the last one
void ResumePlayback(Playback playback);

/// Writes the recording being made to its file and stops recording; false when it could not be written
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
	std::filesystem::path file;
};
[[nodiscard]] Status CurrentStatus();

} // namespace openblack::input::replay
