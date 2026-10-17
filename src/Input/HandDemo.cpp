/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandDemo.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <array>
#include <exception>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "ECS/CreatureLoop.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/InputStateInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "Input/InterfaceActive.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Worship/PlayerSpellIcons.h"

namespace openblack::hand_demo
{
namespace
{

struct HandDemoState
{
	std::vector<Record> records;
	Playhead head;
	bool playing {false};
	uint32_t task {0};        ///< the task that started the demo
	bool waitTrigger {false}; ///< the script's wait-for-trigger flag
	bool trigger {false};     ///< the script's pending trigger
	bool testHook {false};    ///< (openblack's own) started by the OPENBLACK_TEST_HAND_DEMO test hook
	uint32_t hints {0};       ///< the camera hints the playback shows
	Frame frame;
};

/// This module's state (Locator::inputState)
HandDemoState& Get()
{
	if (!Locator::inputState::has_value())
	{
		std::fputs("hand_demo: no input state in the locator (Locator::inputState)\n", stderr);
		std::abort();
	}
	return Locator::inputState::value().Get<HandDemoState>();
}

/// OPENBLACK_HAND_DEMO_TRACE's frame count, in the debug hooks' store (Locator::debugHooks)
struct HandDemoDebugHooksState
{
	uint32_t frames {0};
};

HandDemoDebugHooksState& TraceState()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("hand_demo: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<HandDemoDebugHooksState>();
}

/// The demo a list's land run gives the test hook, in the debug hooks' store (Locator::debugHooks)
struct ListedTestDemoState
{
	std::optional<std::string> name;
};

ListedTestDemoState& ListedDemoState()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("hand_demo: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<ListedTestDemoState>();
}

float ReadFloat(std::span<const uint8_t> data, size_t offset)
{
	float value = 0.0f;
	std::memcpy(&value, data.data() + offset, sizeof(value));
	return value;
}

uint32_t ReadU32(std::span<const uint8_t> data, size_t offset)
{
	uint32_t value = 0;
	std::memcpy(&value, data.data() + offset, sizeof(value));
	return value;
}

} // namespace

std::vector<Record> ParseRecords(std::span<const uint8_t> bytes)
{
	// Only whole records are read: a short read ends the playback
	std::vector<Record> records;
	records.reserve(bytes.size() / k_RecordSize);
	for (size_t offset = 0; offset + k_RecordSize <= bytes.size(); offset += k_RecordSize)
	{
		const std::span<const uint8_t> data {bytes.data() + offset, k_RecordSize};
		std::array<float, 12> block {};
		for (size_t i = 0; i < block.size(); ++i)
		{
			block[i] = ReadFloat(data, 0x04 + i * 4);
		}
		records.push_back(
		    {.message = ReadU32(data, 0x00),
		     .throwBlock = block,
		     .mouse = {ReadFloat(data, 0x34), ReadFloat(data, 0x38)},
		     .eye = {ReadFloat(data, 0x3C), ReadFloat(data, 0x40), ReadFloat(data, 0x44)},
		     .focus = {ReadFloat(data, 0x48), ReadFloat(data, 0x4C), ReadFloat(data, 0x50)},
		     .hints = ReadU32(data, 0x54),
		     .hintValue = ReadFloat(data, 0x58),
		     .trigger = ReadU32(data, 0x5C),
		     .timeMs = ReadU32(data, 0x60),
		     .objectDistance = ReadFloat(data, 0x64),
		     .objectPlace = {static_cast<int32_t>(ReadU32(data, 0x68)), static_cast<int32_t>(ReadU32(data, 0x6C))},
		     .objectAltitude = ReadFloat(data, 0x70),
		     .objectType = ReadU32(data, 0x74),
		     .objectSubtype = ReadU32(data, 0x78)});
	}
	return records;
}

const std::vector<Record>& ReadRecords(std::string_view name)
{
	auto& demos = Locator::resources::value().GetHandDemos();
	const auto id = entt::hashed_string(std::string(name).c_str()).value();
	if (!demos.Contains(id))
	{
		auto& fileSystem = Locator::filesystem::value();
		const auto& bytes = resources::LoadBlob(
		    Locator::resources::value().GetBlobs(),
		    fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / "HandDemo" / (std::string(name) + ".hnd")));
		demos.Load(id, resources::HandDemoLoader::FromBufferTag {}, bytes);
	}
	return *demos.Handle(id);
}

StepResult StepRecords(std::span<const Record> records, Playhead& playhead, uint32_t nowMs, bool first, bool waitTrigger,
                       bool& trigger, const std::function<void(const Record&)>& play)
{
	StepResult result;
	// The records read once this step's first record is read, counted once for the whole step, against the file's
	const size_t readAtStart = playhead.read + 1;
	const size_t total = records.size();
	const auto startFrom = [&](uint32_t firstTimeMs) {
		if (first)
		{
			playhead.firstTimeMs = firstTimeMs;
			playhead.startMs = nowMs;
		}
	};
	while (true)
	{
		// One record per read; a short read ends the playback
		if (playhead.read >= records.size())
		{
			result.ended = true;
			startFrom(0);
			return result;
		}
		const auto& record = records[playhead.read];
		++playhead.read;
		++result.reads;
		// Not due while (time - first time) > (now - start), unsigned: seek back one record
		if (!first && record.timeMs - playhead.firstTimeMs > nowMs - playhead.startMs)
		{
			--playhead.read;
			return result;
		}
		// Waiting for the trigger with one pending, the start is moved on so that time stands still
		// (start = first time - time + now) and the record waits
		if (waitTrigger && trigger)
		{
			playhead.startMs = playhead.firstTimeMs - record.timeMs + nowMs;
			--playhead.read;
			startFrom(0);
			return result;
		}
		// The record's hints are shown, but not near either end of the recording
		result.hints = readAtStart < k_HintlessRecords || total - readAtStart < k_HintlessRecords ? 0u : record.hints;
		result.hintValue = record.hintValue;
		play(record);
		// A trigger in the record becomes the script's pending trigger
		if (record.trigger != 0)
		{
			trigger = true;
		}
		if (first)
		{
			startFrom(record.timeMs);
			return result;
		}
	}
}

glm::ivec2 CursorPixel(glm::vec2 mouse, glm::ivec2 screen, float barFraction)
{
	// The height of a 16:9 picture across the screen's width
	constexpr float k_WideScreenShape = 0.5625f;
	const auto width = static_cast<float>(screen.x);
	const auto height = static_cast<float>(screen.y);
	const auto bar = static_cast<float>(static_cast<int>((height - width * k_WideScreenShape) * barFraction) / 2);
	return {static_cast<int>(width * mouse.x + 0.5f), static_cast<int>((height - 2.0f * bar) * mouse.y + bar + 0.5f)};
}

namespace
{

std::vector<Record> Load(std::string_view name)
{
	// ".\Data\HandDemo\<name>.hnd", opened for reading
	std::vector<Record> records;
	try
	{
		records = ReadRecords(name);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Hand demo {}: {}", name, e.what());
		records.clear();
	}
	return records;
}

/// Applies one record: the mouse or the throw block, the camera and the buttons
void ApplyRecord(Frame& frame, const Record& record)
{
	// A MOUSE_MOVE moves the mouse; any other message copies the recorded throw block into the render hand before it
	// is dispatched: a release throws with the recorded v and h
	if (record.message == 0)
	{
		frame.mouse = record.mouse;
	}
	else if (Locator::handSystem::has_value())
	{
		Locator::handSystem::value().SetThrowBlock(record.throwBlock);
	}
	// The record sets the camera's position and focus
	frame.eye = record.eye;
	frame.focus = record.focus;
	// The message goes through the interface's dispatcher (the button bits of the message). (approximate)
	// openblack's hand reads the buttons' state once a frame, so a press and a release due in the same frame are
	// not both seen. (pending) the object of ACTION messages found again within 3 m by its script type and subtype
	switch (record.message)
	{
	case 1:
		frame.grip = true;
		break;
	case 2:
		frame.grip = false;
		break;
	case 3:
		frame.action = true;
		break;
	case 4:
		frame.action = false;
		break;
	default:
		break;
	}
}

/// The records due, in order (StepRecords). With `first` the first record is taken whatever its time and the
/// playback starts from it. Every call that reads a record sets the game speed to 1; the end of the records ends the
/// playback.
void ProcessRecords(bool first, uint32_t nowMs)
{
	auto& state = Get();
	const auto result = StepRecords(state.records, state.head, nowMs, first, state.waitTrigger, state.trigger,
	                                [&state](const Record& record) { ApplyRecord(state.frame, record); });
	if (result.reads != 0)
	{
		game_clock::SetSpeed(1.0f);
	}
	if (result.hints)
	{
		state.hints = *result.hints;
	}
	if (result.ended)
	{
		End();
	}
}

} // namespace

bool Play(std::string_view name, uint32_t task, bool waitTrigger, bool keepHand)
{
	auto& state = Get();
	// Without keepHand: an object in the hand is dropped (ForceDropHeld), the player's spells stop charging
	// (CancelAllSpellsCharging) and a leash held in the hand comes off
	if (!keepHand)
	{
		if (Locator::handSystem::has_value() && Locator::handSystem::value().GetHeldObject().has_value())
		{
			Locator::handSystem::value().ForceDropHeld();
		}
		worship::player::CancelAllSpellsCharging(PlayerNames::PLAYER_ONE);
		ecs::creature_loop::ReleaseLeashHeldInHand(PlayerNames::PLAYER_ONE);
	}
	if (state.playing)
	{
		End();
	}
	game_clock::SetSpeed(1.0f);
	state.task = task;
	state.testHook = false;
	// The wide screen bars are turned on when they are off, then snapped in at once every time
	if (auto* helpSystem = help::Get(); helpSystem != nullptr && helpSystem->GetWideScreen() == 0)
	{
		helpSystem->SetWideScreen(1, 0);
	}
	if (Locator::cinematicDirectorSystem::has_value())
	{
		Locator::cinematicDirectorSystem::value().SnapWideScreen();
	}
	// The interface is made active, so that the recorded input moves the hand
	interface_active::SetActive(true);
	state.records = Load(name);
	state.head.read = 0;
	state.frame = Frame {};
	state.hints = 0;
	state.playing = false;
	if (!state.records.empty())
	{
		// The first record at once, and its time; the start is now; the real mouse is turned off
		state.playing = true;
		ProcessRecords(true, game_clock::VisualMs());
	}
	// The script's flags are set whatever the playback start did
	state.waitTrigger = waitTrigger;
	state.trigger = false;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand demo {}: {} records, task {}, wait trigger {}, keep hand {}", name,
	                   state.records.size(), task, waitTrigger, keepHand);
	return state.playing;
}

std::optional<uint32_t> PlayingHints()
{
	const auto& state = Get();
	return state.playing ? std::optional(state.hints) : std::nullopt;
}

bool IsPlaying(uint32_t task)
{
	const auto& state = Get();
	if (task != 0 && task != state.task)
	{
		return false;
	}
	return state.playing;
}

void MarkStartedByTestHook()
{
	Get().testHook = true;
}

bool IsTestHookPlaying()
{
	const auto& state = Get();
	return state.playing && state.testHook;
}

void SetListedTestDemo(std::string_view name)
{
	ListedDemoState().name = std::string(name);
}

std::optional<std::string> ListedTestDemo()
{
	return ListedDemoState().name;
}

bool ConsumeTrigger()
{
	auto& state = Get();
	const bool trigger = state.trigger;
	state.trigger = false;
	return trigger;
}

void End()
{
	auto& state = Get();
	// Only while playing: the file is closed, the playing flag and task are cleared, the real mouse is back, the
	// script's wait-for-trigger flag is cleared, and the interface is made inactive when the wide screen has an owner.
	// The bars stay and the pending trigger stays.
	if (!state.playing)
	{
		return;
	}
	if (const auto* helpSystem = help::Get(); helpSystem != nullptr && helpSystem->GetWideScreenOwner() != 0)
	{
		interface_active::SetActive(false);
	}
	state.playing = false;
	state.task = 0;
	state.testHook = false;
	state.records.clear();
	state.head.read = 0;
	state.waitTrigger = false;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand demo ended");
}

void EndIfTask(uint32_t task)
{
	if (Get().playing && Get().task == task)
	{
		End();
	}
}

std::optional<Frame> Update(uint32_t nowMs)
{
	auto& state = Get();
	if (!state.playing)
	{
		return std::nullopt;
	}
	ProcessRecords(false, nowMs);
	// OPENBLACK_HAND_DEMO_TRACE=1: every 60 frames the record reached and the clocks
	if (static const bool trace = std::getenv("OPENBLACK_HAND_DEMO_TRACE") != nullptr; trace)
	{
		if (++TraceState().frames % 60 == 0)
		{
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				SPDLOG_LOGGER_INFO(
				    logger, "Hand demo trace: record {}/{}, now {} start {} first {}, mouse ({:.3f}, {:.3f}) grip {} action {}",
				    state.head.read, state.records.size(), nowMs, state.head.startMs, state.head.firstTimeMs,
				    state.frame.mouse.x, state.frame.mouse.y, state.frame.grip, state.frame.action);
			}
		}
	}
	// the frame of the last record due; on the frame the file ends the last record still counts
	return state.frame;
}

} // namespace openblack::hand_demo
