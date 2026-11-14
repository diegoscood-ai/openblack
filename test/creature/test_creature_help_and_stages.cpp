/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureHelp.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLostThing.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "ECS/GodCamera.h"

using namespace openblack;

TEST(CreaturePlanTarget, ThePlansThingWhileItIsCarriedOut)
{
	creature_planner::PlannerState state;
	EXPECT_FALSE(creature_planner::TargetObject(state).has_value());
	state.current = creature_planner::Plan {.object = 7};
	EXPECT_EQ(creature_planner::TargetObject(state), std::optional<uint32_t>(7));
}

TEST(CreaturePlanTarget, TheLastPlansThingStaysAfterThePlanStops)
{
	creature_planner::PlannerState state;
	state.current = creature_planner::Plan {.object = 7};
	creature_planner::StopPlan(state);
	EXPECT_FALSE(state.current.has_value());
	EXPECT_EQ(creature_planner::TargetObject(state), std::optional<uint32_t>(7));
}

TEST(CreaturePlanTarget, ANewPlanTakesThePlaceOfTheLastOne)
{
	creature_planner::PlannerState state;
	state.current = creature_planner::Plan {.object = 7};
	creature_planner::StopPlan(state);
	state.current = creature_planner::Plan {};
	EXPECT_FALSE(creature_planner::TargetObject(state).has_value());
	creature_planner::StopPlan(state);
	EXPECT_FALSE(creature_planner::TargetObject(state).has_value());
}

TEST(CreatureLeavesHome, AYoungCreatureDoesNotLookOutToSea)
{
	const auto* executor = creature_plan_actions::For("LookOutToSea");
	ASSERT_NE(executor, nullptr);
	creature_plan_actions::Situation situation;
	situation.stage = creature_plan_actions::k_LeaveHomeStage - 1;
	EXPECT_FALSE(creature_plan_actions::Possible(*executor, situation));
	situation.stage = creature_plan_actions::k_LeaveHomeStage;
	EXPECT_TRUE(creature_plan_actions::Possible(*executor, situation));
	// Forced on it, a young creature looks out to sea all the same
	situation.stage = creature_plan_actions::k_LeaveHomeStage - 1;
	situation.forced = true;
	EXPECT_TRUE(creature_plan_actions::Possible(*executor, situation));
}

TEST(CreatureLeavesHome, AYoungCreatureDoesNotSitDownOnTheBeach)
{
	const auto* executor = creature_plan_actions::For("SitDownOnBeach");
	ASSERT_NE(executor, nullptr);
	creature_plan_actions::Situation situation;
	situation.stage = creature_plan_actions::k_LeaveHomeStage - 1;
	EXPECT_FALSE(creature_plan_actions::Possible(*executor, situation));
	situation.stage = creature_plan_actions::k_LeaveHomeStage;
	EXPECT_TRUE(creature_plan_actions::Possible(*executor, situation));
}

TEST(CreatureRestsAgain, SittingAndScratchingAreLeftOutForTwoSecondsAfterTheyWereDone)
{
	using creature_plan_actions::DoneWithin;
	// a count of nothing, just done or never, is no recent doing; then whole seconds of turns
	EXPECT_FALSE(DoneWithin(0, 10, 2.0f));
	EXPECT_TRUE(DoneWithin(1, 10, 2.0f));
	EXPECT_TRUE(DoneWithin(19, 10, 2.0f));
	EXPECT_FALSE(DoneWithin(20, 10, 2.0f));
	EXPECT_FALSE(DoneWithin(5, 0, 2.0f));
	for (const auto* name : {"SitDown", "Scratch"})
	{
		const auto* executor = creature_plan_actions::For(name);
		ASSERT_NE(executor, nullptr);
		EXPECT_EQ(executor->notDoneWithinSeconds, std::optional(2.0f)) << name;
	}
	EXPECT_FALSE(creature_plan_actions::For("SitDownOnBeach")->notDoneWithinSeconds.has_value());
}

TEST(CreatureLostThing, ThePlansThingsAndTheStepsThingsFromTheCurrentStep)
{
	const creature_planner::Plan plan {.object = 3, .activityObject = 4, .instrument = 5};
	EXPECT_EQ(creature_lost_thing::PlanThings(plan), (std::vector<uint32_t> {4, 3, 5}));
	std::vector<creature_mind::Step> agenda(3);
	agenda[0].object = 10;
	agenda[1].movement.object = 11;
	agenda[2].cast.object = 12;
	agenda[2].order.object = 13;
	EXPECT_EQ(creature_lost_thing::StepThings(agenda, 1), (std::vector<uint32_t> {11, 13, 12}));
}

TEST(CreatureLostThing, AGoneThingIsLostUnlessTheCreatureUsedItUpItself)
{
	const auto gone = [](uint32_t thing) { return thing == 3; };
	creature_lost_thing::InUse inUse {.planThings = {3}};
	EXPECT_TRUE(creature_lost_thing::Lost(inUse, gone));
	inUse.usedByItself = {3};
	EXPECT_FALSE(creature_lost_thing::Lost(inUse, gone));
	EXPECT_FALSE(creature_lost_thing::Lost({.stepThings = {4}}, gone));
	EXPECT_TRUE(creature_lost_thing::Lost({.stepThings = {4, 3}}, gone));
}

TEST(CreatureLostThing, AStepsThingNoLongerAvailableIsLostButAPlansIsNot)
{
	const auto gone = [](uint32_t) { return false; };
	// a dying villager, say: still there, no longer available
	const auto unavailable = [](uint32_t thing) { return thing == 7; };
	EXPECT_TRUE(creature_lost_thing::Lost({.stepThings = {6, 7}}, gone, unavailable));
	// a plan's thing is lost only when it is gone, and what it used up itself never
	EXPECT_FALSE(creature_lost_thing::Lost({.planThings = {7}}, gone, unavailable));
	EXPECT_FALSE(creature_lost_thing::Lost({.stepThings = {7}, .usedByItself = {7}}, gone, unavailable));
	EXPECT_FALSE(creature_lost_thing::Lost({.stepThings = {7}}, gone));
}

TEST(CreatureLostThing, TheAdvisorSaysItLostTheThing)
{
	EXPECT_EQ(creature_help::Text(creature_help::Kind::FailureExplanation, creature_help::k_LostThing),
	          std::optional<uint32_t>(1096 + creature_help::k_LostThing));
}

TEST(GodCamera, ACreatureSeesItsOwnGodsCamera)
{
	using ecs::god_camera::Camera;
	using ecs::god_camera::Choose;
	const Camera game {.origin = {1.0f, 2.0f, 3.0f}, .focus = {4.0f, 5.0f, 6.0f}};
	const glm::vec3 hand {10.0f, 20.0f, 30.0f};
	const glm::vec3 citadel {40.0f, 50.0f, 60.0f};
	// the god at this machine, and a creature with no god, see the game's camera
	for (const auto owner : {PlayerNames::PLAYER_ONE, PlayerNames::NEUTRAL})
	{
		const auto seen = Choose(owner, PlayerNames::PLAYER_ONE, game, hand, citadel);
		ASSERT_TRUE(seen.has_value());
		EXPECT_EQ(seen->origin, game.origin);
		EXPECT_EQ(seen->focus, game.focus);
	}
	// another god's from where it reports it towards the focus its citadel set
	const auto rival = Choose(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, game, hand, citadel);
	ASSERT_TRUE(rival.has_value());
	EXPECT_EQ(rival->origin, hand);
	EXPECT_EQ(rival->focus, citadel);
	// with no reported camera or no citadel it has none
	EXPECT_FALSE(Choose(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, game, std::nullopt, citadel).has_value());
	EXPECT_FALSE(Choose(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, game, hand, std::nullopt).has_value());
}
