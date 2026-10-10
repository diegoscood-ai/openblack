/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The window's minimise pause (Windowing/WindowAway): which window events hold the game loop, with fake events

#include <cstdint>

#include <array>
#include <span>

#include <SDL_video.h>
#include <gtest/gtest.h>

#include "Windowing/WindowAway.h"

using namespace openblack::windowing;

namespace
{
/// The away state after a sequence of fake window events, from a window that is shown
bool AwayAfterEvents(std::span<const std::uint8_t> events)
{
	bool away = false;
	for (const auto event : events)
	{
		away = AwayAfter(away, ActivationFor(event));
	}
	return away;
}

constexpr std::array<std::uint8_t, 11> k_OtherEvents = {
    SDL_WINDOWEVENT_SHOWN,        SDL_WINDOWEVENT_EXPOSED,    SDL_WINDOWEVENT_MOVED,     SDL_WINDOWEVENT_RESIZED,
    SDL_WINDOWEVENT_SIZE_CHANGED, SDL_WINDOWEVENT_MAXIMIZED,  SDL_WINDOWEVENT_ENTER,     SDL_WINDOWEVENT_LEAVE,
    SDL_WINDOWEVENT_FOCUS_GAINED, SDL_WINDOWEVENT_FOCUS_LOST, SDL_WINDOWEVENT_TAKE_FOCUS};
} // namespace

TEST(WindowAway, minimisingDeactivatesAndRestoringReactivates)
{
	EXPECT_EQ(ActivationFor(SDL_WINDOWEVENT_MINIMIZED), Activation::Deactivate);
	EXPECT_EQ(ActivationFor(SDL_WINDOWEVENT_RESTORED), Activation::Reactivate);
}

TEST(WindowAway, noOtherWindowEventChangesTheActivation)
{
	for (const auto event : k_OtherEvents)
	{
		EXPECT_EQ(ActivationFor(event), Activation::Unchanged) << static_cast<int>(event);
	}
}

TEST(WindowAway, unchangedKeepsEitherState)
{
	EXPECT_FALSE(AwayAfter(false, Activation::Unchanged));
	EXPECT_TRUE(AwayAfter(true, Activation::Unchanged));
}

TEST(WindowAway, aMinimiseHoldsUntilTheRestore)
{
	constexpr std::array<std::uint8_t, 3> k_Minimise = {SDL_WINDOWEVENT_FOCUS_LOST, SDL_WINDOWEVENT_HIDDEN,
	                                                    SDL_WINDOWEVENT_MINIMIZED};
	EXPECT_TRUE(AwayAfterEvents(k_Minimise));

	constexpr std::array<std::uint8_t, 6> k_MinimiseAndRestore = {SDL_WINDOWEVENT_FOCUS_LOST, SDL_WINDOWEVENT_MINIMIZED,
	                                                              SDL_WINDOWEVENT_SHOWN,      SDL_WINDOWEVENT_EXPOSED,
	                                                              SDL_WINDOWEVENT_RESTORED,   SDL_WINDOWEVENT_FOCUS_GAINED};
	EXPECT_FALSE(AwayAfterEvents(k_MinimiseAndRestore));
}

TEST(WindowAway, switchingAwayWithoutMinimisingDoesNotHold)
{
	constexpr std::array<std::uint8_t, 4> k_AltTab = {SDL_WINDOWEVENT_FOCUS_LOST, SDL_WINDOWEVENT_LEAVE, SDL_WINDOWEVENT_ENTER,
	                                                  SDL_WINDOWEVENT_FOCUS_GAINED};
	EXPECT_FALSE(AwayAfterEvents(std::span(k_AltTab).first(2)));
	EXPECT_FALSE(AwayAfterEvents(k_AltTab));
}

TEST(WindowAway, gainingTheFocusOrMaximisingDoesNotRelease)
{
	constexpr std::array<std::uint8_t, 4> k_Events = {SDL_WINDOWEVENT_MINIMIZED, SDL_WINDOWEVENT_FOCUS_GAINED,
	                                                  SDL_WINDOWEVENT_MAXIMIZED, SDL_WINDOWEVENT_SHOWN};
	EXPECT_TRUE(AwayAfterEvents(k_Events));
}

TEST(WindowAway, aRepeatedMinimiseOrRestoreChangesNothingMore)
{
	constexpr std::array<std::uint8_t, 2> k_TwoMinimises = {SDL_WINDOWEVENT_MINIMIZED, SDL_WINDOWEVENT_MINIMIZED};
	EXPECT_TRUE(AwayAfterEvents(k_TwoMinimises));
	// a restore from maximised, with the window shown, is no release from anything
	constexpr std::array<std::uint8_t, 2> k_RestoreShown = {SDL_WINDOWEVENT_MAXIMIZED, SDL_WINDOWEVENT_RESTORED};
	EXPECT_FALSE(AwayAfterEvents(k_RestoreShown));
}

TEST(WindowAway, theLoopWaitsOnlyWhileAwayAndRunning)
{
	EXPECT_FALSE(WaitsForWindow(false, true));
	EXPECT_TRUE(WaitsForWindow(true, true));
	// quitting while minimised ends the wait
	EXPECT_FALSE(WaitsForWindow(true, false));
	EXPECT_FALSE(WaitsForWindow(false, false));
}

TEST(WindowAway, aRunThatIsNeverMinimisedNeverWaits)
{
	// the deterministic runs: focus changes and resizes only
	bool away = false;
	for (const auto event : k_OtherEvents)
	{
		away = AwayAfter(away, ActivationFor(event));
		EXPECT_FALSE(WaitsForWindow(away, true)) << static_cast<int>(event);
	}
}
