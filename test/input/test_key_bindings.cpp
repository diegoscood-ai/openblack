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

#include <SDL_events.h>
#include <gtest/gtest.h>

#include "Debug/TestbedHeldKeys.h"
#include "Input/FixedMouse.h"
#include "Input/GameActionMap.h"
#include "Input/GameCursor.h"
#include "Input/KeyBindings.h"
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
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_N));
	EXPECT_TRUE(Pressed(map, BindableActionMap::SHOW_VILLAGER_NAMES));
	map.Frame();
	EXPECT_TRUE(map.Get(BindableActionMap::SHOW_VILLAGER_NAMES));
	EXPECT_FALSE(map.GetChanged(BindableActionMap::SHOW_VILLAGER_NAMES));
	map.ProcessEvent(KeyEvent(SDL_KEYUP, SDL_SCANCODE_N));
	EXPECT_FALSE(map.Get(BindableActionMap::SHOW_VILLAGER_NAMES));
}

TEST(GameActionMap, AKeyIsHeldFromItsPressToItsRelease)
{
	GameActionMap map;
	EXPECT_FALSE(map.IsKeyHeld(SDL_SCANCODE_KP_ENTER));
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_KP_ENTER));
	EXPECT_TRUE(map.IsKeyHeld(SDL_SCANCODE_KP_ENTER));
	EXPECT_FALSE(map.IsKeyHeld(SDL_SCANCODE_RETURN));
	map.ProcessEvent(KeyEvent(SDL_KEYUP, SDL_SCANCODE_KP_ENTER));
	EXPECT_FALSE(map.IsKeyHeld(SDL_SCANCODE_KP_ENTER));
}

TEST(GameActionMap, ChordReplacesThePlainKey)
{
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_LCTRL, KMOD_LCTRL));
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_S, KMOD_LCTRL));
	EXPECT_TRUE(map.Get(BindableActionMap::ZOOM_ON));
	EXPECT_TRUE(Pressed(map, BindableActionMap::QUICK_SAVE));
	EXPECT_FALSE(map.Get(BindableActionMap::SHOW_VILLAGER_DETAILS));
	map.ProcessEvent(KeyEvent(SDL_KEYUP, SDL_SCANCODE_S, KMOD_LCTRL));
	EXPECT_FALSE(map.Get(BindableActionMap::QUICK_SAVE));
}

TEST(GameActionMap, RebindingMovesTheAction)
{
	GameActionMap map;
	map.SetKeyBinding(BindableActionMap::ZOOM_TO_TEMPLE, KeyChord {SDL_SCANCODE_HOME});
	map.Frame();
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE));
	EXPECT_FALSE(map.Get(BindableActionMap::ZOOM_TO_TEMPLE));
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_HOME));
	EXPECT_TRUE(Pressed(map, BindableActionMap::ZOOM_TO_TEMPLE));
	map.ResetKeyBindings();
	EXPECT_EQ(map.GetKeyBindings()[*IndexOf(map.GetKeyBindings(), BindableActionMap::ZOOM_TO_TEMPLE)].key,
	          KeyChord {SDL_SCANCODE_SPACE});
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
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_N));
	map.SetBlockedActions(BindableActionMap::NONE);
	EXPECT_TRUE(Pressed(map, BindableActionMap::SHOW_VILLAGER_NAMES));
}

TEST(GameActionMap, BlockedActionsReadAsNotHeld)
{
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_N));
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE));
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
	map.ProcessEvent(click);
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
	// Its release is a change once, then nothing
	click.type = SDL_MOUSEBUTTONUP;
	map.ProcessEvent(click);
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
	map.ProcessEvent(WheelEvent(1.0f));
	EXPECT_EQ(map.GetMouseWheelDelta(), 1.0f);
	map.ProcessEvent(WheelEvent(1.0f));
	EXPECT_EQ(map.GetMouseWheelDelta(), 2.0f);
	map.Frame();
	EXPECT_EQ(map.GetMouseWheelDelta(), 0.0f);
	// Towards the user, a fine wheel's half notch, and a wheel the system flips
	map.ProcessEvent(WheelEvent(-1.0f));
	EXPECT_EQ(map.GetMouseWheelDelta(), -1.0f);
	map.Frame();
	map.ProcessEvent(WheelEvent(0.5f));
	EXPECT_EQ(map.GetMouseWheelDelta(), 0.5f);
	map.Frame();
	map.ProcessEvent(WheelEvent(1.0f, SDL_MOUSEWHEEL_FLIPPED));
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
	map.ProcessEvent(MiddleButtonEvent(SDL_MOUSEBUTTONDOWN));
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
	map.ProcessEvent(MiddleButtonEvent(SDL_MOUSEBUTTONDOWN));
	map.Frame();
	EXPECT_TRUE(map.IsCursorFrozen());
	EXPECT_EQ(map.GetMousePosition(), heldAt);

	// The cursor moved elsewhere goes back to where it is held
	map.WarpCursor({300, 200});
	map.Frame();
	EXPECT_TRUE(map.IsCursorFrozen());
	EXPECT_EQ(map.GetMousePosition(), heldAt);

	// Letting go of the button lets go of the cursor, which stays at the held spot that frame
	map.ProcessEvent(MiddleButtonEvent(SDL_MOUSEBUTTONUP));
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
	map.ProcessEvent(MiddleButtonEvent(SDL_MOUSEBUTTONDOWN));
	map.Frame();
	ASSERT_TRUE(map.IsCursorFrozen());
	EXPECT_EQ(SDL_GetRelativeMouseMode(), SDL_FALSE);
	map.ProcessEvent(MiddleButtonEvent(SDL_MOUSEBUTTONUP));
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
	map.ProcessEvent(MiddleButtonEvent(SDL_MOUSEBUTTONDOWN));
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_N));
	map.Frame();
	map.Frame();
	EXPECT_TRUE(map.Get(BindableActionMap::ROTATE_AROUND_MOUSE_ON));
	EXPECT_TRUE(map.Get(BindableActionMap::SHOW_VILLAGER_NAMES));
}

TEST_F(GameActionMapCursor, ButtonLetGoElsewhereIsLetGoOutsideTestRuns)
{
	if (FixedMouse().has_value() || IgnoreRealInput())
	{
		GTEST_SKIP() << "the run's environment fixes the mouse";
	}
	// OPENBLACK_MOUSE_AT unset: the real mouse, which holds no button here, let go of the middle one elsewhere
	OverrideMouseAt("");
	GameActionMap map;
	map.Frame();
	map.ProcessEvent(MiddleButtonEvent(SDL_MOUSEBUTTONDOWN));
	EXPECT_TRUE(map.Get(BindableActionMap::ROTATE_AROUND_MOUSE_ON));
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
	map.ProcessEvent(KeyEvent(SDL_KEYDOWN, SDL_SCANCODE_N));
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
	EXPECT_EQ(KeyChordName({.key = SDL_SCANCODE_S, .modifier = Modifier::Ctrl}), "Ctrl+S");
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
	actions.ProcessEvent(testbed_scenarios::DoubleClickEvent(false, 10, 20, 3));
	EXPECT_FALSE(actions.Get(UnbindableActionMap::DOUBLE_CLICK));
}
