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
	/// How useful the creature has learnt it is to do the desire's actions to
	float usefulness {k_DefaultUsefulness};
	/// How useful the creature has learnt it is as what a plan for the desire is for, its goal
	float goalUsefulness {k_DefaultUsefulness};
	/// The creature is kept within an area and the thing lies farther than its radius: it is worth nothing
	bool outOfReach {false};
	/// How many times the desire's worth of the thing goes, for what the thing is (ActOnFactor)
	float actOnFactor {1.0f};
};

/// What play and curiosity look at in a thing to act on
struct ActOnFacts
{
	/// A villager in a football match
	bool playingFootball {false};
	/// A toy: the football, a toy ball, a bowling ball, a cuddly toy, a dice or a skittle
	bool toy {false};
	/// A villager doing something the creature could learn by watching and has not learnt yet
	bool doingSomethingNew {false};
	/// An object of a kind the creature has already looked over
	bool kindLookedOver {false};
};
/// How many times a desire's worth of a thing to act on goes, for what the thing is: play takes a villager playing
/// football at five times and a toy at three; curiosity takes a villager doing something new to it at five times, and
/// an object of a kind it has already looked over at a hundredth; every other desire, and every other thing, as it is
[[nodiscard]] float ActOnFactor(creature_desires::Desire desire, const ActOnFacts& facts);
/// Whether a thing lies beyond the area a creature is kept within, where it is worth nothing to act on: farther from the
/// creature than the radius, unless the desire is to hang around at home and the thing is part of a temple. A distance
/// that is no number counts as within it
[[nodiscard]] bool BeyondKeptArea(creature_desires::Desire desire, std::optional<float> keptWithin, float distance,
                                  bool templePart);
/// How useful a thing is to act on: what the creature has learnt of it for the desire, times how the distance weighs
/// against it (DistancePriority), times the desire's factor for the thing, in that order; nothing out of reach
[[nodiscard]] float ActOnUsefulness(const ObjectCandidate& object, float distanceWeight);

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
	/// How much what it is done to counted for in the priority
	float objectUsefulness {k_DefaultUsefulness};
};
/// Whether a plan the creature holds can be carried out as it stands, as creature_learning::PlanIsValid judges a
/// remembered one: an action, what it is for when its desire looks for a goal (compassion's town; for the others the
/// thing it is done to), and what it is done with when the action needs that. Every plan is done to something (the
/// creature itself when it names nothing), and every desire of the planner is one of the creature's
[[nodiscard]] bool IsValid(const Plan& plan, bool searchesForGoal, bool needsInstrument);

/// A plan forced on the creature, by a reaction, scores the most a score can be, so that no plan of its own is ever
/// pressing enough to replace it
constexpr float k_ForcedScore = std::numeric_limits<float>::max();
/// The game's action that a creature under a script's control obeys with, following the player, by its row
constexpr uint32_t k_FollowPlayerAction = 22;

/// The thing a plan for a town is done to: of the town's own buildings and people, the one most useful to act on
/// (ActOnUsefulness), with no limit on how far; nothing out of reach, and nothing worth nothing; the first of equals
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
	/// The goal is itself what the plan is done to, and counts for how useful it is to act on as well; otherwise what the
	/// plan is done to counts as a thing nothing is known about
	bool actsOnGoal {false};
};
/// The best plan for a desire of a strength: the goal by distance and its usefulness as a goal, then the action most
/// wanted
[[nodiscard]] std::optional<Plan> PlanDesire(creature_desires::Desire desire, float strength,
                                             std::span<const ObjectCandidate> objects, float distanceWeight,
                                             std::span<const ActionCandidate> actions, const GoalSearch& search = {});

/// The thing to use for a planned action, among those it may use, never the goal itself: the one the distance weighs
/// least against (something held most of all), the first of equals; none when there is none, or every one is too far to
/// count
[[nodiscard]] std::optional<uint32_t> ChooseUsed(std::span<const ObjectCandidate> objects, float distanceWeight,
                                                 std::optional<uint32_t> goal);

/// An action of a desire with no goal, in the desire's list, and the things it could be done to
struct ActionChoice
{
	ActionCandidate action;
	std::span<const ObjectCandidate> objects;
};
/// The best plan for a desire that looks for no goal: the action most wanted first (the last of equals), then the
/// thing most useful to act on (ActOnUsefulness, by the desire's distance weight), worth more than nothing (the first of
/// equals). An action with nothing to do it to is passed over for the next most wanted, up to a number of tries. What
/// it is done to counts for that usefulness, and the goal as a thing nothing is known about. An action that is done to
/// no thing is done to the creature itself, given by its entity's number, as a thing nothing is known about; with none
/// given, to nothing.
[[nodiscard]] std::optional<Plan> PlanWithoutGoal(creature_desires::Desire desire, float strength,
                                                  std::span<const ActionChoice> choices, float distanceWeight,
                                                  std::optional<uint32_t> itself = std::nullopt);

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
	/// Its player has not looked at it for longer than it takes to feel ignored
	bool ignoredByPlayer {false};
};
/// Whether a desire may be planned now: wanderlust not once grown past the young stages and worn out, getting warmer
/// only when cold, getting colder only when hot, running away from the player only when it dislikes the player,
/// hanging around at home only near it, attracting the player's attention only when ignored; every other desire
/// always
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
	/// The thing that the last plan acted on. It stays after the plan stops, until the creature starts a new plan.
	std::optional<uint32_t> lastObject;
	/// The action of the last plan, kept as its thing is. Another creature reads the desire that it shows from it.
	std::optional<uint32_t> lastAction;
};
/// The plan carried out stops: the thing it acted on and its action are kept as the last plan's
void StopPlan(PlannerState& state);
/// The thing that the creature's plan acts on, or after the plan stops the thing of the last plan, until a new plan
[[nodiscard]] std::optional<uint32_t> TargetObject(const PlannerState& state);
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
/// From now on, the plan carried out acts on the creature itself rather than on the thing it chose. Nothing when there
/// is no plan
void ActOnItself(PlannerState& state, uint32_t self);
/// The plan carried out uses the creature itself from now on, rather than the thing it chose to use
void UseItself(PlannerState& state, uint32_t self);

/// One desire that a creature weighs for one thing
struct DesireOnThing
{
	creature_desires::Desire desire {creature_desires::Desire::Impress};
	/// The strength of the desire. The usefulness of the thing for the desire, with its distance included.
	float strength {0.0f};
	float usefulness {0.0f};
};
/// The desire that the creature most wants to do about one thing, the desires weighed in their order. The score of a
/// desire is its strength times the usefulness of the thing. A desire becomes the best when its score is more than the
/// usefulness of the best desire before it: that comparison leaves out the strength of the best desire. With no score
/// above nothing, there is no best desire.
[[nodiscard]] std::optional<creature_desires::Desire> BestDesireOnThing(std::span<const DesireOnThing> desires);

} // namespace openblack::creature_planner
