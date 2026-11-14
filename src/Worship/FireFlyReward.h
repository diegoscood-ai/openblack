/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <array>
#include <span>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// The fireflies' miracle: picking up an object a firefly sleeps on frees it, and by the land's probabilities
// (FIRE_FLY_SPELL_REWARD_PROB, only in Land1.txt: HEAL 20, FIRE, LIGHTNING_BOLT, NATURE, FOOD, WOOD and WATER 1) a
// one-shot miracle appears there.

namespace openblack::worship::fire_fly
{
/// Map command 88: probs[magic] and their running sums (the last one is the total)
void SetRewardProbability(MagicType magic, float probability);
/// After an object is placed in the magic hand, a firefly at exactly its map coordinates goes and Reward runs there
void OnPlacedInMagicHand(entt::entity object);
/// r = GameFloatRand(total); nothing for 0; the first magic whose running sum is >= r (not 0); its first seed and
/// level; a one-shot at the point when the seed exists (GSpellSeedInfo.exists). The orb, or entt::null.
entt::entity Reward(const glm::vec3& position);
/// A land is loaded (the probabilities go to 0, the running sums stay)
void Reset();
[[nodiscard]] float Total();
/// The number of miracles the table weighs
inline constexpr size_t k_RewardMagics = 42;
/// The running sums the draw reads, and the weights, by miracle, as a saved game keeps them
[[nodiscard]] std::array<float, k_RewardMagics> RewardSums();
[[nodiscard]] std::array<float, k_RewardMagics> RewardProbabilities();
/// The table as a load reads it: the running sums and the weights, each as many as the save holds, up to the table's
void RestoreRewards(std::span<const float> sums, std::span<const float> probabilities);
} // namespace openblack::worship::fire_fly
