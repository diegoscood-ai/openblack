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

#include <functional>
#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "ScriptHeaders/ScriptEnums.h"

namespace openblack::map_coords
{
struct MapCoords;
} // namespace openblack::map_coords

/// The script containers and the script commands on them: a town (script type 9), a dance (10, ECS/Dances.h) and a
/// flock (11, ECS/Flocks.h). The command handlers in CHLApi.cpp call these; the ids they return are the entities (the
/// script's ids).
namespace openblack::ecs::script_containers
{

/// Towns, dances and flocks are containers, every other thing is not
[[nodiscard]] bool IsContainer(entt::entity thing);

/// The type's loop function: each member (controlled by the script first when a script flag is set, (pending) its
/// writer: never here), then `fn`, which stops the walk when it returns true. A town's members are in the town's
/// villager order (its abodes, each one's inhabitants, then the homeless); a flock's from its head; a dance's group by
/// group, each in its joining order. False for a type without one (the callers' "No Loop function for type")
bool ForEachMember(entt::entity container, const std::function<bool(entt::entity)>& fn);

/// CALL_IN 055: the type's find function with the filter !IsInScript && script_type::Matches when
/// `excludingScripted`, else script_type::Matches. A town finds villagers (types 4 / 5), (pending) animals (6) and its
/// storage pit (16, no filter); another type logs "Looking for strange type in Town". A dance finds the first of its
/// dancers that passes, a flock the first of its members. entt::null for none
[[nodiscard]] entt::entity Find(entt::entity container, script::ObjectType type, uint32_t subtype, bool excludingScripted);

/// CALL_IN_NEAR 067: the same find with a filter that also wants the thing within `radius` of the point
/// (GetDistanceInMetres to it, or to a worship site's totem, not above the radius). A town finds villagers (types 4 / 5)
/// and, (pending) animals (6); any other type, its storage pit too, logs "Looking for strange type in Town". entt::null
/// for none
[[nodiscard]] entt::entity FindNear(entt::entity container, script::ObjectType type, uint32_t subtype,
                                    const map_coords::MapCoords& coords, float radius, bool excludingScripted);

/// CALL_IN_NOT_NEAR 141: the same find with a filter that wants the thing farther than `radius` from the point (from
/// a worship site's totem), strictly: one at the radius, or a distance that does not compare, does not pass. The types
/// as FindNear's. entt::null for none
[[nodiscard]] entt::entity FindNotNear(entt::entity container, script::ObjectType type, uint32_t subtype,
                                       const map_coords::MapCoords& coords, float radius, bool excludingScripted);

/// GET_FIRST_IN_CONTAINER 455: a flock's head; a town's first abode's first inhabitant, else its first homeless (a
/// later abode is not looked in); a dance's first group's first dancer. "Invalid container" for another thing or no
/// thing. entt::null for none
[[nodiscard]] entt::entity FirstMember(uint32_t container);
/// GET_NEXT_IN_CONTAINER 456: no `after` (0, or a thing that has gone) -> FirstMember. Else the member after it in
/// the type's loop order (ForEachMember): none when it is the last or not a member. A dance's walk is over a list of
/// its own that nothing fills: none, always. "Invalid container" as FirstMember. Nothing is kept between calls: the
/// script's `after` is the cursor
[[nodiscard]] entt::entity NextMember(uint32_t container, uint32_t after);

/// FLOCK_CREATE 036: a flock with the default flock info, added as a created script thing (controlled). The new
/// flock
entt::entity CreateFlock(const glm::vec3& position);

/// FLOCK_ATTACH 037: pops the leader flag, the target and the obj; a dance target (controlled by the script first: the
/// obj, or each living member of a container obj, joins it in the IN_DANCE script state), a flock target (the target
/// controlled by the script first), a town target, else "Thing not added to id". The id it pushes (0 for none)
uint32_t Attach(uint32_t object, uint32_t target, bool asLeader);
/// FLOCK_DETACH 038: pops the container, then the obj id. The id it pushes
uint32_t Detach(uint32_t container, uint32_t object);
/// FLOCK_DISBAND 039: every member out, from the head: an animal a flock of its own (unless a game flag is set,
/// (pending) that flag: clear here), a villager only removed; its script reference released; a controlled one gets
/// the IN_SCRIPT state. A dance's dancers likewise, the first of its file's groups' each time. A town or an abode:
/// nothing; the container is not deleted
void Disband(uint32_t container);
/// ID_SIZE 040: a flock's size, a dance's dancers, a town's adults + children, a football pitch's players (none while
/// no match is played); nullopt for another thing ("Cannot Find Flock/Dance/Town Size", 0)
[[nodiscard]] std::optional<float> Size(uint32_t container);
/// ID_ADULT_SIZE 387: a town's adults or an abode's adults. 0 for no thing ("Object should be alive!") and for a thing
/// that is neither a town nor an abode ("Object should be town, or abode")
[[nodiscard]] float AdultSize(uint32_t object);
/// OBJECT_CAPACITY 388: a town's places for adults and children in its homes, or an abode's room for adults and
/// children; 0 and the same messages as AdultSize otherwise
[[nodiscard]] float Capacity(uint32_t object);
/// OBJECT_ADULT_CAPACITY 389: a town's places for adults in its homes, or an abode's room for adults; 0 and the same
/// messages as AdultSize otherwise
[[nodiscard]] float AdultCapacity(uint32_t object);
/// CHANGE_INNER_OUTER_PROPERTIES 056 on a flock: outer != 0 -> domain radius = outer, inner != 0 -> flock distance =
/// inner, calm always (the hunters read it: ECS/AnimalPredators.cpp, ECS/AnimalLairs.cpp), each truncated. True when
/// the thing was a flock; the caller then tries a weather thing (Magic/Script/CHLWeather.h)
bool ChangeInnerOuter(uint32_t object, float inner, float outer, float calm);

/// GET_TOWN_AND_VILLAGER_HEALTH_TOTAL 443: the life of each of the town's abodes (newest first) after the lives of its
/// inhabitants, then the lives of the homeless, summed in that order from 0; nullopt for what is not a town ("Invalid
/// objects", 0)
[[nodiscard]] std::optional<float> TownAndVillagerHealthTotal(entt::entity town);

/// POPULATE_CONTAINER 109: where a container stands, as the game reads it: its map x and z, and its own height above
/// the land at its place (a dance's place; a flock's centre less the land there); none for another thing
[[nodiscard]] std::optional<glm::vec3> PlaceOf(entt::entity container);
/// POPULATE_CONTAINER 109: a living it made joins its container, holding a reference: a dance in the IN_DANCE state, a
/// flock in MOVE_IN_FLOCK ("Living already in Flock" logged and on all the same); false for another container ("Not
/// implemented - Thing not added to id")
bool TakePopulated(entt::entity container, entt::entity living);

} // namespace openblack::ecs::script_containers
