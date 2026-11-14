/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InputReplay.h"

#include <cstdio>
#include <cstdlib>

#include <fstream>
#include <iterator>
#include <span>
#include <utility>

#include <spdlog/spdlog.h>

#include "ECS/Systems/InputStateInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Input/InputRecording.h"
#include "Locator.h"

namespace openblack::input::replay
{
namespace
{
/// This module's state (Locator::inputState)
struct ReplayState
{
	std::optional<Playback> playback;
	std::optional<Recorder> recorder;
	std::optional<HandOver> handOver;
	bool handOverTaken {true};
};

ReplayState& State()
{
	if (!Locator::inputState::has_value())
	{
		std::fputs("input replay: no input state in the locator (Locator::inputState)\n", stderr);
		std::abort();
	}
	return Locator::inputState::value().Get<ReplayState>();
}

/// The state when there is one, for the queries that have an answer without it
const ReplayState* StateIfAny()
{
	return Locator::inputState::has_value() ? &Locator::inputState::value().Get<ReplayState>() : nullptr;
}
} // namespace

bool StartPlayback(const std::filesystem::path& path, uint32_t startFrame)
{
	auto& state = State();
	state.playback.reset();
	// the recording, the player's own file, read whole; one that is not there reads as nothing
	const auto bytes = Locator::filesystem::has_value() ? Locator::filesystem::value().ReadUserFile(path) : std::nullopt;
	auto read = recording::Read(bytes.value_or(std::vector<uint8_t> {}));
	if (!read.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Input replay: {} is not an input recording of version {}",
		                    path.generic_string(), recording::k_Version);
		return false;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Input replay: playing {} ({} events) from frame {}", path.generic_string(),
	                   read->events.size(), startFrame);
	state.playback = Playback {.recording = std::move(*read), .playhead = {.startFrame = startFrame}, .file = path};
	return true;
}

bool StartResume(const std::filesystem::path& path, uint32_t startFrame)
{
	if (!StartPlayback(path, startFrame))
	{
		return false;
	}
	State().playback->handsOver = true;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Input resume: {} plays, then the player takes over", path.generic_string());
	return true;
}

std::span<const recording::Event> PlaybackEvents()
{
	if (!IsPlaying())
	{
		return {};
	}
	return State().playback->recording.events;
}

void StartRecording(const std::filesystem::path& path, uint32_t startFrame, int32_t windowWidth, int32_t windowHeight,
                    std::span<const recording::Event> first)
{
	auto& state = State();
	state.recorder = Recorder {
	    .recording = {.window = {windowWidth, windowHeight}, .events = {first.begin(), first.end()}},
	    .clock = {.startFrame = startFrame},
	    .file = path,
	};
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Input recording: recording to {} from frame {}, after {} events",
	                   path.generic_string(), startFrame, first.size());
	// the header (and the events it starts with) at once: the file is a recording, with no events yet, from the first frame
	FlushRecording();
}

bool IsPlaying()
{
	const auto* state = StateIfAny();
	return state != nullptr && state->playback.has_value();
}

bool IsRecording()
{
	const auto* state = StateIfAny();
	return state != nullptr && state->recorder.has_value();
}

bool TakeRealEvent(const SDL_Event& event, uint32_t frame)
{
	if (!Locator::inputState::has_value())
	{
		return true;
	}
	auto& state = State();
	if (recording::DropWhileReplaying(event.type, state.playback.has_value()))
	{
		return false;
	}
	if (state.recorder.has_value())
	{
		if (const auto at = recording::FrameAt(state.recorder->clock, frame); at.has_value())
		{
			if (const auto kept = recording::FromSdl(event, *at); kept.has_value())
			{
				state.recorder->recording.events.push_back(*kept);
			}
			for (const auto& piece : recording::TextFromSdl(event, *at))
			{
				state.recorder->recording.events.push_back(piece);
			}
		}
	}
	return true;
}

std::vector<SDL_Event> DueEvents(uint32_t frame)
{
	std::vector<SDL_Event> due;
	// a playback whose last events were given and not yet ended gives nothing more
	EndIfPlayedOut(frame);
	if (!IsPlaying())
	{
		return due;
	}
	auto& playback = *State().playback;
	for (const auto& event : recording::Due(playback.recording.events, playback.playhead, frame))
	{
		// the logo's frames are no window event
		if (event.kind != recording::Kind::Logo)
		{
			due.push_back(recording::ToSdl(event));
		}
	}
	return due;
}

void EndIfPlayedOut(uint32_t frame)
{
	if (!IsPlaying())
	{
		return;
	}
	auto& state = State();
	auto& playback = *state.playback;
	if (playback.playhead.next < playback.recording.events.size())
	{
		return;
	}
	const uint32_t at = recording::FrameAt(playback.playhead, frame).value_or(0);
	// a resumed playback lets go of what it left held at its next frame, as its own events, recorded too, so that a
	// resume of the new recording gives the same ups
	if (playback.handsOver && !std::exchange(playback.releasesGiven, true))
	{
		const auto ups = recording::ReleasesOfHeld(playback.recording.events, at + 1);
		if (!ups.empty())
		{
			playback.recording.events.insert(playback.recording.events.end(), ups.begin(), ups.end());
			if (state.recorder.has_value())
			{
				auto& kept = state.recorder->recording.events;
				kept.insert(kept.end(), ups.begin(), ups.end());
			}
			return;
		}
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Input replay: {} played to its end at frame {}", playback.file.generic_string(),
	                   frame);
	if (playback.handsOver)
	{
		state.handOver = HandOver {.gameFrame = frame, .recordingFrame = at, .events = playback.recording.events.size()};
		state.handOverTaken = false;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Input resume: live play from frame {} (the recording's frame {}, {} events)",
		                   frame, at, playback.recording.events.size());
	}
	state.playback.reset();
}

std::optional<HandOver> TakeHandOver()
{
	if (!Locator::inputState::has_value())
	{
		return std::nullopt;
	}
	auto& state = State();
	if (std::exchange(state.handOverTaken, true))
	{
		return std::nullopt;
	}
	return state.handOver;
}

void CountAgainFrom(uint32_t frame)
{
	if (IsPlaying())
	{
		recording::CountAgainFrom(State().playback->playhead, frame);
	}
	if (IsRecording())
	{
		recording::CountAgainFrom(State().recorder->clock, frame);
	}
}

std::optional<Playback> TakePlayback()
{
	if (!IsPlaying())
	{
		return std::nullopt;
	}
	return std::exchange(State().playback, std::nullopt);
}

void ResumePlayback(Playback playback)
{
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Input replay: {} plays on in the new game, at its frame {}",
	                   playback.file.generic_string(), playback.playhead.played);
	State().playback = std::move(playback);
}

std::optional<Recorder> TakeRecording()
{
	if (!IsRecording())
	{
		return std::nullopt;
	}
	return std::exchange(State().recorder, std::nullopt);
}

void ResumeRecording(Recorder recorder)
{
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Input recording: {} records on in the new game, at its frame {}",
	                   recorder.file.generic_string(), recorder.clock.played);
	State().recorder = std::move(recorder);
}

void FlushIfDue(uint32_t frame)
{
	if (!IsRecording())
	{
		return;
	}
	const auto& recorder = *State().recorder;
	const auto at = recording::FrameAt(recorder.clock, frame);
	if (at.has_value() && *at >= recorder.lastFlushAt + k_FlushFrames)
	{
		FlushRecording();
		State().recorder->lastFlushAt = *at;
	}
}

bool FlushRecording()
{
	if (!IsRecording())
	{
		return false;
	}
	auto& recorder = *State().recorder;
	const auto events = std::span(recorder.recording.events).subspan(recorder.flushed);
	if (recorder.headerWritten && events.empty())
	{
		return true;
	}
	// the header once, with the file made anew; the events appended after it, so what is written stays written
	std::ofstream out(recorder.file, std::ios::binary | (recorder.headerWritten ? std::ios::app : std::ios::trunc));
	if (!recorder.headerWritten)
	{
		const auto header = recording::WriteHeader(recorder.recording.window);
		out.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));
	}
	const auto bytes = recording::WriteEvents(events);
	out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	out.close();
	if (!out.good())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Input recording: could not write {}", recorder.file.generic_string());
		return false;
	}
	recorder.headerWritten = true;
	recorder.flushed = recorder.recording.events.size();
	return true;
}

bool FinishRecording()
{
	if (!IsRecording())
	{
		return false;
	}
	const bool written = FlushRecording();
	const auto& recorder = *State().recorder;
	if (written)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Input recording: {} events written to {}", recorder.recording.events.size(),
		                   recorder.file.generic_string());
	}
	State().recorder.reset();
	return written;
}

void StopPlayback()
{
	if (Locator::inputState::has_value())
	{
		State().playback.reset();
	}
}

Status CurrentStatus()
{
	Status status;
	const auto* state = StateIfAny();
	if (state == nullptr)
	{
		return status;
	}
	if (state->playback.has_value())
	{
		status.playing = true;
		status.played = state->playback->playhead.next;
		status.total = state->playback->recording.events.size();
		status.resuming = state->playback->handsOver;
		status.file = state->playback->file;
	}
	status.handOver = state->handOver;
	if (state->recorder.has_value())
	{
		status.recording = true;
		status.recorded = state->recorder->recording.events.size();
		status.flushed = state->recorder->flushed;
		if (!status.playing)
		{
			status.file = state->recorder->file;
		}
	}
	return status;
}

} // namespace openblack::input::replay
