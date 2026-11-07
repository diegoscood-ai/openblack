/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What each answer to the start-of-game question skips, and the developers' start that answers it

#include <Story/NewGameChoice.h>
#include <gtest/gtest.h>

using openblack::new_game_choice::AsksAtNewGame;
using openblack::new_game_choice::Choice;
using openblack::new_game_choice::ClampChoice;
using openblack::new_game_choice::NewGameStart;
using openblack::new_game_choice::OpensOnBlack;
using openblack::new_game_choice::ParseNewGameStart;
using openblack::new_game_choice::SkipFor;
using openblack::new_game_choice::StartsTheStory;
using openblack::new_game_choice::TutorialSkip;

TEST(NewGameChoice, EachAnswerSkipsMoreThanTheOneBefore)
{
	EXPECT_EQ(SkipFor(Choice::StartNormally), TutorialSkip {});
	EXPECT_EQ(SkipFor(Choice::SkipToCreatureSelect), (TutorialSkip {.skipTutorial = true}));
	EXPECT_EQ(SkipFor(Choice::SkipCreatureTutorial), (TutorialSkip {.skipTutorial = true, .skipCreatureTraining = true}));
	EXPECT_EQ(SkipFor(Choice::KeepOldCreature),
	          (TutorialSkip {.skipTutorial = true, .skipCreatureTraining = true, .keepOldCreature = true}));
}

TEST(NewGameChoice, OnlyAReturningPlayerIsAsked)
{
	EXPECT_FALSE(AsksAtNewGame(0, false));
	EXPECT_FALSE(AsksAtNewGame(1, false));
	EXPECT_TRUE(AsksAtNewGame(1, true));
	EXPECT_TRUE(AsksAtNewGame(2, false));
	EXPECT_TRUE(AsksAtNewGame(0, true));
}

TEST(NewGameChoice, SelectionsAreBroughtIntoRange)
{
	EXPECT_EQ(ClampChoice(-1), Choice::StartNormally);
	EXPECT_EQ(ClampChoice(2), Choice::SkipCreatureTutorial);
	EXPECT_EQ(ClampChoice(7), Choice::KeepOldCreature);
}

// The story's first script starts only with its program read and on the first land of the story, as the original
// (docs/wiki/scripts/land-change-and-saving.md#the-programs-start)
TEST(NewGameChoice, TheStoryStartsOnlyWithItsProgramReadAndOnLand1)
{
	EXPECT_TRUE(StartsTheStory(true, 1));
	EXPECT_FALSE(StartsTheStory(false, 1));
	EXPECT_FALSE(StartsTheStory(true, 2));
	EXPECT_FALSE(StartsTheStory(true, 0));
	EXPECT_FALSE(StartsTheStory(false, 5));
}

// The black opening is only for a story that started while nothing held the game for the question
TEST(NewGameChoice, OnlyAStoryNotHeldForTheQuestionOpensOnBlack)
{
	EXPECT_TRUE(OpensOnBlack(true, 1, false));
	EXPECT_FALSE(OpensOnBlack(true, 1, true));
	EXPECT_FALSE(OpensOnBlack(false, 1, false));
	EXPECT_FALSE(OpensOnBlack(false, 1, true));
}

// A new game on Land 1 opens on black
TEST(NewGameChoice, Land1OpensOnBlack)
{
	EXPECT_TRUE(OpensOnBlack(true, 1, false));
}

// A new game on any other start land does not, even with the story started and nothing holding the game
TEST(NewGameChoice, AnotherStartLandDoesNotOpenOnBlack)
{
	for (const int32_t land : {0, 2, 3, 4, 5, 6})
	{
		EXPECT_FALSE(OpensOnBlack(true, land, false)) << "land " << land;
		EXPECT_FALSE(OpensOnBlack(true, land, true)) << "land " << land;
	}
}

// The developers' start answers the question with one of its own answers, or asks it
TEST(NewGameChoice, DevelopersStartNamesTheQuestionsAnswers)
{
	EXPECT_EQ(ParseNewGameStart("creature"), NewGameStart {.answer = Choice::SkipToCreatureSelect});
	EXPECT_EQ(ParseNewGameStart("story"), NewGameStart {.answer = Choice::SkipCreatureTutorial});
	EXPECT_EQ(ParseNewGameStart("old"), NewGameStart {.answer = Choice::KeepOldCreature});
	EXPECT_EQ(ParseNewGameStart("normal"), NewGameStart {.answer = Choice::StartNormally});
	EXPECT_EQ(ParseNewGameStart("ask"), NewGameStart {.ask = true});
	EXPECT_FALSE(ParseNewGameStart("beach").has_value());
	EXPECT_FALSE(ParseNewGameStart("").has_value());
	// Skipping to the creature select skips the opening only; the story's answer skips the creature's training too
	EXPECT_EQ(SkipFor(ParseNewGameStart("creature")->answer.value()),
	          (TutorialSkip {.skipTutorial = true, .skipCreatureTraining = false, .keepOldCreature = false}));
	EXPECT_EQ(SkipFor(ParseNewGameStart("story")->answer.value()),
	          (TutorialSkip {.skipTutorial = true, .skipCreatureTraining = true, .keepOldCreature = false}));
}
