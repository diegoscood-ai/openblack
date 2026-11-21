/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureMiracleReactions.h"
#include "Creature/CreaturePlanActions.h"

using namespace openblack::creature_mind;

namespace
{
/// A picker that runs to a fixed point
std::optional<glm::vec2> RunsToFarAway(glm::vec2, uint32_t)
{
	return glm::vec2(-60.0f, 0.0f);
}
} // namespace

TEST(CreatureMiracleReactions, ItRunsFromANastyMiracleSometimesStartingInFright)
{
	const auto startled = RunAwayFromMiracle(
	    {10.0f, 0.0f}, 15.0f, [](uint32_t) { return 0u; }, RunsToFarAway);
	ASSERT_TRUE(startled.has_value());
	ASSERT_EQ(startled->size(), 2u);
	EXPECT_EQ((*startled)[0].kind, Step::Kind::Action);
	EXPECT_EQ((*startled)[0].animation, k_FrightenedAnimation);
	// startled with the frightened look, the run the main step
	EXPECT_TRUE((*startled)[0].frightenedLook);
	EXPECT_EQ((*startled)[0].heldEyes, Eyes::Wide);
	EXPECT_FALSE((*startled)[0].main);
	// the run is a walk at the run speed to the point picked, arriving within five times its height
	const auto& run = (*startled)[1];
	EXPECT_EQ(run.movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(run.movement.point, glm::vec2(-60.0f, 0.0f));
	EXPECT_TRUE(run.movement.run);
	EXPECT_FLOAT_EQ(run.movement.minDistance, 0.0f);
	EXPECT_FLOAT_EQ(run.movement.maxDistance, 5.0f * 15.0f);
	EXPECT_TRUE(run.main);
	// on the way its head has the look of a walk to the point, read from where the walk is going
	ASSERT_TRUE(run.gaze.has_value());
	EXPECT_TRUE(run.gaze->goingTowards);
	EXPECT_TRUE(run.gaze->towardsWalkEnd);
	EXPECT_FALSE(run.gaze->point.has_value());
	const auto unstartled = RunAwayFromMiracle(
	    {10.0f, 0.0f}, 15.0f, [](uint32_t) { return 1u; }, RunsToFarAway);
	ASSERT_TRUE(unstartled.has_value());
	EXPECT_EQ(unstartled->size(), 1u);
}

TEST(CreatureMiracleReactions, ThePointItRunsToIsPickedAsThePlanIsMadeAfterTheStartOfFrightIsDrawn)
{
	// every draw and the pick, in order
	std::vector<std::string> order;
	const auto random = [&order](uint32_t range) {
		order.push_back("draw " + std::to_string(range));
		return range == 2 ? 0u : 17u;
	};
	const auto pick = [&order](glm::vec2 place, uint32_t extra) -> std::optional<glm::vec2> {
		order.push_back("pick " + std::to_string(extra));
		// away from the place it runs from
		EXPECT_EQ(place, glm::vec2(10.0f, 0.0f));
		return glm::vec2(-60.0f, 0.0f);
	};
	const auto agenda = RunAwayFromMiracle({10.0f, 0.0f}, 15.0f, random, pick);
	ASSERT_TRUE(agenda.has_value());
	// the coin for the start of fright, then up to 40 m further than its species' distance, then the point
	EXPECT_EQ(order, (std::vector<std::string> {"draw 2", "draw 40", "pick 17"}));
	EXPECT_EQ(agenda->back().movement.point, glm::vec2(-60.0f, 0.0f));
	// with no point to run to, no plan, the draws made all the same
	order.clear();
	EXPECT_FALSE(RunAwayFromMiracle({10.0f, 0.0f}, 15.0f, random, [](glm::vec2, uint32_t) {
		             return std::optional<glm::vec2>();
	             }).has_value());
	EXPECT_EQ(order, (std::vector<std::string> {"draw 2", "draw 40"}));
	EXPECT_FALSE(RunAwayFromMiracle({10.0f, 0.0f}, 15.0f, random, {}).has_value());
}

TEST(CreatureMiracleReactions, ItGoesToLookAndPointsOrIsPuzzled)
{
	const auto pointing = ExamineMiracle({10.0f, 0.0f}, 15.0f, [](uint32_t) { return 1u; });
	ASSERT_EQ(pointing.size(), 3u);
	EXPECT_EQ(pointing[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_FLOAT_EQ(pointing[0].movement.maxDistance, 5.0f * 15.0f);
	// On its way its head has the look of a walk to the place
	ASSERT_TRUE(pointing[0].gaze.has_value());
	EXPECT_TRUE(pointing[0].gaze->goingTowards);
	EXPECT_EQ(pointing[0].gaze->point, glm::vec3(10.0f, 0.0f, 0.0f));
	EXPECT_EQ(pointing[1].movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(pointing[2].order.kind, ObjectOrder::Kind::PointAt);
	// as it turns to the place and points at it, its head is on the place on the ground, at the level of the sea
	for (const auto& step : {pointing[1], pointing[2]})
	{
		ASSERT_TRUE(step.gaze.has_value());
		EXPECT_FALSE(step.gaze->goingTowards);
		EXPECT_EQ(step.gaze->point, glm::vec3(10.0f, 0.0f, 0.0f));
	}
	// once there it points two seconds, and a second when it points before going
	EXPECT_EQ(pointing[2].order.seconds, k_LastPointSeconds);
	// the last step is the main one
	EXPECT_TRUE(pointing[2].main);
	EXPECT_FALSE(pointing[0].main);
	const auto puzzled = ExamineMiracle({10.0f, 0.0f}, 15.0f, [](uint32_t) { return 0u; });
	ASSERT_EQ(puzzled.size(), 5u);
	EXPECT_EQ(puzzled[1].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(puzzled[1].order.seconds, k_FirstPointSeconds);
	for (const auto& step : {puzzled[0], puzzled[1], puzzled[3]})
	{
		ASSERT_TRUE(step.gaze.has_value());
		EXPECT_FALSE(step.gaze->goingTowards);
		EXPECT_EQ(step.gaze->point, glm::vec3(10.0f, 0.0f, 0.0f));
	}
	// waiting puzzled, it has no look of its own
	EXPECT_FALSE(puzzled.back().gaze.has_value());
	EXPECT_EQ(puzzled.back().kind, Step::Kind::Wait);
	EXPECT_FLOAT_EQ(puzzled.back().seconds, k_PuzzledSeconds);
	EXPECT_TRUE(puzzled.back().main);
	EXPECT_FALSE(puzzled[1].main);
}

TEST(CreatureMiracleReactions, ThePlansToRunFromOrLookAtAPlaceGoAboutThePlaceItsMindKeeps)
{
	namespace actions = openblack::creature_plan_actions;
	const auto* run = actions::ForcedFor("RunAwayFromPos");
	const auto* look = actions::ForcedFor("ExaminePos");
	ASSERT_NE(run, nullptr);
	ASSERT_NE(look, nullptr);
	// Only ever forced on it: its planner never weighs them
	EXPECT_EQ(actions::For("RunAwayFromPos"), nullptr);
	EXPECT_EQ(actions::For("ExaminePos"), nullptr);
	EXPECT_EQ(run->build, actions::Build::RunFromPlace);
	EXPECT_EQ(look->build, actions::Build::ExaminePlace);
	EXPECT_TRUE(actions::WalksByItsHeight(run->build));
	EXPECT_TRUE(actions::WalksByItsHeight(look->build));
	const auto random = [](uint32_t) { return 0u; };
	actions::Situation situation {.forced = true};
	situation.height = 15.0f;
	// With no place kept, neither plan is made
	EXPECT_FALSE(actions::Agenda(*run, 7, {}, situation, random).has_value());
	EXPECT_FALSE(actions::Agenda(*look, 7, {}, situation, random).has_value());
	situation.miraclePlace = glm::vec2(10.0f, 0.0f);
	// with nothing to pick where it runs to, it doesn't run
	EXPECT_FALSE(actions::Agenda(*run, 7, {}, situation, random).has_value());
	std::vector<glm::vec2> pickedFrom;
	situation.pickRunAwayPoint = [&pickedFrom](glm::vec2 place, uint32_t) {
		pickedFrom.push_back(place);
		return std::optional(glm::vec2(-60.0f, 0.0f));
	};
	const auto fleeing = actions::Agenda(*run, 7, {}, situation, random);
	ASSERT_TRUE(fleeing.has_value());
	const auto expectedFlee = RunAwayFromMiracle({10.0f, 0.0f}, 15.0f, random, situation.pickRunAwayPoint);
	ASSERT_TRUE(expectedFlee.has_value());
	ASSERT_EQ(fleeing->size(), expectedFlee->size());
	// the run goes to the point picked away from the place its mind keeps
	EXPECT_EQ(pickedFrom.front(), glm::vec2(10.0f, 0.0f));
	EXPECT_EQ(fleeing->back().movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(fleeing->back().movement.point, glm::vec2(-60.0f, 0.0f));
	EXPECT_FLOAT_EQ(fleeing->back().movement.maxDistance, expectedFlee->back().movement.maxDistance);
	const auto looking = actions::Agenda(*look, 7, {}, situation, random);
	ASSERT_TRUE(looking.has_value());
	ASSERT_EQ(looking->size(), ExamineMiracle({10.0f, 0.0f}, 15.0f, random).size());
	EXPECT_EQ(looking->back().kind, Step::Kind::Wait);
}
