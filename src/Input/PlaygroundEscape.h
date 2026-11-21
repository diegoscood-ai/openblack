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

#include <string_view>

/// Escape on the Gods' Playground, the tutorial island: during one of the island's lessons it stops the lesson,
/// restarting the island's scripts on its free play; with none it asks whether to leave the island, and a yes loads the
/// game saved before the trip. docs/wiki/project/progress/story/tutorial_island.md
namespace openblack::input::playground_escape
{
/// The tutorial island's land number, where the island takes the key
inline constexpr int32_t k_PlaygroundLand = 6;
/// The developer function (DEV_FUNCTION) each of the island's lessons runs as it starts: Escape stops the lesson
inline constexpr int32_t k_LessonDevFunction = 5;
/// The island's free play, which the restart starts: the scripts that wait for the player to choose a lesson again
inline constexpr std::string_view k_FreePlayScript = "KeepRunningControl";

/// The help text the leaving question shows: "Are you sure you want to leave the Gods' Playground?"
inline constexpr uint32_t k_LeaveQuestionText = 6907;
/// The slot the way back loads: the game F2's yes saved before the trip to the island
inline constexpr uint32_t k_WayBackSlot = 20;

/// What decides what Escape does on the island
struct Gates
{
	/// The local player's game is over: the game's own question takes the key
	bool gameOver {false};
	bool networkGame {false};
	/// The land played
	int32_t land {0};
	/// A script holds the wide screen
	bool scriptWideScreen {false};
	/// The scripts' fade is still moving
	bool fadeMoving {false};
	/// The scripts' fade colour is drawn over the picture (its alpha is not 0)
	bool fadeDrawn {false};
};

/// What Escape does
enum class Outcome
{
	/// The island does not take the key: it goes on to the game's other uses of Escape
	NotTaken,
	/// The island takes the key and nothing happens
	Nothing,
	/// The lesson stops: the island's scripts restart on its free play at the end of the turn
	StopLesson,
	/// No lesson is running: the game asks whether to leave the Gods' Playground
	AskToLeave,
};

/// Escape on the island, in a single-player game that is not over: taken whatever happens next; nothing while a script
/// holds the wide screen or while the scripts' fade is moving or drawn; otherwise the lesson stops when one is running,
/// and the game asks whether to leave when none is
[[nodiscard]] constexpr Outcome OnEscape(const Gates& gates, bool lessonRunning)
{
	if (gates.gameOver || gates.networkGame || gates.land != k_PlaygroundLand)
	{
		return Outcome::NotTaken;
	}
	if (gates.scriptWideScreen || gates.fadeMoving || gates.fadeDrawn)
	{
		return Outcome::Nothing;
	}
	return lessonRunning ? Outcome::StopLesson : Outcome::AskToLeave;
}

/// What the restart does, in its order, as the game's services do it
class RestartSteps
{
public:
	virtual ~RestartSteps() = default;
	/// Every thing a script made goes, and every other thing a script held goes back into the game
	virtual void DeleteScriptThings() = 0;
	/// Every script task stops
	virtual void StopAllScripts() = 0;
	/// What the scripts set in the game is put back as it is with no script: the camera and the interface, the hand
	/// demo, the intro's things, the hidden sun, the sound confirmation and the script sound switches
	virtual void CleanForRestart() = 0;
	/// A script starts as a new task
	virtual void StartScript(std::string_view script) = 0;
	/// No lesson is running any more
	virtual void LessonStopped() = 0;
	/// The interface interaction back to normal
	virtual void InterfaceToNormal() = 0;
};

/// The restart of the island's scripts that stopping a lesson asks for, done at the end of the turn
inline void RestartOnFreePlay(RestartSteps& steps)
{
	steps.DeleteScriptThings();
	steps.StopAllScripts();
	steps.CleanForRestart();
	steps.StartScript(k_FreePlayScript);
	steps.LessonStopped();
	steps.InterfaceToNormal();
}
/// What the answer to the leaving question does, in its order, as the game's services do it
class LeaveSteps
{
public:
	virtual ~LeaveSteps() = default;
	/// The game goes on: the pause the question put on is taken off
	virtual void Unpause() = 0;
	/// The interface interaction back to normal
	virtual void InterfaceToNormal() = 0;
	/// A saved game's slot loads once this frame has ended, as the save game room's load does
	virtual void LoadSlot(uint32_t slot) = 0;
	/// The game's gathering flag on or off (input/PlaygroundKey.h)
	virtual void SetGatheringFlag(bool on) = 0;
	/// No lesson is running any more
	virtual void LessonStopped() = 0;
};

/// The answer to the leaving question: no only lets the game go on; yes puts the interface interaction back to normal,
/// turns the gathering flag the trip set off and loads the game saved before the trip, which the game starts again from, with
/// no lesson marked. The yes leaves the pause on: with no game in the slot nothing loads and the game stays paused
inline void AnswerLeave(bool yes, LeaveSteps& steps)
{
	if (!yes)
	{
		steps.Unpause();
		return;
	}
	steps.InterfaceToNormal();
	steps.SetGatheringFlag(false);
	steps.LoadSlot(k_WayBackSlot);
	steps.LessonStopped();
}
} // namespace openblack::input::playground_escape
