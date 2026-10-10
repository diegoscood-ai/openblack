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

#include "Input/HandDemo.h"
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
	EXPECT_TRUE(HandGripping(state));
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

TEST(MouseButtons, gripIsLeftOrMiddle)
{
	MouseButtonsState state;
	ApplyMouseButton(state, Down(MouseButton::Left));
	ApplyMouseButton(state, Down(MouseButton::Middle));
	ApplyMouseButton(state, Up(MouseButton::Left));
	EXPECT_TRUE(HandGripping(state));
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
