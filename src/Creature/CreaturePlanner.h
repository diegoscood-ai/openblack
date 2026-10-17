/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "Creature/CreatureDesires.h"

/// How a creature decides what to do. Each desire it has gets a plan: the thing to act on (the goal), and the action to
/// take. The goal is the thing the creature finds most useful for the desire, by what its decision trees have learnt,
/// the nearer the better. Among the actions that satisfy the desire, its opinion of each and how long since it last did
/// it break the ties. A plan's priority is the desire's strength times how useful the goal and the action are. Two
/// desires are planned again each game turn, and the creature changes what it is doing only for a plan more than twice
/// as pressing as the one it carries out.
namespace openblack::creature_planner
{
/// The farthest a goal counts as being, and how much a held thing is preferred
constexpr float k_MaxGoalDistance = 200.0f;
constexpr float k_HeldPriority = 1.6f;
/// How much a thing nothing is known about is worth, and the most an action's priority counts for in a plan
constexpr float k_DefaultUsefulness = 0.1f;
constexpr float k_MaxActionPriority = 0.01f;
/// Scales priorities to readable numbers; it doesn't change which plan wins
constexpr float k_PriorityScale = 1e5f;
/// How long since an action was done before it is fully new again, in game turns, and the most novelty adds
constexpr uint32_t k_NoveltyTurns = 36000;
constexpr float k_MaxNovelty = 0.01f;
/// Desires planned each game turn
constexpr size_t k_GoalsPerTurn = 2;
/// The most actions a desire with no goal tries before it gives up
constexpr size_t k_ActionAttempts = 30;

/// How the distance to a goal weighs against it: 1.6 for something held, else less the further it is, by the desire's
/// distance weight, up to 200 away. The distance is the game's in metres (gutils::GetDistanceInMetres)
[[nodiscard]] float DistancePriority(bool held, float distance, float distanceWeight);
/// How much the creature wants to take an action, by its opinion of it from -1 to 1; nothing for an action it can't
/// take now
[[nodiscard]] float ActionPriority(float opinion, bool disabled = false);
/// A little extra for an action not done for a long time
[[nodiscard]] float Novelty(uint32_t turnsSinceDone, bool servesFirstDesire);
/// A plan's priority from its parts: the desire's strength, the goal's usefulness (or none), the action's priority (of
/// which at most a hundredth counts), and the usefulness of what is acted on and of what is used (or none), multiplied
/// in the game's order so that it rounds as the game's does
[[nodiscard]] float Priority(float desire, std::optional<float> goalUsefulness, float actionPriority, float objectUsefulness,
                             std::optional<float> usedUsefulness);
/// Whether a plan is pressing enough to change to: more than twice the priority of what the creature carries out
[[nodiscard]] bool ShouldSwitch(float best, float current);

/// Something a desire could be satisfied with
struct ObjectCandidate
{
	/// The thing's entity number
	uint32_t id {0};
	float distance {0.0f};
	bool held {false};
	/// How useful the creature has learnt it is for the desire
	float usefulness {k_DefaultUsefulness};
	/// The creature is kept within an area and the thing lies farther than its radius: it is worth nothing
	bool outOfReach {false};
};

/// An action that satisfies a desire, by its row in the game's action table
struct ActionCandidate
{
	uint32_t action {0};
	/// Whether it acts on a thing, so needs a goal
	bool needsObject {false};
	float opinion {0.0f};
	uint32_t turnsSinceDone {k_NoveltyTurns};
	/// Whether the table says it serves the first desire, which the game counts for less novelty
	bool servesFirstDesire {false};
	bool disabled {false};
};

struct Plan
{
	creature_desires::Desire desire {creature_desires::Desire::Impress};
	uint32_t action {0};
	std::optional<uint32_t> object;
	float goalUsefulness {k_DefaultUsefulness};
	float actionPriority {0.0f};
	float priority {0.0f};
	/// What the plan is done for (the town of a burning house, say), and the thing it is done with, by their numbers
	std::optional<uint32_t> activityObject;
	std::optional<uint32_t> instrument;
	/// Forced on the creature rather than chosen by it: carrying it out leaves its desires as they were
	bool forced {false};
};
/// A plan forced on the creature, by a reaction, scores the most a score can be, so that no plan of its own is ever
/// pressing enough to replace it
constexpr float k_ForcedScore = std::numeric_limits<float>::max();
/// The game's action that a creature under a script's control obeys with, following the player, by its row
constexpr uint32_t k_FollowPlayerAction = 22;

/// The thing a plan for a town is done to: of the town's own buildings and people, the one most worth it for the
/// distance, with no limit on how far; nothing out of reach, and nothing worth nothing; the first of equals
[[nodiscard]] std::optional<size_t> BestActOn(std::span<const ObjectCandidate> members, float distanceWeight);
/// Whether a plan takes over from the best so far for the same desire: more pressing, or as pressing with an action
/// later in the desire's list, as one walk down the list that keeps the last of equals would have it
[[nodiscard]] bool Replaces(const Plan& plan, const Plan& best, std::span<const uint32_t> actions);
/// How a desire's plan finds its goal
struct GoalSearch
{
	/// The desire looks for a goal before its action: the goal counts for its worth for the distance, and without one
	/// there is no plan. Otherwise the goal counts as a thing nothing is known about
	bool searches {false};
	/// The goal's worth when the goal was found beforehand (the town compassion is about), not among the things
	std::optional<float> foundWorth;
	/// The goal is itself what the plan is done to, and counts for its usefulness as that; otherwise what the plan is
	/// done to counts as a thing nothing is known about
	bool actsOnGoal {false};
};
/// The best plan for a desire of a strength: the goal by distance and usefulness, then the action most wanted
[[nodiscard]] std::optional<Plan> PlanDesire(creature_desires::Desire desire, float strength,
                                             std::span<const ObjectCandidate> objects, float distanceWeight,
                                             std::span<const ActionCandidate> actions, const GoalSearch& search = {});

/// An action of a desire with no goal, in the desire's list, and the things it could be done to
struct ActionChoice
{
	ActionCandidate action;
	std::span<const ObjectCandidate> objects;
};
/// The best plan for a desire that looks for no goal: the action most wanted first (the last of equals), then the
/// thing most useful to do it to, however far, worth more than nothing (the first of equals). An action with nothing to
/// do it to is passed over for the next most wanted, up to a number of tries. What it is done to counts for its
/// usefulness, and the goal as a thing nothing is known about
[[nodiscard]] std::optional<Plan> PlanWithoutGoal(creature_desires::Desire desire, float strength,
                                                  std::span<const ActionChoice> choices);

/// What some desires look at to tell whether they may be planned now
struct PlanFacts
{
	uint32_t developmentPhase {0};
	float exhaustion {0.0f};
	/// From -1 (cold) to 1 (hot)
	float warmth {0.0f};
	float attitudeToPlayer {0.0f};
	/// How far it is from its home, in the game's metres, when it has one
	std::optional<float> metresFromHome;
};
/// Whether a desire may be planned now: wanderlust not once grown past the young stages and worn out, getting warmer
/// only when cold, getting colder only when hot, running away from the player only when it dislikes the player,
/// hanging around at home only near it; every other desire always
[[nodiscard]] bool MayPlan(creature_desires::Desire desire, const PlanFacts& facts);

/// What the planner holds between turns
struct PlannerState
{
	/// The best plan found for each desire, the last time it was planned
	std::array<std::optional<Plan>, creature_desires::k_DesireCount> best {};
	/// The desires still to plan in this pass over them, the next one last
	std::vector<creature_desires::Desire> remaining;
	/// The plan carried out
	std::optional<Plan> current;
};
/// A new pass over the desires that may be planned, the first desire first
void StartPass(PlannerState& state, const std::array<bool, creature_desires::k_DesireCount>& eligible);
/// The desires a pass goes over: those activated and not held back, whatever their strength or actions
[[nodiscard]] std::array<bool, creature_desires::k_DesireCount> PassDesires(const creature_desires::Desires& desires);
/// The next desires of the pass to plan this turn
[[nodiscard]] std::vector<creature_desires::Desire> NextGoals(PlannerState& state, size_t count = k_GoalsPerTurn);
/// Whether the pass is over, when the planner decides what to do and starts the next
[[nodiscard]] bool PassOver(const PlannerState& state);
/// The most pressing plan of all the desires', if any is above nothing; the first desire's of equals
[[nodiscard]] std::optional<Plan> Best(const PlannerState& state);
/// The plan to change to now, if the best is pressing enough against what the creature carries out (nothing counts as 0)
[[nodiscard]] std::optional<Plan> Choose(const PlannerState& state);

} // namespace openblack::creature_planner
