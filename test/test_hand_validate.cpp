/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// hand_validate, the interface's check of its action against its objects as the map's clear runs it: the needs of the
// 24 action states (from the original's table, as its start-up fills it) and the order of the check.

#include <cstdint>

#include <gtest/gtest.h>

#include "ECS/HandValidate.h"

using namespace openblack::ecs::hand_validate;

TEST(HandValidate, TheStatesNeedWhatTheOriginalsTableSays)
{
	for (int32_t state = 0; state < k_StateCount; ++state)
	{
		const auto needs = NeedsOf(state);
		const bool actionObject = state == 7 || state == 13;
		const bool held = state == 8 || state == 9 || state == 10 || state == 11 || state == 12 || state == 15 || state == 20;
		EXPECT_EQ(needs.actionObject, actionObject) << "state " << state;
		EXPECT_EQ(needs.heldObject, held) << "state " << state;
	}
}

TEST(HandValidate, AnIdleHandOverNothingKeepsItsAction)
{
	EXPECT_EQ(Validate({.state = 0}), Outcome::Keep);
	// the land grip and the bubble grip need nothing either
	EXPECT_EQ(Validate({.state = 2}), Outcome::Keep);
	EXPECT_EQ(Validate({.state = 17}), Outcome::Keep);
}

TEST(HandValidate, AnActionWhoseObjectIsGoneOrInAHandEnds)
{
	EXPECT_EQ(Validate({.state = 0, .actionObject = true}), Outcome::EndAction);
	EXPECT_EQ(Validate({.state = 3, .actionObject = true}), Outcome::EndAction);
	EXPECT_EQ(Validate({.state = 0, .actionObject = true, .actionObjectInteractable = true}), Outcome::Keep);
	EXPECT_EQ(Validate({.state = 0, .actionObject = true, .actionObjectInteractable = true, .actionObjectInHand = true}),
	          Outcome::EndAction);
}

TEST(HandValidate, TheGrabAndTheWaitForThePackageEndWithoutAnObject)
{
	EXPECT_EQ(Validate({.state = 7}), Outcome::EndAction);
	EXPECT_EQ(Validate({.state = 13}), Outcome::EndAction);
	// with their object still there they go on
	EXPECT_EQ(Validate({.state = 13, .actionObject = true, .actionObjectInteractable = true}), Outcome::Keep);
}

TEST(HandValidate, TheStatesOfWhatTheHandHoldsEndWhenItIsGone)
{
	for (const int32_t state : {8, 9, 10, 11, 12, 15, 20})
	{
		EXPECT_EQ(Validate({.state = state}), Outcome::EndAction) << "state " << state;
		EXPECT_EQ(Validate({.state = state, .holdsInteractable = true}), Outcome::Keep) << "state " << state;
	}
	// the other states do not look at the hand
	EXPECT_EQ(Validate({.state = 18}), Outcome::Keep);
}

TEST(HandValidate, AnInactiveInterfaceEndsItsActionUntilItIsInItsInactiveState)
{
	EXPECT_EQ(Validate({.active = false, .state = 0}), Outcome::EndAction);
	EXPECT_EQ(Validate({.active = false, .state = 3}), Outcome::EndAction);
	EXPECT_EQ(Validate({.active = false, .state = k_InactiveState}), Outcome::Keep);
	// in the inactive state its objects are still checked
	EXPECT_EQ(Validate({.active = false, .state = k_InactiveState, .actionObject = true}), Outcome::EndAction);
}

TEST(HandValidate, AScriptsWideScreenMakesAnActiveInterfaceInactiveUnlessAHandDemoPlays)
{
	EXPECT_EQ(Validate({.active = true, .scriptHoldsWideScreen = true}), Outcome::SetInactive);
	EXPECT_EQ(Validate({.active = true, .scriptHoldsWideScreen = true, .demoPlaying = true}), Outcome::Keep);
	// an interface already inactive is not made inactive again
	EXPECT_EQ(Validate({.active = false, .scriptHoldsWideScreen = true, .state = k_InactiveState}), Outcome::Keep);
}
