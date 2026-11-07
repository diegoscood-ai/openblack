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
#include <optional>
#include <string_view>
#include <utility>

// Which script task has the dialogue, the wide screen and the camera: the script side of CHL 030 START_CAMERA_CONTROL,
// 031 END_CAMERA_CONTROL, 032 SET_WIDESCREEN, 120 START_DIALOGUE, 121 END_DIALOGUE and 122 IS_DIALOGUE_READY, and the
// part of the task-stop callback that gives them back. The HelpSystem side is in HelpSystem.h; the dialogue's owner is
// ecs::systems::DialogueControlSystemInterface's, the camera's and the game speed's ScriptControlSystemInterface's.

namespace openblack::audio
{
struct ScriptAudioState;
}

namespace openblack
{
class Camera;
}

namespace openblack::ecs::systems
{
class DialogueControlSystemInterface;
class ScriptControlSystemInterface;
} // namespace openblack::ecs::systems

namespace openblack::help
{
class HelpSystem;
}

namespace openblack::help::script_control
{

/// The task's script type (lhvm::ScriptType): Script
constexpr uint32_t k_ScriptTypeScript = 1;
/// Script type Help
constexpr uint32_t k_ScriptTypeHelp = 2;
/// Inside the citadel only TempleHelp | TempleSpecial tasks get the camera
constexpr uint32_t k_CitadelCameraTypes = 0x18;
/// The help scripts stopped for a new help: Help | TempleHelp | MultiplayerHelp
constexpr uint32_t k_HelpScriptTypes = 0x4A;
/// The field of view set when the script camera is released: 1.2217305 rad (70 degrees) over 0.5 s
constexpr float k_ScriptEndFov = 1.2217305f;
constexpr float k_ScriptEndFovTime = 0.5f;

/// What these functions ask the script VM. Unset: task 0 and type 1.
struct Vm
{
	/// The task running now (0 outside a task)
	std::function<uint32_t()> taskNumber;
	/// The current task's script type (1 outside a task)
	std::function<uint32_t()> currentTaskType;
	/// A task's script type (1 for a task that does not exist)
	std::function<uint32_t(uint32_t task)> taskType;
	/// Stops every task whose type is in the mask
	std::function<void(uint32_t typeMask)> stopTasksOfType;
	/// Pushes a float on the VM stack
	std::function<void(float value)> pushFloat;
	/// Starts a script (name, script type mask): the parameters come from the stack
	std::function<void(std::string_view name, uint32_t typeMask)> startScript;
	/// The game is a multiplayer one. Unset: false.
	std::function<bool()> multiplayer;
};

/// The types a started script may have, 0x7F in a single-player game, 0x60 in a multiplayer one
constexpr uint32_t k_SinglePlayerScriptTypes = 0x7F;
constexpr uint32_t k_MultiplayerScriptTypes = 0x60;
/// A task that has the dialogue and whose type has neither Help (2) nor MultiplayerHelp (0x40) keeps it: false; else
/// the help scripts (the types 0x4A) are stopped and true
bool StopHelpScriptsForNewHelp(const ecs::systems::DialogueControlSystemInterface& dialogue, const Vm& vm);
/// Used by the bubble properties (through CHLApi::StartHelpScript): a dialogue owner whose type is Help (2) -> the help
/// scripts are stopped; then, with no dialogue owner left, the script is started (true); else nothing (false)
bool StartHelpScript(const ecs::systems::DialogueControlSystemInterface& dialogue, std::string_view name, const Vm& vm);
/// Before a script that needs the dialogue starts: a dialogue owner whose type is Help (2) -> the help scripts are
/// stopped; true when no owner is left
bool FreeDialogueForScript(const ecs::systems::DialogueControlSystemInterface& dialogue, const Vm& vm);
/// A challenge's scroll tapped again (and the debugger's remind): a dialogue owner whose type is Help (2) -> the help
/// scripts are stopped, even before the challenge has a record; then, with no dialogue owner left, the challenge's
/// reminder starts (startReminder, true when it started); any other owner keeps it from starting (false)
bool StartChallengeReminder(const ecs::systems::DialogueControlSystemInterface& dialogue, const Vm& vm,
                            const std::function<bool()>& startReminder);
/// As StartHelpScript, for a script that takes arguments: a dialogue owner whose type is Help (2) -> the help scripts
/// are stopped; then, with no dialogue owner left and a script to start, pushArguments (when set) puts its arguments on
/// the stack and the script is started (true); else nothing more (false). No name: only the help is stopped.
bool StartScriptWithArguments(const ecs::systems::DialogueControlSystemInterface& dialogue,
                              std::optional<std::string_view> name, const std::function<void()>& pushArguments, const Vm& vm);
/// A script of the temple's help is started, with the types of a single player game; not in a multiplayer game
void StartTempleScript(std::string_view name, const Vm& vm);
/// The help scripts stop: Help, TempleHelp and MultiplayerHelp (the types 0x4A)
void StopHelp(const Vm& vm);
/// The player clicked the door of a room that is not open: the help scripts are stopped and the temple's
/// "CitadelWorldRoomNotAvailable" starts
void RoomNotAvailable(const Vm& vm);
/// false for first > last or when StopHelpScriptsForNewHelp refuses; else the message turn is set, first and last are
/// pushed as floats and the script started (the guidance spirits' "MultiHelpJustTalkWithText", whose two parameters
/// WhichTextFirst / WhichTextLast are these); true.
/// `townNeed`, a town need's text and how much the town wants it in percent, is pushed first, before either check, as
/// the Help key's answer about a town's need does before it asks for the message ("HelpTownNeeds" takes four
/// parameters); refused, the two values stay on the stack as they do in the original
bool RunMessage(HelpSystem& help, const ecs::systems::DialogueControlSystemInterface& dialogue, uint32_t first, uint32_t last,
                std::string_view script, const Vm& vm, uint32_t turn,
                std::optional<std::pair<uint32_t, float>> townNeed = std::nullopt);

/// The script camera's switches. A script reset sets drawHighlight = drawLeash = 1 and field7C = 0. The owners of the
/// camera and of the game speed are the script control's (ecs::systems::ScriptControlSystemInterface)
struct CameraControl
{
	/// The leashes are drawn: CHLApi SetDrawLeash (SET_DRAW_LEASH) and START/END_CAMERA_CONTROL write it, the leash
	/// draw reads it
	int32_t drawLeash {1};
	/// Only the camera release (= 0) and the script reset write it; no reader found
	int32_t field7C {0};
	/// The highlights are drawn (SET_DRAW_HIGHLIGHT; a did-you-know is drawn anyway): CHLApi SetDrawHighlight writes it,
	/// ecs::script_highlight::SyncMeshes and Draw read it
	int32_t drawHighlight {1};

	/// The camera part of the script reset
	void Reset();
};

/// The script camera state of the running game (one)
CameraControl& GetCameraControl();

/// CHL 120 START_DIALOGUE: the bool it pushes
bool StartDialogue(HelpSystem& help, ecs::systems::DialogueControlSystemInterface& dialogue, const Vm& vm);
/// CHL 121 END_DIALOGUE
void EndDialogue(HelpSystem& help, ecs::systems::DialogueControlSystemInterface& dialogue, audio::ScriptAudioState& audio,
                 const Vm& vm);
/// CHL 122 IS_DIALOGUE_READY: the dialogue is not controlled, by a task or by the wide screen of one (`wideScreenOwner`,
/// HelpSystem::GetWideScreenOwner)
[[nodiscard]] bool IsSpiritReady(const ecs::systems::DialogueControlSystemInterface& dialogue, uint32_t wideScreenOwner);
/// CHL 032 SET_WIDESCREEN, after the pop (`on` is the popped value as it is): true when it called
/// HelpSystem::SetWideScreen
bool SetWideScreen(HelpSystem& help, int32_t on, const Vm& vm);

/// CHL 030 START_CAMERA_CONTROL: the bool it pushes. `insideCitadel`: the player is in the citadel, where only
/// TempleHelp / TempleSpecial tasks get it; outside, the control makes the script camera mode from `world`'s zoomers,
/// which fails when the current camera mode cannot be left. Once taken outside, the leashes and highlights are hidden
bool StartCameraControl(CameraControl& camera, ecs::systems::ScriptControlSystemInterface& control, Camera& world, const Vm& vm,
                        bool insideCitadel);
/// CHL 031 END_CAMERA_CONTROL: when this task has the camera, the control gives it back and ReleaseCameraControl
/// follows. True when it released it
bool EndCameraControl(CameraControl& camera, ecs::systems::ScriptControlSystemInterface& control, Camera& world,
                      audio::ScriptAudioState& audio, const Vm& vm);
/// What follows the camera given back by the control: the switches and the SuperVillagers
void ReleaseCameraControl(CameraControl& camera, audio::ScriptAudioState& audio);

/// CHL 128 START_GAME_SPEED: when nobody or this task controls the game speed, the task takes it (the original also
/// turns on a script slow-motion flag of the camera that nothing else reads; not ported)
void StartGameSpeed(ecs::systems::ScriptControlSystemInterface& control, const Vm& vm);
/// CHL 129 END_GAME_SPEED: game speed 1.0 and no owner, when nobody or this task controls it
void EndGameSpeed(ecs::systems::ScriptControlSystemInterface& control, const Vm& vm);
/// CHL 066 SET_GAMESPEED, after the pop: sets the game speed only for the task that controls it (an owner of 0 matches
/// no running task)
void SetGameSpeed(const ecs::systems::ScriptControlSystemInterface& control, const Vm& vm, float speed);

/// The task-stop callback, for the parts ported: nothing without a game or HelpSystem; else the dialogue, the wide
/// screen (ReleaseWideScreenOf), the camera and the game speed are released and the hand demo playback of the task
/// ends (Input/HandDemo.h). The VM calls it before the task leaves the list, so the task's script type is still known
/// while releasing the dialogue, as LHVM::StopTask does (InvokeStopTaskCallback before the erase)
/// `dialogue` is nullptr without the dialogue control, `control` without the script control or `world` without the
/// game's camera: their part is then skipped, with an error
void OnTaskStopped(uint32_t task, HelpSystem* help, ecs::systems::DialogueControlSystemInterface* dialogue,
                   ecs::systems::ScriptControlSystemInterface* control, Camera* world, CameraControl& camera,
                   audio::ScriptAudioState& audio);

} // namespace openblack::help::script_control
