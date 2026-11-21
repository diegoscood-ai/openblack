/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PredatorFlee.h"

using namespace openblack;
using namespace openblack::ecs::effects;

namespace
{
/// The low byte of a float truncated to a 64-bit whole number; out of range (or NaN) the conversion gives the
/// smallest 64-bit number, whose low byte is 0
uint8_t LowByteOfTruncation(float value)
{
	constexpr float k_TwoToThe63 = 9223372036854775808.0f;
	if (!(value >= -k_TwoToThe63 && value < k_TwoToThe63))
	{
		return 0;
	}
	return static_cast<uint8_t>(static_cast<uint64_t>(static_cast<int64_t>(value)) & 0xFFu);
}
} // namespace

uint8_t flee::VillagerFleeFromPredatorPriority(const VillagerPredatorFacts& facts)
{
	if (!facts.initiatorIsAnimal)
	{
		return 0;
	}
	const bool farAway = facts.distance > facts.minDistanceToRunAway;
	if (facts.atHome || facts.topState == VillagerStates::GoAndHideInNearbyBuilding)
	{
		return 0;
	}
	if (facts.storedFinalState == VillagerStates::GoAndHideInNearbyBuilding && farAway)
	{
		return 0;
	}
	// Unless it has looked whether it is safe for a while: a sleeping or hidden predator, or a far one it ran from
	// before that is not coming at it, frightens it no more
	if (facts.finalState != VillagerStates::LookToSeeIfItIsSafe || facts.turnsSinceStateChange < k_LookIfSafeSettleTurns)
	{
		if (facts.predatorTopState == k_PredatorAsleep || facts.predatorTopState == k_PredatorInLair)
		{
			return 0;
		}
		if (farAway && facts.turnReactionStarted > 0 && !facts.predatorComingTowards)
		{
			return 0;
		}
	}
	// Each step rounded to a float, as the original computes it
	float scale = facts.predatorSpeed / static_cast<float>(facts.predatorWalkingSpeed);
	scale *= 0.5f;
	scale += 0.5f;
	const float priority = static_cast<float>(facts.priority) * scale;
	return LowByteOfTruncation(priority);
}

uint32_t flee::LivingTurnsToReactToPredator(bool initiatorIsAnimal, uint8_t topState, float predatorSpeed,
                                            bool predatorComingTowards, uint32_t standard)
{
	if (!initiatorIsAnimal)
	{
		return 0;
	}
	constexpr uint8_t k_LookingAtObject = 7;
	if (topState == k_LookingAtObject && predatorSpeed > k_PredatorChasingSpeed && predatorComingTowards)
	{
		return 0;
	}
	return standard;
}

uint32_t flee::LivingTurnsBeforeReactingAgainToPredator(bool initiatorIsAnimal, float predatorSpeed, float distance,
                                                        float minDistanceToRunAway, uint32_t standard)
{
	// (as the original's comparisons: a NaN speed counts as slow, a NaN distance as near)
	if (!initiatorIsAnimal || predatorSpeed > k_PredatorChasingSpeed || !(distance >= minDistanceToRunAway))
	{
		return 0;
	}
	return standard;
}

uint32_t flee::VillagerTurnsToReactToPredator(VillagerStates finalState, uint32_t living)
{
	if (finalState == VillagerStates::GoAndHideInNearbyBuilding || finalState == VillagerStates::LookToSeeIfItIsSafe)
	{
		return k_TurnsForever;
	}
	return living;
}

uint32_t flee::VillagerTurnsBeforeReactingAgainToPredator(VillagerStates finalState, uint32_t living)
{
	return finalState == VillagerStates::LookToSeeIfItIsSafe ? 0 : living;
}

bool flee::VillagerStartsFleeingFromPredator(VillagerStates finalState)
{
	return finalState != VillagerStates::GoAndHideInNearbyBuilding;
}

std::optional<size_t> flee::NearestHidingPlace(std::span<const HidingPlace> places)
{
	float best = k_HideMaxDistance;
	std::optional<size_t> found;
	for (size_t i = 0; i < places.size(); ++i)
	{
		// (a NaN distance counts as nearer, as the original's comparison)
		if (places[i].canHide && !(places[i].distance >= best))
		{
			best = places[i].distance;
			found = i;
		}
	}
	return found;
}

flee::PredatorFleeStep flee::VillagerPredatorFleeStep(float distance, float maxDistanceToRunAway, bool lookingIfSafe,
                                                      bool hidingPlaceStillThere, float predatorSpeed,
                                                      bool predatorComingTowards)
{
	if (distance > maxDistanceToRunAway)
	{
		return PredatorFleeStep::Stop;
	}
	if (lookingIfSafe && hidingPlaceStillThere)
	{
		return PredatorFleeStep::HideAgain;
	}
	if (predatorSpeed > k_PredatorChasingSpeed && predatorComingTowards)
	{
		return PredatorFleeStep::LookForHidingPlace;
	}
	return PredatorFleeStep::Draw;
}

flee::HideStep flee::VillagerHideStep(bool buildingThere, bool arrived, bool predatorKnown, float distance,
                                      float minDistanceToRunAway)
{
	if (!buildingThere)
	{
		return HideStep::Stop;
	}
	if (!arrived)
	{
		return HideStep::WalkOn;
	}
	if (predatorKnown && !(distance > minDistanceToRunAway))
	{
		return HideStep::Wait;
	}
	return HideStep::Count;
}

bool flee::HiddenLongEnough(uint32_t hiddenTurns, uint32_t draw)
{
	return hiddenTurns > draw + k_HideMinTurns;
}

flee::LookIfSafeStep flee::VillagerLookIfSafeStep(bool predatorKnown, bool predatorIsAnimal, float distance,
                                                  float maxReactionDistance, uint8_t predatorTopState,
                                                  bool hidingPlaceStillThere)
{
	const bool about = predatorKnown && !(distance > maxReactionDistance) && predatorIsAnimal &&
	                   predatorTopState != k_PredatorAsleep && predatorTopState != k_PredatorInLair;
	if (!about)
	{
		return LookIfSafeStep::Count;
	}
	return hidingPlaceStillThere ? LookIfSafeStep::HideAgain : LookIfSafeStep::FleeIfComing;
}

bool flee::LookedLongEnough(uint32_t lookedTurns, uint32_t draw)
{
	return lookedTurns > draw + k_LookIfSafeMinTurns;
}
