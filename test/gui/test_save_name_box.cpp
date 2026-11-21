/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The save game room's name box, which asks for the name of a save

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <Gui/SaveNameBox.h>
#include <Gui/SaveNameBoxRules.h>
#include <Gui/TextDatabase.h>
#include <SDL_keycode.h>
#include <gtest/gtest.h>

#include "FakeGameFont.h"

using namespace openblack::gui;
using namespace openblack::gui::test;

namespace
{
TextDatabase SaveNameTexts()
{
	std::u16string script;
	const std::vector<std::pair<std::u16string, std::u16string>> texts = {
	    {u"HELP_TEXT_NONE", u"BAD"},
	    {u"HELP_TEXT_ROOM_CITADEL_SAVEGAME_ROOM_SAVEMESSAGE", u"AB"},
	    {u"HELP_TEXT_REQUESTER_BOXES_03", u"AB"},
	    {u"HELP_TEXT_REQUESTER_BOXES_04", u"AW"},
	};
	for (const auto& [name, text] : texts)
	{
		script.append(u"ADD_TEXT(1, N, \"").append(name).append(u"\", \"").append(text).append(u"\")\n");
	}
	TextDatabase database;
	database.AddScript(Utf16Script(script));
	return database;
}

struct SaveNameFixture
{
	/// Before the font, which makes its texture
	NoopRenderer renderer;
	TextDatabase texts = SaveNameTexts();
	GameFont font = LoadFont();
	SaveNameBox box {texts, font};

	explicit SaveNameFixture(std::u16string_view name = u"")
	{
		box.Show(name);
		// Faded in
		box.Update(1.0f);
	}

	bool Click(glm::ivec2 point)
	{
		box.MouseMove(point);
		box.MouseDown(point);
		return box.MouseUp(point);
	}
};

constexpr glm::ivec2 k_Ok = (SaveNameBox::k_Ok.min + SaveNameBox::k_Ok.max) / 2;
constexpr glm::ivec2 k_Cancel = (SaveNameBox::k_Cancel.min + SaveNameBox::k_Cancel.max) / 2;
} // namespace

TEST(SaveNameBox, ReturnInTheNameGoesToOkAndReturnElsewhereIsOk)
{
	using save_name_box::KeyStep;
	using save_name_box::StepOfKey;
	EXPECT_EQ(StepOfKey(true, false, true), KeyStep::FocusOk);
	EXPECT_EQ(StepOfKey(true, false, false), KeyStep::Save);
	EXPECT_EQ(StepOfKey(false, true, true), KeyStep::Cancel);
	EXPECT_EQ(StepOfKey(false, true, false), KeyStep::Cancel);
	EXPECT_EQ(StepOfKey(false, false, true), KeyStep::ToControl);
}

TEST(SaveNameBox, ANewSaveStartsEmptyAndOkGivesTheNameTyped)
{
	SaveNameFixture f;
	EXPECT_TRUE(f.box.IsActive());
	EXPECT_EQ(f.box.GetName(), u"");
	// The name's box has the focus as the box comes up: typing goes straight in
	f.box.TextInput(u"Ann");
	EXPECT_TRUE(f.Click(k_Ok));
	const auto answer = f.box.TakeAnswer();
	ASSERT_TRUE(answer.has_value());
	ASSERT_TRUE(answer->has_value());
	EXPECT_EQ(**answer, u"Ann");
	EXPECT_FALSE(f.box.IsActive());
	EXPECT_FALSE(f.box.TakeAnswer().has_value());
	// It fades out over a fifth of a second
	EXPECT_TRUE(f.box.IsVisible());
	f.box.Update(0.25f);
	EXPECT_FALSE(f.box.IsVisible());
}

TEST(SaveNameBox, ASaveOverAnotherStartsWithItsNameAndTakesThirtyTwoCharacters)
{
	{
		SaveNameFixture f(u"Old");
		EXPECT_EQ(f.box.GetName(), u"Old");
	}
	// One box at a time: the renderer of nothing starts once
	SaveNameFixture longer(std::u16string(40, u'x'));
	EXPECT_EQ(longer.box.GetName().size(), 32u);
}

TEST(SaveNameBox, CancelEscapeAndPuttingAwaySaveNothing)
{
	{
		SaveNameFixture f(u"Old");
		EXPECT_TRUE(f.Click(k_Cancel));
		const auto answer = f.box.TakeAnswer();
		ASSERT_TRUE(answer.has_value());
		EXPECT_FALSE(answer->has_value());
	}
	{
		SaveNameFixture f(u"Old");
		f.box.KeyDown(SDLK_ESCAPE, false);
		const auto answer = f.box.TakeAnswer();
		ASSERT_TRUE(answer.has_value());
		EXPECT_FALSE(answer->has_value());
	}
	{
		SaveNameFixture f(u"Old");
		f.box.PutAway();
		const auto answer = f.box.TakeAnswer();
		ASSERT_TRUE(answer.has_value());
		EXPECT_FALSE(answer->has_value());
	}
}

TEST(SaveNameBox, ReturnTwiceSavesUnderTheName)
{
	SaveNameFixture f(u"Old");
	f.box.KeyDown(SDLK_RETURN, false);
	EXPECT_TRUE(f.box.IsActive());
	EXPECT_FALSE(f.box.TakeAnswer().has_value());
	f.box.KeyDown(SDLK_RETURN, false);
	const auto answer = f.box.TakeAnswer();
	ASSERT_TRUE(answer.has_value());
	ASSERT_TRUE(answer->has_value());
	EXPECT_EQ(**answer, u"Old");
}
