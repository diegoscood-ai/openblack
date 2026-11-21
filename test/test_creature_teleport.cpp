/*******************************************************************************
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

#include "3D/MapCoords.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureReactionRules.h"
#include "Creature/CreatureTeleportAgendas.h"
#include "Creature/LeashRules.h"

using namespace openblack;
using namespace openblack::creature_mind;

namespace
{
uint32_t NoRandom(uint32_t /*range*/)
{
	return 0;
}

/// Ten turns a second, as the game's
constexpr uint32_t k_TurnsPerSecond = 10;

const Senses k_Busy {.seconds = 0.1f, .turnsPerSecond = k_TurnsPerSecond, .bodyBusy = true, .bodyPlaying = true};
const Senses k_Free {.seconds = 0.1f, .turnsPerSecond = k_TurnsPerSecond};
} // namespace

// The walk to the stone ends within 5 m of it, looking at it with a puzzled face, and the jump is the main step; the
// game's puzzled wait after the jump is never reached and left out
TEST(CreatureTeleport, GoingToAStoneWalksUpPuzzledAndAsksForTheJump)
{
	const auto agenda = GoToTeleport(42, {100.0f, 200.0f});
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Move);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(map_coords::Quantise(100.0f), map_coords::Quantise(200.0f)));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 5.0f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Puzzled);
	ASSERT_TRUE(agenda[0].gaze.has_value() && agenda[0].gaze->point.has_value());
	EXPECT_FALSE(agenda[0].gaze->goingTowards);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Teleport);
	EXPECT_EQ(agenda[1].object, std::optional<uint32_t>(42));
	EXPECT_TRUE(agenda[1].main);
	EXPECT_FALSE(agenda[0].main);
}

// After the jump: carried to the far stone, the walk on to within 5 m of where it was going with a smile, and the stone
// letting go; with the walk refused, only the carry and the letting go
TEST(CreatureTeleport, AfterTheJumpItIsCarriedWalksOnSmilingAndIsLetGo)
{
	const auto agenda = AfterTeleport({300.0f, 50.0f}, glm::vec2 {400.0f, 60.0f});
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Transported);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(map_coords::Quantise(300.0f), map_coords::Quantise(50.0f)));
	EXPECT_EQ(agenda[1].kind, Step::Kind::Move);
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 5.0f);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Smile);
	EXPECT_EQ(agenda[2].kind, Step::Kind::StopTeleportControl);

	const auto refused = AfterTeleport({300.0f, 50.0f}, std::nullopt);
	ASSERT_EQ(refused.size(), 2u);
	EXPECT_EQ(refused[0].kind, Step::Kind::Transported);
	EXPECT_EQ(refused[1].kind, Step::Kind::StopTeleportControl);
}

// Two seconds of whole turns: 20 at ten turns a second
TEST(CreatureTeleport, AFadeLastsTwoSecondsOfWholeTurns)
{
	EXPECT_EQ(CarryTurns(10), 20u);
	EXPECT_EQ(CarryTurns(30), 60u);
	EXPECT_EQ(CarryTurns(0), 0u);
}

// The carry: on each turn after its start it fades out by one less the count's twentieth, arrives once the count runs
// out (with no fade that turn), then fades back in by the count's twentieth and is in full sight on the last
TEST(CreatureTeleport, TheCarryFadesOutArrivesAndFadesBackIn)
{
	uint16_t turns = CarryTurns(k_TurnsPerSecond);
	bool fadingIn = false;
	const float share = 1.0f / 20.0f;
	for (uint16_t left = 19; left >= 1; --left)
	{
		const auto turn = StepCarry(turns, fadingIn, k_TurnsPerSecond);
		ASSERT_TRUE(turn.fizz.has_value());
		EXPECT_EQ(*turn.fizz, 1.0f - share * static_cast<float>(left));
		EXPECT_FALSE(turn.arrive);
		EXPECT_FALSE(turn.done);
	}
	const auto arrival = StepCarry(turns, fadingIn, k_TurnsPerSecond);
	EXPECT_TRUE(arrival.arrive);
	EXPECT_FALSE(arrival.fizz.has_value());
	EXPECT_TRUE(fadingIn);
	EXPECT_EQ(turns, 20u);
	for (uint16_t left = 19; left >= 1; --left)
	{
		const auto turn = StepCarry(turns, fadingIn, k_TurnsPerSecond);
		ASSERT_TRUE(turn.fizz.has_value());
		EXPECT_EQ(*turn.fizz, share * static_cast<float>(left));
		EXPECT_FALSE(turn.done);
	}
	const auto last = StepCarry(turns, fadingIn, k_TurnsPerSecond);
	ASSERT_TRUE(last.fizz.has_value());
	EXPECT_EQ(*last.fizz, 0.0f);
	EXPECT_TRUE(last.done);
}

// The mind asks the stone for the jump as the step starts, even while the body is busy, and the step stays: only the
// stone's answer moves the agenda on
TEST(CreatureTeleport, TheJumpIsAskedForAtOnceAndTheStepStays)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, {{.kind = Step::Kind::Teleport, .object = 7, .main = true}});
	const auto first = Think(mind, k_Busy, NoRandom);
	EXPECT_EQ(first.teleport, std::optional<uint32_t>(7));
	EXPECT_TRUE(mind.stepStarted);
	const auto second = Think(mind, k_Free, NoRandom);
	EXPECT_FALSE(second.teleport.has_value());
	EXPECT_EQ(mind.step, 0u);
}

// Through the mind: the carry starts while the body is busy, fades out over 20 turns, is put at the far stone on the
// 20th, fades back in over the next 20 and the walk on starts on the turn after; the stone lets go of it as its last
// step starts and again on the next turn, when the plan is over
TEST(CreatureTeleport, TheMindCarriesItThenWalksOnThenIsLetGo)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, AfterTeleport({300.0f, 50.0f}, glm::vec2 {400.0f, 60.0f}));
	auto commands = Think(mind, k_Busy, NoRandom);
	EXPECT_TRUE(mind.stepStarted);
	EXPECT_FALSE(commands.teleportFizz.has_value());
	EXPECT_EQ(mind.carryTurns, 20u);
	uint32_t arrivedOn = 0;
	uint32_t turn = 0;
	while (mind.step == 0 && turn < 100)
	{
		++turn;
		commands = Think(mind, k_Busy, NoRandom);
		if (commands.teleportArrive.has_value())
		{
			arrivedOn = turn;
			EXPECT_EQ(*commands.teleportArrive, glm::vec2(map_coords::Quantise(300.0f), map_coords::Quantise(50.0f)));
		}
	}
	EXPECT_EQ(arrivedOn, 20u);
	EXPECT_EQ(turn, 40u);
	ASSERT_TRUE(commands.teleportFizz.has_value());
	EXPECT_EQ(*commands.teleportFizz, 0.0f);
	// The walk on starts on the next turn
	commands = Think(mind, k_Free, NoRandom);
	ASSERT_TRUE(commands.move.has_value());
	EXPECT_EQ(mind.step, 1u);
	// Skip to the letting go
	mind.step = 2;
	mind.stepStarted = false;
	commands = Think(mind, k_Busy, NoRandom);
	EXPECT_TRUE(commands.stopTeleportControl);
	EXPECT_EQ(mind.step, 2u);
	commands = Think(mind, k_Busy, NoRandom);
	EXPECT_TRUE(commands.stopTeleportControl);
	EXPECT_EQ(mind.step, 3u);
}

// A creature a teleport stone controls takes up no reaction, and the area it is kept within doesn't call it back
TEST(CreatureTeleport, AStonesControlKeepsReactionsAndTheKeptAreaOff)
{
	EXPECT_TRUE(creature_reaction_rules::IsAvailableForReaction({}, 0));
	EXPECT_FALSE(creature_reaction_rules::IsAvailableForReaction({.controlledByTeleport = true}, 255));
	const creature_leash::WalkBackCheck strayed {
	    .confined = true, .pointOnMap = true, .pointOnLand = true, .distance = 50.0f, .radius = 20.0f};
	EXPECT_TRUE(creature_leash::ShouldWalkBack(strayed));
	auto teleporting = strayed;
	teleporting.teleporting = true;
	EXPECT_FALSE(creature_leash::ShouldWalkBack(teleporting));
}

// Far from its home, the wandering creature casts teleport at itself as a helpful miracle: it goes near itself, gets
// away from itself, turns to face itself and casts at itself; at 150 m or nearer, or with no home, it doesn't
TEST(CreatureTeleport, FarFromHomeItCastsTeleportAtItselfAsAHelpfulMiracle)
{
	namespace plan_actions = creature_plan_actions;
	const auto* executor = plan_actions::For("ExploreAndCastTeleport");
	ASSERT_NE(executor, nullptr);
	EXPECT_TRUE(plan_actions::IsCast(*executor));
	plan_actions::Situation situation;
	situation.self = 9;
	EXPECT_FALSE(plan_actions::Possible(*executor, situation));
	situation.homeDistance = 150.0f;
	EXPECT_FALSE(plan_actions::Possible(*executor, situation));
	situation.homeDistance = 150.5f;
	EXPECT_TRUE(plan_actions::Possible(*executor, situation));
	const plan_actions::CastInfo cast {.magicType = 12, .gesture = 0, .height = 10.0f};
	const auto agenda = plan_actions::Agenda(*executor, std::nullopt, {}, situation, NoRandom, cast);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_GE(agenda->size(), 4u);
	const auto& steps = *agenda;
	const auto castAt = steps.size() - 1;
	EXPECT_EQ(steps[castAt].kind, Step::Kind::Cast);
	EXPECT_EQ(steps[castAt].cast.magicType, 12u);
	EXPECT_EQ(steps[castAt].cast.object, std::optional<uint32_t>(9));
	EXPECT_EQ(steps[castAt - 3].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(steps[castAt - 3].movement.object, std::optional<uint32_t>(9));
	EXPECT_EQ(steps[castAt - 2].movement.kind, Movement::Kind::GetAwayFromObject);
	EXPECT_EQ(steps[castAt - 1].movement.kind, Movement::Kind::TurnToFaceObject);
	// The plan of going to a stone is only forced, never weighed by the planner
	EXPECT_EQ(plan_actions::For(k_GoToTeleportAction), nullptr);
	EXPECT_NE(plan_actions::ForcedFor(k_GoToTeleportAction), nullptr);
}
