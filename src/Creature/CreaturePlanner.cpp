/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreaturePlanner.h"

#include <algorithm>

using namespace openblack;
using namespace openblack::creature_planner;
using creature_desires::Desire;

namespace
{
/// The action's base priority, and how much each step of opinion adds
constexpr float k_BaseActionPriority = 0.5f;
constexpr float k_OpinionWeight = 0.0025f;
constexpr float k_NoveltyGrowth = 4.0f;
constexpr float k_FirstDesireNovelty = 0.025f;
/// The stage of growing up after which wanderlust gives way to being worn out, and how worn out
constexpr uint32_t k_WanderlustPhase = 5;
constexpr float k_TooWornOutToWander = 0.8f;
/// How cold or hot it must feel to want to get warmer or colder
constexpr float k_FeltWarmth = 0.3f;
/// How near home hanging around at home is planned
constexpr float k_NearHomeMetres = 60.0f;
} // namespace

float creature_planner::DistancePriority(bool held, float distance, float distanceWeight)
{
	if (held)
	{
		return k_HeldPriority;
	}
	// Beyond the farthest a goal counts, the whole weight; a distance that is no number counts as within it
	if (distance > k_MaxGoalDistance)
	{
		return 1.0f - distanceWeight;
	}
	return 1.0f - distanceWeight * distance / k_MaxGoalDistance;
}

float creature_planner::ActionPriority(float opinion, bool disabled)
{
	return disabled ? 0.0f : k_BaseActionPriority + k_OpinionWeight * (opinion + 1.0f);
}

float creature_planner::Novelty(uint32_t turnsSinceDone, bool servesFirstDesire)
{
	const auto share = static_cast<float>(std::min(turnsSinceDone, k_NoveltyTurns)) / static_cast<float>(k_NoveltyTurns);
	return std::clamp(k_NoveltyGrowth * share * (servesFirstDesire ? k_FirstDesireNovelty : 1.0f), 0.0f, k_MaxNovelty);
}

float creature_planner::Priority(float desire, std::optional<float> goalUsefulness, float actionPriority,
                                 float objectUsefulness, std::optional<float> usedUsefulness)
{
	// In the game's order, each step rounded to a float: the action's share times what it is done to, times the
	// desire's strength times the goal (rounded on its own first), times the scale, times what it is done with
	const float desireForGoal = desire * goalUsefulness.value_or(k_DefaultUsefulness);
	return std::min(actionPriority, k_MaxActionPriority) * objectUsefulness * desireForGoal * k_PriorityScale *
	       usedUsefulness.value_or(k_DefaultUsefulness);
}

bool creature_planner::ShouldSwitch(float best, float current)
{
	return best - current > current;
}

std::optional<size_t> creature_planner::BestActOn(std::span<const ObjectCandidate> members, float distanceWeight)
{
	std::optional<size_t> best;
	float bestScore = 0.0f;
	for (size_t i = 0; i < members.size(); ++i)
	{
		const auto& member = members[i];
		const float score =
		    member.outOfReach ? 0.0f : DistancePriority(member.held, member.distance, distanceWeight) * member.usefulness;
		// strictly more than the best so far, which starts at nothing
		if (score > bestScore)
		{
			best = i;
			bestScore = score;
		}
	}
	return best;
}

bool creature_planner::Replaces(const Plan& plan, const Plan& best, std::span<const uint32_t> actions)
{
	if (plan.priority != best.priority)
	{
		return plan.priority > best.priority;
	}
	// The last place each action has in the list, as an action listed twice is last weighed there
	const auto lastPlace = [actions](uint32_t action) -> std::optional<size_t> {
		for (size_t i = actions.size(); i > 0; --i)
		{
			if (actions[i - 1] == action)
			{
				return i - 1;
			}
		}
		return std::nullopt;
	};
	return lastPlace(plan.action) > lastPlace(best.action);
}

std::optional<Plan> creature_planner::PlanDesire(Desire desire, float strength, std::span<const ObjectCandidate> objects,
                                                 float distanceWeight, std::span<const ActionCandidate> actions,
                                                 const GoalSearch& search)
{
	// The goal: the thing that is most useful for the distance; one that looks for its goal takes only one worth more
	// than nothing
	const ObjectCandidate* goal = nullptr;
	float goalScore = 0.0f;
	for (const auto& object : objects)
	{
		const auto score = DistancePriority(object.held, object.distance, distanceWeight) * object.usefulness;
		if ((goal == nullptr && !search.searches) || score > goalScore)
		{
			goal = &object;
			goalScore = score;
		}
	}
	auto goalWorth = search.foundWorth;
	if (!goalWorth.has_value() && search.searches && goal != nullptr)
	{
		goalWorth = goalScore;
	}
	if (search.searches && !goalWorth.has_value())
	{
		return std::nullopt;
	}
	// The action: the one most wanted, which nothing else is used for here
	const ActionCandidate* chosen = nullptr;
	float chosenPriority = 0.0f;
	float chosenScore = -1.0f;
	for (const auto& action : actions)
	{
		if (action.disabled || (action.needsObject && goal == nullptr))
		{
			continue;
		}
		const auto priority = ActionPriority(action.opinion) + Novelty(action.turnsSinceDone, action.servesFirstDesire);
		const auto score = k_DefaultUsefulness * k_DefaultUsefulness * priority;
		if (score >= chosenScore)
		{
			chosen = &action;
			chosenPriority = priority;
			chosenScore = score;
		}
	}
	if (chosen == nullptr)
	{
		return std::nullopt;
	}
	Plan plan {.desire = desire, .action = chosen->action, .actionPriority = chosenPriority};
	if (chosen->needsObject && goal != nullptr)
	{
		plan.object = goal->id;
	}
	plan.goalUsefulness = goalWorth.value_or(k_DefaultUsefulness);
	const auto objectUsefulness = search.actsOnGoal && goal != nullptr ? goal->usefulness : k_DefaultUsefulness;
	plan.priority = Priority(strength, goalWorth, chosenPriority, objectUsefulness, std::nullopt);
	return plan;
}

std::optional<Plan> creature_planner::PlanWithoutGoal(Desire desire, float strength, std::span<const ActionChoice> choices)
{
	std::vector<bool> passedOver(choices.size(), false);
	for (size_t attempt = 0; attempt < k_ActionAttempts; ++attempt)
	{
		// The action: the one most wanted, a later one as wanted taking over; one wanted by no number never is
		std::optional<size_t> chosen;
		float chosenPriority = 0.0f;
		for (size_t i = 0; i < choices.size(); ++i)
		{
			const auto& action = choices[i].action;
			if (action.disabled || passedOver[i])
			{
				continue;
			}
			const auto priority = ActionPriority(action.opinion) + Novelty(action.turnsSinceDone, action.servesFirstDesire);
			if (priority >= chosenPriority)
			{
				chosen = i;
				chosenPriority = priority;
			}
		}
		if (!chosen.has_value())
		{
			return std::nullopt;
		}
		const auto& choice = choices[*chosen];
		Plan plan {.desire = desire, .action = choice.action.action, .actionPriority = chosenPriority};
		float objectUsefulness = k_DefaultUsefulness;
		if (choice.action.needsObject)
		{
			// The thing: the most useful, the first of equals, nothing out of reach
			const ObjectCandidate* object = nullptr;
			float best = 0.0f;
			for (const auto& candidate : choice.objects)
			{
				const auto usefulness = candidate.outOfReach ? 0.0f : candidate.usefulness;
				if (usefulness > best)
				{
					object = &candidate;
					best = usefulness;
				}
			}
			if (object == nullptr)
			{
				passedOver.at(*chosen) = true;
				continue;
			}
			plan.object = object->id;
			objectUsefulness = best;
		}
		plan.priority = Priority(strength, std::nullopt, chosenPriority, objectUsefulness, std::nullopt);
		return plan;
	}
	return std::nullopt;
}

bool creature_planner::MayPlan(Desire desire, const PlanFacts& facts)
{
	// Each test is written as the game makes it, so that a value that is no number goes the same way
	switch (desire)
	{
	case Desire::Wanderlust:
		return !(facts.developmentPhase > k_WanderlustPhase && facts.exhaustion >= k_TooWornOutToWander);
	case Desire::GetWarmer:
		return !(facts.warmth > -k_FeltWarmth);
	case Desire::GetColder:
		return facts.warmth >= k_FeltWarmth;
	case Desire::RunAwayFromPlayer:
		return !(facts.attitudeToPlayer >= 0.0f);
	case Desire::HangAroundAtHome:
		return !facts.metresFromHome.has_value() || !(*facts.metresFromHome >= k_NearHomeMetres);
	default:
		return true;
	}
}

void creature_planner::StartPass(PlannerState& state, const std::array<bool, creature_desires::k_DesireCount>& eligible)
{
	state.remaining.clear();
	for (size_t d = eligible.size(); d > 0; --d)
	{
		if (eligible.at(d - 1))
		{
			state.remaining.push_back(static_cast<Desire>(d - 1));
		}
	}
}

std::array<bool, creature_desires::k_DesireCount> creature_planner::PassDesires(const creature_desires::Desires& desires)
{
	std::array<bool, creature_desires::k_DesireCount> listed {};
	for (size_t d = 0; d < listed.size(); ++d)
	{
		const auto& state = desires.desires.at(d);
		listed.at(d) = state.activated && state.suppressedTurns == 0;
	}
	return listed;
}

std::vector<Desire> creature_planner::NextGoals(PlannerState& state, size_t count)
{
	std::vector<Desire> goals;
	while (goals.size() < count && !state.remaining.empty())
	{
		goals.push_back(state.remaining.back());
		state.remaining.pop_back();
	}
	return goals;
}

bool creature_planner::PassOver(const PlannerState& state)
{
	return state.remaining.empty();
}

std::optional<Plan> creature_planner::Best(const PlannerState& state)
{
	std::optional<Plan> best;
	float bestPriority = 0.0f;
	for (const auto& plan : state.best)
	{
		// Taken unless it is no more than the best so far, which starts at nothing
		if (plan.has_value() && !(plan->priority <= bestPriority))
		{
			best = plan;
			bestPriority = plan->priority;
		}
	}
	return best;
}

std::optional<Plan> creature_planner::Choose(const PlannerState& state)
{
	const auto best = Best(state);
	if (!best.has_value())
	{
		return std::nullopt;
	}
	const auto current = state.current.has_value() ? state.current->priority : 0.0f;
	return ShouldSwitch(best->priority, current) ? best : std::nullopt;
}
