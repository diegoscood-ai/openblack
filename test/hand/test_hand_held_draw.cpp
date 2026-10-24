/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Which hand states draw the object the hand holds, and what stops them; and when the hand's draw preparation stops
// after its state change

#include <cstdint>

#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/HandSystemDetail.h"

using namespace openblack::ecs::systems::hand_detail;

TEST(HandHeldDraw, OnlyTheTugHoldingMultiplePickUpAndGrainStatesDrawTheHeldObject)
{
	for (int32_t state = 0; state <= 25; ++state)
	{
		const bool draws = state == 3 || state == 4 || state == 6 || state == 8;
		EXPECT_EQ(DrawsHeldObject(state, true, false), draws) << "state " << state;
	}
}

TEST(HandHeldDraw, ABoxThatHasTheMouseHidesTheHeldObjectInEveryState)
{
	for (int32_t state = 0; state <= 25; ++state)
	{
		EXPECT_FALSE(DrawsHeldObject(state, true, true)) << "state " << state;
	}
}

TEST(HandHeldDraw, AnInactiveInterfaceHidesTheHeldObjectWithOrWithoutABox)
{
	for (const int32_t state : {3, 4, 6, 8})
	{
		EXPECT_FALSE(DrawsHeldObject(state, false, false)) << "state " << state;
		EXPECT_FALSE(DrawsHeldObject(state, false, true)) << "state " << state;
	}
}

TEST(HandDrawPreparation, RunsInEveryVisibleStateWithTheInterfaceActiveAndNoBox)
{
	for (int32_t state = 1; state <= 10; ++state)
	{
		EXPECT_FALSE(DrawPreparationStops(state, true, false)) << "state " << state;
	}
}

TEST(HandDrawPreparation, StopsInTheInvisibleStateWhateverTheInterface)
{
	EXPECT_TRUE(DrawPreparationStops(0, true, false));
	EXPECT_TRUE(DrawPreparationStops(0, true, true));
	EXPECT_TRUE(DrawPreparationStops(0, false, false));
}

TEST(HandDrawPreparation, ABoxThatHasTheMouseStopsItInEveryState)
{
	for (int32_t state = 0; state <= 10; ++state)
	{
		EXPECT_TRUE(DrawPreparationStops(state, true, true)) << "state " << state;
	}
}

TEST(HandDrawPreparation, AnInactiveInterfaceStopsItWithOrWithoutABox)
{
	for (int32_t state = 0; state <= 10; ++state)
	{
		EXPECT_TRUE(DrawPreparationStops(state, false, false)) << "state " << state;
		EXPECT_TRUE(DrawPreparationStops(state, false, true)) << "state " << state;
	}
}
