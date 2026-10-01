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

#include <functional>
#include <string_view>

// Which script task has the dialogue, the wide screen and the camera: the GScript side (runblack.exe W120) of CHL 030
// START_CAMERA_CONTROL, 031 END_CAMERA_CONTROL, 032 SET_WIDESCREEN, 120 START_DIALOGUE, 121 END_DIALOGUE and 122
// IS_DIALOGUE_READY, and the part of the task-stop callback 0x6EC6D0 that gives them back. The HelpSystem side
// (+0x45CC, +0x45E8, +0x45EC) is in HelpSystem.h. Sources: dev\tmp_dis\audio\script.md §2.8 and the disassembly of
// 0x6ECCA0..0x6ECF31, 0x710690..0x710853, 0x6F7BF0..0x6F7C63, 0x5C6670..0x5C6896 and 0x6EC6D0..0x6EC74D (bwdis.py), and of
// ScriptLibraryR.dll for the task queries (LHVM.h), cited at each step.

namespace openblack::audio
{
struct ScriptAudioState;
}

namespace openblack::help
{
class HelpSystem;
}

namespace openblack::help::script_control
{

/// VMScriptType (the task's +0x158 in ScriptLibraryR.dll; lhvm::ScriptType): Script
constexpr uint32_t k_ScriptTypeScript = 1;
/// VMScriptType Help
constexpr uint32_t k_ScriptTypeHelp = 2;
/// StartCameraControl 0x6ECD3E: inside the citadel only TempleHelp | TempleSpecial tasks get the camera
constexpr uint32_t k_CitadelCameraTypes = 0x18;
/// GScript::StopHelpScripts 0x6EC780: StopScriptsOfType(0x4A), Help | TempleHelp | MultiplayerHelp
constexpr uint32_t k_HelpScriptTypes = 0x4A;
/// fn_006ECD70 0x6ECD98 / 0x6ECE35: GCamera::SetCameraFov(fn_00443670 = 1.2217305 rad (0x8C762C, 70 degrees), 0.5)
constexpr float k_ScriptEndFov = 1.2217305f;
constexpr float k_ScriptEndFovTime = 0.5f;

/// What these functions ask the script VM (ScriptDLL, components/ScriptLibrary LHVM.h). Unset: task 0 and type 1.
struct Vm
{
	/// ScriptDLL::TaskNumber 0x6F69F0: the task running now (0 outside a task)
	std::function<uint32_t()> taskNumber;
	/// ScriptDLL::GetCurrentTaskScriptType 0x6F6A90 (1 outside a task)
	std::function<uint32_t()> currentTaskType;
	/// ScriptDLL::GetScriptType 0x6F6C50 (1 for a task that does not exist)
	std::function<uint32_t(uint32_t task)> taskType;
	/// ScriptDLL::StopTasksOfType 0x6F68F0 (GScript::StopScriptsOfType 0x6F0CC0)
	std::function<void(uint32_t typeMask)> stopTasksOfType;
	/// ScriptDLL::PUSH 0x6F6BA0 of a float (VMType 2)
	std::function<void(float value)> pushFloat;
	/// ScriptDLL::StartScript 0x6F6880(name, VMScriptType mask): the parameters come from the stack
	std::function<void(std::string_view name, uint32_t typeMask)> startScript;
	/// GGame::IsMultiplayerGame 0x552F80 (GScript::StartScript 0x6EB724). Unset: false.
	std::function<bool()> multiplayer;
};

/// GScript::StartScript 0x6EB710: the types a script may have, 0x7F in a single-player game, 0x60 in a multiplayer one
constexpr uint32_t k_SinglePlayerScriptTypes = 0x7F;
constexpr uint32_t k_MultiplayerScriptTypes = 0x60;
/// HelpSystem::StopHelpScriptsForNewHelp 0x5C8C40 (StopRunningScripts 0x5C8C80 jumps to it): a task that has the
/// dialogue (+0x45CC) and whose type has neither Help (2) nor MultiplayerHelp (0x40) (`test al, 0x42`) keeps it: false;
/// else GScript::StopHelpScripts 0x6EC780 (the types 0x4A) and true
bool StopHelpScriptsForNewHelp(const HelpSystem& help, const Vm& vm);
/// HelpSystem::RunMessage 0x5C8CE0(first, last, script): false for first > last or when StopRunningScripts refuses;
/// else +0x560 = the turn, PUSH(float first), PUSH(float last) and GScript::StartScript(script) (GGuidance::HelpSpiritSay
/// 0x71D2AC: "MultiHelpJustTalkWithText", whose two parameters WhichTextFirst / WhichTextLast are these); true
bool RunMessage(HelpSystem& help, uint32_t first, uint32_t last, std::string_view script, const Vm& vm, uint32_t turn);

/// The GScript fields (g_game+0x250090) of the script camera. GScript::Reset 0x6EB2D0 sets +0x80 = +0x78 = 1
/// (0x6EB2FA, 0x6EB300) and +0x7C = 0 (0x6EB303); it does not write +0xA8 (0 from the constructor: inferred)
struct CameraControl
{
	/// +0xA8: the task that has the camera (StartCameraControl 0x6ECCDF / 0x6ECD5B; 0 in fn_006ECD70 0x6ECE59)
	uint32_t owner {0};
	/// +0x78: the leashes are drawn (SET_DRAW_LEASH 0x708C9D; read by GInterface::DrawAllLeashes 0x5D9382). The opcode
	/// is not ported: only START/END_CAMERA_CONTROL write it here
	int32_t drawLeash {1};
	/// +0x7C: only fn_006ECD70 (0x6ECEDB, = 0) and GScript::Reset write it; no reader found (script_scanfields.py)
	int32_t field7C {0};
	/// +0x80: the highlights are drawn (SET_DRAW_HIGHLIGHT 0x708CCD; read by ScriptHighlight::Draw 0x709C9D). Not
	/// ported either
	int32_t drawHighlight {1};

	/// The camera part of GScript::Reset 0x6EB2D0
	void Reset();
};

/// GScript's camera state of the running game (one, as g_game+0x250090)
CameraControl& GetCameraControl();

/// GScript::StartDialogue 0x710690 (CHL 120 START_DIALOGUE): the bool it pushes
bool StartDialogue(HelpSystem& help, const Vm& vm);
/// GScript::EndDialogue 0x710780 (CHL 121 END_DIALOGUE)
void EndDialogue(HelpSystem& help, audio::ScriptAudioState& audio, const Vm& vm);
/// GScript::IsSpiritReady 0x710830 (CHL 122 IS_DIALOGUE_READY): !HelpSystem::IsDialogueControlled
[[nodiscard]] bool IsSpiritReady(const HelpSystem& help);
/// GScript::SetWideScreen 0x6F7BF0 (CHL 032 SET_WIDESCREEN), after the pop (`on` is the popped value as it is, 0x6F7C1C):
/// true when it called HelpSystem::SetWideScreen
bool SetWideScreen(HelpSystem& help, int32_t on, const Vm& vm);

/// GScript::StartCameraControl 0x6ECCA0 (CHL 030 START_CAMERA_CONTROL): the bool it pushes. `insideCitadel` is
/// g_game+0x205A28 == 1; `cameraTaken` is fn_00461140(GGame::GetCamera()) != 0, the script camera mode created
/// (CameraModeScript 0x461180, a CameraModeFollow; null when GCamera::CantExitCurrentMode 0x441B70)
bool StartCameraControl(CameraControl& camera, const Vm& vm, bool insideCitadel, bool cameraTaken);
/// GScript::EndCameraControl 0x6ECEF0 (CHL 031 END_CAMERA_CONTROL): fn_006ECD70 when this task has the camera. True
/// when it released it
bool EndCameraControl(CameraControl& camera, audio::ScriptAudioState& audio, const Vm& vm);
/// fn_006ECD70 (inside the W120 symbol StartCameraControl, which spans 0x6ECCA0..0x6ECEF0): give the camera back
void ReleaseCameraControl(CameraControl& camera, audio::ScriptAudioState& audio);
/// fn_006ECF20(task) (GScript method, from the task-stop callback 0x6EC70C): fn_006ECD70 when the task has the camera
bool ReleaseCameraOf(CameraControl& camera, audio::ScriptAudioState& audio, uint32_t task);

/// The task-stop callback 0x6EC6D0 (fn_006EB1D0 passes it to ScriptDLL at 0x6EB1F1), for the parts ported: nothing
/// without a game or HelpSystem (0x6EC6D5 / 0x6EC6DF); HelpSystem fn_005C6800(task) (0x6EC6E9) and GScript
/// fn_006ECF20(task) (0x6EC70C). Not ported: HelpSystem fn_005C78C0(task) (0x6EC6FA), GScript fn_006FAA40(task)
/// (0x6EC71E) and GInterface::EndPlayBack when IsPlayBack(task) (0x6EC72A..0x6EC748). ScriptLibraryR.dll calls it
/// before the task leaves the list (StopTask 0x100065B0: the callback 0x1003BDD0 at 0x100065CD, the unlink from
/// 0x10006612), so GetScriptType(task) in fn_005C6800 still sees it, as LHVM::StopTask does (InvokeStopTaskCallback
/// before the erase)
void OnTaskStopped(uint32_t task, HelpSystem* help, CameraControl& camera, audio::ScriptAudioState& audio);

} // namespace openblack::help::script_control
