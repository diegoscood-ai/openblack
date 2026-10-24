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
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

#include "ECS/Components/Town.h"
#include "Enums.h"

// The town's TownStats: the original keeps them incrementally (added and removed when a villager or an abode joins or
// leaves the town); openblack recomputes them from the entities at the start of the town's turn (ecs::town_process).
// (approximate) the same counts; the float sums are added in another order.

namespace openblack
{
struct GAbodeInfo;
enum class MeshId : uint32_t;
} // namespace openblack

namespace openblack::ecs::town_stats
{
/// Counts each villager of the town (Villager::town) and each of its abodes (Abode::townId; the fields too, they are
/// Abodes) that MakeFunctional has counted (Abode::addedToTownStats); the civic plans and the wood at the town's
/// building sites
[[nodiscard]] components::TownStats Compute(entt::entity town);
/// A town's room for villagers: the places of its counted abodes and the adult places still free in them
struct TownRoom
{
	uint32_t adultPlaces {0};    ///< Max villagers of the counted abodes with places
	uint32_t childPlaces {0};    ///< Max children of the counted abodes with places
	int32_t freeAdultPlaces {0}; ///< Max villagers of the counted abodes less the adults housed in them
};
/// The room with one more counted abode: its max villagers less its adults are free; its max villagers and max children
/// are places when it has any
[[nodiscard]] TownRoom AddAbodeRoom(TownRoom room, uint32_t maxVillagers, uint32_t maxChildren, uint32_t adultsHoused);
/// The town's room as it stands now, over the abodes Compute counts. The original keeps these as running counts, moved
/// at once when an abode is first made functional or leaves the town and when a villager moves into or out of a home or
/// grows up in one, so every reader between two town turns sees them as they are; Compute's copy is the town turn's
[[nodiscard]] TownRoom RoomOf(entt::entity town);
/// A town's buildings as its stats count them: the abodes with places, the civic ones and how many of each abode number
struct TownBuildings
{
	uint32_t abodesWithPlaces {0};             ///< Counted abodes with max villagers + max children != 0
	uint32_t civicBuildings {0};               ///< Counted abodes of a civic type
	std::array<uint8_t, 16> abodesByNumber {}; ///< Counted abodes per abode number (bytes, as the original's)
};
/// The buildings with one more counted abode: one with places when it has any, one civic building when it is civic,
/// and one more of its abode number (a number out of the table counts nowhere)
[[nodiscard]] TownBuildings AddAbodeBuildings(TownBuildings buildings, uint32_t maxVillagers, uint32_t maxChildren, bool civic,
                                              AbodeNumber number);
/// The town's buildings as they stand now, over the abodes Compute counts. The original keeps these as running counts,
/// moved at once when an abode is first made functional or leaves the town, so a reader between two town turns sees
/// them as they are; Compute's copy is the town turn's
[[nodiscard]] TownBuildings BuildingsOf(entt::entity town);
/// The town turn's stats with the counts the original keeps running, and openblack makes at the town's turn, put back
/// as they stand now: the buildings (BuildingsOf), the room (RoomOf) and the wood at the sites (WoodAtSites). The
/// villagers' counts are kept live already
[[nodiscard]] components::TownStats AsTheyStand(components::TownStats stats, entt::entity town);
/// The wood at the town's building sites as it stands now: each site's wood for the stats, for the sites whose town is
/// this town. The original keeps it as a running sum, moved as sites join or leave the town and as wood is added to or
/// taken from them
[[nodiscard]] float WoodAtSites(entt::entity town);
/// The abodes of the town: Abode::townId == Town::id, newest first (the creation index from high to low, as
/// town_queries' congregation point: an abode joining the town goes to the head)
[[nodiscard]] std::vector<entt::entity> AbodesOf(entt::entity town);
/// The abode's GAbodeInfo: the record of its abode number and mesh, else its tribe's (as influence::AbodeInfoOf;
/// (inferred) openblack keeps no info pointer); nullptr when none
[[nodiscard]] const GAbodeInfo* AbodeInfoOf(entt::entity abode, Tribe tribe);
/// The resource id of an info record's mesh, as resources::HashIdentifier gives it (the hashed text of the mesh's
/// number), without making the text a string: the abode info lookups compare it with each record they read
[[nodiscard]] entt::id_type MeshIdHash(MeshId mesh);
/// (TRIBE_TYPE, ABODE_NUMBER): the FIRST record whose tribe is the given one or Tribe::NONE and whose number matches;
/// null when none. (GAbodeInfo::Find of InfoConstants.cpp keeps the last match and throws without one: this one is the
/// original's)
[[nodiscard]] const GAbodeInfo* FindAbodeInfo(Tribe tribe, AbodeNumber number);
/// The civic ABODE_TYPEs: Totem 0x14, StoragePit 0x24, Creche 0x44, Workshop 0x84, Wonder 0x100, Graveyard 0x204,
/// TownCentre 0x404, FootballPitch 0x1004, SpellDispenser 0x2004; not Field 0x4004 nor Citadel 0x804
[[nodiscard]] bool IsCivic(AbodeType type);
} // namespace openblack::ecs::town_stats
