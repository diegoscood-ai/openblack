/*******************************************************************************
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

#include <algorithm>
#include <optional>
#include <string_view>

namespace openblack::new_game_choice
{

/// The four ways a returning player can start a new game, in the order the start-of-game question lists them
enum class Choice : uint8_t
{
	StartNormally,
	SkipToCreatureSelect,
	SkipCreatureTutorial,
	KeepOldCreature,
};

inline constexpr size_t k_ChoiceCount = 4;

/// What the story's scripts are told about the player's choice. The first land's set-up reads them through
/// CAN_SKIP_TUTORIAL, CAN_SKIP_CREATURE_TRAINING and IS_KEEPING_OLD_CREATURE.
struct TutorialSkip
{
	bool skipTutorial {false};
	bool skipCreatureTraining {false};
	bool keepOldCreature {false};

	[[nodiscard]] constexpr bool operator==(const TutorialSkip&) const = default;
};

/// Each choice skips everything the one before it skips, and one thing more
[[nodiscard]] constexpr TutorialSkip SkipFor(Choice choice) noexcept
{
	const auto level = static_cast<uint8_t>(choice);
	return {.skipTutorial = level >= 1, .skipCreatureTraining = level >= 2, .keepOldCreature = level >= 3};
}

/// The question is only put to a player who has played before: when there are at least two player profiles, or the
/// current profile already has a creature
[[nodiscard]] constexpr bool AsksAtNewGame(size_t profileCount, bool currentProfileHasCreature) noexcept
{
	return profileCount >= 2 || currentProfileHasCreature;
}

/// The only start land whose new game can open on black: the first land of the story
inline constexpr int32_t k_BlackOpeningLand = 1;

/// Whether a new game starts the story's first script: only once the story's program was read, and only on the
/// first land of the story (the number the land's script gives), as the original
[[nodiscard]] constexpr bool StartsTheStory(bool programLoaded, int32_t landNumber) noexcept
{
	return programLoaded && landNumber == k_BlackOpeningLand;
}

/// Whether a new game opens on a black screen, to be faded in by the story's scripts: only when the story's scripts
/// started, the start land (the number its script gives) is the first land of the story, and the start-of-game question
/// did not hold the game paused, as it does whenever it is asked
[[nodiscard]] constexpr bool OpensOnBlack(bool storyStarted, int32_t landNumber, bool heldForTheQuestion) noexcept
{
	return storyStarted && landNumber == k_BlackOpeningLand && !heldForTheQuestion;
}

/// A selection brought into the range of the choices
[[nodiscard]] constexpr Choice ClampChoice(int selection) noexcept
{
	return static_cast<Choice>(std::clamp(selection, 0, static_cast<int>(k_ChoiceCount) - 1));
}

/// How a developer or an agent starts a new game from the command line or the inspector (not in the original game,
/// which only asks a returning player): asked the start-of-game question whatever the profiles, or with the question
/// answered at once, the story then going exactly as it does after the player answers it
struct NewGameStart
{
	bool ask {false};
	std::optional<Choice> answer;

	[[nodiscard]] constexpr bool operator==(const NewGameStart&) const = default;
};

/// "ask", "normal", "creature" (straight to choosing a creature), "story" (skip all of the creature's tutorial too)
/// or "old" (keep the old creature); nothing for anything else
[[nodiscard]] constexpr std::optional<NewGameStart> ParseNewGameStart(std::string_view text) noexcept
{
	if (text == "ask")
	{
		return NewGameStart {.ask = true};
	}
	if (text == "normal")
	{
		return NewGameStart {.answer = Choice::StartNormally};
	}
	if (text == "creature")
	{
		return NewGameStart {.answer = Choice::SkipToCreatureSelect};
	}
	if (text == "story")
	{
		return NewGameStart {.answer = Choice::SkipCreatureTutorial};
	}
	if (text == "old")
	{
		return NewGameStart {.answer = Choice::KeepOldCreature};
	}
	return std::nullopt;
}

} // namespace openblack::new_game_choice
