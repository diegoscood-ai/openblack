/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The mouse buttons and motion the hand reads (Input/MouseButtons), with fake button events: what Game::ProcessEvents
// and Game::Update did with their function statics. Then fake SDL events and frames through the game's routing: the
// events a box (the escape menu, the SkipBox) or a debug window takes, and the interface's frame pass

#include <cstdint>

#include <optional>
#include <vector>

#include <SDL_events.h>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Hand/HandDemo.h"
#include "Input/DoubleClickBox.h"
#include "Input/MouseButtons.h"

using namespace openblack::input;

namespace
{
MouseButtonEvent Down(MouseButton button, glm::ivec2 position = {0, 0})
{
	return {.button = button, .down = true, .position = position};
}

MouseButtonEvent Up(MouseButton button, glm::ivec2 position = {0, 0})
{
	return {.button = button, .down = false, .position = position};
}

SDL_Event SdlButton(uint32_t type, uint8_t button, int x = 0, int y = 0)
{
	SDL_Event event {};
	event.type = type;
	event.button.type = type;
	event.button.button = button;
	event.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
	event.button.x = x;
	event.button.y = y;
	return event;
}

SDL_Event SdlDown(uint8_t button, int x = 0, int y = 0)
{
	return SdlButton(SDL_MOUSEBUTTONDOWN, button, x, y);
}

SDL_Event SdlUp(uint8_t button, int x = 0, int y = 0)
{
	return SdlButton(SDL_MOUSEBUTTONUP, button, x, y);
}

/// A button's down or up as SDL counts its clicks: 2 on the second click of a double click
SDL_Event SdlClicks(uint32_t type, uint8_t button, uint8_t clicks)
{
	auto event = SdlButton(type, button);
	event.button.clicks = clicks;
	return event;
}

SDL_Event SdlKey(SDL_Keycode key)
{
	SDL_Event event {};
	event.type = SDL_KEYDOWN;
	event.key.type = SDL_KEYDOWN;
	event.key.keysym.sym = key;
	return event;
}

// An SDL mouse button event as Game.cpp reads it for the hand
std::optional<MouseButtonEvent> AsButton(const SDL_Event& event)
{
	if (event.type != SDL_MOUSEBUTTONDOWN && event.type != SDL_MOUSEBUTTONUP)
	{
		return std::nullopt;
	}
	return MouseButtonEvent {
	    .button = event.button.button == SDL_BUTTON_LEFT     ? MouseButton::Left
	              : event.button.button == SDL_BUTTON_MIDDLE ? MouseButton::Middle
	              : event.button.button == SDL_BUTTON_RIGHT  ? MouseButton::Right
	                                                         : MouseButton::Other,
	    .down = event.type == SDL_MOUSEBUTTONDOWN,
	    .position = {event.button.x, event.button.y},
	    .doubleClick = double_click::IsDoubleClick(event.button.clicks),
	};
}

// Who an event goes to in the game's event handler
enum class To : uint8_t
{
	Game,
	Menu,
	DebugWindow,
};

// One event through the game's event handler, as the hand's buttons see it: true when it sent a press or a release
bool Route(MouseButtonsState& state, const SDL_Event& event, To to)
{
	const auto button = AsButton(event);
	if (!button)
	{
		return false;
	}
	switch (to)
	{
	case To::Game:
		return ApplyMouseButton(state, *button);
	case To::Menu:
		return false; // the menu takes it: nothing reaches the hand's buttons
	case To::DebugWindow:
		DebugWindowTookButton(state, *button);
		return false;
	}
	return false;
}

// What the hand reads after a frame: the frame pass's buttons
struct Frame
{
	bool box;
	bool gripping;
	bool action;
};

Frame Pass(MouseButtonsState& state, bool menuUp)
{
	const bool box = BoxTakesInput(state, menuUp);
	return {.box = box, .gripping = HandGripping(state), .action = HandAction(state)};
}
} // namespace

TEST(MouseButtons, startsWithNothingHeld)
{
	const MouseButtonsState state;
	EXPECT_FALSE(HandGripping(state));
	EXPECT_FALSE(HandAction(state));
	EXPECT_EQ(state.middlePressPosition, glm::ivec2(0, 0));
}

TEST(MouseButtons, leftPressAndReleaseGripsThenLetsGo)
{
	MouseButtonsState state;
	ApplyMouseButton(state, Down(MouseButton::Left));
	EXPECT_TRUE(state.left);
	EXPECT_TRUE(HandGripping(state));
	ApplyMouseButton(state, Up(MouseButton::Left));
	EXPECT_FALSE(state.left);
	EXPECT_FALSE(HandGripping(state));
}

TEST(MouseButtons, aDownWhileItsBindingIsPressedSendsNothing)
{
	// two downs in a row (an up lost while the window had no focus): the second sends no press, the next up releases
	MouseButtonsState state;
	EXPECT_TRUE(ApplyMouseButton(state, Down(MouseButton::Left)));
	EXPECT_FALSE(ApplyMouseButton(state, Down(MouseButton::Left)));
	EXPECT_TRUE(state.left);
	EXPECT_TRUE(ApplyMouseButton(state, Up(MouseButton::Left)));
	EXPECT_FALSE(state.left);
	// an up without its press sends nothing: it does not hold the button
	EXPECT_FALSE(ApplyMouseButton(state, Up(MouseButton::Left)));
	EXPECT_FALSE(state.left);
}

TEST(MouseButtons, middleFlipsAndKeepsWhereItWentDown)
{
	MouseButtonsState state;
	ApplyMouseButton(state, Down(MouseButton::Middle, {120, 340}));
	EXPECT_TRUE(state.middle);
	EXPECT_FALSE(HandGripping(state));
	EXPECT_EQ(state.middlePressPosition, glm::ivec2(120, 340));
	// the release does not move the kept position
	ApplyMouseButton(state, Up(MouseButton::Middle, {500, 20}));
	EXPECT_FALSE(state.middle);
	EXPECT_EQ(state.middlePressPosition, glm::ivec2(120, 340));
	// a second down with no up between sends nothing, so it keeps the first one's position
	ApplyMouseButton(state, Down(MouseButton::Middle, {1, 2}));
	ApplyMouseButton(state, Down(MouseButton::Middle, {3, 4}));
	EXPECT_TRUE(state.middle);
	EXPECT_EQ(state.middlePressPosition, glm::ivec2(1, 2));
}

TEST(MouseButtons, rightIsHeldWhileDown)
{
	MouseButtonsState state;
	ApplyMouseButton(state, Down(MouseButton::Right));
	EXPECT_TRUE(HandAction(state));
	ApplyMouseButton(state, Down(MouseButton::Right));
	EXPECT_TRUE(HandAction(state));
	ApplyMouseButton(state, Up(MouseButton::Right));
	EXPECT_FALSE(HandAction(state));
	ApplyMouseButton(state, Up(MouseButton::Right));
	EXPECT_FALSE(HandAction(state));
	EXPECT_FALSE(HandGripping(state));
}

TEST(MouseButtons, theGripIsTheLeftButtonAloneAndTheMiddleButtonIsNoGrip)
{
	// the middle button turns the camera only: held alone, or kept after the left one is let go, it grips nothing, so
	// the hand keeps its normal hover while the camera turns
	MouseButtonsState state;
	ApplyMouseButton(state, Down(MouseButton::Middle));
	EXPECT_FALSE(HandGripping(state));
	ApplyMouseButton(state, Down(MouseButton::Left));
	EXPECT_TRUE(HandGripping(state));
	ApplyMouseButton(state, Up(MouseButton::Left));
	EXPECT_FALSE(HandGripping(state));
	ApplyMouseButton(state, Up(MouseButton::Middle));
	EXPECT_FALSE(HandGripping(state));
}

TEST(MouseButtons, otherButtonsChangeNothing)
{
	MouseButtonsState state;
	ApplyMouseButton(state, Down(MouseButton::Middle, {7, 8}));
	ApplyMouseButton(state, Down(MouseButton::Other, {9, 9}));
	ApplyMouseButton(state, Up(MouseButton::Other, {9, 9}));
	EXPECT_FALSE(state.left);
	EXPECT_TRUE(state.middle);
	EXPECT_FALSE(state.right);
	EXPECT_EQ(state.middlePressPosition, glm::ivec2(7, 8));
	EXPECT_FALSE(ApplyMouseButton(state, Down(MouseButton::Other)));
}

TEST(BoxTakesInput, noBoxChangesNothing)
{
	MouseButtonsState state;
	Route(state, SdlDown(SDL_BUTTON_RIGHT), To::Game);
	const auto frame = Pass(state, false);
	EXPECT_FALSE(frame.box);
	EXPECT_TRUE(frame.action);
	EXPECT_TRUE(state.rightPressed);
	EXPECT_FALSE(state.boxTookInput);
}

TEST(BoxTakesInput, theMenuClearsTheHeldRightButtonEveryFrame)
{
	// hold the right button on the land and press Escape: from the menu's first frame the hand holds nothing
	MouseButtonsState state;
	EXPECT_TRUE(Route(state, SdlDown(SDL_BUTTON_RIGHT), To::Game));
	EXPECT_TRUE(Pass(state, false).action);
	EXPECT_FALSE(Route(state, SdlKey(SDLK_ESCAPE), To::Menu));
	for (int i = 0; i < 3; ++i)
	{
		const auto frame = Pass(state, true);
		EXPECT_TRUE(frame.box);
		EXPECT_FALSE(frame.action);
	}
	// the release the menu takes sends nothing: the binding is still pressed
	EXPECT_FALSE(Route(state, SdlUp(SDL_BUTTON_RIGHT), To::Menu));
	EXPECT_TRUE(state.rightPressed);
	EXPECT_FALSE(Pass(state, true).action);
}

TEST(BoxTakesInput, theFirstPressAfterTheBoxIsSwallowed)
{
	// the button held when the box came keeps its binding pressed: the first press after the box sends no press, and
	// its release sends the release, which finds the button let go already
	MouseButtonsState state;
	Route(state, SdlDown(SDL_BUTTON_RIGHT), To::Game);
	(void)Pass(state, true);
	Route(state, SdlUp(SDL_BUTTON_RIGHT), To::Menu);
	(void)Pass(state, true);
	(void)Pass(state, false); // the menu closed
	EXPECT_FALSE(Route(state, SdlDown(SDL_BUTTON_RIGHT), To::Game));
	EXPECT_FALSE(Pass(state, false).action);
	EXPECT_TRUE(Route(state, SdlUp(SDL_BUTTON_RIGHT), To::Game));
	EXPECT_FALSE(state.rightPressed);
	EXPECT_FALSE(Pass(state, false).action);
	// the next click is an ordinary one
	EXPECT_TRUE(Route(state, SdlDown(SDL_BUTTON_RIGHT), To::Game));
	EXPECT_TRUE(Pass(state, false).action);
	EXPECT_TRUE(Route(state, SdlUp(SDL_BUTTON_RIGHT), To::Game));
	EXPECT_FALSE(Pass(state, false).action);
}

TEST(BoxTakesInput, aButtonStillHeldAfterTheBoxIsReleasedByItsUp)
{
	// hold the left button through the menu: after it, its up sends the release and the left button is not left
	// gripping
	MouseButtonsState state;
	Route(state, SdlDown(SDL_BUTTON_LEFT), To::Game);
	EXPECT_TRUE(Pass(state, false).gripping);
	EXPECT_FALSE(Pass(state, true).gripping);
	(void)Pass(state, false);
	EXPECT_TRUE(Route(state, SdlUp(SDL_BUTTON_LEFT), To::Game));
	EXPECT_FALSE(Pass(state, false).gripping);
	EXPECT_TRUE(Route(state, SdlDown(SDL_BUTTON_LEFT), To::Game));
	EXPECT_TRUE(Pass(state, false).gripping);
}

TEST(BoxTakesInput, theEventsBeforeThePassAfterTheBoxAreDropped)
{
	// the menu closes during a frame: the button events until the next pass still find the box's input taken
	MouseButtonsState state;
	(void)Pass(state, true);
	EXPECT_TRUE(state.boxTookInput);
	EXPECT_FALSE(Route(state, SdlDown(SDL_BUTTON_RIGHT), To::Game));
	EXPECT_FALSE(state.rightPressed);
	EXPECT_FALSE(Route(state, SdlUp(SDL_BUTTON_RIGHT), To::Game));
	EXPECT_FALSE(Pass(state, false).box);
	EXPECT_FALSE(state.boxTookInput);
	EXPECT_TRUE(Route(state, SdlDown(SDL_BUTTON_RIGHT), To::Game));
	EXPECT_TRUE(Pass(state, false).action);
}

TEST(BoxTakesInput, everyButtonIsClearedAndTheMiddlePositionKept)
{
	MouseButtonsState state;
	Route(state, SdlDown(SDL_BUTTON_LEFT), To::Game);
	Route(state, SdlDown(SDL_BUTTON_RIGHT), To::Game);
	Route(state, SdlDown(SDL_BUTTON_MIDDLE, 120, 340), To::Game);
	const auto frame = Pass(state, true);
	EXPECT_FALSE(frame.gripping);
	EXPECT_FALSE(frame.action);
	EXPECT_FALSE(state.middle);
	EXPECT_TRUE(state.leftPressed);
	EXPECT_TRUE(state.middlePressed);
	EXPECT_TRUE(state.rightPressed);
	EXPECT_EQ(state.middlePressPosition, glm::ivec2(120, 340));
}

TEST(BoxTakesInput, aDebugWindowTakingTheUpTakesTheInputForOnePass)
{
	// a modal debug window takes the up of a press the hand got: at the next pass it takes the input as a box, then
	// gives it back; the binding stays pressed, so the next press is swallowed
	MouseButtonsState state;
	Route(state, SdlDown(SDL_BUTTON_RIGHT), To::Game);
	EXPECT_TRUE(Pass(state, false).action);
	Route(state, SdlUp(SDL_BUTTON_RIGHT), To::DebugWindow);
	const auto frame = Pass(state, false);
	EXPECT_TRUE(frame.box);
	EXPECT_FALSE(frame.action);
	EXPECT_FALSE(Pass(state, false).box);
	EXPECT_FALSE(Route(state, SdlDown(SDL_BUTTON_RIGHT), To::Game));
	EXPECT_TRUE(Route(state, SdlUp(SDL_BUTTON_RIGHT), To::Game));
}

TEST(BoxTakesInput, aDebugWindowHoldsTheInputWhileItHoldsAClick)
{
	// the right button held on the land, a click on a debug window: from the pass after the window's down to the one
	// after its up the window takes the input
	MouseButtonsState state;
	Route(state, SdlDown(SDL_BUTTON_RIGHT), To::Game);
	Route(state, SdlDown(SDL_BUTTON_LEFT), To::DebugWindow);
	EXPECT_TRUE(Pass(state, false).box);
	EXPECT_TRUE(Pass(state, false).box);
	EXPECT_FALSE(Pass(state, false).action);
	Route(state, SdlUp(SDL_BUTTON_LEFT), To::DebugWindow);
	EXPECT_TRUE(Pass(state, false).box);
	EXPECT_FALSE(Pass(state, false).box);
	// the window's own click never reached the left binding
	EXPECT_FALSE(state.leftPressed);
}

TEST(BoxTakesInput, aDebugWindowsHeldClickEndsWithItsUpAnywhere)
{
	// the window took the down; the up reaches the game (released over the land): the window's hold ends, the up sends
	// nothing (the binding never had the press)
	MouseButtonsState state;
	Route(state, SdlDown(SDL_BUTTON_LEFT), To::DebugWindow);
	EXPECT_TRUE(Pass(state, false).box);
	EXPECT_FALSE(Route(state, SdlUp(SDL_BUTTON_LEFT), To::Game));
	EXPECT_FALSE(state.leftInWindow);
	EXPECT_FALSE(Pass(state, false).box);
}

TEST(BoxTakesInput, keysAndMotionTakenByADebugWindowAreNotABox)
{
	MouseButtonsState state;
	Route(state, SdlDown(SDL_BUTTON_RIGHT), To::Game);
	EXPECT_FALSE(Route(state, SdlKey(SDLK_a), To::DebugWindow));
	const auto frame = Pass(state, false);
	EXPECT_FALSE(frame.box);
	EXPECT_TRUE(frame.action);
}

TEST(BoxTakesInput, nothingHeldOnlyTheBoxFlagChanges)
{
	// the deterministic runs: no real mouse event while the SkipBox is up; the buttons stay as they were
	MouseButtonsState state;
	for (int i = 0; i < 30; ++i)
	{
		EXPECT_TRUE(Pass(state, true).box);
	}
	EXPECT_FALSE(Pass(state, false).box);
	EXPECT_FALSE(state.left);
	EXPECT_FALSE(state.middle);
	EXPECT_FALSE(state.right);
	EXPECT_FALSE(state.leftPressed);
	EXPECT_FALSE(state.middlePressed);
	EXPECT_FALSE(state.rightPressed);
	EXPECT_FALSE(state.windowTookButton);
	EXPECT_FALSE(state.boxTookInput);
	EXPECT_EQ(state.middlePressPosition, glm::ivec2(0, 0));
}

TEST(BoxTakesInput, aScriptsDemoGetsTheRuleTheTestHooksDoesNot)
{
	// under a box: the hand gets the rule with no demo and while a script's demo plays (the tutorial's
	// PLAY_HAND_DEMO), not while a demo the test hook started plays (openblack's own)
	MouseButtonsState state;
	ASSERT_TRUE(Pass(state, true).box);
	EXPECT_TRUE(openblack::hand_demo::BoxRuleReachesHand(false, false));
	EXPECT_TRUE(openblack::hand_demo::BoxRuleReachesHand(true, false));
	EXPECT_FALSE(openblack::hand_demo::BoxRuleReachesHand(true, true));
	// a mark left from an ended test hook demo changes nothing
	EXPECT_TRUE(openblack::hand_demo::BoxRuleReachesHand(false, true));
	static_assert(openblack::hand_demo::BoxRuleReachesHand(true, false));
}

TEST(MouseMotion, theFirstFrameHasNoMotion)
{
	MouseMotionState state;
	EXPECT_EQ(MouseMotion(state, {400, 300}), glm::ivec2(0, 0));
}

TEST(MouseMotion, eachFrameIsTheMoveSinceTheLast)
{
	MouseMotionState state;
	(void)MouseMotion(state, {400, 300});
	EXPECT_EQ(MouseMotion(state, {410, 295}), glm::ivec2(10, -5));
	EXPECT_EQ(MouseMotion(state, {410, 295}), glm::ivec2(0, 0));
	EXPECT_EQ(MouseMotion(state, {0, 0}), glm::ivec2(-410, -295));
}

// The grab button's double click: its second down is the window's double click alone, for the camera

TEST(GrabDoubleClick, theSecondClickOfTheGrabButtonSendsNoPressAndItsUpNoRelease)
{
	MouseButtonsState state;
	EXPECT_TRUE(Route(state, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 1), To::Game));
	EXPECT_TRUE(Route(state, SdlClicks(SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, 1), To::Game));
	EXPECT_FALSE(Route(state, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 2), To::Game));
	EXPECT_FALSE(state.left);
	EXPECT_FALSE(state.leftPressed);
	EXPECT_FALSE(HandGripping(state));
	EXPECT_FALSE(Route(state, SdlClicks(SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, 2), To::Game));
	EXPECT_FALSE(state.actionDoubleClick);
	// the next single click presses again
	EXPECT_TRUE(Route(state, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 1), To::Game));
	EXPECT_TRUE(state.left);
}

// The Action button's double click, latched by the window's event and taken once a frame (the leash's tie)

TEST(ActionDoubleClick, theSecondClickOfTheActionButtonLatchesItAndStillPresses)
{
	MouseButtonsState state;
	EXPECT_TRUE(Route(state, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 1), To::Game));
	EXPECT_FALSE(state.actionDoubleClick);
	EXPECT_TRUE(Route(state, SdlClicks(SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT, 1), To::Game));
	EXPECT_TRUE(Route(state, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 2), To::Game));
	EXPECT_TRUE(state.actionDoubleClick);
	EXPECT_TRUE(HandAction(state));
	// reading the double click presses the binding as a press does
	EXPECT_TRUE(state.rightPressed);
	EXPECT_FALSE(TakeActionPressWhileHeld(state));
	// so its up sends the release, and the latch still waits for the frame to take it
	EXPECT_TRUE(Route(state, SdlClicks(SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT, 2), To::Game));
	EXPECT_TRUE(state.actionDoubleClick);
	EXPECT_FALSE(HandAction(state));
	EXPECT_FALSE(state.rightPressed);
}

// Until 2026-11-22 the double click left the binding unpressed, so the action stayed held after its up until the next
// plain press's up; the double click's reader presses the binding, and its up lets the action go
TEST(ActionDoubleClick, theDoubleClicksUpLetsTheActionGoAndTheNextPressIsAPlainOne)
{
	MouseButtonsState state;
	Route(state, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 1), To::Game);
	Route(state, SdlClicks(SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT, 1), To::Game);
	Route(state, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 2), To::Game);
	Route(state, SdlClicks(SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT, 2), To::Game);
	ASSERT_TRUE(TakeActionDoubleClick(state, false));
	EXPECT_FALSE(HandAction(state));
	// the next plain press holds the action again, a new press for the hand, not one while held
	EXPECT_TRUE(Route(state, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 1), To::Game));
	EXPECT_TRUE(HandAction(state));
	EXPECT_FALSE(TakeActionPressWhileHeld(state));
	EXPECT_TRUE(Route(state, SdlClicks(SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT, 1), To::Game));
	EXPECT_FALSE(HandAction(state));
}

TEST(ActionDoubleClick, aPlainPressIsNeverAPressWhileHeld)
{
	MouseButtonsState state;
	for (int click = 0; click < 3; ++click)
	{
		ApplyMouseButton(state, Down(MouseButton::Right));
		ApplyMouseButton(state, Up(MouseButton::Right));
	}
	EXPECT_FALSE(TakeActionPressWhileHeld(state));
}

TEST(ActionDoubleClick, aBoxDropsAPressWhileHeld)
{
	MouseButtonsState state;
	ApplyMouseButton(state, Down(MouseButton::Right));
	ApplyMouseButton(state, {.button = MouseButton::Right, .down = true, .doubleClick = true});
	ASSERT_TRUE(state.actionPressedWhileHeld);
	EXPECT_TRUE(Pass(state, true).box);
	EXPECT_FALSE(TakeActionPressWhileHeld(state));
}

TEST(ActionDoubleClick, theFrameTakesItOnce)
{
	MouseButtonsState state;
	ApplyMouseButton(state, {.button = MouseButton::Right, .down = true, .doubleClick = true});
	EXPECT_TRUE(TakeActionDoubleClick(state, false));
	EXPECT_FALSE(state.actionDoubleClick);
	EXPECT_FALSE(TakeActionDoubleClick(state, false));
}

TEST(ActionDoubleClick, onlyTheActionButtonsSecondDownLatchesIt)
{
	MouseButtonsState state;
	for (const auto button : {SDL_BUTTON_LEFT, SDL_BUTTON_MIDDLE, SDL_BUTTON_X1})
	{
		Route(state, SdlClicks(SDL_MOUSEBUTTONDOWN, static_cast<uint8_t>(button), 2), To::Game);
		Route(state, SdlClicks(SDL_MOUSEBUTTONUP, static_cast<uint8_t>(button), 2), To::Game);
	}
	// a triple click's third down is not a double click
	Route(state, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 3), To::Game);
	// nor is an up
	Route(state, SdlClicks(SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT, 2), To::Game);
	EXPECT_FALSE(state.actionDoubleClick);
	EXPECT_FALSE(TakeActionDoubleClick(state, false));
}

TEST(ActionDoubleClick, itIsLatchedEvenWhileTheBindingIsPressed)
{
	// the window's double click is its own event: a down while the binding is pressed already still latches it
	MouseButtonsState state;
	ApplyMouseButton(state, Down(MouseButton::Right));
	// the double click presses the action as a press does, while it is held already
	EXPECT_TRUE(ApplyMouseButton(state, {.button = MouseButton::Right, .down = true, .doubleClick = true}));
	EXPECT_TRUE(TakeActionDoubleClick(state, false));
	EXPECT_TRUE(TakeActionPressWhileHeld(state));
	// and its up is the binding's one release
	EXPECT_TRUE(ApplyMouseButton(state, Up(MouseButton::Right)));
	EXPECT_FALSE(HandAction(state));
	EXPECT_FALSE(ApplyMouseButton(state, Up(MouseButton::Right)));
}

TEST(ActionDoubleClick, theTemplesInteriorKeepsItFromTheHand)
{
	MouseButtonsState state;
	ApplyMouseButton(state, {.button = MouseButton::Right, .down = true, .doubleClick = true});
	EXPECT_FALSE(TakeActionDoubleClick(state, true));
	// taken all the same: it does not wait for the interior to close
	EXPECT_FALSE(state.actionDoubleClick);
	EXPECT_FALSE(TakeActionDoubleClick(state, false));
}

TEST(ActionDoubleClick, aBoxDropsIt)
{
	// latched, then a box takes the input at the frame pass: gone with the buttons
	MouseButtonsState state;
	Route(state, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 2), To::Game);
	ASSERT_TRUE(state.actionDoubleClick);
	EXPECT_TRUE(Pass(state, true).box);
	EXPECT_FALSE(TakeActionDoubleClick(state, false));
	// while the box holds the input, the events that come are dropped
	Route(state, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 2), To::Game);
	EXPECT_FALSE(state.actionDoubleClick);
	// a menu or a debug window that takes the click keeps it from the hand
	MouseButtonsState other;
	Route(other, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 2), To::Menu);
	Route(other, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 2), To::DebugWindow);
	EXPECT_FALSE(other.actionDoubleClick);
}

// The middle button's second click of a double click: the window gives it as nothing at all

TEST(MiddleDoubleClick, theSecondClickOfTheMiddleButtonHoldsNothingAndItsUpReleasesNothing)
{
	MouseButtonsState state;
	EXPECT_TRUE(Route(state, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_MIDDLE, 1), To::Game));
	EXPECT_TRUE(Route(state, SdlClicks(SDL_MOUSEBUTTONUP, SDL_BUTTON_MIDDLE, 1), To::Game));
	EXPECT_FALSE(Route(state, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_MIDDLE, 2), To::Game));
	EXPECT_FALSE(state.middle);
	EXPECT_FALSE(state.middlePressed);
	EXPECT_FALSE(HandGripping(state));
	EXPECT_FALSE(Route(state, SdlClicks(SDL_MOUSEBUTTONUP, SDL_BUTTON_MIDDLE, 2), To::Game));
	EXPECT_FALSE(state.actionDoubleClick);
	// a quick row's third press is a plain press again, its fourth nothing
	EXPECT_TRUE(Route(state, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_MIDDLE, 3), To::Game));
	EXPECT_TRUE(state.middle);
	EXPECT_TRUE(Route(state, SdlClicks(SDL_MOUSEBUTTONUP, SDL_BUTTON_MIDDLE, 3), To::Game));
	EXPECT_FALSE(Route(state, SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_MIDDLE, 4), To::Game));
	EXPECT_FALSE(state.middle);
}

TEST(MiddleDoubleClick, theSecondClickKeepsTheFirstPressPosition)
{
	MouseButtonsState state;
	auto first = SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_MIDDLE, 1);
	first.button.x = 10;
	first.button.y = 20;
	Route(state, first, To::Game);
	Route(state, SdlClicks(SDL_MOUSEBUTTONUP, SDL_BUTTON_MIDDLE, 1), To::Game);
	auto second = SdlClicks(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_MIDDLE, 2);
	second.button.x = 11;
	second.button.y = 21;
	Route(state, second, To::Game);
	EXPECT_EQ(state.middlePressPosition, glm::ivec2(10, 20));
}

TEST(HandBindings, AKeyBoundToTheActionPressesHoldsAndReleasesItAsTheButtonDid)
{
	// the Action bound to a key and no button: the key's press, hold and release reach the hand as the button's did,
	// read once a pass
	MouseButtonsState key;
	key.bindings.action = std::nullopt;
	MouseButtonsState button;
	EXPECT_EQ(ApplyBindingKey(key, HandBinding::Action, true), BindingKeySent::Press);
	EXPECT_TRUE(ApplyMouseButton(button, Down(MouseButton::Right)));
	EXPECT_EQ(HandAction(key), HandAction(button));
	EXPECT_TRUE(HandAction(key));
	// held through later passes, it sends nothing more
	EXPECT_EQ(ApplyBindingKey(key, HandBinding::Action, true), BindingKeySent::Nothing);
	EXPECT_TRUE(HandAction(key));
	EXPECT_EQ(ApplyBindingKey(key, HandBinding::Action, false), BindingKeySent::Release);
	EXPECT_TRUE(ApplyMouseButton(button, Up(MouseButton::Right)));
	EXPECT_EQ(HandAction(key), HandAction(button));
	EXPECT_FALSE(HandAction(key));
	EXPECT_EQ(ApplyBindingKey(key, HandBinding::Action, false), BindingKeySent::Nothing);
	// the right button, bound to nothing now, gives the hand nothing
	EXPECT_FALSE(ApplyMouseButton(key, Down(MouseButton::Right)));
	EXPECT_FALSE(HandAction(key));
}

TEST(HandBindings, AKeyBoundToTheMoveGripsAsTheLeftButtonDid)
{
	MouseButtonsState state;
	EXPECT_EQ(ApplyBindingKey(state, HandBinding::Move, true), BindingKeySent::Press);
	EXPECT_TRUE(HandGripping(state));
	EXPECT_FALSE(HandAction(state));
	EXPECT_EQ(ApplyBindingKey(state, HandBinding::Move, false), BindingKeySent::Release);
	EXPECT_FALSE(HandGripping(state));
}

TEST(HandBindings, ThePressOfTheKeyOrTheButtonIsReleasedOnlyByItsOwnLettingGo)
{
	// the Action bound to both a key and the right button: one press of the two holds it
	MouseButtonsState state;
	EXPECT_EQ(ApplyBindingKey(state, HandBinding::Action, true), BindingKeySent::Press);
	EXPECT_FALSE(ApplyMouseButton(state, Down(MouseButton::Right)));
	// the button's up does not let go of the key's press
	EXPECT_FALSE(ApplyMouseButton(state, Up(MouseButton::Right)));
	EXPECT_TRUE(HandAction(state));
	EXPECT_EQ(ApplyBindingKey(state, HandBinding::Action, false), BindingKeySent::Release);
	EXPECT_FALSE(HandAction(state));
	// pressed by the button, the key held sends nothing until the button's up lets it go, then presses again
	EXPECT_TRUE(ApplyMouseButton(state, Down(MouseButton::Right)));
	EXPECT_EQ(ApplyBindingKey(state, HandBinding::Action, true), BindingKeySent::Nothing);
	EXPECT_TRUE(ApplyMouseButton(state, Up(MouseButton::Right)));
	EXPECT_FALSE(HandAction(state));
	EXPECT_EQ(ApplyBindingKey(state, HandBinding::Action, true), BindingKeySent::Press);
	EXPECT_TRUE(HandAction(state));
}

TEST(HandBindings, AKeySendsNothingWhileABoxTookTheInput)
{
	MouseButtonsState state;
	EXPECT_TRUE(BoxTakesInput(state, true));
	EXPECT_EQ(ApplyBindingKey(state, HandBinding::Action, true), BindingKeySent::Nothing);
	EXPECT_FALSE(HandAction(state));
	EXPECT_FALSE(BoxTakesInput(state, false));
	EXPECT_EQ(ApplyBindingKey(state, HandBinding::Action, true), BindingKeySent::Press);
	EXPECT_TRUE(HandAction(state));
}

// The double click presses the Action's binding, so the key bound to it is no new press until the button's up has let
// it go; after that the key's press is a plain one, with nothing latched as a press over a held action
TEST(HandBindings, TheKeysPressAfterADoubleClicksUpIsAPlainPress)
{
	MouseButtonsState state;
	ApplyMouseButton(state, {.button = MouseButton::Right, .down = true, .doubleClick = true});
	EXPECT_TRUE(TakeActionDoubleClick(state, false));
	EXPECT_EQ(ApplyBindingKey(state, HandBinding::Action, true), BindingKeySent::Nothing);
	ApplyBindingKey(state, HandBinding::Action, false);
	EXPECT_TRUE(ApplyMouseButton(state, {.button = MouseButton::Right, .down = false}));
	EXPECT_EQ(ApplyBindingKey(state, HandBinding::Action, true), BindingKeySent::Press);
	EXPECT_FALSE(TakeActionPressWhileHeld(state));
}

TEST(HandBindings, ButtonsSwappedOnTheControlsPageSwapTheHandsGripAndAction)
{
	MouseButtonsState state;
	state.bindings = {.move = MouseButton::Right, .action = MouseButton::Left};
	EXPECT_TRUE(ApplyMouseButton(state, Down(MouseButton::Right)));
	EXPECT_TRUE(HandGripping(state));
	EXPECT_FALSE(HandAction(state));
	EXPECT_TRUE(ApplyMouseButton(state, Down(MouseButton::Left)));
	EXPECT_TRUE(HandAction(state));
	// the window's double click is the binding's: the left's is now the Action's, the right's the Move's (no press)
	EXPECT_TRUE(ApplyMouseButton(state, Up(MouseButton::Left)));
	EXPECT_TRUE(ApplyMouseButton(state, {.button = MouseButton::Left, .down = true, .doubleClick = true}));
	EXPECT_TRUE(TakeActionDoubleClick(state, false));
	EXPECT_TRUE(ApplyMouseButton(state, Up(MouseButton::Right)));
	EXPECT_FALSE(ApplyMouseButton(state, {.button = MouseButton::Right, .down = true, .doubleClick = true}));
	EXPECT_FALSE(HandGripping(state));
}

TEST(HandBindings, TheMiddleButtonBoundToTheActionIsTheActionAndNoLongerItsOwn)
{
	MouseButtonsState state;
	state.bindings.action = MouseButton::Middle;
	EXPECT_TRUE(ApplyMouseButton(state, Down(MouseButton::Middle, {5, 6})));
	EXPECT_TRUE(HandAction(state));
	EXPECT_FALSE(state.middle);
	EXPECT_FALSE(ApplyMouseButton(state, Down(MouseButton::Right)));
}

TEST(HandBindings, TheDefaultBindingsGiveTheSameInputAsTheButtonsThemselves)
{
	// the default bindings are the left and the right buttons: a row of downs, ups and double clicks of every button
	// leaves the same buttons, whatever the keys (none bound) send
	const std::vector<MouseButtonEvent> events {
	    Down(MouseButton::Left), Down(MouseButton::Right),
	    Up(MouseButton::Left),   {.button = MouseButton::Left, .down = true, .doubleClick = true},
	    Up(MouseButton::Right),  {.button = MouseButton::Right, .down = true, .doubleClick = true},
	    Up(MouseButton::Right),  Down(MouseButton::Middle, {3, 4}),
	    Up(MouseButton::Left),   Down(MouseButton::Right),
	};
	MouseButtonsState state;
	EXPECT_EQ(state.bindings.move, MouseButton::Left);
	EXPECT_EQ(state.bindings.action, MouseButton::Right);
	std::vector<bool> sent;
	for (const auto& event : events)
	{
		sent.push_back(ApplyMouseButton(state, event));
		EXPECT_EQ(ApplyBindingKey(state, HandBinding::Move, false), BindingKeySent::Nothing);
		EXPECT_EQ(ApplyBindingKey(state, HandBinding::Action, false), BindingKeySent::Nothing);
	}
	// the right double click's up lets the action go, as a plain press's up does, so the right press after it is a
	// plain press and nothing is latched as a press over a held action
	EXPECT_EQ(sent, (std::vector<bool> {true, true, true, false, true, true, true, true, false, true}));
	EXPECT_FALSE(state.left);
	EXPECT_TRUE(state.middle);
	EXPECT_TRUE(state.right);
	EXPECT_EQ(state.middlePressPosition, glm::ivec2(3, 4));
	EXPECT_TRUE(TakeActionDoubleClick(state, false));
	EXPECT_FALSE(TakeActionPressWhileHeld(state));
}

// A save's input reset forgets the Action's double click and its press while held not yet taken, and leaves the buttons
// held as they are
TEST(HandBindings, ASavesInputResetForgetsTheUntakenPressesAndKeepsTheButtonsHeld)
{
	MouseButtonsState state;
	state.right = true;
	state.rightPressed = true;
	state.actionDoubleClick = true;
	state.actionPressedWhileHeld = true;
	ForgetPresses(state);
	EXPECT_FALSE(TakeActionDoubleClick(state, false));
	EXPECT_FALSE(TakeActionPressWhileHeld(state));
	EXPECT_TRUE(state.right);
	EXPECT_TRUE(state.rightPressed);
}
