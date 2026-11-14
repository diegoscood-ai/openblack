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
#include <vector>

#include <GameTailRecords.h>
#include <entt/entity/entity.hpp>

/// The scripts' object table of a saved game. The scripts' machine names an object by its entity; the saved game names
/// it by its place in a table of 511 places that the world file keeps (each place: the object, whether a script made it,
/// how many script variables hold it), and the machine's file holds the places. A save gives each object the scripts
/// name a place, a load puts the loaded objects back in their places and the machine's values back on them. See
/// docs/wiki/engine/saved-games.md#the-script-machines-file.
namespace openblack::ecs::saved_script_objects
{

/// The places of the table; place 0 is the scripts' null, so the places are 1 to 511
inline constexpr size_t k_Places = gamesave::k_ScriptObjectCount;

/// The objects of the table, place 1 first: each object the machine's values name and each one a script holds, once,
/// in the order of their entity numbers. None when they do not fit in the table. The value 0 is the scripts' null
[[nodiscard]] std::optional<std::vector<uint32_t>> PlacesOf(std::span<const uint32_t> named, std::span<const uint32_t> held);
/// The place of a machine's value among the places: 0 for the null or a value that has none
[[nodiscard]] uint32_t PlaceOf(std::span<const uint32_t> places, uint32_t value);
/// The machine's value of a place of the loaded table: 0 for place 0, a place past the table or a place that lost its
/// object
[[nodiscard]] uint32_t ValueOf(std::span<const uint32_t> loaded, uint32_t place);

/// Whether there is a scripts' machine whose objects a save writes (a game without one keeps the table it loaded)
[[nodiscard]] bool HasMachine();
/// From the Locator's services: the table's objects for a save now, as PlacesOf; none when they do not fit
[[nodiscard]] std::optional<std::vector<uint32_t>> Current();

/// One place of a loaded table, its object found among the loaded entities
struct LoadedPlace
{
	entt::entity object {entt::null};
	bool createdByScript {false};
	uint8_t references {0};
};
/// A load puts each loaded object back in its place: it is in a script again, as made by one or not, held by as many
/// variables as its place says. Gives the machine's value of each place (place 1 first), 0 for a place with no object
[[nodiscard]] std::vector<uint32_t> PutBack(std::span<const LoadedPlace> places);

} // namespace openblack::ecs::saved_script_objects
