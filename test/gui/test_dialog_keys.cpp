/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <SDL_keycode.h>
#include <gtest/gtest.h>

#include "Gui/Dialog.h"
#include "Gui/DialogKeyRules.h"

using namespace openblack::gui;
using namespace openblack::gui::dialog_keys;

namespace
{
/// A control that, like an edit box, takes Return as a click and passes the focus on
class ReturnPasser final: public Control
{
public:
	using Control::Control;
	void Draw(const DialogPainter& /*painter*/, bool /*hovered*/, bool /*focused*/, bool /*pressed*/) const override {}
	[[nodiscard]] bool PassesFocusOnReturn() const override { return true; }
};
} // namespace

TEST(DialogKeys, TabGoesToTheNextControlThatCanTakeTheFocusAndWraps)
{
	const std::array<bool, 4> canFocus {false, true, false, true};
	EXPECT_EQ(NextFocus(canFocus, std::nullopt, false), 1u);
	EXPECT_EQ(NextFocus(canFocus, 1u, false), 3u);
	EXPECT_EQ(NextFocus(canFocus, 3u, false), 1u);
	EXPECT_EQ(NextFocus(canFocus, std::nullopt, true), 3u);
	EXPECT_EQ(NextFocus(canFocus, 3u, true), 1u);
	EXPECT_EQ(NextFocus(canFocus, 1u, true), 3u);
	// A static text with the focus from a click gives it on
	EXPECT_EQ(NextFocus(canFocus, 0u, false), 1u);
}

TEST(DialogKeys, TheFocusStaysWhenNoOtherControlCanTakeIt)
{
	const std::array<bool, 3> one {false, true, false};
	EXPECT_EQ(NextFocus(one, 1u, false), 1u);
	const std::array<bool, 2> none {false, false};
	EXPECT_EQ(NextFocus(none, 0u, false), 0u);
	// With no focus and no control to take it, the game never stops looking: openblack gives no focus
	EXPECT_EQ(NextFocus(none, std::nullopt, false), std::nullopt);
}

TEST(DialogKeys, SliderKeysMoveATenthOrToAnEnd)
{
	EXPECT_FLOAT_EQ(MoveSlider(0.5f, SliderKey::Left), 0.5f - 0.1f);
	EXPECT_FLOAT_EQ(MoveSlider(0.5f, SliderKey::Right), 0.5f + 0.1f);
	EXPECT_EQ(MoveSlider(0.05f, SliderKey::Left), 0.0f);
	EXPECT_EQ(MoveSlider(0.95f, SliderKey::Right), 1.0f);
	EXPECT_EQ(MoveSlider(0.5f, SliderKey::Home), 0.0f);
	EXPECT_EQ(MoveSlider(0.5f, SliderKey::End), 1.0f);
}

TEST(DialogKeys, ListKeysPickAnotherItemAndWrap)
{
	EXPECT_EQ(MoveList(std::nullopt, 3, ListKey::Down), 0u);
	EXPECT_EQ(MoveList(std::nullopt, 3, ListKey::Up), 2u);
	EXPECT_EQ(MoveList(0u, 3, ListKey::Up), 2u);
	EXPECT_EQ(MoveList(2u, 3, ListKey::Down), 0u);
	EXPECT_EQ(MoveList(1u, 3, ListKey::Down), 2u);
	EXPECT_EQ(MoveList(1u, 3, ListKey::Home), 0u);
	EXPECT_EQ(MoveList(1u, 3, ListKey::End), 2u);
	EXPECT_EQ(MoveList(std::nullopt, 0, ListKey::Home), std::nullopt);
}

TEST(DialogKeys, TabSkipsStaticTextAndHiddenControlsAndComesToTheMenusTabsLast)
{
	Dialog dialog({{.label = u"Main", .onSelect = {}}, {.label = u"Stats", .onSelect = {}}}, 0);
	dialog.Add<StaticText>(DialogRect {.min = {0, 60}, .max = {100, 80}}, u"Hello", StaticText::Layout::Left);
	auto& first = dialog.Add<Button>(DialogRect {.min = {0, 100}, .max = {100, 140}}, u"One");
	auto& hidden = dialog.Add<Button>(DialogRect {.min = {0, 150}, .max = {100, 190}}, u"Two");
	hidden.visible = false;
	auto& slider = dialog.Add<Slider>(DialogRect {.min = {0, 200}, .max = {300, 230}}, u"Three", 0.5f);

	EXPECT_TRUE(dialog.KeyDown(SDLK_TAB));
	EXPECT_EQ(dialog.GetFocus(), &first);
	dialog.KeyDown(SDLK_TAB);
	EXPECT_EQ(dialog.GetFocus(), &slider);
	// Then the five tabs, those without a label too, and back to the first control
	for (int i = 0; i < 5; ++i)
	{
		dialog.KeyDown(SDLK_TAB);
		EXPECT_NE(dialog.GetFocus(), &first);
		EXPECT_NE(dialog.GetFocus(), &slider);
	}
	dialog.KeyDown(SDLK_TAB);
	EXPECT_EQ(dialog.GetFocus(), &first);
	// Shift goes back, round over the five tabs to the last control
	for (int i = 0; i < 5; ++i)
	{
		dialog.KeyDown(SDLK_TAB, true);
		EXPECT_NE(dialog.GetFocus(), &slider);
	}
	dialog.KeyDown(SDLK_TAB, true);
	EXPECT_EQ(dialog.GetFocus(), &slider);
}

TEST(DialogKeys, ADialogWithoutTheMenusTabsGoesInTheOrderItsControlsWereMade)
{
	Dialog dialog({{.label = u"One", .onSelect = {}}}, 0, DialogRect {.min = {0, 100}, .max = {400, 400}});
	auto& button = dialog.Add<Button>(DialogRect {.min = {0, 100}, .max = {100, 140}}, u"OK");
	dialog.KeyDown(SDLK_TAB);
	EXPECT_NE(dialog.GetFocus(), &button);
	dialog.KeyDown(SDLK_TAB);
	EXPECT_EQ(dialog.GetFocus(), &button);
}

TEST(DialogKeys, TabWithNothingToFocusGivesNoFocus)
{
	Dialog dialog;
	dialog.Add<StaticText>(DialogRect {.min = {0, 60}, .max = {100, 80}}, u"Hello", StaticText::Layout::Left);
	EXPECT_TRUE(dialog.KeyDown(SDLK_TAB));
	EXPECT_EQ(dialog.GetFocus(), nullptr);
}

TEST(DialogKeys, TheFocusedSliderTakesTheArrowsAndTellsOfItsValue)
{
	Dialog dialog;
	auto& slider = dialog.Add<Slider>(DialogRect {.min = {0, 200}, .max = {300, 230}}, u"Volume", 0.5f);
	int told = 0;
	slider.onChange = [&told](float /*value*/) { ++told; };
	// Without the focus the slider takes no key
	EXPECT_FALSE(dialog.KeyDown(SDLK_RIGHT));
	dialog.KeyDown(SDLK_TAB);
	EXPECT_TRUE(dialog.KeyDown(SDLK_RIGHT));
	EXPECT_FLOAT_EQ(slider.GetValue(), 0.5f + 0.1f);
	EXPECT_TRUE(dialog.KeyDown(SDLK_END));
	EXPECT_EQ(slider.GetValue(), 1.0f);
	// At the end it still tells
	EXPECT_TRUE(dialog.KeyDown(SDLK_END));
	EXPECT_EQ(told, 3);
	EXPECT_FALSE(dialog.KeyDown(SDLK_UP));
}

TEST(DialogKeys, ReturnInAnEditBoxClicksItAndMovesTheFocusOn)
{
	Dialog dialog;
	auto& edit = dialog.Add<ReturnPasser>(DialogRect {.min = {0, 100}, .max = {100, 140}});
	auto& button = dialog.Add<Button>(DialogRect {.min = {0, 150}, .max = {100, 190}}, u"OK");
	int clicks = 0;
	edit.onClick = [&clicks]() { ++clicks; };
	button.onClick = [&clicks]() { clicks += 10; };
	dialog.SetFocus(&edit);
	EXPECT_TRUE(dialog.KeyDown(SDLK_RETURN));
	EXPECT_EQ(clicks, 1);
	EXPECT_EQ(dialog.GetFocus(), &button);
	// A button with the focus takes no Return of its own
	EXPECT_FALSE(dialog.KeyDown(SDLK_RETURN));
	EXPECT_EQ(clicks, 1);
}
