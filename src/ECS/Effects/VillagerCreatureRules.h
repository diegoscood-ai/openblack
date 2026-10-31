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

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

// How a villager reacts to a creature nearby: how urgently it takes the reaction up, how long it keeps it and how long
// before it takes it up again, and the steps of the inspection, the respect and the fear that follow, by its town's
// attitude to the creature; and how an animal takes the same reaction up (AnimalPriority; the two turn counts are every
// living being's, with no town for an animal). Pure functions: the creature's and the town's state, the distances and
// the random draws come in as arguments, so the caller keeps the order of the draws. Wiki:
// docs/wiki/beings/villager-reactions.md#a-creature-reaction-6, docs/wiki/beings/animal-life.md#reactions.

namespace openblack::ecs::effects::creature_reaction
{
/// How impressive an action of the creature's action table is
inline constexpr uint32_t k_NotImpressive = 24;
inline constexpr uint32_t k_QuiteImpressive = 25;
inline constexpr uint32_t k_VeryImpressive = 26;
/// The rows of the creature's action table
inline constexpr uint32_t k_ActionRows = 328;

/// A villager starts inspecting a creature from this many metres, and each look brings it this much closer
inline constexpr float k_InspectStartDistance = 20.0f;
inline constexpr float k_InspectCloser = 8.0f;
/// Once the distance it inspects from is below this, its last look ends the reaction
inline constexpr float k_InspectNearEnough = 1.0f;
/// It walks up to the creature only when it stands further than this beyond the distance it inspects from
inline constexpr float k_ApproachMargin = 1.0f;
/// A walk up to a creature is over this near its goal, in metres
inline constexpr float k_ArrivedWithin = 0.5f;
/// A look lasts at least this many seconds, plus a draw below the first look's or a later look's range
inline constexpr float k_InspectMinSeconds = 1.5f;
inline constexpr float k_InspectFirstRandomSeconds = 3.0f;
inline constexpr float k_InspectAgainRandomSeconds = 7.0f;
/// A respectful villager faces the creature this many seconds
inline constexpr float k_RespectSeconds = 4.0f;
/// It turns back to the creature when it faces more than this many 2048ths of a turn away from it
inline constexpr uint32_t k_RespectFacingSlack = 0x100;
/// The turning step, in the villager's look modes, of a look while it inspects and while it turns to face the creature
inline constexpr uint32_t k_InspectLookMode = 1;
inline constexpr uint32_t k_RespectLookMode = 2;
/// A frightened villager runs this many times the reaction's reach away, and gives up once the creature is further than
/// this many times that
inline constexpr float k_FleeReachScale = 1.2f;
inline constexpr float k_FleeGiveUpScale = 1.5f;
/// While the creature acts on something, one draw in this many sends a frightened villager running
inline constexpr uint32_t k_FleeChanceRange = 10;

/// What a villager's priority for reacting to a creature depends on
struct PriorityFacts
{
	/// The villager is dancing
	bool dancing {false};
	/// The reaction's initiator is a creature
	bool isCreature {true};
	/// The creature is one villagers ignore, or is invisible
	bool ignoredByVillagers {false};
	bool invisible {false};
	/// From the villager to the creature, in metres, and the reaction info's maxDistanceToRunAwayFromObject
	float distance {0.0f};
	float maxDistanceToRunAway {0.0f};
	/// The villager has a town, and the town's attitude to the creature (town_creature_attitude)
	bool hasTown {false};
	uint32_t attitude {0};
	/// The town and the creature have the same player
	bool sameOwner {false};
	/// The villager is a disciple
	bool disciple {false};
	/// The town desire its final state serves (-1 none) and how much of it, and the town's raw desire of that kind
	/// (read only for a desire of the table)
	int32_t servedDesire {-1};
	float servedDesireAmount {0.0f};
	float townRawDesire {0.0f};
	/// The priorities of the reaction info's rows for this reaction, for running from a thing and for following one
	uint32_t priority {0};
	uint32_t fleePriority {0};
	uint32_t followPriority {0};
};
/// The priority of reacting to a creature, 0 for none: none while dancing, for no creature, for one villagers ignore or
/// cannot see, or for one further than maxDistanceToRunAway. With no town, the reaction's priority. Else by the town's
/// attitude: a creature it has never judged half as much again, none for one it does not care about, twice the
/// priority of running from a thing for one it fears (of which only the low byte is kept, so it wraps), and for one it
/// respects: the priority of following a thing, unless the creature is its own player's and the villager is a disciple
/// or busy with a state its town wants done. Every value keeps only its low byte
[[nodiscard]] uint8_t VillagerPriority(const PriorityFacts& facts);

/// The priority of an animal for reacting to a creature: none while it dances, else the reaction's priority, of which
/// only the low byte is kept. Unlike a villager's, it asks nothing of the creature, its distance or a town
[[nodiscard]] uint8_t AnimalPriority(bool dancing, uint32_t priority);

/// What the reaction's two turn counts depend on
struct TurnsFacts
{
	/// The reaction's thing is a creature
	bool isCreature {true};
	/// The spread's distance to the villager, in metres
	float distance {0.0f};
	/// The metres of a map cell and the reaction info's maxReactionDistance and howImportantIsDistance
	float metresPerCell {10.0f};
	float maxReactionDistance {0.0f};
	float howImportantIsDistance {0.0f};
	/// How impressive the last action the creature carried out to its end was
	uint32_t impressiveType {k_NotImpressive};
	/// How impressive the action of the plan it carries out is, when the action is a row of the action table
	std::optional<uint32_t> currentActionImpressiveType;
	/// The villager's town's attitude to the creature, with a town (an animal has none)
	std::optional<uint32_t> townAttitude;
	/// The reaction info's numGameTurnsForNormalThingsToReact or ...BeforeReactingAgain
	uint32_t turns {0};
};
/// How long a villager keeps reacting to a creature: the info's turns, more for a near creature (by the distance's
/// importance, over the reaction's reach plus a cell), twice for a quite impressive last action and three times for a
/// very impressive one, four times more when its town fears the creature. Truncated, kept as an unsigned number: a
/// creature far enough gives a negative count, which ends the reaction at once. None when the thing is no creature
[[nodiscard]] uint32_t TurnsToReact(const TurnsFacts& facts);
/// How long before a villager reacts to a creature again: the info's turns, less for a near creature, halved for a quite
/// impressive last action and a third for a very impressive one. None (at once) while the creature carries out an
/// impressive action of the table, or while its town fears or respects it
[[nodiscard]] uint32_t TurnsBeforeReactingAgain(const TurnsFacts& facts);

/// The state a villager takes the reaction up in, by its town's attitude to the creature: inspecting it, or for a
/// feared one running from it; none for one its town does not care about
[[nodiscard]] std::optional<VillagerStates> SetupState(uint32_t attitude);

/// The turns of a look at the creature: the minimum plus the draw
[[nodiscard]] uint32_t InspectTurns(int32_t minTurns, uint32_t draw);
/// What a villager looking at a creature does this turn, with its look's turns left after this one
enum class InspectStep
{
	/// The last look, near enough: it stops reacting
	Stop,
	/// Near enough, still looking: nothing
	Wait,
	/// Its look is over: it goes closer
	Approach,
	/// It looks on, turning to the creature
	Look,
};
[[nodiscard]] InspectStep PerformInspectStep(uint32_t turnsLeft, float inspectDistance);
/// It walks up to the creature when it stands more than k_ApproachMargin further than it inspects from
[[nodiscard]] bool ApproachFurther(float distance, float inspectDistance);
/// A distance in metres as whole map units, the metres cut to a whole number first
[[nodiscard]] int32_t MetresToWholeDistance(float metres);
/// The step to the point the villager walks to, along the angle to the creature, to stand `inspectDistance` from it
[[nodiscard]] glm::ivec2 ApproachStep(uint16_t angle, float distance, float inspectDistance);
/// The distance it inspects from after a look up close
[[nodiscard]] float NextInspectDistance(float inspectDistance);
/// A walk up to the creature is over at the goal (a NaN distance counts as there)
[[nodiscard]] bool Arrived(float distanceToGoal);
/// The point a respectful villager walks to, by the creature's edge: through metres and back to whole map units
[[nodiscard]] map_coords::MapCoords RespectWalkGoal(const map_coords::MapCoords& edge);
/// It faces away from the creature: the difference of the two angles, with no wrap round the turn, beyond the slack
[[nodiscard]] bool FacingAway(uint16_t angleToCreature, uint16_t gameAngle);
/// The run from a feared creature, and whether the creature is too far to run from any more (a NaN distance too)
[[nodiscard]] float FleeDistance(float maxReactionDistance);
[[nodiscard]] bool FleeGivesUp(float fleeDistance, float distance);
/// Half a whole speed, towards zero
[[nodiscard]] int32_t HalfSpeed(int32_t speed);

/// What a villager reacting to a creature does about its town's attitude to it, every turn of the reaction
enum class AttitudeStep
{
	/// Nothing changes
	Keep,
	/// The town no longer cares: it stops reacting
	Stop,
	/// It runs from the creature
	Flee,
	/// It goes to stand by the creature in respect
	Respect,
};
/// By the attitude: none -> Stop; fear -> Flee unless it is running from it already; respect -> Respect unless it is
/// respecting it already or turning to face it; else Keep
[[nodiscard]] AttitudeStep StepFromAttitude(uint32_t attitude, VillagerStates finalState);
} // namespace openblack::ecs::effects::creature_reaction
