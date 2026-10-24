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

#include <array>
#include <bit>
#include <limits>
#include <optional>

#include <glm/ext/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "Creature/CreatureRig.h"
#include "Creature/LeashRules.h"
#include "ECS/Components/CreatureLeash.h"

using namespace openblack;
using namespace openblack::creature_leash;
using creature_desires::Desire;

namespace
{
constexpr float k_Tolerance = 1e-4f;
}

TEST(LeashRules, HandLengthsGrowWithTheCreature)
{
	const auto one = InHand(1.0f);
	EXPECT_NEAR(one.slack, 32.5f, k_Tolerance);
	EXPECT_NEAR(one.max, 77.0f, k_Tolerance);
	const auto two = InHand(2.0f);
	EXPECT_NEAR(two.slack, 43.0f, k_Tolerance);
	EXPECT_NEAR(two.max, 122.0f, k_Tolerance);
}

TEST(LeashRules, TiedToATreeTheLeashIsSixHeightsAtMostForty)
{
	// The original's numbers, bit for bit in single precision
	// 6.6 high: 39.6
	const auto low = TiedToTree(std::bit_cast<float>(0x40D33333u));
	EXPECT_EQ(std::bit_cast<uint32_t>(low.max), 0x421E6666u);
	EXPECT_EQ(std::bit_cast<uint32_t>(low.slack), 0x419E6666u);
	// six times 6.6666665 rounds to 40 itself
	const auto edge = TiedToTree(std::bit_cast<float>(0x40D55555u));
	EXPECT_EQ(std::bit_cast<uint32_t>(edge.max), 0x42200000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(edge.slack), 0x41A00000u);
	const auto tall = TiedToTree(10.5f);
	EXPECT_EQ(tall.max, 40.0f);
	EXPECT_EQ(tall.slack, 20.0f);
	EXPECT_EQ(TiedToTree(4.5f).max, 27.0f);
	EXPECT_EQ(TiedToTree(4.5f).slack, 13.5f);
}

TEST(LeashRules, TiedToAnythingElseTheLeashGoesByTheDistance)
{
	// 201.7 away: 302.55
	const auto mid = TiedToObject(std::bit_cast<float>(0x4349B333u));
	EXPECT_EQ(std::bit_cast<uint32_t>(mid.max), 0x43974666u);
	EXPECT_EQ(std::bit_cast<uint32_t>(mid.slack), 0x43174666u);
	EXPECT_EQ(TiedToObject(133.33333f).max, 200.0f);
	EXPECT_EQ(TiedToObject(133.33333f).slack, 100.0f);
	// at least 180, at most 360
	EXPECT_EQ(TiedToObject(100.3f).max, 180.0f);
	EXPECT_EQ(TiedToObject(100.3f).slack, 90.0f);
	EXPECT_EQ(TiedToObject(0.0f).max, 180.0f);
	EXPECT_EQ(TiedToObject(260.1f).max, 360.0f);
	EXPECT_EQ(TiedToObject(260.1f).slack, 180.0f);
	// a distance that is not a number gives the shortest
	EXPECT_EQ(TiedToObject(std::numeric_limits<float>::quiet_NaN()).max, 180.0f);
}

TEST(LeashRules, EachLeashLooksItsOwn)
{
	EXPECT_FLOAT_EQ(LookFor(LeashType::Evil).v0, 0.375f);
	EXPECT_FLOAT_EQ(LookFor(LeashType::Evil).v1, 0.5f);
	EXPECT_FLOAT_EQ(LookFor(LeashType::Rope).v0, 0.125f);
	EXPECT_FLOAT_EQ(LookFor(LeashType::Good).v0, 0.25f);
	EXPECT_FLOAT_EQ(LookFor(LeashType::Good).halfWidth, 0.225f);
	EXPECT_FLOAT_EQ(LookFor(LeashType::Rope).halfWidth, 0.15f);
}

TEST(LeashRules, LeashesForceTheirFeelings)
{
	EXPECT_EQ(ForcedDesireFor(LeashType::Evil), Desire::Anger);
	EXPECT_EQ(ForcedDesireFor(LeashType::Good), Desire::Compassion);
	EXPECT_FALSE(ForcedDesireFor(LeashType::Rope).has_value());
	EXPECT_EQ(MiracleSightingWeight(true), 3u);
	EXPECT_EQ(MiracleSightingWeight(false), 1u);
}

TEST(LeashRules, Confinement)
{
	EXPECT_TRUE(FreeOfHome(139.0f, true));
	EXPECT_FALSE(FreeOfHome(141.0f, true));
	EXPECT_FALSE(FreeOfHome(10.0f, false));
	EXPECT_TRUE(IsConfined(k_HomeConfinement, false, true));
	EXPECT_FALSE(IsConfined(0.0f, false, true));
	// On a leash that doesn't work, it isn't kept anywhere
	EXPECT_FALSE(IsConfined(12.0f, true, false));
	EXPECT_TRUE(IsConfined(12.0f, true, true));
}

TEST(LeashRules, StrayedFartherThanTheRadiusItWalksBack)
{
	const WalkBackCheck strayed {.confined = true, .pointOnMap = true, .pointOnLand = true, .distance = 20.0f, .radius = 12.0f};
	EXPECT_TRUE(ShouldWalkBack(strayed));
	// at the radius or within it, it stays
	auto check = strayed;
	check.distance = 12.0f;
	EXPECT_FALSE(ShouldWalkBack(check));
	// not kept anywhere, or the point off the map or not on land it can stand on
	check = strayed;
	check.confined = false;
	EXPECT_FALSE(ShouldWalkBack(check));
	check = strayed;
	check.pointOnMap = false;
	EXPECT_FALSE(ShouldWalkBack(check));
	check = strayed;
	check.pointOnLand = false;
	EXPECT_FALSE(ShouldWalkBack(check));
	// busy with what keeps it where it is
	for (const auto busy : {&WalkBackCheck::clearingWay, &WalkBackCheck::sentByLeash, &WalkBackCheck::teleporting,
	                        &WalkBackCheck::sentToPoint, &WalkBackCheck::controlledByScript})
	{
		check = strayed;
		check.*busy = true;
		EXPECT_FALSE(ShouldWalkBack(check));
	}
}

TEST(LeashRules, WalkingBackItSetsOffAgainOnlyOnceThePointHasMoved)
{
	// half the radius, at least 5
	EXPECT_FLOAT_EQ(WalkBackRestartDistance(4.0f), 5.0f);
	EXPECT_FLOAT_EQ(WalkBackRestartDistance(10.0f), 5.0f);
	EXPECT_FLOAT_EQ(WalkBackRestartDistance(77.0f), 38.5f);
	WalkBackCheck check {.confined = true,
	                     .pointOnMap = true,
	                     .pointOnLand = true,
	                     .distance = 100.0f,
	                     .radius = 77.0f,
	                     .walkingBack = true,
	                     .walkingToFromPoint = 38.5f};
	EXPECT_FALSE(ShouldWalkBack(check));
	check.walkingToFromPoint = 38.6f;
	EXPECT_TRUE(ShouldWalkBack(check));
}

TEST(LeashRules, TheWalkBacksSpeedAndWhereItStops)
{
	// 0.8 of the top speed times how far it strayed over twice the radius, at most 0.8
	EXPECT_FLOAT_EQ(WalkBackHurry(15.0f, 10.0f), 0.6f);
	EXPECT_FLOAT_EQ(WalkBackHurry(20.0f, 10.0f), 0.8f);
	EXPECT_FLOAT_EQ(WalkBackHurry(500.0f, 10.0f), 0.8f);
	EXPECT_FLOAT_EQ(WalkBackHurry(-1.0f, 10.0f), 0.0f);
	EXPECT_EQ(WalkBackHurry(15.0f, 10.0f), (15.0f * 0.8f) / 20.0f);
	// its height or the radius, whichever is more
	EXPECT_FLOAT_EQ(WalkBackArrival(15.0f, 10.0f), 15.0f);
	EXPECT_FLOAT_EQ(WalkBackArrival(15.0f, 77.0f), 77.0f);
}

TEST(LeashRules, SentToAPointOfTheLandItWalksThereUnlessSomethingElseComesFirst)
{
	// on dry land, nothing in its hand, nothing at the point
	constexpr PointOrderCheck k_Plain {};
	EXPECT_EQ(DecidePointOrder(k_Plain), PointOrder::WalkThere);

	// refused: its mind off, a script controlling it, its leash not working, held locked by the hand
	for (const auto& check : {PointOrderCheck {.mindActive = false}, PointOrderCheck {.controlledByScript = true},
	                          PointOrderCheck {.leashWorks = false}, PointOrderCheck {.lockedSelect = true}})
	{
		EXPECT_EQ(DecidePointOrder(check), PointOrder::Refused);
	}
	// no life, or one that can't be compared, refuses; so does an exhaustion of 1 or more, but not one that can't be
	// compared
	EXPECT_EQ(DecidePointOrder({.life = 0.0f}), PointOrder::Refused);
	EXPECT_EQ(DecidePointOrder({.life = -0.5f}), PointOrder::Refused);
	EXPECT_EQ(DecidePointOrder({.life = std::numeric_limits<float>::quiet_NaN()}), PointOrder::Refused);
	EXPECT_EQ(DecidePointOrder({.exhaustion = 1.0f}), PointOrder::Refused);
	EXPECT_EQ(DecidePointOrder({.exhaustion = 0.99f}), PointOrder::WalkThere);
	EXPECT_EQ(DecidePointOrder({.exhaustion = std::numeric_limits<float>::quiet_NaN()}), PointOrder::WalkThere);
	// the refusals come before anything it holds
	EXPECT_EQ(DecidePointOrder({.lockedSelect = true, .holding = true}), PointOrder::Refused);

	// holding something, that is all that counts: on the aggression leash or with the point where it can't stand it
	// goes about it another way, else it carries it there to put it down, even onto water or a field
	EXPECT_EQ(DecidePointOrder({.holding = true, .water = true, .field = true}), PointOrder::CarryAndPutDown);
	EXPECT_EQ(DecidePointOrder({.holding = true, .evilLeash = true}), PointOrder::CarryOtherwise);
	EXPECT_EQ(DecidePointOrder({.holding = true, .pointValid = false, .foundValid = false}), PointOrder::CarryOtherwise);
	// the aggression leash alone changes nothing for an empty hand
	EXPECT_EQ(DecidePointOrder({.evilLeash = true}), PointOrder::WalkThere);

	// where it can't stand: a place round it does, else it can't get there, before the busy test
	EXPECT_EQ(DecidePointOrder({.pointValid = false}), PointOrder::WalkThere);
	EXPECT_EQ(DecidePointOrder({.pointValid = false, .foundValid = false, .busy = true}), PointOrder::CantGetThere);
	// busy in that other way, it takes no notice unless it is fighting
	EXPECT_EQ(DecidePointOrder({.busy = true}), PointOrder::Refused);
	EXPECT_EQ(DecidePointOrder({.fighting = true, .busy = true}), PointOrder::WalkThere);

	// on water it drinks only wanting water and with some thirst, else it looks at its reflection; water comes first
	EXPECT_EQ(DecidePointOrder({.water = true, .wantsWater = true, .thirst = 0.2f}), PointOrder::DrinkFromSea);
	EXPECT_EQ(DecidePointOrder({.water = true, .wantsWater = true, .thirst = 0.0f}), PointOrder::LookAtReflection);
	EXPECT_EQ(DecidePointOrder({.water = true, .thirst = 0.2f}), PointOrder::LookAtReflection);
	EXPECT_EQ(DecidePointOrder({.water = true, .field = true}), PointOrder::LookAtReflection);
	EXPECT_EQ(DecidePointOrder({.field = true}), PointOrder::ActOnField);

	// tired near its home, with a temple, and not planning to sleep already: it sleeps; a field comes first
	constexpr PointOrderCheck k_Tired {.exhaustion = 0.5f, .hasTemple = true, .homeToPoint = 11.9f};
	EXPECT_EQ(DecidePointOrder(k_Tired), PointOrder::Sleep);
	auto check = k_Tired;
	check.field = true;
	EXPECT_EQ(DecidePointOrder(check), PointOrder::ActOnField);
	check = k_Tired;
	check.homeToPoint = k_SleepOrderReach;
	EXPECT_EQ(DecidePointOrder(check), PointOrder::WalkThere);
	check = k_Tired;
	check.homeToPoint.reset();
	EXPECT_EQ(DecidePointOrder(check), PointOrder::WalkThere);
	check = k_Tired;
	check.exhaustion = k_SleepOrderExhaustion;
	EXPECT_EQ(DecidePointOrder(check), PointOrder::WalkThere);
	check = k_Tired;
	check.hasTemple = false;
	EXPECT_EQ(DecidePointOrder(check), PointOrder::WalkThere);
	check = k_Tired;
	check.planIsSleep = true;
	EXPECT_EQ(DecidePointOrder(check), PointOrder::WalkThere);
}

TEST(LeashRules, SentToAPointItWalksToWithinItsHeightAtMostEight)
{
	EXPECT_FLOAT_EQ(PointOrderArrival(5.0f), 5.0f);
	EXPECT_FLOAT_EQ(PointOrderArrival(8.0f), 8.0f);
	EXPECT_FLOAT_EQ(PointOrderArrival(30.0f), 8.0f);
	// a height that can't be compared is kept
	EXPECT_TRUE(std::isnan(PointOrderArrival(std::numeric_limits<float>::quiet_NaN())));
}

TEST(LeashRules, EveryOrderOnTheLandIsAcknowledgedButARefusalAndNowhereToGo)
{
	EXPECT_FALSE(AcknowledgesOrder(PointOrder::Refused));
	EXPECT_FALSE(AcknowledgesOrder(PointOrder::CantGetThere));
	for (const auto order : {PointOrder::CarryAndPutDown, PointOrder::CarryOtherwise, PointOrder::DrinkFromSea,
	                         PointOrder::LookAtReflection, PointOrder::ActOnField, PointOrder::Sleep, PointOrder::WalkThere})
	{
		EXPECT_TRUE(AcknowledgesOrder(order));
	}
	EXPECT_EQ(k_OrderAcknowledgedSample, 1);
}

TEST(LeashRules, SentAgainItWaitsTwoSecondsAndTheDrawInWholeTurns)
{
	// the turns a second are a whole division, times the seconds, cut towards 0
	EXPECT_EQ(SecondOrderWaitTurns(0.0f, 100), 20);
	EXPECT_EQ(SecondOrderWaitTurns(1.5f, 100), 35);
	EXPECT_EQ(SecondOrderWaitTurns(2.999f, 100), 49);
	// 1000 / 30 is 33 turns a second: 3.5 s are 115.5, cut to 115
	EXPECT_EQ(SecondOrderWaitTurns(1.5f, 30), 115);
	EXPECT_EQ(SecondOrderWaitTurns(1.5f, 0), 0);

	// a turn counted: over once none are left, a count of none or less is over at its first turn
	EXPECT_EQ(CountWait(3).left, 2);
	EXPECT_FALSE(CountWait(3).over);
	EXPECT_EQ(CountWait(1).left, 0);
	EXPECT_TRUE(CountWait(1).over);
	EXPECT_TRUE(CountWait(0).over);
	EXPECT_TRUE(CountWait(-5).over);
	// below the least it wraps round, as the game counts
	EXPECT_EQ(CountWait(std::numeric_limits<int32_t>::min()).left, std::numeric_limits<int32_t>::max());
	EXPECT_FALSE(CountWait(std::numeric_limits<int32_t>::min()).over);
}

TEST(LeashRules, AYoungCreatureIsKeptAtHomeOnlyOnTheFirstLand)
{
	constexpr HomeKeeping k_Young {
	    .leashed = false,
	    .developmentPhase = 4,
	    .localPlayers = true,
	    .computerPlayer = false,
	    .multiplayer = false,
	    .landNumber = 1,
	};
	EXPECT_TRUE(KeptAtHome(k_Young));
	EXPECT_EQ(k_YoungHomeRadius, 10.0f);
	auto keeping = k_Young;
	keeping.developmentPhase = 0;
	EXPECT_TRUE(KeptAtHome(keeping));
	// each condition on its own keeps it free
	keeping = k_Young;
	keeping.leashed = true;
	EXPECT_FALSE(KeptAtHome(keeping));
	keeping = k_Young;
	keeping.developmentPhase = 5;
	EXPECT_FALSE(KeptAtHome(keeping));
	keeping = k_Young;
	keeping.localPlayers = false;
	EXPECT_FALSE(KeptAtHome(keeping));
	keeping = k_Young;
	keeping.computerPlayer = true;
	EXPECT_FALSE(KeptAtHome(keeping));
	keeping = k_Young;
	keeping.multiplayer = true;
	EXPECT_FALSE(KeptAtHome(keeping));
	keeping = k_Young;
	keeping.landNumber = 2;
	EXPECT_FALSE(KeptAtHome(keeping));
	keeping.landNumber = 0;
	EXPECT_FALSE(KeptAtHome(keeping));
}

TEST(LeashRules, ReachingTheLeavingStageLetsAKeptCreatureGo)
{
	EXPECT_FALSE(ReleasedFromHome(4, k_YoungHomeRadius, false));
	EXPECT_TRUE(ReleasedFromHome(k_YoungUntilPhase, k_YoungHomeRadius, false));
	EXPECT_TRUE(ReleasedFromHome(12, k_YoungHomeRadius, false));
	EXPECT_TRUE(ReleasedFromHome(13, k_HomeConfinement, false));
}

TEST(LeashRules, ALeashedOrFreeCreatureIsNotLetGo)
{
	EXPECT_FALSE(ReleasedFromHome(13, k_YoungHomeRadius, true));
	EXPECT_FALSE(ReleasedFromHome(13, 0.0f, false));
}

TEST(LeashRules, AScriptsOnlyDesireFreesTheOthersOnTheLearningLeashWornOrNot)
{
	// the player's leash in use starts as the learning leash, and a creature's leashes start with it picked
	EXPECT_EQ(k_LeashInUseAtStart, LeashType::Rope);
	EXPECT_EQ(ecs::components::CreatureLeash {}.selected, k_LeashInUseAtStart);
	// a creature with no player, whatever the leash
	EXPECT_TRUE(OnlyDesireLetsGo(false, LeashType::Evil));
	EXPECT_TRUE(OnlyDesireLetsGo(false, LeashType::Good));
	// its player's leash in use the learning leash, on or not
	EXPECT_TRUE(OnlyDesireLetsGo(true, LeashType::Rope));
	// the aggression or compassion leash in use keeps the others held down
	EXPECT_FALSE(OnlyDesireLetsGo(true, LeashType::Evil));
	EXPECT_FALSE(OnlyDesireLetsGo(true, LeashType::Good));
}

TEST(LeashRules, LeashedCreaturesWarmOrCoolToEachOther)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(AttitudeStep(LeashType::Evil)), 0xBDCCCCCDu);
	EXPECT_EQ(std::bit_cast<uint32_t>(AttitudeStep(LeashType::Good)), 0x3DCCCCCDu);
	EXPECT_EQ(AttitudeStep(LeashType::Rope), 0.0f);
}

TEST(LeashRules, TheFirstAttitudeStepIsAtOnceThenAfterMoreThan600Turns)
{
	// never stepped: at once, whatever the turn
	EXPECT_TRUE(AttitudeStepDue(0, 0));
	EXPECT_TRUE(AttitudeStepDue(0, 5));
	// then only once more than 600 turns have passed
	EXPECT_FALSE(AttitudeStepDue(1000, 1000));
	EXPECT_FALSE(AttitudeStepDue(1000, 1600));
	EXPECT_TRUE(AttitudeStepDue(1000, 1601));
	// a turn before the last step counts as long after it
	EXPECT_TRUE(AttitudeStepDue(1000, 999));
}

TEST(LeashRules, TypeIndices)
{
	EXPECT_EQ(IndexOf(LeashType::Evil), 0u);
	EXPECT_EQ(IndexOf(LeashType::Good), 2u);
	EXPECT_FALSE(IndexOf(LeashType::None).has_value());
}

TEST(LeashRules, LessonsOfWhatThePlayerShows)
{
	const auto evil = LessonsFor(LeashType::Evil, false);
	ASSERT_EQ(evil.size(), 2u);
	EXPECT_EQ(evil[0].desire, Desire::Anger);
	EXPECT_FLOAT_EQ(evil[0].change, 1.0f);
	EXPECT_EQ(evil[1].desire, Desire::Compassion);
	EXPECT_FLOAT_EQ(evil[1].change, -1.0f);
	const auto good = LessonsFor(LeashType::Good, true);
	ASSERT_EQ(good.size(), 2u);
	EXPECT_EQ(good[1].desire, Desire::BeFriends);
	EXPECT_FLOAT_EQ(good[1].change, 1.0f);
	EXPECT_TRUE(LessonsFor(LeashType::Rope, false).empty());
}

namespace
{
/// A one-bone fake pose: the collar bone sits 10 up and 2 forward of the body's origin
const std::array<glm::mat4, 1> k_CollarPose = {glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 2.0f))};
constexpr float k_Height = 15.0f;
} // namespace

TEST(LeashRules, ACollarWithNoBoneSitsHighOnTheDrawnBody)
{
	const auto position = glm::vec3(100.0f, 3.0f, 100.0f);
	const auto drawn = affine::Model(position, glm::mat3(1.0f), glm::vec3(1.0f));
	// bit for bit where it sat on the Transform: its position, lifted by its height's share
	const auto expected = position + glm::vec3(0.0f, k_Height * k_CollarHeightShare, 0.0f);
	EXPECT_EQ(CollarAt(drawn, {}, std::nullopt, k_Height, 1.0f), expected);
	// a bone the pose lacks is no bone
	EXPECT_EQ(CollarAt(drawn, k_CollarPose, 1u, k_Height, 1.0f), expected);
}

TEST(LeashRules, InThePenTheCollarIsTheSmallBodysNeck)
{
	constexpr float k_PenShare = 0.22f;
	const auto position = glm::vec3(100.0f, 3.0f, 100.0f);
	const auto rotation = affine::AngleY(0.5f);
	const auto drawn = affine::Model(position, rotation, glm::vec3(k_PenShare));
	const auto high = CollarAt(drawn, {}, std::nullopt, k_Height, k_PenShare);
	EXPECT_FLOAT_EQ(high.x, position.x);
	EXPECT_FLOAT_EQ(high.y, position.y + (k_Height * k_PenShare * k_CollarHeightShare));
	EXPECT_FLOAT_EQ(high.z, position.z);
	const auto neck = CollarAt(drawn, k_CollarPose, 0u, k_Height, k_PenShare);
	const auto expected = position + (rotation * (k_PenShare * glm::vec3(0.0f, 10.0f, 2.0f)));
	EXPECT_NEAR(neck.x, expected.x, k_Tolerance);
	EXPECT_NEAR(neck.y, expected.y, k_Tolerance);
	EXPECT_NEAR(neck.z, expected.z, k_Tolerance);
}

TEST(LeashRules, TheCollarFollowsTheBodyBetweenTurns)
{
	// its turn put it at the end of its step; this frame it is drawn halfway along
	const auto turnEnd = glm::vec3(110.0f, 0.0f, 100.0f);
	const auto midway = glm::vec3(105.0f, 0.0f, 100.0f);
	const auto drawn = affine::Model(midway, glm::mat3(1.0f), glm::vec3(1.0f));
	const auto neck = CollarAt(drawn, k_CollarPose, 0u, k_Height, 1.0f);
	EXPECT_NEAR(neck.x, midway.x, k_Tolerance);
	EXPECT_NEAR(neck.y, 10.0f, k_Tolerance);
	EXPECT_NEAR(neck.z, midway.z + 2.0f, k_Tolerance);
	EXPECT_GT(glm::distance(neck, turnEnd + glm::vec3(0.0f, 10.0f, 2.0f)), 4.0f);
	const auto high = CollarAt(drawn, {}, std::nullopt, k_Height, 1.0f);
	EXPECT_EQ(high, midway + glm::vec3(0.0f, k_Height * k_CollarHeightShare, 0.0f));
}

TEST(LeashRules, AStandingCreatureOutOfThePenKeepsItsCollar)
{
	// drawn where its Transform is, at its own size: the same neck as the renderer's placement gives, up to rounding
	const auto position = glm::vec3(250.0f, 12.0f, -40.0f);
	const auto rotation = affine::AngleY(0.7f);
	const auto scale = glm::vec3(1.3f);
	const auto neck = CollarAt(affine::Model(position, rotation, scale), k_CollarPose, 0u, k_Height, 1.0f);
	const auto before =
	    glm::vec3(creature::PosedBone(0, k_CollarPose, creature::PlacementMatrix(position, rotation, scale))[3]);
	EXPECT_NEAR(neck.x, before.x, k_Tolerance);
	EXPECT_NEAR(neck.y, before.y, k_Tolerance);
	EXPECT_NEAR(neck.z, before.z, k_Tolerance);
}

TEST(LeashRules, AHeldLeashMeetsTheHandAtItsWrist)
{
	// a fake hand of two bones: the wrist (the first) 5 behind the fingertip the hand is placed by, the finger after it
	const std::array<glm::mat4, 2> handBones = {glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, -5.0f)),
	                                            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -0.5f))};
	const auto fingertip = glm::vec3(300.0f, 20.0f, 150.0f);
	const auto rotation = affine::AngleY(0.4f);
	constexpr float k_HandScale = 2.0f;
	const auto hand = affine::Model(fingertip, rotation, glm::vec3(k_HandScale));
	const auto wrist = WristAt(hand, handBones);
	ASSERT_TRUE(wrist.has_value());
	const auto expected = fingertip + (rotation * (k_HandScale * glm::vec3(0.0f, 1.0f, -5.0f)));
	EXPECT_NEAR(wrist->x, expected.x, k_Tolerance);
	EXPECT_NEAR(wrist->y, expected.y, k_Tolerance);
	EXPECT_NEAR(wrist->z, expected.z, k_Tolerance);
	// it is not the fingertip
	EXPECT_GT(glm::distance(*wrist, fingertip), 5.0f);
	// a hand with no posed bones has no wrist
	EXPECT_FALSE(WristAt(hand, {}).has_value());
}

TEST(LeashRules, TiedToAVillageItWantsToImpressItWhileItsBeliefIsLow)
{
	EXPECT_FLOAT_EQ(k_LeashDesireSeconds, 36000.0f);
	EXPECT_FLOAT_EQ(k_ImpressTownSeconds, 120.0f);
	// its player's village believing in them no more than half as much as in another
	EXPECT_TRUE(WantsToImpress({.beliefInPlayer = 2.0f, .mostInAnother = 4.0f, .playersOwn = true}));
	EXPECT_TRUE(WantsToImpress({.beliefInPlayer = 0.0f, .mostInAnother = 0.0f, .playersOwn = true}));
	EXPECT_FALSE(WantsToImpress({.beliefInPlayer = 2.5f, .mostInAnother = 4.0f, .playersOwn = true}));
	// someone else's village, whatever it believes
	EXPECT_TRUE(WantsToImpress({.beliefInPlayer = 9.0f, .mostInAnother = 1.0f, .playersOwn = false}));
	// with no number to compare, only someone else's village
	const auto nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FALSE(WantsToImpress({.beliefInPlayer = nan, .mostInAnother = 4.0f, .playersOwn = true}));
	EXPECT_TRUE(WantsToImpress({.beliefInPlayer = nan, .mostInAnother = 4.0f, .playersOwn = false}));
}
