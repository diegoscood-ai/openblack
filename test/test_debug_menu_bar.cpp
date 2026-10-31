/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Debug/DebugGuiInterface.h"

using openblack::debug::gui::MenuBarShown;

TEST(DebugMenuBar, ShowsOnlyWithTheGamesMenu)
{
	// in play the bar is hidden; the escape menu brings it up
	EXPECT_FALSE(MenuBarShown(true, false));
	EXPECT_TRUE(MenuBarShown(true, true));
}

TEST(DebugMenuBar, AlwaysShownWithoutAGameInterface)
{
	// before the game's interface exists, the bar is the only way to the debug windows
	EXPECT_TRUE(MenuBarShown(false, false));
	EXPECT_TRUE(MenuBarShown(false, true));
}
