/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/ComputerHand.h"

using namespace openblack;
using namespace openblack::computer_hand;
using openblack::ecs::components::ComputerHand;
using openblack::ecs::components::HandArgument;
using openblack::ecs::components::HandStep;
using openblack::ecs::components::HandSubAction;

namespace
{

/// Stages that give the results the test lines up, and a record of what ran
class FakeRunner final: public StageRunner
{
public:
	StageResult Run(ComputerHand& /*hand*/, HandStep& step, uint8_t stage) override
	{
		ran.emplace_back(step.type, stage);
		if (results.empty())
		{
			return StageResult::Done;
		}
		const auto result = results.front();
		results.pop_front();
		return result;
	}
	void LetGo(ComputerHand& hand) override
	{
		++letGo;
		hand.heldObject = entt::null;
	}
	[[nodiscard]] bool IsGone(entt::entity thing) const override { return gone.contains(thing); }
	[[nodiscard]] bool CanBePickedUp(entt::entity thing) const override { return !refused.contains(thing); }
	bool TakeIntoHand(ComputerHand& /*hand*/, entt::entity thing) override
	{
		taken.push_back(thing);
		return goesIn;
	}

	std::deque<StageResult> results;
	std::vector<std::tuple<HandSubAction, uint8_t>> ran;
	int letGo {0};
	std::set<entt::entity> gone;
	std::set<entt::entity> refused;
	std::vector<entt::entity> taken;
	bool goesIn {true};
};

/// What the test says of the things: which are objects and where the markers are
class FakeFacts final: public ThingFacts
{
public:
	[[nodiscard]] bool IsObject(entt::entity thing) const override { return objects.contains(thing); }
	[[nodiscard]] bool HasPlace(entt::entity thing) const override { return thing != entt::null && !placeless.contains(thing); }
	[[nodiscard]] std::optional<glm::vec3> MarkerPoint(entt::entity thing) const override
	{
		const auto found = markers.find(thing);
		return found != markers.end() ? std::optional<glm::vec3>(found->second) : std::nullopt;
	}

	std::set<entt::entity> objects;
	std::set<entt::entity> placeless;
	std::map<entt::entity, glm::vec3> markers;
};

/// Flat land at 5 m, wherever the hand asks
class FlatGround final: public Ground
{
public:
	[[nodiscard]] float AltitudeAt(int32_t /*mapX*/, int32_t /*mapZ*/) const override { return 5.0f; }
};

/// The float of the bits the original gave
float Bits(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

/// A hand at the point going to the target at the speed
ComputerHand HandGoing(glm::vec3 point, glm::vec3 target, float speed)
{
	ComputerHand hand;
	hand.point = point;
	hand.target = target;
	hand.speed = speed;
	return hand;
}

HandArgument PointAt(float x, float y, float z)
{
	return {.kind = HandArgument::Kind::Point, .point = {x, y, z}};
}

HandArgument ThingOf(entt::entity thing)
{
	return {.kind = HandArgument::Kind::Thing, .thing = thing};
}

} // namespace

TEST(ComputerHand, ReadyOnlyWithoutSteps)
{
	ComputerHand hand;
	EXPECT_TRUE(IsReady(hand));
	Append(hand, {.type = HandSubAction::MoveHandToPos, .arguments = {PointAt(1, 2, 3)}});
	EXPECT_FALSE(IsReady(hand));
}

TEST(ComputerHand, PrerequisitesFromTheGamesTable)
{
	EXPECT_EQ(Prerequisite(HandSubAction::PickUpObject), HandSubAction::MoveHandToObject);
	EXPECT_EQ(Prerequisite(HandSubAction::DropObjectInHandAtPos), HandSubAction::MoveHandToPos);
	EXPECT_EQ(Prerequisite(HandSubAction::ThrowObjectInHandAtPos), HandSubAction::MoveHandNearPos);
	EXPECT_EQ(Prerequisite(HandSubAction::MoveHandToPos), HandSubAction::None);
	EXPECT_EQ(Prerequisite(HandSubAction::MoveHandToObject), HandSubAction::None);
	EXPECT_EQ(Prerequisite(HandSubAction::SetHandToPos), HandSubAction::None);
	EXPECT_EQ(Prerequisite(HandSubAction::SetHandToPosDirectY), HandSubAction::None);
	EXPECT_EQ(Prerequisite(HandSubAction::None), HandSubAction::None);
}

TEST(ComputerHand, AStepWithoutAMoveTakesThreeTurns)
{
	ComputerHand hand;
	FakeRunner runner;
	Append(hand, {.type = HandSubAction::MoveHandToPos, .arguments = {PointAt(1, 2, 3)}});
	EXPECT_TRUE(Step(hand, runner));
	EXPECT_TRUE(Step(hand, runner));
	EXPECT_FALSE(IsReady(hand));
	EXPECT_TRUE(Step(hand, runner));
	EXPECT_TRUE(IsReady(hand));
	const std::vector<std::tuple<HandSubAction, uint8_t>> expected {
	    {HandSubAction::MoveHandToPos, 0}, {HandSubAction::MoveHandToPos, 1}, {HandSubAction::MoveHandToPos, 2}};
	EXPECT_EQ(runner.ran, expected);
}

TEST(ComputerHand, AStageGoingOnStaysOnItsStage)
{
	ComputerHand hand;
	FakeRunner runner;
	runner.results = {StageResult::Done, StageResult::Going, StageResult::Going, StageResult::Done};
	Append(hand, {.type = HandSubAction::MoveHandToPos, .arguments = {PointAt(1, 2, 3)}});
	for (int turn = 0; turn < 5; ++turn)
	{
		Step(hand, runner);
	}
	EXPECT_TRUE(IsReady(hand));
	ASSERT_EQ(runner.ran.size(), 5u);
	EXPECT_EQ(std::get<1>(runner.ran.at(3)), 1);
	EXPECT_EQ(std::get<1>(runner.ran.at(4)), 2);
}

TEST(ComputerHand, TheMoveGoesInFrontWithACopyOfTheFirstArgument)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto villager = static_cast<entt::entity>(7);
	Append(hand, {.type = HandSubAction::PickUpObject,
	              .arguments = {ThingOf(villager), {.kind = HandArgument::Kind::Float, .floatValue = 150.0f}}});
	// The first turn only puts the move in front
	EXPECT_TRUE(Step(hand, runner));
	EXPECT_TRUE(runner.ran.empty());
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.front().type, HandSubAction::MoveHandToObject);
	ASSERT_EQ(hand.steps.front().arguments.size(), 1u);
	EXPECT_EQ(hand.steps.front().arguments.front().thing, villager);
	// The move runs its three stages, then the pick up its three without a second move in front
	for (int turn = 0; turn < 6; ++turn)
	{
		Step(hand, runner);
	}
	EXPECT_TRUE(IsReady(hand));
	const std::vector<std::tuple<HandSubAction, uint8_t>> expected {
	    {HandSubAction::MoveHandToObject, 0}, {HandSubAction::MoveHandToObject, 1}, {HandSubAction::MoveHandToObject, 2},
	    {HandSubAction::PickUpObject, 0},     {HandSubAction::PickUpObject, 1},     {HandSubAction::PickUpObject, 2}};
	EXPECT_EQ(runner.ran, expected);
}

TEST(ComputerHand, TheNextStepIsLookedAtForItsOwnMove)
{
	ComputerHand hand;
	FakeRunner runner;
	Append(hand, {.type = HandSubAction::MoveHandToPos, .arguments = {PointAt(1, 2, 3)}});
	Append(hand, {.type = HandSubAction::DropObjectInHandAtPos, .arguments = {PointAt(4, 5, 6)}});
	for (int turn = 0; turn < 3; ++turn)
	{
		Step(hand, runner);
	}
	ASSERT_EQ(hand.steps.size(), 1u);
	Step(hand, runner);
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.front().type, HandSubAction::MoveHandToPos);
	EXPECT_EQ(hand.steps.front().arguments.front().point.x, 4.0f);
}

TEST(ComputerHand, AFailedStageGivesUpEveryStepAndLetsGo)
{
	ComputerHand hand;
	FakeRunner runner;
	runner.results = {StageResult::Done, StageResult::Failed};
	Append(hand, {.type = HandSubAction::MoveHandToPos, .arguments = {PointAt(1, 2, 3)}});
	Append(hand, {.type = HandSubAction::MoveHandToPos, .arguments = {PointAt(4, 5, 6)}});
	hand.busy = true;
	EXPECT_TRUE(Step(hand, runner));
	EXPECT_FALSE(Step(hand, runner));
	EXPECT_TRUE(IsReady(hand));
	EXPECT_EQ(runner.letGo, 1);
	EXPECT_EQ(hand.stage, 0);
	EXPECT_TRUE(hand.checkPrerequisite);
	EXPECT_FALSE(hand.busy);
}

TEST(ComputerHand, AGoneThingClearsTheSteps)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto scaffold = static_cast<entt::entity>(3);
	Append(hand, {.type = HandSubAction::PickUpObject, .arguments = {ThingOf(scaffold)}});
	SweepGone(hand, runner);
	EXPECT_FALSE(IsReady(hand));
	runner.gone.insert(scaffold);
	SweepGone(hand, runner);
	EXPECT_TRUE(IsReady(hand));
	EXPECT_EQ(runner.letGo, 1);
}

TEST(ComputerHand, AThingArgumentWithoutAThingClearsTheSteps)
{
	ComputerHand hand;
	FakeRunner runner;
	Append(hand, {.type = HandSubAction::PickUpObject, .arguments = {ThingOf(entt::null)}});
	SweepGone(hand, runner);
	EXPECT_TRUE(IsReady(hand));
}

TEST(ComputerHand, TheMapPlaceIsCutTowardZero)
{
	ComputerHand hand;
	hand.point = {1.0f, 50.0f, -1.0f};
	KeepMapPlace(hand);
	EXPECT_EQ(hand.mapX, 6553);
	EXPECT_EQ(hand.mapZ, -6553);
}

// The values below are the original's own, from its move functions run on the same hand (dev/tools/oracle), with the
// land at 5 m and the turn at 100 ms

TEST(ComputerHandMoves, AStepIsTheSpeedOverTheTurnsInASecond)
{
	EXPECT_EQ(StepLength(150.0f), 15.0f);
	EXPECT_EQ(StepLength(90.0f), 9.0f);
}

TEST(ComputerHandMoves, OverTheGroundAcrossTheLandHeldAboveIt)
{
	const FlatGround ground;
	auto hand = HandGoing({10.0f, 50.0f, 20.0f}, {100.0f, 7.0f, 60.0f}, 150.0f);
	EXPECT_EQ(MoveOverGround(hand, ground), StageResult::Going);
	EXPECT_EQ(hand.point.x, Bits(0x41BDA84Au));
	EXPECT_EQ(hand.point.y, 15.0f);
	EXPECT_EQ(hand.point.z, Bits(0x41D0BC93u));
}

TEST(ComputerHandMoves, OverTheGroundArrivesWithinAStep)
{
	const FlatGround ground;
	auto hand = HandGoing({10.0f, 50.0f, 20.0f}, {12.0f, 7.0f, 21.0f}, 150.0f);
	EXPECT_EQ(MoveOverGround(hand, ground), StageResult::Done);
	EXPECT_EQ(hand.point, glm::vec3(12.0f, 15.0f, 21.0f));
}

TEST(ComputerHandMoves, OverTheGroundRightAboveTheTargetArrives)
{
	const FlatGround ground;
	auto hand = HandGoing({10.0f, 50.0f, 20.0f}, {10.0f, 7.0f, 20.0f}, 150.0f);
	EXPECT_EQ(MoveOverGround(hand, ground), StageResult::Done);
	EXPECT_EQ(hand.point, glm::vec3(10.0f, 15.0f, 20.0f));
}

TEST(ComputerHandMoves, OverTheGroundAStepAwayStillSteps)
{
	// The game's distance across the land comes out a little over 15 here, so the hand steps onto the target and
	// arrives only on the next turn
	const FlatGround ground;
	auto hand = HandGoing({0.0f, 5.0f, 0.0f}, {15.0f, 15.0f, 0.0f}, 150.0f);
	EXPECT_EQ(MoveOverGround(hand, ground), StageResult::Going);
	EXPECT_EQ(hand.point, glm::vec3(15.0f, 15.0f, 0.0f));
}

TEST(ComputerHandMoves, StraightGoesUpAndDownToo)
{
	auto hand = HandGoing({10.0f, 50.0f, 20.0f}, {100.0f, 7.0f, 60.0f}, 150.0f);
	EXPECT_EQ(MoveStraight(hand), StageResult::Going);
	EXPECT_EQ(hand.point.x, Bits(0x41B47F24u));
	EXPECT_EQ(hand.point.y, Bits(0x422FFE12u));
	EXPECT_EQ(hand.point.z, Bits(0x41CCAA49u));
}

TEST(ComputerHandMoves, StraightArrivesAtThePointsOwnHeight)
{
	auto hand = HandGoing({10.0f, 50.0f, 20.0f}, {12.0f, 7.0f, 21.0f}, 150.0f);
	EXPECT_EQ(MoveStraight(hand), StageResult::Done);
	EXPECT_EQ(hand.point, glm::vec3(12.0f, 7.0f, 21.0f));
}

TEST(ComputerHandMoves, ToAThingFarAwayInLongStrides)
{
	const FlatGround ground;
	auto hand = HandGoing({0.0f, 0.0f, 0.0f}, {400.0f, 7.0f, 30.0f}, 150.0f);
	EXPECT_EQ(MoveToThing(hand, ground), StageResult::Going);
	EXPECT_EQ(hand.point.x, Bits(0x42C7709Bu));
	EXPECT_EQ(hand.point.y, 15.0f);
	EXPECT_EQ(hand.point.z, Bits(0x40EF53EDu));
}

TEST(ComputerHandMoves, ToAThingNearerInShortStrides)
{
	const FlatGround ground;
	auto hand = HandGoing({0.0f, 0.0f, 0.0f}, {200.0f, 7.0f, 30.0f}, 150.0f);
	EXPECT_EQ(MoveToThing(hand, ground), StageResult::Going);
	EXPECT_EQ(hand.point.x, Bits(0x411E3AD5u));
	EXPECT_EQ(hand.point.z, Bits(0x3FBDE033u));
}

TEST(ComputerHandMoves, ToAThingCloseAtTheHandsSpeed)
{
	const FlatGround ground;
	auto hand = HandGoing({0.0f, 0.0f, 0.0f}, {50.0f, 7.0f, 30.0f}, 150.0f);
	EXPECT_EQ(MoveToThing(hand, ground), StageResult::Going);
	EXPECT_EQ(hand.point.x, Bits(0x414DCC5Eu));
	EXPECT_EQ(hand.point.z, Bits(0x40F6F53Cu));
}

TEST(ComputerHandMoves, ToAThingWithinAStepArrives)
{
	const FlatGround ground;
	auto hand = HandGoing({0.0f, 0.0f, 0.0f}, {10.0f, 7.0f, 3.0f}, 150.0f);
	EXPECT_EQ(MoveToThing(hand, ground), StageResult::Done);
	EXPECT_EQ(hand.point, glm::vec3(10.0f, 15.0f, 3.0f));
	auto touching = HandGoing({0.0f, 0.0f, 0.0f}, {0.5f, 7.0f, 0.3f}, 150.0f);
	EXPECT_EQ(MoveToThing(touching, ground), StageResult::Done);
	EXPECT_EQ(touching.point, glm::vec3(0.5f, 15.0f, 0.3f));
}

TEST(ComputerHandMoves, PutAtHoversOrKeepsThePointsHeight)
{
	const FlatGround ground;
	ComputerHand hand;
	PutAt(hand, {1.0f, 40.0f, 2.0f}, &ground);
	EXPECT_EQ(hand.point, glm::vec3(1.0f, 15.0f, 2.0f));
	PutAt(hand, {1.0f, 40.0f, 2.0f}, nullptr);
	EXPECT_EQ(hand.point, glm::vec3(1.0f, 40.0f, 2.0f));
}

TEST(ComputerHandMoves, APointFromTheMapGrid)
{
	const FlatGround ground;
	EXPECT_EQ(PointFromMap(65536, -65536, 2.0f, ground), glm::vec3(10.0f, 7.0f, -10.0f));
}

TEST(ComputerHandScript, TakingTheHandLeavesOnlyTheScriptsStep)
{
	ComputerHand hand;
	FakeRunner runner;
	Append(hand, {.type = HandSubAction::MoveHandToObject, .arguments = {ThingOf(static_cast<entt::entity>(4))}});
	hand.stage = 1;
	TakeForScript(hand, StepToPoint(HandSubAction::MoveHandToPos, {1.0f, 2.0f, 3.0f}), 200.0f, runner);
	ASSERT_EQ(hand.steps.size(), 1u);
	EXPECT_EQ(hand.steps.front().type, HandSubAction::MoveHandToPos);
	EXPECT_EQ(hand.steps.front().arguments.front().kind, HandArgument::Kind::Point);
	EXPECT_EQ(hand.steps.front().arguments.front().point, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(hand.stage, 0);
	EXPECT_EQ(hand.speed, 200.0f);
	EXPECT_EQ(hand.playerSpeed, ComputerHand::k_StartingSpeed);
	EXPECT_TRUE(hand.held);
	EXPECT_TRUE(hand.busy);
	// The hand lets go as it gives up its steps, and again once it has the speed
	EXPECT_EQ(runner.letGo, 2);
	EXPECT_TRUE(MovedByScript(hand));
	EXPECT_TRUE(TakesItsTurn(hand));
}

TEST(ComputerHandScript, APauseHoldsTheHandAndGivesThePlayersSpeed)
{
	ComputerHand hand;
	SetSpeed(hand, 150.0f);
	hand.speed = 40.0f;
	Pause(hand, true);
	EXPECT_TRUE(hand.held);
	EXPECT_EQ(hand.speed, 150.0f);
	EXPECT_TRUE(TakesItsTurn(hand));
	Pause(hand, false);
	EXPECT_FALSE(hand.held);
	EXPECT_FALSE(TakesItsTurn(hand));
}

TEST(ComputerHandScript, TheSpeedIsThePlayersAndTheSteps)
{
	ComputerHand hand;
	SetSpeed(hand, 350.0f);
	EXPECT_EQ(hand.playerSpeed, 350.0f);
	EXPECT_EQ(hand.speed, 350.0f);
}

TEST(ComputerHandScript, AFreshHandIsAboveTheMiddleOfTheMap)
{
	const FlatGround ground;
	const ComputerHand hand;
	EXPECT_EQ(OnGround(hand, ground), glm::vec3(1285.0f, 5.0f, 1285.0f));
	EXPECT_FALSE(MovedByScript(hand));
	EXPECT_FALSE(TakesItsTurn(hand));
}

TEST(ComputerHandScript, TheLandUnderTheHandAsOfItsLastTurn)
{
	const FlatGround ground;
	ComputerHand hand;
	FakeRunner runner;
	hand.point = {100.0f, 60.0f, 200.0f};
	hand.held = true;
	Step(hand, runner);
	hand.point = {300.0f, 60.0f, 400.0f};
	EXPECT_EQ(OnGround(hand, ground), glm::vec3(100.0f, 5.0f, 200.0f));
}

TEST(ComputerHandHolding, APickUpTakesTheThingIntoTheHand)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto villager = static_cast<entt::entity>(9);
	EXPECT_EQ(PickUpTurn(hand, villager, runner), StageResult::Done);
	EXPECT_EQ(hand.heldObject, villager);
	ASSERT_EQ(runner.taken.size(), 1u);
	EXPECT_EQ(runner.taken.front(), villager);
}

TEST(ComputerHandHolding, AFullHandLetsGoFirstAndTriesAgain)
{
	ComputerHand hand;
	FakeRunner runner;
	hand.heldObject = static_cast<entt::entity>(2);
	const auto scaffold = static_cast<entt::entity>(5);
	EXPECT_EQ(PickUpTurn(hand, scaffold, runner), StageResult::Going);
	EXPECT_EQ(runner.letGo, 1);
	EXPECT_TRUE(runner.taken.empty());
	EXPECT_EQ(PickUpTurn(hand, scaffold, runner), StageResult::Done);
	EXPECT_EQ(hand.heldObject, scaffold);
}

TEST(ComputerHandHolding, AThingThatMayNotBePickedUpFailsTheStep)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto rock = static_cast<entt::entity>(6);
	runner.refused.insert(rock);
	EXPECT_EQ(PickUpTurn(hand, rock, runner), StageResult::Failed);
	EXPECT_EQ(hand.heldObject, entt::entity {entt::null});
}

TEST(ComputerHandHolding, AThingThatDoesNotGoInEndsTheStepEmptyHanded)
{
	ComputerHand hand;
	FakeRunner runner;
	runner.goesIn = false;
	EXPECT_EQ(PickUpTurn(hand, static_cast<entt::entity>(8), runner), StageResult::Done);
	EXPECT_EQ(hand.heldObject, entt::entity {entt::null});
}

TEST(ComputerHandHolding, AThingInAHandMayNotBePickedUp)
{
	const auto villager = static_cast<entt::entity>(9);
	const auto scaffold = static_cast<entt::entity>(5);
	const std::vector<entt::entity> computerHands {scaffold};
	EXPECT_TRUE(MayBePickedUp(villager, std::nullopt, computerHands));
	EXPECT_FALSE(MayBePickedUp(villager, villager, computerHands));
	EXPECT_FALSE(MayBePickedUp(scaffold, std::nullopt, computerHands));
	EXPECT_FALSE(MayBePickedUp(entt::null, std::nullopt, {}));
}

TEST(ComputerHandHolding, WhatTheHandLetsGoOfIsItsPlayersThrow)
{
	// the hand lets go through its own player's interface, a computer player's or any other's
	EXPECT_EQ(LetGoCreditedTo(PlayerNames::PLAYER_TWO), std::optional(PlayerNames::PLAYER_TWO));
	EXPECT_EQ(LetGoCreditedTo(PlayerNames::PLAYER_ONE), std::optional(PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(LetGoCreditedTo(PlayerNames::NEUTRAL).has_value());
}

TEST(ComputerHandHolding, TheDropLetsGo)
{
	ComputerHand hand;
	FakeRunner runner;
	hand.heldObject = static_cast<entt::entity>(2);
	EXPECT_EQ(DropTurn(hand, runner), StageResult::Done);
	EXPECT_EQ(runner.letGo, 1);
	EXPECT_EQ(hand.heldObject, entt::entity {entt::null});
}

TEST(ComputerHandHolding, WhatALetGoLeavesOverGoesBackIntoTheHand)
{
	// the rest of a scaffold put down gently goes into the hand that let go, as a pick-up's thing does
	ComputerHand hand;
	FakeRunner runner;
	const auto leftOver = static_cast<entt::entity>(6);
	EXPECT_TRUE(TakeLeftOver(hand, leftOver, runner));
	EXPECT_EQ(runner.taken, std::vector<entt::entity> {leftOver});
	EXPECT_EQ(hand.heldObject, leftOver);
	// nothing left over: nothing is taken
	ComputerHand empty;
	EXPECT_FALSE(TakeLeftOver(empty, entt::null, runner));
	EXPECT_EQ(runner.taken.size(), 1u);
	EXPECT_EQ(empty.heldObject, entt::entity {entt::null});
	// a thing that does not go in leaves the hand empty
	runner.goesIn = false;
	EXPECT_FALSE(TakeLeftOver(empty, leftOver, runner));
	EXPECT_EQ(empty.heldObject, entt::entity {entt::null});
}

TEST(ComputerHandHeldDraw, ATurnOfStepsKeepsWhereItBegan)
{
	ComputerHand hand;
	FakeRunner runner;
	hand.point = {1.0f, 2.0f, 3.0f};
	hand.target = {9.0f, 2.0f, 3.0f};
	Append(hand, StepToPoint(HandSubAction::MoveHandToPos, {9.0f, 2.0f, 3.0f}));
	Step(hand, runner);
	EXPECT_EQ(hand.turnStartPoint, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(hand.stepTurns, 1u);
}

TEST(ComputerHandHeldDraw, WithoutTimeTheThingIsAtTheHand)
{
	EXPECT_EQ(SlideOfHeld({0.0f, 0.0f, 0.0f}, {10.0f, 20.0f, 30.0f}, 50, 0), glm::vec3(10.0f, 20.0f, 30.0f));
}

TEST(ComputerHandHeldDraw, ItSlidesOverTheTimeAndStopsAtTheHand)
{
	const glm::vec3 start {0.0f, 0.0f, 0.0f};
	const glm::vec3 hand {10.0f, 20.0f, 30.0f};
	EXPECT_EQ(SlideOfHeld(start, hand, 25, 100), glm::vec3(2.5f, 5.0f, 7.5f));
	EXPECT_EQ(SlideOfHeld(start, hand, 150, 100), hand);
}

TEST(ComputerHandHeldDraw, ATurnStartsTheSlideAtTheTurnsPoint)
{
	HeldDraw draw;
	EXPECT_EQ(HeldDrawPoint(draw, 30), glm::vec3(0.0f));
	EXPECT_EQ(HeldDrawPoint(draw, 40), glm::vec3(0.0f));
	NewTurnsOfTheHand(draw, {5.0f, 6.0f, 7.0f}, 1);
	// The time drawn in the turn before is the slide's, and the slide goes from the point to the same point
	EXPECT_EQ(draw.durationMs, 70);
	EXPECT_EQ(draw.elapsedMs, 0);
	EXPECT_EQ(draw.start, draw.hand);
	EXPECT_EQ(HeldDrawPoint(draw, 20), glm::vec3(5.0f, 6.0f, 7.0f));
	// Two turns with no frame between keep no time
	NewTurnsOfTheHand(draw, {8.0f, 6.0f, 7.0f}, 2);
	EXPECT_EQ(draw.durationMs, 0);
	EXPECT_EQ(HeldDrawPoint(draw, 20), glm::vec3(8.0f, 6.0f, 7.0f));
	// No turn: nothing changes
	NewTurnsOfTheHand(draw, {1.0f, 1.0f, 1.0f}, 0);
	EXPECT_EQ(draw.hand, glm::vec3(8.0f, 6.0f, 7.0f));
}

TEST(ComputerHandHeldDraw, ThroughTheMapGrid)
{
	const FlatGround ground;
	EXPECT_EQ(OnTheMapGrid({10.0f, 15.0f, -20.0f}, ground), glm::vec3(10.0f, 15.0f, -20.0f));
	EXPECT_EQ(OnTheMapGrid({10.00001f, 15.0f, 0.0f}, ground).x, 10.0f);
}

TEST(ComputerHandActions, NamesAreFoundAsTheGameFindsThem)
{
	EXPECT_EQ(FindMindAction("FindPlaceToUseScaffoldForBuilding"), MindAction::FindPlaceToUseScaffoldForBuilding);
	EXPECT_EQ(FindMindAction("DIRECTPickupAndDropObject"), MindAction::DirectPickupAndDropObject);
	// A name only has to begin with the node's name
	EXPECT_EQ(FindMindAction("DIRECTPickupAndDropObject: 182 "), MindAction::DirectPickupAndDropObject);
	EXPECT_EQ(FindMindAction("ShallCastWoodSpell"), MindAction::NotPorted);
	EXPECT_EQ(FindMindAction("DestroyTown"), MindAction::NotPorted);
	EXPECT_EQ(FindMindAction("NoSuchAction"), std::nullopt);
	// The node's whole name must be there
	EXPECT_EQ(FindMindAction("DIRECTPickupAndDrop"), std::nullopt);
}

TEST(ComputerHandActions, DirectInAnyCase)
{
	EXPECT_TRUE(IsDirectAction("DIRECTPickupAndDropObject"));
	EXPECT_TRUE(IsDirectAction("directSomething"));
	EXPECT_FALSE(IsDirectAction("DIREC"));
	EXPECT_FALSE(IsDirectAction("FindPlaceToUseScaffoldForBuilding"));
}

TEST(ComputerHandActions, TheDirectPickUpAndDropGivesUpTheStepsAndGivesTwo)
{
	ComputerHand hand;
	FakeRunner runner;
	auto mind = computer_player::MakeMind();
	Append(hand, StepToPoint(HandSubAction::MoveHandToPos, {1.0f, 2.0f, 3.0f}));
	const auto villager = static_cast<entt::entity>(9);
	const auto marker = static_cast<entt::entity>(10);
	FakeFacts facts;
	facts.objects.insert(villager);
	facts.markers[marker] = glm::vec3(40.0f, 0.0f, 50.0f);
	EXPECT_TRUE(ForceAction(hand, mind, "DIRECTPickupAndDropObject", villager, marker, 0, runner, facts));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(0).type, HandSubAction::PickUpObject);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, villager);
	EXPECT_EQ(hand.steps.at(1).type, HandSubAction::DropObjectInHandAtPos);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).point, glm::vec3(40.0f, 0.0f, 50.0f));
	EXPECT_TRUE(hand.busy);
	EXPECT_FALSE(hand.held);
}

TEST(ComputerHandActions, TheDirectActionNeedsAnObjectAndAMarker)
{
	ComputerHand hand;
	FakeRunner runner;
	auto mind = computer_player::MakeMind();
	Append(hand, StepToPoint(HandSubAction::MoveHandToPos, {1.0f, 2.0f, 3.0f}));
	FakeFacts facts;
	facts.objects.insert(static_cast<entt::entity>(9));
	EXPECT_FALSE(
	    ForceAction(hand, mind, "DIRECTPickupAndDropObject", static_cast<entt::entity>(9), entt::null, 0, runner, facts));
	// It gave up the steps before it looked
	EXPECT_TRUE(IsReady(hand));
	EXPECT_FALSE(hand.busy);
}

TEST(ComputerHandActions, TheScaffoldGoesToTheMarker)
{
	ComputerHand hand;
	FakeRunner runner;
	auto mind = computer_player::MakeMind();
	const auto scaffold = static_cast<entt::entity>(5);
	const auto marker = static_cast<entt::entity>(11);
	FakeFacts facts;
	facts.markers[marker] = glm::vec3(7.0f);
	EXPECT_TRUE(ForceAction(hand, mind, "FindPlaceToUseScaffoldForBuilding", scaffold, marker, 0, runner, facts));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, scaffold);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).point, glm::vec3(7.0f));
	// Without a marker the place is weighed, which needs a way of scoring openblack lacks: nothing
	ComputerHand other;
	auto fresh = computer_player::MakeMind();
	EXPECT_FALSE(ForceAction(other, fresh, "FindPlaceToUseScaffoldForBuilding", scaffold, entt::null, 0, runner, facts));
	// a thing that may not be picked up gives nothing either
	runner.refused.insert(scaffold);
	EXPECT_FALSE(ForceAction(other, fresh, "FindPlaceToUseScaffoldForBuilding", scaffold, marker, 0, runner, facts));
	EXPECT_TRUE(IsReady(other));
	EXPECT_FALSE(other.busy);
}

TEST(ComputerHandActions, TheScaffoldGivenAThingThatIsNoMarkerGoesToTheChildsPointZeroUntilSet)
{
	// the child takes the second thing and is chosen, but no point: the nodes start at 0, so the drop is at 0
	ComputerHand hand;
	FakeRunner runner;
	auto mind = computer_player::MakeMind();
	const FakeFacts facts;
	const auto scaffold = static_cast<entt::entity>(5);
	EXPECT_TRUE(ForceAction(hand, mind, "FindPlaceToUseScaffoldForBuilding", scaffold, static_cast<entt::entity>(12), 0, runner,
	                        facts));
	ASSERT_EQ(hand.steps.size(), 2u);
	EXPECT_EQ(hand.steps.at(0).arguments.at(0).thing, scaffold);
	EXPECT_EQ(hand.steps.at(1).arguments.at(0).point, glm::vec3(0.0f));
	// a point a marker gave the child before stays for the next such force
	ComputerHand again;
	FakeFacts withMarker;
	withMarker.markers[static_cast<entt::entity>(11)] = glm::vec3(7.0f);
	EXPECT_TRUE(ForceAction(again, mind, "FindPlaceToUseScaffoldForBuilding", scaffold, static_cast<entt::entity>(11), 0,
	                        runner, withMarker));
	ComputerHand later;
	EXPECT_TRUE(ForceAction(later, mind, "FindPlaceToUseScaffoldForBuilding", scaffold, static_cast<entt::entity>(12), 0,
	                        runner, facts));
	EXPECT_EQ(later.steps.at(1).arguments.at(0).point, glm::vec3(7.0f));
}

TEST(ComputerHandActions, ADirectNodeKeepsOnlyASecondThingWithAPlace)
{
	ComputerHand hand;
	FakeRunner runner;
	auto mind = computer_player::MakeMind();
	FakeFacts facts;
	const auto villager = static_cast<entt::entity>(9);
	const auto town = static_cast<entt::entity>(13);
	facts.objects.insert(villager);
	facts.placeless.insert(town);
	EXPECT_FALSE(ForceAction(hand, mind, "DIRECTPickupAndDropObject", villager, town, 0, runner, facts));
	EXPECT_EQ(mind.nodes.at(6).secondThing, entt::entity {entt::null});
}

TEST(ComputerHandActions, ANodeNotPortedOrAMissingNameDoesNothing)
{
	ComputerHand hand;
	FakeRunner runner;
	auto mind = computer_player::MakeMind();
	const FakeFacts facts;
	Append(hand, StepToPoint(HandSubAction::MoveHandToPos, {1.0f, 2.0f, 3.0f}));
	EXPECT_FALSE(ForceAction(hand, mind, "ShallCastWoodSpell", static_cast<entt::entity>(20), static_cast<entt::entity>(21), 0,
	                         runner, facts));
	EXPECT_FALSE(ForceAction(hand, mind, "NoSuchAction", entt::null, entt::null, 0, runner, facts));
	EXPECT_EQ(hand.steps.size(), 1u);
	// even once a weighing chose it, an action openblack lacks is not run, and its node is not held back
	mind.nodes.at(65).chosen = 65;
	EXPECT_FALSE(ForceAction(hand, mind, "ShallCastWoodSpell", static_cast<entt::entity>(20), static_cast<entt::entity>(21), 0,
	                         runner, facts));
	EXPECT_EQ(hand.steps.size(), 1u);
	EXPECT_EQ(mind.nodes.at(65).suppression, 1.0f);
}

TEST(ComputerHandActions, TheQueueKeepsItsOrderAndDropsGoneThings)
{
	ComputerHand hand;
	FakeRunner runner;
	const auto town = static_cast<entt::entity>(20);
	const auto icon = static_cast<entt::entity>(21);
	QueueAction(hand, "ShallCastWoodSpell", town, icon);
	QueueAction(hand, "DIRECTPickupAndDropObject", static_cast<entt::entity>(22), entt::null);
	QueueAction(hand, "FindPlaceToUseScaffoldForBuilding", entt::null, entt::null);
	runner.gone.insert(icon);
	SweepQueue(hand, runner);
	ASSERT_EQ(hand.queue.size(), 2u);
	const auto next = TakeNext(hand);
	ASSERT_TRUE(next.has_value());
	EXPECT_EQ(next->name, "DIRECTPickupAndDropObject");
	EXPECT_EQ(hand.queue.size(), 1u);
	EXPECT_TRUE(TakeNext(hand).has_value());
	EXPECT_FALSE(TakeNext(hand).has_value());
}
