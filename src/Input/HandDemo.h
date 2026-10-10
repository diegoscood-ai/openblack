/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The hand demos of the tutorial (Data\HandDemo\*.hnd): the interface's playback and the script's side
// (PLAY_HAND_DEMO, IS_PLAYING_HAND_DEMO, HAND_DEMO_TRIGGER). The recorded interface messages drive the real hand and
// the records set the camera.
// Wiki: docs/bw1-notes/hand-and-interface.md, "Hand demos".
namespace openblack::hand_demo
{

/// One 124-byte record of a .hnd file (no header)
struct Record
{
	uint32_t message; ///< interface message: 0 MOUSE_MOVE, 1 GRAB_DOWN, 2 GRAB_UP, 3 ACTION_DOWN, 4 ACTION_UP
	/// The hand's throw block when recorded: velocity, angular velocity,
	/// the hand position, angles
	std::array<float, 12> throwBlock {};
	glm::vec2 mouse;  ///< the mouse, normalised 0..1
	glm::vec3 eye;    ///< the camera position
	glm::vec3 focus;  ///< the camera focus
	uint32_t trigger; ///< the trigger key state when it changed since the last record, else 0 ((inferred) the space key)
	uint32_t timeMs;  ///< the visual clock when recorded
};
constexpr size_t k_RecordSize = 0x7C;

/// The whole records of a .hnd file's bytes, in order; a short last record is left out
[[nodiscard]] std::vector<Record> ParseRecords(std::span<const uint8_t> bytes);

/// What the playback gives the frame: the mouse (normalised 0..1), the buttons as the recorded messages left them, and
/// the camera of the last record due
struct Frame
{
	glm::vec2 mouse {0.5f};
	bool grip {false};   ///< messages 1 GRAB_DOWN / 2 GRAB_UP
	bool action {false}; ///< messages 3 ACTION_DOWN / 4 ACTION_UP
	std::optional<glm::vec3> eye;
	std::optional<glm::vec3> focus;
};

/// PLAY_HAND_DEMO: starts the playback of ".\Data\HandDemo\<name>.hnd" for `task`, then sets the script's
/// wait-for-trigger flag to waitTrigger and clears its pending trigger. False when the file cannot be read.
bool Play(std::string_view name, uint32_t task, bool waitTrigger, bool keepHand);
/// Whether a demo plays, and when `task` is not 0, whether that task started it
[[nodiscard]] bool IsPlaying(uint32_t task = 0);
/// (openblack's own) the demo now playing was started by the OPENBLACK_TEST_HAND_DEMO test hook, not by a script;
/// the next Play clears it
void MarkStartedByTestHook();
/// Whether the demo now playing was started by the test hook
[[nodiscard]] bool IsTestHookPlaying();
/// Whether the interface's box rule (Input/MouseButtons, BoxTakesInput) reaches the hand: always, but while a demo the
/// test hook started plays (openblack's own: the hook plays it from the land's start, under the SkipBox, which no
/// script of the original does)
[[nodiscard]] constexpr bool BoxRuleReachesHand(bool playing, bool startedByTestHook)
{
	return !(playing && startedByTestHook);
}
/// HAND_DEMO_TRIGGER: the script's pending trigger, which it clears
bool ConsumeTrigger();
/// Ends the playback
void End();
/// The task stop callback: ends the playback when the stopped task owns the demo
void EndIfTask(uint32_t task);
/// Once a frame before the hand's ray: the records due at the visual clock `nowMs`. Empty when no demo plays.
[[nodiscard]] std::optional<Frame> Update(uint32_t nowMs);

} // namespace openblack::hand_demo
