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
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Effects/EffectValues.h"

// What a lightning fork's tip does when no spell is behind the bolt (a storm's fork strike, a script's lightning strike):
// it applies the weather lightning effect around the tip, instead of sending the spell its "landed" event. Wiki:
// docs/wiki/magic/fireball-and-lightning.md#lightning.

namespace openblack
{
class LandIslandInterface;
namespace v120
{
struct InfoConstants;
}
using InfoConstants = v120::InfoConstants;
} // namespace openblack

namespace openblack::particles
{
struct Collection;
class Modifier;
class Effect;
class SpellSink;
struct SpellEventInfo;
} // namespace openblack::particles

namespace openblack::particles::lightning
{

/// The effect a tip applies without a spell: the weather lightning row of the effect info (in the game's data burn
/// 10000, alignment 1, radius 1), applied by nobody, for no player and not scaled
[[nodiscard]] ecs::effects::EffectValues NoSpellTipValues(const InfoConstants& info);

/// Where that effect is applied: the tip's x and z in metres (the effect turns them into map coordinates itself) and y
/// the altitude above the island's ground there, as a map position holds it (without an island the ground is 0)
[[nodiscard]] glm::vec3 TipMapPosition(const LandIslandInterface* island, const glm::vec3& tip);

/// A struck tip: with a spell behind the bolt (a sink) the event goes to it and nothing is applied to the map; with
/// none, the values to apply around the tip (NoSpellTipValues), or nothing without the game's info
[[nodiscard]] std::optional<ecs::effects::EffectValues> StrikeTip(SpellSink* sink, const SpellEventInfo& event,
                                                                  const InfoConstants* info);

/// A bolt that takes its targets from the effect's spell targets takes every one out, its points first and then its
/// objects, the last one first each time. Each becomes a place: a point as it is; an object, its map position on the
/// land (x and z, and the ground there plus its height above it). An object with no position leaves the place the one
/// before it left (0, 0, 0 for the first)
[[nodiscard]] std::vector<glm::vec3> TakeTargetPlaces(Effect& effect);

/// Of those places, in their order, the ones strictly within `radius` of `origin` across the land
/// (dz dz + dx dx < radius radius): every one is kept, however many, and nothing else is asked of them
[[nodiscard]] std::vector<glm::vec3> PlacesInReach(std::span<const glm::vec3> places, const glm::vec3& origin, float radius);

/// One of a bolt's targets as a save writes it: the object (none for a ground point) and the game turn it was found,
/// its ground point, whether it is an object, may be struck and has had its arcs asked for, and its cooldown
struct SavedTarget
{
	entt::entity object {entt::null};
	uint32_t turn {0};
	glm::vec3 point {0.0f};
	bool isObject {false};
	bool active {false};
	bool lightMapDone {false};
	int cooldown {0};
};

/// What a lightning keeps on its collection, as a save writes it: the bolts it is linked to and that are linked to it
/// (their collections), its origin, the last search's origin, the targets' centroid, its heading, whether its first
/// step is to come, whether it was cast from the hand, the age since its last search, the distance to the centroid, the
/// search's cap and the most targets a step picks, its targets, the lists of targets its forks took and the next free
/// list, its forks (their collections, each with whether it was left off the root this step), the next free fork, the
/// fork the step picked at random, its forks' scale, the turn it was made, its clash point and its own collection
struct SavedBolt
{
	const Collection* link {nullptr};
	const Collection* linkedBy {nullptr};
	/// The effects those two collections are of (0 for none)
	uint32_t linkEffect {0};
	uint32_t linkedByEffect {0};
	glm::vec3 origin {0.0f};
	glm::vec3 searchOrigin {0.0f};
	glm::vec3 centroid {0.0f};
	float heading {0.0f};
	bool first {true};
	bool fromHand {false};
	float life {0.0f};
	float centroidDistance {1.0f};
	int searchLimit {0};
	int atOnce {0};
	std::vector<SavedTarget> targets;
	std::vector<std::vector<int>> lists;
	int nextList {0};
	std::vector<std::pair<const Collection*, bool>> forks;
	int nextFork {0};
	const Collection* pickedFork {nullptr};
	float forkScale {1.0f};
	uint32_t createdTurn {0};
	glm::vec3 clash {0.0f};
	const Collection* collection {nullptr};
};

/// The lightning rule's data on the collection, if the rule is that rule and the collection has stepped it
[[nodiscard]] std::optional<SavedBolt> SavedBoltOf(const Collection& collection, const Modifier& rule);

/// For a load: the numbers in the saved game of a bolt's data and of the bolts' data it is linked with (0 for none)
struct LoadedBoltNumbers
{
	uint32_t number {0};
	uint32_t link {0};
	uint32_t linkedBy {0};
};
/// For a load, once the collection's atoms and forks are made: the lightning rule's data on the collection as the save
/// kept it (its root the collection's first atom, the picked fork among the root's collections, its forks left off the
/// root drawn hidden), noted as made, at the head of the bolts; its links wait for LinkLoadedBolts. False, with nothing
/// changed, for a rule of another class
bool RestoreBolt(Collection& collection, const Modifier& rule, const SavedBolt& bolt, const LoadedBoltNumbers& numbers);
/// For a load, once every effect is made: each restored bolt is linked with the bolts its saved links name
void LinkLoadedBolts();

} // namespace openblack::particles::lightning
