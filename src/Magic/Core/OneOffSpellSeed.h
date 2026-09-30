/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// One-shot miracles (OneOffSpellSeed, SpellSeed.cpp 0x72A2F0..0x72A840): the orbs on the land, and a fully charged
// seed straight into the hand.

namespace openblack::magic::one_off
{
/// OneOffSpellSeed::Create 0x72A2F0 (pos, seed 0..29, pu, scale): the orb at a world point; entt::null for a bad seed
entt::entity Create(const glm::vec3& worldPosition, SpellSeedType seedType, int powerUp, float scale);

/// OneOffSpellSeed::CreateSpellIntoHand 0x72A730 (iface, seed, pu, multiplier): if the hand is free, a seed at the hand
/// (linked to the player's best worship icon for it: M7, none yet) charged for free with its full cost, the magic
/// marked ever enabled, placed in the magic hand and ready at once (fn_00729900(0)). The seed, or entt::null.
entt::entity CreateSpellIntoHand(PlayerNames player, SpellSeedType seedType, int powerUp, float multiplier);

/// OneOffSpellSeed::InterfaceTap 0x72A640: CreateSpellIntoHand with the orb's seed, then immersion 0xE (M2), sample
/// 0x6D and the orb goes (3). 0 when the hand could not take it.
int InterfaceTap(entt::entity orb, PlayerNames player);

/// OneOffSpellSeed::InterfaceSetInMagicHand 0x72A530 (picking the orb itself): only "ever enabled"
void InterfaceSetInMagicHand(entt::entity orb, PlayerNames player);

/// UpdateFrame 0x72A570 for every orb: phase = fmod(phase + ms x 18 x 0.001, 16), frame = int(phase), the texture
/// offset ((frame % 4) / 4, (frame / 4) / 4). milliseconds: g_game_time_inc of this frame.
void UpdateFrames(float milliseconds);
} // namespace openblack::magic::one_off
