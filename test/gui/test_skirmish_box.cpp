/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The skirmish box and its land list from the playground folder

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <Gui/Controls.h>
#include <Gui/SkirmishBox.h>
#include <Gui/SkirmishBoxRules.h>
#include <Gui/TextDatabase.h>
#include <SDL_keycode.h>
#include <gtest/gtest.h>

#include "FakeGameFont.h"

using namespace openblack::gui;
using namespace openblack::gui::test;

namespace
{
TextDatabase Texts()
{
	const std::vector<std::pair<std::u16string, std::u16string>> texts = {
	    {u"HELP_TEXT_DIALOG_ADDITION_26", u"ABA"},
	    {u"HELP_TEXT_DIALOG_BACK", u"AB"},
	    {u"HELP_TEXT_DIALOG_LOBBYSTARTGAME", u"AW"},
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

std::vector<std::u16string> Stems(const std::vector<skirmish_box::Land>& lands)
{
	std::vector<std::u16string> stems;
	std::ranges::transform(lands, std::back_inserter(stems), &skirmish_box::Land::stem);
	return stems;
}

struct BoxFixture
{
	/// Before the font, which makes its texture
	NoopRenderer renderer;
	TextDatabase texts = Texts();
	GameFont font = LoadFont();
	SkirmishBox box {texts, font};

	void Show(std::vector<std::u16string> lands)
	{
		box.Show(std::move(lands));
		// The box is now fully in view
		box.Update(1.0f);
	}

	bool Click(glm::ivec2 point)
	{
		box.MouseMove(point);
		box.MouseDown(point);
		return box.MouseUp(point);
	}

	/// A point on a land of the list, none when it does not show
	[[nodiscard]] std::optional<glm::ivec2> LandPoint(size_t index) const
	{
		const auto& list = box.GetList();
		for (int y = list.rect.min.y; y < list.rect.max.y; ++y)
		{
			if (list.ItemAt(y) == index)
			{
				return glm::ivec2((list.rect.min.x + list.GetBoxRight()) / 2, y + 2);
			}
		}
		return std::nullopt;
	}
};

constexpr glm::ivec2 k_Back = SkirmishBox::k_BackPosition + (SkirmishBox::k_ArrowSize / 2);
constexpr glm::ivec2 k_Start = SkirmishBox::k_StartPosition + (SkirmishBox::k_ArrowSize / 2);
} // namespace

TEST(SkirmishBoxRules, TheListHoldsEveryTxtFileWithAnyCase)
{
	EXPECT_TRUE(skirmish_box::IsLandFile(u"TwoGods.txt"));
	EXPECT_TRUE(skirmish_box::IsLandFile(u"Island Wars.TXT"));
	EXPECT_TRUE(skirmish_box::IsLandFile(u".txt"));
	EXPECT_FALSE(skirmish_box::IsLandFile(u"construct.txt.bak-2026-10-12"));
	EXPECT_FALSE(skirmish_box::IsLandFile(u"TwoGods.lnd"));
	EXPECT_FALSE(skirmish_box::IsLandFile(u"txt"));
}

TEST(SkirmishBoxRules, ALandIsNamedByItsFileWithoutTheLastFourCharacters)
{
	EXPECT_EQ(skirmish_box::Stem(u"Death Comes To Those That Wait.txt"), u"Death Comes To Those That Wait");
	EXPECT_EQ(skirmish_box::Stem(u"Firestorm.TXT"), u"Firestorm");
	EXPECT_EQ(skirmish_box::Stem(u".txt"), u"");
	EXPECT_EQ(skirmish_box::Stem(u"abc"), u"abc");
}

TEST(SkirmishBoxRules, TwoThreeAndFourGodsShowHowManyGodsToDefeat)
{
	EXPECT_EQ(skirmish_box::TitleTextOf(u"TwoGods"), "HELP_TEXT_LAST_LINES_01");
	EXPECT_EQ(skirmish_box::TitleTextOf(u"threegods"), "HELP_TEXT_LAST_LINES_02");
	EXPECT_EQ(skirmish_box::TitleTextOf(u"FOURGODS"), "HELP_TEXT_LAST_LINES_03");
	EXPECT_EQ(skirmish_box::TitleTextOf(u"FiveGods"), std::nullopt);
	EXPECT_EQ(skirmish_box::TitleTextOf(u"TwoGods2"), std::nullopt);
	EXPECT_EQ(skirmish_box::TitleTextOf(u"Island Wars"), std::nullopt);
}

TEST(SkirmishBoxRules, TheFolderOrderComparesTheNamesAsCapitals)
{
	EXPECT_TRUE(skirmish_box::FolderOrderLess(u"construct.txt", u"Death.txt"));
	EXPECT_FALSE(skirmish_box::FolderOrderLess(u"Death.txt", u"construct.txt"));
	// A shorter name comes before a longer name that starts with it, and a space before a full stop
	EXPECT_TRUE(skirmish_box::FolderOrderLess(u"Two", u"TwoGods"));
	EXPECT_TRUE(skirmish_box::FolderOrderLess(u"A B.txt", u"A.txt"));
	// An underscore comes after the letters, which compare as capitals
	EXPECT_TRUE(skirmish_box::FolderOrderLess(u"az.txt", u"a_.txt"));
	// Latin-1 letters compare as their capitals
	EXPECT_FALSE(skirmish_box::FolderOrderLess(u"\u00E9.txt", u"\u00C9.txt"));
	EXPECT_FALSE(skirmish_box::FolderOrderLess(u"\u00C9.txt", u"\u00E9.txt"));
	EXPECT_FALSE(skirmish_box::FolderOrderLess(u"a.txt", u"A.txt"));
}

TEST(SkirmishBoxRules, TheLandsOfTheGameComeInTheOrderOfTheFolder)
{
	// The files of the playground folder, in an order that another file system can give
	const std::vector<std::u16string> files = {
	    u"TwoGods.txt",   u"Island Wars.txt", u"construct.txt.bak-2026-10-12-tree",  u"Firestorm.txt",
	    u"construct.txt", u"ThreeGods.txt",   u"Death Comes To Those That Wait.txt", u"FourGods.txt"};
	const auto lands = skirmish_box::ListLands(files);
	EXPECT_EQ(Stems(lands), (std::vector<std::u16string> {u"construct", u"Death Comes To Those That Wait", u"Firestorm",
	                                                      u"FourGods", u"Island Wars", u"ThreeGods", u"TwoGods"}));
	EXPECT_EQ(lands.at(3).titleText, "HELP_TEXT_LAST_LINES_03");
	EXPECT_EQ(lands.at(5).titleText, "HELP_TEXT_LAST_LINES_02");
	EXPECT_EQ(lands.at(6).titleText, "HELP_TEXT_LAST_LINES_01");
	EXPECT_EQ(lands.at(1).titleText, std::nullopt);
	EXPECT_EQ(lands.at(0).fileName, u"construct.txt");
}

TEST(SkirmishBoxRules, StartShowsOnlyWithALandPicked)
{
	EXPECT_FALSE(skirmish_box::StartShows(std::nullopt, 3));
	EXPECT_TRUE(skirmish_box::StartShows(0, 3));
	EXPECT_FALSE(skirmish_box::StartShows(3, 3));
}

TEST(SkirmishBox, StartGameShowsOnceALandIsPickedAndStartsIt)
{
	BoxFixture f;
	f.Show({u"AA", u"BB", u"WW"});
	EXPECT_TRUE(f.box.IsActive());
	EXPECT_FALSE(f.box.StartShows());
	// A click on Start Game's place does nothing while it does not show
	f.Click(k_Start);
	EXPECT_EQ(f.box.TakeAnswer().choice, SkirmishBox::Choice::None);

	const auto second = f.LandPoint(1);
	ASSERT_TRUE(second.has_value());
	f.Click(*second);
	EXPECT_EQ(f.box.GetSelection(), std::optional<size_t>(1));
	EXPECT_TRUE(f.box.StartShows());
	EXPECT_TRUE(f.Click(k_Start));
	const auto answer = f.box.TakeAnswer();
	EXPECT_EQ(answer.choice, SkirmishBox::Choice::Start);
	EXPECT_EQ(answer.land, std::optional<size_t>(1));
	EXPECT_FALSE(f.box.IsActive());
	// It fades out over a fifth of a second
	EXPECT_TRUE(f.box.IsVisible());
	f.box.Update(0.25f);
	EXPECT_FALSE(f.box.IsVisible());
	// The answer comes once
	EXPECT_EQ(f.box.TakeAnswer().choice, SkirmishBox::Choice::None);
}

// Every dialog put away: the box fades out over a fifth of a second as it does when it closes, with no land started
TEST(SkirmishBox, PuttingEveryDialogAwayClosesItWithNoAnswer)
{
	BoxFixture f;
	f.Show({u"AA"});
	f.box.Select(0);
	f.box.PutAway();
	EXPECT_FALSE(f.box.IsActive());
	EXPECT_EQ(f.box.TakeAnswer().choice, SkirmishBox::Choice::None);
	EXPECT_TRUE(f.box.IsVisible());
	f.box.Update(0.25f);
	EXPECT_FALSE(f.box.IsVisible());
}

TEST(SkirmishBox, BackGoesBackWithNoLand)
{
	BoxFixture f;
	f.Show({u"AA"});
	f.box.Select(0);
	EXPECT_TRUE(f.Click(k_Back));
	const auto answer = f.box.TakeAnswer();
	EXPECT_EQ(answer.choice, SkirmishBox::Choice::Back);
	EXPECT_EQ(answer.land, std::nullopt);
}

TEST(SkirmishBox, EscapeDoesNothing)
{
	BoxFixture f;
	f.Show({u"AA"});
	f.box.KeyDown(SDLK_ESCAPE);
	EXPECT_TRUE(f.box.IsActive());
	EXPECT_EQ(f.box.TakeAnswer().choice, SkirmishBox::Choice::None);
}

TEST(SkirmishBox, TheLandPickedStaysPickedWhenTheBoxOpensAgain)
{
	BoxFixture f;
	f.Show({u"AA", u"BB", u"WW"});
	f.box.Select(2);
	f.Click(k_Back);
	std::ignore = f.box.TakeAnswer();
	f.Show({u"AA", u"BB", u"WW"});
	EXPECT_EQ(f.box.GetSelection(), std::optional<size_t>(2));
	EXPECT_TRUE(f.box.StartShows());
	// With fewer lands, a pick past the end of the list goes, and Start Game does not show
	f.Click(k_Back);
	f.Show({u"AA"});
	EXPECT_EQ(f.box.GetSelection(), std::nullopt);
	EXPECT_FALSE(f.box.StartShows());
}

TEST(SkirmishBox, TheArrowsAndTheListAreInTheBox)
{
	for (const auto& point : {k_Back, k_Start})
	{
		EXPECT_GE(point.x, SkirmishBox::k_Box.min.x);
		EXPECT_LE(point.x, SkirmishBox::k_Box.max.x);
		EXPECT_GE(point.y, SkirmishBox::k_Box.min.y);
		EXPECT_LE(point.y, SkirmishBox::k_Box.max.y);
	}
	EXPECT_GE(SkirmishBox::k_List.min.y, SkirmishBox::k_Title.max.y);
	EXPECT_LE(SkirmishBox::k_List.max.y, SkirmishBox::k_BackPosition.y);
}
