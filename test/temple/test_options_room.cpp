/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The temple's options room (Temple/OptionsRoomRules.h): it opens the options as the player arrives and goes back once
// they close

#include <gtest/gtest.h>

#include "Temple/OptionsRoomRules.h"

using namespace openblack::temple::options_room;

TEST(OptionsRoom, WaitsForTheCameraAndForNoDialog)
{
	bool opened = false;
	EXPECT_EQ(Update(opened, {.dialogUp = false, .arrived = false}), Step::None);
	EXPECT_EQ(Update(opened, {.dialogUp = true, .arrived = true}), Step::None);
	EXPECT_FALSE(opened);
}

TEST(OptionsRoom, ArrivingOpensTheOptionsOnce)
{
	bool opened = false;
	EXPECT_EQ(Update(opened, {.arrived = true}), Step::OpenOptions);
	EXPECT_TRUE(opened);
	// While they are up the room waits
	EXPECT_EQ(Update(opened, {.dialogUp = true, .arrived = true}), Step::None);
	EXPECT_TRUE(opened);
}

TEST(OptionsRoom, ClosingGoesBackAndTheNextVisitOpensThemAgain)
{
	bool opened = false;
	EXPECT_EQ(Update(opened, {.arrived = true}), Step::OpenOptions);
	EXPECT_EQ(Update(opened, {.arrived = true}), Step::GoBackToMainRoom);
	EXPECT_FALSE(opened);
	EXPECT_EQ(Update(opened, {.arrived = true}), Step::OpenOptions);
}

TEST(OptionsRoom, ARestartWaitingKeepsThePlayerInTheRoom)
{
	bool opened = true;
	EXPECT_EQ(Update(opened, {.arrived = true, .restartPending = true}), Step::None);
	EXPECT_TRUE(opened);
	// A room not yet opened still opens the options
	opened = false;
	EXPECT_EQ(Update(opened, {.arrived = true, .restartPending = true}), Step::OpenOptions);
}
