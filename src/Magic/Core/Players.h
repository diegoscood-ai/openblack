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

#include "ECS/Components/PlayerAlignment.h"
#include "ECS/Components/PlayerMagic.h"
#include "Enums.h"

namespace openblack
{
struct GMagicEffectInfo;
} // namespace openblack

// The GPlayer side of the miracles: which magic a player has, its tribal power and alignment, its last cast. A player
// openblack creates has the components on its entity (PlayerArchetype); the others (the neutral / script player,
// g_game + 0x18 + g_game[0x205A5B] * 0xA60) keep them here.

namespace openblack::magic::players
{
/// The player's entity (the first valid one with that name), or entt::null
[[nodiscard]] entt::entity EntityOf(PlayerNames player);

[[nodiscard]] ecs::components::PlayerMagic& MagicOf(PlayerNames player);
[[nodiscard]] ecs::components::PlayerAlignment& AlignmentOf(PlayerNames player);

/// GPlayer +0x8E0 == 1: the human player at this computer's interface
[[nodiscard]] bool IsHuman(PlayerNames player);

/// GMagicEffectInfo::GetTribalPower 0x5FB6A0 of the player (nullptr player: 1)
[[nodiscard]] float TribalPower(const GMagicEffectInfo& effect, const PlayerNames* player);

/// GPlayer::IsMagicTypeEnabled 0x64C220: the cheat, or a holder enables it
[[nodiscard]] bool IsMagicTypeEnabled(PlayerNames player, MagicType type);
/// GPlayer::SetMagicTypeEnabled 0x64C300: on -> one more holder and ever enabled; off -> one less (not under 0).
/// TODO(M7): the citadel worship sites' icons redraw their power-up levels (fn_0077B8A0).
void SetMagicTypeEnabled(PlayerNames player, MagicType type, bool on);
/// SetMagicTypeEverBeenEnabled 0x64C250 / HasMagicTypeEverBeenEnabled 0x64C260
void SetMagicTypeEverBeenEnabled(PlayerNames player, MagicType type);
[[nodiscard]] bool HasMagicTypeEverBeenEnabled(PlayerNames player, MagicType type);

/// g_game+0x205A54, the world's villagers: + 1 in the Villager ctor (0x74FAFF), - 1 in SetDying (0x76A552) or in the
/// dtor (0x74FBD4) of one not yet counted out (+0xE0 & 0x40). openblack keeps no counter: the villager entities without
/// that flag are counted (a corpse stays an entity, counted out by SetDying; a deleted one is gone)
[[nodiscard]] uint32_t WorldPopulation();
/// GPlayer::GetProportionOfWorldPopulationWhoBelieveInMe 0x64B680: the men and women (TownStats +0x54 / +0x58, Town
/// +0x664 / +0x668) of the player's towns (+0xA50) over WorldPopulation (fild qword of each, a float division); 0 when
/// either is 0
[[nodiscard]] float ProportionOfWorldPopulationWhoBelieveInMe(PlayerNames player);

/// A land is loaded (GPlayer::OnEndOfClearMap 0x64CD00): the state of the players without an entity is cleared
void Reset();
} // namespace openblack::magic::players
