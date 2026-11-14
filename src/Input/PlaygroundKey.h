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

/// F2, the key that asks whether to go to the Gods' Playground, the tutorial island, and the trip there once the player
/// says yes. docs/wiki/project/progress/story/tutorial_island.md
namespace openblack::input::playground_key
{
/// The tutorial island's land number, where the key does nothing
inline constexpr int32_t k_PlaygroundLand = 6;
/// The help text the question shows: "Are you sure you want to go to the Gods' Playground?"
inline constexpr uint32_t k_QuestionText = 6653;
/// The island's land script, in the game's script folder, and its control script
inline constexpr std::string_view k_LandScript = "LandT.txt";
inline constexpr std::string_view k_ControlScript = "LandControlT";

/// What decides whether F2 asks the question
struct Gates
{
	/// A skirmish or playground game, rather than the story
	bool skirmish {false};
	bool networkGame {false};
	/// The player is inside the temple
	bool insideTemple {false};
	/// Another box takes the input (the start-up requester, a message, the question itself)
	bool boxUp {false};
	/// A script holds the wide screen
	bool scriptWideScreen {false};
	/// The land played
	int32_t land {0};
};

/// Whether F2 asks the question: only in a single-player story game, outside the temple, with no other box up, no
/// script's wide screen and not already on the island
[[nodiscard]] constexpr bool KeyAsks(const Gates& gates)
{
	return !gates.skirmish && !gates.networkGame && !gates.insideTemple && !gates.boxUp && gates.land != k_PlaygroundLand &&
	       !gates.scriptWideScreen;
}

/// What the trip does, in its order, as the game's services do it
class TripSteps
{
public:
	virtual ~TripSteps() = default;
	/// The game goes on: the pause the question put on is taken off
	virtual void Unpause() = 0;
	/// The player is taken out of the temple, if inside
	virtual void LeaveTemple() = 0;
	/// The game is saved into slot 20, as the game's instant save does, which the way back reloads
	virtual void Save() = 0;
	/// Every script task stops
	virtual void StopAllScripts() = 0;
	/// The land of a script in the game's script folder is loaded, as a script's land change loads it
	virtual void ChangeLand(std::string_view landScript) = 0;
	/// The island's lessons are taken as running, so that Escape stops the first one (input/PlaygroundEscape.h)
	virtual void LessonStarted() = 0;
	/// A script starts as a new task
	virtual void StartScript(std::string_view script) = 0;
};

/// The answer to the question: yes goes to the island, saving the game first and starting the island's control
/// script on it, its lesson taken as running; no only lets the game go on
inline void Answer(bool yes, TripSteps& steps)
{
	steps.Unpause();
	if (!yes)
	{
		return;
	}
	steps.LeaveTemple();
	steps.Save();
	steps.StopAllScripts();
	steps.ChangeLand(k_LandScript);
	steps.LessonStarted();
	steps.StartScript(k_ControlScript);
}
} // namespace openblack::input::playground_key
