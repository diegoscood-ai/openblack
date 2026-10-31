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
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"
#include "Magic/VortexCrossingRules.h"

namespace openblack::ecs
{
class Registry;
}

/// The crossing file's line for a thing a way out of a land takes, read from its components, as each of the original's
/// classes saves itself (docs/wiki/world/vortex.md#the-land-script-round-trip-of-vortextxt), and its text in the land
/// scripts' words (lhscriptx::WriteCommand)
namespace openblack::ecs::vortex_save_lines
{

/// What the lines need from the rest of the game
struct World
{
	/// The game turn now, for the villagers' ages
	uint32_t turn {0};
	/// The row of the villagers' table a villager is made from
	std::function<int32_t(entt::entity)> villagerRow;
	/// The kind of resource a kind of pot holds
	std::function<int32_t(PotInfo)> potResource;
	/// Its life, 0 to 1
	std::function<float(entt::entity)> life;
	/// The player it belongs to, none for nobody
	std::function<std::optional<PlayerNames>(entt::entity)> player;
	/// Whether a pile is one of a storage pit's, which stays with its pit
	std::function<bool(entt::entity)> ofAStoragePit;
	/// The name of the miracle a seed casts at a power-up level
	std::function<std::string(SpellSeedType, int)> magicName;
	/// Where a thing stands, as the line places it: its map place and its height above the land there
	std::function<map_coords::MapCoords(entt::entity)> placeOf;
	/// Whether a thing lies on the land, not flying, carried by a particle or held
	std::function<bool(entt::entity)> lyingOnTheLand;
};

/// The line making the thing again relative to the vortex's place: a villager, an animal, a pot (not one of a storage
/// pit's piles), a tree, a rock or other static thing (and its artefact line when it is a player's artefact), a piece
/// knocked off a building (as a rock), a scaffold with its town, a thing lying about, a dead tree or a globe holding a
/// miracle, where it stands now (its map place and its height above the land). None for anything else
[[nodiscard]] std::optional<openblack::vortex::crossing::Line> LineFor(const Registry& registry, entt::entity thing,
                                                                       const map_coords::MapCoords& place,
                                                                       const map_coords::MapCoords& vortex, const World& world);

/// The line's text, ending in a new line; none if the land scripts have no such command
[[nodiscard]] std::optional<std::string> Text(const openblack::vortex::crossing::Line& line);

/// The player a thing belongs to as its line names it: a dead tree's own (the one it was made for, the neutral
/// player's for a felled tree or a dropped log); none for anything else
[[nodiscard]] std::optional<PlayerNames> OwnerOf(const Registry& registry, entt::entity thing);

/// The other static things of the same kind lying on the land exactly where a static thing stands, at the same height
/// above it: once its line is written they count as written too, so a pile of them in one place comes out as one
[[nodiscard]] std::vector<entt::entity> IdenticalStatics(const Registry& registry, entt::entity thing, const World& world);
/// The same for a thing lying about (not a scaffold): the other things lying about of its kind just where it stands
[[nodiscard]] std::vector<entt::entity> IdenticalMobileObjects(const Registry& registry, entt::entity thing,
                                                               const World& world);

} // namespace openblack::ecs::vortex_save_lines
