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

#include "Common/GUtilsDistance.h"
#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureIdleMind.h"

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
	const float apart = gutils::GetDistanceInMetres(from, glm::vec2(focus.x, focus.z));
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
