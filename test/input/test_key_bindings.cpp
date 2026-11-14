/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The action map from the key bindings table: keys, chords and the mouse to actions, rebinding, and the debug window's
// test presses (raffclar's tests of his key bindings); the wheel's notches, the cursor's warp and its freeze; the
// actions the land's scripts block; the lettings go made up outside the test runs; the testbed's keys and double click

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <string_view>
#include <vector>

#include <SDL_events.h>
#include <gtest/gtest.h>

#include "Debug/TestbedHeldKeys.h"
#include "Input/FixedMouse.h"
#include "Input/GameActionMap.h"
#include "Input/GameCursor.h"
#include "Input/InputRecording.h"
#include "Input/KeyBindings.h"
#include "Input/KeyBuffer.h"
#include "Input/RealInput.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::input;

namespace
{
[[nodiscard]] SDL_Event KeyEvent(uint32_t type, SDL_Scancode key, uint16_t modifiers = KMOD_NONE)
{
	SDL_Event event {};
	event.type = type;
	event.key.keysym.scancode = key;
	event.key.keysym.mod = modifiers;
	return event;
}

/// An event as the game's window dispatches it: seen as the window's, then given to the action map
void Dispatch(GameActionMap& map, const SDL_Event& event)
{
	map.SeeDispatchedEvent(event);
	map.ProcessEvent(event);
}

[[nodiscard]] bool Pressed(const GameActionMap& map, BindableActionMap action)
{
	return map.Get(action) && map.GetChanged(action);
}

[[nodiscard]] SDL_Event WheelEvent(float notches, uint32_t direction = SDL_MOUSEWHEEL_NORMAL)
{
	SDL_Event event {};
	event.type = SDL_MOUSEWHEEL;
	event.wheel.y = static_cast<Sint32>(notches);
	event.wheel.preciseY = notches;
	event.wheel.direction = direction;
	return event;
}

[[nodiscard]] SDL_Event MiddleButtonEvent(uint32_t type)
{
	SDL_Event event {};
	event.type = type;
	event.button.button = SDL_BUTTON_MIDDLE;
	event.button.clicks = 1;
	return event;
}

/// A window of 800 by 600 that counts how often its handle, which the real pointer is moved through, is asked for
class CountingWindowing final: public windowing::WindowingInterface
{
public:
	[[nodiscard]] void* GetHandle() const final
	{
		++handleReads;
		return nullptr;
	}
	[[nodiscard]] NativeHandles GetNativeHandles() const final { return {}; }
	[[nodiscard]] uint32_t GetID() const final { return 0; }
	[[nodiscard]] glm::ivec2 GetSize() const final { return {800, 600}; }
	[[nodiscard]] float GetAspectRatio() const final { return 800.0f / 600.0f; }
	[[nodiscard]] bool IsMinimised() const final { return false; }
	WindowingInterface& SetDisplayMode(windowing::DisplayMode /*mode*/) final { return *this; }

	mutable int handleReads = 0;
};

/// The action map with the counting window injected, as in a test run (OPENBLACK_MOUSE_AT set) or not
class GameActionMapCursor: public testing::Test
{
protected:
	void SetUp() override { _window = &static_cast<CountingWindowing&>(Locator::windowing::emplace<CountingWindowing>()); }

	test::RestoreService<Locator::windowing> _restoreWindowing;
	CountingWindowing* _window = nullptr;
};
} // namespace

TEST(KeyBindings, ConflictsAreFound)
{
	auto bindings = k_DefaultKeyBindings;
	bindings.at(*IndexOf(bindings, BindableActionMap::TALK)).key = KeyChord {SDL_SCANCODE_N};
	const auto conflicts = FindConflicts(bindings);
	ASSERT_EQ(conflicts.size(), 1u);
	EXPECT_EQ(bindings.at(conflicts[0].first).action, BindableActionMap::TALK);
	EXPECT_EQ(bindings.at(conflicts[0].second).action, BindableActionMap::SHOW_VILLAGER_NAMES);
}

TEST(KeyBindings, KeysMapToActions)
{
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_SPACE, KMOD_NONE), BindableActionMap::ZOOM_TO_TEMPLE);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_F3, KMOD_NONE), BindableActionMap::ZOOM_TO_REALM);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_X, KMOD_NONE), BindableActionMap::NONE);
}

/// No action holds Escape, with or without a modifier, so a film's skip is never taken by a binding first: the
/// original's defaults bind no Escape and its key binding dialog refuses it (docs/wiki/video/playback.md)
TEST(KeyBindings, NoActionIsBoundToEscape)
{
	for (const auto& binding : k_DefaultKeyBindings)
	{
		EXPECT_FALSE(binding.key.has_value() && binding.key->key == SDL_SCANCODE_ESCAPE) << binding.name;
	}
	for (const uint16_t modifiers : {KMOD_NONE, KMOD_LSHIFT, KMOD_LCTRL, KMOD_LALT})
	{
		EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_ESCAPE, modifiers), BindableActionMap::NONE);
	}
}

TEST(KeyBindings, EitherSidesModifierKeyCounts)
{
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_LCTRL, KMOD_LCTRL), BindableActionMap::ZOOM_ON);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_RCTRL, KMOD_RCTRL), BindableActionMap::ZOOM_ON);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_LSHIFT, KMOD_LSHIFT), BindableActionMap::ROTATE_ON);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_RSHIFT, KMOD_RSHIFT), BindableActionMap::ROTATE_ON);
}

TEST(KeyBindings, ModifierBindingWinsOverPlainOne)
{
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_NONE), BindableActionMap::SHOW_VILLAGER_DETAILS);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_RCTRL), BindableActionMap::QUICK_SAVE);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_L, KMOD_NONE), BindableActionMap::LEASH_UNLEASH_CREATURE);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_L, KMOD_LCTRL), BindableActionMap::QUICK_LOAD);
	// Shift isn't the quick save's modifier, so S with Shift is still the details
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_LSHIFT), BindableActionMap::SHOW_VILLAGER_DETAILS);
}

TEST(KeyBindings, LettingGoOfAKeyEndsAllItsActions)
{
	EXPECT_EQ(ActionsForKey(k_DefaultKeyBindings, SDL_SCANCODE_S),
	          static_cast<BindableActionMap>(static_cast<uint64_t>(BindableActionMap::SHOW_VILLAGER_DETAILS) |
	                                         static_cast<uint64_t>(BindableActionMap::QUICK_SAVE)));
}

TEST(KeyBindings, MouseMapsToActions)
{
	EXPECT_EQ(ActionsForMouse(k_DefaultKeyBindings, MouseInput::LeftButton), BindableActionMap::MOVE);
	EXPECT_EQ(ActionsForMouse(k_DefaultKeyBindings, MouseInput::RightButton), BindableActionMap::ACTION);
	EXPECT_EQ(ActionsForMouse(k_DefaultKeyBindings, MouseInput::MiddleButton), BindableActionMap::ROTATE_AROUND_MOUSE_ON);
	EXPECT_EQ(ActionsForMouse(k_DefaultKeyBindings, MouseInput::WheelUp), BindableActionMap::ZOOM_IN);
	EXPECT_EQ(ActionsForMouse(k_DefaultKeyBindings, MouseInput::WheelDown), BindableActionMap::ZOOM_OUT);
	EXPECT_EQ(ActionsForMouse(k_DefaultKeyBindings, MouseInput::None), BindableActionMap::NONE);
}

TEST(GameActionMap, KeyEventsPressAndReleaseActions)
{
	GameActionMap map;
	map.Frame();
	Dispatch(map, KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_N));
	EXPECT_TRUE(Pressed(map, BindableActionMap::SHOW_VILLAGER_NAMES));
	map.Frame();
	EXPECT_TRUE(map.Get(BindableActionMap::SHOW_VILLAGER_NAMES));
	EXPECT_FALSE(map.GetChanged(BindableActionMap::SHOW_VILLAGER_NAMES));
	Dispatch(map, KeyEvent(SDL_KEYUP, SDL_SCANCODE_N));
	EXPECT_FALSE(map.Get(BindableActionMap::SHOW_VILLAGER_NAMES));
}

TEST(GameActionMap, AKeyIsHeldFromItsPressToItsRelease)
{
	GameActionMap map;
	EXPECT_FALSE(map.IsKeyHeld(SDL_SCANCODE_KP_ENTER));
	Dispatch(map, KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_KP_ENTER));
	EXPECT_TRUE(map.IsKeyHeld(SDL_SCANCODE_KP_ENTER));
	EXPECT_FALSE(map.IsKeyHeld(SDL_SCANCODE_RETURN));
	Dispatch(map, KeyEvent(SDL_KEYUP, SDL_SCANCODE_KP_ENTER));
	EXPECT_FALSE(map.IsKeyHeld(SDL_SCANCODE_KP_ENTER));
}

TEST(GameActionMap, ChordReplacesThePlainKey)
{
	GameActionMap map;
	map.Frame();
	Dispatch(map, KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_LCTRL, KMOD_LCTRL));
	Dispatch(map, KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_S, KMOD_LCTRL));
	EXPECT_TRUE(map.Get(BindableActionMap::ZOOM_ON));
	EXPECT_TRUE(Pressed(map, BindableActionMap::QUICK_SAVE));
	EXPECT_FALSE(map.Get(BindableActionMap::SHOW_VILLAGER_DETAILS));
	Dispatch(map, KeyEvent(SDL_KEYUP, SDL_SCANCODE_S, KMOD_LCTRL));
	EXPECT_FALSE(map.Get(BindableActionMap::QUICK_SAVE));
}

TEST(GameActionMap, RebindingMovesTheAction)
{
	GameActionMap map;
	map.SetKeyBinding(BindableActionMap::ZOOM_TO_TEMPLE, KeyChord {SDL_SCANCODE_HOME});
	map.Frame();
	Dispatch(map, KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE));
	EXPECT_FALSE(map.Get(BindableActionMap::ZOOM_TO_TEMPLE));
	Dispatch(map, KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_HOME));
	EXPECT_TRUE(Pressed(map, BindableActionMap::ZOOM_TO_TEMPLE));
	map.ResetKeyBindings();
	EXPECT_EQ(map.GetKeyBindings()[*IndexOf(map.GetKeyBindings(), BindableActionMap::ZOOM_TO_TEMPLE)].key,
	          KeyChord {SDL_SCANCODE_SPACE});
}

TEST(GameActionMap, RebindingTakesTheKeyOffEveryOtherAction)
{
	GameActionMap map;
	// S is Show Villager Details' key; Ctrl+S, Quick Save's, is another chord and stays
	map.SetKeyBinding(BindableActionMap::ZOOM_TO_TEMPLE, KeyChord {SDL_SCANCODE_S});
	const auto bindings = map.GetKeyBindings();
	const auto keyOf = [&bindings](BindableActionMap action) { return bindings[*IndexOf(bindings, action)].key; };
	EXPECT_EQ(keyOf(BindableActionMap::ZOOM_TO_TEMPLE), KeyChord {SDL_SCANCODE_S});
	EXPECT_FALSE(keyOf(BindableActionMap::SHOW_VILLAGER_DETAILS).has_value());
	EXPECT_EQ(keyOf(BindableActionMap::QUICK_SAVE), (KeyChord {SDL_SCANCODE_S, Modifier::Ctrl}));
	// Every other binding is untouched
	for (const auto& binding : k_DefaultKeyBindings)
	{
		if (binding.action != BindableActionMap::ZOOM_TO_TEMPLE && binding.action != BindableActionMap::SHOW_VILLAGER_DETAILS)
		{
			SCOPED_TRACE(binding.name);
			EXPECT_EQ(keyOf(binding.action), binding.key);
		}
	}
	map.Frame();
	Dispatch(map, KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_S));
	EXPECT_TRUE(Pressed(map, BindableActionMap::ZOOM_TO_TEMPLE));
	EXPECT_FALSE(map.Get(BindableActionMap::SHOW_VILLAGER_DETAILS));
}

TEST(GameActionMap, UnbindingAKeyLeavesTheOtherActions)
{
	GameActionMap map;
	map.SetKeyBinding(BindableActionMap::ZOOM_TO_TEMPLE, std::nullopt);
	const auto bindings = map.GetKeyBindings();
	for (const auto& binding : k_DefaultKeyBindings)
	{
		SCOPED_TRACE(binding.name);
		const auto expected = binding.action == BindableActionMap::ZOOM_TO_TEMPLE ? std::nullopt : binding.key;
		EXPECT_EQ(bindings[*IndexOf(bindings, binding.action)].key, expected);
	}
}

TEST(GameActionMap, QueuedPressGoesThroughTheKeyPath)
{
	GameActionMap map;
	map.Frame();
	// Every action, pressed from the debug window, shows as pressed for one frame and let go of the next
	for (const auto& binding : k_DefaultKeyBindings)
	{
		SCOPED_TRACE(binding.name);
		map.QueuePress(binding.action);
		EXPECT_TRUE(map.HasQueuedPresses());
		map.Frame();
		EXPECT_TRUE(Pressed(map, binding.action));
		map.Frame();
		EXPECT_FALSE(map.Get(binding.action));
		map.Frame();
		EXPECT_FALSE(map.HasQueuedPresses());
	}
}

TEST(GameActionMap, QueuedChordPressesOnlyTheChordsAction)
{
	GameActionMap map;
	map.Frame();
	map.QueuePress(BindableActionMap::QUICK_SAVE);
	map.Frame();
	EXPECT_TRUE(Pressed(map, BindableActionMap::QUICK_SAVE));
	EXPECT_FALSE(map.Get(BindableActionMap::SHOW_VILLAGER_DETAILS));
}

TEST(GameActionMap, NothingIsBlockedUntilAsked)
{
	GameActionMap map;
	map.Frame();
	Dispatch(map, KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_N));
	map.SetBlockedActions(BindableActionMap::NONE);
	EXPECT_TRUE(Pressed(map, BindableActionMap::SHOW_VILLAGER_NAMES));
}

TEST(GameActionMap, BlockedActionsReadAsNotHeld)
{
	GameActionMap map;
	map.Frame();
	Dispatch(map, KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_N));
	Dispatch(map, KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE));
	map.SetBlockedActions(BindableActionMap::SHOW_VILLAGER_NAMES);
	EXPECT_FALSE(map.Get(BindableActionMap::SHOW_VILLAGER_NAMES));
	EXPECT_FALSE(map.GetChanged(BindableActionMap::SHOW_VILLAGER_NAMES));
	EXPECT_FALSE(map.GetAny(BindableActionMap::SHOW_VILLAGER_NAMES));
	// The others are read as ever
	EXPECT_TRUE(Pressed(map, BindableActionMap::ZOOM_TO_TEMPLE));
	map.Frame();
	EXPECT_FALSE(map.GetRepeat(BindableActionMap::SHOW_VILLAGER_NAMES));
	EXPECT_TRUE(map.GetRepeat(BindableActionMap::ZOOM_TO_TEMPLE));
	// The key is still down, so once allowed the action reads as held again
	map.SetBlockedActions(BindableActionMap::NONE);
	EXPECT_TRUE(map.GetRepeat(BindableActionMap::SHOW_VILLAGER_NAMES));
}

TEST(GameActionMap, ADoubleClickChangesOnlyOnTheFrameItHappens)
{
	GameActionMap map;
	SDL_Event click {};
	click.type = SDL_MOUSEBUTTONDOWN;
	click.button.button = SDL_BUTTON_LEFT;
	click.button.clicks = 2;
	map.Frame();
	Dispatch(map, click);
	EXPECT_TRUE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_TRUE(map.GetChanged(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_FALSE(map.GetRepeat(UnbindableActionMap::DOUBLE_CLICK));
	// The button still down on the next frames is the same double click
	for (int frame = 0; frame < 2; ++frame)
	{
		map.Frame();
		EXPECT_TRUE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
		EXPECT_FALSE(map.GetChanged(UnbindableActionMap::DOUBLE_CLICK));
		EXPECT_TRUE(map.GetRepeat(UnbindableActionMap::DOUBLE_CLICK));
	}
	// Its letting go leaves it waiting; taken, it is a change once, then nothing
	click.type = SDL_MOUSEBUTTONUP;
	Dispatch(map, click);
	EXPECT_TRUE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_TRUE(map.TakeDoubleClick());
	EXPECT_FALSE(map.Get(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_TRUE(map.GetChanged(UnbindableActionMap::DOUBLE_CLICK));
	map.Frame();
	EXPECT_FALSE(map.GetChanged(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_FALSE(map.GetRepeat(UnbindableActionMap::DOUBLE_CLICK));
}

TEST(GameActionMap, WheelGivesItsNotchesForOneFrame)
{
	GameActionMap map;
	map.Frame();
	EXPECT_EQ(map.GetMouseWheelDelta(), 0.0f);
	Dispatch(map, WheelEvent(1.0f));
	EXPECT_EQ(map.GetMouseWheelDelta(), 1.0f);
	Dispatch(map, WheelEvent(1.0f));
	EXPECT_EQ(map.GetMouseWheelDelta(), 2.0f);
	map.Frame();
	EXPECT_EQ(map.GetMouseWheelDelta(), 0.0f);
	// Towards the user, a fine wheel's half notch, and a wheel the system flips
	Dispatch(map, WheelEvent(-1.0f));
	EXPECT_EQ(map.GetMouseWheelDelta(), -1.0f);
	map.Frame();
	Dispatch(map, WheelEvent(0.5f));
	EXPECT_EQ(map.GetMouseWheelDelta(), 0.5f);
	map.Frame();
	Dispatch(map, WheelEvent(1.0f, SDL_MOUSEWHEEL_FLIPPED));
	EXPECT_EQ(map.GetMouseWheelDelta(), -1.0f);
	map.Frame();
	EXPECT_EQ(map.GetMouseWheelDelta(), 0.0f);
}

TEST_F(GameActionMapCursor, WarpIsKeptForTheFrame)
{
	OverrideMouseAt("0.5,0.5");
	GameActionMap map;
	map.Frame();
	EXPECT_FALSE(map.GetCursorWarp().has_value());
	map.WarpCursor({300, 200});
	ASSERT_TRUE(map.GetCursorWarp().has_value());
	EXPECT_EQ(*map.GetCursorWarp(), glm::ivec2(300, 200));
	EXPECT_EQ(map.GetMousePosition(), glm::uvec2(300, 200));
	map.Frame();
	EXPECT_FALSE(map.GetCursorWarp().has_value());
}

TEST_F(GameActionMapCursor, FreezeIsOffUntilAllowed)
{
	OverrideMouseAt("0.5,0.5");
	GameActionMap map;
	map.Frame();
	Dispatch(map, MiddleButtonEvent(SDL_MOUSEBUTTONDOWN));
	map.Frame();
	EXPECT_TRUE(map.Get(BindableActionMap::ROTATE_AROUND_MOUSE_ON));
	EXPECT_FALSE(map.IsCursorFrozen());
}

TEST_F(GameActionMapCursor, FreezeHoldsTheCursorAndPutsItBack)
{
	OverrideMouseAt("0.5,0.5");
	GameActionMap map;
	map.AllowCursorFreeze(true);
	map.Frame();
	const auto heldAt = map.GetMousePosition();
	Dispatch(map, MiddleButtonEvent(SDL_MOUSEBUTTONDOWN));
	map.Frame();
	EXPECT_TRUE(map.IsCursorFrozen());
	EXPECT_EQ(map.GetMousePosition(), heldAt);

	// The cursor moved elsewhere goes back to where it is held
	map.WarpCursor({300, 200});
	map.Frame();
	EXPECT_TRUE(map.IsCursorFrozen());
	EXPECT_EQ(map.GetMousePosition(), heldAt);

	// Letting go of the button lets go of the cursor, which stays at the held spot that frame
	Dispatch(map, MiddleButtonEvent(SDL_MOUSEBUTTONUP));
	map.Frame();
	EXPECT_FALSE(map.IsCursorFrozen());
	EXPECT_EQ(map.GetMousePosition(), heldAt);
}

TEST_F(GameActionMapCursor, TestRunsNeverMoveTheRealPointer)
{
	// OPENBLACK_MOUSE_AT set: the warp and the freeze move only the game's cursor
	OverrideMouseAt("0.5,0.5");
	GameActionMap map;
	map.AllowCursorFreeze(true);
	map.Frame();
	map.WarpCursor({300, 200});
	Dispatch(map, MiddleButtonEvent(SDL_MOUSEBUTTONDOWN));
	map.Frame();
	ASSERT_TRUE(map.IsCursorFrozen());
	EXPECT_EQ(SDL_GetRelativeMouseMode(), SDL_FALSE);
	Dispatch(map, MiddleButtonEvent(SDL_MOUSEBUTTONUP));
	map.Frame();
	EXPECT_FALSE(map.IsCursorFrozen());
	EXPECT_EQ(_window->handleReads, 0);
}

TEST_F(GameActionMapCursor, WarpMovesTheRealPointerOutsideTestRuns)
{
	if (FixedMouse().has_value() || IgnoreRealInput())
	{
		GTEST_SKIP() << "the run's environment fixes the mouse";
	}
	// OPENBLACK_MOUSE_AT unset, whatever the environment has
	OverrideMouseAt("");
	GameActionMap map;
	map.Frame();
	map.WarpCursor({300, 200});
	EXPECT_EQ(_window->handleReads, 1);
}

TEST_F(GameActionMapCursor, TestRunsMakeUpNoLettingGo)
{
	// OPENBLACK_MOUSE_AT set: the real mouse and keyboard, which hold nothing here, don't let go of what is held
	OverrideMouseAt("0.5,0.5");
	GameActionMap map;
	map.Frame();
	Dispatch(map, MiddleButtonEvent(SDL_MOUSEBUTTONDOWN));
	Dispatch(map, KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_N));
	map.Frame();
	map.Frame();
	EXPECT_TRUE(map.Get(BindableActionMap::ROTATE_AROUND_MOUSE_ON));
	EXPECT_TRUE(map.Get(BindableActionMap::SHOW_VILLAGER_NAMES));
}

TEST_F(GameActionMapCursor, BothButtonsAreTakenFromTheirOwnEvents)
{
	// Both buttons held are the ones their own button events pressed, as the original's window messages set its mouse
	// buttons (OPENBLACK_MOUSE_AT set, so that the real mouse lets nothing go here)
	OverrideMouseAt("0.5,0.5");
	GameActionMap map;
	map.Frame();
	SDL_Event left {};
	left.type = SDL_MOUSEBUTTONDOWN;
	left.button.button = SDL_BUTTON_LEFT;
	left.button.clicks = 1;
	SDL_Event right = left;
	right.button.button = SDL_BUTTON_RIGHT;
	Dispatch(map, left);
	map.Frame();
	EXPECT_FALSE(map.Get(UnbindableActionMap::TWO_BUTTON_CLICK));
	Dispatch(map, right);
	map.Frame();
	EXPECT_TRUE(map.Get(UnbindableActionMap::TWO_BUTTON_CLICK));
	right.type = SDL_MOUSEBUTTONUP;
	Dispatch(map, right);
	map.Frame();
	EXPECT_FALSE(map.Get(UnbindableActionMap::TWO_BUTTON_CLICK));
}

TEST_F(GameActionMapCursor, TheRightButtonsDoubleClickIsNoBothButtonsNorAnAction)
{
	// The window gives the right button's second press of a double click as the double click alone, with no button
	// held: with the left held it makes no both buttons, and it presses no action
	OverrideMouseAt("0.5,0.5");
	GameActionMap map;
	map.Frame();
	SDL_Event left {};
	left.type = SDL_MOUSEBUTTONDOWN;
	left.button.button = SDL_BUTTON_LEFT;
	left.button.clicks = 1;
	SDL_Event right = left;
	right.button.button = SDL_BUTTON_RIGHT;
	SDL_Event rightUp = right;
	rightUp.type = SDL_MOUSEBUTTONUP;
	Dispatch(map, left);
	Dispatch(map, right);
	Dispatch(map, rightUp);
	map.Frame();
	EXPECT_FALSE(map.Get(UnbindableActionMap::TWO_BUTTON_CLICK));
	right.button.clicks = 2;
	Dispatch(map, right);
	map.Frame();
	EXPECT_FALSE(map.Get(UnbindableActionMap::TWO_BUTTON_CLICK));
	EXPECT_FALSE(map.Get(BindableActionMap::ACTION));
	// the row's third press is a plain press again
	rightUp.button.clicks = 2;
	Dispatch(map, rightUp);
	right.button.clicks = 3;
	Dispatch(map, right);
	map.Frame();
	EXPECT_TRUE(map.Get(UnbindableActionMap::TWO_BUTTON_CLICK));
}

TEST_F(GameActionMapCursor, ButtonLetGoElsewhereIsLetGoOutsideTestRuns)
{
	if (FixedMouse().has_value() || IgnoreRealInput())
	{
		GTEST_SKIP() << "the run's environment fixes the mouse";
	}
	// OPENBLACK_MOUSE_AT unset: the window let go of the middle button, and the event went elsewhere
	OverrideMouseAt("");
	GameActionMap map;
	map.Frame();
	Dispatch(map, MiddleButtonEvent(SDL_MOUSEBUTTONDOWN));
	EXPECT_TRUE(map.Get(BindableActionMap::ROTATE_AROUND_MOUSE_ON));
	map.SeeDispatchedEvent(MiddleButtonEvent(SDL_MOUSEBUTTONUP));
	map.Frame();
	EXPECT_FALSE(map.Get(BindableActionMap::ROTATE_AROUND_MOUSE_ON));
}

TEST_F(GameActionMapCursor, KeysAreKeptWithoutAWindowHoldingTheKeyboard)
{
	if (FixedMouse().has_value() || IgnoreRealInput())
	{
		GTEST_SKIP() << "the run's environment fixes the mouse";
	}
	// Outside the test runs, but no window has the keyboard, so SDL's state of the keys isn't trusted
	OverrideMouseAt("");
	GameActionMap map;
	map.Frame();
	Dispatch(map, KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_N));
	map.Frame();
	EXPECT_TRUE(map.Get(BindableActionMap::SHOW_VILLAGER_NAMES));
}

namespace
{
SDL_Event KeyDown(SDL_Scancode key, uint16_t modifiers)
{
	SDL_Event event {};
	event.type = SDL_KEYDOWN;
	event.key.keysym.scancode = key;
	event.key.keysym.mod = modifiers;
	return event;
}

SDL_Event MouseEvent(MouseInput mouse)
{
	SDL_Event event {};
	switch (mouse)
	{
	case MouseInput::WheelUp:
	case MouseInput::WheelDown:
		event.type = SDL_MOUSEWHEEL;
		event.wheel.y = mouse == MouseInput::WheelUp ? 1 : -1;
		break;
	default:
		event.type = SDL_MOUSEBUTTONDOWN;
		event.button.clicks = 1;
		event.button.button = mouse == MouseInput::LeftButton     ? SDL_BUTTON_LEFT
		                      : mouse == MouseInput::MiddleButton ? SDL_BUTTON_MIDDLE
		                                                          : SDL_BUTTON_RIGHT;
		break;
	}
	return event;
}

uint16_t SdlModifier(Modifier modifier)
{
	switch (modifier)
	{
	case Modifier::Ctrl:
		return KMOD_LCTRL;
	case Modifier::Shift:
		return KMOD_LSHIFT;
	case Modifier::Alt:
		return KMOD_LALT;
	default:
		return KMOD_NONE;
	}
}

struct ExpectedBinding
{
	BindableActionMap action;
	std::string_view name;
	SDL_Scancode key;
	Modifier modifier;
	MouseInput mouse;
};

// The options screen's controls list as the game sets it up, in its order
// clang-format off
constexpr std::array<ExpectedBinding, k_KeyBindingCount> k_Documented {{
	{BindableActionMap::HELP,                   "Help",                        SDL_SCANCODE_F1,      Modifier::None, MouseInput::None},
	{BindableActionMap::MOVE,                   "Move",                        SDL_SCANCODE_UNKNOWN, Modifier::None, MouseInput::LeftButton},
	{BindableActionMap::ACTION,                 "Action",                      SDL_SCANCODE_UNKNOWN, Modifier::None, MouseInput::RightButton},
	{BindableActionMap::ZOOM_OUT,               "Zoom Out",                    SDL_SCANCODE_UNKNOWN, Modifier::None, MouseInput::WheelDown},
	{BindableActionMap::ZOOM_IN,                "Zoom In",                     SDL_SCANCODE_UNKNOWN, Modifier::None, MouseInput::WheelUp},
	{BindableActionMap::TALK,                   "Talk",                        SDL_SCANCODE_T,       Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_ON,                "Zoom On",                     SDL_SCANCODE_RCTRL,   Modifier::None, MouseInput::None},
	{BindableActionMap::MOVE_LEFT,              "Move Left",                   SDL_SCANCODE_LEFT,    Modifier::None, MouseInput::None},
	{BindableActionMap::MOVE_RIGHT,             "Move Right",                  SDL_SCANCODE_RIGHT,   Modifier::None, MouseInput::None},
	{BindableActionMap::MOVE_FORWARDS,          "Move Forwards",               SDL_SCANCODE_UP,      Modifier::None, MouseInput::None},
	{BindableActionMap::MOVE_BACKWARDS,         "Move Backwards",              SDL_SCANCODE_DOWN,    Modifier::None, MouseInput::None},
	{BindableActionMap::TILT_UP,                "Tilt Up",                     SDL_SCANCODE_A,       Modifier::None, MouseInput::None},
	{BindableActionMap::TILT_DOWN,              "Tilt Down",                   SDL_SCANCODE_Q,       Modifier::None, MouseInput::None},
	{BindableActionMap::ROTATE_LEFT,            "Rotate Left",                 SDL_SCANCODE_Z,       Modifier::None, MouseInput::None},
	{BindableActionMap::ROTATE_RIGHT,           "Rotate Right",                SDL_SCANCODE_U,       Modifier::None, MouseInput::None},
	{BindableActionMap::ROTATE_ON,              "Tilt/Rotate On",              SDL_SCANCODE_RSHIFT,  Modifier::None, MouseInput::None},
	{BindableActionMap::ROTATE_AROUND_MOUSE_ON, "Tilt/Rotate Around Mouse On", SDL_SCANCODE_UNKNOWN, Modifier::None, MouseInput::MiddleButton},
	{BindableActionMap::ZOOM_TO_TEMPLE,         "Zoom To Temple",              SDL_SCANCODE_SPACE,   Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_TO_CREATURE,       "Zoom To Creature",            SDL_SCANCODE_C,       Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_TO_REALM,          "Zoom To Realm",               SDL_SCANCODE_F3,      Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_TO_INSIDE_TEMPLE,  "Zoom To Inside Temple",       SDL_SCANCODE_F4,      Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_TO_CREATURE_ROOM,  "Zoom To Creature Room",       SDL_SCANCODE_F5,      Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_TO_CHALLENGE_ROOM, "Zoom To Challenge Room",      SDL_SCANCODE_F6,      Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_TO_SAVE_GAME_ROOM, "Zoom To Save Game Room",      SDL_SCANCODE_F7,      Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_TO_OPTIONS_ROOM,   "Zoom To Options Room",        SDL_SCANCODE_F8,      Modifier::None, MouseInput::None},
	{BindableActionMap::ZOOM_TO_LIBRARY,        "Zoom To Library",             SDL_SCANCODE_F9,      Modifier::None, MouseInput::None},
	{BindableActionMap::LEASH_UNLEASH_CREATURE, "Leash/UnLeash Creature",      SDL_SCANCODE_L,       Modifier::None, MouseInput::None},
	{BindableActionMap::SHOW_VILLAGER_NAMES,    "Show Villager Names",         SDL_SCANCODE_N,       Modifier::None, MouseInput::None},
	{BindableActionMap::SHOW_VILLAGER_DETAILS,  "Show Villager Details",       SDL_SCANCODE_S,       Modifier::None, MouseInput::None},
	{BindableActionMap::QUICK_SAVE,             "Quick Save",                  SDL_SCANCODE_S,       Modifier::Ctrl, MouseInput::None},
	{BindableActionMap::QUICK_LOAD,             "Quick Load",                  SDL_SCANCODE_L,       Modifier::Ctrl, MouseInput::None},
	{BindableActionMap::PREVIOUS_LEASH,         "Previous Leash",              SDL_SCANCODE_V,       Modifier::None, MouseInput::None},
	{BindableActionMap::NEXT_LEASH,             "Next Leash",                  SDL_SCANCODE_B,       Modifier::None, MouseInput::None},
}};
// clang-format on
} // namespace

TEST(KeyBindings, DefaultsMatchTheOptionsScreen)
{
	for (size_t i = 0; i < k_KeyBindingCount; ++i)
	{
		const auto& binding = k_DefaultKeyBindings.at(i);
		const auto& expected = k_Documented.at(i);
		SCOPED_TRACE(expected.name);
		EXPECT_EQ(binding.action, expected.action);
		EXPECT_EQ(binding.name, expected.name);
		EXPECT_EQ(binding.mouse, expected.mouse);
		if (expected.key == SDL_SCANCODE_UNKNOWN)
		{
			EXPECT_FALSE(binding.key.has_value());
		}
		else
		{
			ASSERT_TRUE(binding.key.has_value());
			EXPECT_EQ(binding.key->key, expected.key);
			EXPECT_EQ(binding.key->modifier, expected.modifier);
		}
	}
}

/// One row for each bindable action
TEST(KeyBindings, EveryActionIsInTheTableOnce)
{
	uint64_t seen = 0;
	for (const auto& binding : k_DefaultKeyBindings)
	{
		const auto bit = static_cast<uint64_t>(binding.action);
		EXPECT_EQ(seen & bit, 0u) << binding.name;
		seen |= bit;
	}
	EXPECT_EQ(seen, static_cast<uint64_t>(BindableActionMap::ALL));
}

/// The table the Key Bindings window shows is the game's action map: each row's key, with its modifier, or its mouse
/// input makes our action map hold that row's action
TEST(KeyBindings, TableIsTheGamesActionMap)
{
	for (const auto& binding : k_DefaultKeyBindings)
	{
		GameActionMap actions;
		if (binding.key.has_value())
		{
			actions.ProcessEvent(KeyDown(binding.key->key, SdlModifier(binding.key->modifier)));
		}
		else
		{
			actions.ProcessEvent(MouseEvent(binding.mouse));
		}
		EXPECT_TRUE(actions.GetBindable(binding.action)) << binding.name;
	}
}

/// Where the table names the right Ctrl or Shift, the left one does the same
TEST(KeyBindings, EitherSideOfAModifier)
{
	for (const auto key : {SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL})
	{
		GameActionMap actions;
		actions.ProcessEvent(KeyDown(key, KMOD_NONE));
		EXPECT_TRUE(actions.GetBindable(BindableActionMap::ZOOM_ON));
	}
	for (const auto key : {SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT})
	{
		GameActionMap actions;
		actions.ProcessEvent(KeyDown(key, KMOD_NONE));
		EXPECT_TRUE(actions.GetBindable(BindableActionMap::ROTATE_ON));
	}
}

TEST(KeyBindings, Names)
{
	EXPECT_EQ(KeyChordName({.key = SDL_SCANCODE_S, .modifier = Modifier::Ctrl}), "Ctrl + S");
	EXPECT_EQ(KeyChordName({.key = SDL_SCANCODE_F1, .modifier = Modifier::Shift}), "Shift + F1");
	EXPECT_EQ(KeyChordName({.key = SDL_SCANCODE_RCTRL, .modifier = Modifier::Shift}), "Shift + Ctrl");
	EXPECT_EQ(KeyChordName({.key = SDL_SCANCODE_F1}), "F1");
	EXPECT_EQ(KeyChordName({.key = SDL_SCANCODE_RCTRL}), "Ctrl");
	EXPECT_EQ(MouseInputName(MouseInput::WheelUp), "Wheel Up");
	EXPECT_EQ(CategoryName(BindCategory::Places), "Places");
}

TEST(KeyBindings, DefaultsDoNotConflict)
{
	EXPECT_TRUE(FindConflicts(k_DefaultKeyBindings).empty());
	// Ctrl+S quick saves rather than also showing villagers' details
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_LCTRL), BindableActionMap::QUICK_SAVE);
	EXPECT_EQ(ActionsForKeyDown(k_DefaultKeyBindings, SDL_SCANCODE_S, KMOD_NONE), BindableActionMap::SHOW_VILLAGER_DETAILS);
}

TEST(KeyBindings, TheTestbedsCameraKeysReachTheActionsCreatureModesCameraReads)
{
	const auto keys = testbed_scenarios::CameraKeysOf(k_DefaultKeyBindings, 0, false);
	ASSERT_TRUE(keys.has_value());
	GameActionMap actions;
	const auto give = [&actions](SDL_Scancode key, bool down) {
		SDL_Event event {};
		event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
		event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
		event.key.keysym.scancode = key;
		actions.ProcessEvent(event);
	};
	for (const auto key : *keys)
	{
		give(key, true);
	}
	EXPECT_TRUE(actions.Get(BindableActionMap::ROTATE_ON));
	EXPECT_TRUE(actions.Get(BindableActionMap::MOVE_LEFT));
	EXPECT_FALSE(actions.Get(BindableActionMap::ZOOM_ON));
	give(keys->at(1), false);
	give(keys->at(0), false);
	EXPECT_FALSE(actions.Get(BindableActionMap::ROTATE_ON));
	EXPECT_FALSE(actions.Get(BindableActionMap::MOVE_LEFT));
}

TEST(KeyBindings, TheTestbedsDoubleClickReachesTheActionsAsTheWindowsOwn)
{
	GameActionMap actions;
	const auto down = testbed_scenarios::DoubleClickEvent(true, 10, 20, 3);
	EXPECT_EQ(down.type, SDL_MOUSEBUTTONDOWN);
	EXPECT_EQ(down.button.button, SDL_BUTTON_LEFT);
	EXPECT_EQ(down.button.clicks, 2);
	EXPECT_EQ(down.button.x, 10);
	EXPECT_EQ(down.button.y, 20);
	EXPECT_EQ(down.button.windowID, 3u);
	actions.ProcessEvent(down);
	// Creature Mode takes a double click as it goes down; it is not a press of the Move button
	EXPECT_TRUE(actions.Get(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_TRUE(actions.GetChanged(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_FALSE(actions.Get(BindableActionMap::MOVE));
	// Its letting go leaves it waiting until it is taken
	actions.ProcessEvent(testbed_scenarios::DoubleClickEvent(false, 10, 20, 3));
	EXPECT_TRUE(actions.Get(UnbindableActionMap::DOUBLE_CLICK));
	EXPECT_TRUE(actions.TakeDoubleClick());
	EXPECT_FALSE(actions.Get(UnbindableActionMap::DOUBLE_CLICK));
}

TEST(GameActionMap, BothButtonsTogetherNeverGripTheLandNorAct)
{
	const auto bits = [](BindableActionMap action) { return static_cast<uint64_t>(action); };
	const auto left = ActionsForMouse(k_DefaultKeyBindings, MouseInput::LeftButton);
	const auto right = ActionsForMouse(k_DefaultKeyBindings, MouseInput::RightButton);
	const auto held = static_cast<BindableActionMap>(bits(BindableActionMap::MOVE) | bits(BindableActionMap::ACTION) |
	                                                 bits(BindableActionMap::ZOOM_IN));
	EXPECT_EQ(ClearedForBothButtons(held, BindableActionMap::NONE, BindableActionMap::NONE, left, right),
	          BindableActionMap::ZOOM_IN);
	// what either button does with its modifier key goes too
	const auto rotating = static_cast<BindableActionMap>(bits(BindableActionMap::ROTATE_ON) | bits(BindableActionMap::MOVE));
	EXPECT_EQ(ClearedForBothButtons(rotating, BindableActionMap::ROTATE_ON, BindableActionMap::NONE, left, right),
	          BindableActionMap::NONE);
	// nothing else is touched
	EXPECT_EQ(ClearedForBothButtons(BindableActionMap::TALK, BindableActionMap::NONE, BindableActionMap::NONE, left, right),
	          BindableActionMap::TALK);
}

namespace
{
/// The actions held, the mouse's moves and the wheel's notches after a frame's events: what the game reads of them
struct DerivedInput
{
	std::array<bool, 64> held {};
	glm::ivec2 delta {0, 0};
	float wheel = 0.0f;
	glm::uvec2 cursor {0, 0};
	bool frozen = false;

	friend bool operator==(const DerivedInput&, const DerivedInput&) = default;
};

[[nodiscard]] DerivedInput Derived(const GameActionMap& map)
{
	DerivedInput derived {.delta = map.GetMouseDelta(),
	                      .wheel = map.GetMouseWheelDelta(),
	                      .cursor = map.GetMousePosition(),
	                      .frozen = map.IsCursorFrozen()};
	for (size_t bit = 0; bit < derived.held.size(); ++bit)
	{
		derived.held.at(bit) = map.Get(static_cast<BindableActionMap>(uint64_t {1} << bit));
	}
	return derived;
}

[[nodiscard]] SDL_Event Motion(int x, int y, int xrel, int yrel)
{
	SDL_Event event {};
	event.type = SDL_MOUSEMOTION;
	event.motion.x = x;
	event.motion.y = y;
	event.motion.xrel = xrel;
	event.motion.yrel = yrel;
	return event;
}
} // namespace

// A middle-button drag with Num Lock on, a fine wheel's turn and a modifier held give the game the same input frame by
// frame from the window's own events as from their recording played back
TEST_F(GameActionMapCursor, ADragTheWheelAndTheModifiersReadTheSameFromTheirRecording)
{
	OverrideMouseAt("0.5,0.5");
	constexpr uint16_t k_NumLock = KMOD_NUM;
	auto middleDown = MiddleButtonEvent(SDL_MOUSEBUTTONDOWN);
	middleDown.button.x = 435;
	middleDown.button.y = 436;
	auto middleUp = MiddleButtonEvent(SDL_MOUSEBUTTONUP);
	middleUp.button.x = 423;
	middleUp.button.y = 497;
	const std::vector<std::vector<SDL_Event>> frames {
	    {KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_LEFT, k_NumLock), Motion(435, 436, 2, 1)},
	    {middleDown, Motion(430, 450, -5, 14)},
	    {Motion(425, 480, -5, 30), WheelEvent(0.5f)},
	    {Motion(423, 497, -2, 17), middleUp},
	    {Motion(435, 436, 12, -61), KeyEvent(SDL_KEYUP, SDL_SCANCODE_LEFT, k_NumLock)},
	    {KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_LCTRL, KMOD_LCTRL | k_NumLock),
	     KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_S, KMOD_LCTRL | k_NumLock)},
	    {WheelEvent(-1.0f, SDL_MOUSEWHEEL_FLIPPED)},
	};
	GameActionMap live;
	GameActionMap replayed;
	live.AllowCursorFreeze(true);
	replayed.AllowCursorFreeze(true);
	uint32_t frame = 0;
	for (const auto& events : frames)
	{
		live.Frame();
		replayed.Frame();
		for (const auto& event : events)
		{
			Dispatch(live, event);
			const auto recorded = recording::FromSdl(event, frame);
			ASSERT_TRUE(recorded.has_value());
			Dispatch(replayed, recording::ToSdl(*recorded));
		}
		EXPECT_EQ(Derived(live), Derived(replayed)) << "frame " << frame;
		++frame;
	}
	// the run went through what it was meant to: the drag turned the camera, the wheel's half notch came through
	GameActionMap check;
	check.Frame();
	Dispatch(check, middleDown);
	EXPECT_TRUE(check.Get(BindableActionMap::ROTATE_AROUND_MOUSE_ON));
	Dispatch(check, recording::ToSdl(*recording::FromSdl(WheelEvent(0.5f), 0)));
	EXPECT_EQ(check.GetMouseWheelDelta(), 0.5f);
}

// Num Lock on or off, a key does what it does
TEST(GameActionMap, TheLockKeysChangeNoKeysActions)
{
	GameActionMap plain;
	GameActionMap locked;
	plain.Frame();
	locked.Frame();
	Dispatch(plain, KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_LEFT));
	Dispatch(locked, KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_LEFT, KMOD_NUM | KMOD_CAPS));
	EXPECT_EQ(Derived(plain).held, Derived(locked).held);
	EXPECT_TRUE(locked.Get(BindableActionMap::MOVE_LEFT) || locked.Get(BindableActionMap::ROTATE_LEFT));
}

// A key or a button let go while something else took the event (the menu, a box, a debug window) is let go at the next
// frame, from what the window's events held: the same live as from the recording, which keeps every event
TEST_F(GameActionMapCursor, ALettingGoTakenElsewhereIsMadeUpFromTheWindowsEvents)
{
	if (FixedMouse().has_value() || IgnoreRealInput())
	{
		GTEST_SKIP() << "the run's environment fixes the mouse";
	}
	OverrideMouseAt("");
	GameActionMap map;
	map.Frame();
	Dispatch(map, KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_N));
	Dispatch(map, MiddleButtonEvent(SDL_MOUSEBUTTONDOWN));
	map.Frame();
	// still held by the window: nothing is let go
	EXPECT_TRUE(map.Get(BindableActionMap::SHOW_VILLAGER_NAMES));
	EXPECT_TRUE(map.Get(BindableActionMap::ROTATE_AROUND_MOUSE_ON));
	EXPECT_TRUE(map.IsMouseButtonHeld(MouseButton::Middle));
	// the window lets go, and the events go elsewhere
	map.SeeDispatchedEvent(KeyEvent(SDL_KEYUP, SDL_SCANCODE_N));
	map.SeeDispatchedEvent(MiddleButtonEvent(SDL_MOUSEBUTTONUP));
	EXPECT_FALSE(map.IsMouseButtonHeld(MouseButton::Middle));
	map.Frame();
	EXPECT_FALSE(map.Get(BindableActionMap::SHOW_VILLAGER_NAMES));
	EXPECT_FALSE(map.Get(BindableActionMap::ROTATE_AROUND_MOUSE_ON));
}

// The game's key buffer of a frame: a key down the same as the last one taken, key and Shift, Ctrl and Alt, is not taken
// again; another key between them, or another modifier, lets it in; 128 a frame at most; each frame starts empty
TEST(KeyBuffer, ARepeatOfTheLastKeyDownWithTheSameModifiersIsNotTakenAgain)
{
	KeyBuffer buffer;
	EXPECT_TRUE(buffer.Take(SDL_SCANCODE_SPACE, KMOD_NONE));
	EXPECT_FALSE(buffer.Take(SDL_SCANCODE_SPACE, KMOD_NONE));
	// either side's Shift is the same modifier, and the lock keys count for nothing
	EXPECT_TRUE(buffer.Take(SDL_SCANCODE_SPACE, KMOD_LSHIFT));
	EXPECT_FALSE(buffer.Take(SDL_SCANCODE_SPACE, KMOD_RSHIFT | KMOD_NUM | KMOD_CAPS));
	EXPECT_TRUE(buffer.Take(SDL_SCANCODE_HOME, KMOD_NONE));
	EXPECT_TRUE(buffer.Take(SDL_SCANCODE_SPACE, KMOD_NONE));
	EXPECT_EQ(buffer.Count(), 4u);
	buffer.Clear();
	EXPECT_TRUE(buffer.Take(SDL_SCANCODE_SPACE, KMOD_NONE));
}

TEST(KeyBuffer, AFrameTakesAHundredAndTwentyEightKeyDownsAtMost)
{
	KeyBuffer buffer;
	for (std::size_t i = 0; i < KeyBuffer::k_Capacity; ++i)
	{
		EXPECT_TRUE(buffer.Take(i % 2 == 0 ? SDL_SCANCODE_A : SDL_SCANCODE_B, KMOD_NONE));
	}
	EXPECT_EQ(KeyBuffer::k_Capacity, 128u);
	EXPECT_FALSE(buffer.Take(SDL_SCANCODE_C, KMOD_NONE));
	buffer.Clear();
	EXPECT_TRUE(buffer.Take(SDL_SCANCODE_C, KMOD_NONE));
}

TEST(GameActionMap, EachFrameTakesAHeldKeysRepeatOnce)
{
	GameActionMap map;
	map.Frame();
	EXPECT_TRUE(map.TakeKeyDown(SDL_SCANCODE_SPACE, KMOD_NONE));
	EXPECT_FALSE(map.TakeKeyDown(SDL_SCANCODE_SPACE, KMOD_NONE));
	map.Frame();
	EXPECT_TRUE(map.TakeKeyDown(SDL_SCANCODE_SPACE, KMOD_NONE));
}
