/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <span>

#include "Enums.h"

// How a living being reacts to a predator nearby: how urgently a villager runs, how long the reaction lasts, where it
// hides and what it does next. Pure functions: the random draws, the distances and the predator's state come in as
// arguments, so the caller keeps the order of the draws. The animals' side is in ECS/AnimalFlee.cpp.

namespace openblack::ecs::effects::flee
{
/// A predator in either of these top states (asleep, or hiding in its lair) frightens no villager
inline constexpr uint8_t k_PredatorAsleep = 35;
inline constexpr uint8_t k_PredatorInLair = 51;
/// A villager that has just come out to look whether it is safe ignores a sleeping predator for this many turns
inline constexpr uint16_t k_LookIfSafeSettleTurns = 20;
/// A predator faster than this, in metres per second, is chasing
inline constexpr float k_PredatorChasingSpeed = 5.0f;
/// A villager looks for a building to hide in no further than this, in metres
inline constexpr float k_HideMaxDistance = 100.0f;
/// One draw in this many sends a fleeing villager to hide when the predator is not chasing it
inline constexpr uint32_t k_HideChanceRange = 6;
/// A hidden villager stays in at least this many turns, plus a draw below k_HideRandomTurns
inline constexpr uint32_t k_HideMinTurns = 150;
inline constexpr uint32_t k_HideRandomTurns = 150;
/// A villager that came out looks around at least this many turns, plus a draw below k_LookIfSafeRandomTurns
inline constexpr uint32_t k_LookIfSafeMinTurns = 25;
inline constexpr uint32_t k_LookIfSafeRandomTurns = 25;
/// While hiding or looking around, the reaction to a predator never runs out on its own
inline constexpr uint32_t k_TurnsForever = 0x7FFFFFFF;

/// What a villager's priority for running from a predator depends on
struct VillagerPredatorFacts
{
	/// The reaction's initiator is an animal; nothing else is a predator
	bool initiatorIsAnimal {false};
	/// From the villager to the predator, in metres
	float distance {0.0f};
	/// The reaction info's minDistanceToRunAwayFromObject: further than this, the predator is far away
	float minDistanceToRunAway {0.0f};
	/// The villager is at home
	bool atHome {false};
	/// The villager's top state and the final state its action holds
	VillagerStates topState {VillagerStates::InvalidState};
	VillagerStates storedFinalState {VillagerStates::InvalidState};
	/// The villager's final state as it reports it, and the turns since its state changed
	VillagerStates finalState {VillagerStates::InvalidState};
	uint16_t turnsSinceStateChange {0};
	/// The predator's top state
	uint8_t predatorTopState {0};
	/// The turn the villager last started reacting to a predator (0 never)
	uint32_t turnReactionStarted {0};
	/// The predator is coming at the villager (asked only when it is far away and the villager has run from one before)
	bool predatorComingTowards {false};
	/// The predator's speed in metres per second and its info's whole walking speed
	float predatorSpeed {0.0f};
	int32_t predatorWalkingSpeed {1};
	/// The reaction info's priority for running from a predator
	uint32_t priority {0};
};

/// The priority of running from a predator for a villager, 0 for none: none at home, while hiding or on the way to hide
/// (on the way only when far away), and, unless it has been looking whether it is safe for a while, none for a sleeping
/// or hidden predator nor for a far one it ran from before that is not coming at it. Otherwise the priority grows with
/// the predator's speed over its walking speed: priority x (0.5 + 0.5 x speed / walking), cut to a whole number, of
/// which only the low byte is kept, so a fast predator can wrap round to a small value
[[nodiscard]] uint8_t VillagerFleeFromPredatorPriority(const VillagerPredatorFacts& facts);

/// The common part of how long any living being keeps reacting to a predator: none for an initiator that is not an
/// animal, none while it looks at a predator (top state 7) that chases it and is coming at it; else `standard`, the
/// common turns to react
[[nodiscard]] uint32_t LivingTurnsToReactToPredator(bool initiatorIsAnimal, uint8_t topState, float predatorSpeed,
                                                    bool predatorComingTowards, uint32_t standard);
/// The common part of how long before any living being reacts to a predator again: none for an initiator that is not
/// an animal, for a chasing predator, or for one nearer than `minDistanceToRunAway`. The distance is measured from the
/// living being to the predator, not the reaction's spread distance; `standard` is the common turns before reacting
/// again (which takes the spread distance)
[[nodiscard]] uint32_t LivingTurnsBeforeReactingAgainToPredator(bool initiatorIsAnimal, float predatorSpeed, float distance,
                                                                float minDistanceToRunAway, uint32_t standard);

/// A villager's turns to react to a predator: forever while it hides or looks whether it is safe, else the living
/// being's (`living`, from LivingTurnsToReactToPredator)
[[nodiscard]] uint32_t VillagerTurnsToReactToPredator(VillagerStates finalState, uint32_t living);
/// A villager's turns before reacting to a predator again: none while it looks whether it is safe, else the living
/// being's (`living`, from LivingTurnsBeforeReactingAgainToPredator)
[[nodiscard]] uint32_t VillagerTurnsBeforeReactingAgainToPredator(VillagerStates finalState, uint32_t living);

/// A villager on its way to hide does not take the reaction again: it only starts counting its hidden turns afresh
[[nodiscard]] bool VillagerStartsFleeingFromPredator(VillagerStates finalState);

/// A building of the villager's town, in the town's list order
struct HidingPlace
{
	/// From the villager to the building's arrival point, in metres
	float distance {0.0f};
	/// The building can be hidden in
	bool canHide {false};
};
/// The nearest building that can be hidden in, strictly nearer than k_HideMaxDistance; on a tie the first in the list
[[nodiscard]] std::optional<size_t> NearestHidingPlace(std::span<const HidingPlace> places);

/// What a villager running from a predator does this turn
enum class PredatorFleeStep
{
	/// The predator is out of reach: it stops reacting
	Stop,
	/// It goes back to hide in the building it hid in before
	HideAgain,
	/// It looks for a building to hide in; with none, it runs if the predator is coming at it
	LookForHidingPlace,
	/// One draw below k_HideChanceRange decides: 0 looks for a building to hide in, anything else runs if the predator
	/// is coming at it
	Draw,
};
/// The step of a villager running from an available predator: out of reach (strictly further than
/// `maxDistanceToRunAway`) it stops; back from hiding with its building still there it hides again; chased (the
/// predator faster than k_PredatorChasingSpeed and coming at it) it looks for a building; else a draw decides. The
/// coming-at test is only asked when the predator is chasing
[[nodiscard]] PredatorFleeStep VillagerPredatorFleeStep(float distance, float maxDistanceToRunAway, bool lookingIfSafe,
                                                        bool hidingPlaceStillThere, float predatorSpeed,
                                                        bool predatorComingTowards);

/// What a villager going to hide does this turn
enum class HideStep
{
	/// Its building is gone or can no longer be hidden in: it stops reacting
	Stop,
	/// Not there yet: it walks on to the building
	WalkOn,
	/// Hidden, and the predator is still within the reaction's minimum distance: it stays without counting
	Wait,
	/// Hidden: one more turn counted (HiddenLongEnough)
	Count,
};
/// The step of a villager going to hide: `predatorKnown` when its predator is still there and available
[[nodiscard]] HideStep VillagerHideStep(bool buildingThere, bool arrived, bool predatorKnown, float distance,
                                        float minDistanceToRunAway);
/// After counting `hiddenTurns` (the count after this turn's), it has hidden long enough to come out when the count is
/// above the minimum plus the draw (`draw` below k_HideRandomTurns)
[[nodiscard]] bool HiddenLongEnough(uint32_t hiddenTurns, uint32_t draw);

/// What a villager looking whether it is safe does this turn
enum class LookIfSafeStep
{
	/// The predator is still about: back into its building
	HideAgain,
	/// The predator is still about and its building is gone: it runs if the predator is coming at it
	FleeIfComing,
	/// Safe this turn: one more turn counted (LookedLongEnough)
	Count,
};
/// The step of a villager looking whether it is safe: the predator is about when it is known, an animal, within the
/// reaction's maxReactionDistance and neither asleep nor in its lair
[[nodiscard]] LookIfSafeStep VillagerLookIfSafeStep(bool predatorKnown, bool predatorIsAnimal, float distance,
                                                    float maxReactionDistance, uint8_t predatorTopState,
                                                    bool hidingPlaceStillThere);
/// After counting `lookedTurns`, it stops reacting when the count is above the minimum plus the draw (`draw` below
/// k_LookIfSafeRandomTurns)
[[nodiscard]] bool LookedLongEnough(uint32_t lookedTurns, uint32_t draw);
} // namespace openblack::ecs::effects::flee
