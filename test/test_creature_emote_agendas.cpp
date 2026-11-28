/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GameDistance.h"
#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLocomotion.h"

using namespace openblack;
using namespace openblack::creature_emote;
using creature_mind::Effect;
using creature_mind::Movement;
using creature_mind::Step;

namespace
{
constexpr float k_TurnSeconds = 0.1f;
constexpr uint32_t k_TurnsPerSecond = 10;

uint32_t NoDraw(uint32_t /*range*/)
{
	return 0;
}

creature_mind::Senses StandingSenses()
{
	return {.seconds = k_TurnSeconds, .turnsPerSecond = k_TurnsPerSecond};
}

/// The step that follows the camera, as the actions with the player make it, then a turn to the camera
std::vector<Step> FollowThenTurn(float height)
{
	return {{.kind = Step::Kind::FollowCamera,
	         .seconds = k_FollowCameraSeconds,
	         .movement = {.maxDistance = FollowCameraReach(height)},
	         .gaze = creature_mind::Gaze {.camera = true}},
	        {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::TurnToFace, .point = {0.0f, 50.0f}}}};
}
} // namespace

TEST(CreatureEmoteAgendas, AFaceStepPullsTheFaceForItsSecondsAndEndsAtOnce)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned,
	                    {{.kind = Step::Kind::Face, .seconds = 5.0f, .animation = 18, .effect = Effect::Completed},
	                     {.kind = Step::Kind::Wait, .seconds = 1.0f}});
	const auto commands = creature_mind::Think(mind, StandingSenses(), NoDraw);
	ASSERT_TRUE(commands.face.has_value());
	EXPECT_EQ(commands.face->face, creature_face::Face::Growl);
	EXPECT_FLOAT_EQ(commands.face->milliseconds, 5000.0f);
	EXPECT_EQ(commands.effect, Effect::Completed);
	// The next step starts on the next turn
	EXPECT_EQ(mind.step, 1u);
	EXPECT_FALSE(mind.stepStarted);
}

TEST(CreatureEmoteAgendas, FollowingTheCameraEndsWithNoPlayerOrWithItsTimeUp)
{
	uint32_t turns = 3;
	EXPECT_EQ(FollowCameraTurn({.focus = std::nullopt}, turns, 10.0f), FollowCameraAction::Done);
	turns = 0;
	EXPECT_EQ(FollowCameraTurn({.focus = glm::vec3(100.0f, 0.0f, 100.0f)}, turns, 10.0f), FollowCameraAction::Done);
}

TEST(CreatureEmoteAgendas, StandingItWalksToTheFocusUntilWithinReach)
{
	uint32_t turns = 10;
	EXPECT_EQ(FollowCameraTurn({.focus = glm::vec3(100.0f, 5.0f, 0.0f), .position = {0.0f, 0.0f}}, turns, 10.0f),
	          FollowCameraAction::MoveAgain);
	EXPECT_EQ(turns, 9u);
	EXPECT_EQ(FollowCameraTurn({.focus = glm::vec3(5.0f, 5.0f, 0.0f), .position = {0.0f, 0.0f}}, turns, 10.0f),
	          FollowCameraAction::Done);
	// A focus at the map's origin ends it as well
	EXPECT_EQ(FollowCameraTurn({.focus = glm::vec3(0.0f), .position = {500.0f, 500.0f}}, turns, 10.0f),
	          FollowCameraAction::Done);
}

// A point of focus off the map gives the follow up, moving or not; the reach is weighed along the ground in the map's
// own units, as the game's distance between two map points
TEST(CreatureEmoteAgendas, AFocusOffTheMapFailsAndTheReachIsInTheMapsUnits)
{
	uint32_t turns = 10;
	EXPECT_EQ(FollowCameraTurn({.focus = glm::vec3(-10.0f, 0.0f, 100.0f), .position = {0.0f, 0.0f}}, turns, 10.0f),
	          FollowCameraAction::Fail);
	EXPECT_EQ(turns, 9u);
	EXPECT_EQ(FollowCameraTurn({.focus = glm::vec3(100.0f, 0.0f, 9000.0f), .moving = true, .focusAhead = true}, turns, 10.0f),
	          FollowCameraAction::Fail);
	const glm::vec2 from {100.0f, 100.0f};
	const glm::vec3 focus {107.3f, 0.0f, 106.9f};
	const float apart = game_math::GetDistanceInMetres(from, glm::vec2(focus.x, focus.z));
	EXPECT_EQ(FollowCameraTurn({.focus = focus, .position = from}, turns, apart), FollowCameraAction::MoveAgain);
	EXPECT_EQ(FollowCameraTurn({.focus = focus, .position = from}, turns, std::nextafter(apart, 100.0f)),
	          FollowCameraAction::Done);
}

TEST(CreatureEmoteAgendas, MovingItWalksAgainOnlyWhenTheFocusIsNoLongerAhead)
{
	uint32_t turns = 10;
	const FollowCameraView ahead {.focus = glm::vec3(100.0f, 0.0f, 0.0f), .moving = true, .focusAhead = true};
	EXPECT_EQ(FollowCameraTurn(ahead, turns, 10.0f), FollowCameraAction::Continue);
	auto behind = ahead;
	behind.focusAhead = false;
	EXPECT_EQ(FollowCameraTurn(behind, turns, 10.0f), FollowCameraAction::MoveAgain);
}

TEST(CreatureEmoteAgendas, TheFollowStepCountsWholeTurnsAndMovesShortOfTheReach)
{
	creature_mind::IdleMind mind;
	const float reach = FollowCameraReach(10.0f);
	EXPECT_FLOAT_EQ(reach, 22.0f);
	creature_mind::Plan(mind, creature_mind::Activity::Planned, FollowThenTurn(10.0f));
	auto senses = StandingSenses();
	senses.cameraFocus = glm::vec3(200.0f, 0.0f, 0.0f);
	// The first turn sets the time: thirty seconds of turns
	static_cast<void>(creature_mind::Think(mind, senses, NoDraw));
	EXPECT_EQ(mind.followCameraTurns, 300u);
	const auto commands = creature_mind::Think(mind, senses, NoDraw);
	ASSERT_TRUE(commands.move.has_value());
	EXPECT_EQ(commands.move->kind, Movement::Kind::ToPoint);
	EXPECT_EQ(commands.move->point, glm::vec2(200.0f, 0.0f));
	EXPECT_FLOAT_EQ(commands.move->maxDistance, reach - k_FollowCameraStopShort);
	EXPECT_EQ(mind.followCameraTurns, 299u);
	// Within reach, it turns to the camera next
	senses.cameraFocus = glm::vec3(1.0f, 0.0f, 0.0f);
	static_cast<void>(creature_mind::Think(mind, senses, NoDraw));
	EXPECT_EQ(mind.step, 1u);
}

// A body busy playing an action, not walking, still counts as on its way: within reach and the focus ahead, it goes on
// rather than being done, as the game asks whether the body is busy at all
TEST(CreatureEmoteAgendas, ABodyPlayingAnActionGoesOnFollowingTheCamera)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned, FollowThenTurn(10.0f));
	auto senses = StandingSenses();
	senses.cameraFocus = glm::vec3(1.0f, 0.0f, 0.0f);
	static_cast<void>(creature_mind::Think(mind, senses, NoDraw));
	senses.bodyBusy = true;
	senses.bodyPlaying = true;
	senses.cameraFocusAhead = true;
	const auto commands = creature_mind::Think(mind, senses, NoDraw);
	EXPECT_FALSE(commands.move.has_value());
	EXPECT_EQ(mind.step, 0u);
	// free again, within reach it is done
	senses.bodyBusy = false;
	senses.bodyPlaying = false;
	static_cast<void>(creature_mind::Think(mind, senses, NoDraw));
	EXPECT_EQ(mind.step, 1u);
}

// The player is near and frightening with its nearest hand within 10 of it across the land and its attitude to the
// player below 0; a distance or an attitude that is no number passes, as the game's compares take it. The distances are
// the game's own, through its table of square roots: a hand 10 away along x reads 10.0024414 in the exe, one 9.99 away
// 9.9902439 (oracle, GUtils::GetDistance with its setup)
TEST(CreatureEmoteAgendas, ThePlayerIsFrighteningWithItsHandNearAndItsAttitudeBelowNothing)
{
	EXPECT_TRUE(PlayerNearbyAndFrightening(9.0f, -0.1f));
	EXPECT_FALSE(PlayerNearbyAndFrightening(9.0f, 0.0f));
	EXPECT_FALSE(PlayerNearbyAndFrightening(10.0f, -0.5f));
	EXPECT_FALSE(PlayerNearbyAndFrightening(std::nullopt, -0.5f));
	const float nan = std::nanf("");
	EXPECT_TRUE(PlayerNearbyAndFrightening(nan, -0.5f));
	EXPECT_TRUE(PlayerNearbyAndFrightening(9.0f, nan));
	const glm::vec3 creature {0.0f, 0.0f, 0.0f};
	const float tenAway = game_math::GetDistance(creature, glm::vec3(10.0f, 0.0f, 0.0f));
	EXPECT_EQ(tenAway, 10.00244236f);
	EXPECT_FALSE(PlayerNearbyAndFrightening(tenAway, -0.5f));
	const float justUnder = game_math::GetDistance(creature, glm::vec3(9.99f, 30.0f, 0.0f));
	EXPECT_EQ(justUnder, 9.990243911f);
	EXPECT_TRUE(PlayerNearbyAndFrightening(justUnder, -0.5f));
}

// The run from the hand: it starts while the creature walks, from where the hand is then; it is done on the next turn
// that the body plays no action, the run going on, having achieved the plan's aim only when far enough from the hand
TEST(CreatureEmoteAgendas, TheRunFromTheHandEndsOnTheFirstTurnItsBodyIsFree)
{
	const std::vector<Step> agenda {
	    {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::FleeFromHand}, .startsWhileWalking = true},
	    {.kind = Step::Kind::Wait, .seconds = 1.0f}};
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned, agenda);
	auto senses = StandingSenses();
	senses.moving = true;
	senses.bodyBusy = true;
	senses.fleeHand = creature_mind::Senses::FleeHandView {.hand = {30.0f, 40.0f}};
	const auto start = creature_mind::Think(mind, senses, NoDraw);
	ASSERT_TRUE(start.move.has_value());
	EXPECT_EQ(start.move->kind, Movement::Kind::FleeFromHand);
	EXPECT_EQ(start.move->point, glm::vec2(30.0f, 40.0f));
	EXPECT_TRUE(mind.stepStarted);
	// while the body plays an action it goes on
	senses.bodyPlaying = true;
	static_cast<void>(creature_mind::Think(mind, senses, NoDraw));
	EXPECT_EQ(mind.step, 0u);
	// on its way and playing nothing, it is done, not far enough yet
	senses.bodyPlaying = false;
	const auto done = creature_mind::Think(mind, senses, NoDraw);
	EXPECT_EQ(mind.step, 1u);
	EXPECT_FALSE(done.achievedAim);
	EXPECT_FALSE(done.stopMoving);

	// far enough from the hand, it has achieved the plan's aim
	creature_mind::Plan(mind, creature_mind::Activity::Planned, agenda);
	static_cast<void>(creature_mind::Think(mind, senses, NoDraw));
	senses.fleeHand->beyondRunAway = true;
	EXPECT_TRUE(creature_mind::Think(mind, senses, NoDraw).achievedAim);
	EXPECT_EQ(mind.step, 1u);
}

// While its body plays an action, the run from the hand works out where it would run to on every turn it waits, its
// distance drawn each time; without a player it draws nothing
TEST(CreatureEmoteAgendas, WaitingForItsBodyTheRunFromTheHandDrawsItsDistanceEachTurn)
{
	const std::vector<Step> agenda {{.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::FleeFromHand}},
	                                {.kind = Step::Kind::Wait, .seconds = 1.0f}};
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned, agenda);
	auto senses = StandingSenses();
	senses.bodyBusy = true;
	senses.bodyPlaying = true;
	senses.fleeHand = creature_mind::Senses::FleeHandView {.hand = {30.0f, 40.0f}};
	std::vector<uint32_t> draws;
	const creature_mind::Random record = [&draws](uint32_t range) {
		draws.push_back(range);
		return 0u;
	};
	for (int turn = 0; turn < 3; ++turn)
	{
		const auto waiting = creature_mind::Think(mind, senses, record);
		EXPECT_FALSE(waiting.move.has_value());
	}
	EXPECT_EQ(draws, std::vector<uint32_t>(3, creature_locomotion::k_RunAwayExtra));
	EXPECT_FALSE(mind.stepStarted);
	// without a player, nothing is drawn
	draws.clear();
	senses.fleeHand.reset();
	static_cast<void>(creature_mind::Think(mind, senses, record));
	EXPECT_TRUE(draws.empty());
}

// While its body plays an action, the run from a thing works out where it would run to on every turn it waits, its
// distance drawn each time and not used
TEST(CreatureEmoteAgendas, WaitingForItsBodyTheRunFromAThingDrawsItsDistanceEachTurn)
{
	const std::vector<Step> agenda {
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::FleeFrom, .point = glm::vec2(30.0f, 40.0f), .run = true}},
	    {.kind = Step::Kind::Wait, .seconds = 1.0f}};
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned, agenda);
	auto senses = StandingSenses();
	senses.bodyBusy = true;
	senses.bodyPlaying = true;
	std::vector<uint32_t> draws;
	const creature_mind::Random record = [&draws](uint32_t range) {
		draws.push_back(range);
		return 0u;
	};
	for (int turn = 0; turn < 3; ++turn)
	{
		const auto waiting = creature_mind::Think(mind, senses, record);
		EXPECT_FALSE(waiting.move.has_value());
	}
	EXPECT_EQ(draws, std::vector<uint32_t>(3, creature_locomotion::k_RunAwayExtra));
	EXPECT_FALSE(mind.stepStarted);
}

// Without a player there is no run, and the step is done on the next turn
TEST(CreatureEmoteAgendas, WithoutAPlayerThereIsNoRunFromTheHand)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned,
	                    {{.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::FleeFromHand}},
	                     {.kind = Step::Kind::Wait, .seconds = 1.0f}});
	const auto start = creature_mind::Think(mind, StandingSenses(), NoDraw);
	EXPECT_FALSE(start.move.has_value());
	EXPECT_EQ(mind.step, 0u);
	static_cast<void>(creature_mind::Think(mind, StandingSenses(), NoDraw));
	EXPECT_EQ(mind.step, 1u);
}
