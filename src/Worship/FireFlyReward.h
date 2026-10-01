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

// The fireflies' miracle (FireFly.cpp 0x52B5A0..0x52B790): picking up an object a firefly sleeps on frees it, and by
// the land's probabilities (FIRE_FLY_SPELL_REWARD_PROB, only in Land1.txt: HEAL 20, FIRE, LIGHTNING_BOLT, NATURE, FOOD,
// WOOD and WATER 1) a one-shot miracle appears there.

namespace openblack::worship::fire_fly
{
/// fn_0052B630 (map command 88): probs[magic] (0xCCFBAC) and their running sums (0xCCFB04; the total 0xCCFBA8)
void SetRewardProbability(MagicType magic, float probability);
/// GInterface::PlaceObjectInMagicHand 0x5DA6F0 -> fn_0052B600: after the object's InterfaceSetInMagicHand, a firefly at
/// exactly its MapCoords goes (fn_0052B5A0) and fn_0052B6F0 runs there
void OnPlacedInMagicHand(entt::entity object);
/// fn_0052B6F0: r = GameFloatRand(total); nothing for 0; the first magic whose running sum is >= r (not 0); its first
/// seed and level; a one-shot at the point when the seed exists (GSpellSeedInfo.exists). The orb, or entt::null.
entt::entity Reward(const glm::vec3& position);
/// A land is loaded (FireFly::OnClearMap 0x52A1E0: the probabilities go to 0, the running sums stay)
void Reset();
[[nodiscard]] float Total();
} // namespace openblack::worship::fire_fly
