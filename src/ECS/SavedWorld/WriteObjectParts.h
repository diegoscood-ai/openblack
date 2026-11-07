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

#include <SaveFields.h>
#include <SaveParts.h>
#include <entt/entity/entity.hpp>

#include "ECS/SavedWorld/WorldWriter.h"
#include "Enums.h"

// The parts every class of object writes before its own values, in the original's order, written from an entity: the
// place, the object's own values, the fixed object's and the object of several cells' (see
// docs/wiki/engine/saved-games.md#writing-the-world-file). Each takes the part the entity's load kept, or nothing for an
// object made in play, and puts in what openblack holds; a value whose meaning is not read stays as kept, or zero.
namespace openblack::ecs::saved_world_write
{
/// The place's flags word: what each bit says of the object
namespace position_flags
{
inline constexpr uint16_t k_InMap = 0x0001;
inline constexpr uint16_t k_OfSeveralCells = 0x0002;
inline constexpr uint16_t k_InTheHand = 0x0004;
inline constexpr uint16_t k_InLockedSelect = 0x0010;
inline constexpr uint16_t k_InPhysics = 0x0040;
inline constexpr uint16_t k_OnAStructure = 0x0080;
inline constexpr uint16_t k_DrawListed = 0x0100;
inline constexpr uint16_t k_InScript = 0x0200;
inline constexpr uint16_t k_ControlledByScript = 0x0400;
inline constexpr uint16_t k_Immovable = 0x1000;
inline constexpr uint16_t k_CannotBePickedUp = 0x2000;
inline constexpr uint16_t k_Indestructible = 0x4000;
inline constexpr uint16_t k_CountsAsFixed = 0x8000;
} // namespace position_flags

/// The place's flags word of an entity, from what openblack holds; the bit of a walker on a structure, which openblack
/// does not hold yet, as `kept` has it
[[nodiscard]] uint16_t PositionFlagsOf(const Registry& registry, entt::entity entity, uint16_t kept);

/// The place: its map coordinates, kept while the entity stands where its load put it (a
/// place in metres does not give back the same coordinates), else those of where it stands, and its flags
[[nodiscard]] gamesave::PositionPart PositionPartOf(const WriteContext& context, entt::entity entity,
                                                    const gamesave::PositionPart* kept);

/// The object's values: the row of its kind and its target place (given: each class knows its own; a
/// fixed object's is its place), its creation number, life, size, angle and fire, and its body when the physics moves
/// it
[[nodiscard]] gamesave::ObjectPart ObjectPartOf(WriteContext& context, entt::entity entity, const gamesave::ObjectPart* kept,
                                                const gamesave::MapCoords& target, uint32_t info);

/// A fixed object's: the town artifact it holds
[[nodiscard]] gamesave::FixedPart FixedPartOf(WriteContext& context, entt::entity entity, const gamesave::FixedPart* kept);

/// An object of several cells': its flags, how much of it is built, its footpath link and its
/// building site
[[nodiscard]] gamesave::MultiMapFixedPart MultiMapFixedPartOf(WriteContext& context, entt::entity entity,
                                                              const gamesave::MultiMapFixedPart* kept);

/// The number that names a player: the player the loaded game's record names, else the player's entity's
[[nodiscard]] gamesave::ObjectNumber PlayerReference(WriteContext& context, PlayerNames player);

/// A fixed object's parts together, from the place on
[[nodiscard]] gamesave::FixedFields FixedFieldsOf(WriteContext& context, entt::entity entity, const gamesave::FixedFields* kept,
                                                  uint32_t info);
/// An object of several cells' parts together, from the place on
[[nodiscard]] gamesave::MultiMapFixedFields MultiMapFixedFieldsOf(WriteContext& context, entt::entity entity,
                                                                  const gamesave::MultiMapFixedFields* kept, uint32_t info);
} // namespace openblack::ecs::saved_world_write
