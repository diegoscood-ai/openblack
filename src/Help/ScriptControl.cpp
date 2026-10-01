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

#include <string_view>

#include <spdlog/spdlog.h>

#include "Audio/ScriptAudioState.h"
#include "HelpSystem.h"

namespace openblack::help::script_control
{

namespace
{
bool Tracing()
{
	static const bool k_Trace = std::getenv("OPENBLACK_TEXT_TRACE") != nullptr;
	return k_Trace && spdlog::get("game") != nullptr;
}

/// GScript::ScriptErrorMessage 0x6F62B0 / ScriptWarningMessage 0x6F62C0: a debug message, the script goes on
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
} // namespace

void CameraControl::Reset()
{
	drawHighlight = 1; // 0x6EB2FA (edi = 1)
	drawLeash = 1;     // 0x6EB300
	field7C = 0;       // 0x6EB303 (ebx = 0)
	// Not original (mod game.skip-intro, "free start"): a new game looks for its opening task again
	freeStartTask = 0;
	freeStartArmed = true;
}

bool IsFreeStartTask(const CameraControl& camera, uint32_t task)
{
	return camera.freeStartTask != 0 && camera.freeStartTask == task;
}

CameraControl& GetCameraControl()
{
	static CameraControl camera;
	return camera;
}

bool StartDialogue(HelpSystem& help, const Vm& vm)
{
	auto owner = help.GetDialogueOwner(); // 0x71069C (HelpSystem+0x45CC)
	const auto task = TaskNumber(vm);     // 0x7106A9
	if (owner == 0)
	{
		// 0x71073A..0x71075F: both advisors go home (SpiritHome(1, 0), SpiritHome(2, 0)), then the request
		help.SpiritHome(1, 0);
		help.SpiritHome(2, 0);
	}
	else
	{
		if (owner == task) // 0x7106B8
		{
			ScriptMessage("Script Asking For Dialogue Control It already has! - Dangerous"); // 0xC20B18
			return true;                                                                  // 0x7106CF
		}
		// 0x7106E2..0x710713: a Script task takes the dialogue from a Help one: StopHelpScripts (their task-stop
		// callbacks give it back) and read the owner again
		if (TaskType(vm, owner) == k_ScriptTypeHelp && CurrentTaskType(vm) == k_ScriptTypeScript)
		{
			if (vm.stopTasksOfType)
			{
				vm.stopTasksOfType(k_HelpScriptTypes); // 0x6EC780
			}
			owner = help.GetDialogueOwner(); // 0x71070D
		}
		if (owner != 0) // 0x710713
		{
			if (Tracing())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: START_DIALOGUE task {}: task {} has it", task, owner);
			}
			return false; // 0x71076C
		}
	}
	// 0x710722: the result of DialogueControlRequest is not used, START_DIALOGUE pushes true (0x71072E) even when the
	// wide screen of another task refuses it
	const bool granted = help.DialogueControlRequest(task);
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: START_DIALOGUE task {} (control {})", task, granted);
	}
	return true;
}

void EndDialogue(HelpSystem& help, audio::ScriptAudioState& audio, const Vm& vm)
{
	if (help.GetDialogueOwner() != TaskNumber(vm)) // 0x71078C..0x71079F
	{
		return;
	}
	// 0x7107AB..0x7107DC: SpiritHome(1 / 2, the task is a Help script)
	const int32_t helpScript = CurrentTaskType(vm) == k_ScriptTypeHelp ? 1 : 0;
	help.SpiritHome(1, helpScript);
	help.SpiritHome(2, helpScript);
	const auto task = TaskNumber(vm);  // 0x7107E7
	help.ReleaseDialogueControl(task); // 0x7107F9 fn_005C6800
	audio.EndDialogue();               // 0x71080A / 0x710820: GScript+0x84 = 1, +0x9C = 0
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: END_DIALOGUE task {}", task);
	}
}

bool IsSpiritReady(const HelpSystem& help)
{
	return !help.IsDialogueControlled(); // 0x71083B..0x71084A
}

bool SetWideScreen(HelpSystem& help, int32_t on, const Vm& vm)
{
	const auto owner = help.GetWideScreenOwner(); // 0x6F7C16 (+0x45EC)
	const auto task = TaskNumber(vm);             // 0x6F7C1E
	if (on != 0 && owner == task) // 0x6F7C23..0x6F7C2B
	{
		ScriptMessage("Script asking for Widescreen it has control of! Bad"); // 0xC0D3B4
	}
	if (owner != 0 && owner != task) // 0x6F7C3A..0x6F7C40: another task holds it
	{
		return false;
	}
	help.SetWideScreen(on, TaskNumber(vm)); // 0x6F7C48..0x6F7C5A
	return true;
}

bool StartCameraControl(CameraControl& camera, const Vm& vm, bool insideCitadel, bool cameraTaken)
{
	bool result = false;
	if (insideCitadel) // 0x6ECCA6..0x6ECCB2 (g_game+0x205A28 == 1)
	{
		// 0x6ECD33..0x6ECD69: only temple scripts get it there, without a camera mode
		if ((CurrentTaskType(vm) & k_CitadelCameraTypes) != 0)
		{
			camera.owner = TaskNumber(vm); // 0x6ECD5B
			result = true;
		}
	}
	else if (cameraTaken) // 0x6ECCBA..0x6ECCC6 fn_00461140
	{
		camera.owner = TaskNumber(vm); // 0x6ECCDF
		camera.drawHighlight = 0;      // 0x6ECCF0
		camera.drawLeash = 0;          // 0x6ECD06
		result = true;
	}
	else
	{
		ScriptMessage("Note-Camera control Failed"); // 0xC0C130
	}
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: START_CAMERA_CONTROL task {} -> {}", TaskNumber(vm), result);
	}
	return result; // 0x6ECD1D / 0x6ECD69
}

void ReleaseCameraControl(CameraControl& camera, audio::ScriptAudioState& audio)
{
	// 0x6ECD71..0x6ECE2E, pending (openblack has no camera modes): the current mode of GGame::GetCamera() (+0x28[+0x58]);
	// none -> "Script camera has been removed!" (0xC0C0CC); else GScript::ReleaseDualCamera 0x6ED410 and, when the mode
	// is a CameraModeScript (__RTDynamicCast), its vt+0x30 and a new CameraModeNew3 (0x4572E0, 0x300 bytes), or "We are
	// in the wrong camera mode! - exception happened?" (0xC0C14C). Pending too: GCamera::SetCameraFov(k_ScriptEndFov,
	// k_ScriptEndFovTime) (0x6ECE48)
	// Not original (mod game.skip-intro, "free start"): the land's opening task has given the camera back, so from here
	// the script gets the player back too
	if (IsFreeStartTask(camera, camera.owner))
	{
		camera.freeStartTask = 0;
	}
	camera.owner = 0;         // 0x6ECE59
	audio.creatureSound = 1;  // 0x6ECE74 (+0x84)
	camera.drawHighlight = 1; // 0x6ECE85
	camera.drawLeash = 1;     // 0x6ECE97
	// 0x6ECEA1 fn_0042A5F0(1): LH_AudioSystem+0xC4 (the sound effects off) = 0, without LHSampleStopAll. openblack has no
	// such switch, and nothing in the game sets it to 1 (dev\tmp_dis\audio\engine.md), so it changes nothing.
	// 0x6ECEA6..0x6ECECA: every SuperVillager Release()d and the list emptied; 0x6ECEC4 GLandscape::DrawListRebuildCount
	// = 1 (not ported: no SuperVillager, no landscape draw list)
	camera.field7C = 0; // 0x6ECEDB
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: camera control released");
	}
}

bool EndCameraControl(CameraControl& camera, audio::ScriptAudioState& audio, const Vm& vm)
{
	if (camera.owner != TaskNumber(vm)) // 0x6ECEFC..0x6ECF10
	{
		return false;
	}
	ReleaseCameraControl(camera, audio); // 0x6ECF12
	return true;
}

bool ReleaseCameraOf(CameraControl& camera, audio::ScriptAudioState& audio, uint32_t task)
{
	if (camera.owner != task) // 0x6ECF20..0x6ECF2A
	{
		return false;
	}
	ReleaseCameraControl(camera, audio); // 0x6ECF2C
	return true;
}

void OnTaskStopped(uint32_t task, HelpSystem* help, CameraControl& camera, audio::ScriptAudioState& audio)
{
	if (help == nullptr) // 0x6EC6D5 / 0x6EC6DF
	{
		return;
	}
	help->ReleaseDialogueControl(task);  // 0x6EC6E9
	ReleaseCameraOf(camera, audio, task); // 0x6EC70C
}

} // namespace openblack::help::script_control
