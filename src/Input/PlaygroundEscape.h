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
/// restarting the island's scripts on its free play. docs/wiki/project/progress/story/tutorial_island.md
namespace openblack::input::playground_escape
{
/// The tutorial island's land number, where the island takes the key
inline constexpr int32_t k_PlaygroundLand = 6;
/// The developer function (DEV_FUNCTION) each of the island's lessons runs as it starts: Escape stops the lesson
inline constexpr int32_t k_LessonDevFunction = 5;
/// The island's free play, which the restart starts: the scripts that wait for the player to choose a lesson again
inline constexpr std::string_view k_FreePlayScript = "KeepRunningControl";

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
} // namespace openblack::input::playground_escape
