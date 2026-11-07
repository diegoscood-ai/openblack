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
#include <span>
#include <string>
#include <string_view>

#include "ECS/GameEndRules.h"
#include "Enums.h"

// The boxes that the end of a skirmish opens. The world gives the events of each turn (see game_end::ProcessTurn), and
// the boxes follow them for the player at this computer:
//
// - When the god of this computer is out, the end box tells the player that the game is lost. While two or more gods are
//   still in, the player can watch the game.
// - When another god is out and one god is left, the end box tells the player that the game is won. The game does not
//   look at whether the god of this computer is out (a vanilla bug, kept).
// - When another god is out and the game goes on, a box with OK tells the player that the god is out. It does not open
//   while the end box shows itself. The end box stops showing itself when it closes, and when the Statistics tab opens
//   its box over it.
//
// An end box closes the box with OK.
//
// Pure, tested with fakes.

namespace openblack::end_game_box
{

/// What the end box says
enum class Kind : uint8_t
{
	/// The god of this computer is out, and the game goes on
	Lost = 1,
	Won = 2,
	/// The god of this computer is out, and the game is decided
	LostAndDecided = 3,
};

/// An end box that opens
struct Box
{
	Kind kind;
	/// The player can watch the game: the box has Watch Game
	bool watch {false};
};

/// What the boxes of this computer keep between turns
struct State
{
	/// The end box shows itself: it opened, and it did not close or show another box over it since
	bool endBoxShowing {false};
};

/// The boxes that open in a turn. A later box of the turn takes the place of an earlier one.
struct Opened
{
	std::optional<Box> endBox;
	/// The box with OK that tells that this god is out
	std::optional<PlayerNames> godOut;
};

/// The boxes that the events of a turn open on the computer of the local god
[[nodiscard]] Opened Follow(std::span<const game_end::Event> events, PlayerNames local, State& state);

/// The end box closed, or another box shows over it
void StoppedShowing(State& state);

/// True when the player of this computer only looks on: players five to eight never win or lose
[[nodiscard]] constexpr bool LooksOn(PlayerNames local)
{
	return !game_end::CanLose(local);
}

/// The title of the end box. The texts are the game's: lost is "You have lost the game.", won is "Congratulations!
/// You've won the game!", gameOver is "Game Over" and winnerFormat is "%s - %s Won". A looker-on that the game tells
/// "won" sees "Game Over - <the winner> Won".
struct Titles
{
	std::u16string_view lost;
	std::u16string_view won;
	std::u16string_view gameOver;
	std::u16string_view winnerFormat;
};
[[nodiscard]] std::u16string Title(Kind kind, bool looksOn, std::u16string_view winner, const Titles& titles);

/// Puts each word in place of each %s of a text, in order
[[nodiscard]] std::u16string Format(std::u16string_view text, std::span<const std::u16string_view> words);

/// The name of a god in the boxes: the name of the human that plays it, or "Player[n]", where n is the number of the god
/// from 1
[[nodiscard]] std::u16string GodName(PlayerNames god, std::optional<std::u16string_view> humanName);

} // namespace openblack::end_game_box
