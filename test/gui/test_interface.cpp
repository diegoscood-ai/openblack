/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game's interface against a made up script and font: its texts, its font and the Escape menu

#include <cstdint>
#include <cstring>

#include <array>
#include <bit>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Gui/ButtonSound.h>
#include <Gui/GameFont.h>
#include <Gui/GameInterface.h>
#include <Gui/GameMenu.h>
#include <Gui/ListMouse.h>
#include <Gui/StatisticsBox.h>
#include <Gui/StoryHistoryBox.h>
#include <Gui/TattooEditorDialog.h>
#include <Gui/TextDatabase.h>
#include <SDL_keycode.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Common/PlayerProfiles.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "FakeGameFont.h"
#include "Help/DialogueText.h"
#include "Help/HelpSystem.h"
#include "Help/PlayerChoices.h"
#include "Help/ToolTips.h"
#include "Resources/ResourcesInterface.h"
#include "support/BgfxShutdown.h"

using namespace openblack::gui;
using namespace openblack::gui::test;
using openblack::resources::GameFontLoader;

namespace
{
std::u16string ToString(const std::vector<std::u16string_view>& lines)
{
	std::u16string result;
	for (const auto& line : lines)
	{
		result += u"[";
		result += line;
		result += u"]";
	}
	return result;
}
} // namespace

TEST(TextDatabase, ReadsTheAddTextLines)
{
	TextDatabase texts;
	const auto added =
	    texts.AddScript(Utf16Script(u"// A comment ADD_TEXT\r\n"
	                                u"ADD_TEXT( 1, HELP_TEXT_NARRATOR_NONE, \"FIRST\", \"Continue Game\")\r\n"
	                                u"ADD_TEXT(1,HELP_TEXT_NARRATOR_NONE,\"SECOND\",\"Line one\\nline \\\"two\\\"\")\r\n"
	                                u"ADD_TEXT( 1, HELP_TEXT_NARRATOR_NONE, \"BROKEN\")\r\n"
	                                u"MY_ADD_TEXT( 1, NONE, \"NOT\", \"read\")\r\n"));
	EXPECT_EQ(added, 2);
	EXPECT_EQ(texts.Get("FIRST"), u"Continue Game");
	EXPECT_EQ(texts.Get("SECOND"), u"Line one\nline \"two\"");
	EXPECT_TRUE(texts.Get("BROKEN").empty());
	EXPECT_TRUE(texts.Get("NOT").empty());
	EXPECT_TRUE(texts.Get("MISSING").empty());

	// A later script, with no byte order mark, replaces a text of the same name
	texts.AddScript(Utf16Script(u"ADD_TEXT(1, NONE, \"FIRST\", \"Patched\")", false));
	EXPECT_EQ(texts.Get("FIRST"), u"Patched");
	EXPECT_EQ(texts.GetCount(), 2);
}

TEST(TextDatabase, NumbersTheHelpTextsInTheirOrder)
{
	TextDatabase texts;
	texts.AddHelpScript(Utf16Script(u"ADD_TEXT( 0, HELP_TEXT_NARRATOR_NONE, \"NONE\", \"Invalid\")\r\n"
	                                u"ADD_TEXT( 1, HELP_TEXT_NARRATOR_MAN, \"FIRST\", \"Hello\")\r\n"
	                                u"ADD_TEXT( 1, HELP_TEXT_NARRATOR_MAN, \"SECOND\", \"Over a line\r\nbreak\")\r\n"));
	texts.AddScript(Utf16Script(u"ADD_TEXT(1, NONE, \"PATCH\", \"Not numbered\")", false));
	const auto names = texts.GetHelpNames();
	ASSERT_EQ(names.size(), 3);
	EXPECT_EQ(names[0], "NONE");
	EXPECT_EQ(names[1], "FIRST");
	EXPECT_EQ(names[2], "SECOND");
	// A text whose closing quote is on the next line has its number, but no text by number or by name
	EXPECT_TRUE(texts.GetHelpText(2).text.empty());
	EXPECT_TRUE(texts.Get("SECOND").empty());
	EXPECT_EQ(texts.Get("FIRST"), u"Hello");
	EXPECT_EQ(texts.Get("PATCH"), u"Not numbered");
}

TEST(TextDatabase, KeepsTheHelpTextsNarratorsAndCodes)
{
	TextDatabase texts;
	texts.AddHelpScript(Utf16Script(u"SLONG    HELP_TEXT_NARRATOR_NONE\r\n"
	                                u"HELP_TEXT_NARRATOR_NONE =  0\r\n"
	                                u"HELP_TEXT_NARRATOR_EVIL_SPIRIT =  3\r\n"
	                                u"ADD_TEXT( 0, HELP_TEXT_NARRATOR_NONE, \"NONE\", \"Invalid\")\r\n"
	                                u"ADD_TEXT( 1, HELP_TEXT_NARRATOR_EVIL_SPIRIT, \"EVIL\", \"Click. $m2\")\r\n"
	                                u"ADD_TEXT( 0, 7, \"BREAK\", \"One\\nTwo\")\r\n"));
	ASSERT_EQ(texts.GetHelpTextCount(), 3);
	EXPECT_EQ(texts.GetHelpText(1).narrator, 3);
	EXPECT_TRUE(texts.GetHelpText(1).important);
	EXPECT_EQ(texts.GetHelpText(1).text, u"Click. $m2");
	EXPECT_EQ(texts.GetHelpText(2).narrator, 7);
	EXPECT_FALSE(texts.GetHelpText(2).important);
	// The help texts are converted as the game loads them: a backslash and n is a line break, by number as by name
	EXPECT_EQ(texts.GetHelpText(2).text, u"One\nTwo");
	EXPECT_EQ(texts.Get("BREAK"), u"One\nTwo");
	// Past the last, the first
	EXPECT_EQ(texts.GetHelpText(3).text, u"Invalid");
	EXPECT_EQ(texts.GetHelpText(0).narrator, 0);
}

TEST(TextDatabase, ConvertsBetweenUtf8AndUtf16)
{
	const std::u16string text = u"Bläck & Wh€te \U0001F600";
	EXPECT_EQ(ToUtf16(ToUtf8(text)), text);
	EXPECT_EQ(ToUtf8(u"Black & White"), "Black & White");
}

TEST(GameFont, DecodesTheGlyphsAtHalfSize)
{
	const NoopRenderer renderer;
	const auto font = LoadFont();
	EXPECT_EQ(font.GetHeight(), 80);
	EXPECT_EQ(font.GetAtlasSize(), glm::u16vec2(1024, 64));

	// Each glyph's place in the atlas is half its width, rounded up, and half the font's height
	const auto size = [&font](char16_t c) {
		const auto* glyph = font.Find(c);
		return glm::ivec2(glyph->atlasMax) - glm::ivec2(glyph->atlasMin);
	};
	EXPECT_EQ(size(u'A'), glm::ivec2(1, 40));
	EXPECT_EQ(size(u'B'), glm::ivec2(1, 40));
	EXPECT_EQ(size(u'?'), glm::ivec2(1, 40));
	EXPECT_EQ(size(u'W'), glm::ivec2(2, 40));
	EXPECT_EQ(size(u' ').x, 0);
}

TEST(GameFont, FallsBackToTheQuestionMark)
{
	const NoopRenderer renderer;
	const auto font = LoadFont();
	EXPECT_EQ(font.Find(u'Z'), font.Find(u'?'));
	EXPECT_EQ(font.Find(u'A')->character, u'A');
}

TEST(GameFont, MeasuresInLinesOfASize)
{
	const NoopRenderer renderer;
	const auto font = LoadFont();
	// The pen moves left + ink + right, scaled from the font's height to the size
	EXPECT_FLOAT_EQ(font.GetWidth(u"A", 4.0f), 4.0f);
	EXPECT_FLOAT_EQ(font.GetWidth(u"AB", 8.0f), 12.0f);
	EXPECT_FLOAT_EQ(font.GetWidth(u"A A", 4.0f), 10.0f);
	EXPECT_FLOAT_EQ(font.GetWidth(u"A\nA", 4.0f), 8.0f);
}

TEST(GameFont, WrapsAtSpacesHyphensAndLineBreaks)
{
	const NoopRenderer renderer;
	const auto font = LoadFont();
	// At size 4: A and B 4 and 2 wide, a space 2, a hyphen 2, W 8
	EXPECT_EQ(ToString(font.Wrap(u"AA AA", 4.0f, 10.0f)), u"[AA][AA]");
	EXPECT_EQ(ToString(font.Wrap(u"AA AA", 4.0f, 18.0f)), u"[AA AA]");
	EXPECT_EQ(ToString(font.Wrap(u"AA-BB", 4.0f, 11.0f)), u"[AA-][BB]");
	EXPECT_EQ(ToString(font.Wrap(u"A\nB", 4.0f, 100.0f)), u"[A][B]");
	EXPECT_EQ(ToString(font.Wrap(u"A\r\nB", 4.0f, 100.0f)), u"[A][B]");
	// A word too long for a line breaks where it reaches the end
	EXPECT_EQ(ToString(font.Wrap(u"WWW", 4.0f, 17.0f)), u"[WW][W]");
}

TEST(GameFont, RejectsFilesThatDontFit)
{
	MakeGraphicsLog();
	auto fake = MakeFont();
	const std::vector<uint8_t> shortMet(fake.met.begin(), fake.met.begin() + 100);
	EXPECT_THROW((void)GameFontLoader {}(GameFontLoader::FromBufferTag {}, shortMet, fake.fnt, "fake"), std::runtime_error);
	EXPECT_FALSE(GameFont::From(nullptr).has_value());
}

namespace
{
TextDatabase MenuTexts()
{
	std::u16string script;
	const std::vector<std::pair<std::u16string, std::u16string>> texts = {
	    {u"HELP_TEXT_DIALOG_WELCOME", u"Welcome %s"},
	    {u"HELP_TEXT_DIALOG_CONTINUEGAME", u"Continue"},
	    {u"HELP_TEXT_DIALOG_ADDITION_169", u"Skirmish"},
	    {u"HELP_TEXT_DIALOG_JOINGONLINE", u"Online"},
	    {u"HELP_TEXT_DIALOG_OPTIONS", u"Options"},
	    {u"HELP_TEXT_DIALOG_QUIT", u"Quit"},
	    {u"HELP_TEXT_DIALOG_ADDITION_88", u"Main"},
	    // Inside the temple the first tab reads the World Room's title; a tab with no text cannot be clicked
	    {u"HELP_TEXT_ROOM_WORLD_TITLE", u"World Room"},
	    {u"HELP_TEXT_PAUSE_STATS_151", u"Stats"},
	    {u"HELP_TEXT_DIALOG_PLAYERLIST", u"Players"},
	    {u"HELP_TEXT_FRONT_END_06", u"Advanced"},
	    {u"HELP_TEXT_DIALOG_CONTROLOPTIONS", u"Controls"},
	    {u"HELP_TEXT_DIALOG_QUIT_QUESTION", u"Sure?"},
	    {u"HELP_TEXT_DIALOG_AREYOUSUREQUIT", u"Quit game?"},
	    {u"HELP_TEXT_REQUESTER_BOXES_01", u"Yes."},
	    {u"HELP_TEXT_REQUESTER_BOXES_02", u"No."},
	    {u"HELP_TEXT_DIALOG_BACK", u"Back"},
	    {u"HELP_TEXT_DIALOG_DETAIL_00", u"Minimum"},
	    {u"HELP_TEXT_DIALOG_DETAIL_04", u"Maximum"},
	    {u"HELP_TEXT_ACTIONS_HELP", u"Help"},
	    {u"HELP_TEXT_ACTIONS_SELECT", u"Move"},
	    {u"HELP_TEXT_ACTIONS_LEFTMOUSEBUTTON", u"LMB"},
	    {u"HELP_TEXT_ACTIONS_ZOOMON", u"Zoom On"},
	    {u"HELP_TEXT_REQUESTER_BOXES_04", u"OK"},
	    {u"HELP_TEXT_FRONT_END_12", u"Restart"},
	    {u"HELP_TEXT_DIALOG_RESTART_WARNING", u"Restart?"},
	    {u"HELP_TEXT_DIALOG_RESTART", u"Restarting now."},
	    {u"HELP_TEXT_DIALOG_AERYOUSUREDELETE", u"Delete player %s?"},
	    {u"HELP_TEXT_DIALOG_ADDITION_20", u"New game?"},
	    {u"HELP_TEXT_DIALOG_LOADDEFAULTS", u"Load Defaults"},
	};
	for (const auto& [name, text] : texts)
	{
		script.append(u"ADD_TEXT(1, N, \"").append(name).append(u"\", \"").append(text).append(u"\")\n");
	}
	TextDatabase database;
	database.AddScript(Utf16Script(script));
	return database;
}

using Action = GameMenu::Action;
using Page = GameMenu::Page;

/// Presses and lets go of the mouse button at a point
Action Click(GameMenu& menu, glm::ivec2 point)
{
	menu.MouseMove(point);
	menu.MouseDown(point);
	return menu.MouseUp(point);
}

glm::ivec2 ButtonCentre(size_t index)
{
	return GameMenu::GetButtonRect(index).Centre();
}

glm::ivec2 TabCentre(size_t index)
{
	return Dialog::GetTabRect(index).Centre();
}

struct MenuFixture
{
	NoopRenderer renderer;
	TextDatabase texts = MenuTexts();
	GameFont font = LoadFont();
	GameMenu menu {texts, font, u"aaa", MenuSettings {}};

	MenuFixture() { menu.Open(); }
};

// The arrows and squares are hit in their middle
constexpr glm::ivec2 k_Back {50, 550};
constexpr glm::ivec2 k_OptionsQuit {750, 550};
// The video change notice's OK
constexpr glm::ivec2 k_NoticeOk {400, 354};
} // namespace

TEST(GameMenu, AModsLineNamesTheModItsVersionAndItsAuthor)
{
	EXPECT_EQ(MenuModLabel(u"Greener Grass", u"1.2", u"Someone"), u"Greener Grass 1.2 (Someone)");
	EXPECT_EQ(MenuModLabel(u"Greener Grass", u"", u""), u"Greener Grass");
	EXPECT_EQ(MenuModLabel(u"Greener Grass", u"", u"Someone"), u"Greener Grass (Someone)");
}

TEST(GameMenu, WithoutTheModsTheOptionsKeepTheOriginalsFiveTabs)
{
	MenuFixture f;
	EXPECT_FALSE(f.menu.HasModsPage());
	EXPECT_TRUE(f.menu.GetMods().empty());
	Click(f.menu, ButtonCentre(3));
	// the fifth tab is still the Controls one, at the original's place
	Click(f.menu, TabCentre(4));
	EXPECT_EQ(f.menu.GetPage(), Page::Controls);
}

TEST(GameMenu, GivenTheModsASixthTabListsThemWithASwitchEach)
{
	NoopRenderer renderer;
	const TextDatabase texts = MenuTexts();
	const GameFont font = LoadFont();
	std::vector<std::pair<size_t, bool>> asked;
	MenuMods mods {.heading = u"Mod loader 2: 2 mods",
	               .mods = {{.label = u"Alpha", .enabled = false}, {.label = u"Beta 1.0 (B)", .enabled = false}},
	               .onSwitch = [&asked](size_t index, bool on) {
		               asked.emplace_back(index, on);
		               return index == 0; // the second cannot be kept
	               }};
	GameMenu menu {texts, font, u"aaa", MenuSettings {}, std::move(mods)};
	menu.Open();
	EXPECT_TRUE(menu.HasModsPage());
	Click(menu, ButtonCentre(3));
	EXPECT_EQ(menu.GetPage(), Page::Options);
	// six tabs of 130 across the box from x 10: the Mods tab is the last, from 660 to 790
	Click(menu, {725, 30});
	EXPECT_EQ(menu.GetPage(), Page::Mods);
	// the first mod's square at 60, 120 and the second's 48 lower
	Click(menu, {72, 132});
	Click(menu, {72, 180});
	EXPECT_EQ(asked, (std::vector<std::pair<size_t, bool>> {{0, true}, {1, true}}));
	EXPECT_TRUE(menu.GetMods().at(0).enabled);
	EXPECT_FALSE(menu.GetMods().at(1).enabled);
	// the other tabs share the width too, and Back goes to the first page
	Click(menu, {75 + 130, 30});
	EXPECT_EQ(menu.GetPage(), Page::Options);
	Click(menu, {725, 30});
	Click(menu, k_Back);
	EXPECT_EQ(menu.GetPage(), Page::Main);
}

TEST(GameMenu, IsLaidOutAsTheOriginal)
{
	// The main menu's 440 by 70 buttons 80 apart, from 145 down
	EXPECT_EQ(GameMenu::GetButtonRect(0).min, glm::ivec2(180, 145));
	EXPECT_EQ(GameMenu::GetButtonRect(0).max, glm::ivec2(620, 215));
	EXPECT_EQ(GameMenu::GetButtonRect(4).min, glm::ivec2(180, 465));
	// The main menu's tabs: a fifth of the box's width each, along its top
	EXPECT_EQ(Dialog::GetTabRect(0).min, glm::ivec2(10, 10));
	EXPECT_EQ(Dialog::GetTabRect(0).max, glm::ivec2(166, 50));
	EXPECT_EQ(Dialog::GetTabRect(4).max, glm::ivec2(790, 50));
}

TEST(GameMenu, GreetsThePlayer)
{
	const MenuFixture f;
	EXPECT_EQ(f.menu.GetWelcome(), u"Welcome aaa");
	EXPECT_EQ(f.menu.GetButtonLabel(0), u"Continue");
	EXPECT_EQ(f.menu.GetButtonLabel(4), u"Quit");
	// The player's creature takes their name until they give it another
	EXPECT_EQ(f.menu.GetSettings().creatureName, u"aaa");
}

TEST(GameMenu, FadesInAndOut)
{
	const NoopRenderer renderer;
	const auto texts = MenuTexts();
	const auto font = LoadFont();
	GameMenu menu(texts, font, u"aaa", MenuSettings {});
	EXPECT_FALSE(menu.IsVisible());
	menu.Open();
	EXPECT_TRUE(menu.IsOpen());
	menu.Update(0.6f);
	menu.Close();
	EXPECT_FALSE(menu.IsOpen());
	EXPECT_TRUE(menu.IsVisible());
	menu.Update(0.1f);
	EXPECT_TRUE(menu.IsVisible());
	menu.Update(0.11f);
	EXPECT_FALSE(menu.IsVisible());
	// A page shown over the first and the menu closed at once: the page fades out as the menu did, the first gone
	menu.Open();
	menu.Update(0.6f);
	menu.ShowPage(GameMenu::Page::Options, GameMenu::PageSwitch::CrossFade);
	menu.Close();
	menu.Update(0.1f);
	EXPECT_TRUE(menu.IsVisible());
	menu.Update(0.11f);
	EXPECT_FALSE(menu.IsVisible());
}

TEST(StatisticsBoxLayout, ABoxWithTabsIsFortyDeeperBelowTheMiddle)
{
	// 780 by 500: from 250 above the middle of the dialog space to 290 below it
	EXPECT_EQ(TabbedBox({780, 500}).min, glm::ivec2(10, 50));
	EXPECT_EQ(TabbedBox({780, 500}).max, glm::ivec2(790, 590));
	EXPECT_EQ(StatisticsBox::k_Box.min, Dialog::k_Box.min);
	EXPECT_EQ(StatisticsBox::k_Box.max, Dialog::k_Box.max);
}

TEST(GameMenu, ButtonsActWhenLetGoOverTheOneTheyWentDownOn)
{
	MenuFixture f;
	EXPECT_EQ(Click(f.menu, ButtonCentre(0)), Action::Continue);
	EXPECT_EQ(Click(f.menu, ButtonCentre(1)), Action::StartSkirmish);
	EXPECT_EQ(Click(f.menu, ButtonCentre(2)), Action::JoinOnline);
	// The Statistics tab opens the Game Statistics box over the menu, and its first tab returns to the menu
	EXPECT_EQ(Click(f.menu, TabCentre(1)), Action::None);
	EXPECT_EQ(f.menu.GetPage(), Page::Statistics);
	EXPECT_EQ(Click(f.menu, {75, 30}), Action::None);
	EXPECT_EQ(f.menu.GetPage(), Page::Main);
	// Tabs without a label take no clicks
	EXPECT_EQ(Click(f.menu, TabCentre(3)), Action::None);
	// Between the buttons
	EXPECT_EQ(Click(f.menu, {400, 220}), Action::None);

	// Let go over another button
	f.menu.MouseDown(ButtonCentre(0));
	f.menu.MouseMove(ButtonCentre(2));
	EXPECT_EQ(f.menu.MouseUp(ButtonCentre(2)), Action::None);

	// Closed, it takes no clicks
	f.menu.Close();
	EXPECT_EQ(Click(f.menu, ButtonCentre(0)), Action::None);
}

TEST(GameMenu, InsideTheTempleTheFirstTabClosesTheOptionsWithoutTheMainMenu)
{
	MenuFixture f;
	f.menu.SetInsideTemple(true);
	f.menu.ShowPage(Page::Options, GameMenu::PageSwitch::Replace);
	ASSERT_EQ(f.menu.GetPage(), Page::Options);
	// The first tab (the World Room's, inside) has no page of its own: the dialogs go and no main menu shows
	Click(f.menu, TabCentre(0));
	EXPECT_FALSE(f.menu.IsOpen());
}

TEST(GameMenu, OptionsOpenTheOptionsTabs)
{
	MenuFixture f;
	EXPECT_EQ(Click(f.menu, ButtonCentre(3)), Action::None);
	EXPECT_EQ(f.menu.GetPage(), Page::Options);
	Click(f.menu, TabCentre(2));
	EXPECT_EQ(f.menu.GetPage(), Page::Players);
	Click(f.menu, TabCentre(3));
	EXPECT_EQ(f.menu.GetPage(), Page::Advanced);
	Click(f.menu, TabCentre(4));
	EXPECT_EQ(f.menu.GetPage(), Page::Controls);
	Click(f.menu, TabCentre(1));
	EXPECT_EQ(f.menu.GetPage(), Page::Options);
	// Back, or the Main Menu tab, goes back to the first page
	Click(f.menu, k_Back);
	EXPECT_EQ(f.menu.GetPage(), Page::Main);
	Click(f.menu, ButtonCentre(3));
	Click(f.menu, TabCentre(0));
	EXPECT_EQ(f.menu.GetPage(), Page::Main);
	// Opening the menu again starts on the first page
	Click(f.menu, ButtonCentre(3));
	f.menu.Close();
	f.menu.Open();
	EXPECT_EQ(f.menu.GetPage(), Page::Main);
}

TEST(GameMenu, QuittingAsksFirst)
{
	MenuFixture f;
	EXPECT_EQ(Click(f.menu, ButtonCentre(4)), Action::None);
	EXPECT_TRUE(f.menu.IsAskingToQuit());
	// The menu's buttons are behind the question
	EXPECT_EQ(Click(f.menu, ButtonCentre(0)), Action::None);

	// Escape over the first page's question is the page's Continue: the question goes and the game goes on
	EXPECT_EQ(f.menu.Escape(), Action::Continue);
	EXPECT_FALSE(f.menu.IsAskingToQuit());
	EXPECT_TRUE(f.menu.IsOpen());

	// No backs out too, on the arrow or its label to its left
	Click(f.menu, ButtonCentre(4));
	EXPECT_EQ(Click(f.menu, {584, 354}), Action::None);
	EXPECT_FALSE(f.menu.IsAskingToQuit());
	Click(f.menu, ButtonCentre(4));
	EXPECT_EQ(Click(f.menu, {560, 354}), Action::None);
	EXPECT_FALSE(f.menu.IsAskingToQuit());

	// Yes quits, on its label to its right too
	Click(f.menu, ButtonCentre(4));
	EXPECT_EQ(Click(f.menu, {240, 354}), Action::Quit);
	EXPECT_FALSE(f.menu.IsAskingToQuit());
	Click(f.menu, ButtonCentre(4));
	EXPECT_EQ(Click(f.menu, {216, 354}), Action::Quit);

	// The options' Quit asks too
	Click(f.menu, ButtonCentre(3));
	EXPECT_EQ(Click(f.menu, k_OptionsQuit), Action::None);
	EXPECT_TRUE(f.menu.IsAskingToQuit());
	EXPECT_EQ(Click(f.menu, {216, 354}), Action::Quit);
}

TEST(GameMenu, AnAnsweredQuestionGivesTheBoxBackWhole)
{
	// Asked as the menu has only begun to fade in: its No puts the menu at once at its full alpha
	MenuFixture f;
	Click(f.menu, ButtonCentre(4));
	ASSERT_TRUE(f.menu.IsAskingToQuit());
	EXPECT_LT(f.menu.GetFades().Shown()->second, 1.0f);
	EXPECT_EQ(Click(f.menu, {584, 354}), Action::None);
	EXPECT_FALSE(f.menu.IsAskingToQuit());
	EXPECT_EQ(f.menu.GetFades().Shown()->second, 1.0f);
}

TEST(GameMenu, TheTabOfThePageShownShowsItAgainFromNothing)
{
	// a tab hides every box and shows its page, its own page's too: the page fades in again from nothing
	MenuFixture f;
	Click(f.menu, ButtonCentre(3));
	Click(f.menu, TabCentre(3));
	ASSERT_EQ(f.menu.GetPage(), Page::Advanced);
	f.menu.Update(0.6f);
	ASSERT_EQ(f.menu.GetFades().Shown()->second, 1.0f);
	Click(f.menu, TabCentre(3));
	EXPECT_EQ(f.menu.GetPage(), Page::Advanced);
	EXPECT_EQ(f.menu.GetFades().Shown()->second, 0.0f);
	EXPECT_FALSE(f.menu.GetFades().Leaving().has_value());
}

TEST(GameMenu, EscapeBacksOut)
{
	MenuFixture f;
	Click(f.menu, ButtonCentre(3));
	EXPECT_EQ(f.menu.Escape(), Action::None);
	EXPECT_EQ(f.menu.GetPage(), Page::Main);
	// The original's first page refuses Escape (its CanESCOut is false), so the key goes to the page, which does
	// nothing with it: the menu stays open on the first page
	EXPECT_EQ(f.menu.Escape(), Action::None);
	EXPECT_TRUE(f.menu.IsOpen());
	EXPECT_EQ(f.menu.GetPage(), Page::Main);
	f.menu.Close();
	EXPECT_EQ(f.menu.Escape(), Action::None);
}

TEST(GameMenu, ClicksOnEveryControlThatActs)
{
	MenuFixture f;
	EXPECT_FALSE(f.menu.TakeClicked());
	Click(f.menu, ButtonCentre(2));
	EXPECT_TRUE(f.menu.TakeClicked());
	EXPECT_FALSE(f.menu.TakeClicked());
	// Opening the question and answering it click too
	Click(f.menu, ButtonCentre(4));
	EXPECT_TRUE(f.menu.TakeClicked());
	Click(f.menu, {584, 354});
	EXPECT_TRUE(f.menu.TakeClicked());
	Click(f.menu, TabCentre(1));
	EXPECT_TRUE(f.menu.TakeClicked());
	// Missing, or letting go elsewhere, doesn't
	Click(f.menu, {400, 220});
	EXPECT_FALSE(f.menu.TakeClicked());
	f.menu.MouseDown(ButtonCentre(0));
	f.menu.MouseUp(ButtonCentre(2));
	EXPECT_FALSE(f.menu.TakeClicked());
	// Nor does Escape
	f.menu.Escape();
	EXPECT_FALSE(f.menu.TakeClicked());
}

TEST(GameMenu, TheClickIsTheMenuButtonsSound)
{
	// sample 159 of the InGame bank, mode 3, once, in 2D
	constexpr auto& k_Sound = openblack::gui::k_ButtonSound;
	EXPECT_EQ(k_Sound.sample, 159);
	EXPECT_EQ(k_Sound.mode, 3);
	EXPECT_EQ(k_Sound.loops, 0);
	EXPECT_FALSE(k_Sound.extra3DFlag);
	EXPECT_FALSE(k_Sound.is3D);
	EXPECT_EQ(k_Sound.bank, openblack::audio::SfxBank::InGame);
}

TEST(GameMenu, ASliderClicksAsItIsLetGoAndTheKeysNeverClick)
{
	MenuFixture f;
	Click(f.menu, ButtonCentre(3));
	EXPECT_TRUE(f.menu.TakeClicked());
	// A step of the SFX slider (300 by 30 at 250, 140) clicks as it is let go
	Click(f.menu, {300, 155});
	EXPECT_TRUE(f.menu.TakeClicked());
	// So does its knob, dragged and let go over the slider
	f.menu.MouseDown({500, 155});
	f.menu.MouseMove({473, 155});
	f.menu.Update(0.0f);
	f.menu.MouseUp({473, 155});
	EXPECT_TRUE(f.menu.TakeClicked());
	// Working the focused slider and the page from the keyboard never clicks
	for (const int key : {SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN, SDLK_TAB, SDLK_SPACE, SDLK_RETURN})
	{
		f.menu.KeyDown(key);
		EXPECT_FALSE(f.menu.TakeClicked()) << key;
	}
}

TEST(GameMenu, SlidersStepOrFollowTheKnob)
{
	MenuFixture f;
	Click(f.menu, ButtonCentre(3));
	f.menu.TakeSettingsChanged();
	// The SFX slider is 300 by 30 at 250, 140: at full volume its knob is at its right end
	EXPECT_FLOAT_EQ(f.menu.GetSettings().sfxVolume, 1.0f);
	Click(f.menu, {300, 155});
	EXPECT_NEAR(f.menu.GetSettings().sfxVolume, 0.9f, 1e-5f);
	EXPECT_TRUE(f.menu.TakeSettingsChanged());

	// Dragging the knob, at 250 + 270 * 0.9 = 493, 27 pixels left takes a tenth off
	f.menu.MouseDown({500, 155});
	f.menu.MouseMove({473, 155});
	f.menu.Update(0.0f);
	f.menu.MouseUp({473, 155});
	EXPECT_NEAR(f.menu.GetSettings().sfxVolume, 0.8f, 1e-5f);

	// The music slider below
	Click(f.menu, {300, 215});
	EXPECT_NEAR(f.menu.GetSettings().musicVolume, 0.9f, 1e-5f);
}

TEST(GameMenu, AVolumeSliderIsTheVolumeTimesTheFloatNearestOneOver127)
{
	// The eight volumes where that differs from dividing by 127, as the original's options box sets them
	constexpr std::array<std::pair<uint32_t, uint32_t>, 8> k_Cases = {{
	    {9, 0x3D912244u},
	    {13, 0x3DD1A346u},
	    {18, 0x3E112244u},
	    {26, 0x3E51A346u},
	    {36, 0x3E912244u},
	    {52, 0x3ED1A346u},
	    {72, 0x3F112244u},
	    {104, 0x3F51A346u},
	}};
	EXPECT_EQ(std::bit_cast<uint32_t>(k_SliderPerVolume), 0x3C010204u);
	for (const auto& [volume, bits] : k_Cases)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(SliderOfVolume(volume)), bits) << volume;
		// and read back, one lower
		EXPECT_EQ(VolumeOfSlider(SliderOfVolume(volume)), volume - 1) << volume;
	}
	EXPECT_EQ(SliderOfVolume(0), 0.0f);
	EXPECT_EQ(SliderOfVolume(127), 1.0f);
	EXPECT_EQ(VolumeOfSlider(1.0f), 127u);
}

TEST(GameMenu, TheVolumeSlidersAreSetFromTheMainVolumesEachTimeAnOptionsPageShows)
{
	MenuFixture f;
	uint32_t sfx = 127;
	uint32_t music = 127;
	f.menu.SetVolumeSource([&sfx, &music](bool /*boxLeft*/) { return std::pair(sfx, music); });
	Click(f.menu, ButtonCentre(3));
	EXPECT_EQ(f.menu.GetSettings().sfxVolume, 1.0f);
	// A click left of the knob steps it a tenth, to 0.9, which the game takes as the volume 114
	Click(f.menu, {300, 155});
	EXPECT_NEAR(f.menu.GetSettings().sfxVolume, 0.9f, 1e-5f);
	sfx = VolumeOfSlider(f.menu.GetSettings().sfxVolume);
	ASSERT_EQ(sfx, 114u);
	// The Advanced page shows: the sliders are set again, at the volume taken
	Click(f.menu, TabCentre(3));
	EXPECT_EQ(f.menu.GetSettings().sfxVolume, SliderOfVolume(114));
	// The Options page again, from its own tab too, and the knob is there: the next step starts from it
	music = 9;
	Click(f.menu, TabCentre(1));
	Click(f.menu, TabCentre(1));
	EXPECT_EQ(std::bit_cast<uint32_t>(f.menu.GetSettings().musicVolume), 0x3D912244u);
	Click(f.menu, {300, 155});
	EXPECT_FLOAT_EQ(f.menu.GetSettings().sfxVolume, SliderOfVolume(114) - 0.1f);
	// The Players and Controls pages and the first page do not set them
	music = 50;
	Click(f.menu, TabCentre(2));
	Click(f.menu, TabCentre(4));
	Click(f.menu, k_Back);
	EXPECT_EQ(std::bit_cast<uint32_t>(f.menu.GetSettings().musicVolume), 0x3D912244u);
}

TEST(GameMenu, TheVolumesAreKeptAsTheBoxIsLeftBeforeTheShownPageReadsThem)
{
	MenuFixture f;
	// The game's side: the volume played, the one kept for the computer, and what was done to them
	uint32_t played = 127;
	uint32_t kept = 100;
	std::vector<std::string> done;
	f.menu.SetVolumeSource([&played, &kept, &done](bool boxLeft) {
		if (boxLeft)
		{
			kept = played;
			done.emplace_back("keep");
		}
		played = kept;
		done.emplace_back("read");
		return std::pair(played, played);
	});
	// The game's own step for a leave keeps the volume played
	const auto takeLeave = [&f, &played, &kept, &done] {
		if (f.menu.TakeOptionsLeft())
		{
			kept = played;
			done.emplace_back("keep");
		}
	};
	// From the first page the Options page reads the kept volume
	Click(f.menu, ButtonCentre(3));
	takeLeave();
	EXPECT_EQ(done, std::vector<std::string> {"read"});
	EXPECT_EQ(f.menu.GetSettings().sfxVolume, SliderOfVolume(100));
	// The slider stepped down: the game plays its volume at once
	Click(f.menu, {300, 155});
	played = VolumeOfSlider(f.menu.GetSettings().sfxVolume);
	ASSERT_EQ(played, 87u);
	// The Advanced tab: the volume is kept before the Advanced page reads it, so the change stays
	done.clear();
	Click(f.menu, TabCentre(3));
	EXPECT_EQ(done, (std::vector<std::string> {"keep", "read"}));
	EXPECT_EQ(kept, 87u);
	EXPECT_EQ(f.menu.GetSettings().sfxVolume, SliderOfVolume(87));
	takeLeave();
	EXPECT_EQ(kept, 87u);
	// Its own tab: kept, then read again
	done.clear();
	Click(f.menu, TabCentre(3));
	EXPECT_EQ(done, (std::vector<std::string> {"keep", "read"}));
	takeLeave();
	// Back: kept by the game's step, nothing read
	done.clear();
	Click(f.menu, k_Back);
	takeLeave();
	EXPECT_EQ(done, std::vector<std::string> {"keep"});
}

TEST(GameMenu, SelectorsAndCheckBoxesChangeTheSettings)
{
	MenuFixture f;
	Click(f.menu, ButtonCentre(3));
	// The detail: the right arrow steps on and wraps round, the left steps back, the button steps on
	EXPECT_EQ(f.menu.GetSettings().detail, 4);
	Click(f.menu, {524, 365});
	EXPECT_EQ(f.menu.GetSettings().detail, 0);
	Click(f.menu, {276, 365});
	EXPECT_EQ(f.menu.GetSettings().detail, 4);
	Click(f.menu, {400, 365});
	EXPECT_EQ(f.menu.GetSettings().detail, 0);

	// AutoSave's square at 285, 450
	EXPECT_TRUE(f.menu.GetSettings().autoSave);
	Click(f.menu, {297, 462});
	EXPECT_FALSE(f.menu.GetSettings().autoSave);

	// The detail changed since the Options page opened: another tab first tells the player about the video change,
	// and the notice's OK goes on to that tab
	Click(f.menu, TabCentre(3));
	EXPECT_EQ(f.menu.GetPage(), Page::Options);
	EXPECT_TRUE(f.menu.IsAskingToQuit());
	Click(f.menu, k_NoticeOk);
	EXPECT_FALSE(f.menu.IsAskingToQuit());
	EXPECT_EQ(f.menu.GetPage(), Page::Advanced);

	// Advanced: the left handed hand at 185, 450 and the help level
	Click(f.menu, {197, 462});
	EXPECT_TRUE(f.menu.GetSettings().leftHandedHand);
	EXPECT_EQ(f.menu.GetSettings().helpLevel, 3);
	Click(f.menu, {524, 165});
	EXPECT_EQ(f.menu.GetSettings().helpLevel, 4);
	EXPECT_TRUE(f.menu.TakeSettingsChanged());
}

TEST(GameMenu, TheOptionsBoxIsLeftByBackATabEscapeOrTheMenuClosing)
{
	MenuFixture f;
	Click(f.menu, ButtonCentre(3));
	EXPECT_FALSE(f.menu.TakeOptionsLeft());
	// The Options page left for the Advanced tab
	Click(f.menu, TabCentre(3));
	ASSERT_EQ(f.menu.GetPage(), Page::Advanced);
	EXPECT_TRUE(f.menu.TakeOptionsLeft());
	// A selector steps only the page's own choice
	Click(f.menu, {524, 165});
	EXPECT_EQ(f.menu.GetSettings().helpLevel, 4);
	EXPECT_FALSE(f.menu.TakeOptionsLeft());
	// Its own tab again
	Click(f.menu, TabCentre(3));
	EXPECT_TRUE(f.menu.TakeOptionsLeft());
	// Back
	Click(f.menu, k_Back);
	EXPECT_EQ(f.menu.GetPage(), Page::Main);
	EXPECT_TRUE(f.menu.TakeOptionsLeft());
	// Escape
	Click(f.menu, ButtonCentre(3));
	Click(f.menu, TabCentre(3));
	(void)f.menu.TakeOptionsLeft();
	(void)f.menu.Escape();
	EXPECT_EQ(f.menu.GetPage(), Page::Main);
	EXPECT_TRUE(f.menu.TakeOptionsLeft());
	// The menu closing over the Advanced page, and only once
	Click(f.menu, ButtonCentre(3));
	Click(f.menu, TabCentre(3));
	(void)f.menu.TakeOptionsLeft();
	f.menu.Close();
	EXPECT_TRUE(f.menu.TakeOptionsLeft());
	f.menu.Close();
	EXPECT_FALSE(f.menu.TakeOptionsLeft());
}

TEST(GameMenu, ThePlayersAndControlsPagesAreNotTheOptionsBox)
{
	MenuFixture f;
	Click(f.menu, ButtonCentre(3));
	// Controls: the Options page is left
	Click(f.menu, TabCentre(4));
	ASSERT_EQ(f.menu.GetPage(), Page::Controls);
	EXPECT_TRUE(f.menu.TakeOptionsLeft());
	// Players, then the first page: the options' box is not left again
	Click(f.menu, TabCentre(2));
	ASSERT_EQ(f.menu.GetPage(), Page::Players);
	Click(f.menu, k_Back);
	ASSERT_EQ(f.menu.GetPage(), Page::Main);
	EXPECT_FALSE(f.menu.TakeOptionsLeft());
	// Nor by the menu closing over the first page
	f.menu.Close();
	EXPECT_FALSE(f.menu.TakeOptionsLeft());
}

TEST(GameMenu, TheHelpTextSpeedIsTakenAsTheOptionsBoxIsLeft)
{
	MenuFixture f;
	Click(f.menu, ButtonCentre(3));
	Click(f.menu, TabCentre(3));
	(void)f.menu.TakeOptionsLeft();
	// The help text speed slider, 300 by 30 at 250, 365: a click left of the knob at 0.5 steps it a tenth down
	Click(f.menu, {300, 380});
	EXPECT_FLOAT_EQ(f.menu.GetSettings().helpTextSpeed, 0.4f);
	EXPECT_FALSE(f.menu.TakeOptionsLeft());
	// Back: the box is left with the new speed, which the game takes then, not as the menu closes later
	Click(f.menu, k_Back);
	EXPECT_TRUE(f.menu.TakeOptionsLeft());
	EXPECT_FLOAT_EQ(f.menu.GetSettings().helpTextSpeed, 0.4f);
	f.menu.Close();
	EXPECT_FALSE(f.menu.TakeOptionsLeft());
}

namespace
{
/// The creature help's switch, which is all the options set
class FakeCreatureHelp final: public openblack::ecs::systems::CreatureHelpSystemInterface
{
public:
	void Raise(entt::entity /*creature*/, const openblack::creature_help::Message& /*message*/) override {}
	void ProcessTurn() override {}
	void PromptKnown(entt::entity /*creature*/, openblack::creature_help::Prompt /*prompt*/) override {}
	void Fainting(entt::entity /*creature*/) override {}
	void SetHelpOn(bool on) override { helpOn = on; }
	[[nodiscard]] bool IsHelpOn() const override { return helpOn; }
	[[nodiscard]] const std::vector<Told>& GetTold() const override { return _told; }

	bool helpOn {true};

private:
	std::vector<Told> _told;
};
} // namespace

TEST(GameMenu, TheLeftHandedBoxReachesTheHandOnlyAsAPageShowsAfterTheLeave)
{
	// The original's check boxes tick themselves with no handler, the leave writes the left-handed box to the profile,
	// and the hand takes its side from the profile as a page next shows (the first page's own, the options pages'
	// own, and a tab's through the first page)
	MenuFixture f;
	EXPECT_TRUE(f.menu.TakeProfileShown());
	EXPECT_FALSE(f.menu.TakeProfileShown());
	Click(f.menu, ButtonCentre(3));
	Click(f.menu, TabCentre(3));
	ASSERT_EQ(f.menu.GetPage(), Page::Advanced);
	(void)f.menu.TakeOptionsLeft();
	(void)f.menu.TakeProfileShown();
	// The box at 185, 450: ticked, and no page shown
	Click(f.menu, {197, 462});
	EXPECT_TRUE(f.menu.GetSettings().leftHandedHand);
	EXPECT_FALSE(f.menu.TakeProfileShown());
	EXPECT_FALSE(f.menu.TakeOptionsLeft());
	// Back: the box left, then the first page shows
	Click(f.menu, k_Back);
	EXPECT_TRUE(f.menu.TakeOptionsLeft());
	EXPECT_TRUE(f.menu.TakeProfileShown());
	// The menu closing shows no page
	f.menu.Close();
	EXPECT_FALSE(f.menu.TakeProfileShown());
}

TEST(GameMenu, TheMenuPausesTheGameAsItOpensAndContinueUnpausesItButNotInsideTheTemple)
{
	// The original's main menu pauses the game each time it shows (PauseGame(1), a single player game) and Continue
	// unpauses it (PauseGame(0), a plain flag, whatever paused the game before)
	bool menuPaused = false;
	auto change = StepMenuPause(true, false, false, menuPaused);
	EXPECT_EQ(change.pause, std::optional<bool>(true));
	EXPECT_FALSE(change.closed);
	EXPECT_TRUE(menuPaused);
	// open from frame to frame: nothing more
	change = StepMenuPause(true, true, false, menuPaused);
	EXPECT_FALSE(change.pause.has_value());
	EXPECT_FALSE(change.closed);
	change = StepMenuPause(false, true, false, menuPaused);
	EXPECT_EQ(change.pause, std::optional<bool>(false));
	EXPECT_TRUE(change.closed);
	change = StepMenuPause(false, false, false, menuPaused);
	EXPECT_FALSE(change.pause.has_value());
	EXPECT_FALSE(change.closed);

	// Inside the temple the main menu never shows (a tab shows it only outside the citadel, and the options room
	// shows the Options page itself) and no options page pauses: the temple's own pause holds, and closing gives
	// nothing back, even with the temple left while it was open
	change = StepMenuPause(true, false, true, menuPaused);
	EXPECT_FALSE(change.pause.has_value());
	EXPECT_FALSE(menuPaused);
	change = StepMenuPause(false, true, false, menuPaused);
	EXPECT_FALSE(change.pause.has_value());
	EXPECT_TRUE(change.closed);
}

TEST(GameMenu, TheAdvancedChoicesReachTheHelpAsTheOptionsBoxIsLeft)
{
	// The original's leave writes each selector's place, each box and the slider to the profile as they are, and the
	// help system takes them from there: the help level, the texts shown (TEXT_DRAW), the tooltips' level, the creature
	// help, where the newest text goes (TEXT_TOPTOBOTTOM) and the read speed
	MenuFixture f;
	Click(f.menu, ButtonCentre(3));
	Click(f.menu, TabCentre(3));
	ASSERT_EQ(f.menu.GetPage(), Page::Advanced);
	(void)f.menu.TakeOptionsLeft();
	// The help level's right arrow (3 to 4, all help), the story text's left one (1 to 0, no text), the tooltips'
	// right one (2 to 3, all), the creature help's box at 385, 450 (off), the text from the bottom's at 585, 450 (on)
	// and the speed slider a tenth down
	Click(f.menu, {524, 165});
	Click(f.menu, {276, 230});
	Click(f.menu, {524, 295});
	Click(f.menu, {397, 462});
	Click(f.menu, {597, 462});
	Click(f.menu, {300, 380});
	// Nothing reaches the help until the box is left
	Click(f.menu, k_Back);
	ASSERT_TRUE(f.menu.TakeOptionsLeft());
	const auto kept = ToProfileSettings(f.menu.GetSettings());

	std::unique_ptr<openblack::help::DialogueText> dialogue;
	openblack::help::HelpSystem help(openblack::help::HelpSystem::Info {8, 5}, {}, {}, dialogue);
	FakeCreatureHelp creatureHelp;
	openblack::help::ApplyPlayerChoices(kept, &help, dialogue.get(), &creatureHelp);
	EXPECT_EQ(help.GetHelpLevel(), 4);
	EXPECT_EQ(dialogue->GetTextDraw(), 0);
	EXPECT_EQ(openblack::help::tooltips::Level(), 3);
	EXPECT_FALSE(creatureHelp.IsHelpOn());
	EXPECT_TRUE(dialogue->GetTextTopToBottom());
	EXPECT_FLOAT_EQ(dialogue->GetReadSpeed(), 0.4f);

	// Before the help system is made only the tooltips' level is taken
	openblack::player_profiles::Settings again = kept;
	again.toolTips = 1;
	openblack::help::ApplyPlayerChoices(again, nullptr, nullptr, nullptr);
	EXPECT_EQ(openblack::help::tooltips::Level(), 1);
}

TEST(GameMenu, TheCreatureHelpBoxShowsTheSwitchAndThePlayerChangesItFromThere)
{
	MenuFixture f;
	// The game shows the switch a script turned off: no change of the player's
	f.menu.ShowCreatureHelp(false);
	EXPECT_FALSE(f.menu.GetSettings().creatureHelp);
	EXPECT_FALSE(f.menu.TakeSettingsChanged());
	// Advanced: the creature help box at 385, 450 turns it on again from what it showed
	Click(f.menu, ButtonCentre(3));
	Click(f.menu, TabCentre(3));
	EXPECT_EQ(f.menu.GetPage(), Page::Advanced);
	f.menu.TakeSettingsChanged();
	Click(f.menu, {397, 462});
	EXPECT_TRUE(f.menu.GetSettings().creatureHelp);
	// And shown off again, the next click turns it on, not off
	f.menu.ShowCreatureHelp(false);
	Click(f.menu, {397, 462});
	EXPECT_TRUE(f.menu.GetSettings().creatureHelp);
	EXPECT_TRUE(f.menu.TakeSettingsChanged());
}

TEST(GameMenu, TheOptionsPagesTakeTheProfilesChoicesEachTimeEitherShows)
{
	MenuFixture f;
	openblack::player_profiles::Settings kept;
	kept.helpLevel = 1;
	kept.storyText = 2;
	kept.toolTips = 0;
	kept.creatureHelp = false;
	kept.readSpeed = 0.25f;
	kept.textFromBottom = true;
	kept.leftHanded = true;
	kept.autoSave = false;
	f.menu.SetProfileSource([&kept] { return std::optional(kept); });
	// What the game does as the box is left: the menu's choices kept with the profile
	const auto keepIfLeft = [&f, &kept] {
		if (f.menu.TakeOptionsLeft())
		{
			kept = ToProfileSettings(f.menu.GetSettings());
		}
	};
	// While the menu is closed the creature help box follows a script's switch
	f.menu.ShowCreatureHelp(true);
	Click(f.menu, ButtonCentre(3));
	keepIfLeft();
	const auto& shown = f.menu.GetSettings();
	EXPECT_FALSE(shown.creatureHelp);
	EXPECT_FALSE(shown.autoSave);
	EXPECT_EQ(shown.helpLevel, 1);
	EXPECT_EQ(shown.storyText, 2);
	EXPECT_EQ(shown.toolTips, 0);
	EXPECT_FLOAT_EQ(shown.helpTextSpeed, 0.25f);
	EXPECT_TRUE(shown.textFromBottom);
	// The left-handed box stays as the hand is drawn
	EXPECT_FALSE(shown.leftHandedHand);
	// The boxes show them: the auto save box ticks on from off
	Click(f.menu, {297, 462});
	EXPECT_TRUE(shown.autoSave);
	// The Advanced tab: leaving the Options page keeps its choice first, so it stays
	Click(f.menu, TabCentre(3));
	EXPECT_TRUE(shown.autoSave);
	keepIfLeft();
	EXPECT_TRUE(kept.autoSave);
	// The creature help box ticks on from off, and the help text speed steps a tenth down from 0.25
	Click(f.menu, {397, 462});
	EXPECT_TRUE(shown.creatureHelp);
	Click(f.menu, {260, 380});
	EXPECT_FLOAT_EQ(shown.helpTextSpeed, 0.15f);
	Click(f.menu, k_Back);
	keepIfLeft();
	EXPECT_TRUE(kept.creatureHelp);

	// The profile changed while the box was away: shown again, the page takes it, and its selector steps on from it
	kept.helpLevel = 4;
	kept.creatureHelp = false;
	Click(f.menu, ButtonCentre(3));
	keepIfLeft();
	Click(f.menu, TabCentre(3));
	keepIfLeft();
	EXPECT_EQ(shown.helpLevel, 4);
	EXPECT_FALSE(shown.creatureHelp);
	Click(f.menu, {524, 165});
	EXPECT_EQ(shown.helpLevel, 0);

	// Without a profile the pages keep the menu's own choices
	f.menu.SetProfileSource([] { return std::optional<openblack::player_profiles::Settings>(); });
	Click(f.menu, k_Back);
	(void)f.menu.TakeOptionsLeft();
	Click(f.menu, ButtonCentre(3));
	EXPECT_EQ(shown.helpLevel, 0);
}

TEST(GameMenu, PlayersNameTheirCreatureAndPickASymbol)
{
	MenuFixture f;
	Click(f.menu, ButtonCentre(3));
	Click(f.menu, TabCentre(2));

	// The creature's name box takes the focus and the keyboard; the first click into it selects the whole name, which
	// typing replaces
	Click(f.menu, {400, 390});
	f.menu.TextInput(u"AB");
	EXPECT_EQ(f.menu.GetSettings().creatureName, u"AB");
	f.menu.KeyDown(SDLK_BACKSPACE);
	EXPECT_EQ(f.menu.GetSettings().creatureName, u"A");

	// Holding the button down on the symbol brings up the others to pick from, once they are all the way up
	const SymbolPicture picture({.min = {368, 295}, .max = {432, 359}}, 15);
	const auto three = picture.GetRingRect(3).Centre();
	f.menu.MouseDown({400, 327});
	f.menu.MouseMove(three);
	f.menu.Update(0.1f);
	f.menu.MouseUp(three);
	EXPECT_EQ(f.menu.GetSettings().symbol, 15);
	f.menu.MouseDown({400, 327});
	f.menu.MouseMove(three);
	f.menu.Update(0.6f);
	f.menu.MouseUp(three);
	EXPECT_EQ(f.menu.GetSettings().symbol, 3);
}

namespace
{
/// A point on a row of the Players page's list
glm::ivec2 PlayerRow(const GameMenu& menu, size_t row)
{
	for (int y = 120; y < 280; ++y)
	{
		if (menu.GetPlayers().ItemAt(y) == row)
		{
			return {400, y};
		}
	}
	return {400, 0};
}

/// The menu on the Players page, Ann, Bob and Matt kept and Matt played
struct PlayersFixture: MenuFixture
{
	PlayersFixture()
	{
		menu.SetPlayers({u"Ann", u"Bob", u"Matt"}, 2);
		Click(menu, ButtonCentre(3));
		Click(menu, TabCentre(2));
	}
};

constexpr glm::ivec2 k_Yes {216, 354};
constexpr glm::ivec2 k_No {584, 354};
constexpr glm::ivec2 k_Ok {400, 354};
} // namespace

TEST(GameMenu, PlayersPickAProfileAndLeavingStartsTheGameAgain)
{
	PlayersFixture f;
	EXPECT_EQ(f.menu.GetPlayers().GetSelection(), 2u);
	EXPECT_EQ(f.menu.GetPlayersBackLabel(), u"Back");
	EXPECT_FALSE(f.menu.IsRestartPending());

	// Picking another plays as it at once; leaving the page then starts the game again
	EXPECT_EQ(Click(f.menu, PlayerRow(f.menu, 0)), Action::PickPlayer);
	EXPECT_EQ(f.menu.GetPickedPlayer(), 0u);
	EXPECT_TRUE(f.menu.IsRestartPending());
	EXPECT_EQ(f.menu.GetPlayersBackLabel(), u"Restart");
	// The one picked again is no change
	EXPECT_EQ(Click(f.menu, PlayerRow(f.menu, 0)), Action::None);

	// Escape can't leave the page
	EXPECT_EQ(f.menu.Escape(), Action::None);
	EXPECT_EQ(f.menu.GetPage(), Page::Players);
	// Restart asks first; No stays
	EXPECT_EQ(Click(f.menu, k_Back), Action::None);
	EXPECT_EQ(f.menu.GetQuestionText(), u"Restart?");
	EXPECT_EQ(Click(f.menu, k_No), Action::None);
	EXPECT_EQ(f.menu.GetPage(), Page::Players);
	// So does another tab, but not the page's own
	Click(f.menu, TabCentre(2));
	EXPECT_FALSE(f.menu.IsAskingToQuit());
	Click(f.menu, TabCentre(1));
	EXPECT_EQ(f.menu.GetQuestionText(), u"Restart?");
	EXPECT_EQ(Click(f.menu, k_Yes), Action::Restart);

	// Picking the one the game was started as again is Back once more
	EXPECT_EQ(Click(f.menu, PlayerRow(f.menu, 2)), Action::PickPlayer);
	EXPECT_FALSE(f.menu.IsRestartPending());
	EXPECT_EQ(f.menu.GetPlayersBackLabel(), u"Back");
	Click(f.menu, k_Back);
	EXPECT_EQ(f.menu.GetPage(), Page::Main);
}

TEST(GameMenu, PlayersAreDeletedAfterAskingByName)
{
	PlayersFixture f;
	// Deleting another than the one the game was started as plays the one it was started as
	Click(f.menu, PlayerRow(f.menu, 0));
	Click(f.menu, f.menu.GetPlayerButtonRect(1).Centre());
	EXPECT_EQ(f.menu.GetQuestionText(), u"Delete player Ann?");
	EXPECT_EQ(Click(f.menu, k_No), Action::None);
	Click(f.menu, f.menu.GetPlayerButtonRect(1).Centre());
	EXPECT_EQ(Click(f.menu, k_Yes), Action::DeletePlayer);
	EXPECT_EQ(f.menu.GetPlayerToDelete(), u"Ann");
	EXPECT_EQ(f.menu.GetPickedPlayer(), 1u);
	f.menu.SetPlayers({u"Bob", u"Matt"}, 1);
	EXPECT_FALSE(f.menu.IsRestartPending());

	// Deleting the one it was started as plays the first left, and leaving the page must start the game again
	Click(f.menu, f.menu.GetPlayerButtonRect(1).Centre());
	EXPECT_EQ(Click(f.menu, k_Yes), Action::DeletePlayer);
	EXPECT_EQ(f.menu.GetPlayerToDelete(), u"Matt");
	EXPECT_EQ(f.menu.GetPickedPlayer(), 0u);
	f.menu.SetPlayers({u"Bob"}, 0);
	EXPECT_TRUE(f.menu.IsRestartPending());
	EXPECT_EQ(f.menu.GetPlayersBackLabel(), u"Restart");
	// It tells the player, with an OK
	Click(f.menu, k_Back);
	EXPECT_EQ(f.menu.GetQuestionText(), u"Restarting now.");
	EXPECT_EQ(Click(f.menu, k_Ok), Action::Restart);

	// Deleting the last leaves none to play
	Click(f.menu, f.menu.GetPlayerButtonRect(1).Centre());
	EXPECT_EQ(Click(f.menu, k_Yes), Action::DeletePlayer);
	EXPECT_FALSE(f.menu.GetPickedPlayer().has_value());
}

TEST(GameMenu, PlayersStartANewGameOrMakeAPlayerAfterAsking)
{
	PlayersFixture f;
	Click(f.menu, f.menu.GetPlayerButtonRect(3).Centre());
	EXPECT_EQ(f.menu.GetQuestionText(), u"New game?");
	EXPECT_EQ(Click(f.menu, k_Yes), Action::StartNewGame);
	EXPECT_EQ(Click(f.menu, f.menu.GetPlayerButtonRect(0).Centre()), Action::CreatePlayer);
	// A profile made is played: the game must start again
	f.menu.SetPlayers({u"Ann", u"Bob", u"Matt", u"Zed"}, 3);
	EXPECT_TRUE(f.menu.IsRestartPending());
}

TEST(GameMenu, PlayersSquareButtonsSpreadOutWithEditTattooOrWithout)
{
	PlayersFixture f;
	// Without Edit Tattoo, three
	EXPECT_FALSE(f.menu.IsPlayerButtonVisible(2));
	EXPECT_EQ(f.menu.GetPlayerButtonRect(0).min, glm::ivec2(188, 450));
	EXPECT_EQ(f.menu.GetPlayerButtonRect(1).min, glm::ivec2(388, 450));
	EXPECT_EQ(f.menu.GetPlayerButtonRect(3).min, glm::ivec2(588, 450));
	EXPECT_EQ(f.menu.GetPlayerButtonRect(3).max, glm::ivec2(613, 475));
	// With it, four
	f.menu.SetTattooAvailable(true);
	EXPECT_TRUE(f.menu.IsPlayerButtonVisible(2));
	EXPECT_EQ(f.menu.GetPlayerButtonRect(0).min.x, 138);
	EXPECT_EQ(f.menu.GetPlayerButtonRect(1).min.x, 305);
	EXPECT_EQ(f.menu.GetPlayerButtonRect(2).min.x, 471);
	EXPECT_EQ(f.menu.GetPlayerButtonRect(3).min.x, 638);
	// Not while leaving the page starts the game again
	Click(f.menu, PlayerRow(f.menu, 0));
	EXPECT_FALSE(f.menu.IsPlayerButtonVisible(2));
	EXPECT_EQ(f.menu.GetPlayerButtonRect(0).min.x, 188);
}

TEST(GameMenu, PlayersEditsAreKeptAsThePageIsLeft)
{
	PlayersFixture f;
	EXPECT_FALSE(f.menu.TakePlayerEdits().has_value());
	Click(f.menu, {400, 390});
	f.menu.TextInput(u"Rex");
	EXPECT_FALSE(f.menu.TakePlayerEdits().has_value());
	f.menu.KeyDown(SDLK_RETURN);
	auto edits = f.menu.TakePlayerEdits();
	ASSERT_TRUE(edits.has_value());
	EXPECT_EQ(edits->creatureName, u"Rex");
	Click(f.menu, TabCentre(3));
	EXPECT_TRUE(f.menu.TakePlayerEdits().has_value());
	EXPECT_FALSE(f.menu.TakePlayerEdits().has_value());

	// Another profile shows its own creature's name, symbol and options
	openblack::player_profiles::Settings kept;
	kept.helpLevel = 0;
	f.menu.ShowPlayer(u"Spot", 7, kept);
	EXPECT_EQ(f.menu.GetSettings().creatureName, u"Spot");
	EXPECT_EQ(f.menu.GetSettings().symbol, 7);
	EXPECT_EQ(f.menu.GetSettings().helpLevel, 0);
}

TEST(GameMenu, AToolTipLevelOutOfTheSelectorsFourIsShownAtItsPlaceOnIt)
{
	// The options box's own wrap: a negative value 400 up, then its size, of four
	EXPECT_EQ(OptionsIndex(2, 4, 400), 2);
	EXPECT_EQ(OptionsIndex(5, 4, 400), 1);
	EXPECT_EQ(OptionsIndex(7, 4, 400), 3);
	EXPECT_EQ(OptionsIndex(-1, 4, 400), 3);
	EXPECT_EQ(OptionsIndex(-401, 4, 400), 1);
	// and with the counts and offsets of the help level's and the story text's selectors
	EXPECT_EQ(OptionsIndex(-1, 5, 500), 4);
	EXPECT_EQ(OptionsIndex(4, 3, 300), 1);

	// A profile's level 5 shows at Minimum, the Advanced page made with it, and is kept so as the box is left
	MenuFixture f;
	openblack::player_profiles::Settings kept;
	kept.toolTips = 5;
	f.menu.ShowPlayer(u"Spot", 7, kept);
	EXPECT_EQ(f.menu.GetSettings().toolTips, 1);
	EXPECT_EQ(ToProfileSettings(f.menu.GetSettings()).toolTips, 1);
	MenuSettings settings;
	kept.toolTips = -1;
	TakeProfileSettings(settings, kept);
	EXPECT_EQ(settings.toolTips, 3);
}

TEST(GameMenu, AStoryTextOrHelpLevelOutOfItsSelectorIsShownAtItsPlaceOnIt)
{
	// The story text's selector, of three, a negative value 300 up; the help level's, of five, 500 up
	MenuSettings settings;
	openblack::player_profiles::Settings kept;
	const std::array<std::pair<int, int>, 5> k_Stories = {{{2, 2}, {4, 1}, {7, 1}, {-1, 2}, {-301, 1}}};
	for (const auto& [value, place] : k_Stories)
	{
		kept.storyText = value;
		TakeProfileSettings(settings, kept);
		EXPECT_EQ(settings.storyText, place) << value;
	}
	const std::array<std::pair<int, int>, 5> k_Helps = {{{3, 3}, {5, 0}, {12, 2}, {-1, 4}, {-500, 0}}};
	for (const auto& [value, place] : k_Helps)
	{
		kept.helpLevel = value;
		TakeProfileSettings(settings, kept);
		EXPECT_EQ(settings.helpLevel, place) << value;
	}
	// Below -500 the original reads its label from outside its list: here the selector still shows one of its five
	kept.helpLevel = -503;
	TakeProfileSettings(settings, kept);
	EXPECT_GE(settings.helpLevel, 0);
	EXPECT_LT(settings.helpLevel, 5);

	// The Advanced page is made with such values without throwing, and keeps their places as the box is left
	MenuFixture f;
	kept.storyText = 7;
	kept.helpLevel = -1;
	f.menu.ShowPlayer(u"Spot", 7, kept);
	EXPECT_EQ(f.menu.GetSettings().storyText, 1);
	EXPECT_EQ(f.menu.GetSettings().helpLevel, 4);
	EXPECT_EQ(ToProfileSettings(f.menu.GetSettings()).helpLevel, 4);
}

TEST(GameMenu, APickedProfilesChoicesWaitForTheOptionsBoxToBeLeft)
{
	MenuFixture f;
	Click(f.menu, ButtonCentre(3));
	Click(f.menu, TabCentre(2));
	ASSERT_EQ(f.menu.GetPage(), Page::Players);
	(void)f.menu.TakeOptionsLeft();
	// Another profile picked: the menu shows its choices, but picking is not leaving the options' box
	openblack::player_profiles::Settings kept;
	kept.helpLevel = 0;
	kept.readSpeed = 0.8f;
	f.menu.ShowPlayer(u"Spot", 7, kept);
	EXPECT_EQ(f.menu.GetSettings().helpLevel, 0);
	EXPECT_FALSE(f.menu.TakeOptionsLeft());
	// Nor is going on to the Advanced page from the Players page
	Click(f.menu, TabCentre(3));
	ASSERT_EQ(f.menu.GetPage(), Page::Advanced);
	EXPECT_FALSE(f.menu.TakeOptionsLeft());
	// Leaving it is, with the picked profile's choices, the read speed too
	Click(f.menu, k_Back);
	EXPECT_TRUE(f.menu.TakeOptionsLeft());
	const auto applied = ToProfileSettings(f.menu.GetSettings());
	EXPECT_EQ(applied.helpLevel, 0);
	EXPECT_FLOAT_EQ(applied.readSpeed, 0.8f);
}

TEST(GameMenu, OptionsAreKeptWithTheProfileAsTheyAre)
{
	MenuSettings settings;
	settings.toolTips = 0;
	settings.storyText = 2;
	settings.helpLevel = 1;
	settings.creatureHelp = false;
	settings.helpTextSpeed = 0.75f;
	settings.textFromBottom = true;
	settings.leftHandedHand = true;
	settings.pushScrolling = true;
	settings.autoSave = false;
	const auto kept = ToProfileSettings(settings);
	MenuSettings back;
	TakeProfileSettings(back, kept);
	EXPECT_EQ(ToProfileSettings(back), kept);
	EXPECT_EQ(kept.helpLevel, 1);
	EXPECT_FALSE(kept.autoSave);
	EXPECT_TRUE(kept.leftHanded);
}

TEST(SymbolPicture, RingComesUpAndGoes)
{
	SymbolPicture picture({.min = {368, 295}, .max = {432, 359}}, 15);
	EXPECT_FLOAT_EQ(picture.GetRingOpening(), 0.0f);
	// The symbol picture's ring opens up over half a second, down over a second
	picture.MouseDown({400, 327});
	picture.Update(0.25f);
	EXPECT_GT(picture.GetRingOpening(), 0.0f);
	EXPECT_LT(picture.GetRingOpening(), 1.0f);
	picture.Update(0.3f);
	EXPECT_FLOAT_EQ(picture.GetRingOpening(), 1.0f);
	picture.Release();
	EXPECT_FALSE(picture.IsRingOpen());
	picture.Update(0.5f);
	EXPECT_GT(picture.GetRingOpening(), 0.0f);
	picture.Update(0.6f);
	EXPECT_FLOAT_EQ(picture.GetRingOpening(), 0.0f);

	// Six symbols on the inner ring 56 out from the centre, the first at the top, ten on the outer 104 out, each 23 either way
	// of its centre (24 at an opacity of 255 over 256)
	EXPECT_EQ(picture.GetRingRect(0).Centre(), glm::ivec2(400, 327 - 56));
	EXPECT_EQ(picture.GetRingRect(0).Width(), 46);
	EXPECT_EQ(picture.GetRingRect(3).Centre(), glm::ivec2(400, 327 + 56));
	EXPECT_EQ(picture.GetRingRect(6).Centre(), glm::ivec2(400, 327 - 104));
}

// The key binding list's mouse picture stands at the item's top, as high as the text size, across from the middle of
// the box less half the item's text width by the text before its gap, and only for an item wholly inside the list
TEST(GameMenu, AControlsMousePictureStandsAtItsItemsTopByTheTextBeforeItsGap)
{
	using openblack::gui::list_mouse::PlaceOf;
	// a box from 100 to 700, the list from 120 to 440; an item from 150 to 175 of text 26 high, 201 wide, 120 before
	// the gap: (100 - 201 + 700) / 2 = 299 (cut towards zero), then 120 on
	const auto place = PlaceOf(100, 700, 120, 440, 150, 175, 26, 201, 120);
	ASSERT_TRUE(place.has_value());
	EXPECT_EQ(place->x, 419);
	EXPECT_EQ(place->y, 150);
	// the item is shorter than the text size: as high as the item
	EXPECT_EQ(place->size, 25);
	EXPECT_EQ(PlaceOf(100, 700, 120, 440, 150, 190, 26, 201, 120)->size, 26);
	// widths past the box are cut to it
	EXPECT_EQ(PlaceOf(100, 700, 120, 440, 150, 175, 26, 900, 900)->x, 700);
	// an item cut by either end of the list has none
	EXPECT_FALSE(PlaceOf(100, 700, 120, 440, 110, 135, 26, 201, 120).has_value());
	EXPECT_FALSE(PlaceOf(100, 700, 120, 440, 430, 455, 26, 201, 120).has_value());
}

TEST(GameMenu, ListsTheControls)
{
	const MenuFixture f;
	const auto& items = f.menu.GetControls().GetItems();
	ASSERT_EQ(items.size(), 33);
	// A control's text: the action, its key and its mouse button after a gap for the mouse's picture
	EXPECT_EQ(items[0].text, u"Help : F1 ");
	EXPECT_FALSE(items[0].mouseCell.has_value());
	EXPECT_EQ(items[1].text, u"Move :       (LMB)");
	EXPECT_EQ(items[1].mouseCell, 9);
	EXPECT_TRUE(items[1].mouseMirrored);
	EXPECT_EQ(items[6].text, u"Zoom On : CTRL ");
	EXPECT_EQ(items[6].mouseCell, 10);
	EXPECT_EQ(items[29].text, u" : CTRL + S ");
}

TEST(GameMenu, PushScrollIsReportedAsItIsClicked)
{
	MenuFixture f;
	Click(f.menu, ButtonCentre(3));
	ASSERT_EQ(f.menu.GetPage(), Page::Options);
	// AutoSave's square at 285, 450 is not Push Scroll
	Click(f.menu, {297, 462});
	EXPECT_FALSE(f.menu.TakePushScrollChanged());
	// Push Scroll's at 485, 450, reported once
	Click(f.menu, {497, 462});
	EXPECT_TRUE(f.menu.GetSettings().pushScrolling);
	EXPECT_TRUE(f.menu.TakePushScrollChanged());
	EXPECT_FALSE(f.menu.TakePushScrollChanged());
}

TEST(GameMenu, LoadDefaultsAsksFirstWithItsOwnLabel)
{
	MenuFixture f;
	Click(f.menu, ButtonCentre(3));
	Click(f.menu, TabCentre(4));
	ASSERT_EQ(f.menu.GetPage(), Page::Controls);
	constexpr glm::ivec2 k_LoadDefaults {400, 550};
	// A no leaves the bindings as they are
	EXPECT_EQ(Click(f.menu, k_LoadDefaults), Action::None);
	EXPECT_EQ(f.menu.GetQuestionText(), u"Load Defaults");
	EXPECT_EQ(Click(f.menu, k_No), Action::None);
	// A yes puts them back
	Click(f.menu, k_LoadDefaults);
	EXPECT_EQ(Click(f.menu, k_Yes), Action::LoadDefaultControls);
	EXPECT_EQ(f.menu.GetPage(), Page::Controls);
}

namespace
{
/// A tattoo editor that only counts what the dialog asks of it
class FakeTattooEditor final: public openblack::ecs::systems::TattooEditorSystemInterface
{
public:
	void Open(entt::entity /*creature*/, const openblack::creature_tattoo_editor::Orbit& /*view*/) override { open = true; }
	void Close() override { open = false; }
	[[nodiscard]] bool IsOpen() const override { return open; }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override { return std::nullopt; }
	[[nodiscard]] const openblack::creature_tattoo_editor::Orbit& GetView() const override { return view; }
	[[nodiscard]] const openblack::creature_tattoo_editor::Session& GetSession() const override { return session; }
	void Update(float /*milliseconds*/) override {}
	void Steer(glm::ivec2 /*grab*/, glm::ivec2 /*pointer*/, float /*milliseconds*/) override {}
	void Hover(std::span<const openblack::creature_tattoo_editor::SiteOnScreen> /*sites*/, glm::ivec2 /*pointer*/) override {}
	[[nodiscard]] std::optional<uint8_t> GetHoveredSite() const override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::ivec2> GetHoveredPoint() const override { return std::nullopt; }
	bool Drop(uint8_t /*design*/, const glm::u8vec3& /*colour*/) override { return false; }
	std::optional<openblack::creature_tattoo::Slot> Lift() override { return std::nullopt; }
	openblack::creature_tattoo_editor::Accept Ok(bool /*networkGame*/) override
	{
		++oks;
		open = false;
		return openblack::creature_tattoo_editor::Accept::Close;
	}
	void Answered() override {}
	void Cancel() override
	{
		++cancels;
		open = false;
	}

	bool open {true};
	int oks {0};
	int cancels {0};
	openblack::creature_tattoo_editor::Orbit view {};
	openblack::creature_tattoo_editor::Session session {};
};
} // namespace

TEST(TattooEditorDialog, TakesOnlyEnterAndEscapeOfTheKeys)
{
	// The movement keys go past the dialog to the camera, which closes the editor as it is
	const NoopRenderer renderer;
	const auto texts = MenuTexts();
	const auto font = LoadFont();
	TattooEditorDialog dialog(texts, font);
	FakeTattooEditor editor;
	dialog.Open(false);
	for (const int key : {SDLK_w, SDLK_a, SDLK_s, SDLK_d, SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT})
	{
		EXPECT_FALSE(dialog.KeyDown(key, editor));
	}
	EXPECT_TRUE(dialog.IsOpen());
	EXPECT_EQ(editor.oks + editor.cancels, 0);
	// Enter is OK and Escape Cancel
	EXPECT_TRUE(dialog.KeyDown(SDLK_RETURN, editor));
	EXPECT_EQ(editor.oks, 1);
	EXPECT_FALSE(dialog.IsOpen());
	editor.open = true;
	dialog.Open(false);
	EXPECT_TRUE(dialog.KeyDown(SDLK_ESCAPE, editor));
	EXPECT_EQ(editor.cancels, 1);
	EXPECT_FALSE(dialog.IsOpen());
}

TEST(HideAllDialogs, TheMenuClosesAndTheTempleBoxesGoWithNeitherOkNorCancel)
{
	// As the bars come in: the menu fades out over a fifth of a second; the tattoo editor is left open for its box to
	// come back, its tattoos as they are, and the history's buttons go
	const NoopRenderer renderer;
	const auto texts = MenuTexts();
	const auto font = LoadFont();
	GameMenu menu(texts, font, u"aaa", MenuSettings {});
	TattooEditorDialog tattoo(texts, font);
	StoryHistoryBox history(texts, font);
	FakeTattooEditor editor;
	menu.Open();
	tattoo.Open(false);
	history.SetShown(true);
	menu.Update(0.6f);
	tattoo.Update(0.6f, editor, {}, false);
	history.Update(0.6f);

	HideAllDialogs(menu, tattoo, history);

	EXPECT_FALSE(menu.IsOpen());
	EXPECT_TRUE(menu.IsVisible());
	menu.Update(0.21f);
	EXPECT_FALSE(menu.IsVisible());
	EXPECT_FALSE(tattoo.IsOpen());
	EXPECT_EQ(editor.oks + editor.cancels, 0);
	EXPECT_TRUE(editor.open);
	EXPECT_FALSE(history.IsActive());
	// Nothing is up: putting them away again changes nothing
	HideAllDialogs(menu, tattoo, history);
	EXPECT_FALSE(menu.IsOpen());
	EXPECT_EQ(editor.oks + editor.cancels, 0);
}

TEST(TattooEditorDialog, ShownAgainItFadesInFromNothing)
{
	// As every box shown: from nothing over half a second, even when it was still fading out
	const NoopRenderer renderer;
	const auto texts = MenuTexts();
	const auto font = LoadFont();
	TattooEditorDialog tattoo(texts, font);
	StoryHistoryBox history(texts, font);
	FakeTattooEditor editor;
	tattoo.Open(false);
	history.SetShown(true);
	tattoo.Update(0.6f, editor, {}, false);
	history.Update(0.6f);
	ASSERT_EQ(tattoo.GetAlpha(), 1.0f);
	ASSERT_EQ(history.GetAlpha(), 1.0f);
	tattoo.Close();
	history.SetShown(false);
	tattoo.Update(0.05f, editor, {}, false);
	history.Update(0.05f);
	EXPECT_GT(tattoo.GetAlpha(), 0.0f);
	EXPECT_GT(history.GetAlpha(), 0.0f);
	tattoo.Open(false);
	history.SetShown(true);
	EXPECT_EQ(tattoo.GetAlpha(), 0.0f);
	EXPECT_EQ(history.GetAlpha(), 0.0f);
	tattoo.Update(0.6f, editor, {}, false);
	history.Update(0.6f);
	EXPECT_EQ(tattoo.GetAlpha(), 1.0f);
	EXPECT_EQ(history.GetAlpha(), 1.0f);
}

namespace
{
/// The Game Statistics box's texts, by the names the original's box reads: the six tabs, the end of a game's first tab,
/// the heading, the Back arrow, the line graphs, the check boxes and the totals' heading
TextDatabase StatisticsTexts()
{
	std::u16string script;
	const std::vector<std::pair<std::u16string, std::u16string>> texts = {
	    {u"HELP_TEXT_DIALOG_ADDITION_88", u"Main Menu"},
	    {u"HELP_TEXT_DIALOG_ADDITION_111", u"Player Stats"},
	    {u"HELP_TEXT_DIALOG_ADDITION_46", u"Creature Stats"},
	    {u"HELP_TEXT_DIALOG_ADDITION_138", u"Town Stats"},
	    {u"HELP_TEXT_DIALOG_ADDITION_87", u"Miracle Stats"},
	    {u"HELP_TEXT_DIALOG_ADDITION_124", u"Tech Stats"},
	    {u"HELP_TEXT_GAME_OVER", u"Game Over"},
	    {u"HELP_TEXT_DIALOG_ADDITION_61", u"Game Statistics"},
	    {u"HELP_TEXT_DIALOG_TATOOBACK", u"Back"},
	    {u"HELP_TEXT_PAUSE_STATS_05", u"Influence"},
	    {u"HELP_TEXT_PAUSE_STATS_06", u"Population"},
	    {u"HELP_TEXT_PAUSE_STATS_07", u"Graphic"},
	    {u"HELP_TEXT_PAUSE_STATS_08", u"Numeric"},
	    {u"HELP_TEXT_PAUSE_STATS_03", u"Total"},
	};
	for (const auto& [name, text] : texts)
	{
		script.append(u"ADD_TEXT(1, N, \"").append(name).append(u"\", \"").append(text).append(u"\")\n");
	}
	TextDatabase database;
	database.AddScript(Utf16Script(script));
	return database;
}

/// Two players with a few figures
openblack::statistics::BoxFigures TwoPlayersFigures()
{
	openblack::statistics::BoxFigures figures;
	figures.playersInGame = 2;
	figures.landNumber = 1;
	openblack::statistics::PlayerFigures one {.player = openblack::PlayerNames::PLAYER_ONE};
	one.population = 40;
	one.births = 3;
	openblack::statistics::PlayerFigures two {.player = openblack::PlayerNames::PLAYER_TWO};
	two.population = 10;
	two.births = 1;
	figures.players = {one, two};
	return figures;
}

struct StatisticsFixture
{
	NoopRenderer renderer;
	TextDatabase texts = StatisticsTexts();
	GameFont font = LoadFont();
	int backs {0};
	StatisticsBox box {texts, font, [this] { ++backs; }};

	StatisticsFixture() { box.Open(TwoPlayersFigures(), openblack::PlayerNames::PLAYER_ONE, false); }

	void Click(glm::ivec2 point)
	{
		auto& dialog = box.GetDialog();
		dialog.MouseMove(point);
		dialog.MouseDown(point);
		dialog.MouseUp(point);
	}
	bool Over(glm::ivec2 point)
	{
		box.GetDialog().MouseMove(point);
		return box.GetDialog().IsPointerOverControl();
	}
};

/// The middle of one of the box's tabs: six across the box from 10, 130 wide each
glm::ivec2 StatisticsTab(size_t index)
{
	return {75 + (130 * static_cast<int>(index)), 30};
}
} // namespace

TEST(StatisticsBox, IsSetUpAsTheOriginal)
{
	// The heading's band, the three graph boxes, the two line graphs and the list, as the original makes them
	EXPECT_EQ(StatisticsBox::k_Heading.min, glm::ivec2(40, 60));
	EXPECT_EQ(StatisticsBox::k_Heading.max, glm::ivec2(760, 88));
	const std::array<int, 3> left {40, 290, 540};
	for (size_t i = 0; i < left.size(); ++i)
	{
		EXPECT_EQ(StatisticsBox::k_GraphBoxes.at(i).min, glm::ivec2(left.at(i), 100));
		EXPECT_EQ(StatisticsBox::k_GraphBoxes.at(i).Width(), 220);
		EXPECT_EQ(StatisticsBox::k_GraphBoxes.at(i).Height(), 230);
	}
	EXPECT_EQ(StatisticsBox::k_Influence.min, glm::ivec2(40, 100));
	EXPECT_EQ(StatisticsBox::k_Influence.max, glm::ivec2(510, 200));
	EXPECT_EQ(StatisticsBox::k_Population.min, glm::ivec2(40, 230));
	EXPECT_EQ(StatisticsBox::k_Population.max, glm::ivec2(510, 330));
	EXPECT_EQ(StatisticsBox::k_List.min, glm::ivec2(40, 365));
	EXPECT_EQ(StatisticsBox::k_List.max, glm::ivec2(760, 515));
	// The Back arrow, 40 across, and the two check boxes along the bottom
	EXPECT_EQ(StatisticsBox::k_BackAt, glm::ivec2(20, 530));
	EXPECT_EQ(StatisticsBox::k_BackSize, 40);
	EXPECT_EQ(StatisticsBox::k_GraphicAt, glm::ivec2(290, 530));
	EXPECT_EQ(StatisticsBox::k_NumericAt, glm::ivec2(490, 530));
	// Small text for the tabs and the list, mid text for the heading
	EXPECT_EQ(StatisticsBox::k_TabTextSize, DialogPainter::k_SmallTextSize);
	EXPECT_EQ(StatisticsBox::k_ListTextSize, DialogPainter::k_SmallTextSize);
	EXPECT_EQ(StatisticsBox::k_HeadingTextSize, DialogPainter::k_MidTextSize);
	EXPECT_EQ(StatisticsBox::k_HeadingText, "HELP_TEXT_DIALOG_ADDITION_61");
	EXPECT_EQ(StatisticsBox::k_BackText, "HELP_TEXT_DIALOG_TATOOBACK");
	EXPECT_EQ(StatisticsBox::k_InfluenceText, "HELP_TEXT_PAUSE_STATS_05");
	EXPECT_EQ(StatisticsBox::k_PopulationText, "HELP_TEXT_PAUSE_STATS_06");
	EXPECT_EQ(StatisticsBox::k_GraphicText, "HELP_TEXT_PAUSE_STATS_07");
	EXPECT_EQ(StatisticsBox::k_NumericText, "HELP_TEXT_PAUSE_STATS_08");
	EXPECT_EQ(StatisticsBox::k_TotalText, "HELP_TEXT_PAUSE_STATS_03");
}

TEST(StatisticsBox, ItsControlsAnswerAtTheirPlaces)
{
	StatisticsFixture f;
	// Player Stats: the two line graphs on the left and the right hand graph box
	EXPECT_TRUE(f.Over({40, 100}));
	EXPECT_TRUE(f.Over({509, 199}));
	EXPECT_FALSE(f.Over({100, 215}));
	EXPECT_TRUE(f.Over({100, 230}));
	EXPECT_FALSE(f.Over({520, 150}));
	EXPECT_TRUE(f.Over({540, 150}));
	// Town Stats: the three graph boxes, 30 apart
	f.box.SetTab(statistics_box::Tab::Town);
	EXPECT_TRUE(f.Over({259, 329}));
	EXPECT_FALSE(f.Over({260, 200}));
	EXPECT_TRUE(f.Over({290, 200}));
	EXPECT_FALSE(f.Over({100, 99}));
	EXPECT_FALSE(f.Over({100, 330}));
	EXPECT_TRUE(f.Over({759, 329}));
	// The list below them
	EXPECT_FALSE(f.Over({100, 364}));
	EXPECT_TRUE(f.Over({100, 365}));
	EXPECT_FALSE(f.Over({39, 400}));
	// The heading takes no clicks
	EXPECT_FALSE(f.Over({400, 70}));
}

TEST(StatisticsBox, SixTabsReadTheOriginalsTextsAndOpenTheirPages)
{
	EXPECT_EQ(StatisticsBox::k_TabTexts,
	          (std::array<std::string_view, statistics_box::k_TabCount> {
	              "HELP_TEXT_DIALOG_ADDITION_88", "HELP_TEXT_DIALOG_ADDITION_111", "HELP_TEXT_DIALOG_ADDITION_46",
	              "HELP_TEXT_DIALOG_ADDITION_138", "HELP_TEXT_DIALOG_ADDITION_87", "HELP_TEXT_DIALOG_ADDITION_124"}));
	EXPECT_EQ(StatisticsBox::k_GameOverText, "HELP_TEXT_GAME_OVER");
	// A tab without its text takes no click: each tab is found by its own text
	StatisticsFixture f;
	const std::array<statistics_box::Tab, 5> pages {statistics_box::Tab::Player, statistics_box::Tab::Creature,
	                                                statistics_box::Tab::Town, statistics_box::Tab::Miracle,
	                                                statistics_box::Tab::Tech};
	for (size_t i = pages.size(); i > 0; --i)
	{
		f.Click(StatisticsTab(i));
		EXPECT_EQ(f.box.GetTab(), pages.at(i - 1));
	}
	EXPECT_EQ(f.backs, 0);
	f.Click(StatisticsTab(0));
	EXPECT_EQ(f.backs, 1);
	// Opened from the end of a game the first tab reads Game Over, and still goes back
	f.box.Open(TwoPlayersFigures(), openblack::PlayerNames::PLAYER_ONE, true);
	f.Click(StatisticsTab(0));
	EXPECT_EQ(f.backs, 2);
}

TEST(StatisticsBox, ItOpensOnPlayerStatsWithTheFiguresTakenOnce)
{
	StatisticsFixture f;
	EXPECT_EQ(f.box.GetTab(), statistics_box::Tab::Player);
	// Every tab lists the figures the box took as it opened
	const auto figures = TwoPlayersFigures();
	f.box.SetTab(statistics_box::Tab::Town);
	f.box.SetTab(statistics_box::Tab::Player);
	f.box.SetTab(statistics_box::Tab::Town);
	const auto expected = statistics_box::MakePage(statistics_box::Tab::Town, figures, openblack::PlayerNames::PLAYER_ONE);
	ASSERT_EQ(f.box.GetPage().rows.size(), expected.rows.size());
	for (size_t i = 0; i < expected.rows.size(); ++i)
	{
		EXPECT_EQ(f.box.GetPage().rows.at(i).label, expected.rows.at(i).label);
		EXPECT_EQ(f.box.GetPage().rows.at(i).values, expected.rows.at(i).values);
	}
	// Opened again, it is on Player Stats again
	f.box.Open(figures, openblack::PlayerNames::PLAYER_ONE, false);
	EXPECT_EQ(f.box.GetTab(), statistics_box::Tab::Player);
}

TEST(StatisticsBox, TheBackArrowClosesTheBox)
{
	StatisticsFixture f;
	f.Click({40, 550});
	EXPECT_EQ(f.backs, 1);
}

TEST(StatisticsBox, GraphicAndNumericWorkAsAPair)
{
	StatisticsFixture f;
	constexpr glm::ivec2 k_Graphic {300, 540};
	constexpr glm::ivec2 k_Numeric {500, 540};
	// Graphic starts ticked; a click on the ticked box keeps it ticked, a click on the other swaps them
	EXPECT_TRUE(f.box.IsGraphic());
	f.Click(k_Graphic);
	EXPECT_TRUE(f.box.IsGraphic());
	f.Click(k_Numeric);
	EXPECT_FALSE(f.box.IsGraphic());
	f.Click(k_Numeric);
	EXPECT_FALSE(f.box.IsGraphic());
	f.Click(k_Graphic);
	EXPECT_TRUE(f.box.IsGraphic());
}

TEST(StatisticsBoxLayout, AGraphsNameIsUnderItWithAShadowAndLightsUp)
{
	const auto places = statistics_box::CaptionPlacesOf({.min = {40, 100}, .max = {510, 200}});
	// The shadow one down and to the right, the name along the graph's bottom edge, a small text high
	EXPECT_EQ(places.shadow.min, glm::ivec2(41, 201));
	EXPECT_EQ(places.shadow.max, glm::ivec2(511, 221));
	EXPECT_EQ(places.text.min, glm::ivec2(40, 200));
	EXPECT_EQ(places.text.max, glm::ivec2(510, 220));
	EXPECT_EQ(statistics_box::CaptionColour(true), glm::vec4(0xC0 / 255.0f, 0x80 / 255.0f, 0x20 / 255.0f, 1.0f));
	EXPECT_EQ(statistics_box::CaptionColour(false), glm::vec4(1.0f));
}

TEST(StatisticsBoxLayout, ALineGraphWritesTheValueAtThePointer)
{
	using statistics_box::LineReadout;
	// Shares: a whole percentage, cut down
	EXPECT_EQ(LineReadout(true, 0.456f, {}), "45%");
	EXPECT_EQ(LineReadout(true, 1.0f, {}), "100%");
	// Values: two decimals up to a top of 10, whole numbers, cut down, above it
	EXPECT_EQ(LineReadout(false, 0.5f, {.lowest = 0.0f, .highest = 8.0f}), "4.00");
	EXPECT_EQ(LineReadout(false, 0.25f, {.lowest = 0.0f, .highest = 10.0f}), "2.50");
	EXPECT_EQ(LineReadout(false, 0.33f, {.lowest = 0.0f, .highest = 40.0f}), "13");
	EXPECT_EQ(LineReadout(false, 0.5f, {.lowest = 20.0f, .highest = 40.0f}), "30");
}
