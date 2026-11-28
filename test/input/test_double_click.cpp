/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <SDL_events.h>
#include <SDL_mouse.h>
#include <gtest/gtest.h>

#include "Input/GameActionMap.h"
#include "Input/KeyBindings.h"
#include "Input/KeyDoubleClick.h"

using namespace openblack::input;

namespace
{

[[nodiscard]] SDL_Event ButtonEvent(bool down, uint8_t button, uint8_t clicks)
{
	SDL_Event event {};
	event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.button = button;
	event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.button.clicks = clicks;
	return event;
}

/// A click, then the second press of the double click, each let go of a frame later
void DoubleClick(GameActionMap& map, uint8_t button)
{
	map.ProcessEvent(ButtonEvent(true, button, 1));
	map.Frame();
	map.ProcessEvent(ButtonEvent(false, button, 1));
	map.Frame();
	map.ProcessEvent(ButtonEvent(true, button, 2));
	map.Frame();
	map.ProcessEvent(ButtonEvent(false, button, 2));
	map.Frame();
}

} // namespace

TEST(DoubleClick, SecondPressMarksItUntilTaken)
{
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 1));
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_LEFT, 1));
	EXPECT_FALSE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 2));
	EXPECT_TRUE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_TRUE(map.TakeDoubleClick());
	EXPECT_FALSE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_FALSE(map.TakeDoubleClick());
}

TEST(DoubleClick, LettingGoLeavesItWaiting)
{
	GameActionMap map;
	map.Frame();
	DoubleClick(map, SDL_BUTTON_LEFT);
	// However many frames pass, and whatever the button does, it waits for the camera
	for (int frame = 0; frame < 100; ++frame)
	{
		map.Frame();
	}
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 1));
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_LEFT, 1));
	EXPECT_TRUE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_TRUE(map.TakeDoubleClick());
	EXPECT_FALSE(map.TakeDoubleClick());
}

TEST(DoubleClick, LostLettingGoChangesNothing)
{
	// The second press's letting go may never come, as when it went to a window over the game: the double click is
	// taken all the same, and nothing stays held
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 1));
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_LEFT, 1));
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 2));
	map.Frame();
	EXPECT_TRUE(map.TakeDoubleClick());
	map.Frame();
	EXPECT_FALSE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_FALSE(map.Get(BindableActionMap::MOVE));
}

TEST(DoubleClick, SecondPressPressesNoActions)
{
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 1));
	EXPECT_TRUE(map.Get(BindableActionMap::MOVE));
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_LEFT, 1));
	EXPECT_FALSE(map.Get(BindableActionMap::MOVE));
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 2));
	EXPECT_FALSE(map.Get(BindableActionMap::MOVE));
}

TEST(DoubleClick, OnlyTheLeftButtonDoubleClicks)
{
	GameActionMap map;
	map.Frame();
	DoubleClick(map, SDL_BUTTON_RIGHT);
	DoubleClick(map, SDL_BUTTON_MIDDLE);
	EXPECT_FALSE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_FALSE(map.TakeDoubleClick());
}

TEST(DoubleClick, AQuickRowsFourthPressIsADoubleClickAgain)
{
	// The system pairs a quick row's presses two by two: the third press is a plain press, the fourth a double click
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 1));
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_LEFT, 1));
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 2));
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_LEFT, 2));
	EXPECT_TRUE(map.TakeDoubleClick());
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 3));
	EXPECT_TRUE(map.Get(BindableActionMap::MOVE));
	EXPECT_FALSE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_LEFT, 3));
	EXPECT_FALSE(map.Get(BindableActionMap::MOVE));
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 4));
	EXPECT_FALSE(map.Get(BindableActionMap::MOVE));
	EXPECT_TRUE(map.TakeDoubleClick());
	EXPECT_EQ(map.GetLastDoubleClickPress(), 4);
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_LEFT, 4));
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 5));
	EXPECT_TRUE(map.Get(BindableActionMap::MOVE));
	EXPECT_FALSE(map.TakeDoubleClick());
}

TEST(DoubleClick, TheMiddleButtonsSecondPressPressesNoActions)
{
	// The window gives the middle button's second press of a double click as nothing: the camera does not turn for it
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_MIDDLE, 1));
	EXPECT_TRUE(map.Get(BindableActionMap::ROTATE_AROUND_MOUSE_ON));
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_MIDDLE, 1));
	EXPECT_FALSE(map.Get(BindableActionMap::ROTATE_AROUND_MOUSE_ON));
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_MIDDLE, 2));
	EXPECT_FALSE(map.Get(BindableActionMap::ROTATE_AROUND_MOUSE_ON));
	EXPECT_FALSE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_MIDDLE, 2));
	// the row's third press turns it again
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_MIDDLE, 3));
	EXPECT_TRUE(map.Get(BindableActionMap::ROTATE_AROUND_MOUSE_ON));
}

namespace
{
[[nodiscard]] SDL_Event KeyDown(SDL_Scancode key, uint8_t repeat = 0)
{
	SDL_Event event {};
	event.type = SDL_KEYDOWN;
	event.key.keysym.scancode = key;
	event.key.repeat = repeat;
	return event;
}

[[nodiscard]] SDL_Event KeyUp(SDL_Scancode key)
{
	SDL_Event event {};
	event.type = SDL_KEYUP;
	event.key.keysym.scancode = key;
	return event;
}
} // namespace

TEST(KeyDoubleClick, ASecondPressWithinAQuarterOfASecondIsADoubleClickGivenAtTheNextPass)
{
	key_double_click::State state;
	EXPECT_FALSE(key_double_click::Pass(state, false, 1000));
	EXPECT_FALSE(key_double_click::Pass(state, true, 2000));
	EXPECT_FALSE(key_double_click::Pass(state, true, 2249));
	EXPECT_TRUE(key_double_click::Pass(state, false, 2266));
	EXPECT_FALSE(key_double_click::Pass(state, false, 2283));
}

TEST(KeyDoubleClick, ASecondPressAtOrOverAQuarterOfASecondIsNone)
{
	key_double_click::State state;
	EXPECT_FALSE(key_double_click::Pass(state, false, 1000));
	EXPECT_FALSE(key_double_click::Pass(state, true, 2000));
	EXPECT_FALSE(key_double_click::Pass(state, true, 2250));
	EXPECT_FALSE(key_double_click::Pass(state, false, 2266));
	// timed from the press just before: 2250's
	EXPECT_FALSE(key_double_click::Pass(state, true, 2600));
	EXPECT_FALSE(key_double_click::Pass(state, false, 2616));
}

TEST(KeyDoubleClick, EveryPressRestartsTheTiming)
{
	// three quick presses: the second and the third are both double clicks, each given at the pass after it
	key_double_click::State state;
	EXPECT_FALSE(key_double_click::Pass(state, false, 1000));
	EXPECT_FALSE(key_double_click::Pass(state, true, 2000));
	EXPECT_FALSE(key_double_click::Pass(state, true, 2200));
	EXPECT_TRUE(key_double_click::Pass(state, true, 2400));
	EXPECT_TRUE(key_double_click::Pass(state, false, 2416));
}

TEST(KeyDoubleClick, TheFirstPassStartsTheTimingAndTheClockMayWrap)
{
	// a press at the first pass comes no time after the timing's start: a double click
	key_double_click::State first;
	EXPECT_FALSE(key_double_click::Pass(first, true, 5000));
	EXPECT_TRUE(key_double_click::Pass(first, false, 5016));
	// the wait is taken across the clock's wrap
	key_double_click::State state;
	EXPECT_FALSE(key_double_click::Pass(state, false, 0xFFFFFE00U));
	EXPECT_FALSE(key_double_click::Pass(state, true, 0xFFFFFFA0U));
	EXPECT_FALSE(key_double_click::Pass(state, true, 0x40U));
	EXPECT_TRUE(key_double_click::Pass(state, false, 0x50U));
}

TEST(BindingKey, OnlyTheKeyBoundToTheActionHoldsItWhileItIsDown)
{
	GameActionMap map;
	map.SetKeyBinding(BindableActionMap::ACTION, KeyChord {SDL_SCANCODE_K});
	map.Frame();
	EXPECT_FALSE(map.IsBindingKeyHeld(BindableActionMap::ACTION));
	map.ProcessEvent(KeyDown(SDL_SCANCODE_J));
	EXPECT_FALSE(map.IsBindingKeyHeld(BindableActionMap::ACTION));
	map.ProcessEvent(KeyDown(SDL_SCANCODE_K));
	EXPECT_TRUE(map.IsBindingKeyHeld(BindableActionMap::ACTION));
	EXPECT_FALSE(map.IsBindingKeyHeld(BindableActionMap::MOVE));
	// a held key's repeats keep it held
	map.ProcessEvent(KeyDown(SDL_SCANCODE_K, 1));
	EXPECT_TRUE(map.IsBindingKeyHeld(BindableActionMap::ACTION));
	map.ProcessEvent(KeyUp(SDL_SCANCODE_K));
	EXPECT_FALSE(map.IsBindingKeyHeld(BindableActionMap::ACTION));
}

TEST(BindingKey, AKeyWithAModifierIsHeldOnlyWithTheModifierOnEitherSide)
{
	GameActionMap map;
	map.SetKeyBinding(BindableActionMap::MOVE, KeyChord {SDL_SCANCODE_M, Modifier::Ctrl});
	map.Frame();
	map.ProcessEvent(KeyDown(SDL_SCANCODE_M));
	EXPECT_FALSE(map.IsBindingKeyHeld(BindableActionMap::MOVE));
	map.ProcessEvent(KeyDown(SDL_SCANCODE_RCTRL));
	EXPECT_TRUE(map.IsBindingKeyHeld(BindableActionMap::MOVE));
	map.ProcessEvent(KeyUp(SDL_SCANCODE_RCTRL));
	EXPECT_FALSE(map.IsBindingKeyHeld(BindableActionMap::MOVE));
}

TEST(BindingKey, AKeyWithoutAModifierIgnoresTheModifiersHeld)
{
	const auto down = [](SDL_Scancode key) { return key == SDL_SCANCODE_K || key == SDL_SCANCODE_LSHIFT; };
	EXPECT_TRUE(KeyHeldFor(KeyChord {SDL_SCANCODE_K}, down));
	EXPECT_TRUE(KeyHeldFor(KeyChord {SDL_SCANCODE_K, Modifier::Shift}, down));
	EXPECT_FALSE(KeyHeldFor(KeyChord {SDL_SCANCODE_K, Modifier::Alt}, down));
	// a modifier key bound alone is either side's
	EXPECT_TRUE(KeyHeldFor(KeyChord {SDL_SCANCODE_LSHIFT}, [](SDL_Scancode key) { return key == SDL_SCANCODE_RSHIFT; }));
	EXPECT_FALSE(KeyHeldFor(std::nullopt, down));
}

TEST(BindingKey, TheDefaultBindingsHoldNoKeyAndGiveTheHandTheLeftAndTheRightButtons)
{
	// by default the Move and the Action are the left and the right buttons, and have no key
	GameActionMap map;
	map.Frame();
	DoubleClick(map, SDL_BUTTON_RIGHT);
	EXPECT_FALSE(map.IsBindingKeyHeld(BindableActionMap::ACTION));
	EXPECT_FALSE(map.IsBindingKeyHeld(BindableActionMap::MOVE));
	EXPECT_FALSE(map.TakeDoubleClick());
	const auto buttons = HandMouseBindingsOf(map.GetKeyBindings());
	EXPECT_EQ(buttons.move, MouseButton::Left);
	EXPECT_EQ(buttons.action, MouseButton::Right);
}

TEST(BindingKey, TheHandTakesTheButtonsTheControlsPageBinds)
{
	GameActionMap map;
	map.SetMouseBinding(BindableActionMap::ACTION, MouseInput::MiddleButton);
	map.SetMouseBinding(BindableActionMap::MOVE, MouseInput::WheelUp);
	const auto buttons = HandMouseBindingsOf(map.GetKeyBindings());
	EXPECT_EQ(buttons.move, std::nullopt);
	EXPECT_EQ(buttons.action, MouseButton::Middle);
}

TEST(KeyDoubleClick, TheMovesSecondPressIsADoubleClickGivenAtOnceAndNotLatched)
{
	key_double_click::State state;
	EXPECT_FALSE(key_double_click::Press(state, false, 1000));
	EXPECT_FALSE(key_double_click::Press(state, true, 2000));
	EXPECT_TRUE(key_double_click::Press(state, true, 2249));
	EXPECT_FALSE(key_double_click::Press(state, false, 2266));
	EXPECT_FALSE(key_double_click::Press(state, true, 2600));
	// every press restarts the timing, as the Action's
	EXPECT_TRUE(key_double_click::Press(state, true, 2800));
	EXPECT_TRUE(key_double_click::Press(state, true, 3000));
}

TEST(KeyDoubleClick, AMarkedDoubleClickWaitsForTheCameraAsTheButtonsDoes)
{
	GameActionMap map;
	map.Frame();
	map.MarkDoubleClick();
	map.Frame();
	EXPECT_TRUE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	map.Frame();
	EXPECT_TRUE(map.TakeDoubleClick());
	EXPECT_FALSE(map.TakeDoubleClick());
}

TEST(DoubleClick, TheButtonBoundToTheMoveMarksTheDoubleClick)
{
	// the Move on the right button: its double click is the camera's, and the left's is no longer
	GameActionMap map;
	map.SetMouseBinding(BindableActionMap::MOVE, MouseInput::RightButton);
	map.SetMouseBinding(BindableActionMap::ACTION, MouseInput::LeftButton);
	map.Frame();
	DoubleClick(map, SDL_BUTTON_LEFT);
	EXPECT_FALSE(map.TakeDoubleClick());
	DoubleClick(map, SDL_BUTTON_RIGHT);
	EXPECT_TRUE(map.TakeDoubleClick());
	// the left's second press still presses nothing
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 1));
	map.ProcessEvent(ButtonEvent(false, SDL_BUTTON_LEFT, 1));
	map.Frame();
	map.ProcessEvent(ButtonEvent(true, SDL_BUTTON_LEFT, 2));
	EXPECT_FALSE(map.Get(BindableActionMap::ACTION));
}
