/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// A creature's size: how tall a size makes it, and a size following another's (a rival god's creature that a script
/// sets to keep pace with the local player's creature, CREATURE_AUTOSCALE). Pure functions, tested on their own. Wiki:
/// docs/wiki/creature/players-creature.md#land-2s-creatures.
namespace openblack::creature_size
{

/// A creature of size 1 is this tall: how the minds and the casting reckon another creature's height from its size
inline constexpr float k_HeightOfSizeOne = 15.0f;

/// A creature following another's size grows no bigger than this
inline constexpr float k_AutoscaleLargest = 2.0f;

/// One turn of a creature following another's size: halfway from its own size towards the other's size times the
/// factor, no bigger than k_AutoscaleLargest. `localBase` is the other creature's size before any size spell on it
[[nodiscard]] float AutoscaleStep(float own, float localBase, float factor);

} // namespace openblack::creature_size
