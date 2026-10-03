/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandDemo.h"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/ScreenFade.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "Input/InterfaceActive.h"
#include "Locator.h"
#include "Worship/PlayerSpellIcons.h"

namespace openblack::hand_demo
{
namespace
{

/// One 124-byte record of a .hnd file (no header; written by the recorder fn_005DB4D0 0x5DB4D0)
struct Record
{
	uint32_t message;   ///< +0x00 interface message (table 0xD186B8): 0 MOUSE_MOVE, 1 GRAB_DOWN, 2 GRAB_UP, 3 ACTION_DOWN, 4 ACTION_UP
	glm::vec2 mouse;    ///< +0x34 the mouse, normalised 0..1 (fn_0081E8E0)
	glm::vec3 eye;      ///< +0x3C the camera position (GCamera +0x118 / +0x148 / +0x178)
	glm::vec3 focus;    ///< +0x48 the camera focus (GCamera +0x88 / +0xB8 / +0xE8)
	uint32_t trigger;   ///< +0x5C the byte [0xE853AD] when it changed since the last record, else 0 ((inferred) the space key)
	uint32_t timeMs;    ///< +0x60 g_game +0x25053C when recorded
};
constexpr size_t k_RecordSize = 0x7C;

struct State
{
	std::vector<Record> records;
	size_t read {0};          ///< [0xD18854] / 0x7C: the records read
	bool playing {false};     ///< GInterface +0x15C
	uint32_t task {0};        ///< +0x160
	uint32_t firstTimeMs {0}; ///< +0x164: the time of the first record
	uint32_t startMs {0};     ///< +0x168: g_game +0x25053C at the start (re-based while waiting for the trigger)
	bool waitTrigger {false}; ///< the script's +0x8C
	bool trigger {false};     ///< the script's +0x88
	Frame frame;
};

State& Get()
{
	static State state;
	return state;
}

float ReadFloat(const uint8_t* data, size_t offset)
{
	float value = 0.0f;
	std::memcpy(&value, data + offset, sizeof(value));
	return value;
}

uint32_t ReadU32(const uint8_t* data, size_t offset)
{
	uint32_t value = 0;
	std::memcpy(&value, data + offset, sizeof(value));
	return value;
}

std::vector<Record> Load(std::string_view name)
{
	// GScript::PlayHandDemo 0x6FDAD0: sprintf(".\Data\HandDemo\%s.hnd" [0xC0DA18], name); LHOSFile::Open mode 2
	std::vector<Record> records;
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		const auto bytes = fileSystem.ReadAll(
		    fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / "HandDemo" / (std::string(name) + ".hnd")));
		// fn_005DAEE0 reads whole records only: a short read ends the playback
		records.reserve(bytes.size() / k_RecordSize);
		for (size_t offset = 0; offset + k_RecordSize <= bytes.size(); offset += k_RecordSize)
		{
			const auto* data = bytes.data() + offset;
			records.push_back({ReadU32(data, 0x00), {ReadFloat(data, 0x34), ReadFloat(data, 0x38)},
			                   {ReadFloat(data, 0x3C), ReadFloat(data, 0x40), ReadFloat(data, 0x44)},
			                   {ReadFloat(data, 0x48), ReadFloat(data, 0x4C), ReadFloat(data, 0x50)}, ReadU32(data, 0x5C),
			                   ReadU32(data, 0x60)});
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Hand demo {}: {}", name, e.what());
		records.clear();
	}
	return records;
}

/// fn_005DAEE0(first, handled): the records due, in order. With `first` the first record is taken whatever its time and
/// its time is returned.
uint32_t ProcessRecords(bool first, uint32_t nowMs)
{
	auto& state = Get();
	while (true)
	{
		// LHOSFile::Read of 0x7C bytes; a short read is EndPlayBack (0x5DB33D). Every read sets the speed to 1
		// (GGame::SetSpeed(1.0) 0x5DAFC4).
		if (state.read >= state.records.size())
		{
			End();
			return 0;
		}
		const auto& record = state.records[state.read];
		++state.read;
		game_clock::SetSpeed(1.0f);
		// 0x5DB00A: not due while (time - first time) > (now - start), unsigned: seek back one record
		if (!first && record.timeMs - state.firstTimeMs > nowMs - state.startMs)
		{
			--state.read;
			return 0;
		}
		// 0x5DB034 / 0x5DB3A4: waiting for the trigger with one pending, the start is re-based so that time stands still
		// (+0x168 = +0x164 - time + now) and the record waits
		if (state.waitTrigger && state.trigger)
		{
			state.startMs = state.firstTimeMs - record.timeMs + nowMs;
			--state.read;
			return 0;
		}
		// 0x5DB054: a MOUSE_MOVE moves the mouse (fn_0081E920, LHMouse::SetPosition 0x5DB081). (pending) any other
		// message copies the recorded throw information into the render hand (CHand +0x48C8, rep movsd 0x5DB0B6)
		if (record.message == 0)
		{
			state.frame.mouse = record.mouse;
		}
		// (not ported) 0x5DB0BC..0x5DB0EA: the tricon flags [0xC5B0F4] (0 for the first and the last 20 records) and
		// [0xC5B0AC]: openblack has no camera tricons
		// 0x5DB10D: GCamera::SetPositionAndFocus(eye, focus) and LH3DTech::UpdateCamera(eye, focus)
		state.frame.eye = record.eye;
		state.frame.focus = record.focus;
		// 0x5DB125: a trigger in the record is the script's pending trigger (+0x88 = 1)
		if (record.trigger != 0)
		{
			state.trigger = true;
		}
		// 0x5DB2EE: the message goes through the interface's dispatcher fn_005D9BC0 (the button bits of the message
		// table 0xD186B8). (approximate) openblack's hand reads the buttons' state once a frame, so a press and a
		// release due in the same frame are not both seen. (pending) the object of ACTION messages found again within
		// 3 m by its script type and subtype (fn_005DB4B0, FindNearPos 0x6F7280, fn_005D5E40)
		switch (record.message)
		{
		case 1:
			state.frame.grip = true;
			break;
		case 2:
			state.frame.grip = false;
			break;
		case 3:
			state.frame.action = true;
			break;
		case 4:
			state.frame.action = false;
			break;
		default:
			break;
		}
		if (first)
		{
			return record.timeMs;
		}
	}
}

} // namespace

bool Play(std::string_view name, uint32_t task, bool waitTrigger, bool keepHand)
{
	auto& state = Get();
	// GInterface::StartPlayBack 0x5DAD60. Without keepHand: an object in the hand is dropped (fn_005D4350, the 0x4D packet
	// of ForceDropHeld) and the player's spells stop charging (GPlayer::CancelAllSpellsCharging 0x64BC60). (not
	// ported) a creature on the leash is let go (GLeashStatus::SetOn(creature, 0)): openblack has no creature.
	if (!keepHand)
	{
		if (Locator::handSystem::has_value() && Locator::handSystem::value().GetHeldObject().has_value())
		{
			Locator::handSystem::value().ForceDropHeld();
		}
		worship::player::CancelAllSpellsCharging(PlayerNames::PLAYER_ONE);
	}
	if (state.playing)
	{
		End();
	}
	game_clock::SetSpeed(1.0f);
	state.task = task;
	// HelpSystem::SetWideScreen(1, 0) when +0x45E8 is 0, then fn_005C6C40 (the bars at once) every time
	if (auto* helpSystem = help::Get(); helpSystem != nullptr && helpSystem->GetWideScreen() == 0)
	{
		helpSystem->SetWideScreen(1, 0);
	}
	if (Game::Instance() != nullptr)
	{
		Game::Instance()->GetScreenFade().SnapWideScreen();
	}
	// GInterface::SetActive(1), so that the recorded input moves the hand
	interface_active::SetActive(true);
	state.records = Load(name);
	state.read = 0;
	state.frame = Frame {};
	state.playing = false;
	if (!state.records.empty())
	{
		// +0x164 = fn_005DAEE0(1, 0): the first record at once, and its time; +0x168 = now; +0x15C = 1;
		// SetTurnOffMouseMove(1)
		state.playing = true;
		const uint32_t now = game_clock::VisualMs();
		state.firstTimeMs = ProcessRecords(true, now);
		state.startMs = now;
	}
	// GScript::PlayHandDemo 0x6FDB59 / 0x6FDB6D, whatever StartPlayBack did
	state.waitTrigger = waitTrigger;
	state.trigger = false;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand demo {}: {} records, task {}, wait trigger {}, keep hand {}", name,
	                   state.records.size(), task, waitTrigger, keepHand);
	return state.playing;
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
	// GInterface::EndPlayBack 0x5DB3F0, only while playing: the file closed, +0x15C = +0x160 = 0,
	// SetTurnOffMouseMove(0), the script's +0x8C = 0, SetActive(0) when HelpSystem +0x45EC is set. The bars stay and the
	// pending trigger (+0x88) stays.
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
	state.records.clear();
	state.read = 0;
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
		static uint32_t frames = 0;
		if (++frames % 60 == 0)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Hand demo trace: record {}/{}, now {} start {} first {}, mouse ({:.3f}, {:.3f}) grip {} action {}",
			                   state.read, state.records.size(), nowMs, state.startMs, state.firstTimeMs, state.frame.mouse.x,
			                   state.frame.mouse.y, state.frame.grip, state.frame.action);
		}
	}
	// the frame of the last record due; on the frame the file ends the last record still counts
	return state.frame;
}

} // namespace openblack::hand_demo
