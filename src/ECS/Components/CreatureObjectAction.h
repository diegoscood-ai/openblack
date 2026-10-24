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

#include <array>
#include <optional>
#include <string>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureReach.h"

namespace openblack::ecs::components
{

/// What a creature is doing with something, from walking up to it to the end of the animation that acts on it
struct CreatureObjectAction
{
	enum class Phase : uint8_t
	{
		/// Walking up to it or turning to it until it is in reach
		Approach,
		/// Playing the animations
		Playing,
	};
	creature_object_actions::Kind kind {creature_object_actions::Kind::PickUp};
	Phase phase {Phase::Approach};
	creature_object_actions::Status status {creature_object_actions::Status::Running};
	/// What it acts on, and the point it throws at or points at
	std::optional<entt::entity> target;
	glm::vec3 point {0.0f};

	/// Up to four animations played together by weight, all at the same time, played left to right or not
	std::array<size_t, 4> animations {};
	std::array<float, 4> weights {};
	uint8_t animationCount {0};
	bool mirrored {false};
	float timeMs {0.0f};
	float durationMs {0.0f};
	/// When it takes hold or lets go, in the animation's time, and whether that has happened
	float eventMs {0.0f};
	bool eventDone {false};
	/// Pointing loops its animations for a while rather than playing them once: for the seconds it was asked to point,
	/// or its usual while. Pointing at a thing (the target), it points wherever the thing goes.
	float holdMs {0.0f};
	std::optional<float> pointSeconds;

	/// Where the hand gets to in the reaching animations, and how far the creature can reach, measured as it starts
	std::optional<creature_reach::Points> reach;
	float maxReach {0.0f};
	/// How many times it has walked or turned to get in reach
	uint32_t attempts {0};
	/// It turns on the spot to face what it acts on, as part of acting on it: its hands go straight on from that turn,
	/// where after any other move they start the turn after it ends
	bool turningToIt {false};
	/// Throwing: how long the throw is to take to get there, and how long it was asked to take (none to take as long as
	/// it would to fall the distance to the target)
	float flightSeconds {0.0f};
	std::optional<float> givenFlightSeconds;
	/// Eating: what it let go of at the moment it ate, neither in its hand nor on the land, used up and judged when the
	/// animation ends
	std::optional<entt::entity> meal;
	/// Picking up a portion of food: what it was made from, which loses it as the hand takes hold
	std::optional<entt::entity> source;
	/// Why it gave up, when it did
	std::string failure;
};

/// An action asked for while the hands still play another or catch: it is set about the turn after that ends, in place
/// of any asked before it
struct CreatureObjectActionWaiting
{
	CreatureObjectAction action;
};

/// Where a creature's hand takes hold in its four pick-up animations, at size one, measured the first time its test of
/// whether it can pick a thing up asks and kept until its body's animations are built again (bodyRevision)
struct CreaturePickUpReach
{
	creature_reach::Points points {};
	std::optional<uint32_t> bodyRevision;
};

/// What a creature holds in its hand, and which hand
struct CreatureHeldObject
{
	entt::entity object {entt::null};
	/// Held in the left hand, having reached for it with the animations mirrored
	bool mirrored {false};
	/// The bone it rides on, and how it is turned against the creature
	uint32_t bone {0};
	glm::mat3 rotation {1.0f};
	/// Where its middle is from where it stands, in its own space at the world's scale, so it is held by its middle
	glm::vec3 middle {0.0f};
};

/// On something a creature holds: which creature
struct HeldByCreature
{
	entt::entity creature {entt::null};
};

/// A creature catching a thing flying at it (CreatureObjectActionSystemInterface::Catch): it faces back along the
/// thing's flight, waits for it, steps across to where it will pass, and reaches for it with the four catching
/// animations blended, taking it in its hand as the hand closes or drawing the catch back. Kept once over, so that its
/// hands' part reads as done, until another action takes its place
struct CreatureCatching
{
	enum class Stage : uint8_t
	{
		/// Turning to face the thing, then waiting for it
		Ready,
		/// Stepping across to where it will pass
		Stepping,
		/// Reaching for it
		Reaching,
		Over,
	};
	/// While reaching: the hand still going for it, the catch missed and drawn back, or the thing caught
	enum class Hand : uint8_t
	{
		Reaching,
		Missed,
		Caught,
	};
	entt::entity object {entt::null};
	Stage stage {Stage::Ready};
	Hand hand {Hand::Reaching};
	/// It has finished turning to face the thing
	bool turned {false};
	/// The step or the catch is made with the left hand
	bool mirrored {false};
	/// It has stepped across in this catch: the rest of the catch is named by the step until it stands again
	bool stepped {false};
	/// Started in this turn: the body takes it up from the next
	bool fresh {true};
	/// How far through its step or its catch it is, in whole milliseconds, how far it went in the last turn, and how long
	/// the animation it plays lasts
	int32_t timeMs {0};
	int32_t stepMs {0};
	int32_t durationMs {0};
	/// The four catching animations' weights, and how high the thing was against the hand when last weighed
	std::array<float, 4> weights {};
	float height {0.0f};
};

/// Where the creature's hand closes in each catching animation, in the model's units: measured at its first catch and
/// never again, whatever its shape becomes
struct CatchHands
{
	std::array<glm::vec3, 4> hands {};
};

} // namespace openblack::ecs::components
