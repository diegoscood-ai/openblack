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

#include "ECS/HandActionPoint.h"
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

// The interface's own rules, as every frame's and turn's interface pass runs them before its action state takes a press

TEST(HandValidate, AGrabUnderWayAsAScriptsBarsComeInEndsAndNoPressIsTakenUnderThem)
{
	// the grab's wait (13) with its object, as the script's wide screen comes in: the interface is made inactive, which
	// ends the grab before it picks anything up
	const Facts grabbing {
	    .active = true, .scriptHoldsWideScreen = true, .state = 13, .actionObject = true, .actionObjectInteractable = true};
	EXPECT_EQ(ValidateInterface(grabbing), Outcome::SetInactive);
	EXPECT_FALSE(TakesPresses(grabbing));
	// the next passes: inactive, in the inactive state, which takes no press while the bars hold
	const Facts under {.active = false, .scriptHoldsWideScreen = true, .state = k_InactiveState};
	EXPECT_EQ(ValidateInterface(under), Outcome::Keep);
	EXPECT_FALSE(TakesPresses(under));
}

TEST(HandValidate, AnActionUnderWayOnAnInactiveInterfaceEndsAtTheNextPass)
{
	// a grab, a tap's wait, a locked select: each ends, whatever its objects
	for (const int32_t state : {3, 13, 19})
	{
		const Facts facts {.active = false,
		                   .scriptHoldsWideScreen = true,
		                   .state = state,
		                   .actionObject = true,
		                   .actionObjectInteractable = true};
		EXPECT_EQ(ValidateInterface(facts), Outcome::EndAction) << "state " << state;
		EXPECT_FALSE(TakesPresses(facts)) << "state " << state;
	}
}

TEST(HandValidate, WithAHandDemoPlayingUnderTheBarsTheInterfaceStaysActiveAndTakesPresses)
{
	const Facts demo {.active = true,
	                  .scriptHoldsWideScreen = true,
	                  .demoPlaying = true,
	                  .state = 13,
	                  .actionObject = true,
	                  .actionObjectInteractable = true};
	EXPECT_EQ(ValidateInterface(demo), Outcome::Keep);
	EXPECT_TRUE(TakesPresses(demo));
}

TEST(HandValidate, OutsideTheBarsTheInterfacesRulesChangeNothing)
{
	for (int32_t state = 0; state < k_StateCount; ++state)
	{
		if (state == k_InactiveState)
		{
			continue;
		}
		const Facts facts {.active = true, .state = state};
		EXPECT_EQ(ValidateInterface(facts), Outcome::Keep) << "state " << state;
		EXPECT_TRUE(TakesPresses(facts)) << "state " << state;
	}
	// the object rules are the clear's: a grab whose object is gone is not ended by the interface's rules
	EXPECT_EQ(ValidateInterface({.state = 13}), Outcome::Keep);
	EXPECT_EQ(Validate({.state = 13}), Outcome::EndAction);
}

// The object rules, as every frame's and turn's interface pass runs them after the interface's own

TEST(HandValidate, AGrabWhoseObjectHasGoneIntoTheHandEndsAtTheNextPass)
{
	// the grab's wait and the wait for the place in the hand keep their object until it is in the hand
	for (const int32_t state : {7, 13})
	{
		EXPECT_EQ(Validate({.state = state, .actionObject = true, .actionObjectInteractable = true}), Outcome::Keep)
		    << "state " << state;
		EXPECT_EQ(Validate({.state = state,
		                    .actionObject = true,
		                    .actionObjectInteractable = true,
		                    .actionObjectInHand = true,
		                    .holdsInteractable = true}),
		          Outcome::EndAction)
		    << "state " << state;
	}
}

TEST(HandValidate, AReleaseOrApplyWhoseHeldObjectIsDestroyedEndsAndAnIdleHandIsLeftAlone)
{
	// the release (12) and a seed's apply (8) with the held object destroyed under them
	EXPECT_EQ(Validate({.state = 12}), Outcome::EndAction);
	EXPECT_EQ(Validate({.state = 8}), Outcome::EndAction);
	// an idle hand holding nothing, over nothing, is not touched; nor one holding an object it can still act on
	EXPECT_EQ(Validate({.state = 0}), Outcome::Keep);
	EXPECT_EQ(Validate({.state = 0, .holdsInteractable = true}), Outcome::Keep);
	// an idle hand over an object it can act on keeps it
	EXPECT_EQ(Validate({.state = 0, .actionObject = true, .actionObjectInteractable = true}), Outcome::Keep);
}

TEST(HandValidate, TheWaitForThePlaceInTheHandKeepsItsObjectUntilItIsInTheHandThenEnds)
{
	namespace action_point = openblack::ecs::hand_action_point;
	const auto stone = static_cast<entt::entity>(15);
	const auto other = static_cast<entt::entity>(16);
	// the pick-up sent, the hand not holding it yet: an action in progress, so the interface keeps the stone whatever is
	// under the cursor, and the check keeps the wait
	ASSERT_TRUE(action_point::WaitsForPlaceInHand(true, false, false, false));
	EXPECT_EQ(action_point::CollideObject(stone, other, action_point::WaitsForPlaceInHand(true, false, false, false)), stone);
	EXPECT_EQ(Validate({.state = 7, .actionObject = true, .actionObjectInteractable = true}), Outcome::Keep);
	// a locked select or a creature's lock is not that wait
	EXPECT_FALSE(action_point::WaitsForPlaceInHand(true, false, true, false));
	EXPECT_FALSE(action_point::WaitsForPlaceInHand(true, false, false, true));
	// the stone in the hand: no longer the wait, and the check ends the action on the kept stone, now in the hand
	EXPECT_FALSE(action_point::WaitsForPlaceInHand(true, true, false, false));
	EXPECT_EQ(Validate({.state = 0,
	                    .actionObject = true,
	                    .actionObjectInteractable = true,
	                    .actionObjectInHand = true,
	                    .holdsInteractable = true}),
	          Outcome::EndAction);
}
