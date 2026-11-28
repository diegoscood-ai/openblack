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

/// Escape once the local player's game is over: the game's requester asks its question, and a yes starts a new game
/// while a no lets the game go on. docs/wiki/project/progress/story/losing_and_game_over.md
namespace openblack::input::game_over_escape
{
/// The help text the requester asks
inline constexpr uint32_t k_QuestionText = 3537;

/// What decides whether Escape asks
struct Gates
{
	/// The local player's game is over
	bool gameOver {false};
	/// A script holds the wide screen
	bool scriptWideScreen {false};
};

/// Whether Escape asks the requester's question: once the game is over, unless a script holds the wide screen. When it
/// does not ask, the key goes on to the game's other uses of Escape
[[nodiscard]] constexpr bool Asks(const Gates& gates) noexcept
{
	return gates.gameOver && !gates.scriptWideScreen;
}

/// What the answer does, as the game's services do it
class AnswerSteps
{
public:
	virtual ~AnswerSteps() = default;
	/// A new game starts, as the setup's Start Game starts one
	virtual void StartNewGame() = 0;
	/// The game goes on again
	virtual void Unpause() = 0;
};

/// The answer: a yes starts a new game; a no lets the game go on, unless the temple holds the game paused
inline void Answer(bool yes, bool templePaused, AnswerSteps& steps)
{
	if (yes)
	{
		steps.StartNewGame();
		return;
	}
	if (!templePaused)
	{
		steps.Unpause();
	}
}

} // namespace openblack::input::game_over_escape
