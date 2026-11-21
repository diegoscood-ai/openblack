/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCatch.h"

#include <cmath>

#include <algorithm>

#include <glm/mat3x3.hpp>

namespace openblack::creature_catch
{
namespace
{
/// The heading's sine and minus its cosine, each kept as a float, as the game keeps them for ahead
struct Facing
{
	float sine;
	float minusCosine;
};
Facing FacingOf(float heading)
{
	return {.sine = static_cast<float>(std::sin(static_cast<double>(heading))),
	        .minusCosine = -static_cast<float>(std::cos(static_cast<double>(heading)))};
}
/// A thing's offset from the creature along a facing, across the land
float Along(const Facing& facing, glm::vec3 thing, glm::vec3 creature)
{
	const float dx = thing.x - creature.x;
	const float dz = thing.z - creature.z;
	return (facing.minusCosine * dz) + (dx * facing.sine);
}
} // namespace

float AheadOf(glm::vec3 thing, const Stance& creature)
{
	return Along(FacingOf(creature.heading), thing, creature.position);
}

float RightOf(glm::vec3 thing, const Stance& creature)
{
	const float angle = creature.heading - k_QuarterTurn;
	const auto sine = static_cast<float>(std::sin(static_cast<double>(angle)));
	// The cosine is not rounded to a float before it is multiplied, as the game leaves it
	const double minusCosine = -std::cos(static_cast<double>(angle));
	const float dx = thing.x - creature.position.x;
	const float dz = thing.z - creature.position.z;
	return static_cast<float>(minusCosine * static_cast<double>(dz)) + (dx * sine);
}

bool Reaches(const Approach& approach)
{
	const float vx = approach.velocity.x;
	const float vz = approach.velocity.z;
	const float speedSquared = (vz * vz) + (vx * vx);
	if (speedSquared < k_LeastSpeedSquared)
	{
		return false;
	}
	// When it passes closest across the land: along the way back against its flight, over its speed
	const float speed = std::sqrt(speedSquared);
	const float back = -1.0f / speed;
	const float backX = back * vx;
	const float backZ = back * vz;
	const float dx = approach.thing.x - approach.creature.x;
	const float dz = approach.thing.z - approach.creature.z;
	const float seconds = ((backZ * dz) + (dx * backX)) / speed;
	if (seconds > k_MostSeconds)
	{
		return false;
	}
	// The creature's animations play faster the smaller it is
	const float rate = 1.6f - ((approach.size * 0.5f) * 0.85f);
	// How far from it the thing is then, across the land
	const float missX = ((vx * seconds) + approach.thing.x) - approach.creature.x;
	const float missZ = ((vz * seconds) + approach.thing.z) - approach.creature.z;
	const float msPerSecond = rate * 1000.0f;
	const float spare = seconds - (approach.catchMs / msPerSecond);
	if (spare < k_LeastSpare)
	{
		return false;
	}
	const float step = approach.stepMs / msPerSecond;
	const float miss = std::sqrt((missZ * missZ) + (missX * missX));
	const float pace = -(approach.stepTravel * approach.modelScale) / step;
	const float reach = ((step * 0.5f) + spare) * pace;
	return reach > miss;
}

bool MayTry(const Eligibility& creature)
{
	if (creature.heldByHand || creature.plansToFight || creature.fighting || creature.scriptControlled)
	{
		return false;
	}
	if (!creature.availableForReaction && !creature.onlyDesire)
	{
		return false;
	}
	return !creature.plansToCatch;
}

float MostWeight(float creatureWeight)
{
	return creatureWeight * k_MostWeightShare;
}

bool GoesForOtherPlayersThrow(uint32_t draw)
{
	return draw <= k_OtherPlayersChance;
}

bool FastEnoughToStart(glm::vec3 velocity)
{
	return (velocity.x * velocity.x) + (velocity.y * velocity.y) >= k_LeastSpeedSquared;
}

Blend Weigh(glm::vec3 thing, const Stance& creature, const std::array<glm::vec3, 4>& hands, float modelScale, bool mirrored)
{
	// The hand at the moment it closes: low and to one side, low and to the other, high and to one side, high and to the
	// other
	const auto& lowOther = hands[1];
	const auto& highSide = hands[2];
	const auto& highOther = hands[3];
	// How high, over the creature's feet, between the low and the high corner's hands
	const float lowY = lowOther.y * modelScale + creature.position.y;
	const float highY = highOther.y * modelScale + creature.position.y;
	float height = (thing.y - lowY) / (highY - lowY);
	// How far along its right, turned round for the left hand, between the two high corners' hands
	float side = RightOf(thing, creature);
	if (mirrored)
	{
		side = -side;
	}
	const float rightX = -(highOther.x * modelScale);
	const float leftX = -(highSide.x * modelScale);
	side = (side - leftX) / (rightX - leftX);
	// Each is kept within the limits; one that can't be measured is kept at the lower
	const auto keep = [](float& weight) {
		bool clamped = false;
		if (!(weight >= k_LeastWeight))
		{
			clamped = true;
			weight = k_LeastWeight;
		}
		if (weight > k_MostWeight)
		{
			clamped = true;
			weight = k_MostWeight;
		}
		return clamped;
	};
	// Both weights are always kept, so neither test is skipped.
	const bool heightClamped = keep(height);
	const bool sideClamped = keep(side);
	const bool clamped = heightClamped || sideClamped;
	return {.weights = {(1.0f - height) * (1.0f - side), (1.0f - height) * side, height * (1.0f - side), height * side},
	        .clamped = clamped,
	        .height = height,
	        .side = side};
}

Blend WeighWithout(float lastHeight)
{
	const float height = std::clamp(lastHeight, k_LeastWeight, k_MostWeight);
	constexpr float k_Middle = 0.5f;
	return {.weights = {(1.0f - height) * (1.0f - k_Middle), (1.0f - height) * k_Middle, height * (1.0f - k_Middle),
	                    height * k_Middle},
	        .clamped = height != lastHeight,
	        .height = height};
}

Ready ReadyToCatch(glm::vec3 thing, glm::vec3 velocity, const Stance& creature, const std::array<glm::vec3, 4>& hands,
                   float modelScale, float leadSeconds)
{
	// It must be in front of the creature; a place that can't be measured gives up too
	const auto facing = FacingOf(creature.heading);
	const float ahead = Along(facing, thing, creature.position);
	if (!(ahead >= 0.0f))
	{
		return {.readiness = Readiness::GiveUp, .mirrored = false};
	}
	const float side = RightOf(thing, creature);
	const auto& highSide = hands[2];
	const auto& highOther = hands[3];
	const float reach = -((((highOther.x - highSide.x) * k_ReachBeyondHand) + highSide.x) * modelScale);
	// The reach is measured to the far side: a reach that comes out on the near side always takes the step, and so does a
	// side that can't be measured
	if (!(reach >= std::abs(side)))
	{
		return {.readiness = Readiness::Step, .mirrored = !(side > 0.0f)};
	}
	// When it arrives, coming at the creature along its front
	const float closing = ((-velocity.z) * facing.minusCosine) + ((-velocity.x) * facing.sine);
	// With no closing speed the thing never arrives and the creature waits, unless it is already there, when it can't
	// tell and gives up
	const float arrives = ahead / closing;
	if (arrives >= leadSeconds + k_LateArrival)
	{
		return {.readiness = Readiness::Wait, .mirrored = false};
	}
	if (arrives > leadSeconds - k_EarlyArrival)
	{
		return {.readiness = Readiness::Catch, .mirrored = !(side >= 0.0f)};
	}
	return {.readiness = Readiness::GiveUp, .mirrored = false};
}

CatchLook LookWhileCatching(bool reaching, const std::optional<glm::vec3>& kept)
{
	return {.head = reaching ? std::nullopt : kept, .eyes = kept};
}

CatchLook LookUnderTheHand(bool reaching, bool handCloses, const CatchLook& hand)
{
	return {.head = reaching ? std::nullopt : hand.head, .eyes = handCloses ? std::nullopt : hand.eyes};
}

float LeadSeconds(float catchMs, float size)
{
	const float half = size * 0.5f;
	const float rate = 1.6f - (half * 0.85f);
	return catchMs / (rate * 1000.0f);
}

int32_t AdvanceCyclic(int32_t timeMs, int32_t stepMs, int32_t durationMs)
{
	if (stepMs == 0)
	{
		return timeMs;
	}
	int32_t time = timeMs + stepMs;
	if (time >= durationMs)
	{
		time -= durationMs;
	}
	return time >= durationMs ? durationMs - 1 : time;
}

bool StepEnds(int32_t timeMs, int32_t stepMs, int32_t durationMs)
{
	return timeMs + stepMs >= durationMs;
}

bool CatchEnds(bool drawnBack, int32_t timeMs, int32_t stepMs, int32_t durationMs)
{
	return drawnBack ? timeMs < stepMs : timeMs + stepMs >= durationMs;
}

int32_t CatchClock(bool drawnBack, int32_t timeMs, int32_t stepMs, int32_t durationMs)
{
	return drawnBack ? std::max(0, timeMs - stepMs) : AdvanceCyclic(timeMs, stepMs, durationMs);
}

glm::vec3 StepMove(glm::vec3 travel, const glm::mat3& rotation, float modelScale, float stepMs, float durationMs, bool mirrored)
{
	const float share = stepMs * modelScale / durationMs;
	const auto move = rotation * (travel * share);
	return mirrored ? -move : move;
}
} // namespace openblack::creature_catch
