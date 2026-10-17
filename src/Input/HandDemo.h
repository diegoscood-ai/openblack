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
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The hand demos of the tutorial (Data\HandDemo\*.hnd): the interface's playback and the script's side
// (PLAY_HAND_DEMO, IS_PLAYING_HAND_DEMO, HAND_DEMO_TRIGGER). The recorded interface messages drive the real hand and
// the records set the camera.
// Wiki: docs/wiki/project/debugging.md#hand-demos.
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
	/// The camera hints shown when recorded: 1 tilting, 2 turning, 4 zooming
	uint32_t hints {0};
	float hintValue {0.0f}; ///< the hint's amount when recorded
	uint32_t trigger; ///< the trigger key state when it changed since the last record, else 0 ((inferred) the space key)
	uint32_t timeMs;  ///< the visual clock when recorded
	float objectDistance {0.0f}; ///< the distance of the object under the hand
	glm::ivec2 objectPlace {0};  ///< the object's place on the map (x, z)
	float objectAltitude {0.0f}; ///< the object's altitude
	uint32_t objectType {0};     ///< the object's script type
	uint32_t objectSubtype {0};  ///< the object's script subtype
};
constexpr size_t k_RecordSize = 0x7C;

/// The whole records of a .hnd file's bytes, in order; a short last record is left out
[[nodiscard]] std::vector<Record> ParseRecords(std::span<const uint8_t> bytes);
/// The records of ".\Data\HandDemo\<name>.hnd", read once into the resources (HandDemoManager, by the name's hash);
/// throws when the file cannot be read
[[nodiscard]] const std::vector<Record>& ReadRecords(std::string_view name);

/// Where a playback is in its recording
struct Playhead
{
	size_t read {0};          ///< the records read
	uint32_t firstTimeMs {0}; ///< the time of the first record
	uint32_t startMs {0};     ///< the visual clock at the start (moved on while a trigger holds the playback)
};

/// What one step through the records did
struct StepResult
{
	size_t reads {0};   ///< the records read, those put back for later included
	bool ended {false}; ///< the recording ran out
	/// The camera hints of the last record played, none when no record was played (the hints shown stay as they
	/// were). They are forced off while fewer than k_HintlessRecords records had been read at the step's first read,
	/// or fewer than k_HintlessRecords are left after them
	std::optional<uint32_t> hints;
	float hintValue {0.0f};
};
/// No hints are shown this near either end of a recording
constexpr size_t k_HintlessRecords = 20;

/// Plays the records due at the visual clock `nowMs`, in order from `playhead.read`, giving each to `play`. A record is
/// due when (its time - the first time) <= (now - the start), compared unsigned, so a clock gone back plays everything
/// at once. A record with a trigger sets `trigger`; while `waitTrigger` and `trigger` are both set, the next record is
/// put back and the start moved on (start = first time - time + now), so the rest keeps its pace once the script has
/// read the trigger. With `first`, the next record is played whatever its time and the playback starts from it: the
/// first time is its time (0 if it was not played) and the start is now. Pure.
StepResult StepRecords(std::span<const Record> records, Playhead& playhead, uint32_t nowMs, bool first, bool waitTrigger,
                       bool& trigger, const std::function<void(const Record&)>& play);

/// The screen pixel of a recorded mouse (normalised 0..1): across the whole width, and down the picture between the
/// cinema bars at `barFraction` (0 no bars .. 1 all the way in). Each bar is (H - 0.5625 W) barFraction / 2 pixels,
/// truncated, signed and not clamped: on a screen wider than 16:9 it is negative and the cursor can go past the top and
/// the bottom of the screen
[[nodiscard]] glm::ivec2 CursorPixel(glm::vec2 mouse, glm::ivec2 screen, float barFraction);

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

/// A demo's recorded hints (1 tilting, 2 turning, 4 zooming) as the camera's hints the hand reads (camera_drag::tricon)
[[nodiscard]] constexpr uint32_t HintsAsTricons(uint32_t hints)
{
	return ((hints & 2u) != 0 ? 0x01u : 0u) | ((hints & 1u) != 0 ? 0x02u : 0u) | ((hints & 4u) != 0 ? 0x08u : 0u);
}
/// The camera hints the demo now playing shows, none when no demo plays
[[nodiscard]] std::optional<uint32_t> PlayingHints();

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
/// (openblack's own) the demo a land run of a --scenarios list gives the test hook, in place of
/// OPENBLACK_TEST_HAND_DEMO's, for this game only (it is kept in the debug hooks' store, which a new game makes again)
void SetListedTestDemo(std::string_view name);
/// The demo the list gave the test hook for this game, none without one
[[nodiscard]] std::optional<std::string> ListedTestDemo();
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
