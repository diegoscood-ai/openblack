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
struct Playback
{
	recording::Recording recording;
	recording::Playhead playhead;
	std::filesystem::path file;
};

struct Recorder
{
	recording::Recording recording;
	uint32_t startFrame {0};
	std::filesystem::path file;
};

/// This module's state (Locator::inputState)
struct ReplayState
{
	std::optional<Playback> playback;
	std::optional<Recorder> recorder;
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

void StartRecording(const std::filesystem::path& path, uint32_t startFrame, int32_t windowWidth, int32_t windowHeight)
{
	State().recorder = Recorder {
	    .recording = {.window = {windowWidth, windowHeight}},
	    .startFrame = startFrame,
	    .file = path,
	};
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Input recording: recording to {} from frame {}", path.generic_string(),
	                   startFrame);
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
	if (state.recorder.has_value() && frame >= state.recorder->startFrame)
	{
		if (const auto kept = recording::FromSdl(event, frame - state.recorder->startFrame); kept.has_value())
		{
			state.recorder->recording.events.push_back(*kept);
		}
	}
	return true;
}

std::vector<SDL_Event> DueEvents(uint32_t frame)
{
	std::vector<SDL_Event> due;
	if (!IsPlaying())
	{
		return due;
	}
	auto& playback = *State().playback;
	for (const auto& event : recording::Due(playback.recording.events, playback.playhead, frame))
	{
		due.push_back(recording::ToSdl(event));
	}
	if (playback.playhead.next >= playback.recording.events.size())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Input replay: {} played to its end at frame {}",
		                   playback.file.generic_string(), frame);
		State().playback.reset();
	}
	return due;
}

bool FinishRecording()
{
	auto& state = State();
	if (!state.recorder.has_value())
	{
		return false;
	}
	const auto recorder = std::move(*state.recorder);
	state.recorder.reset();
	const auto bytes = recording::Write(recorder.recording);
	std::ofstream out(recorder.file, std::ios::binary | std::ios::trunc);
	out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	if (!out.good())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Input recording: could not write {}", recorder.file.generic_string());
		return false;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Input recording: {} events written to {}", recorder.recording.events.size(),
	                   recorder.file.generic_string());
	return true;
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
		status.file = state->playback->file;
	}
	if (state->recorder.has_value())
	{
		status.recording = true;
		status.recorded = state->recorder->recording.events.size();
		if (!status.playing)
		{
			status.file = state->recorder->file;
		}
	}
	return status;
}

} // namespace openblack::input::replay
