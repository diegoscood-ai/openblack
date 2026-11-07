/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The land information box of a land's script, and the help texts of its lines

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <functional>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <Common/EventManager.h>
#include <ECS/Events/LandEvents.h>
#include <ECS/Systems/Implementations/ScriptState.h>
#include <Gui/Controls.h>
#include <Gui/GameInterface.h>
#include <Gui/LandInfoBox.h>
#include <Gui/TextDatabase.h>
#include <LHScriptX/Script.h>
#include <LandMessage.h>
#include <Locator.h>
#include <SDL_keycode.h>
#include <gtest/gtest.h>

#include "FakeGameFont.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::gui;
using namespace openblack::gui::test;

namespace
{
TextDatabase Texts()
{
	const std::vector<std::pair<std::u16string, std::u16string>> texts = {
	    {u"HELP_TEXT_DIALOG_ADDITION_62", u"AW"},
	    {u"HELP_TEXT_DIALOG_BACK", u"AB"},
	};
	std::u16string script;
	for (const auto& [name, text] : texts)
	{
		script.append(u"ADD_TEXT(1, N, \"").append(name).append(u"\", \"").append(text).append(u"\")\n");
	}
	TextDatabase database;
	database.AddScript(Utf16Script(script));
	return database;
}

struct BoxFixture
{
	/// Before the font, which makes its texture
	NoopRenderer renderer;
	TextDatabase texts = Texts();
	GameFont font = LoadFont();
	LandInfoBox box {texts, font};

	void Show(std::u16string title)
	{
		box.Show(std::move(title));
		// The box is now fully in view
		box.Update(1.0f);
	}

	bool Click(glm::ivec2 point)
	{
		box.MouseMove(point);
		box.MouseDown(point);
		return box.MouseUp(point);
	}

	[[nodiscard]] std::vector<std::u16string> Lines() const
	{
		std::vector<std::u16string> lines;
		std::ranges::transform(box.GetList().GetItems(), std::back_inserter(lines), &List::Item::text);
		return lines;
	}
};

constexpr glm::ivec2 k_Back = LandInfoBox::k_BackPosition + (LandInfoBox::k_ArrowSize / 2);
} // namespace

TEST(LandMessage, ANumberOfZeroShowsTheTextOfTheScriptLine)
{
	EXPECT_FALSE(land_message::HelpTextOf(0).has_value());
}

TEST(LandMessage, ThreeAndFourShowTheLinesForTwoAndThreeGods)
{
	EXPECT_EQ(land_message::HelpTextOf(3), "HELP_TEXT_LAST_LINES_02");
	EXPECT_EQ(land_message::HelpTextOf(4), "HELP_TEXT_LAST_LINES_03");
}

TEST(LandMessage, EveryOtherNumberShowsTheLineForOneGod)
{
	for (const auto number : {1, 2, 5, 8, -1})
	{
		EXPECT_EQ(land_message::HelpTextOf(number), "HELP_TEXT_LAST_LINES_01") << number;
	}
}

TEST(LandInfoBox, TheTitleShowsTheBoxWithAnEmptyList)
{
	BoxFixture fixture;
	fixture.box.AddLine(u"old");
	fixture.Show(u"Two Gods");
	EXPECT_TRUE(fixture.box.IsActive());
	EXPECT_EQ(fixture.box.GetTitle().text, u"Two Gods");
	EXPECT_TRUE(fixture.Lines().empty());
	fixture.box.AddLine(u"Battle another god");
	fixture.box.AddLine(u"second");
	EXPECT_EQ(fixture.Lines(), (std::vector<std::u16string> {u"Battle another god", u"second"}));
}

// A land's script gives its title first and then each line of its list (a playground's own lines, number 0): the
// script's commands send them as land messages, in their order, and the box takes the title and then each line at the
// end of its list, as the game's interface applies the land messages
TEST(LandInfoBox, TheScriptsAddedLinesFollowItsTitleInTheListInTheirOrder)
{
	const openblack::test::RestoreService<Locator::events> restoreEvents;
	const openblack::test::RestoreService<Locator::scriptState> restoreState;
	Locator::events::emplace<EventManager>();
	Locator::scriptState::emplace<ecs::systems::ScriptState>();
	BoxFixture fixture;
	std::vector<land_message::Message> given;
	Locator::events::value().AddHandler(std::function([&](const ecs::events::LandMessageGiven& message) {
		given.push_back(message.message);
		ApplyLandMessage(fixture.box, ToUtf16(message.message.text), message.message.kind == land_message::Message::Kind::Start,
		                 []() {});
	}));

	lhscriptx::Script script;
	script.Load("START_GAME_MESSAGE(\"1 - The Norse God\", 0)\n"
	            "ADD_GAME_MESSAGE_LINE(\"A easy to complete map\", 0)\n"
	            "ADD_GAME_MESSAGE_LINE(\"Difficulty level: VERY EASY\", 0)\n");

	using Kind = land_message::Message::Kind;
	EXPECT_EQ(given, (std::vector<land_message::Message> {
	                     {.kind = Kind::Start, .text = "1 - The Norse God", .number = 0},
	                     {.kind = Kind::AddLine, .text = "A easy to complete map", .number = 0},
	                     {.kind = Kind::AddLine, .text = "Difficulty level: VERY EASY", .number = 0},
	                 }));
	fixture.box.Update(1.0f);
	EXPECT_TRUE(fixture.box.IsActive());
	EXPECT_EQ(fixture.box.GetTitle().text, u"1 - The Norse God");
	EXPECT_EQ(fixture.Lines(), (std::vector<std::u16string> {u"A easy to complete map", u"Difficulty level: VERY EASY"}));
}

// The start message puts every dialog away before the box takes its title; an added line puts none away
TEST(LandInfoBox, TheStartMessagePutsEveryDialogAwayBeforeTheTitle)
{
	BoxFixture fixture;
	fixture.Show(u"old");
	fixture.box.AddLine(u"old line");
	std::vector<std::u16string> titlesWhenHidden;
	const auto hide = [&]() { titlesWhenHidden.push_back(fixture.box.GetTitle().text); };

	ApplyLandMessage(fixture.box, u"Two Gods", true, hide);
	EXPECT_EQ(titlesWhenHidden, (std::vector<std::u16string> {u"old"}));
	EXPECT_EQ(fixture.box.GetTitle().text, u"Two Gods");
	EXPECT_TRUE(fixture.Lines().empty());

	ApplyLandMessage(fixture.box, u"Battle another god", false, hide);
	EXPECT_EQ(titlesWhenHidden.size(), 1U);
	EXPECT_EQ(fixture.Lines(), (std::vector<std::u16string> {u"Battle another god"}));
}

// Every dialog put away: the box fades out over a fifth of a second as it does when it closes, with neither Go! nor Back
TEST(LandInfoBox, PuttingEveryDialogAwayClosesItWithNoChoice)
{
	BoxFixture fixture;
	fixture.Show(u"Two Gods");
	fixture.box.PutAway();
	EXPECT_FALSE(fixture.box.IsActive());
	EXPECT_EQ(fixture.box.TakeChoice(), LandInfoBox::Choice::None);
	EXPECT_TRUE(fixture.box.IsVisible());
	fixture.box.Update(0.25f);
	EXPECT_FALSE(fixture.box.IsVisible());
}

TEST(LandInfoBox, GoClosesTheBox)
{
	BoxFixture fixture;
	fixture.Show(u"Two Gods");
	EXPECT_TRUE(fixture.Click(LandInfoBox::k_Go.Centre()));
	EXPECT_FALSE(fixture.box.IsActive());
	EXPECT_EQ(fixture.box.TakeChoice(), LandInfoBox::Choice::Go);
	EXPECT_EQ(fixture.box.TakeChoice(), LandInfoBox::Choice::None);
	// It fades out over a fifth of a second
	EXPECT_TRUE(fixture.box.IsVisible());
	fixture.box.Update(0.25f);
	EXPECT_FALSE(fixture.box.IsVisible());
}

TEST(LandInfoBox, BackClosesTheBoxToLeaveTheLand)
{
	BoxFixture fixture;
	fixture.Show(u"Two Gods");
	EXPECT_TRUE(fixture.Click(k_Back));
	EXPECT_FALSE(fixture.box.IsActive());
	EXPECT_EQ(fixture.box.TakeChoice(), LandInfoBox::Choice::Back);
}

TEST(LandInfoBox, EscapeClosesTheBoxOnlyWithNoModifierKey)
{
	BoxFixture fixture;
	fixture.Show(u"Two Gods");
	fixture.box.KeyDown(SDLK_ESCAPE, KMOD_CTRL);
	EXPECT_TRUE(fixture.box.IsActive());
	fixture.box.KeyDown(SDLK_ESCAPE, KMOD_NONE);
	EXPECT_FALSE(fixture.box.IsActive());
	EXPECT_EQ(fixture.box.TakeChoice(), LandInfoBox::Choice::Go);
}
