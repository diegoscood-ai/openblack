/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Input/GameOverEscape.h"

using namespace openblack::input::game_over_escape;

namespace
{
/// The answer's steps, recorded in their order
class RecordedSteps final: public AnswerSteps
{
public:
	void StartNewGame() override { steps.emplace_back("new game"); }
	void Unpause() override { steps.emplace_back("unpause"); }

	std::vector<std::string> steps;
};
} // namespace

TEST(GameOverEscape, EscapeAsksOnlyOnceTheGameIsOverAndNoScriptHoldsTheWideScreen)
{
	EXPECT_FALSE(Asks({}));
	EXPECT_TRUE(Asks({.gameOver = true}));
	EXPECT_FALSE(Asks({.gameOver = true, .scriptWideScreen = true}));
	EXPECT_FALSE(Asks({.scriptWideScreen = true}));
	EXPECT_EQ(k_QuestionText, 3537u);
}

TEST(GameOverEscape, AYesStartsANewGameAndANoLetsTheGameGoOnUnlessTheTempleHoldsItPaused)
{
	RecordedSteps yes;
	Answer(true, false, yes);
	EXPECT_EQ(yes.steps, std::vector<std::string> {"new game"});

	RecordedSteps no;
	Answer(false, false, no);
	EXPECT_EQ(no.steps, std::vector<std::string> {"unpause"});

	RecordedSteps inTemple;
	Answer(false, true, inTemple);
	EXPECT_TRUE(inTemple.steps.empty());
}
