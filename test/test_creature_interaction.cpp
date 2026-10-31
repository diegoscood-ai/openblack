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
#include <numeric>
#include <optional>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "3D/SkeletalAnimation.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureReach.h"
#include "Creature/CreatureThrow.h"
#include "ECS/ObjectMetrics.h"

using namespace openblack;

namespace
{
constexpr float k_Tolerance = 1e-4f;

/// Four made-up reach corners: back left, back right, front left, front right, the left being +x
creature_reach::Points FakeReach()
{
	return {glm::vec3(2.0f, 3.0f, -1.0f), glm::vec3(-2.0f, 3.0f, -1.0f), glm::vec3(2.0f, 2.0f, -7.0f),
	        glm::vec3(-2.0f, 2.0f, -7.0f)};
}

float Sum(const std::array<float, 4>& weights)
{
	return std::accumulate(weights.begin(), weights.end(), 0.0f);
}

/// The same random number every time
uint32_t First(uint32_t /*range*/)
{
	return 0;
}
} // namespace

TEST(CreatureReach, ACornerTakesAllTheWeight)
{
	const auto points = FakeReach();
	for (size_t i = 0; i < points.size(); ++i)
	{
		const auto blend = creature_reach::Solve(points, points.at(i));
		EXPECT_TRUE(blend.inRange);
		for (size_t j = 0; j < points.size(); ++j)
		{
			EXPECT_NEAR(blend.weights.at(j), i == j ? 1.0f : 0.0f, k_Tolerance) << i << " " << j;
		}
	}
}

TEST(CreatureReach, TheMiddleBlendsAllFourEvenly)
{
	const auto blend = creature_reach::Solve(FakeReach(), glm::vec3(0.0f, 0.0f, -4.0f));
	EXPECT_TRUE(blend.inRange);
	for (const auto weight : blend.weights)
	{
		EXPECT_NEAR(weight, 0.25f, k_Tolerance);
	}
}

TEST(CreatureReach, StretchingPastTheCornersExtrapolatesThenFails)
{
	const auto points = FakeReach();
	// Half again as far forward as the front corners: still in reach, the back animations counting against
	const auto stretched = creature_reach::Solve(points, glm::vec3(0.0f, 0.0f, -10.0f));
	EXPECT_TRUE(stretched.inRange);
	EXPECT_NEAR(stretched.backToFront, 1.5f, k_Tolerance);
	EXPECT_NEAR(Sum(stretched.weights), 1.0f, k_Tolerance);
	EXPECT_LT(stretched.weights.at(0), 0.0f);
	// Past 1.8 of the depth, or more than half the width beyond either side, it can't reach
	EXPECT_FALSE(creature_reach::Solve(points, glm::vec3(0.0f, 0.0f, -12.0f)).inRange);
	EXPECT_FALSE(creature_reach::Solve(points, glm::vec3(4.5f, 0.0f, -4.0f)).inRange);
	EXPECT_FALSE(creature_reach::Solve(points, glm::vec3(0.0f, 0.0f, 4.0f)).inRange);
}

TEST(CreatureReach, HeightDoesNotMatter)
{
	const auto low = creature_reach::Solve(FakeReach(), glm::vec3(1.0f, 0.0f, -3.0f));
	const auto high = creature_reach::Solve(FakeReach(), glm::vec3(1.0f, 50.0f, -3.0f));
	for (size_t i = 0; i < low.weights.size(); ++i)
	{
		EXPECT_NEAR(low.weights.at(i), high.weights.at(i), k_Tolerance);
	}
}

TEST(CreatureReach, TheOtherSideIsReachedMirrored)
{
	// Corners biased to the right (-x), as a right hand's are
	auto points = FakeReach();
	for (auto& point : points)
	{
		point.x -= 1.0f;
	}
	EXPECT_FALSE(creature_reach::ReachesMirrored(points, glm::vec3(-2.0f, 0.0f, -4.0f)));
	EXPECT_TRUE(creature_reach::ReachesMirrored(points, glm::vec3(2.0f, 0.0f, -4.0f)));
	const auto mirrored = creature_reach::Mirrored(points);
	for (size_t i = 0; i < points.size(); ++i)
	{
		EXPECT_FLOAT_EQ(mirrored.at(i).x, -points.at(i).x);
		EXPECT_FLOAT_EQ(mirrored.at(i).z, points.at(i).z);
	}
	// A point and its reflection blend the same on their own sides
	const auto right = creature_reach::Solve(points, glm::vec3(-1.5f, 0.0f, -3.0f));
	const auto left = creature_reach::Solve(mirrored, glm::vec3(1.5f, 0.0f, -3.0f));
	for (size_t i = 0; i < right.weights.size(); ++i)
	{
		EXPECT_NEAR(right.weights.at(i), left.weights.at(i), k_Tolerance);
	}
}

TEST(CreatureReach, MaxReachIsTheFrontStretchedAsFarAsItGoes)
{
	// -0.4 of the back pair and 0.9 of the front pair: z = 0.8 - 12.6
	EXPECT_NEAR(creature_reach::MaxReach(FakeReach()), 11.8f, k_Tolerance);
	// The same point with its height: y = -0.4 x 6 + 0.9 x 4 = 1.2
	EXPECT_NEAR(creature_reach::MaxReach3D(FakeReach(), 1.0f), std::sqrt((1.2f * 1.2f) + (11.8f * 11.8f)), k_Tolerance);
	EXPECT_NEAR(creature_reach::MaxReach3D(FakeReach(), 0.5f), 0.5f * std::sqrt((1.2f * 1.2f) + (11.8f * 11.8f)), k_Tolerance);
	EXPECT_NEAR(creature_reach::Centre(FakeReach()).z, -4.0f, k_Tolerance);
}

TEST(CreatureFeedback, AStrokeLandsOnTheNearestPart)
{
	std::array<glm::vec3, creature_feedback::k_BodyPartCount> parts {};
	for (size_t i = 0; i < parts.size(); ++i)
	{
		parts.at(i) = glm::vec3(static_cast<float>(i) * 10.0f, 0.0f, 0.0f);
	}
	EXPECT_EQ(creature_feedback::NearestPart(glm::vec3(1.0f, 2.0f, 0.0f), parts), creature_feedback::BodyPart::Head);
	EXPECT_EQ(creature_feedback::NearestPart(glm::vec3(31.0f, 0.0f, 0.0f), parts), creature_feedback::BodyPart::Belly);
	EXPECT_EQ(creature_feedback::NearestPart(glm::vec3(200.0f, 0.0f, 0.0f), parts), creature_feedback::BodyPart::LeftHand);
	// The left side's rewards are the right's mirrored
	EXPECT_EQ(creature_feedback::k_RewardAnimations.at(1), creature_feedback::k_RewardAnimations.at(2));
	EXPECT_TRUE(creature_feedback::k_RewardMirrored.at(2));
	EXPECT_FALSE(creature_feedback::k_RewardMirrored.at(1));
}

TEST(CreatureFeedback, AStrokeOrASlapBreaksInOnlyPastItsShare)
{
	using creature_feedback::MayInterrupt;
	// the slap's share is the float 0.35: 7 of 20 ms is exactly it, and is not past it
	EXPECT_EQ(7.0f / 20.0f, 0.35f);
	EXPECT_FALSE(MayInterrupt(6.0f, 20.0f, creature_feedback::k_SlapInterruptsAfter));
	EXPECT_FALSE(MayInterrupt(7.0f, 20.0f, creature_feedback::k_SlapInterruptsAfter));
	EXPECT_TRUE(MayInterrupt(8.0f, 20.0f, creature_feedback::k_SlapInterruptsAfter));
	EXPECT_TRUE(MayInterrupt(351.0f, 1000.0f, creature_feedback::k_SlapInterruptsAfter));
	// the stroke's is the double 0.8, which no float share equals: 4 of 5 is the float 0.8, just past it
	EXPECT_FALSE(MayInterrupt(799.0f, 1000.0f, creature_feedback::k_StrokeInterruptsAfter));
	EXPECT_TRUE(MayInterrupt(4.0f, 5.0f, creature_feedback::k_StrokeInterruptsAfter));
	EXPECT_TRUE(MayInterrupt(800.0f, 1000.0f, creature_feedback::k_StrokeInterruptsAfter));
}

TEST(CreatureFeedback, SlapsAreClassedByHeightAndSpeed)
{
	constexpr float k_Height = 15.0f;
	// Too slow, or too high or under the ground, is no slap
	EXPECT_FALSE(creature_feedback::ClassifySlap(7.0f, 5.0f * k_Height, k_Height, false).has_value());
	EXPECT_FALSE(creature_feedback::ClassifySlap(17.0f, 100.0f, k_Height, false).has_value());
	EXPECT_FALSE(creature_feedback::ClassifySlap(-1.0f, 100.0f, k_Height, false).has_value());
	// Feet, waist and head, gently below nine heights a second
	const auto feet = creature_feedback::ClassifySlap(3.0f, 100.0f, k_Height, false);
	ASSERT_TRUE(feet.has_value());
	EXPECT_EQ(feet->animation, 175u);
	EXPECT_TRUE(feet->gentle);
	const auto waist = creature_feedback::ClassifySlap(9.0f, 200.0f, k_Height, true);
	ASSERT_TRUE(waist.has_value());
	EXPECT_EQ(waist->animation, 167u);
	EXPECT_FALSE(waist->gentle);
	EXPECT_TRUE(waist->mirrored);
	const auto head = creature_feedback::ClassifySlap(12.0f, 200.0f, k_Height, false);
	ASSERT_TRUE(head.has_value());
	EXPECT_EQ(head->animation, 165u);
}

TEST(CreatureFeedback, StrokesAndSlapsAddUp)
{
	auto sum = 0.0f;
	for (int i = 0; i < 3; ++i)
	{
		sum = creature_feedback::AfterStroke(sum);
	}
	EXPECT_NEAR(sum, 0.3f, k_Tolerance);
	// Enjoying it, a slap takes twice as much off
	sum = creature_feedback::AfterSlap(sum, true);
	EXPECT_NEAR(sum, 0.1f, k_Tolerance);
	sum = creature_feedback::AfterSlap(sum, false);
	EXPECT_NEAR(sum, -0.1f, k_Tolerance);
	for (int i = 0; i < 20; ++i)
	{
		sum = creature_feedback::AfterSlap(sum, false);
	}
	EXPECT_FLOAT_EQ(sum, -1.0f);
	for (int i = 0; i < 40; ++i)
	{
		sum = creature_feedback::AfterStroke(sum);
	}
	EXPECT_FLOAT_EQ(sum, 1.0f);
	EXPECT_FLOAT_EQ(creature_feedback::Delivered(1.5f), 1.0f);
	EXPECT_NEAR(creature_feedback::AttitudeAfter(0.5f, 1.0f), 0.4f, k_Tolerance);
	EXPECT_NEAR(creature_feedback::AverageAfter(0.5f, -1.0f), -0.4f, k_Tolerance);
}

TEST(CreatureFeedback, TheHandTouchesTheBodyWhereTheLineOfSightMeetsIt)
{
	const std::array<creature_feedback::Capsule, 2> body {{
	    {.from = glm::vec3(0.0f, 0.0f, 0.0f), .to = glm::vec3(0.0f, 10.0f, 0.0f), .radius = 1.0f},
	    {.from = glm::vec3(5.0f, 0.0f, 0.0f), .to = glm::vec3(5.0f, 10.0f, 0.0f), .radius = 1.0f},
	}};
	const auto hit = creature_feedback::RayHit(glm::vec3(0.0f, 5.0f, 20.0f), glm::vec3(0.0f, 0.0f, -2.0f), body);
	ASSERT_TRUE(hit.has_value());
	// It enters the near side of the first capsule, 19 units along: 9.5 lengths of the direction
	EXPECT_NEAR(*hit, 9.5f, k_Tolerance);
	EXPECT_FALSE(creature_feedback::RayHit(glm::vec3(2.5f, 5.0f, 20.0f), glm::vec3(0.0f, 0.0f, -1.0f), body).has_value());
	EXPECT_FALSE(creature_feedback::RayHit(glm::vec3(0.0f, 5.0f, 20.0f), glm::vec3(0.0f, 0.0f, 1.0f), body).has_value());
}

TEST(CreatureFeedback, TheBodyIsACapsuleFromEachJointToItsParentsPlaced)
{
	// A root and two bones hanging off it, the last off the first, its own parent out of range
	const std::array<uint32_t, 3> parents {0xFFFFFFFF, 0, 1};
	const std::array<glm::mat4, 3> bones {
	    glm::translate(glm::vec3(0.0f, 1.0f, 0.0f)),
	    glm::translate(glm::vec3(0.0f, 3.0f, 0.0f)) * glm::eulerAngleY(0.5f),
	    glm::translate(glm::vec3(2.0f, 3.0f, 0.0f)),
	};
	const auto placement = glm::translate(glm::vec3(10.0f, 0.0f, -4.0f)) * glm::scale(glm::vec3(2.0f));
	const auto capsules = creature_feedback::BodyCapsules(parents, bones, placement, 0.5f);
	ASSERT_EQ(capsules.size(), 3u);
	// The root's capsule is only the point where it is
	EXPECT_EQ(capsules[0].from, glm::vec3(10.0f, 2.0f, -4.0f));
	EXPECT_EQ(capsules[0].to, glm::vec3(10.0f, 2.0f, -4.0f));
	EXPECT_EQ(capsules[1].from, glm::vec3(10.0f, 2.0f, -4.0f));
	EXPECT_EQ(capsules[1].to, glm::vec3(10.0f, 6.0f, -4.0f));
	EXPECT_EQ(capsules[2].from, glm::vec3(10.0f, 6.0f, -4.0f));
	EXPECT_EQ(capsules[2].to, glm::vec3(14.0f, 6.0f, -4.0f));
	for (const auto& capsule : capsules)
	{
		EXPECT_FLOAT_EQ(capsule.radius, 0.5f);
	}
	// Each joint is where its whole posed matrix puts the bone's origin
	EXPECT_EQ(capsules[1].to, glm::vec3((placement * bones[1])[3]));
}

TEST(CreatureObjectActions, EachActionIsDoneWithTheHandsItNeeds)
{
	using creature_object_actions::Kind;
	EXPECT_EQ(creature_object_actions::HandsFor(Kind::PickUp), creature_object_actions::Hands::Empty);
	EXPECT_EQ(creature_object_actions::HandsFor(Kind::Throw), creature_object_actions::Hands::Holding);
	// a take from the hand with something held starts, to take nothing
	EXPECT_EQ(creature_object_actions::HandsFor(Kind::TakeFromHand), creature_object_actions::Hands::Either);
}

TEST(CreatureObjectActions, EachKindOfThingHasItsOwnRuleForBeingPickedUp)
{
	using creature_object_actions::DecidedByKind;
	using creature_object_actions::KindRule;
	// Never, whatever holds
	EXPECT_EQ(DecidedByKind({.rule = KindRule::Never, .holdable = true}, true), false);
	// The football and a creed always, even when a script said no
	EXPECT_EQ(DecidedByKind({.rule = KindRule::Always, .holdable = true}, false), true);
	// A bird only when low over the land, whatever else holds
	EXPECT_EQ(DecidedByKind({.rule = KindRule::Bird, .holdable = true, .low = true}, false), true);
	EXPECT_EQ(DecidedByKind({.rule = KindRule::Bird, .holdable = true, .low = false}, true), false);
	// A pile unless a script said no, and only a kind that may fly about
	EXPECT_EQ(DecidedByKind({.rule = KindRule::Pile, .holdable = true, .mayFly = true}, true), true);
	EXPECT_EQ(DecidedByKind({.rule = KindRule::Pile, .holdable = true, .mayFly = false}, true), false);
	EXPECT_EQ(DecidedByKind({.rule = KindRule::Pile, .holdable = true, .mayFly = true}, false), false);
	// Anything else a hand may hold by the test of things in general
	EXPECT_FALSE(DecidedByKind({.rule = KindRule::Whole, .holdable = true}, true).has_value());
	EXPECT_EQ(DecidedByKind({.rule = KindRule::Whole, .holdable = false}, true), false);
}

TEST(CreatureObjectActions, EachKindOfThingTakesItsRule)
{
	using creature_object_actions::Holdable;
	using creature_object_actions::KindRule;
	using creature_object_actions::RuleOf;
	using creature_object_actions::Thing;
	EXPECT_EQ(RuleOf(Thing::Creature), KindRule::Never);
	EXPECT_FALSE(Holdable(Thing::Creature));
	EXPECT_EQ(RuleOf(Thing::Other), KindRule::Never);
	EXPECT_EQ(RuleOf(Thing::Ball), KindRule::Always);
	EXPECT_EQ(RuleOf(Thing::Creed), KindRule::Always);
	EXPECT_EQ(RuleOf(Thing::Bird), KindRule::Bird);
	EXPECT_EQ(RuleOf(Thing::Pile), KindRule::Pile);
	for (const auto thing : {Thing::Villager, Thing::Animal, Thing::MobileObject, Thing::Pot})
	{
		EXPECT_EQ(RuleOf(thing), KindRule::Whole);
		EXPECT_TRUE(Holdable(thing));
	}
}

TEST(CreatureObjectActions, RocksBonfiresAndTreesTakeTheWholeRule)
{
	using creature_object_actions::KindRule;
	using creature_object_actions::Thing;
	for (const auto thing : {Thing::Rock, Thing::Bonfire, Thing::DeadTree, Thing::Tree})
	{
		EXPECT_EQ(creature_object_actions::RuleOf(thing), KindRule::Whole);
		EXPECT_TRUE(creature_object_actions::Holdable(thing));
		EXPECT_FALSE(creature_object_actions::DecidedByKind({.thing = thing, .rule = KindRule::Whole, .holdable = true}, true)
		                 .has_value());
	}
}

TEST(CreatureObjectActions, OfOtherStaticsOnlyTheToysArePickedUp)
{
	using creature_object_actions::KindRule;
	using creature_object_actions::Thing;
	EXPECT_EQ(creature_object_actions::RuleOf(Thing::Static), KindRule::Toy);
	const creature_object_actions::KindFacts toy {.rule = KindRule::Toy, .holdable = true, .toy = true};
	const creature_object_actions::KindFacts statue {.rule = KindRule::Toy, .holdable = true, .toy = false};
	// A toy whatever else holds
	EXPECT_EQ(creature_object_actions::DecidedByKind(toy, false), true);
	EXPECT_EQ(creature_object_actions::DecidedByKind(statue, true), false);
	// Lanterns and shields no hand holds; vortices and teleport stones a creature never picks up
	for (const auto thing : {Thing::Lantern, Thing::Shield, Thing::Vortex, Thing::TeleportStone})
	{
		EXPECT_EQ(creature_object_actions::RuleOf(thing), KindRule::Never);
	}
	EXPECT_FALSE(creature_object_actions::Holdable(Thing::Lantern));
	EXPECT_FALSE(creature_object_actions::Holdable(Thing::Shield));
	EXPECT_TRUE(creature_object_actions::Holdable(Thing::Vortex));
}

TEST(CreatureObjectActions, AMagicMushroomIsPickedUpUnlessAScriptSaidNo)
{
	using creature_object_actions::KindRule;
	using creature_object_actions::Thing;
	EXPECT_EQ(creature_object_actions::RuleOf(Thing::MagicMushroom), KindRule::Mushroom);
	const creature_object_actions::KindFacts mushroom {.rule = KindRule::Mushroom, .holdable = false};
	EXPECT_EQ(creature_object_actions::DecidedByKind(mushroom, true), true);
	EXPECT_EQ(creature_object_actions::DecidedByKind(mushroom, false), false);
	// No hand holds one
	EXPECT_FALSE(creature_object_actions::Holdable(Thing::MagicMushroom));
}

TEST(CreatureObjectActions, TheToysAreKnownByTheirModels)
{
	using creature_object_actions::Toy;
	using creature_object_actions::ToyOf;
	EXPECT_EQ(ToyOf(MeshId::ObjectToyBall), Toy::Ball);
	EXPECT_EQ(ToyOf(MeshId::ObjectToyBowlingBall), Toy::BowlingBall);
	EXPECT_EQ(ToyOf(MeshId::ObjectToyCuddly), Toy::Cuddly);
	EXPECT_EQ(ToyOf(MeshId::ObjectToyDice), Toy::Die);
	EXPECT_EQ(ToyOf(MeshId::ObjectToySkittle), Toy::Skittle);
	EXPECT_FALSE(ToyOf(MeshId::Dummy).has_value());
}

TEST(CreatureDesires, FeedbackPushesEverySourceOfAType)
{
	creature_desires::Desires desires;
	desires[creature_desires::Desire::Play].sources = {{.type = 12, .value = 0.2f, .threshold = 0.5f, .multiplier = 1.0f}};
	desires[creature_desires::Desire::Anger].sources = {{.type = 9, .value = 0.9f, .threshold = 0.5f, .multiplier = 1.0f}};
	creature_desires::ChangeSource(desires, 12, 0.5f);
	creature_desires::ChangeSource(desires, 9, 0.5f);
	EXPECT_NEAR(desires[creature_desires::Desire::Play].sources.front().value, 0.7f, k_Tolerance);
	EXPECT_FLOAT_EQ(desires[creature_desires::Desire::Anger].sources.front().value, 1.0f);
}

// The desire a source drives is the first desire that grows from a source of its type
TEST(CreatureDesires, TheFirstDesireThatGrowsFromASourceIsItsDesire)
{
	creature_desires::Desires desires;
	desires[creature_desires::Desire::Compassion].sources = {{.type = 3, .value = 0.0f}};
	desires[creature_desires::Desire::Anger].sources = {{.type = 7, .value = 0.0f}, {.type = 3, .value = 0.0f}};
	EXPECT_EQ(creature_desires::DesireOfSource(desires, 3), std::optional(creature_desires::Desire::Compassion));
	EXPECT_EQ(creature_desires::DesireOfSource(desires, 7), std::optional(creature_desires::Desire::Anger));
	EXPECT_FALSE(creature_desires::DesireOfSource(desires, 1).has_value());
}

TEST(CreatureIdleMind, EatingIsPickingUpExaminingAndEating)
{
	const auto agenda = creature_mind::Eat(3u);
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].order.kind, creature_mind::ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].order.animation, creature_object_actions::k_ExamineObject);
	EXPECT_EQ(agenda[2].order.kind, creature_mind::ObjectOrder::Kind::Eat);
	EXPECT_EQ(agenda[2].effect, creature_mind::Effect::Eat);
}

TEST(CreatureIdleMind, EatingAliveIsHungryOneTimeInTwoThenPickingUpAndEating)
{
	// The game's eating alive: unless what it holds is a thing it can eat, a draw of two, 0 for the hungry gesture, then
	// the pick-up; the eating is the main step, and none of the steps pulls a face of its own (no examining, unlike
	// eating after examining) (docs/wiki/creature/plans-and-needs.md#eating-alive)
	std::vector<uint32_t> ranges;
	const auto draw = [&ranges](uint32_t result) {
		return [&ranges, result](uint32_t range) {
			ranges.push_back(range);
			return result;
		};
	};
	auto agenda = creature_mind::EatAlive(3u, false, draw(0));
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(ranges, std::vector<uint32_t> {2u});
	EXPECT_EQ(agenda[0].kind, creature_mind::Step::Kind::Action);
	EXPECT_EQ(agenda[0].animation, creature_layers::animations::k_Hungry);
	EXPECT_EQ(agenda[1].order.kind, creature_mind::ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].order.object, std::optional<uint32_t>(3u));
	EXPECT_EQ(agenda[2].order.kind, creature_mind::ObjectOrder::Kind::Eat);
	EXPECT_EQ(agenda[2].effect, creature_mind::Effect::Eat);
	EXPECT_TRUE(agenda[2].main);
	for (const auto& step : agenda)
	{
		EXPECT_EQ(step.face, creature_face::Cue::None);
	}
	// any other draw: no gesture
	ranges.clear();
	agenda = creature_mind::EatAlive(3u, false, draw(1));
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].order.kind, creature_mind::ObjectOrder::Kind::PickUp);
	// holding a thing it can eat: no draw and no pick-up, it eats
	ranges.clear();
	agenda = creature_mind::EatAlive(3u, true, draw(0));
	ASSERT_EQ(agenda.size(), 1u);
	EXPECT_TRUE(ranges.empty());
	EXPECT_EQ(agenda[0].order.kind, creature_mind::ObjectOrder::Kind::Eat);
	// the game's row for eating alive is built this way, and eating after examining keeps its own agenda
	const auto* alive = creature_plan_actions::For("EatAlive");
	ASSERT_NE(alive, nullptr);
	EXPECT_EQ(alive->build, creature_plan_actions::Build::EatAlive);
	const auto* examined = creature_plan_actions::For("EatAfterExamining");
	ASSERT_NE(examined, nullptr);
	EXPECT_EQ(examined->build, creature_plan_actions::Build::Eat);
}

TEST(CreatureIdleMind, StompingAndEatingKnocksTheThingDownAngryThenPicksItUpAndEatsIt)
{
	// The game's stomp and eat: unless what it holds is a thing it can eat, the knock-down with the angry face and the
	// pick-up, each looking at the thing on the way to it, then the eat as the main step, which neither looks nor pulls
	// a face (docs/wiki/creature/script-control.md#an-action-forced-by-a-script-creature_do_action)
	auto agenda = creature_mind::StompAndEat(3u, false);
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].kind, creature_mind::Step::Kind::Object);
	EXPECT_EQ(agenda[0].order.kind, creature_mind::ObjectOrder::Kind::Destroy);
	EXPECT_EQ(agenda[0].order.object, std::optional<uint32_t>(3u));
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Anger);
	EXPECT_EQ(agenda[1].order.kind, creature_mind::ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].order.object, std::optional<uint32_t>(3u));
	EXPECT_EQ(agenda[1].face, creature_face::Cue::None);
	for (size_t i = 0; i < 2; ++i)
	{
		ASSERT_TRUE(agenda[i].gaze.has_value()) << i;
		EXPECT_EQ(agenda[i].gaze->object, std::optional<uint32_t>(3u)) << i;
		EXPECT_TRUE(agenda[i].gaze->goingTowards) << i;
		EXPECT_FALSE(agenda[i].main) << i;
	}
	EXPECT_EQ(agenda[2].order.kind, creature_mind::ObjectOrder::Kind::Eat);
	EXPECT_EQ(agenda[2].effect, creature_mind::Effect::Eat);
	EXPECT_TRUE(agenda[2].main);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::None);
	EXPECT_FALSE(agenda[2].gaze.has_value());
	// holding a thing it can eat: it eats it at once
	agenda = creature_mind::StompAndEat(3u, true);
	ASSERT_EQ(agenda.size(), 1u);
	EXPECT_EQ(agenda[0].order.kind, creature_mind::ObjectOrder::Kind::Eat);
	EXPECT_TRUE(agenda[0].main);
	// no desire lists the row, so only a forced plan builds it
	EXPECT_EQ(creature_plan_actions::For("StompAndEat"), nullptr);
	const auto* forced = creature_plan_actions::ForcedFor("StompAndEat");
	ASSERT_NE(forced, nullptr);
	EXPECT_EQ(forced->build, creature_plan_actions::Build::StompAndEat);
}

TEST(CreatureObjectActions, AKnockDownCrushesALivingThingInTheMapWhileItPlaysOnAfterTheBlow)
{
	// Each turn from the blow to the end of the animation, a living thing still in the map, by a crush of 1 a metre wide
	using creature_object_actions::Kind;
	using creature_object_actions::Status;
	EXPECT_TRUE(creature_object_actions::KnockDownCrushes(Kind::Destroy, Status::Contact, true, true));
	// not before the blow, nor after the animation, nor if it failed
	EXPECT_FALSE(creature_object_actions::KnockDownCrushes(Kind::Destroy, Status::Running, true, true));
	EXPECT_FALSE(creature_object_actions::KnockDownCrushes(Kind::Destroy, Status::Done, true, true));
	EXPECT_FALSE(creature_object_actions::KnockDownCrushes(Kind::Destroy, Status::Failed, true, true));
	// a thing that is not living, or one out of the map, takes nothing
	EXPECT_FALSE(creature_object_actions::KnockDownCrushes(Kind::Destroy, Status::Contact, false, true));
	EXPECT_FALSE(creature_object_actions::KnockDownCrushes(Kind::Destroy, Status::Contact, true, false));
	// a pick-up that has hold of a living thing does not crush it
	EXPECT_FALSE(creature_object_actions::KnockDownCrushes(Kind::PickUp, Status::Contact, true, true));
	EXPECT_FLOAT_EQ(creature_object_actions::k_KnockDownCrush, 1.0f);
	EXPECT_FLOAT_EQ(creature_object_actions::k_KnockDownCrushRadius, 1.0f);
}

TEST(CreatureIdleMind, AnObjectStepWaitsForTheHandsAndGivesUpWhenTheyFail)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Examine, creature_mind::ExamineByPickingUp(4u, false, First));
	creature_mind::Senses senses {.seconds = 0.1f};
	auto commands = creature_mind::Think(mind, senses, First);
	ASSERT_TRUE(commands.object.has_value());
	EXPECT_EQ(commands.object->kind, creature_mind::ObjectOrder::Kind::PickUp);
	senses.hands = creature_mind::HandsState::Busy;
	commands = creature_mind::Think(mind, senses, First);
	EXPECT_EQ(mind.step, 0u);
	senses.hands = creature_mind::HandsState::Done;
	commands = creature_mind::Think(mind, senses, First);
	EXPECT_EQ(mind.step, 1u);
	// The next step starts, then the hands fail: the rest is given up
	commands = creature_mind::Think(mind, senses, First);
	ASSERT_TRUE(commands.object.has_value());
	EXPECT_EQ(commands.object->kind, creature_mind::ObjectOrder::Kind::Keep);
	senses.hands = creature_mind::HandsState::Failed;
	commands = creature_mind::Think(mind, senses, First);
	EXPECT_GE(mind.step, mind.agenda.size());
}

TEST(CreatureIdleMind, ThrowingAboutAimsFromWhereItStands)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::PlayWithObject, creature_mind::ThrowAbout(4u, First));
	mind.step = 1;
	const creature_mind::Senses senses {.seconds = 0.1f, .position = glm::vec2(100.0f, 200.0f)};
	const auto commands = creature_mind::Think(mind, senses, First);
	ASSERT_TRUE(commands.object.has_value());
	EXPECT_EQ(commands.object->kind, creature_mind::ObjectOrder::Kind::Throw);
	// 30 units away at an angle of 0
	EXPECT_NEAR(commands.object->point.x, 130.0f, k_Tolerance);
	EXPECT_NEAR(commands.object->point.y, 200.0f, k_Tolerance);
}

TEST(SkeletalMirror, AMirroredPoseIsTheExactReflection)
{
	// A middle bone and a pair of arms whose rest frames are not mirror images of each other
	const std::array<glm::mat4, 3> rest {
	    glm::mat4(1.0f),
	    glm::translate(glm::vec3(2.0f, 1.0f, 0.0f)) * glm::eulerAngleZ(0.4f),
	    glm::translate(glm::vec3(-2.0f, 1.0f, 0.0f)) * glm::eulerAngleX(1.1f),
	};
	const std::array<uint32_t, 3> parents {skeletal_animation::k_NoParent, 0, 0};
	const auto skeleton = skeletal_animation::Skeleton::FromRestMatrices(parents, rest);
	const std::vector<uint32_t> mirror {0, 2, 1};
	std::vector<skeletal_animation::Pose> poses {
	    {.rotation = skeletal_animation::RotationYXZ({0.1f, 0.3f, -0.2f}), .translation = glm::vec3(0.5f, 3.0f, 1.0f)},
	    {.rotation = skeletal_animation::RotationYXZ({0.7f, -0.4f, 0.9f}), .translation = glm::vec3(2.0f, 1.0f, 0.0f)},
	    {.rotation = skeletal_animation::RotationYXZ({-0.2f, 0.5f, 0.1f}), .translation = glm::vec3(-2.0f, 1.0f, 0.0f)},
	};
	const auto plain = skeletal_animation::ComposeBoneMatrices(poses, parents);
	const auto mirrored =
	    skeletal_animation::ComposeBoneMatrices(skeletal_animation::MirrorPoses(poses, skeleton, mirror), parents);
	const glm::mat4 reflect = glm::scale(glm::vec3(-1.0f, 1.0f, 1.0f));
	for (size_t bone = 0; bone < 3; ++bone)
	{
		// Each bone stands where its mirror bone stood, reflected, and bends away from its rest pose the same way
		const auto& other = plain[mirror[bone]];
		const auto place = glm::vec3(mirrored[bone][3]);
		EXPECT_NEAR(place.x, -other[3].x, k_Tolerance);
		EXPECT_NEAR(place.y, other[3].y, k_Tolerance);
		EXPECT_NEAR(place.z, other[3].z, k_Tolerance);
		const auto bend = glm::mat3(mirrored[bone]) * glm::inverse(glm::mat3(rest[bone]));
		const auto expected =
		    glm::mat3(reflect) * glm::mat3(other) * glm::inverse(glm::mat3(rest[mirror[bone]])) * glm::mat3(reflect);
		for (glm::length_t c = 0; c < 3; ++c)
		{
			for (glm::length_t r = 0; r < 3; ++r)
			{
				EXPECT_NEAR(bend[c][r], expected[c][r], 1e-3f) << bone;
			}
		}
	}
}

namespace
{
float Bits(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

uint32_t BitsOf(float value)
{
	return std::bit_cast<uint32_t>(value);
}

glm::vec3 Bits(uint32_t x, uint32_t y, uint32_t z)
{
	return {Bits(x), Bits(y), Bits(z)};
}
} // namespace

TEST(CreatureThrow, TheFlightTimePerDistanceIsOneOverGravityInSinglePrecision)
{
	volatile float gravity = creature_throw::k_Gravity;
	EXPECT_EQ(BitsOf(creature_throw::k_FlightSecondsSquaredPerDistance), BitsOf(1.0f / gravity));
}

TEST(CreatureThrow, AThrowsFlightTimeIsTheGamesOverTheGroundDistance)
{
	// From the original: the ground distance's bits and the time its throw at a point takes. The first six fall on the
	// last bit where dividing by gravity would round the other way.
	struct Case
	{
		uint32_t distance;
		uint32_t seconds;
	};
	constexpr std::array<Case, 9> k_Cases {{
	    {0x42C4D19A, 0x404AB463},
	    {0x42D38BE0, 0x405226E1},
	    {0x428F3510, 0x402CE83C},
	    {0x4370F4ED, 0x409E97E6},
	    {0x43FE3EAB, 0x40E662DF},
	    {0x443EE662, 0x410D2984},
	    {0x411CF5C3, 0x3F800000},
	    {0x41700000, 0x3F9E4738},
	    {0x42C80000, 0x404C5616},
	}};
	for (const auto& c : k_Cases)
	{
		EXPECT_EQ(BitsOf(creature_throw::ThrowFlightTime(Bits(c.distance))), c.seconds) << Bits(c.distance);
	}
}

TEST(CreatureThrow, TheGroundDistanceLeavesTheHeightsOut)
{
	// From the original's throw into a store: where the creature stands, where it throws and the time it takes
	struct Case
	{
		std::array<uint32_t, 3> creature;
		std::array<uint32_t, 3> point;
		uint32_t seconds;
	};
	constexpr std::array<Case, 4> k_Cases {{
	    {{0x44E99690, 0x450B15C7, 0x4515194B}, {0x3FF0F000, 0x40F00000, 0x00000000}, 0x418C941D},
	    {{0x4530B59D, 0x450ABB22, 0x452CEF9A}, {0x3FF0F000, 0x40F00000, 0x00000000}, 0x41A09FC4},
	    {{0x42AE0808, 0x44AE9BC6, 0x4530E11F}, {0x3FF0F000, 0x40F00000, 0x00000000}, 0x4187E907},
	    {{0x44F35D8F, 0x4528EB39, 0x43A9CF17}, {0x3FF0F000, 0x40F00000, 0x00000000}, 0x4162FDF5},
	}};
	for (const auto& c : k_Cases)
	{
		const auto creature = Bits(c.creature[0], c.creature[1], c.creature[2]);
		const auto point = Bits(c.point[0], c.point[1], c.point[2]);
		EXPECT_EQ(BitsOf(creature_throw::ThrowFlightTime(creature_throw::GroundDistance(creature, point))), c.seconds);
	}
	EXPECT_EQ(creature_throw::GroundDistance({0.0f, 0.0f, 0.0f}, {3.0f, 100.0f, -4.0f}), 5.0f);
}

TEST(CreatureThrow, AThingThatStaysPutIsAimedAtItsMiddle)
{
	// From the original's throw at a thing: where the creature stands, the thing's place, the ground there and its
	// height; the point it throws at and the time the throw takes
	struct Case
	{
		std::array<uint32_t, 3> creature;
		std::array<uint32_t, 2> place;
		uint32_t ground;
		uint32_t height;
		std::array<uint32_t, 3> point;
		uint32_t seconds;
	};
	constexpr std::array<Case, 5> k_Cases {{
	    {{0x44ED1000, 0x41EBD70A, 0x451D80F6},
	     {0x44EB0000, 0x451C4000},
	     0x41FA0000,
	     0x40C00000,
	     {0x44EB0000, 0x42090000, 0x451C4000},
	     0x3FD0477E},
	    {{0x44CA653E, 0x443C8FBC, 0x454B6AC2},
	     {0x44CD162C, 0x4546615F},
	     0x413998B8,
	     0x41A45985,
	     {0x44CD162C, 0x41AEF91E, 0x4546615F},
	     0x403A9F25},
	    {{0x433B7A71, 0x450783A8, 0x43AEA379},
	     {0x43E839AB, 0x453055CA},
	     0x42A9CEC4,
	     0x4204A484,
	     {0x43E839AB, 0x42CAF7E5, 0x453055CA},
	     0x417EC89E},
	    {{0x441AC0A1, 0x448B863C, 0x454412A8},
	     {0x44265160, 0x454032CA},
	     0x43434043,
	     0x4015C2D2,
	     {0x44265160, 0x43446BC9, 0x454032CA},
	     0x4033B649},
	    {{0x45783E5C, 0x455A6F8E, 0x44988F71},
	     {0x4579303E, 0x44B4B215},
	     0x4311E399,
	     0x413DFA07,
	     {0x4579303E, 0x4317D369, 0x44B4B215},
	     0x409973F0},
	}};
	for (const auto& c : k_Cases)
	{
		const auto creature = Bits(c.creature[0], c.creature[1], c.creature[2]);
		const glm::vec3 place {Bits(c.place[0]), 0.0f, Bits(c.place[1])};
		const auto aim = creature_throw::AimAtStillThing(creature, place, Bits(c.ground), Bits(c.height));
		EXPECT_EQ(BitsOf(aim.point.x), c.point[0]);
		EXPECT_EQ(BitsOf(aim.point.y), c.point[1]);
		EXPECT_EQ(BitsOf(aim.point.z), c.point[2]);
		EXPECT_EQ(BitsOf(aim.seconds), c.seconds);
	}
	// The middle is half its height above the ground where it is, whatever height its place was given
	const auto middle = creature_throw::MiddleOf({10.0f, 99.0f, -4.0f}, 2.0f, 7.0f);
	EXPECT_EQ(middle, glm::vec3(10.0f, 5.5f, -4.0f));
}

TEST(CreatureThrow, AMovingThingIsAimedAtWhereItWillBeAfterTheSecondTime)
{
	// A thing going 10 units a second along x: the first time is to where it is, the second to where it will be after
	// the first, and it is aimed at where it will be after the second
	const glm::vec3 creature {0.0f, 0.0f, 0.0f};
	const glm::vec3 thing {100.0f, 0.0f, 0.0f};
	std::vector<float> asked;
	const auto aim = creature_throw::AimAhead(creature, thing, [&](float seconds) {
		asked.push_back(seconds);
		return thing + glm::vec3(10.0f * seconds, 0.0f, 0.0f);
	});
	ASSERT_EQ(asked.size(), 2u);
	const auto first = creature_throw::ThrowFlightTime(100.0f);
	EXPECT_EQ(asked[0], first);
	const auto second = creature_throw::ThrowFlightTime(100.0f + (10.0f * first));
	EXPECT_EQ(asked[1], second);
	EXPECT_EQ(aim.seconds, second);
	EXPECT_EQ(aim.point, thing + glm::vec3(10.0f * second, 0.0f, 0.0f));
}

TEST(CreatureThrow, AWalkingVillagerIsLedByItsStepForEachWholeTurn)
{
	// A step of a whole cell a turn is 10 metres a turn. 0.7 seconds is 7 whole turns at 10 a second: in single
	// precision 0.7 times 10 rounds up to 7, where in double it would truncate to 6.
	const glm::vec3 middle {100.0f, 5.0f, 200.0f};
	const auto led = creature_throw::VillagerAfter(middle, {65536, -32768}, false, 10, 0.7f);
	EXPECT_EQ(led, glm::vec3(170.0f, 5.0f, 165.0f));
	// Less than a whole turn leads it nowhere, and a villager whose top state is a final one is not led
	EXPECT_EQ(creature_throw::VillagerAfter(middle, {65536, 0}, false, 10, 0.09f), middle);
	EXPECT_EQ(creature_throw::VillagerAfter(middle, {65536, 0}, true, 10, 3.0f), middle);
	// At 5 turns a second (200 ms a turn), 3 seconds is 15 turns
	EXPECT_EQ(creature_throw::VillagerAfter(middle, {6554, 0}, false, 5, 3.0f).x,
	          100.0f + (15.0f * ((6554.0f * 10.0f) * (1.0f / 65536.0f))));
}

TEST(CreatureThrow, AVillagerIsLedAsTheGameLeadsIt)
{
	// From the original: a villager's place in whole map units, its step, its height, the ground there, whether its top
	// state is a final one, the time, and where it is aimed at (at 10 turns a second)
	struct Case
	{
		int32_t mapX;
		int32_t mapZ;
		glm::ivec2 step;
		uint32_t height;
		uint32_t ground;
		bool topFinal;
		uint32_t seconds;
		std::array<uint32_t, 3> point;
	};
	const std::array<Case, 8> k_Cases {{
	    {10966024, 5161658, {234, 2332}, 0x3F9EE73F, 0x437661DE, false, 0x3F333333, {0x44D1310A, 0x437700C5, 0x4445863B}},
	    {19655120, 2046120, {1156, -1242}, 0x3F97FF4A, 0x430217FD, false, 0x4039999A, {0x453BC3FA, 0x4302AFFC, 0x43995BCE}},
	    {3143823, 18590077, {477, -2516}, 0x40A44BDD, 0x42148FF6, false, 0x3FC94670, {0x43F0668A, 0x421ED4B4, 0x4530EDCA}},
	    {21153165, 19662129, {-2494, 1727}, 0x407B5F8E, 0x416E0759, true, 0x3FC7B388, {0x4549BB78, 0x4186B9A5, 0x453B833F}},
	    {18778574, 28905421, {-1910, -628}, 0x40461FDF, 0x432234AD, false, 0x40637825, {0x453272EC, 0x4323C0ED, 0x4589BA1B}},
	    {18899114, 27484656, {2586, -1520}, 0x3FC1F4A6, 0x432B5C7F, false, 0x3FAF7880, {0x45348E7D, 0x432C1E74, 0x4582F67C}},
	    {3369227, 18479254, {2833, -2486}, 0x4074990D, 0x4339B3F0, false, 0x40484AC1, {0x4403E042, 0x433B9D22, 0x452F7F38}},
	    {17941570, 14447616, {-427, 814}, 0x407B613A, 0x4307F48F, true, 0x40008E15, {0x452B1AA9, 0x4309EB51, 0x4509C880}},
	}};
	for (const auto& c : k_Cases)
	{
		const glm::vec3 place {map_coords::ToMetres(c.mapX), 0.0f, map_coords::ToMetres(c.mapZ)};
		const auto middle = creature_throw::MiddleOf(place, Bits(c.ground), Bits(c.height));
		const auto led = creature_throw::VillagerAfter(middle, c.step, c.topFinal, 10, Bits(c.seconds));
		EXPECT_EQ(BitsOf(led.x), c.point[0]) << c.mapX;
		EXPECT_EQ(BitsOf(led.y), c.point[1]) << c.mapX;
		EXPECT_EQ(BitsOf(led.z), c.point[2]) << c.mapX;
	}
}

TEST(CreatureThrow, ACreatureIsAimedWhereItsWalkTakesItAtItsHeightAboveTheGround)
{
	// Heading 0 faces along -z: 10 units a second for 2 seconds is 20 units on. The ground is asked for there, and
	// its height is added to it.
	std::vector<glm::vec2> asked;
	const auto groundAt = [&asked](glm::vec2 at) {
		asked.push_back(at);
		return 3.0f;
	};
	const auto ahead = creature_throw::CreatureAfter({100.0f, 50.0f, 200.0f}, 0.0f, 10.0f, 30.0f, 2.0f, groundAt);
	EXPECT_EQ(ahead, glm::vec3(100.0f, 33.0f, 180.0f));
	ASSERT_EQ(asked.size(), 1u);
	EXPECT_EQ(asked[0], glm::vec2(100.0f, 180.0f));
	// The ground is asked for at the point truncated to whole map units, while the aim keeps the point itself
	const glm::vec3 body {100.00007f, 0.0f, 50.00003f};
	const auto still = creature_throw::CreatureAfter(body, 1.0f, 0.0f, 15.0f, 1.0f, groundAt);
	EXPECT_EQ(still, glm::vec3(body.x, 18.0f, body.z));
	ASSERT_EQ(asked.size(), 2u);
	EXPECT_EQ(asked[1], glm::vec2(map_coords::Quantise(body.x), map_coords::Quantise(body.z)));
}

TEST(CreatureThrow, ACreatureIsAimedAsTheGameAimsAtIt)
{
	// From the original: the body's place, the game's angle for its heading, its speed, its size, the ground there,
	// the time, and where it is aimed at
	struct Case
	{
		std::array<uint32_t, 3> body;
		uint32_t angle;
		uint32_t speed;
		uint32_t size;
		uint32_t ground;
		uint32_t seconds;
		std::array<uint32_t, 3> point;
	};
	constexpr std::array<Case, 8> k_Cases {{
	    {{0x4547E148, 0x4351B2C6, 0x44837F3F},
	     0x3EEF6B92,
	     0x4155E085,
	     0x403FEEC2,
	     0x42ACC33E,
	     0x40BC6247,
	     {0x454A18D5, 0x43035D94, 0x44756ECD}},
	    {{0x440C1D38, 0x42FADFAA, 0x453ECD98},
	     0xC00BF1F9,
	     0x00000000,
	     0x40770191,
	     0x41BA4A06,
	     0x405EC93A,
	     {0x440C1D38, 0x42A25B3E, 0x453ECD98}},
	    {{0x4546977A, 0x43758189, 0x44B24F45},
	     0xBF70FCB8,
	     0x414AB24F,
	     0x3F0DEEA9,
	     0x41E0A161,
	     0x3FEB5903,
	     {0x45456A36, 0x421194A0, 0x44B09879}},
	    {{0x4530276D, 0x419BFFFC, 0x45387858},
	     0xBF991F6A,
	     0x416A8E0F,
	     0x3FAD2F38,
	     0x42E77991,
	     0x40838FF0,
	     {0x452CA5F1, 0x43080851, 0x45371790}},
	    {{0x433BFED4, 0x430A8233, 0x443CD8E0},
	     0xC019F98C,
	     0x3FF8E18D,
	     0x3F474FA2,
	     0x429491A3,
	     0x4021D186,
	     {0x4338B22D, 0x42ABECF8, 0x443DC221}},
	    {{0x455AA8BB, 0x41C16527, 0x44E77A96},
	     0x3E9F0C30,
	     0x41B124AB,
	     0x405FCAA1,
	     0x42A70D7A,
	     0x402AB33E,
	     {0x455BC992, 0x4307FA3B, 0x44E07324}},
	    {{0x44BB66A4, 0x4384A101, 0x456FB26D},
	     0xC00C5F77,
	     0x00000000,
	     0x402F2302,
	     0x40679C5A,
	     0x40A13104,
	     {0x44BB66A4, 0x4232AA98, 0x456FB26D}},
	    {{0x444AC8C8, 0x42A9288F, 0x442708D4},
	     0x3E5E8EB4,
	     0x41770BE6,
	     0x4074E4C2,
	     0x434F25EA,
	     0x404F4054,
	     {0x444D7AD2, 0x438445C4, 0x441AD411}},
	}};
	for (const auto& c : k_Cases)
	{
		// The game turns its body the other way round: its angle is the heading's opposite
		const float heading = -Bits(c.angle);
		const float height = Bits(c.size) * creature_throw::k_HeightAtSizeOne;
		const auto ahead = creature_throw::CreatureAfter(Bits(c.body[0], c.body[1], c.body[2]), heading, Bits(c.speed), height,
		                                                 Bits(c.seconds), [&c](glm::vec2) { return Bits(c.ground); });
		EXPECT_EQ(BitsOf(ahead.x), c.point[0]) << c.angle;
		EXPECT_EQ(BitsOf(ahead.y), c.point[1]) << c.angle;
		EXPECT_EQ(BitsOf(ahead.z), c.point[2]) << c.angle;
	}
}

TEST(CreatureThrow, TheLaunchVelocityIsTheGames)
{
	// From the original's release: the target, where the hand lets go, the time, and the velocity. Six of the eight
	// differ in a last bit from dividing by the time with the fall worked out the other way round.
	struct Case
	{
		std::array<uint32_t, 3> target;
		std::array<uint32_t, 3> release;
		uint32_t seconds;
		std::array<uint32_t, 3> velocity;
	};
	constexpr std::array<Case, 8> k_Cases {{
	    {{0x42DC0000, 0x40A00000, 0x41A00000},
	     {0x41200000, 0x41F00000, 0x41A00000},
	     0x404C5616,
	     {0x41FA913F, 0x40FA913F, 0x00000000}},
	    {{0x452F9CB5, 0x428721BB, 0x446237AA},
	     {0x451D0684, 0x444283F9, 0x442FBF8B},
	     0x4120CFB6,
	     {0x41ECB58E, 0xC1AB23A5, 0x41A0AFC9}},
	    {{0x4505A082, 0x42A19732, 0x45001555},
	     {0x44EFA68B, 0x43E1ECB5, 0x44EE12A0},
	     0x415C1BED,
	     {0x41806AEB, 0x422204F1, 0x41285AB4}},
	    {{0x4291B5B7, 0xC228FCFE, 0x44F913AE},
	     {0x43401825, 0x450E2B0D, 0x44DDA98F},
	     0x411FB73A,
	     {0xC13F1F64, 0xC33724B0, 0x41AFC3EC}},
	    {{0x45169218, 0x4242F6A2, 0x45112EF0},
	     {0x4506C78C, 0x4524C704, 0x4505E637},
	     0x41172A30,
	     {0x41D5F0F1, 0xC3638D7F, 0x4198E0C9}},
	    {{0x4536E721, 0x4345BF56, 0x4534999E},
	     {0x452989AF, 0x44FFBE49, 0x452E2D64},
	     0x40832A21,
	     {0x4250AE5B, 0xC3D76602, 0x41C891D1}},
	    {{0x4523F609, 0x4300744A, 0x451ECC35},
	     {0x45215E9A, 0x4534DE62, 0x4529A168},
	     0x41866787,
	     {0x401DF488, 0xC2A46539, 0xC12510E3}},
	    {{0x43B5AE95, 0x434328B0, 0x44790756},
	     {0x4306D21B, 0x4506B525, 0x44783732},
	     0x419FC5FB,
	     {0x4137180B, 0xBE3E9382, 0x3E26BFAB}},
	}};
	for (const auto& c : k_Cases)
	{
		const auto velocity = creature_throw::ThrowVelocity(Bits(c.target[0], c.target[1], c.target[2]),
		                                                    Bits(c.release[0], c.release[1], c.release[2]), Bits(c.seconds));
		EXPECT_EQ(BitsOf(velocity.x), c.velocity[0]);
		EXPECT_EQ(BitsOf(velocity.y), c.velocity[1]);
		EXPECT_EQ(BitsOf(velocity.z), c.velocity[2]);
	}
}

TEST(CreatureThrow, ToThrowIntoSomethingItBacksAwayWalksUpOrStaysPut)
{
	using Kind = creature_throw::ThrowStand::Kind;
	// Height 10, keeping 10, a radius of 5: no nearer than 35 and no further than 35, then 35.01
	const glm::vec2 target {100.0f, 0.0f};
	const auto near = creature_throw::WhereToThrowFrom({90.0f, 0.0f}, target, 10.0f, 5.0f, 10.0f);
	ASSERT_TRUE(near.has_value());
	EXPECT_EQ(near->kind, Kind::BackOff);
	EXPECT_NEAR(near->point.x, 65.0f, k_Tolerance);
	EXPECT_NEAR(near->point.y, 0.0f, k_Tolerance);
	EXPECT_FLOAT_EQ(near->minDistance, 0.0f);
	EXPECT_FLOAT_EQ(near->maxDistance, 20.0f);
	const auto far = creature_throw::WhereToThrowFrom({0.0f, 0.0f}, target, 10.0f, 5.0f, 10.0f);
	ASSERT_TRUE(far.has_value());
	EXPECT_EQ(far->kind, Kind::WalkUp);
	EXPECT_EQ(far->point, target);
	EXPECT_FLOAT_EQ(far->minDistance, 35.0f);
	EXPECT_NEAR(far->maxDistance, 35.01f, k_Tolerance);
	// A small thing leaves a ring to stand in: 30 to 35 away
	const auto ring = creature_throw::WhereToThrowFrom({0.0f, 0.0f}, target, 10.0f, 0.0f, 10.0f);
	ASSERT_TRUE(ring.has_value());
	EXPECT_FLOAT_EQ(ring->minDistance, 30.0f);
	EXPECT_FLOAT_EQ(ring->maxDistance, 35.0f);
	const auto there = creature_throw::WhereToThrowFrom({68.0f, 0.0f}, target, 10.0f, 0.0f, 10.0f);
	ASSERT_TRUE(there.has_value());
	EXPECT_EQ(there->kind, Kind::There);
	// Right on top of it there is no way to back away
	EXPECT_FALSE(creature_throw::WhereToThrowFrom(target, target, 10.0f, 0.0f, 10.0f).has_value());
}

TEST(CreatureIdleMind, GoingWhereItCanThrowFromIsOverseenAndItsEffectTakesPlaceWhenDone)
{
	using namespace creature_mind;
	IdleMind mind;
	Plan(mind, Activity::Planned,
	     {{.kind = Step::Kind::Move,
	       .movement = {.kind = Movement::Kind::ToThrowPosition, .object = 7u, .maxDistance = 12.0f},
	       .effect = Effect::Completed}});
	Senses senses {.seconds = 0.1f};
	auto commands = Think(mind, senses, First);
	ASSERT_TRUE(commands.move.has_value());
	EXPECT_EQ(commands.move->kind, Movement::Kind::ToThrowPosition);
	senses.subMove = SubMove::Done;
	commands = Think(mind, senses, First);
	EXPECT_EQ(commands.effect, Effect::Completed);
	EXPECT_EQ(mind.step, 1u);
}

TEST(CreatureThrow, TheLaunchVelocityLandsOnTheTarget)
{
	const glm::vec3 release {1.0f, 12.0f, -3.0f};
	const glm::vec3 target {20.0f, 0.0f, -60.0f};
	const auto seconds = creature_throw::ThrowFlightTime(creature_throw::GroundDistance(release, target));
	EXPECT_NEAR(seconds * seconds * creature_throw::k_Gravity, glm::distance(glm::vec2(1.0f, -3.0f), glm::vec2(20.0f, -60.0f)),
	            1e-3f);
	const auto velocity = creature_throw::ThrowVelocity(target, release, seconds);
	// Gravity's half g t squared is made up in the upward speed
	EXPECT_NEAR(velocity.y, (target.y - release.y) / seconds + 0.5f * creature_throw::k_Gravity * seconds, k_Tolerance);
	// On the curve of a thing falling freely, it is at the target at that time
	const auto landed =
	    release + (velocity * seconds) + glm::vec3(0.0f, -0.5f * creature_throw::k_Gravity * seconds * seconds, 0.0f);
	EXPECT_NEAR(landed.x, target.x, 1e-3f);
	EXPECT_NEAR(landed.y, target.y, 1e-3f);
	EXPECT_NEAR(landed.z, target.z, 1e-3f);
}

TEST(CreatureThrow, ItThrowsAtNothingTooClose)
{
	// Two thirds of its height, 15 units at size 1, as tall as a creature is everywhere
	EXPECT_EQ(creature_throw::k_HeightAtSizeOne, ecs::object::k_CreatureHeightPerScale);
	EXPECT_FALSE(creature_throw::FarEnoughToThrow(9.8f, 1.0f));
	EXPECT_TRUE(creature_throw::FarEnoughToThrow(10.0f, 1.0f));
	EXPECT_FALSE(creature_throw::FarEnoughToThrow(19.0f, 2.0f));
}

TEST(CreatureThrow, HighTargetsBlendInTheHighThrow)
{
	EXPECT_FLOAT_EQ(creature_throw::HighThrowWeight(0.5f, 0.5f, 1.5f), 0.0f);
	EXPECT_FLOAT_EQ(creature_throw::HighThrowWeight(1.0f, 0.5f, 1.5f), 0.5f);
	EXPECT_FLOAT_EQ(creature_throw::HighThrowWeight(3.0f, 0.5f, 1.5f), 1.0f);
	EXPECT_FLOAT_EQ(creature_throw::HighThrowWeight(-2.0f, 0.5f, 1.5f), 0.0f);
	EXPECT_FLOAT_EQ(creature_throw::HighThrowWeight(1.0f, 0.5f, 0.5f), 0.0f);
}

TEST(CreatureThrow, TossingKeepsSomeOfTheHandsSpeed)
{
	// The hand moved 1 unit ahead in the last 100 ms: 10 units a second
	const auto hand = creature_throw::HandVelocity(glm::vec3(0.0f, 0.0f, -2.0f), glm::vec3(0.0f, 0.0f, -1.0f), 100.0f);
	EXPECT_NEAR(hand.z, -10.0f, k_Tolerance);
	const auto plain = creature_throw::TossVelocity(glm::vec3(3.0f, 1.0f, -10.0f), false, glm::mat3(1.0f), 0.6f);
	EXPECT_NEAR(plain.x, 1.8f, k_Tolerance);
	EXPECT_NEAR(plain.z, -6.0f, k_Tolerance);
	// Tossed with the other hand it goes the other way sideways; turned with the creature
	const auto mirrored = creature_throw::TossVelocity(glm::vec3(3.0f, 1.0f, -10.0f), true, glm::mat3(1.0f), 0.6f);
	EXPECT_NEAR(mirrored.x, -1.8f, k_Tolerance);
	const auto turned = creature_throw::TossVelocity(glm::vec3(0.0f, 0.0f, -10.0f), false,
	                                                 glm::mat3(glm::eulerAngleY(glm::half_pi<float>())), 1.0f);
	EXPECT_NEAR(turned.x, -10.0f, k_Tolerance);
	EXPECT_NEAR(turned.z, 0.0f, k_Tolerance);
}
