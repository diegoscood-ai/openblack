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

#include <optional>

/// GScript::SetInterfaceInteraction(SCRIPT_INTERFACE_LEVEL) 0x70B220 (runblack.exe W120; CHL 063
/// SET_INTERFACE_INTERACTION through 0x70B200): how much of the interface the player keeps while a tutorial script
/// teaches one gesture. Each level writes, through the jump table 0x70B7A8:
/// - GScript+0xA4 = the level (always, also for an invalid one);
/// - GInterface+0x28 (Input/InterfaceActive.h): = 0 (0, 6, 7: the interface is active again), |= 6 or |= 4;
/// - CameraHelp::EnableCameraFeatures(F, -1) (Camera/CameraHelp.h), and for 1, 2 and 10 the auto-pitch
///   fn_004473F0(0.448799, 15.0, 1);
/// - the hand's reach fn_0046BF20(R) on GInterface+0x3A0 (CHand +0x4838 = min(R, 1800)): the hand keeps it (owner
///   Mano, HandSystemInterface::SetHandReach), this only asks for it (LevelHandReach);
/// - the two ControlMap switches (g_game+0x250300)->+0x652C / +0x6530.
/// Research: dev\documentacion\intro\spec_misc_ops.md §063 and spec_demo_mode.md (the level table).
namespace openblack::help::interface_interaction
{

/// [0x8CBEAC]: the hand's largest reach (CHand ctor 0x46BC3E, fn_0046BF20's bound, SetDistanceFromView's 1800). The
/// hand (Mano) owns the reach and its bound; these are the values the levels pass
constexpr float k_MaxHandReach = 1800.0f;
/// JUST_GRAB's reach (0x70B30D, 0x42960000)
constexpr float k_JustGrabHandReach = 75.0f;
/// fn_004473F0's arguments for levels 1, 2 and 10 (0x3EE5C8FA, 0x41700000)
constexpr float k_TutorialAutoPitchParam1 = 0.448799f;
constexpr float k_TutorialAutoPitchParam2 = 15.0f;

/// 0x70B220. Above 15 (unsigned, `cmp eax, 0xF; ja`) and 9: "Unexpected interaction = %d" (0xC20648), only +0xA4 written
void Set(int32_t level);
/// GScript+0xA4. (pending) no reader found besides the save; 0 from the GScript constructor (inferred)
[[nodiscard]] int32_t GetLevel();

/// The R a level passes to fn_0046BF20 (CHand +0x4838 = R <= 1800 ? R : 1800, `test ah, 0x41`): 75 for JUST_GRAB
/// (0x70B30D), 1800 for the other levels that set it, nullopt for JUST_HAND_MOVE (8), JUST_HAND_INTERACTION (12) and
/// the invalid levels, which leave the reach as it is. Set hands it to HandSystemInterface::SetHandReach (Mano), whose
/// readers are the hand's: GInterface fn_005D1AB0 (5 calls through fn_0046BF50), CHand fn_0046DF60 (0x46E00F,
/// 0x46E123) and CHand::SetDistanceFromView 0x46C0D0 (0x46C0E4: the distance is clamped to [2, reach])
[[nodiscard]] std::optional<float> LevelHandReach(int32_t level);

/// ControlMap +0x652C (1 from the ControlMap ctor 0x46F719)
[[nodiscard]] bool GetControlSwitch1();
/// ControlMap +0x6530 (1 from the ControlMap ctor 0x46F724)
[[nodiscard]] bool GetControlSwitch2();
/// GGame::ProcessKey 0x63F42A..0x63F446: the LH_KEY 2..15 block (KB_1..KB_TAB: openblack's camera bookmark keys) needs
/// both switches
[[nodiscard]] bool KeyShortcutsEnabled();
/// fn_0046F750(action), from ControlMap::IsActionPerformed 0x470AF9, fn_00470A00 and fn_00470A60: true = the bindable
/// action is blocked. Switch 1 off: actions 3..19 but 5 (byte table 0x46F7A4); switch 1 on and switch 2 off: 17..19.
/// (inferred) with openblack's BindableActionMap bit order (Input/GameActionMapInterface.h) that is every camera
/// move but TALK, and ZOOM_TO_TEMPLE / ZOOM_TO_CREATURE / ZOOM_TO_REALM. (pending) GameActionMap does not ask it yet
[[nodiscard]] bool IsActionBlocked(int32_t action);

/// The executable's initial state: +0xA4 = 0, both switches 1 (GInterface+0x28, the camera features and the hand's
/// reach have their own Reset / initial values)
void Reset();

} // namespace openblack::help::interface_interaction
