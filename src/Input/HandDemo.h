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
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The hand demos of the tutorial (Data\HandDemo\*.hnd): GInterface's playback (StartPlayBack 0x5DAD60, fn_005DAEE0,
// EndPlayBack 0x5DB3F0) and the script's side (PLAY_HAND_DEMO 0x6FDAD0, IS_PLAYING_HAND_DEMO 0x6FDB80,
// HAND_DEMO_TRIGGER 0x6FE280). The recorded interface messages drive the real hand and the records set the camera.
// Research: dev\documentacion\hand\handdemo\README.md. Wiki: docs/bw1-notes/hand-and-interface.md, "Hand demos".
namespace openblack::hand_demo
{

/// What the playback gives the frame (fn_005DAEE0): the mouse (LHMouse::SetPosition 0x5DB081, normalised 0..1), the
/// buttons as the recorded messages left them, and the camera of the last record due (GCamera::SetPositionAndFocus
/// 0x5DB10D + LH3DTech::UpdateCamera 0x5DB120)
struct Frame
{
	glm::vec2 mouse {0.5f};
	bool grip {false};   ///< messages 1 GRAB_DOWN / 2 GRAB_UP
	bool action {false}; ///< messages 3 ACTION_DOWN / 4 ACTION_UP
	std::optional<glm::vec3> eye;
	std::optional<glm::vec3> focus;
};

/// GScript::PlayHandDemo 0x6FDAD0: StartPlayBack(".\Data\HandDemo\<name>.hnd", task, keepHand), then the script's
/// wait-for-trigger flag (+0x8C) = waitTrigger and its pending trigger (+0x88) = 0. False when the file cannot be read.
bool Play(std::string_view name, uint32_t task, bool waitTrigger, bool keepHand);
/// GInterface::IsPlayBack 0x5DB710: playing (+0x15C), and when `task` is not 0, by that task (+0x160)
[[nodiscard]] bool IsPlaying(uint32_t task = 0);
/// HAND_DEMO_TRIGGER 0x6FE280: the script's pending trigger (+0x88), which it clears
bool ConsumeTrigger();
/// GInterface::EndPlayBack 0x5DB3F0
void End();
/// The task stop callback 0x6EC72A..0x6EC748: EndPlayBack when the stopped task owns the demo
void EndIfTask(uint32_t task);
/// fn_005DAEE0, once a frame before the hand's ray (GInterface::Process 0x5CEC1F -> fn_005D9A20 0x5D9AB3): the records due
/// at the visual clock `nowMs` (g_game +0x25053C). Empty when no demo plays.
[[nodiscard]] std::optional<Frame> Update(uint32_t nowMs);

} // namespace openblack::hand_demo
