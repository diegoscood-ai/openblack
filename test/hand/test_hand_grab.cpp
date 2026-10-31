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

#include <bit>
#include <limits>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Enums.h"
#include "Hand/HandGrabRules.h"

using namespace openblack;
using namespace openblack::hand_grab;

// Placing in the hand is an allowlist: a thing of no named class (a building, a totem, a feature) never goes in
TEST(HandGrab, PlaceInHandIsAnAllowlist)
{
	EXPECT_EQ(PlaceInHandRuleOf({}), PlaceInHandRule::Never);
	EXPECT_EQ(PlaceInHandRuleOf({.creature = true, .mobile = true}), PlaceInHandRule::Never);
	EXPECT_EQ(PlaceInHandRuleOf({.mobile = true}), PlaceInHandRule::Always);
	EXPECT_EQ(PlaceInHandRuleOf({.tree = true}), PlaceInHandRule::Always);
	EXPECT_EQ(PlaceInHandRuleOf({.bigForest = true}), PlaceInHandRule::Always);
	EXPECT_EQ(PlaceInHandRuleOf({.oneOffSpellSeed = true}), PlaceInHandRule::Always);
	EXPECT_EQ(PlaceInHandRuleOf({.deadTree = true, .mobile = true}), PlaceInHandRule::Always);
	EXPECT_EQ(PlaceInHandRuleOf({.mobileStatic = true, .mobile = true}), PlaceInHandRule::Always);
	EXPECT_EQ(PlaceInHandRuleOf({.mobileStatic = true, .rock = true, .mobile = true}), PlaceInHandRule::Rock);
	EXPECT_EQ(PlaceInHandRuleOf({.pot = true}), PlaceInHandRule::Always);
	EXPECT_EQ(PlaceInHandRuleOf({.pot = true, .pile = true}), PlaceInHandRule::Pile);
	// the most particular class decides over the mobile it also is
	EXPECT_EQ(PlaceInHandRuleOf({.villager = true, .mobile = true}), PlaceInHandRule::Villager);
	EXPECT_EQ(PlaceInHandRuleOf({.animal = true, .mobile = true}), PlaceInHandRule::Animal);
	EXPECT_EQ(PlaceInHandRuleOf({.scaffold = true, .mobile = true}), PlaceInHandRule::Scaffold);
	EXPECT_EQ(PlaceInHandRuleOf({.seedOrTeleport = true}), PlaceInHandRule::Seed);
	EXPECT_EQ(PlaceInHandRuleOf({.fireBall = true}), PlaceInHandRule::FireBall);
	EXPECT_EQ(PlaceInHandRuleOf({.field = true}), PlaceInHandRule::Field);
	EXPECT_EQ(PlaceInHandRuleOf({.fishFarm = true}), PlaceInHandRule::FishFarm);
}

// The bonfire and a piece knocked off a building are never placed in the hand, whatever else they are; a dead tree is
TEST(HandGrab, TheBonfireAndAFragmentNeverGoInTheHand)
{
	EXPECT_EQ(PlaceInHandRuleOf({.mobileStatic = true, .bonfire = true, .mobile = true}), PlaceInHandRule::Never);
	EXPECT_EQ(PlaceInHandRuleOf({.mobileStatic = true, .rock = true, .bonfire = true, .mobile = true}), PlaceInHandRule::Never);
	EXPECT_EQ(PlaceInHandRuleOf({.fragment = true}), PlaceInHandRule::Never);
	EXPECT_EQ(PlaceInHandRuleOf({.rock = true, .fragment = true, .mobile = true}), PlaceInHandRule::Never);
	EXPECT_EQ(PlaceInHandRuleOf({.deadTree = true, .mobile = true}), PlaceInHandRule::Always);
}

TEST(HandGrab, FieldsNeedGrowthAndFood)
{
	EXPECT_TRUE(FieldValidForPlaceInHand(1.0f, 0.5f));
	EXPECT_FALSE(FieldValidForPlaceInHand(0.0f, 100.0f));
	EXPECT_FALSE(FieldValidForPlaceInHand(10.0f, 0.0f));
}

// Only a pile of the hand's own row with something in it; a stored or magic pile never
// The scoop's lock: a food or wood pile of any row but the hand's own, empty or not; a pot (potType Pot) never
TEST(HandGrab, TheScoopLocksOntoPilesButNotTheHandsOwnOrPots)
{
	EXPECT_TRUE(hand_grab::PileValidForLockedSelect(PotType::PileFood, PotInfo::FoodPile));
	EXPECT_TRUE(hand_grab::PileValidForLockedSelect(PotType::PileFood, PotInfo::StoragePitFoodPile));
	EXPECT_TRUE(hand_grab::PileValidForLockedSelect(PotType::PileWood, PotInfo::WoodPile_3));
	EXPECT_TRUE(hand_grab::PileValidForLockedSelect(PotType::PileFood, PotInfo::MagicFood));
	EXPECT_FALSE(hand_grab::PileValidForLockedSelect(PotType::PileFood, PotInfo::HandFood));
	EXPECT_FALSE(hand_grab::PileValidForLockedSelect(PotType::PileWood, PotInfo::HandWood));
	EXPECT_FALSE(hand_grab::PileValidForLockedSelect(PotType::Pot, PotInfo::FoodPot));
	EXPECT_FALSE(hand_grab::PileValidForLockedSelect(PotType::Pot, PotInfo::TreeLogsInHand));
}

TEST(HandGrab, OnlyTheHandsOwnPilesWithSomethingIn)
{
	EXPECT_TRUE(PileValidForPlaceInHand(PotType::PileFood, PotInfo::HandFood, 1));
	EXPECT_TRUE(PileValidForPlaceInHand(PotType::PileWood, PotInfo::HandWood, 30));
	EXPECT_FALSE(PileValidForPlaceInHand(PotType::PileFood, PotInfo::HandFood, 0));
	EXPECT_FALSE(PileValidForPlaceInHand(PotType::PileWood, PotInfo::HandFood, 5));
	EXPECT_FALSE(PileValidForPlaceInHand(PotType::PileFood, PotInfo::StoragePitFoodPile, 100));
	EXPECT_FALSE(PileValidForPlaceInHand(PotType::PileFood, PotInfo::MagicFood, 100));
	EXPECT_FALSE(PileValidForPlaceInHand(PotType::PileWood, PotInfo::WoodPile_1, 100));
}

TEST(HandGrab, APressIsTimedByTheClockButNoLongerThanTheTurnsAllow)
{
	// By the clock 500 ms, but the game has only moved on from turn 10 to turn 11
	EXPECT_EQ(ElapsedMs(1500, 1000, 11, 10), 200u);
	EXPECT_EQ(ElapsedMs(1100, 1000, 20, 10), 100u);
}

// A standing tree let go while the hand pulls at it is tapped up to the grab's wait and no later; once pulled free it is
// held, and letting go of it is no tap
TEST(HandGrab, ATreeLetGoWithinTheGrabsWaitIsTapped)
{
	EXPECT_EQ(TreeRelease(0, false), TreeLetGo::Tap);
	EXPECT_EQ(TreeRelease(225, false), TreeLetGo::Tap);
	EXPECT_EQ(TreeRelease(226, false), TreeLetGo::Nothing);
	EXPECT_EQ(TreeRelease(100, true), TreeLetGo::Nothing);
}

// The grab takes a resting thing the frame after its tug hold ends, as the hold stood the frame before; a thing in flight
// once the grab's wait is over; a standing tree never, its pull takes it
TEST(HandGrab, TheGrabTakesAThingTheFrameAfterItsHoldEnds)
{
	EXPECT_FALSE(GrabPicksUp(false, 150, false, false));
	EXPECT_TRUE(GrabPicksUp(false, 150, true, false));
	EXPECT_FALSE(GrabPicksUp(true, 224, true, false));
	EXPECT_TRUE(GrabPicksUp(true, 225, false, false));
	EXPECT_FALSE(GrabPicksUp(false, 1000, true, true));
	EXPECT_FALSE(GrabPicksUp(true, 1000, true, true));
}

// A thing that can't be pulled (a big forest, a fireball, a field, a pile) waits the grab's time as a thing in flight
// does, and the drawn hand does not take hold of it at the press; anything else is held with a short pull, a miracle's
// seed with none
TEST(HandGrab, AThingThatCantBePulledWaitsTheGrabsTime)
{
	const auto pile = GrabStartOf(false, false, false);
	EXPECT_TRUE(pile.timer);
	EXPECT_FALSE(pile.tugHold);
	EXPECT_FALSE(pile.drawnHold);
	const auto thrownPile = GrabStartOf(true, false, false);
	EXPECT_TRUE(thrownPile.timer);
	EXPECT_FALSE(thrownPile.drawnHold);
	const auto rock = GrabStartOf(false, true, false);
	EXPECT_FALSE(rock.timer);
	EXPECT_TRUE(rock.tugHold);
	EXPECT_TRUE(rock.drawnHold);
	const auto thrownRock = GrabStartOf(true, true, false);
	EXPECT_TRUE(thrownRock.timer);
	EXPECT_FALSE(thrownRock.tugHold);
	EXPECT_TRUE(thrownRock.drawnHold);
	const auto seed = GrabStartOf(false, true, true);
	EXPECT_FALSE(seed.timer);
	EXPECT_FALSE(seed.tugHold);
	EXPECT_TRUE(seed.drawnHold);
}

TEST(HandGrab, TheSpringStepsAtLeastOnceAFrame)
{
	HandSpring spring;
	spring.Start({0.0f, 0.0f, 0.0f});
	spring.Count(0);
	spring.Step({1.0f, 0.0f, 0.0f});
	// One step of 10 ms: v = 260 * 1 * 0.01, then x = v * 0.01
	EXPECT_EQ(spring.StepsTaken(), 1u);
	EXPECT_FLOAT_EQ(spring.Velocity().x, 2.6f);
	EXPECT_FLOAT_EQ(spring.Position().x, 0.026f);
}

TEST(HandGrab, TheSpringTakesAsManyStepsAsTheGameTimeAllows)
{
	const auto frame = [](HandSpring& spring, uint32_t ms) {
		spring.Count(ms);
		spring.Step({1.0f, 0.0f, 0.0f});
	};
	HandSpring once;
	once.Start({0.0f, 0.0f, 0.0f});
	frame(once, 30);
	HandSpring stepped;
	stepped.Start({0.0f, 0.0f, 0.0f});
	frame(stepped, 0);
	frame(stepped, 0);
	frame(stepped, 0);
	EXPECT_FLOAT_EQ(once.Position().x, stepped.Position().x);
	// A frame of 15 ms after one of 0 ms takes one more step, not two
	HandSpring uneven;
	uneven.Start({0.0f, 0.0f, 0.0f});
	frame(uneven, 0);
	frame(uneven, 15);
	HandSpring two;
	two.Start({0.0f, 0.0f, 0.0f});
	frame(two, 20);
	EXPECT_FLOAT_EQ(uneven.Position().x, two.Position().x);
}

TEST(HandGrab, TheSpringCatchesUpWithTheTimeHeldBeforeItTookHold)
{
	// Half a second holding with the spring off, then the first frame with it on takes all of that time's steps
	HandSpring spring;
	for (int i = 0; i < 50; ++i)
	{
		spring.Count(10);
	}
	spring.Start({0.0f, 0.0f, 0.0f});
	spring.Count(10);
	spring.Step({1.0f, 0.0f, 0.0f});
	EXPECT_EQ(spring.StepsTaken(), 51u);
	// Taking hold again keeps the time counted
	spring.Start({0.0f, 0.0f, 0.0f});
	spring.Count(10);
	spring.Step({1.0f, 0.0f, 0.0f});
	EXPECT_EQ(spring.StepsTaken(), 1u);
}

TEST(HandGrab, TheSpringSettlesOnItsTargetAndIsCapped)
{
	HandSpring spring;
	spring.Start({0.0f, 0.0f, 0.0f});
	for (int i = 0; i < 200; ++i)
	{
		spring.Count(10);
		spring.Step({3.0f, 1.0f, -2.0f});
	}
	EXPECT_NEAR(spring.Position().x, 3.0f, 1e-3f);
	EXPECT_NEAR(spring.Position().z, -2.0f, 1e-3f);
	HandSpring far;
	far.Start({0.0f, 0.0f, 0.0f});
	far.Count(10);
	far.Step({10000.0f, 0.0f, 0.0f});
	EXPECT_FLOAT_EQ(glm::length(far.Velocity()), k_MaxThrowSpeed);
}

TEST(HandGrab, TheTwistIsCancelledWhenItsCountdownEndsOnNothing)
{
	int32_t ms = 180;
	EXPECT_EQ(CountDown(ms, 100), Countdown::Running);
	EXPECT_EQ(CountDown(ms, 100), Countdown::RunOut);
	// A countdown that lands on exactly nothing gives no twist
	ms = 180;
	EXPECT_EQ(CountDown(ms, 90), Countdown::Running);
	EXPECT_EQ(CountDown(ms, 90), Countdown::Cancelled);
	ms = 0;
	EXPECT_EQ(CountDown(ms, 10), Countdown::Cancelled);
}

TEST(HandGrab, OnlyATreeTheHandCanLiftIsPulledFree)
{
	Tug tug {.axes = glm::mat3(1.0f), .base = glm::vec3(0.0f)};
	TugFrame frame {
	    .hand = {0.0f, 2.0f, 0.0f}, .holdDistance = 1.0f, .weight = 50.0f, .height = 10.0f, .tree = true, .seconds = 0.01f};
	// A pull of 1000 a unit of stretch, a hair over one unit away (the grip a hair below 1, up the axis made unit length
	// with the game's inverse square root): more than the tree weighs. The original's pull, bit for bit
	auto result = PullAt(tug, frame);
	EXPECT_EQ(std::bit_cast<uint32_t>(result.force.y), 0x447A005Eu);
	EXPECT_TRUE(result.comesFree);
	// Pulled no harder than it weighs, it stays
	tug = {.axes = glm::mat3(1.0f), .base = glm::vec3(0.0f)};
	frame.weight = 2000.0f;
	EXPECT_FALSE(PullAt(tug, frame).comesFree);
	// Too heavy for the hand's greatest pull it never comes free, however far the hand pulls
	tug = {.axes = glm::mat3(1.0f), .base = glm::vec3(0.0f)};
	frame.weight = MaxForce(0.0f) / 9.81f + 1.0f;
	frame.hand = {0.0f, 100000.0f, 0.0f};
	result = PullAt(tug, frame);
	EXPECT_FLOAT_EQ(glm::length(result.force), MaxForce(0.0f));
	EXPECT_FALSE(result.comesFree);
	// Anything but a tree comes free at once
	tug = {.axes = glm::mat3(1.0f), .base = glm::vec3(0.0f)};
	frame.tree = false;
	EXPECT_TRUE(PullAt(tug, frame).comesFree);
}

TEST(HandGrab, APulledTreeLeansAndStretchesTowardsTheHand)
{
	Tug tug {.axes = glm::mat3(1.0f), .base = glm::vec3(0.0f)};
	const TugFrame frame {
	    .hand = {1.0f, 1.0f, 0.0f}, .holdDistance = 1.0f, .weight = 1.0e6f, .height = 10.0f, .tree = true, .seconds = 0.01f};
	TugResult result;
	for (int i = 0; i < 20; ++i)
	{
		result = PullAt(tug, frame);
	}
	// Its top leans towards the hand, which is off to its side
	EXPECT_GT(tug.axes[1].x, 0.0f);
	EXPECT_GT(glm::length(tug.pullVelocity), 0.0f);
	// It stretches by how far the hand is from its grip, no more than 1.3 times
	EXPECT_GT(result.stretchTarget, 1.0f);
	EXPECT_LE(result.stretchTarget, 1.3f);
	// Rock-like things neither lean nor stretch
	Tug rock {.axes = glm::mat3(1.0f), .base = glm::vec3(0.0f)};
	auto still = frame;
	still.leans = false;
	static_cast<void>(PullAt(rock, still));
	EXPECT_FLOAT_EQ(rock.axes[1].x, 0.0f);
	EXPECT_EQ(rock.pullVelocity, glm::vec3(0.0f));
}

TEST(HandGrab, AThrowIsFastAcrossTheGround)
{
	EXPECT_FALSE(IsThrow({2.0f, 50.0f, 0.0f}, false));
	EXPECT_TRUE(IsThrow({2.0f, 0.0f, 0.1f}, false));
	EXPECT_FALSE(IsThrow({1.0f, 0.0f, 0.0f}, true));
	EXPECT_TRUE(IsThrow({1.0f, 0.0f, 0.1f}, true));
	EXPECT_TRUE(PotPours({2.0f, 1.0f, 0.0f}));
	EXPECT_FALSE(PotPours({2.0f, 1.0f, 0.1f}));
}

TEST(HandGrab, APotOfAnyClassPutsItsResourceDownWhenLetGoGently)
{
	EXPECT_TRUE(PotDissolvesOnRelease(true, {2.0f, 1.0f, 0.0f}));
	EXPECT_FALSE(PotDissolvesOnRelease(true, {2.0f, 1.0f, 0.1f}));
	EXPECT_FALSE(PotDissolvesOnRelease(false, {0.0f, 0.0f, 0.0f}));
	EXPECT_TRUE(PotDissolvesOnRelease(true, {std::sqrt(5.0f), 0.0f, 0.0f}));
	EXPECT_FALSE(PotDissolvesOnRelease(true, {std::sqrt(5.0001f), 0.0f, 0.0f}));
	EXPECT_TRUE(PotDissolvesOnRelease(true, {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f}));
}

TEST(HandGrab, PeopleFencesAndUprightTreesLeaveThePhysicsOnLanding)
{
	EXPECT_EQ(OutcomeOfLanding({.living = true}), LandedOutcome::LeavesPhysics);
	EXPECT_EQ(OutcomeOfLanding({.fence = true}), LandedOutcome::LeavesPhysics);
	EXPECT_EQ(OutcomeOfLanding({.tree = true, .onLand = true}), LandedOutcome::LeavesPhysics);
	EXPECT_EQ(OutcomeOfLanding({.tree = true, .onLand = true, .tiltX = 0.21f}), LandedOutcome::Falls);
	EXPECT_EQ(OutcomeOfLanding({.tree = true, .onLand = true, .dontReplant = true}), LandedOutcome::Falls);
	// A creature's put-down tree isn't asked its tilt
	EXPECT_EQ(OutcomeOfLanding({.tree = true, .onLand = true, .tiltZ = 1.0f, .byCreature = true}),
	          LandedOutcome::LeavesPhysics);
	EXPECT_EQ(OutcomeOfLanding({.tree = true, .burning = true, .onLand = true}), LandedOutcome::Settles);
	EXPECT_EQ(OutcomeOfLanding({.tree = true}), LandedOutcome::Settles);
	EXPECT_EQ(OutcomeOfLanding({}), LandedOutcome::Settles);
}

TEST(HandGrabRules, GrippingTheLandThrowsUpDustAndGrippingTheWaterSplashes)
{
	// Over the land the grip's start sends the dust (the grip landscape spot visual) and the grab sound, neither while a
	// script has the widescreen on, whatever the hand holds; over the water, or off the map, the splash, which the
	// widescreen does not stop but a paused game does, and so does a held thing the hand draws
	EXPECT_EQ(GripStartEffectAt(true, false, false, false), GripStartEffect::Dust);
	EXPECT_EQ(GripStartEffectAt(true, true, false, false), GripStartEffect::None);
	EXPECT_EQ(GripStartEffectAt(true, false, true, false), GripStartEffect::Dust);
	EXPECT_EQ(GripStartEffectAt(true, false, false, true), GripStartEffect::Dust);
	EXPECT_EQ(GripStartEffectAt(true, true, false, true), GripStartEffect::None);
	EXPECT_EQ(GripStartEffectAt(false, false, false, false), GripStartEffect::Splash);
	EXPECT_EQ(GripStartEffectAt(false, true, false, false), GripStartEffect::Splash);
	EXPECT_EQ(GripStartEffectAt(false, false, true, false), GripStartEffect::None);
	EXPECT_EQ(GripStartEffectAt(false, false, false, true), GripStartEffect::None);
	EXPECT_EQ(static_cast<int>(SpotVisualType::GripLandscape), 2);
}
