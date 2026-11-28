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
#include <cstdint>

#include <functional>
#include <vector>

#include "Creature/CreatureIdleMind.h"

/// What a slapped creature does once it is free to choose: it shows how the slap made it feel
namespace openblack::creature_follow_up
{

/// The clips of the reactions to a slap
constexpr size_t k_Angry = 53;
constexpr size_t k_Sad = 56;
constexpr size_t k_Frightened = 61;
constexpr size_t k_Cry = 219;
constexpr size_t k_RudeGesture = 222;

/// What the creature feels, which the slap's reaction reads
struct Feelings
{
	float compassion {0.0f};
	float anger {0.0f};
};

/// The turns it looks at the player, sadly: one second and a random share of another, cut to whole turns
[[nodiscard]] uint32_t StareTurns(float share, uint32_t turnsPerSecond);

/// A slap: one of four reactions, each as likely:
/// - when it feels no more anger than compassion, it is frightened; else it is angry or makes a rude gesture, each one
///   time in two;
/// - it is sad;
/// - it cries;
/// - it looks at the player for one to two seconds, with stoned eyes and a sad face.
/// The random function gives a whole number below its argument, the random share a number from 0 to 1; the share is
/// drawn only for the look, after the first draw.
[[nodiscard]] std::vector<creature_mind::Step> ShowSlapFeeling(const Feelings& feelings,
                                                               const std::function<uint32_t(uint32_t)>& random,
                                                               const std::function<float()>& randomShare,
                                                               uint32_t turnsPerSecond);

} // namespace openblack::creature_follow_up
