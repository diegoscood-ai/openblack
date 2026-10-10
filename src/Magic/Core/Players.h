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

#include <entt/entity/entity.hpp>

#include "ECS/Components/Alignment.h"
#include "ECS/Components/InfluenceCrossing.h"
#include "ECS/Components/InterfaceAlignment.h"
#include "ECS/Components/PlayerMagic.h"
#include "Enums.h"

namespace openblack
{
struct GMagicEffectInfo;
} // namespace openblack

// The player side of the miracles: which magic a player has, its tribal power and alignment, its last cast. A player
// openblack creates has its PlayerMagic on its entity (PlayerArchetype); the others (the neutral / script player) use
// the player system's (Locator::playerSystem), which also keeps every player's alignment across lands.

namespace openblack::magic::players
{
/// The player's entity (the first valid one with that name), or entt::null
[[nodiscard]] entt::entity EntityOf(PlayerNames player);

[[nodiscard]] ecs::components::PlayerMagic& MagicOf(PlayerNames player);
/// The same player's magic, read through the const registry so that no storage is made
[[nodiscard]] const ecs::components::PlayerMagic& ReadMagicOf(PlayerNames player);
[[nodiscard]] ecs::components::Alignment& AlignmentOf(PlayerNames player);

/// The human player at this computer's interface
[[nodiscard]] bool IsHuman(PlayerNames player);

/// The effect's tribal power for the player (nullptr player: 1)
[[nodiscard]] float TribalPower(const GMagicEffectInfo& effect, const PlayerNames* player);

/// The cheat, or a holder enables it
[[nodiscard]] bool IsMagicTypeEnabled(PlayerNames player, MagicType type);
/// On -> one more holder and ever enabled; off -> one less (not under 0).
/// TODO: the citadel worship sites' icons redraw their power-up levels.
void SetMagicTypeEnabled(PlayerNames player, MagicType type, bool on);
/// Marks / tells whether the player ever had the magic type
void SetMagicTypeEverBeenEnabled(PlayerNames player, MagicType type);
[[nodiscard]] bool HasMagicTypeEverBeenEnabled(PlayerNames player, MagicType type);

/// The world's villagers: one more for each new villager, one less when it dies or, if not yet counted out, when it is
/// deleted. openblack keeps no counter: the villager entities not counted out are counted (a corpse stays an entity,
/// counted out when it died; a deleted one is gone)
[[nodiscard]] uint32_t WorldPopulation();
/// The men and women of the player's towns over WorldPopulation (a float division of the exact counts); 0 when either
/// is 0
[[nodiscard]] float BelieverFraction(PlayerNames player);

/// The local player's interface alignment and hand crossing: the ones its entity carries, or the player system's while
/// no entity does
[[nodiscard]] ecs::components::InterfaceAlignment& LocalInterfaceAlignment();
[[nodiscard]] ecs::components::InfluenceCrossing& LocalInfluenceCrossing();

/// A land is loaded (the map is cleared): the state of the players without an entity is cleared, the local player's
/// interface alignment and hand crossing move off its entity to the player system, and the player system lists no
/// player entity until the new land makes them
void Reset();

/// The state hash's "player_alignment" part: every player's alignment and pending change
void RegisterStateHash();
} // namespace openblack::magic::players
