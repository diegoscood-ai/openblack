/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptControl.h"

#include <cstdlib>

#include <functional>
#include <optional>
#include <string_view>

#include <spdlog/spdlog.h>

#include "Audio/Services/ScriptAudioState.h"
#include "Debug/DebugEnv.h"
#include "Debug/TraceLog.h"
#include "ECS/SuperVillager.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "GameClock.h"
#include "HelpSystem.h"
#include "Input/HandDemo.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptSwitchChanges.h"

namespace openblack::help::script_control
{

namespace
{
bool Tracing()
{
	static const bool k_Trace = debug_env::TextTrace();
	return debug_env::TraceLogs(k_Trace);
}

/// A script error or warning: a debug message, the script goes on
void ScriptMessage(std::string_view message)
{
	if (auto logger = spdlog::get("scripting"))
	{
		SPDLOG_LOGGER_WARN(logger, "{}", message);
	}
}

uint32_t TaskNumber(const Vm& vm)
{
	return vm.taskNumber ? vm.taskNumber() : 0;
}

uint32_t CurrentTaskType(const Vm& vm)
{
	return vm.currentTaskType ? vm.currentTaskType() : k_ScriptTypeScript;
}

uint32_t TaskType(const Vm& vm, uint32_t task)
{
	return vm.taskType ? vm.taskType(task) : k_ScriptTypeScript;
}

/// Stops the help scripts: Help, TempleHelp and MultiplayerHelp
void StopHelpScripts(const Vm& vm)
{
	if (vm.stopTasksOfType)
	{
		vm.stopTasksOfType(k_HelpScriptTypes);
	}
}

/// The task gives the dialogue back when it has it, a Help script's advisors going home at once
void ReleaseDialogue(const HelpSystem& help, ecs::systems::DialogueControlSystemInterface& dialogue, uint32_t task)
{
	if (dialogue.GetOwner() != task)
	{
		return;
	}
	const bool helpScript = help.TaskScriptType(task) == k_ScriptTypeHelp;
	if (dialogue.Release(task, helpScript) && Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: dialogue control of task {} released", task);
	}
}

/// Writes the switches a change of control sets; the creatures' voices only with an audio state
void ApplySwitchChanges(CameraControl& camera, audio::ScriptAudioState* audio, const script::switches::SwitchChanges& changes)
{
	if (changes.leashesDrawn)
	{
		camera.drawLeash = *changes.leashesDrawn ? 1 : 0;
	}
	if (changes.highlightsDrawn)
	{
		camera.drawHighlight = *changes.highlightsDrawn ? 1 : 0;
	}
	if (changes.otherCreatureVoices && audio != nullptr)
	{
		audio->creatureSound = *changes.otherCreatureVoices ? 1 : 0;
	}
}
} // namespace

void CameraControl::Reset()
{
	drawHighlight = 1;
	drawLeash = 1;
	field7C = 0;
}

CameraControl& GetCameraControl()
{
	return openblack::Locator::scriptState::value().Get<CameraControl>();
}

bool StartDialogue(HelpSystem& help, ecs::systems::DialogueControlSystemInterface& dialogue, const Vm& vm)
{
	auto owner = dialogue.GetOwner();
	const auto task = TaskNumber(vm);
	if (owner == 0)
	{
		// Both advisors go home, then the request
		help.SpiritHome(1, 0);
		help.SpiritHome(2, 0);
	}
	else
	{
		if (owner == task)
		{
			ScriptMessage("Script Asking For Dialogue Control It already has! - Dangerous");
			return true;
		}
		// A Script task takes the dialogue from a Help one: stop the help scripts (their task-stop callbacks give it
		// back) and read the owner again
		if (TaskType(vm, owner) == k_ScriptTypeHelp && CurrentTaskType(vm) == k_ScriptTypeScript)
		{
			StopHelpScripts(vm);
			owner = dialogue.GetOwner();
		}
		if (owner != 0)
		{
			if (Tracing())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: START_DIALOGUE task {}: task {} has it", task, owner);
			}
			return false;
		}
	}
	// Whether the request is granted is not used, START_DIALOGUE pushes true even when the wide screen of another task
	// refuses it
	const auto wideScreenOwner = help.GetWideScreenOwner();
	const bool granted = !dialogue.IsControlled(wideScreenOwner);
	dialogue.Request(task, wideScreenOwner);
	if (granted && Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: dialogue control to task {}", task);
	}
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: START_DIALOGUE task {} (control {})", task, granted);
	}
	return true;
}

void EndDialogue(HelpSystem& help, ecs::systems::DialogueControlSystemInterface& dialogue, audio::ScriptAudioState& audio,
                 const Vm& vm)
{
	if (dialogue.GetOwner() != TaskNumber(vm))
	{
		return;
	}
	// Both advisors go home; the second argument says whether the task is a Help script
	const int32_t helpScript = CurrentTaskType(vm) == k_ScriptTypeHelp ? 1 : 0;
	help.SpiritHome(1, helpScript);
	help.SpiritHome(2, helpScript);
	const auto task = TaskNumber(vm);
	ReleaseDialogue(help, dialogue, task);
	// script::switches::DialogueEnded(), the creatures' voices back on, and the music beat back to 0
	audio.EndDialogue();
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: END_DIALOGUE task {}", task);
	}
}

bool IsSpiritReady(const ecs::systems::DialogueControlSystemInterface& dialogue, uint32_t wideScreenOwner)
{
	return !dialogue.IsControlled(wideScreenOwner);
}

bool SetWideScreen(HelpSystem& help, int32_t on, const Vm& vm)
{
	const auto owner = help.GetWideScreenOwner();
	const auto task = TaskNumber(vm);
	if (on != 0 && owner == task)
	{
		ScriptMessage("Script asking for Widescreen it has control of! Bad");
	}
	if (owner != 0 && owner != task) // another task holds it
	{
		return false;
	}
	help.SetWideScreen(on, TaskNumber(vm));
	return true;
}

bool StartCameraControl(CameraControl& camera, ecs::systems::ScriptControlSystemInterface& control, Camera& world, const Vm& vm,
                        bool insideCitadel)
{
	// Only temple scripts get it inside the citadel, without a camera mode
	const ecs::systems::ScriptControlSystemInterface::CameraRequest request {
	    .task = TaskNumber(vm),
	    .templeScript = (CurrentTaskType(vm) & k_CitadelCameraTypes) != 0,
	    .insideTemple = insideCitadel,
	};
	const bool result = control.StartCameraControl(world, request, {});
	if (result)
	{
		// Out in the world the leashes and highlights are hidden; inside the citadel nothing changes
		ApplySwitchChanges(camera, nullptr, script::switches::CameraTaken(insideCitadel));
	}
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: START_CAMERA_CONTROL task {} -> {}", TaskNumber(vm), result);
	}
	return result;
}

void ReleaseCameraControl(CameraControl& camera, audio::ScriptAudioState& audio)
{
	// The camera itself (the dual camera released, the script mode replaced by the player's, the field of view back to
	// k_ScriptEndFov over k_ScriptEndFovTime) has been given back by the script control already
	ApplySwitchChanges(camera, &audio, script::switches::CameraReleased());
	// The original also clears a "sound effects off" switch of the audio system, without stopping the samples.
	// openblack has no such switch, and nothing in the game sets it, so it changes nothing.
	// Every SuperVillager is released and the list emptied; the original also rebuilds the landscape draw list
	// (openblack has none)
	ecs::super_villager::ReleaseAll();
	camera.field7C = 0;
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: camera control released");
	}
}

bool EndCameraControl(CameraControl& camera, ecs::systems::ScriptControlSystemInterface& control, Camera& world,
                      audio::ScriptAudioState& audio, const Vm& vm)
{
	if (!control.EndCameraControl(world, TaskNumber(vm)))
	{
		return false;
	}
	ReleaseCameraControl(camera, audio);
	return true;
}

bool StartHelpScript(const ecs::systems::DialogueControlSystemInterface& dialogue, std::string_view name, const Vm& vm)
{
	return StartScriptWithArguments(dialogue, name, {}, vm);
}

bool FreeDialogueForScript(const ecs::systems::DialogueControlSystemInterface& dialogue, const Vm& vm)
{
	if (const auto owner = dialogue.GetOwner(); owner != 0 && TaskType(vm, owner) == 2)
	{
		StopHelpScripts(vm);
	}
	return dialogue.GetOwner() == 0;
}

bool StartScriptWithArguments(const ecs::systems::DialogueControlSystemInterface& dialogue,
                              std::optional<std::string_view> name, const std::function<void()>& pushArguments, const Vm& vm)
{
	if (!FreeDialogueForScript(dialogue, vm) || !name.has_value())
	{
		return false;
	}
	if (pushArguments)
	{
		pushArguments();
	}
	const bool multiplayer = vm.multiplayer && vm.multiplayer();
	if (vm.startScript)
	{
		vm.startScript(*name, multiplayer ? k_MultiplayerScriptTypes : k_SinglePlayerScriptTypes);
	}
	return true;
}

void StartTempleScript(std::string_view name, const Vm& vm)
{
	if ((vm.multiplayer && vm.multiplayer()) || !vm.startScript)
	{
		return;
	}
	vm.startScript(name, k_SinglePlayerScriptTypes);
}

void StopHelp(const Vm& vm)
{
	StopHelpScripts(vm);
}

void RoomNotAvailable(const Vm& vm)
{
	StopHelpScripts(vm);
	StartTempleScript("CitadelWorldRoomNotAvailable", vm);
}

bool StopHelpScriptsForNewHelp(const ecs::systems::DialogueControlSystemInterface& dialogue, const Vm& vm)
{
	// A task with the dialogue whose type has neither Help nor MultiplayerHelp keeps it
	if (const auto owner = dialogue.GetOwner(); owner != 0 && (TaskType(vm, owner) & 0x42u) == 0)
	{
		return false;
	}
	StopHelpScripts(vm);
	return true;
}

bool RunMessage(HelpSystem& help, const ecs::systems::DialogueControlSystemInterface& dialogue, uint32_t first, uint32_t last,
                std::string_view script, const Vm& vm, uint32_t turn, std::optional<std::pair<uint32_t, float>> townNeed)
{
	if (townNeed.has_value() && vm.pushFloat)
	{
		vm.pushFloat(static_cast<float>(townNeed->first)); // the need's text, as a float
		vm.pushFloat(townNeed->second);
	}
	if (first > last) // unsigned compare
	{
		return false;
	}
	if (!StopHelpScriptsForNewHelp(dialogue, vm))
	{
		return false;
	}
	help.SetMessageTurn(turn);
	if (vm.pushFloat)
	{
		vm.pushFloat(static_cast<float>(first)); // pushed as floats
		vm.pushFloat(static_cast<float>(last));
	}
	// The script types allowed: 0x7F, or 0x60 in a multiplayer game
	const bool multiplayer = vm.multiplayer && vm.multiplayer();
	if (vm.startScript)
	{
		vm.startScript(script, multiplayer ? k_MultiplayerScriptTypes : k_SinglePlayerScriptTypes);
	}
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: RunMessage({}, {}, {})", first, last, script);
	}
	return true;
}

void StartGameSpeed(ecs::systems::ScriptControlSystemInterface& control, const Vm& vm)
{
	control.StartGameSpeed(TaskNumber(vm));
}

void EndGameSpeed(ecs::systems::ScriptControlSystemInterface& control, const Vm& vm)
{
	// Script slow-motion control off when there is a camera (not ported, see StartGameSpeed), game speed 1.0
	if (control.EndGameSpeed(TaskNumber(vm)))
	{
		game_clock::SetSpeed(1.0f);
	}
}

void SetGameSpeed(const ecs::systems::ScriptControlSystemInterface& control, const Vm& vm, float speed)
{
	if (control.MaySetGameSpeed(TaskNumber(vm)))
	{
		game_clock::SetSpeed(speed);
	}
}

void OnTaskStopped(uint32_t task, HelpSystem* help, ecs::systems::DialogueControlSystemInterface* dialogue,
                   ecs::systems::ScriptControlSystemInterface* control, Camera* world, CameraControl& camera,
                   audio::ScriptAudioState& audio)
{
	if (help == nullptr)
	{
		return;
	}
	if (dialogue != nullptr)
	{
		ReleaseDialogue(*help, *dialogue, task);
	}
	else if (auto logger = spdlog::get("game"))
	{
		SPDLOG_LOGGER_ERROR(logger, "Task {} stopped without the dialogue control: its dialogue is not given back", task);
	}
	help->ReleaseWideScreenOf(task);
	if (control != nullptr && world != nullptr)
	{
		// The control gives the camera and the game speed back, then what follows each
		const auto released = control->TaskStopped(*world, task);
		if (released.camera)
		{
			ReleaseCameraControl(camera, audio);
		}
		if (released.gameSpeed)
		{
			game_clock::SetSpeed(1.0f);
		}
	}
	else if (auto logger = spdlog::get("game"))
	{
		SPDLOG_LOGGER_ERROR(logger, "Task {} stopped without the script control or the camera: neither is given back", task);
	}
	hand_demo::EndIfTask(task); // ends the hand demo playback when this task started it
}

} // namespace openblack::help::script_control
