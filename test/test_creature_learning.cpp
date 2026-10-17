/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <functional>
#include <limits>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureEating.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureTownCompassion.h"
#include "Creature/CreatureWatching.h"
#include "Creature/PerceivedDesires.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Systems/Implementations/CreatureMindSystem.h"
#include "ECS/Systems/Implementations/CreatureMindSystemDetail.h"
#include "Enums.h"
#include "Locator.h"
#include "creature/CreatureSystemWorld.h"
#include "support/CreatureFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using creature_desires::Desire;
using creature_tree::Attribute;
using namespace openblack::creature_perceived_desires;
using openblack::ecs::components::CreatureLocomotion;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::Transform;
using openblack::test::creature_loop_fakes::CallLog;
using openblack::test::creature_loop_fakes::FakeLeash;
using openblack::test::creature_loop_fakes::FakeMind;

namespace
{
creature_tree::Belief Thing(uint32_t type, uint32_t animate)
{
	creature_tree::Belief belief {.type = type};
	belief.Set(Attribute::Type, type);
	belief.Set(Attribute::Animate, animate);
	belief.Set(Attribute::Allegiance, 1);
	return belief;
}

constexpr uint32_t k_Villager = creature_tree::belief_types::k_Villager;
constexpr uint32_t k_Food = creature_tree::belief_types::k_Other;
const std::array k_HungerAttributes {Attribute::Allegiance, Attribute::Animate, Attribute::Type};

creature_desires::Desires TwoDesires()
{
	creature_desires::Desires desires;
	for (auto& desire : desires.desires)
	{
		desire.activated = false;
	}
	auto& hunger = desires[Desire::Hunger];
	hunger.activated = true;
	hunger.value = 0.5f;
	hunger.max = 2.0f;
	hunger.decay = 0.9f;
	hunger.increaseSeconds = 20.0f;
	hunger.sources = {{.type = 14, .value = 0.8f, .threshold = 0.5f}, {.type = 15, .value = 0.1f, .threshold = 0.5f}};
	auto& anger = desires[Desire::Anger];
	anger.activated = true;
	anger.value = 0.2f;
	anger.max = 0.8f;
	anger.increaseSeconds = 5.0f;
	return desires;
}
} // namespace

// Decision trees

TEST(CreatureDecisionTree, BucketsFeedbackByTheFirstStepWithinAQuarter)
{
	EXPECT_EQ(creature_tree::BucketOf(-1.0f), 0u);
	EXPECT_EQ(creature_tree::BucketOf(1.0f), 9u);
	EXPECT_EQ(creature_tree::BucketOf(0.0f), 4u);
	EXPECT_EQ(creature_tree::BucketOf(0.5f), 7u);
}

TEST(CreatureDecisionTree, EntropyOfAgreeingExamplesIsZero)
{
	const std::vector<creature_tree::Episode> same {{.belief = Thing(k_Food, 0), .feedback = 1.0f},
	                                                {.belief = Thing(k_Food, 0), .feedback = 1.0f}};
	EXPECT_FLOAT_EQ(creature_tree::Entropy(same), 0.0f);
	const std::vector<creature_tree::Episode> mixed {{.belief = Thing(k_Food, 0), .feedback = 1.0f},
	                                                 {.belief = Thing(k_Villager, 1), .feedback = -1.0f}};
	// One bit for the signs, one for the steps
	EXPECT_NEAR(creature_tree::Entropy(mixed), 1.0f, 1e-5f);
}

TEST(CreatureDecisionTree, GainPicksTheAttributeThatSeparatesFeedback)
{
	std::vector<creature_tree::Episode> episodes;
	for (int i = 0; i < 4; ++i)
	{
		episodes.push_back({.belief = Thing(k_Food, 0), .feedback = -1.0f});
		episodes.push_back({.belief = Thing(k_Villager, 1), .feedback = 1.0f});
	}
	EXPECT_GT(creature_tree::Gain(episodes, Attribute::Type), 0.0f);
	EXPECT_FLOAT_EQ(creature_tree::Gain(episodes, Attribute::Allegiance), 0.0f);
	// An example without the attribute gives no gain
	episodes.push_back({.belief = {.type = k_Food}, .feedback = 1.0f});
	EXPECT_FLOAT_EQ(creature_tree::Gain(episodes, Attribute::Type), 0.0f);
}

TEST(CreatureDecisionTree, BuildsAndEvaluates)
{
	std::vector<creature_tree::Episode> episodes {{.belief = Thing(k_Food, 0), .feedback = -1.0f},
	                                              {.belief = Thing(k_Villager, 1), .feedback = 1.0f},
	                                              {.belief = Thing(k_Food, 0), .feedback = -1.0f}};
	const auto tree = creature_tree::Build(episodes, k_HungerAttributes);
	ASSERT_FALSE(tree.nodes.empty());
	EXPECT_TRUE(tree.nodes[0].test.has_value());
	EXPECT_FLOAT_EQ(creature_tree::Evaluate(tree, Thing(k_Food, 0)), -1.0f);
	EXPECT_FLOAT_EQ(creature_tree::Evaluate(tree, Thing(k_Villager, 1)), 0.8f);
	// A thing whose values no example had is neither good nor bad
	EXPECT_FLOAT_EQ(creature_tree::Evaluate(tree, Thing(creature_tree::belief_types::k_Tree, 2)), 0.0f);
	EXPECT_FALSE(creature_tree::Describe(tree).empty());
}

TEST(CreatureDecisionTree, EmptyTreeKnowsNothing)
{
	const auto tree = creature_tree::Build({}, k_HungerAttributes);
	EXPECT_FLOAT_EQ(creature_tree::Evaluate(tree, Thing(k_Food, 0)), 0.0f);
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(0.0f), 0.1f);
}

TEST(CreatureDecisionTree, UsefulnessFromUtility)
{
	// capped at an eighth, the game's 0.1 x 1.25
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(1.0f), 0.125f);
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(0.5f), 0.125f);
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(0.02f), 0.9f * 0.02f + 0.1f);
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(-1.0f), 0.0f);
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(-0.5f), 0.05f);
	// and never below nothing, nor no number
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(-2.0f), 0.0f);
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(std::numeric_limits<float>::quiet_NaN()), 0.0f);
}

TEST(CreatureDecisionTree, KeepsTheNewestSixteenExamples)
{
	std::vector<creature_tree::Episode> episodes;
	for (int i = 0; i < 20; ++i)
	{
		creature_tree::AddEpisode(episodes, {.belief = Thing(k_Food, 0), .feedback = static_cast<float>(i) / 20.0f});
	}
	ASSERT_EQ(episodes.size(), creature_tree::k_MaxEpisodes);
	EXPECT_FLOAT_EQ(episodes.back().feedback, 19.0f / 20.0f);
	EXPECT_FLOAT_EQ(episodes.front().feedback, 4.0f / 20.0f);
}

TEST(CreatureDecisionTree, SlotsFollowTheKindOfThing)
{
	EXPECT_EQ(creature_tree::SlotsFor(creature_tree::belief_types::k_Villager).size(), 11u);
	EXPECT_EQ(creature_tree::SlotsFor(creature_tree::belief_types::k_Forest).size(), 8u);
	EXPECT_EQ(creature_tree::SlotsFor(creature_tree::belief_types::k_Animal).size(), 7u);
	const std::vector<uint32_t> slots {1, 0, 1, 7, 0, 0, 6, 1, 16, 1, 0};
	const auto belief = creature_tree::FromSlots(k_Villager, slots);
	EXPECT_EQ(belief.Value(Attribute::Sex), 1u);
	EXPECT_EQ(belief.Value(Attribute::VillagerJob), 16u);
	EXPECT_EQ(creature_tree::ToSlots(belief), slots);
}

// The planner

TEST(CreaturePlanner, DistancePriority)
{
	EXPECT_FLOAT_EQ(creature_planner::DistancePriority(true, 500.0f, 0.4f), 1.6f);
	EXPECT_FLOAT_EQ(creature_planner::DistancePriority(false, 0.0f, 0.4f), 1.0f);
	EXPECT_FLOAT_EQ(creature_planner::DistancePriority(false, 100.0f, 0.4f), 0.8f);
	EXPECT_FLOAT_EQ(creature_planner::DistancePriority(false, 900.0f, 0.4f), 0.6f);
}

TEST(CreaturePlanner, ActionPriorityAndNovelty)
{
	EXPECT_FLOAT_EQ(creature_planner::ActionPriority(-1.0f), 0.5f);
	EXPECT_FLOAT_EQ(creature_planner::ActionPriority(1.0f), 0.505f);
	EXPECT_FLOAT_EQ(creature_planner::ActionPriority(1.0f, true), 0.0f);
	EXPECT_FLOAT_EQ(creature_planner::Novelty(0, false), 0.0f);
	EXPECT_FLOAT_EQ(creature_planner::Novelty(36000, false), 0.01f);
	EXPECT_FLOAT_EQ(creature_planner::Novelty(900, false), 0.01f);
	EXPECT_NEAR(creature_planner::Novelty(36000, true), 0.01f, 1e-6f);
	EXPECT_NEAR(creature_planner::Novelty(900, true), 4.0f * 0.025f * 0.025f, 1e-6f);
}

TEST(CreaturePlanner, PriorityAndSwitching)
{
	EXPECT_FLOAT_EQ(creature_planner::Priority(1.0f, std::nullopt, 0.5f, 0.1f, std::nullopt), 1.0f);
	EXPECT_FLOAT_EQ(creature_planner::Priority(1.0f, 1.0f, 0.5f, 0.1f, std::nullopt), 10.0f);
	// The action's priority is capped, not floored: an action worth nothing gives a plan worth nothing
	EXPECT_FLOAT_EQ(creature_planner::Priority(1.0f, std::nullopt, 0.0f, 0.1f, std::nullopt), 0.0f);
	EXPECT_TRUE(creature_planner::ShouldSwitch(21.0f, 10.0f));
	EXPECT_FALSE(creature_planner::ShouldSwitch(20.0f, 10.0f));
	EXPECT_TRUE(creature_planner::ShouldSwitch(1.0f, 0.0f));
}

// The plan's priority to the bit, as openblack works it out today, so that each change to it shows its own numbers
TEST(CreaturePlanner, PlanPriorityToTheBit)
{
	// At most a hundredth of the action's priority counts, and the product goes action, object, desire with goal, scale,
	// instrument
	EXPECT_EQ(creature_planner::Priority(1.0f, 0.1f, 0.5f, 0.1f, std::nullopt), 1.0f);
	EXPECT_EQ(creature_planner::Priority(0.75f, 0.3f, 0.5025f, 0.1f, std::nullopt), 0x1.200002p+1f);
	EXPECT_EQ(creature_planner::Priority(0.4954351f, 0.44949105f, 0.007887233f, 0.1f, 0.65159297f), 0x1.6e3bfp+3f);
	// Beyond the farthest a goal counts, the whole weight is taken off
	EXPECT_EQ(creature_planner::DistancePriority(false, 250.0f, 0.091f), 1.0f - 0.091f);
	// and a distance that is no number counts as within it, so the priority is no number either
	const auto unknown = creature_planner::DistancePriority(false, std::numeric_limits<float>::quiet_NaN(), 0.091f);
	EXPECT_NE(unknown, unknown);
	// The game's distance in metres, through the map's fixed point and its table root, is a little short of the exact
	// one, and so is the priority
	EXPECT_FLOAT_EQ(
	    creature_planner::DistancePriority(false, gutils::GetDistanceInMetres(glm::vec2(0.0f), glm::vec2(100.0f, 0.0f)), 0.4f),
	    0.7999512f);
	EXPECT_EQ(creature_planner::DistancePriority(false, 100.0f, 0.4f), 0x1.99999ap-1f);

	// A desire that doesn't look for a goal counts it as a thing nothing is known about, done to it or not
	const std::vector<creature_planner::ObjectCandidate> nearby {{.id = 1, .distance = 100.0f, .usefulness = 0.4f}};
	const std::vector<creature_planner::ActionCandidate> doneToIt {
	    {.action = 12, .needsObject = true, .opinion = -1.0f, .turnsSinceDone = 0}};
	const auto toIt = creature_planner::PlanDesire(Desire::Hunger, 1.0f, nearby, 0.4f, doneToIt);
	ASSERT_TRUE(toIt.has_value());
	EXPECT_EQ(toIt->object, 1u);
	EXPECT_EQ(toIt->goalUsefulness, 0.1f);
	EXPECT_EQ(toIt->priority, 1.0f);
	const std::vector<creature_planner::ActionCandidate> notToIt {{.action = 12, .opinion = -1.0f, .turnsSinceDone = 0}};
	const auto alone = creature_planner::PlanDesire(Desire::Hunger, 1.0f, nearby, 0.4f, notToIt);
	ASSERT_TRUE(alone.has_value());
	EXPECT_FALSE(alone->object.has_value());
	EXPECT_EQ(alone->priority, 1.0f);
	// and takes a thing worth nothing
	const std::vector<creature_planner::ObjectCandidate> useless {{.id = 1, .distance = 100.0f, .usefulness = 0.0f}};
	const auto toNothing = creature_planner::PlanDesire(Desire::Hunger, 1.0f, useless, 0.4f, doneToIt);
	ASSERT_TRUE(toNothing.has_value());
	EXPECT_EQ(toNothing->object, 1u);
	EXPECT_EQ(toNothing->priority, 1.0f);

	// One that looks for its goal counts it for its worth for the distance, whatever the action
	const creature_planner::GoalSearch searches {.searches = true};
	const auto sought = creature_planner::PlanDesire(Desire::Impress, 1.0f, nearby, 0.4f, doneToIt, searches);
	ASSERT_TRUE(sought.has_value());
	EXPECT_EQ(sought->object, 1u);
	EXPECT_EQ(sought->goalUsefulness, 0x1.47ae16p-2f);
	EXPECT_EQ(sought->priority, 0x1.99999ep+1f);
	EXPECT_EQ(creature_planner::PlanDesire(Desire::Impress, 1.0f, nearby, 0.4f, notToIt, searches)->priority, 0x1.99999ep+1f);
	// and has no plan without a goal worth more than nothing
	EXPECT_FALSE(creature_planner::PlanDesire(Desire::Impress, 1.0f, useless, 0.4f, doneToIt, searches).has_value());
	EXPECT_FALSE(creature_planner::PlanDesire(Desire::Impress, 1.0f, {}, 0.4f, notToIt, searches).has_value());
	// A goal that is itself what the plan is done to counts for its usefulness as that too
	const creature_planner::GoalSearch actsOnIt {.searches = true, .actsOnGoal = true};
	EXPECT_EQ(creature_planner::PlanDesire(Desire::Anger, 1.0f, nearby, 0.4f, doneToIt, actsOnIt)->priority, 0x1.99999ep+3f);
	// A goal found beforehand counts for its own worth
	const creature_planner::GoalSearch found {.searches = true, .foundWorth = 0.25f};
	EXPECT_EQ(creature_planner::PlanDesire(Desire::Compassion, 1.0f, nearby, 0.4f, doneToIt, found)->priority, 0x1.400002p+1f);
}

TEST(CreaturePlanner, PlansTheMostUsefulGoal)
{
	const std::vector<creature_planner::ObjectCandidate> objects {
	    {.id = 1, .distance = 10.0f, .usefulness = 0.0f},
	    {.id = 2, .distance = 150.0f, .usefulness = 0.82f},
	};
	const std::vector<creature_planner::ActionCandidate> actions {{.action = 12, .needsObject = true}};
	const creature_planner::GoalSearch searches {.searches = true};
	const auto plan = creature_planner::PlanDesire(Desire::Impress, 1.0f, objects, 0.05f, actions, searches);
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->object, 2u);
	EXPECT_EQ(plan->goalUsefulness, 0x1.941892p-1f);
	// Without anything learnt the nearer wins
	const std::vector<creature_planner::ObjectCandidate> unknown {
	    {.id = 1, .distance = 10.0f},
	    {.id = 2, .distance = 150.0f},
	};
	EXPECT_EQ(creature_planner::PlanDesire(Desire::Hunger, 1.0f, unknown, 0.05f, actions)->object, 1u);
	// An action that needs a goal can't be planned without one
	EXPECT_FALSE(creature_planner::PlanDesire(Desire::Hunger, 1.0f, {}, 0.05f, actions).has_value());
}

TEST(CreaturePlanner, WithoutAGoalTheActionComesFirstThenTheMostUsefulThing)
{
	const std::vector<creature_planner::ObjectCandidate> things {
	    {.id = 1, .distance = 5.0f, .usefulness = 0.05f},
	    {.id = 2, .distance = 150.0f, .usefulness = 0.12f},
	    {.id = 3, .distance = 300.0f, .usefulness = 0.12f},
	};
	const creature_planner::ActionCandidate liked {.action = 7, .needsObject = true, .opinion = 1.0f, .turnsSinceDone = 0};
	const creature_planner::ActionCandidate disliked {.action = 8, .needsObject = true, .opinion = -1.0f, .turnsSinceDone = 0};
	// The most wanted action has nothing to be done to, so the next is taken, done to the most useful thing however
	// far, the first of equals; what it is done to counts for its usefulness
	const std::vector<creature_planner::ActionChoice> choices {{.action = disliked, .objects = things},
	                                                           {.action = liked, .objects = {}}};
	const auto plan = creature_planner::PlanWithoutGoal(Desire::Hunger, 1.0f, choices);
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->action, 8u);
	EXPECT_EQ(plan->object, 2u);
	EXPECT_EQ(plan->priority, 0x1.333334p+0f);
	// Of actions as wanted, the later one in the list
	const std::vector<creature_planner::ActionChoice> equals {
	    {.action = liked, .objects = things},
	    {.action = {.action = 9, .needsObject = true, .opinion = 1.0f, .turnsSinceDone = 0}, .objects = things}};
	EXPECT_EQ(creature_planner::PlanWithoutGoal(Desire::Hunger, 1.0f, equals)->action, 9u);
	// An action done to nothing counts what it is done to as a thing nothing is known about
	const std::vector<creature_planner::ActionChoice> alone {
	    {.action = {.action = 10, .opinion = -1.0f, .turnsSinceDone = 0}, .objects = {}}};
	const auto byItself = creature_planner::PlanWithoutGoal(Desire::Rest, 1.0f, alone);
	ASSERT_TRUE(byItself.has_value());
	EXPECT_FALSE(byItself->object.has_value());
	EXPECT_EQ(byItself->priority, 1.0f);
	// Nothing worth more than nothing, or out of reach, to do any action to: no plan
	const std::vector<creature_planner::ObjectCandidate> worthless {{.id = 1, .usefulness = 0.0f},
	                                                                {.id = 2, .usefulness = 0.1f, .outOfReach = true}};
	const std::vector<creature_planner::ActionChoice> nothing {{.action = liked, .objects = worthless},
	                                                           {.action = disliked, .objects = worthless}};
	EXPECT_FALSE(creature_planner::PlanWithoutGoal(Desire::Hunger, 1.0f, nothing).has_value());
}

TEST(CreaturePlanner, SomeDesiresArePlannedOnlyInTheirState)
{
	const auto nan = std::numeric_limits<float>::quiet_NaN();
	using creature_planner::MayPlan;
	// Wanderlust: not once past the fifth stage and worn out to eight tenths
	EXPECT_TRUE(MayPlan(Desire::Wanderlust, {.developmentPhase = 5, .exhaustion = 1.0f}));
	EXPECT_FALSE(MayPlan(Desire::Wanderlust, {.developmentPhase = 6, .exhaustion = 0.8f}));
	EXPECT_TRUE(MayPlan(Desire::Wanderlust, {.developmentPhase = 6, .exhaustion = 0.79f}));
	EXPECT_TRUE(MayPlan(Desire::Wanderlust, {.developmentPhase = 6, .exhaustion = nan}));
	// Getting warmer at three tenths of cold or colder, getting colder at three tenths of hot or hotter
	EXPECT_TRUE(MayPlan(Desire::GetWarmer, {.warmth = -0.3f}));
	EXPECT_FALSE(MayPlan(Desire::GetWarmer, {.warmth = -0.29f}));
	EXPECT_TRUE(MayPlan(Desire::GetWarmer, {.warmth = nan}));
	EXPECT_TRUE(MayPlan(Desire::GetColder, {.warmth = 0.3f}));
	EXPECT_FALSE(MayPlan(Desire::GetColder, {.warmth = 0.29f}));
	EXPECT_FALSE(MayPlan(Desire::GetColder, {.warmth = nan}));
	// Running away from the player only when it dislikes the player
	EXPECT_FALSE(MayPlan(Desire::RunAwayFromPlayer, {.attitudeToPlayer = 0.0f}));
	EXPECT_TRUE(MayPlan(Desire::RunAwayFromPlayer, {.attitudeToPlayer = -0.01f}));
	EXPECT_TRUE(MayPlan(Desire::RunAwayFromPlayer, {.attitudeToPlayer = nan}));
	// Hanging around at home only within sixty metres of it
	EXPECT_TRUE(MayPlan(Desire::HangAroundAtHome, {.metresFromHome = 59.9f}));
	EXPECT_FALSE(MayPlan(Desire::HangAroundAtHome, {.metresFromHome = 60.0f}));
	EXPECT_TRUE(MayPlan(Desire::HangAroundAtHome, {.metresFromHome = nan}));
	// Rest, and the desires with no test of their own, always
	EXPECT_TRUE(MayPlan(Desire::Rest, {.developmentPhase = 6, .exhaustion = 1.0f, .warmth = 0.0f}));
	EXPECT_TRUE(MayPlan(Desire::Hunger, {.attitudeToPlayer = 1.0f}));
}

TEST(CreaturePlanner, OpinionBreaksTies)
{
	const std::vector<creature_planner::ActionCandidate> actions {{.action = 1, .opinion = 1.0f, .turnsSinceDone = 0},
	                                                              {.action = 2, .opinion = -1.0f, .turnsSinceDone = 0}};
	EXPECT_EQ(creature_planner::PlanDesire(Desire::Rest, 0.5f, {}, 0.02f, actions)->action, 1u);
}

TEST(CreaturePlanner, APassGoesOverTheDesiresActivatedAndNotHeldBack)
{
	creature_desires::Desires desires;
	for (auto& state : desires.desires)
	{
		state.activated = false;
	}
	desires[Desire::Hunger].activated = true;
	desires[Desire::Hunger].value = 0.0f;
	desires[Desire::Rest].activated = true;
	desires[Desire::Rest].suppressedTurns = 3;
	const auto listed = creature_planner::PassDesires(desires);
	// whatever its strength, but not while it is held back
	EXPECT_TRUE(listed.at(static_cast<size_t>(Desire::Hunger)));
	EXPECT_FALSE(listed.at(static_cast<size_t>(Desire::Rest)));
	EXPECT_FALSE(listed.at(static_cast<size_t>(Desire::Impress)));
}

TEST(CreaturePlanner, PlansTheEligibleDesiresInPassesTwoATurn)
{
	creature_planner::PlannerState state;
	// Before any pass the pass is over, so the first turn decides and starts one
	EXPECT_TRUE(creature_planner::PassOver(state));
	std::array<bool, creature_desires::k_DesireCount> eligible {};
	eligible.at(static_cast<size_t>(Desire::Hunger)) = true;
	eligible.at(static_cast<size_t>(Desire::Tiredness)) = true;
	eligible.at(static_cast<size_t>(Desire::Rest)) = true;
	creature_planner::StartPass(state, eligible);
	const auto first = creature_planner::NextGoals(state);
	ASSERT_EQ(first.size(), 2u);
	EXPECT_EQ(first[0], Desire::Hunger);
	EXPECT_EQ(first[1], Desire::Tiredness);
	EXPECT_FALSE(creature_planner::PassOver(state));
	const auto second = creature_planner::NextGoals(state);
	ASSERT_EQ(second.size(), 1u);
	EXPECT_EQ(second[0], Desire::Rest);
	EXPECT_TRUE(creature_planner::PassOver(state));
	EXPECT_TRUE(creature_planner::NextGoals(state).empty());
}

TEST(CreaturePlanner, NothingReplacesAForcedPlan)
{
	creature_planner::PlannerState state;
	state.best.at(0) = creature_planner::Plan {.desire = Desire::Impress, .action = 1, .priority = 1.0e30f};
	state.current = creature_planner::Plan {.desire = Desire::Compassion,
	                                        .action = 2,
	                                        .goalUsefulness = creature_planner::k_ForcedScore,
	                                        .actionPriority = creature_planner::k_ForcedScore,
	                                        .priority = creature_planner::k_ForcedScore};
	EXPECT_FALSE(creature_planner::Choose(state).has_value());
}

TEST(CreaturePlanner, ChoosesOnlyWhatIsPressingEnough)
{
	creature_planner::PlannerState state;
	// Nothing is chosen at no priority, and anything above it is with nothing carried out
	state.best.at(0) = creature_planner::Plan {.desire = Desire::Impress, .action = 1, .priority = 0.0f};
	EXPECT_FALSE(creature_planner::Choose(state).has_value());
	state.best.at(0)->priority = 0.001f;
	EXPECT_TRUE(creature_planner::Choose(state).has_value());
	state.best.at(0)->priority = 30.0f;
	state.current = creature_planner::Plan {.desire = Desire::Hunger, .action = 2, .priority = 20.0f};
	EXPECT_FALSE(creature_planner::Choose(state).has_value());
	state.best.at(0)->priority = 41.0f;
	EXPECT_TRUE(creature_planner::Choose(state).has_value());
}

TEST(CreaturePlanner, OfEqualPlansTheActionLaterInTheListWins)
{
	const std::vector<uint32_t> actions {28, 29, 48, 31, 29};
	const creature_planner::Plan first {.action = 48, .priority = 0.5f};
	const creature_planner::Plan later {.action = 31, .priority = 0.5f};
	EXPECT_TRUE(creature_planner::Replaces(later, first, actions));
	EXPECT_FALSE(creature_planner::Replaces(first, later, actions));
	// An action listed twice counts at its last place
	const creature_planner::Plan twice {.action = 29, .priority = 0.5f};
	EXPECT_TRUE(creature_planner::Replaces(twice, later, actions));
	// More pressing wins wherever it is in the list
	const creature_planner::Plan pressing {.action = 28, .priority = 0.6f};
	EXPECT_TRUE(creature_planner::Replaces(pressing, later, actions));
	EXPECT_FALSE(creature_planner::Replaces(later, pressing, actions));
}

TEST(CreaturePlanner, TheFirstDesireWinsATie)
{
	creature_planner::PlannerState state;
	state.best.at(3) = creature_planner::Plan {.desire = static_cast<Desire>(3), .action = 1, .priority = 0.5f};
	state.best.at(7) = creature_planner::Plan {.desire = static_cast<Desire>(7), .action = 2, .priority = 0.5f};
	ASSERT_TRUE(creature_planner::Best(state).has_value());
	EXPECT_EQ(creature_planner::Best(state)->action, 1u);
}

// Learning from feedback

TEST(CreatureLearning, LessonFactor)
{
	EXPECT_FLOAT_EQ(creature_learning::LessonFactor(2.0f, 1.0f), 0.5f);
	EXPECT_FLOAT_EQ(creature_learning::LessonFactor(2.0f, -1.0f), 1.5f);
	EXPECT_FLOAT_EQ(creature_learning::LessonFactor(200.0f, 0.0f), 1.0f);
}

TEST(CreatureLearning, DesireLessonAndDependencies)
{
	auto desires = TwoDesires();
	creature_learning::AllDesireRules rules {};
	auto& hunger = rules.at(static_cast<size_t>(Desire::Hunger));
	hunger = {.initialMax = 2.0f, .decayMin = 0.9f, .decayMax = 0.95f, .lessonDivisor = 2.0f, .learnable = true};
	creature_learning::Dependencies dependencies {};
	dependencies.at(static_cast<size_t>(Desire::Hunger)).at(static_cast<size_t>(Desire::Anger)) = -0.5f;
	creature_learning::LearnDesireLesson(desires, Desire::Hunger, 0, 1.0f, rules, dependencies);
	const auto& state = desires[Desire::Hunger];
	// Rewarded, it grows twice as fast, its source drives it sooner, it fades more slowly
	EXPECT_FLOAT_EQ(state.increaseSeconds, 10.0f);
	EXPECT_FLOAT_EQ(state.sources[0].threshold, 0.38f);
	EXPECT_FLOAT_EQ(state.sources[1].threshold, 0.5f);
	EXPECT_FLOAT_EQ(state.max, 2.0f);
	EXPECT_FLOAT_EQ(state.decay, 0.901f);
	// The opposed desire grows slower
	EXPECT_FLOAT_EQ(desires[Desire::Anger].increaseSeconds, 10.0f);
	creature_learning::LearnDesireLesson(desires, Desire::Hunger, 0, -1.0f, rules, dependencies);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].max, 1.9f);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].increaseSeconds, 15.0f);
}

TEST(CreatureLearning, OpinionsAndFeedbackStrength)
{
	EXPECT_FLOAT_EQ(creature_learning::OpinionAfter(0.0f, 1.0f), 0.8f);
	EXPECT_FLOAT_EQ(creature_learning::OpinionAfter(0.8f, -1.0f), -0.64f);
	EXPECT_FLOAT_EQ(creature_learning::Strengthened(0.2f), 0.5f);
	EXPECT_FLOAT_EQ(creature_learning::Strengthened(-0.9f), -0.9f);
}

TEST(CreatureLearning, SpreadsLessonsByDependency)
{
	creature_learning::Dependencies dependencies {};
	dependencies.at(static_cast<size_t>(Desire::Compassion)).at(static_cast<size_t>(Desire::Anger)) = -0.86f;
	dependencies.at(static_cast<size_t>(Desire::Compassion)).at(static_cast<size_t>(Desire::BeFriends)) = 0.5f;
	const auto spread = creature_learning::Spread(Desire::Compassion, 1.0f, dependencies);
	ASSERT_EQ(spread.size(), 3u);
	EXPECT_EQ(spread[0].first, Desire::Compassion);
	EXPECT_FLOAT_EQ(spread[1].second, -0.86f);
	EXPECT_FLOAT_EQ(spread[2].second, 0.5f);
}

TEST(CreatureLearning, AStrokesEatingAndForgivingAreJudgedOnTheNewestThingItDid)
{
	using creature_desires::Desire;
	// nothing done yet: no newest
	EXPECT_EQ(creature_learning::Newest({}), nullptr);
	// the newest is the last remembered, whatever is worth more credit
	std::vector<creature_learning::Context> contexts {
	    {.action = 1, .desire = Desire::RunAwayFromPlayer, .running = true},
	    {.action = 2, .desire = Desire::Hunger, .running = false, .secondsSince = 30.0f, .windowSeconds = 10.0f}};
	ASSERT_EQ(creature_learning::BestContext(contexts), 0u);
	const auto* newest = creature_learning::Newest(contexts);
	ASSERT_NE(newest, nullptr);
	EXPECT_EQ(newest->action, 2u);
	// so the stroke does not forgive the running away an older thing was
	EXPECT_FALSE(creature_learning::StrokeForgivesRunningAway(1.0f, newest));
	EXPECT_TRUE(creature_learning::StrokeForgivesRunningAway(1.0f, &contexts.front()));
	// only a stroke forgives, and only a newest thing
	EXPECT_FALSE(creature_learning::StrokeForgivesRunningAway(-1.0f, &contexts.front()));
	EXPECT_FALSE(creature_learning::StrokeForgivesRunningAway(1.0f, nullptr));
	// eating: a stroke, past the first stage, the newest thing's action allowing it
	EXPECT_TRUE(creature_learning::StrokeTeachesEating(0.5f, 1, true));
	EXPECT_FALSE(creature_learning::StrokeTeachesEating(0.5f, 0, true));
	EXPECT_FALSE(creature_learning::StrokeTeachesEating(-0.5f, 3, true));
	EXPECT_FALSE(creature_learning::StrokeTeachesEating(0.5f, 3, false));
}

TEST(CreatureLearning, FeedbackWithNothingWorthCreditingDoesNothingAndOnlyTheYoungFeelItPlainly)
{
	using creature_learning::FeedbackPath;
	constexpr uint32_t k_LessonPhase = 2;
	// nothing it did worth crediting: nothing, whatever its age
	EXPECT_EQ(creature_learning::PathOf(false, true, 0, k_LessonPhase), FeedbackPath::Nothing);
	EXPECT_EQ(creature_learning::PathOf(false, true, 5, k_LessonPhase), FeedbackPath::Nothing);
	// too young, or an action the tables do not know: the plain feedback
	EXPECT_EQ(creature_learning::PathOf(true, true, 1, k_LessonPhase), FeedbackPath::Plain);
	EXPECT_EQ(creature_learning::PathOf(true, false, 5, k_LessonPhase), FeedbackPath::Plain);
	// old enough: its lessons
	EXPECT_EQ(creature_learning::PathOf(true, true, 2, k_LessonPhase), FeedbackPath::Lessons);
	// the plain stroke stirs play, showing how it is and compassion; the slap anger and fear, by half the feedback
	const auto stroke = creature_learning::PlainOf(0.8f);
	EXPECT_EQ(stroke.sources.size(), 3u);
	EXPECT_FLOAT_EQ(stroke.amount, 0.4f);
	const auto slap = creature_learning::PlainOf(-0.6f);
	ASSERT_EQ(slap.sources.size(), 2u);
	EXPECT_EQ(slap.sources[0], creature_desires::sources::k_AngerFromDamage);
	EXPECT_EQ(slap.sources[1], creature_desires::sources::k_FearFromDamage);
	EXPECT_FLOAT_EQ(slap.amount, 0.3f);
}

TEST(CreatureLearning, APlanItCouldCarryOutHasItsThingsAsTheGamesRowsAsk)
{
	using creature_learning::PlanIsValid;
	const creature_learning::Context done {.action = 12, .desire = Desire::Hunger, .object = 5};
	EXPECT_TRUE(PlanIsValid(done, false, false));
	// Done to nothing, or no action
	EXPECT_FALSE(PlanIsValid({.action = 12, .desire = Desire::Hunger}, false, false));
	EXPECT_FALSE(PlanIsValid({.action = 0, .desire = Desire::Hunger, .object = 5}, false, false));
	// An action done with something needs what it was done with
	EXPECT_FALSE(PlanIsValid(done, false, true));
	EXPECT_TRUE(PlanIsValid({.action = 15, .desire = Desire::Hunger, .object = 5, .used = 6}, false, true));
	// Compassion, which looks for a goal, needs the town it was for
	EXPECT_FALSE(PlanIsValid({.action = 101, .desire = Desire::Compassion, .object = 5}, true, false));
	EXPECT_TRUE(PlanIsValid({.action = 101, .desire = Desire::Compassion, .object = 5, .about = 9}, true, false));
	// The game's rows done with something
	EXPECT_TRUE(creature_mind_tables::NeedsInstrument(15));
	EXPECT_TRUE(creature_mind_tables::NeedsInstrument(327));
	EXPECT_FALSE(creature_mind_tables::NeedsInstrument(12));
}

TEST(CreatureLearning, CreditGoesToRecentActions)
{
	creature_learning::Context running {.action = 1, .running = true, .windowSeconds = 20.0f};
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(running), 1.0f);
	creature_learning::Context finished {
	    .action = 2, .running = false, .secondsSince = 5.0f, .seen = true, .windowSeconds = 20.0f};
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(finished), 0.75f);
	finished.secondsSince = 25.0f;
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(finished), 0.0f);
	// Finished where the player did not see it, it counts for nothing
	finished.secondsSince = 5.0f;
	finished.seen = false;
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(finished), 0.0f);
	creature_learning::Context unlearnable {.action = 3, .learnable = false};
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(unlearnable), 0.0f);

	std::vector<creature_learning::Context> contexts;
	creature_learning::Remember(contexts, {.action = 1, .windowSeconds = 20.0f});
	creature_learning::Finished(contexts, true);
	creature_learning::Age(contexts, 1.0f);
	creature_learning::Remember(contexts, {.action = 2, .windowSeconds = 10.0f});
	creature_learning::Age(contexts, 1.0f);
	ASSERT_EQ(contexts.size(), 2u);
	EXPECT_FALSE(contexts[0].running);
	EXPECT_EQ(creature_learning::BestContext(contexts), 1u);
	creature_learning::Finished(contexts, true);
	creature_learning::Age(contexts, 9.0f);
	// The older action's longer window now gives it the bigger share
	EXPECT_EQ(creature_learning::BestContext(contexts), 0u);
	// Of two as much worth crediting, the newer
	std::vector<creature_learning::Context> equals {{.action = 1, .running = true}, {.action = 2, .running = true}};
	EXPECT_EQ(creature_learning::BestContext(equals), 1u);
	// An action that never finished is forgotten for the next
	creature_learning::Remember(contexts, {.action = 7});
	creature_learning::Remember(contexts, {.action = 8});
	ASSERT_EQ(contexts.size(), 3u);
	EXPECT_EQ(contexts.back().action, 8u);
	for (uint32_t i = 0; i < 20; ++i)
	{
		creature_learning::Remember(contexts, {.action = i});
		creature_learning::Finished(contexts, false);
	}
	EXPECT_EQ(contexts.size(), creature_learning::k_MaxContexts);
}

TEST(CreatureLearning, TheCameraLooksAtWhatIsNearAndInsideTwoThirdsOfItsFieldOfView)
{
	const glm::vec3 eye(0.0f);
	const glm::vec3 focus(0.0f, 0.0f, 10.0f);
	EXPECT_TRUE(creature_learning::LooksAt(eye, focus, glm::vec3(0.0f, 0.0f, 100.0f)));
	// Too far, behind, or outside the cone (its half angle is two thirds of 70 degrees)
	EXPECT_FALSE(creature_learning::LooksAt(eye, focus, glm::vec3(0.0f, 0.0f, 400.0f)));
	EXPECT_FALSE(creature_learning::LooksAt(eye, focus, glm::vec3(0.0f, 0.0f, -100.0f)));
	EXPECT_TRUE(creature_learning::LooksAt(eye, focus, glm::vec3(100.0f, 0.0f, 110.0f)));
	EXPECT_FALSE(creature_learning::LooksAt(eye, focus, glm::vec3(100.0f, 0.0f, 90.0f)));
}

TEST(CreatureLearning, DecidingSuppressesOpposedDesires)
{
	auto desires = TwoDesires();
	creature_learning::Dependencies dependencies {};
	dependencies.at(static_cast<size_t>(Desire::Hunger)).at(static_cast<size_t>(Desire::Anger)) = -0.5f;
	creature_learning::SuppressOpposed(desires, Desire::Hunger, dependencies, 10.0f);
	EXPECT_EQ(desires[Desire::Anger].suppressedTurns, 1200u);
	EXPECT_EQ(desires[Desire::Hunger].suppressedTurns, 0u);
	// a desire that opposes itself is held back too (the game's table opposes none to itself)
	dependencies.at(static_cast<size_t>(Desire::Hunger)).at(static_cast<size_t>(Desire::Hunger)) = -0.25f;
	creature_learning::SuppressOpposed(desires, Desire::Hunger, dependencies, 10.0f);
	EXPECT_EQ(desires[Desire::Hunger].suppressedTurns, 600u);
}

TEST(CreatureLearning, APlanIsRememberedUnlessNothingSaysWhy)
{
	using creature_learning::k_LastTellingSource;
	creature_desires::DesireState desire;
	// No sources at all: nothing says why
	EXPECT_FALSE(creature_learning::RemembersPlan(desire));
	desire.sources.push_back({.type = k_LastTellingSource + 5, .drive = 0.0f});
	desire.sources.push_back({.type = 3, .drive = 2.0f});
	// The source that drove it most says why
	EXPECT_TRUE(creature_learning::RemembersPlan(desire));
	desire.sources[1].type = k_LastTellingSource + 2;
	// Neither the most driving nor the first says why
	EXPECT_FALSE(creature_learning::RemembersPlan(desire));
	// The first source says why
	desire.sources[0].type = k_LastTellingSource;
	EXPECT_TRUE(creature_learning::RemembersPlan(desire));
	// Nothing has driven it: the first source has its say
	desire.sources[1].drive = 0.0f;
	desire.sources[0].type = 4;
	desire.sources[1].type = 2;
	EXPECT_TRUE(creature_learning::RemembersPlan(desire));
	desire.sources[0].type = k_LastTellingSource + 1;
	EXPECT_FALSE(creature_learning::RemembersPlan(desire));
}

TEST(CreatureLearning, DominanceAndActions)
{
	auto desires = TwoDesires();
	creature_learning::MakeLeastDominant(desires, Desire::Hunger, 1.3f, 0.0f);
	EXPECT_NEAR(desires[Desire::Hunger].value, 0.2f / 1.3f, 1e-6f);
	creature_learning::MakeFullyDominant(desires, Desire::Hunger);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].value, 2.0f);
	creature_learning::AfterAction(desires, Desire::Hunger, 0.3f, 0.0f);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].value, 0.6f);
}

TEST(CreatureLearning, ADesireIsSatisfiedOnceItsPlansAreCarriedOutItsWeightInWholeTimes)
{
	creature_desires::DesireState desire;
	// a weight of 1 is satisfied every time
	desire.weight = 1.0f;
	EXPECT_TRUE(creature_learning::CountCompletion(desire));
	EXPECT_EQ(desire.completions, 0u);
	EXPECT_TRUE(creature_learning::CountCompletion(desire));
	// 2, every second time, counting 1 between
	desire.weight = 2.0f;
	EXPECT_FALSE(creature_learning::CountCompletion(desire));
	EXPECT_EQ(desire.completions, 1u);
	EXPECT_TRUE(creature_learning::CountCompletion(desire));
	EXPECT_EQ(desire.completions, 0u);
	// 2.9 counts as 2
	desire.weight = 2.9f;
	EXPECT_FALSE(creature_learning::CountCompletion(desire));
	EXPECT_TRUE(creature_learning::CountCompletion(desire));
	// 8, the eighth time
	desire.weight = 8.0f;
	for (int i = 0; i < 7; ++i)
	{
		EXPECT_FALSE(creature_learning::CountCompletion(desire));
	}
	EXPECT_EQ(desire.completions, 7u);
	EXPECT_TRUE(creature_learning::CountCompletion(desire));
	EXPECT_EQ(desire.completions, 0u);
}

TEST(CreatureLearning, SatisfyingADesireEmptiesOnlyTheSourcesItSpends)
{
	creature_desires::DesireState desire;
	desire.sources = {{.type = 14, .value = 0.8f, .threshold = 0.5f, .spentWhenSatisfied = true},
	                  {.type = 20, .value = 0.6f, .threshold = 0.4f},
	                  {.type = 15, .value = 0.3f, .threshold = 0.2f, .spentWhenSatisfied = true}};
	creature_learning::ClearSourcesAfterSatisfying(desire);
	EXPECT_EQ(desire.sources[0].value, 0.0f);
	EXPECT_EQ(desire.sources[1].value, 0.6f);
	EXPECT_EQ(desire.sources[2].value, 0.0f);
	EXPECT_EQ(desire.sources[0].threshold, 0.5f);
	EXPECT_EQ(desire.sources[1].threshold, 0.4f);
	EXPECT_EQ(desire.sources[2].threshold, 0.2f);
}

TEST(CreatureLearning, AfterAnActionItsDesireIsMultipliedToTheFloorFirstThenItsMaximum)
{
	// within its range: the product
	auto desires = TwoDesires();
	creature_learning::AfterAction(desires, Desire::Hunger, 0.5f, 0.0f);
	EXPECT_EQ(desires[Desire::Hunger].value, 0.25f);
	// below the species' floor: the floor
	desires = TwoDesires();
	creature_learning::AfterAction(desires, Desire::Hunger, 0.5f, 0.5f);
	EXPECT_EQ(desires[Desire::Hunger].value, 0.5f);
	// a product that compares with nothing: the floor
	desires = TwoDesires();
	creature_learning::AfterAction(desires, Desire::Hunger, std::numeric_limits<float>::quiet_NaN(), 0.125f);
	EXPECT_EQ(desires[Desire::Hunger].value, 0.125f);
	// above its maximum: the maximum
	desires = TwoDesires();
	creature_learning::AfterAction(desires, Desire::Hunger, 8.0f, 0.0f);
	EXPECT_EQ(desires[Desire::Hunger].value, 2.0f);
	// the floor is tested before the maximum
	desires = TwoDesires();
	desires[Desire::Hunger].max = 0.1f;
	creature_learning::AfterAction(desires, Desire::Hunger, 0.5f, 0.3f);
	EXPECT_EQ(desires[Desire::Hunger].value, 0.3f);
	// an inactive desire is left alone
	desires = TwoDesires();
	desires[Desire::Hunger].activated = false;
	creature_learning::AfterAction(desires, Desire::Hunger, 0.5f, 0.0f);
	EXPECT_EQ(desires[Desire::Hunger].value, 0.5f);
}

TEST(CreatureLearning, FinishingAPlanHoldsSomeDesiresDownByItsDesire)
{
	constexpr float k_PerSecond = 10.0f;
	const auto heldDownFor = [&](Desire desire, uint32_t turns) {
		auto desires = TwoDesires();
		EXPECT_FALSE(creature_learning::AfterCompletion(desires, desire, 0.0f, k_PerSecond));
		EXPECT_EQ(desires[desire].suppressedTurns, turns) << creature_desires::Name(desire);
		EXPECT_TRUE(desires[desire].planVoided) << creature_desires::Name(desire);
		// only for longer than it already was
		desires[desire].planVoided = false;
		desires[desire].suppressedTurns = turns + 1;
		EXPECT_FALSE(creature_learning::AfterCompletion(desires, desire, 0.0f, k_PerSecond));
		EXPECT_EQ(desires[desire].suppressedTurns, turns + 1);
		EXPECT_FALSE(desires[desire].planVoided);
	};
	heldDownFor(Desire::Poo, 600);
	heldDownFor(Desire::IdleWithPlayer, 1200);
	heldDownFor(Desire::AttractAttention, 600);
	heldDownFor(Desire::GetWarmer, 1200);
	heldDownFor(Desire::GetColder, 1200);
	// tiredness is made least dominant: the weakest active desire divided by 1.3
	auto desires = TwoDesires();
	desires[Desire::Tiredness].value = 0.9f;
	EXPECT_FALSE(creature_learning::AfterCompletion(desires, Desire::Tiredness, 0.0f, k_PerSecond));
	EXPECT_EQ(desires[Desire::Tiredness].value, 0.2f / 1.3f);
	EXPECT_EQ(desires[Desire::Tiredness].suppressedTurns, 0u);
	// restoring health asks for the life to go up
	desires = TwoDesires();
	EXPECT_TRUE(creature_learning::AfterCompletion(desires, Desire::RestoreHealth, 0.0f, k_PerSecond));
	EXPECT_EQ(desires[Desire::RestoreHealth].suppressedTurns, 0u);
	// any other desire: nothing
	desires = TwoDesires();
	EXPECT_FALSE(creature_learning::AfterCompletion(desires, Desire::Hunger, 0.0f, k_PerSecond));
	EXPECT_EQ(desires[Desire::Hunger].suppressedTurns, 0u);
	EXPECT_EQ(desires[Desire::Hunger].value, 0.5f);
}

TEST(CreatureLearning, AForcedPlanLeavesTheDesiresAsTheyWere)
{
	auto desires = TwoDesires();
	desires[Desire::Poo].activated = true;
	desires[Desire::Poo].value = 0.7f;
	desires[Desire::Poo].sources = {{.type = 21, .value = 0.9f, .spentWhenSatisfied = true}};
	const auto result = creature_learning::CompletePlan(
	    desires,
	    {.planDesire = Desire::Poo, .actionDesire = Desire::Poo, .multiplier = 0.01f, .alwaysApplies = true, .forced = true},
	    10.0f);
	EXPECT_FALSE(result.satisfied);
	EXPECT_FALSE(result.restoreLife);
	EXPECT_EQ(desires[Desire::Poo].completions, 0u);
	EXPECT_EQ(desires[Desire::Poo].value, 0.7f);
	EXPECT_EQ(desires[Desire::Poo].sources[0].value, 0.9f);
	EXPECT_EQ(desires[Desire::Poo].suppressedTurns, 0u);
	// restoring health, forced, gives no life either
	EXPECT_FALSE(
	    creature_learning::CompletePlan(desires, {.planDesire = Desire::RestoreHealth, .forced = true}, 10.0f).restoreLife);
}

TEST(CreatureLearning, RestoringHealthGivesHalfALifeWithNoUpperLimit)
{
	// carried out to its end, satisfied or not, a plan to restore health gives its life back
	auto desires = TwoDesires();
	EXPECT_TRUE(creature_learning::CompletePlan(desires, {.planDesire = Desire::RestoreHealth}, 10.0f).restoreLife);
	EXPECT_FALSE(creature_learning::CompletePlan(desires, {.planDesire = Desire::Poo}, 10.0f).restoreLife);
	// the original's 0.5 (0x3F000000) added to the life, which can go above full
	EXPECT_EQ(creature_learning::k_RestoredLife, 0x1p-1f);
	EXPECT_EQ(creature_learning::LifeAfterRestoringHealth(0.25f), 0.75f);
	EXPECT_EQ(creature_learning::LifeAfterRestoringHealth(0.75f), 1.25f);
	EXPECT_EQ(creature_learning::LifeAfterRestoringHealth(1.0f), 1.5f);
}

TEST(CreatureLearning, ACarriedOutPlanCountsThenSatisfiesThenHoldsDown)
{
	// Poo needing two: the first time it is held down but neither multiplied nor emptied
	auto desires = TwoDesires();
	auto& poo = desires[Desire::Poo];
	poo.activated = true;
	poo.value = 0.7f;
	poo.weight = 2.0f;
	poo.sources = {{.type = 21, .value = 0.9f, .spentWhenSatisfied = true}};
	const creature_learning::Completion completion {
	    .planDesire = Desire::Poo, .actionDesire = Desire::Poo, .multiplier = 0.5f, .alwaysApplies = true};
	auto result = creature_learning::CompletePlan(desires, completion, 10.0f);
	EXPECT_FALSE(result.satisfied);
	EXPECT_EQ(poo.completions, 1u);
	EXPECT_EQ(poo.value, 0.7f);
	EXPECT_EQ(poo.sources[0].value, 0.9f);
	EXPECT_EQ(poo.suppressedTurns, 600u);
	// the second: satisfied, its spent source emptied, then multiplied
	poo.suppressedTurns = 0;
	result = creature_learning::CompletePlan(desires, completion, 10.0f);
	EXPECT_TRUE(result.satisfied);
	EXPECT_EQ(poo.completions, 0u);
	EXPECT_EQ(poo.value, 0.7f * 0.5f);
	EXPECT_EQ(poo.sources[0].value, 0.0f);
	EXPECT_EQ(poo.suppressedTurns, 600u);
}

TEST(CreatureLearning, AnActionThatDoesNotAlwaysApplyMultipliesOnlyWhenItAchievedItsAim)
{
	auto desires = TwoDesires();
	const creature_learning::Completion completion {
	    .planDesire = Desire::Anger, .actionDesire = Desire::Anger, .multiplier = 0.1f};
	EXPECT_TRUE(creature_learning::CompletePlan(desires, completion, 10.0f).satisfied);
	EXPECT_EQ(desires[Desire::Anger].value, 0.2f);
	desires.achieved = true;
	EXPECT_TRUE(creature_learning::CompletePlan(desires, completion, 10.0f).satisfied);
	EXPECT_EQ(desires[Desire::Anger].value, 0.2f * 0.1f);
}

TEST(CreatureLearning, TheActionsDesireIsMultipliedWhileThePlansDesireIsCounted)
{
	auto desires = TwoDesires();
	desires[Desire::IdleWithPlayer].activated = true;
	desires[Desire::IdleWithPlayer].value = 0.8f;
	const creature_learning::Completion obey {
	    .planDesire = Desire::ObeyPlayer, .actionDesire = Desire::IdleWithPlayer, .multiplier = 0.5f, .alwaysApplies = true};
	EXPECT_TRUE(creature_learning::CompletePlan(desires, obey, 10.0f).satisfied);
	EXPECT_EQ(desires[Desire::IdleWithPlayer].value, 0.4f);
	EXPECT_EQ(desires[Desire::IdleWithPlayer].completions, 0u);
	// the hold-down goes by the plan's desire, not the action's
	EXPECT_EQ(desires[Desire::IdleWithPlayer].suppressedTurns, 0u);
	// an action naming none of the desires multiplies nothing, the plan still counted
	desires[Desire::ObeyPlayer].weight = 2.0f;
	const creature_learning::Completion none {.planDesire = Desire::ObeyPlayer, .multiplier = 0.5f, .alwaysApplies = true};
	EXPECT_FALSE(creature_learning::CompletePlan(desires, none, 10.0f).satisfied);
	EXPECT_EQ(desires[Desire::ObeyPlayer].completions, 1u);
	EXPECT_TRUE(creature_learning::CompletePlan(desires, none, 10.0f).satisfied);
	EXPECT_EQ(desires[Desire::IdleWithPlayer].value, 0.4f);
}

TEST(CreatureLearning, LeastDominantIsTheWeakestBelowTwoItselfIncludedWithinTheFloorAndItsMax)
{
	// itself the weakest: its own value, divided
	auto desires = TwoDesires();
	desires[Desire::Hunger].value = 0.1f;
	creature_learning::MakeLeastDominant(desires, Desire::Hunger, 1.3f, 0.0f);
	EXPECT_EQ(desires[Desire::Hunger].value, 0.1f / 1.3f);
	// no lower than the species' floor
	desires = TwoDesires();
	creature_learning::MakeLeastDominant(desires, Desire::Hunger, 1.3f, 0.5f);
	EXPECT_EQ(desires[Desire::Hunger].value, 0.5f);
	// no active desire below 2: the first desire's value, active or not
	desires = TwoDesires();
	desires[Desire::Hunger].value = 2.0f;
	desires[Desire::Anger].value = 3.0f;
	desires.desires.front().value = 2.2f;
	creature_learning::MakeLeastDominant(desires, Desire::Hunger, 1.3f, 0.0f);
	EXPECT_EQ(desires[Desire::Hunger].value, 2.2f / 1.3f);
	// and no higher than its own maximum
	desires[Desire::Hunger].value = 2.0f;
	desires[Desire::Hunger].max = 1.5f;
	creature_learning::MakeLeastDominant(desires, Desire::Hunger, 1.3f, 0.0f);
	EXPECT_EQ(desires[Desire::Hunger].value, 1.5f);
	// a value that compares with nothing is taken as weaker than the one before it, and gives the floor
	desires = TwoDesires();
	desires[Desire::Hunger].value = std::numeric_limits<float>::quiet_NaN();
	creature_learning::MakeLeastDominant(desires, Desire::Anger, 1.3f, 0.25f);
	EXPECT_EQ(desires[Desire::Anger].value, 0.25f);
}

TEST(CreatureLearning, DominantSourceFollowsDrive)
{
	auto desires = TwoDesires();
	creature_desires::UpdateDesires(desires, 10.0f);
	EXPECT_EQ(creature_learning::DominantSource(desires[Desire::Hunger]), 0u);
	creature_learning::ResetDrives(desires[Desire::Hunger]);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].sources[0].drive, 0.0f);
}

TEST(CreatureLearning, AttitudesToCreatures)
{
	std::vector<creature_learning::CreatureAttitude> attitudes;
	auto& attitude = creature_learning::AttitudeTo(attitudes, 7);
	const auto lessons = creature_learning::ChangeHowNice(attitude, 1.5f);
	EXPECT_FLOAT_EQ(attitude.howNice, 1.0f);
	EXPECT_EQ(lessons[0].first, Desire::BeFriends);
	EXPECT_FLOAT_EQ(lessons[0].second, 0.5f);
	EXPECT_FLOAT_EQ(lessons[1].second, -0.5f);
	EXPECT_EQ(&creature_learning::AttitudeTo(attitudes, 7), &attitudes[0]);
}

TEST(CreatureLearning, ThoughtsAndLikes)
{
	EXPECT_EQ(creature_learning::DesireLessonText(1.0f, "desire to eat"), "I've learnt to increase my desire to eat");
	EXPECT_EQ(creature_learning::ObjectLessonText(-1.0f, "villagers", "am trying to eat"),
	          "I've learnt to avoid villagers when I am trying to eat");
	creature_desires::DesireState state {.sources = {{.threshold = 0.3f}}};
	const std::array<float, 1> start {0.5f};
	EXPECT_LT(creature_learning::LikesLevel(Desire::Hunger, state, start), 5u);
	state.sources[0].threshold = 0.5f;
	EXPECT_EQ(creature_learning::LikesLevel(Desire::Hunger, state, start), 5u);
	state.value = 1.0f;
	state.max = 1.0f;
	EXPECT_EQ(creature_learning::LikesLevel(Desire::Fear, state, start), 0u);
}

// Learning by watching

TEST(CreatureWatching, LearnsASkillOnceWatchedLongEnough)
{
	const std::vector<creature_watching::SkillRule> skills {{.name = "Build", .watchSeconds = 6.0f, .minPhase = 4}};
	auto knowledge = creature_watching::StartKnowledge(skills, {});
	EXPECT_TRUE(creature_watching::SeeSkill(knowledge, 0, skills, 3, 0, 10.0f).ignored);
	EXPECT_FALSE(creature_watching::SeeSkill(knowledge, 0, skills, 5, 100, 10.0f).learnt);
	EXPECT_FALSE(creature_watching::SeeSkill(knowledge, 0, skills, 5, 150, 10.0f).learnt);
	EXPECT_TRUE(creature_watching::SeeSkill(knowledge, 0, skills, 5, 160, 10.0f).learnt);
	EXPECT_TRUE(knowledge.skillsKnown[0]);
}

TEST(CreatureWatching, LearnsAMiracleBySightingsAndSpecies)
{
	const std::vector<creature_watching::MiracleRule> miracles {{.name = "Heal", .timesToSee = 10, .minPhase = 8},
	                                                            {.name = "Food", .knownAtStart = true}};
	auto knowledge = creature_watching::StartKnowledge({}, miracles);
	// Nothing is known from the start
	EXPECT_FALSE(knowledge.miraclesKnown[0]);
	EXPECT_FALSE(knowledge.miraclesKnown[1]);
	EXPECT_FLOAT_EQ(creature_watching::TimesNeeded(10, 1.5f), 15.0f);
	// Not rounded: a cow needs 1.7 times as many sightings
	EXPECT_FLOAT_EQ(creature_watching::TimesNeeded(9, 1.7f), 15.3f);
	uint32_t turn = 100;
	// Too young, it is told so and nothing counts
	const auto young = creature_watching::SeeMiracle(knowledge, 0, miracles, 7, turn, 3, 1.5f);
	EXPECT_TRUE(young.ignored);
	EXPECT_EQ(young.event, creature_watching::LearningEvent::TooYoung);
	EXPECT_FALSE(knowledge.miraclesKnown[0]);
	// The first sighting counts, and it knows about the miracle from then on
	EXPECT_NEAR(creature_watching::SeeMiracle(knowledge, 0, miracles, 13, turn, 3, 1.5f).share, 0.2f, 1e-6f);
	EXPECT_TRUE(knowledge.miraclesKnown[0]);
	// Seen again within 50 turns it doesn't count, and the wait starts again from then
	turn += 10;
	EXPECT_NEAR(creature_watching::SeeMiracle(knowledge, 0, miracles, 13, turn, 3, 1.5f).share, 0.2f, 1e-6f);
	turn += creature_watching::k_MiracleSightingTurns;
	EXPECT_NEAR(creature_watching::SeeMiracle(knowledge, 0, miracles, 13, turn, 3, 1.5f).share, 0.2f, 1e-6f);
	for (int i = 0; i < 3; ++i)
	{
		turn += creature_watching::k_MiracleSightingTurns + 1;
		EXPECT_FALSE(creature_watching::SeeMiracle(knowledge, 0, miracles, 13, turn, 3, 1.5f).learnt);
	}
	// Three quarters of the way, it is nearly learnt
	EXPECT_EQ(creature_watching::SeeMiracle(knowledge, 0, miracles, 13, turn, 3, 1.5f).event,
	          creature_watching::LearningEvent::NearlyLearnt);
	turn += creature_watching::k_MiracleSightingTurns + 1;
	const auto learnt = creature_watching::SeeMiracle(knowledge, 0, miracles, 13, turn, 3, 1.5f);
	EXPECT_TRUE(learnt.learnt);
	EXPECT_EQ(learnt.event, creature_watching::LearningEvent::Learnt);
	EXPECT_EQ(learnt.meter, 1.0f);
	// It goes on counting, and is told again
	turn += creature_watching::k_MiracleSightingTurns + 1;
	const auto again = creature_watching::SeeMiracle(knowledge, 0, miracles, 13, turn, 3, 1.5f);
	EXPECT_EQ(again.event, creature_watching::LearningEvent::Learnt);
	EXPECT_EQ(knowledge.miraclesSeen[0].count, 18u);
	// 18 is three beyond the 15 needed: the meter no longer shows it
	EXPECT_FALSE(again.meter.has_value());
}

TEST(CreatureWatching, APowerUpTeachesNothingBeforeItsMiracle)
{
	std::vector<creature_watching::MiracleRule> miracles(42, {.timesToSee = 10, .minPhase = 0});
	auto knowledge = creature_watching::StartKnowledge({}, miracles);
	// The fireball's first power-up before the fireball
	EXPECT_TRUE(creature_watching::SeeMiracle(knowledge, 2, miracles, 13, 100, 1, 1.0f).ignored);
	EXPECT_EQ(knowledge.miraclesSeen[2].count, 0u);
	(void)creature_watching::SeeMiracle(knowledge, 1, miracles, 13, 100, 1, 1.0f);
	EXPECT_FALSE(creature_watching::SeeMiracle(knowledge, 2, miracles, 13, 100, 1, 1.0f).ignored);
	// The thirst spell needs the skill of building
	EXPECT_TRUE(creature_watching::SeeMiracle(knowledge, 40, miracles, 13, 100, 1, 1.0f).ignored);
	ASSERT_EQ(creature_watching::MiraclePrerequisite(40)->kind, creature_watching::Prerequisite::Kind::Skill);
	EXPECT_FALSE(creature_watching::MiraclePrerequisite(19).has_value());
}

TEST(CreatureWatching, EveryMiraclesPrerequisiteIsTheGames)
{
	using Kind = creature_watching::Prerequisite::Kind;
	struct Row
	{
		size_t miracle;
		Kind kind;
		size_t index;
	};
	// The game's table of what each miracle needs first (docs/wiki/creature/mind.md#considering-a-sighting): the rows that need
	// something
	constexpr std::array<Row, 11> k_Rows {{
	    {.miracle = 2, .kind = Kind::Miracle, .index = 1},
	    {.miracle = 3, .kind = Kind::Miracle, .index = 1},
	    {.miracle = 5, .kind = Kind::Miracle, .index = 4},
	    {.miracle = 6, .kind = Kind::Miracle, .index = 4},
	    {.miracle = 8, .kind = Kind::Miracle, .index = 7},
	    {.miracle = 9, .kind = Kind::Miracle, .index = 8},
	    {.miracle = 11, .kind = Kind::Miracle, .index = 10},
	    {.miracle = 17, .kind = Kind::Miracle, .index = 16},
	    {.miracle = 18, .kind = Kind::Miracle, .index = 17},
	    {.miracle = 40, .kind = Kind::Skill, .index = 0},
	    {.miracle = 41, .kind = Kind::Skill, .index = 0},
	}};
	for (size_t miracle = 0; miracle < 42; ++miracle)
	{
		const auto needs = creature_watching::MiraclePrerequisite(miracle);
		const auto row = std::ranges::find(k_Rows, miracle, &Row::miracle);
		if (row == k_Rows.end())
		{
			EXPECT_FALSE(needs.has_value()) << miracle;
			continue;
		}
		ASSERT_TRUE(needs.has_value()) << miracle;
		EXPECT_EQ(needs->kind, row->kind) << miracle;
		EXPECT_EQ(needs->index, row->index) << miracle;
	}
}

TEST(CreatureWatching, TheTimesNeededAreAFloatProduct)
{
	// 15 times 1.2 in floats is 18 exactly, though in doubles it is a little more than 18
	EXPECT_EQ(creature_watching::TimesNeeded(15, 1.2f), 18.0f);
	EXPECT_GT(15.0 * static_cast<double>(1.2f), 18.0);
	const std::vector<creature_watching::MiracleRule> miracles {{.timesToSee = 15, .minPhase = 0}};
	auto knowledge = creature_watching::StartKnowledge({}, miracles);
	uint32_t turn = 100;
	for (uint32_t i = 1; i < 18; ++i)
	{
		EXPECT_FALSE(creature_watching::SeeMiracle(knowledge, 0, miracles, 0, turn, 1, 1.2f).learnt) << i;
		turn += creature_watching::k_MiracleSightingTurns + 1;
	}
	// Learnt at the eighteenth
	EXPECT_TRUE(creature_watching::SeeMiracle(knowledge, 0, miracles, 0, turn, 1, 1.2f).learnt);
	EXPECT_EQ(knowledge.miraclesSeen[0].count, 18u);
}

TEST(CreatureWatching, AMiracleCountsOnlyMoreThanFiftyTurnsAfterItWasLastSeen)
{
	const std::vector<creature_watching::MiracleRule> miracles {{.timesToSee = 100, .minPhase = 0}};
	auto knowledge = creature_watching::StartKnowledge({}, miracles);
	const auto& seen = knowledge.miraclesSeen[0];
	(void)creature_watching::SeeMiracle(knowledge, 0, miracles, 0, 100, 1, 1.0f);
	EXPECT_EQ(seen.count, 1u);
	// Exactly 50 turns later it doesn't count, but is the last sighting from then
	(void)creature_watching::SeeMiracle(knowledge, 0, miracles, 0, 150, 1, 1.0f);
	EXPECT_EQ(seen.count, 1u);
	EXPECT_EQ(seen.turn, 150u);
	// 51 turns after the last counts
	(void)creature_watching::SeeMiracle(knowledge, 0, miracles, 0, 201, 1, 1.0f);
	EXPECT_EQ(seen.count, 2u);
	EXPECT_EQ(seen.turn, 201u);
	// A last turn of 0 is never seen: the next sighting counts however soon it comes
	knowledge.miraclesSeen[0].turn = 0u;
	(void)creature_watching::SeeMiracle(knowledge, 0, miracles, 0, 10, 1, 1.0f);
	EXPECT_EQ(seen.count, 3u);
	EXPECT_EQ(seen.turn, 10u);
}

TEST(CreatureWatching, AnIgnoredSightingLeavesTheLastSightingAlone)
{
	std::vector<creature_watching::MiracleRule> miracles(3, {.timesToSee = 10, .minPhase = 8});
	auto knowledge = creature_watching::StartKnowledge({}, miracles);
	knowledge.miraclesSeen[1] = {.count = 2, .turn = 100u};
	knowledge.miraclesSeen[2] = {.count = 0, .turn = 40u};
	// Too young: neither the count nor the last turn moves, and it is not known about
	EXPECT_TRUE(creature_watching::SeeMiracle(knowledge, 1, miracles, 7, 300, 3, 1.0f).ignored);
	EXPECT_EQ(knowledge.miraclesSeen[1].count, 2u);
	EXPECT_EQ(knowledge.miraclesSeen[1].turn, 100u);
	EXPECT_FALSE(knowledge.miraclesKnown[1]);
	// The fireball's power-up while it doesn't know about the fireball: the same, and it is told nothing
	const auto early = creature_watching::SeeMiracle(knowledge, 2, miracles, 13, 300, 3, 1.0f);
	EXPECT_TRUE(early.ignored);
	EXPECT_FALSE(early.event.has_value());
	EXPECT_EQ(knowledge.miraclesSeen[2].count, 0u);
	EXPECT_EQ(knowledge.miraclesSeen[2].turn, 40u);
	EXPECT_FALSE(knowledge.miraclesKnown[2]);
}

TEST(CreatureWatching, AScriptTeachesAMiracleAsSeenEnoughTimesTheFractionDropped)
{
	// the times a species must see it, as a float, cut towards zero: 10 x 0.9f is just under 9 exactly, but 9 once
	// rounded to a float, so 9 sightings
	EXPECT_EQ(creature_watching::TaughtSightings(10, 0.9f), 9u);
	EXPECT_EQ(creature_watching::TaughtSightings(10, 1.35f), 13u);
	EXPECT_EQ(creature_watching::TaughtSightings(10, 1.5f), 15u);
	EXPECT_EQ(creature_watching::TaughtSightings(0, 4.0f), 0u);

	const std::vector<creature_watching::MiracleRule> miracles {{.name = "None"}, {.name = "Fireball", .timesToSee = 10}};
	auto knowledge = creature_watching::StartKnowledge({}, miracles);
	knowledge.miraclesSeen[1] = {.count = 2, .turn = 40u};
	EXPECT_TRUE(creature_watching::SetKnows(knowledge, 1, 1, true, miracles, 1.35f));
	EXPECT_TRUE(knowledge.miraclesKnown[1]);
	EXPECT_EQ(knowledge.miraclesSeen[1].count, 13u);
	// when it last saw it is not touched
	EXPECT_EQ(knowledge.miraclesSeen[1].turn, 40u);
	// taken away, it keeps its sightings
	EXPECT_TRUE(creature_watching::SetKnows(knowledge, 1, 1, false, miracles, 1.35f));
	EXPECT_FALSE(knowledge.miraclesKnown[1]);
	EXPECT_EQ(knowledge.miraclesSeen[1].count, 13u);
}

TEST(CreatureWatching, AScriptTeachesOrTakesAwayASkill)
{
	const std::vector<creature_watching::SkillRule> skills(6);
	auto knowledge = creature_watching::StartKnowledge(skills, {});
	knowledge.skillsSeen[4] = {.count = 3, .turn = 7u};
	EXPECT_TRUE(creature_watching::SetKnows(knowledge, 0, 4, true, {}, 1.0f));
	EXPECT_TRUE(knowledge.skillsKnown[4]);
	// how long it watched it is not touched
	EXPECT_EQ(knowledge.skillsSeen[4].count, 3u);
	EXPECT_EQ(knowledge.skillsSeen[4].turn, 7u);
	EXPECT_TRUE(creature_watching::SetKnows(knowledge, 0, 4, false, {}, 1.0f));
	EXPECT_FALSE(knowledge.skillsKnown[4]);
}

TEST(CreatureWatching, AScriptLeavesKindsAndRowsTheTablesDontHaveAlone)
{
	const std::vector<creature_watching::SkillRule> skills(6);
	const std::vector<creature_watching::MiracleRule> miracles(2);
	auto knowledge = creature_watching::StartKnowledge(skills, miracles);
	const auto before = knowledge;
	// the scripts teach skill 6, one past the table's last row
	EXPECT_FALSE(creature_watching::SetKnows(knowledge, 0, 6, true, miracles, 1.0f));
	EXPECT_FALSE(creature_watching::SetKnows(knowledge, 1, 2, true, miracles, 1.0f));
	EXPECT_FALSE(creature_watching::SetKnows(knowledge, 2, 0, true, miracles, 1.0f));
	EXPECT_EQ(knowledge.skillsKnown, before.skillsKnown);
	EXPECT_EQ(knowledge.miraclesKnown, before.miraclesKnown);
	EXPECT_EQ(knowledge.miraclesSeen[0].count, 0u);
	EXPECT_EQ(knowledge.miraclesSeen[1].count, 0u);
}

TEST(CreatureWatching, MimicryStages)
{
	const std::vector<creature_watching::MimicRule> rules {
	    {.name = "Play with toy", .chance = 0.9f, .needsLearningLeash = false, .stageSteps = 1, .copiesDesire = true},
	    {.name = "Steal", .chance = 0.9f, .needsLearningLeash = true, .stageSteps = 1},
	};
	std::optional<creature_watching::Mimicry> mimicry;
	const auto never = [] { return 0.99f; };
	const auto always = [] { return 0.0f; };
	const auto noExtra = [](uint32_t) { return 0u; };
	creature_watching::MimicConditions grown {.phase = 13};
	EXPECT_FALSE(creature_watching::StartMimicry(mimicry, 0, rules, grown, std::nullopt, never));
	EXPECT_FALSE(creature_watching::StartMimicry(mimicry, 1, rules, grown, std::nullopt, always));
	creature_watching::MimicConditions young {.phase = 2};
	EXPECT_FALSE(creature_watching::StartMimicry(mimicry, 0, rules, young, std::nullopt, always));
	ASSERT_TRUE(creature_watching::StartMimicry(mimicry, 0, rules, grown, 5u, always));
	EXPECT_EQ(mimicry->stage, creature_watching::MimicStage::Notice);
	creature_watching::StepMimicry(mimicry, rules, noExtra);
	creature_watching::StepMimicry(mimicry, rules, noExtra);
	EXPECT_EQ(mimicry->stage, creature_watching::MimicStage::CopyAction);
	creature_watching::StepMimicry(mimicry, rules, noExtra);
	creature_watching::StepMimicry(mimicry, rules, noExtra);
	EXPECT_EQ(mimicry->stage, creature_watching::MimicStage::CopyDesire);
	creature_watching::StepMimicry(mimicry, rules, noExtra);
	creature_watching::StepMimicry(mimicry, rules, noExtra);
	EXPECT_FALSE(mimicry.has_value());
}

// Carrying out plans

TEST(CreaturePlanActions, BuildsAgendas)
{
	const auto random = [](uint32_t) { return 0u; };
	const auto* eat = creature_plan_actions::For("EatAfterExamining");
	ASSERT_NE(eat, nullptr);
	EXPECT_FALSE(creature_plan_actions::Agenda(*eat, std::nullopt, {}, {}, random).has_value());
	EXPECT_EQ(creature_plan_actions::Agenda(*eat, 3u, {}, {}, random)->size(), 3u);
	const auto* drink = creature_plan_actions::For("DrinkFromTheSea");
	ASSERT_NE(drink, nullptr);
	EXPECT_FALSE(creature_plan_actions::Possible(*drink, {}));
	const auto* wave = creature_plan_actions::For("WaveAtPlayer");
	ASSERT_NE(wave, nullptr);
	creature_plan_actions::Situation situation {.camera = glm::vec2(0.0f, -100.0f)};
	EXPECT_EQ(creature_plan_actions::Agenda(*wave, std::nullopt, {}, situation, random)->size(), 2u);
	EXPECT_EQ(creature_plan_actions::For("CastFireball"), nullptr);
}

TEST(CreaturePlanActions, EatingFromAPileGoesUpToItAndEatsAPortion)
{
	// The game's plan goes to within its distance of the pile, turns to it, makes a portion from it and eats that
	const auto random = [](uint32_t) { return 0u; };
	const auto* pile = creature_plan_actions::For("EatFromFoodPile");
	ASSERT_NE(pile, nullptr);
	EXPECT_EQ(pile->target, creature_plan_actions::Target::FoodPile);
	const creature_plan_actions::Situation situation {.portionApproach = 6.0f};
	const auto agenda = creature_plan_actions::Agenda(*pile, 9u, {40.0f, 50.0f}, situation, random);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 4u);
	using creature_mind::Movement;
	using creature_mind::ObjectOrder;
	using creature_mind::Step;
	EXPECT_EQ((*agenda)[0].kind, Step::Kind::Move);
	EXPECT_EQ((*agenda)[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ((*agenda)[0].movement.point, glm::vec2(40.0f, 50.0f));
	EXPECT_EQ((*agenda)[0].movement.maxDistance, 6.0f);
	EXPECT_EQ((*agenda)[1].movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ((*agenda)[1].movement.point, glm::vec2(40.0f, 50.0f));
	EXPECT_EQ((*agenda)[2].kind, Step::Kind::Object);
	EXPECT_EQ((*agenda)[2].order.kind, ObjectOrder::Kind::PickUpPortion);
	EXPECT_EQ((*agenda)[2].order.object, 9u);
	EXPECT_EQ((*agenda)[3].order.kind, ObjectOrder::Kind::Eat);
	EXPECT_EQ((*agenda)[3].effect, creature_mind::Effect::Eat);
	// none without a pile, and none with its hand full
	EXPECT_FALSE(creature_plan_actions::Agenda(*pile, std::nullopt, {}, situation, random).has_value());
	EXPECT_FALSE(creature_plan_actions::Agenda(*pile, 9u, {}, {.handFull = true}, random).has_value());
}

namespace
{
/// Checks a plan for eating from a storage pit, a field or a tree: going to the point within the distance, turning to
/// face the source a tenth of a second, letting go of what the plan acts on (when it does), a portion of the kind from
/// the source, and eating it, with no face of their own
void ExpectEatFrom(const std::vector<creature_mind::Step>& agenda, uint32_t source, glm::vec2 point, float distance,
                   bool clears, PotInfo portion)
{
	using creature_mind::Movement;
	using creature_mind::ObjectOrder;
	using creature_mind::Step;
	ASSERT_EQ(agenda.size(), clears ? 5u : 4u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Move);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, point);
	EXPECT_EQ(std::bit_cast<uint32_t>(agenda[0].movement.maxDistance), std::bit_cast<uint32_t>(distance));
	EXPECT_EQ(agenda[1].kind, Step::Kind::Move);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[1].movement.object, source);
	EXPECT_EQ(std::bit_cast<uint32_t>(agenda[1].seconds), 0x3DCCCCCDu);
	const size_t take = clears ? 3 : 2;
	if (clears)
	{
		EXPECT_EQ(agenda[2].kind, Step::Kind::ClearObjectToActOn);
	}
	EXPECT_EQ(agenda[take].kind, Step::Kind::Object);
	EXPECT_EQ(agenda[take].order.kind, ObjectOrder::Kind::PickUpPortion);
	EXPECT_EQ(agenda[take].order.object, source);
	EXPECT_EQ(agenda[take].order.portion, portion);
	EXPECT_EQ(agenda[take + 1].order.kind, ObjectOrder::Kind::Eat);
	EXPECT_EQ(agenda[take + 1].effect, creature_mind::Effect::Eat);
	for (const auto& step : agenda)
	{
		EXPECT_EQ(step.face, creature_face::Cue::None);
	}
}
} // namespace

TEST(CreaturePlanActions, EatingFromAStoragePitGoesToItsFoodPileAndEatsAPortionOfWheat)
{
	// The pit is what the plan is done to; it goes to within its height and two fifths of the pit's food pile, the
	// product a float, faces the pile and takes the portion from it, whatever its hand holds
	const auto random = [](uint32_t) { return 0u; };
	const creature_plan_actions::Executor pit {.action = "EatFromStoragePit",
	                                           .build = creature_plan_actions::Build::EatFromStoragePit};
	creature_plan_actions::Situation situation {
	    .pitPile = 9u, .pitPilePoint = glm::vec2(40.0f, 50.0f), .height = 0.3f, .holdingEatable = true};
	auto agenda = creature_plan_actions::Agenda(pit, 5u, {1.0f, 2.0f}, situation, random);
	ASSERT_TRUE(agenda.has_value());
	ExpectEatFrom(*agenda, 9u, {40.0f, 50.0f}, std::bit_cast<float>(0x3ED70A3Eu), true, PotInfo::WheatInHand);
	situation.height = 10.0f;
	agenda = creature_plan_actions::Agenda(pit, 5u, {1.0f, 2.0f}, situation, random);
	ASSERT_TRUE(agenda.has_value());
	EXPECT_EQ((*agenda)[0].movement.maxDistance, 14.0f);
	// none without a food pile
	situation.pitPile.reset();
	EXPECT_FALSE(creature_plan_actions::Agenda(pit, 5u, {1.0f, 2.0f}, situation, random).has_value());
}

TEST(CreaturePlanActions, EatingFromAFieldGoesToWithinItsHeightAndEatsAPortionOfWheat)
{
	const auto random = [](uint32_t) { return 0u; };
	const creature_plan_actions::Executor field {.action = "EatFromContainer",
	                                             .build = creature_plan_actions::Build::EatFromField};
	creature_plan_actions::Situation situation {.handFull = true, .height = 1.7f};
	// holding something it cannot eat, it goes all the same
	auto agenda = creature_plan_actions::Agenda(field, 6u, {30.0f, -20.0f}, situation, random);
	ASSERT_TRUE(agenda.has_value());
	ExpectEatFrom(*agenda, 6u, {30.0f, -20.0f}, 1.7f, true, PotInfo::WheatInHand);
	// holding something it can eat, it makes no agenda
	situation.holdingEatable = true;
	EXPECT_FALSE(creature_plan_actions::Agenda(field, 6u, {30.0f, -20.0f}, situation, random).has_value());
}

TEST(CreaturePlanActions, EatingFromATreeGoesToWithinTheRadiiAndEatsAPortionOfFruit)
{
	// Within (the tree's radius + its own) x 1.2, as a pile's: 6.1 + 4.3 rounds to 0x41266666, times 1.2 to 0x4147AE15
	const auto random = [](uint32_t) { return 0u; };
	const creature_plan_actions::Executor tree {.action = "EatFromTree", .build = creature_plan_actions::Build::EatFromTree};
	creature_plan_actions::Situation situation {.handFull = true, .portionApproach = creature_eating::PileApproach(6.1f, 4.3f)};
	auto agenda = creature_plan_actions::Agenda(tree, 4u, {-5.0f, 8.0f}, situation, random);
	ASSERT_TRUE(agenda.has_value());
	ExpectEatFrom(*agenda, 4u, {-5.0f, 8.0f}, std::bit_cast<float>(0x4147AE15u), false, PotInfo::TreeFruitInHand);
	situation.holdingEatable = true;
	EXPECT_FALSE(creature_plan_actions::Agenda(tree, 4u, {-5.0f, 8.0f}, situation, random).has_value());
}

TEST(CreaturePlanActions, ACreatureRunsFromWhatFrightensIt)
{
	// Other creatures, bats, vultures, lions and miracles, not villagers
	const auto* run = creature_plan_actions::For("RunAwayFromObject");
	ASSERT_NE(run, nullptr);
	EXPECT_EQ(run->target, creature_plan_actions::Target::Frightening);
}

// The model of what is learnt

TEST(CreatureMindModel, LearningRebuildsTrees)
{
	creature_desires::Desires desires;
	auto learnt = creature_mind_model::Fresh(desires, 10, {});
	creature_mind_model::Learn(learnt, creature_mind_model::TreeKind::ActOn, Desire::Hunger,
	                           {.belief = Thing(k_Food, 0), .feedback = -1.0f}, k_HungerAttributes);
	creature_mind_model::Learn(learnt, creature_mind_model::TreeKind::ActOn, Desire::Hunger,
	                           {.belief = Thing(k_Villager, 1), .feedback = 1.0f}, k_HungerAttributes);
	const auto& tree = learnt.trees[0][static_cast<size_t>(Desire::Hunger)];
	EXPECT_LT(creature_tree::Evaluate(tree, Thing(k_Food, 0)), 0.0f);
	EXPECT_GT(creature_tree::Evaluate(tree, Thing(k_Villager, 1)), 0.0f);
	for (int i = 0; i < 12; ++i)
	{
		creature_mind_model::Think(learnt, "thought");
	}
	EXPECT_EQ(learnt.thoughts.size(), creature_mind_model::k_MaxThoughts);
}

// A new creature's starting lesson, as the original's (its numbers read from the game's code): one example on the
// hunger act-on tree, an enemy villager, alive and not on fire, with the feedback 0.8

TEST(CreatureMindModel, TheStartingLessonIsAnEnemyVillagerGoodToEat)
{
	const auto lesson = creature_mind_model::StartingLesson();
	EXPECT_EQ(lesson.belief.type, k_Villager);
	EXPECT_EQ(creature_tree::ToSlots(lesson.belief), (std::vector<uint32_t> {1, 0, 0, 0, 0, 0, 6, 0, 0, 1, 1}));
	EXPECT_EQ(lesson.feedback, 0x1.99999ap-1f); // 0.8
	EXPECT_EQ(lesson.saved, (std::array<int32_t, 4> {2, 0, 0, 0}));
}

TEST(CreatureMindModel, ANewCreatureFindsEveryThingAsGoodToEatAsTheLessonsStep)
{
	creature_desires::Desires desires;
	auto learnt = creature_mind_model::Fresh(desires, 10, {});
	creature_mind_model::TeachStartingLesson(learnt, k_HungerAttributes);
	const auto hunger = static_cast<size_t>(Desire::Hunger);
	const auto actOn = static_cast<size_t>(creature_mind_model::TreeKind::ActOn);
	ASSERT_EQ(learnt.episodes[actOn][hunger].size(), 1u);
	// One example makes a single leaf: 0.8 falls into the step 0.6, the first within a quarter of it
	const auto& tree = learnt.trees[actOn][hunger];
	EXPECT_EQ(creature_tree::Evaluate(tree, Thing(k_Villager, 1)), 0x1.333334p-1f); // 0.6
	EXPECT_EQ(creature_tree::Evaluate(tree, Thing(k_Food, 0)), 0x1.333334p-1f);
	// Nothing else learns from it
	for (size_t k = 0; k < creature_mind_model::k_TreeKinds; ++k)
	{
		for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
		{
			EXPECT_EQ(learnt.episodes[k][d].size(), k == actOn && d == hunger ? 1u : 0u);
		}
	}
}

TEST(CreatureMindModel, TheStartingLessonIsOnlyForACreatureThatKnowsNothingAboutWhatToEat)
{
	creature_desires::Desires desires;
	auto learnt = creature_mind_model::Fresh(desires, 10, {});
	creature_mind_model::TeachStartingLesson(learnt, k_HungerAttributes);
	creature_mind_model::TeachStartingLesson(learnt, k_HungerAttributes);
	const auto hunger = static_cast<size_t>(Desire::Hunger);
	EXPECT_EQ(learnt.episodes[0][hunger].size(), 1u);
	auto taught = creature_mind_model::Fresh(desires, 10, {});
	creature_mind_model::Learn(taught, creature_mind_model::TreeKind::ActOn, Desire::Hunger,
	                           {.belief = Thing(k_Food, 0), .feedback = -1.0f}, k_HungerAttributes);
	creature_mind_model::TeachStartingLesson(taught, k_HungerAttributes);
	ASSERT_EQ(taught.episodes[0][hunger].size(), 1u);
	EXPECT_EQ(taught.episodes[0][hunger][0].feedback, -1.0f);
}

// What a creature thinks its player wants (Creature/PerceivedDesires.h): the clamped add and its guards, the fade, the
// dominant desire taken and forgotten, whether it sees a point; and the mind's side with a fake leash:
// only the player's own creature, and only when it sees the point, the feedback's desires, and the town needs passed on
// by the game's handler

namespace
{
map_coords::MapCoords At(float x, float z)
{
	return map_coords::FromMetres(glm::vec2(x, z));
}
} // namespace

TEST(PerceivedDesires, TheWeightIsAddedThenHeldBetweenNoneAndOne)
{
	PerceivedDesires desires;
	Increase(desires, 2, 0.75f);
	EXPECT_FLOAT_EQ(desires.player[2], 0.75f);
	Increase(desires, 2, 0.75f);
	EXPECT_EQ(desires.player[2], 1.0f);
	Increase(desires, 2, -1.5f);
	EXPECT_EQ(desires.player[2], 0.0f);
	// not a number is none
	Increase(desires, 3, std::numeric_limits<float>::quiet_NaN());
	EXPECT_EQ(desires.player[3], 0.0f);
	IncreaseTown(desires, 0, 0.5f);
	IncreaseTown(desires, 0, 0.75f);
	EXPECT_EQ(desires.town[0], 1.0f);
	IncreaseTown(desires, 0, -2.0f);
	EXPECT_EQ(desires.town[0], 0.0f);
}

TEST(PerceivedDesires, ADesireOutOfItsArrayIsNothing)
{
	PerceivedDesires desires;
	Increase(desires, 39, 0.5f);
	Increase(desires, 40, 0.5f);
	Increase(desires, -1, 0.5f);
	IncreaseTown(desires, 16, 0.25f);
	IncreaseTown(desires, 17, 0.25f);
	IncreaseTown(desires, -1, 0.25f);
	PerceivedDesires expected;
	expected.player[39] = 0.5f;
	expected.town[16] = 0.25f;
	EXPECT_EQ(desires.player, expected.player);
	EXPECT_EQ(desires.town, expected.town);
}

TEST(PerceivedDesires, EveryValueFadesEachTurn)
{
	PerceivedDesires desires;
	desires.player.fill(1.0f);
	desires.town.fill(0.5f);
	Fade(desires);
	for (const auto value : desires.player)
	{
		EXPECT_EQ(value, k_TurnFade);
	}
	for (const auto value : desires.town)
	{
		EXPECT_EQ(value, 0.5f * k_TurnFade);
	}
}

TEST(PerceivedDesires, TheDominantIsTheLastActivatedOneWantedAndThoseFoundAreForgotten)
{
	PerceivedDesires desires;
	Increase(desires, 1, 0.2f);
	Increase(desires, 2, 0.9f);
	Increase(desires, 5, 0.3f);
	desires.player[7] = -0.5f;
	const auto activated = [](size_t desire) { return desire != 5; };
	EXPECT_EQ(TakeDominant(desires, activated), 2u);
	EXPECT_EQ(desires.player[1], 0.0f);
	EXPECT_EQ(desires.player[2], 0.0f);
	// not activated, or not wanted at all: kept and not taken
	EXPECT_FLOAT_EQ(desires.player[5], 0.3f);
	EXPECT_EQ(desires.player[7], -0.5f);
	EXPECT_FALSE(TakeDominant(desires, activated).has_value());
}

TEST(PerceivedDesires, ACreatureSeesTwoThirdsOfAHalfTurnEitherWayOrInItsCell)
{
	EXPECT_TRUE(CanSeePos(0, 0x2AA, false));
	EXPECT_FALSE(CanSeePos(0, 0x2AB, false));
	EXPECT_TRUE(CanSeePos(0x2AA, 0, false));
	// across the turn's end: the difference folds once past a half turn
	EXPECT_TRUE(CanSeePos(0x7FF, 0x2A9, false));
	EXPECT_TRUE(CanSeePos(0, 0x556, false));
	EXPECT_FALSE(CanSeePos(0, 0x555, false));
	// a half turn exactly does not fold
	EXPECT_FALSE(CanSeePos(0, 0x400, false));
	EXPECT_TRUE(CanSeePos(0, 0x400, true));
}

TEST(PerceivedDesires, WhereItLooksOnTheMap)
{
	const float alongX = std::numbers::pi_v<float> / 2.0f;
	EXPECT_TRUE(CanSeePos(alongX, At(105.0f, 105.0f), At(205.0f, 105.0f)));
	EXPECT_FALSE(CanSeePos(alongX, At(105.0f, 105.0f), At(5.0f, 105.0f)));
	// behind it, in its own cell
	EXPECT_TRUE(CanSeePos(alongX, At(105.0f, 105.0f), At(101.0f, 105.0f)));
	EXPECT_FALSE(CanSeePos(alongX, At(105.0f, 105.0f), At(99.0f, 105.0f)));
	// a body yaw of 0 looks along -z
	EXPECT_TRUE(CanSeePos(0.0f, At(105.0f, 105.0f), At(105.0f, 5.0f)));
	EXPECT_FALSE(CanSeePos(0.0f, At(105.0f, 105.0f), At(105.0f, 205.0f)));
}

TEST(PerceivedDesires, ItLooksWhereItsHeadLooksUnlessAtNothingOrWhereItStands)
{
	const glm::vec3 here(100.0f, 3.0f, 100.0f);
	EXPECT_EQ(LookYaw(std::nullopt, here, 0.25f), 0.25f);
	EXPECT_EQ(LookYaw(glm::vec3(0.0f), here, 0.25f), 0.25f);
	EXPECT_EQ(LookYaw(here, here, 0.25f), 0.25f);
	EXPECT_FLOAT_EQ(LookYaw(glm::vec3(150.0f, 0.0f, 100.0f), here, 0.25f), std::numbers::pi_v<float> / 2.0f);
	EXPECT_FLOAT_EQ(LookYaw(glm::vec3(100.0f, 9.0f, 50.0f), here, 0.25f), 0.0f);
}

namespace
{
/// A registry of its own, the player's creature behind a fake leash, and the minds made by the test
class PerceivedDesiresMindTest: public ::testing::Test
{
protected:
	PerceivedDesiresMindTest()
	{
		leash = &static_cast<FakeLeash&>(Locator::leashSystem::emplace<FakeLeash>(log));
		auto& registry = test::creature_world::World::Registry();
		creature = registry.Create();
		registry.Assign<Transform>(creature, glm::vec3(105.0f, 0.0f, 105.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<CreatureMindState>(creature);
		auto& moving = registry.Assign<CreatureLocomotion>(creature);
		// a heading of minus a quarter turn faces +x
		moving.started = true;
		moving.heading = -std::numbers::pi_v<float> / 2.0f;
		leash->playersCreature = creature;
	}

	[[nodiscard]] const CreatureMindState& Mind() const
	{
		return std::as_const(test::creature_world::World::Registry()).Get<CreatureMindState>(creature);
	}

	test::creature_world::World world;
	CallLog log;
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	FakeLeash* leash {nullptr};
	entt::entity creature {entt::null};
	ecs::systems::CreatureMindSystem minds;
};
} // namespace

TEST_F(PerceivedDesiresMindTest, ThePlayersCreatureTakesItTheWantsWhatItSees)
{
	minds.EmpathiseWithPlayer(PlayerNames::PLAYER_ONE, CreatureDesires::Compassion, 0.5f, glm::vec3(205.0f, 0.0f, 105.0f));
	minds.EmpathiseWithTownDesire(PlayerNames::PLAYER_ONE, TownDesireInfo::ForWood, 0.5f, glm::vec3(205.0f, 0.0f, 105.0f));
	EXPECT_EQ(Mind().perceivedDesires.player[1], 0.5f);
	EXPECT_EQ(Mind().perceivedDesires.town[1], 0.5f);
}

TEST_F(PerceivedDesiresMindTest, WhatItCannotSeeChangesNothing)
{
	minds.EmpathiseWithPlayer(PlayerNames::PLAYER_ONE, CreatureDesires::Compassion, 0.5f, glm::vec3(5.0f, 0.0f, 105.0f));
	minds.EmpathiseWithTownDesire(PlayerNames::PLAYER_ONE, TownDesireInfo::ForWood, 0.5f, glm::vec3(5.0f, 0.0f, 105.0f));
	EXPECT_EQ(Mind().perceivedDesires.player, PerceivedDesires {}.player);
	EXPECT_EQ(Mind().perceivedDesires.town, PerceivedDesires {}.town);
}

TEST_F(PerceivedDesiresMindTest, APlayerWithNoCreatureOrNoLeashChangesNothing)
{
	leash->playersCreature = std::nullopt;
	minds.EmpathiseWithPlayer(PlayerNames::PLAYER_ONE, CreatureDesires::Anger, 0.5f, glm::vec3(205.0f, 0.0f, 105.0f));
	Locator::leashSystem::reset();
	minds.EmpathiseWithPlayer(PlayerNames::PLAYER_ONE, CreatureDesires::Anger, 0.5f, glm::vec3(205.0f, 0.0f, 105.0f));
	EXPECT_EQ(Mind().perceivedDesires.player[2], 0.0f);
}

TEST_F(PerceivedDesiresMindTest, AStrokeShowsCompassionAndASlapAngerByHowHard)
{
	minds.ReceiveFeedback(creature, 0.25f);
	minds.ReceiveFeedback(creature, -0.75f);
	// too slight to count
	minds.ReceiveFeedback(creature, 0.01f);
	EXPECT_EQ(Mind().perceivedDesires.player[1], 0.25f);
	EXPECT_EQ(Mind().perceivedDesires.player[2], 0.75f);
}

TEST_F(PerceivedDesiresMindTest, AnyFeedbackFirstStopsWhatItWasDoing)
{
	for (const float feedback : {0.25f, -0.75f, 0.01f})
	{
		auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
		mind.planActive = true;
		mind.planner.current = creature_planner::Plan {.desire = Desire::Hunger, .action = 12, .priority = 5.0f};
		minds.ReceiveFeedback(creature, feedback);
		EXPECT_FALSE(Mind().planActive);
		EXPECT_FALSE(Mind().planner.current.has_value());
	}
}

TEST_F(PerceivedDesiresMindTest, FeedbackThatOnlyMovesTheAttitudeLeavesTheRestAsItWas)
{
	const float attitude = Mind().attitudeToPlayer;
	minds.UpdateAttitudeFromFeedback(creature, -0.5f);
	// it takes its player to want anger and cools to the player, as a slap would
	EXPECT_EQ(Mind().perceivedDesires.player[2], 0.5f);
	EXPECT_LT(Mind().attitudeToPlayer, attitude);
	// but it was not given feedback: nothing remembers it, and it goes on with what it was doing
	EXPECT_FALSE(Mind().lastFeedback.has_value());
}

TEST_F(PerceivedDesiresMindTest, TheGamesHandlerPassesTheTownNeedOnToTheMinds)
{
	const test::RestoreService<Locator::creatureMindSystem> restoreMind;
	CallLog heard;
	Locator::creatureMindSystem::emplace<FakeMind>(heard);
	ecs::creature_mimic::AddMimicEventHandlers(Locator::events::value());
	auto& registry = test::creature_world::World::Registry();
	const auto villager = registry.Create();
	registry.Assign<Transform>(villager, glm::vec3(4.0f, 0.5f, 8.0f), glm::mat3(1.0f), glm::vec3(1.0f));

	ecs::creature_mimic::EmpathiseWithTownDesire(PlayerNames::PLAYER_TWO, TownDesireInfo::ForFood, 0.5f, villager);
	ecs::creature_mimic::EmpathiseWithTownDesire(std::nullopt, TownDesireInfo::ForFood, 0.5f, villager);

	ASSERT_EQ(heard.size(), 1u);
	EXPECT_EQ(heard.front().name, "mind.EmpathiseWithTownDesire");
	EXPECT_EQ(heard.front().args,
	          (std::vector<float> {static_cast<float>(PlayerNames::PLAYER_TWO), 0.0f, 0.5f, 4.0f, 0.5f, 8.0f}));
}

// A creature's compassion for a town

namespace
{
namespace town_compassion = creature_town_compassion;

/// A town that wants food (0), wood (1) and protection (3), in that order, and relaxation (15) last; its other desires
/// have no actions or aren't felt
struct FakeTown
{
	std::array<float, town_compassion::k_TownDesireCount> felt {};
	std::vector<uint32_t> order {0, 1, 3, 15, 2, 4};
	town_compassion::DesireActions actions {};

	FakeTown()
	{
		felt.at(0) = 0.8f;
		felt.at(1) = 0.5f;
		felt.at(3) = 0.3f;
		felt.at(15) = 0.2f;
		actions.at(0) = {29, 28};
		actions.at(1) = {32};
		actions.at(3) = {286};
		actions.at(4) = {38, 101};
		actions.at(15) = {288};
	}
};

std::function<uint32_t(uint32_t)> Always(uint32_t value)
{
	return [value](uint32_t) { return value; };
}
} // namespace

TEST(CreatureTownCompassion, ATownsDesiresToHelpWithRunInItsOrderToTheFirstItDoesntFeel)
{
	FakeTown town;
	EXPECT_EQ(town_compassion::DesiresToHelp(town.order, town.felt, town.actions), (std::vector<uint32_t> {0, 1, 3, 15}));
	// Mercy has actions but isn't felt: the list stops there, even with desires after it
	town.order = {0, 4, 1};
	EXPECT_EQ(town_compassion::DesiresToHelp(town.order, town.felt, town.actions), (std::vector<uint32_t> {0}));
	// Desires without actions are passed over, felt or not
	town.felt.at(2) = 0.9f;
	town.order = {2, 1};
	EXPECT_EQ(town_compassion::DesiresToHelp(town.order, town.felt, town.actions), (std::vector<uint32_t> {1}));
}

TEST(CreatureTownCompassion, ItTakesUpTheFirstDesireAndRemembersMostOfHowMuchTheTownFeelsIt)
{
	const FakeTown town;
	const auto desires = town_compassion::DesiresToHelp(town.order, town.felt, town.actions);
	town_compassion::State state;
	town_compassion::Settle(state, desires, town.felt);
	ASSERT_EQ(state.desire, 0u);
	EXPECT_FLOAT_EQ(state.remembered, 0.8f * 0.85f);
	EXPECT_EQ(town_compassion::Actions(state, town.actions, std::nullopt), (std::vector<uint32_t> {29, 28}));
	// One it is already helping with stays
	state.desire = 3;
	town_compassion::Settle(state, desires, town.felt);
	EXPECT_EQ(state.desire, 3u);
	// With nothing to help with, nothing changes
	state.desire = 7;
	town_compassion::Settle(state, {}, town.felt);
	EXPECT_EQ(state.desire, 7u);
}

TEST(CreatureTownCompassion, ItMovesOnAsItPlansOnlyWhenCompassionLeadsAndHalfAMinuteHasGone)
{
	town_compassion::State state {.lastTurn = 100};
	EXPECT_FALSE(town_compassion::MovesOnWhilePlanning(state, true, false, 130));
	EXPECT_TRUE(town_compassion::MovesOnWhilePlanning(state, true, false, 131));
	EXPECT_FALSE(town_compassion::MovesOnWhilePlanning(state, false, false, 131));
	EXPECT_FALSE(town_compassion::MovesOnWhilePlanning(state, true, true, 131));
	state.choosesFreely = false;
	EXPECT_FALSE(town_compassion::MovesOnWhilePlanning(state, true, false, 131));

	const FakeTown town;
	const auto desires = town_compassion::DesiresToHelp(town.order, town.felt, town.actions);
	town_compassion::State going {.desire = 15, .index = 3, .timesKept = 2};
	town_compassion::MoveOnWhilePlanning(going, desires, town.felt, 500);
	EXPECT_EQ(going.desire, 0u);
	EXPECT_EQ(going.index, 0u);
	EXPECT_EQ(going.lastTurn, 500u);
	EXPECT_EQ(going.timesKept, 0u);
	town_compassion::MoveOn(going, {}, town.felt);
	EXPECT_FALSE(going.desire.has_value());
}

TEST(CreatureTownCompassion, HavingHelpedItStaysWithADesireTheTownStillFeelsMoreAFewTimes)
{
	FakeTown town;
	const auto desires = town_compassion::DesiresToHelp(town.order, town.felt, town.actions);
	town_compassion::State state;
	town_compassion::Settle(state, desires, town.felt);
	// The town still wants food more than it remembered: with the low toss it stays three times more
	for (uint32_t times = 1; times <= 3; ++times)
	{
		town_compassion::FinishedHelping(state, desires, town.felt, 10 * times, Always(0));
		EXPECT_EQ(state.desire, 0u);
		EXPECT_EQ(state.timesKept, times);
		EXPECT_EQ(state.lastTurn, 10 * times);
	}
	town_compassion::FinishedHelping(state, desires, town.felt, 40, Always(0));
	EXPECT_EQ(state.desire, 1u);
	EXPECT_EQ(state.timesKept, 0u);
	EXPECT_FLOAT_EQ(state.remembered, 0.5f * 0.85f);
	// Once the town feels it no more than remembered, it moves on at once
	town.felt.at(1) = 0.4f;
	town_compassion::FinishedHelping(state, desires, town.felt, 50, Always(1));
	EXPECT_EQ(state.desire, 3u);
	// It never stays with relaxation
	state = {.desire = 15, .index = 3, .remembered = 0.0f};
	town_compassion::FinishedHelping(state, desires, town.felt, 60, Always(1));
	EXPECT_EQ(state.desire, 0u);
	// Held to a desire by the leash, it doesn't move on
	state = {.desire = 1, .index = 1, .choosesFreely = false};
	town_compassion::FinishedHelping(state, desires, town.felt, 70, Always(1));
	EXPECT_EQ(state.desire, 1u);
	EXPECT_EQ(state.lastTurn, 0u);
}

TEST(CreatureTownCompassion, HurtPeopleMakeItThinkOfHealingFirstOnceItHasSeenHealingEnough)
{
	EXPECT_TRUE(town_compassion::HealFirst(1, false, 7.0f, 10.0f, Always(0)));
	EXPECT_FALSE(town_compassion::HealFirst(1, false, 7.0f, 10.0f, Always(1)));
	EXPECT_FALSE(town_compassion::HealFirst(1, false, 6.0f, 10.0f, Always(0)));
	EXPECT_FALSE(town_compassion::HealFirst(0, false, 10.0f, 10.0f, Always(0)));
	EXPECT_FALSE(town_compassion::HealFirst(1, true, 10.0f, 10.0f, Always(0)));

	const FakeTown town;
	const town_compassion::State state {.desire = 0};
	EXPECT_EQ(town_compassion::Actions(state, town.actions, 101u), (std::vector<uint32_t> {101, 28}));
	// Healing comes first even with no desire to help with
	EXPECT_EQ(town_compassion::Actions({}, town.actions, 101u), (std::vector<uint32_t> {101}));
}

TEST(CreatureTownCompassion, TheListEndsAtADesireFeltByNothingAndHealingFirstAsksAShareStrictlyAboveTheMark)
{
	FakeTown town;
	// a desire felt by a value that compares with nothing ends the list, as one felt not at all does
	town.felt.at(1) = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(town_compassion::DesiresToHelp(town.order, town.felt, town.actions), (std::vector<uint32_t> {0}));
	// the town's count of hurt people may fall below none: that is nobody hurt
	EXPECT_FALSE(town_compassion::HealFirst(-1, false, 10.0f, 10.0f, Always(0)));
	// the share strictly above six tenths: six tenths itself is not enough
	EXPECT_FALSE(town_compassion::HealFirst(1, false, 6.0f, 10.0f, Always(0)));
	EXPECT_TRUE(town_compassion::HealFirst(1, false, 6.5f, 10.0f, Always(0)));
	// a miracle that needs no sightings: any sighting is more than enough, none at all is no number and is not
	EXPECT_TRUE(town_compassion::HealFirst(1, false, 1.0f, 0.0f, Always(0)));
	EXPECT_FALSE(town_compassion::HealFirst(1, false, 0.0f, 0.0f, Always(0)));
}

TEST(CreatureTownCompassion, TakingUpCompassionForATownStartsTheHalfMinuteAfresh)
{
	town_compassion::State state {.lastTurn = 100};
	EXPECT_TRUE(town_compassion::MovesOnWhilePlanning(state, true, false, 200));
	town_compassion::StartHalfMinute(state, 190);
	EXPECT_FALSE(town_compassion::MovesOnWhilePlanning(state, true, false, 200));
	EXPECT_TRUE(town_compassion::MovesOnWhilePlanning(state, true, false, 221));
}

TEST(CreatureTownCompassion, ATownWithNothingInItIsOfNoUseToCompassion)
{
	test::creature_world::World world;
	auto& registry = test::creature_world::World::Registry();
	const auto town = registry.Create();
	registry.Assign<ecs::components::Town>(town).id = 7;
	EXPECT_FALSE(ecs::systems::mind_detail::TownHasMembers(registry, town));
	// one of its villagers, while it is not going
	const auto villager = registry.Create();
	registry.Assign<ecs::components::Villager>(villager).town = town;
	EXPECT_TRUE(ecs::systems::mind_detail::TownHasMembers(registry, town));
	registry.Assign<ecs::components::Unavailable>(villager);
	EXPECT_FALSE(ecs::systems::mind_detail::TownHasMembers(registry, town));
	// one of its abodes
	const auto hut = registry.Create();
	registry.Assign<ecs::components::Abode>(hut).townId = 7;
	EXPECT_TRUE(ecs::systems::mind_detail::TownHasMembers(registry, town));
	// not a town at all
	EXPECT_FALSE(ecs::systems::mind_detail::TownHasMembers(registry, hut));
}

TEST(CreatureTownCompassion, AMemberOfTheTownIsChosenForTheDistanceWithNoLimitOnHowFar)
{
	using creature_planner::ObjectCandidate;
	// compassion weighs the distance by 0.02: 250 m off is still worth 0.98 of itself
	constexpr float k_Weight = 0.02f;
	const std::vector<ObjectCandidate> far {{.id = 1, .distance = 250.0f, .usefulness = 0.1f}};
	EXPECT_EQ(creature_planner::BestActOn(far, k_Weight), 0u);
	// the best for the distance, the first of equals
	const std::vector<ObjectCandidate> two {{.id = 1, .distance = 50.0f, .usefulness = 0.1f},
	                                        {.id = 2, .distance = 50.0f, .usefulness = 0.1f},
	                                        {.id = 3, .distance = 10.0f, .usefulness = 0.05f}};
	EXPECT_EQ(creature_planner::BestActOn(two, k_Weight), 0u);
	// nothing worth nothing, nor out of reach of a creature kept within an area
	const std::vector<ObjectCandidate> none {{.id = 1, .distance = 10.0f, .usefulness = 0.0f},
	                                         {.id = 2, .distance = 10.0f, .usefulness = 0.1f, .outOfReach = true}};
	EXPECT_FALSE(creature_planner::BestActOn(none, k_Weight).has_value());
	EXPECT_FALSE(creature_planner::BestActOn({}, k_Weight).has_value());
}

TEST(CreatureTownCompassion, WhatACreatureMakesOfATown)
{
	EXPECT_EQ(town_compassion::ReligiousBelief(0.19f), 0u);
	EXPECT_EQ(town_compassion::ReligiousBelief(0.2f), 1u);
	EXPECT_EQ(town_compassion::ReligiousBelief(0.5f), 2u);
	EXPECT_EQ(town_compassion::ReligiousBelief(0.6f), 3u);
	EXPECT_EQ(town_compassion::ReligiousBelief(9.0f), 3u);
	EXPECT_EQ(town_compassion::NeedsMost(4), 4u);
	EXPECT_EQ(town_compassion::NeedsMost(-1), 16u);
	EXPECT_EQ(town_compassion::TownSize(19), 0u);
	EXPECT_EQ(town_compassion::TownSize(20), 1u);
	EXPECT_EQ(town_compassion::TownSize(40), 2u);
}
