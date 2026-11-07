/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The boxes that the end of a skirmish opens on this computer

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "Gui/EndGameBoxRules.h"

using namespace openblack;
using namespace openblack::end_game_box;
using openblack::game_end::Event;

namespace
{
constexpr auto k_One = PlayerNames::PLAYER_ONE;
constexpr auto k_Two = PlayerNames::PLAYER_TWO;
constexpr auto k_Three = PlayerNames::PLAYER_THREE;
} // namespace

TEST(EndGameBox, TheLocalGodOutWhileTheGameGoesOnCanWatch)
{
	State state;
	const std::vector<Event> events {{.kind = Event::Kind::GodOut, .god = k_One}};
	const auto opened = Follow(events, k_One, state);
	ASSERT_TRUE(opened.endBox.has_value());
	EXPECT_EQ(opened.endBox->kind, Kind::Lost);
	EXPECT_TRUE(opened.endBox->watch);
	EXPECT_FALSE(opened.godOut.has_value());
	EXPECT_TRUE(state.endBoxShowing);
}

TEST(EndGameBox, TheLocalGodOutAndDecidedIsLostWithNoWatching)
{
	State state;
	const std::vector<Event> events {{.kind = Event::Kind::GodOutAndDecided, .god = k_One}};
	const auto opened = Follow(events, k_One, state);
	ASSERT_TRUE(opened.endBox.has_value());
	EXPECT_EQ(opened.endBox->kind, Kind::LostAndDecided);
	EXPECT_FALSE(opened.endBox->watch);
}

TEST(EndGameBox, AnotherGodOutAndDecidedIsWon)
{
	State state;
	const std::vector<Event> events {{.kind = Event::Kind::GodOutAndDecided, .god = k_Two}};
	const auto opened = Follow(events, k_One, state);
	ASSERT_TRUE(opened.endBox.has_value());
	EXPECT_EQ(opened.endBox->kind, Kind::Won);
}

TEST(EndGameBox, APlayerWhoLostAndWatchesIsToldTheGameIsWon)
{
	State state;
	std::ignore = Follow(std::vector<Event> {{.kind = Event::Kind::GodOut, .god = k_One}}, k_One, state);
	StoppedShowing(state);
	const auto opened = Follow(std::vector<Event> {{.kind = Event::Kind::GodOutAndDecided, .god = k_Two}}, k_One, state);
	ASSERT_TRUE(opened.endBox.has_value());
	EXPECT_EQ(opened.endBox->kind, Kind::Won);
}

TEST(EndGameBox, AnotherGodOutOpensTheBoxWithOk)
{
	State state;
	const auto opened = Follow(std::vector<Event> {{.kind = Event::Kind::GodOut, .god = k_Three}}, k_One, state);
	EXPECT_FALSE(opened.endBox.has_value());
	EXPECT_EQ(opened.godOut, k_Three);
}

TEST(EndGameBox, NoBoxWithOkWhileTheEndBoxShows)
{
	State state;
	std::ignore = Follow(std::vector<Event> {{.kind = Event::Kind::GodOut, .god = k_One}}, k_One, state);
	auto opened = Follow(std::vector<Event> {{.kind = Event::Kind::GodOut, .god = k_Three}}, k_One, state);
	EXPECT_FALSE(opened.godOut.has_value());
	// once the end box no longer shows itself, the box with OK opens again
	StoppedShowing(state);
	opened = Follow(std::vector<Event> {{.kind = Event::Kind::GodOut, .god = k_Two}}, k_One, state);
	EXPECT_EQ(opened.godOut, k_Two);
}

TEST(EndGameBox, AnEndBoxClosesTheBoxWithOk)
{
	State state;
	const std::vector<Event> events {{.kind = Event::Kind::GodOut, .god = k_Three},
	                                 {.kind = Event::Kind::GodOutAndDecided, .god = k_Two}};
	const auto opened = Follow(events, k_One, state);
	EXPECT_FALSE(opened.godOut.has_value());
	ASSERT_TRUE(opened.endBox.has_value());
	EXPECT_EQ(opened.endBox->kind, Kind::Won);
}

TEST(EndGameBox, TheTitlesAreTheGamesTexts)
{
	const Titles titles {.lost = u"lost", .won = u"won", .gameOver = u"Game Over", .winnerFormat = u"%s - %s Won"};
	EXPECT_EQ(Title(Kind::Lost, false, u"", titles), u"lost");
	EXPECT_EQ(Title(Kind::LostAndDecided, false, u"", titles), u"lost");
	EXPECT_EQ(Title(Kind::Won, false, u"Ann", titles), u"won");
	// a looker-on is told who won
	EXPECT_EQ(Title(Kind::Won, true, u"Ann", titles), u"Game Over - Ann Won");
	EXPECT_TRUE(LooksOn(PlayerNames::PLAYER_FIVE));
	EXPECT_FALSE(LooksOn(k_Two));
}

TEST(EndGameBox, TheNamesOfTheGods)
{
	EXPECT_EQ(GodName(k_Two, std::nullopt), u"Player[2]");
	EXPECT_EQ(GodName(k_One, std::u16string_view(u"Ann")), u"Ann");
	EXPECT_EQ(GodName(k_Three, std::u16string_view(u"")), u"Player[3]");
	const std::array<std::u16string_view, 1> words {u"Player[2]"};
	EXPECT_EQ(Format(u"%s is out of the game.", words), u"Player[2] is out of the game.");
}
