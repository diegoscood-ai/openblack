/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <glm/vec3.hpp>

/// Which of the in-game bank's samples the player's hand plays, and how: gripping the land and the sea, picking things
/// up, scooping food and wood, and putting trees back in the ground or pulling them out.
///
/// Pure values and functions, tested on their own; the hand's system plays them.
namespace openblack::hand_sounds
{

/// Gripping the land: a random one of six samples from the first land-grab sample, heard flat (not placed in the world)
inline constexpr int k_GripLandFirst = 4;
inline constexpr int k_GripLandCount = 6;
inline constexpr bool k_GripLandIs3D = false;

/// Gripping the sea: the first of the ten hand-in-water samples, the water counter (0..9 in turn) picks which; placed in
/// the world
inline constexpr int k_WaterFirst = 99;
inline constexpr bool k_WaterIs3D = true;

/// The water sample for the water counter's value
[[nodiscard]] constexpr int WaterSample(int counter) noexcept
{
	return k_WaterFirst + counter;
}

/// The water sound's point: on the water's surface, 0.2 high, where the hand went in
[[nodiscard]] inline glm::vec3 WaterPoint(glm::vec3 point) noexcept
{
	return glm::vec3(point.x, 0.2f, point.z);
}

/// Picking an object up: the pick-up sample at the object, placed in the world
inline constexpr int k_PickUpObject = 10;
inline constexpr bool k_PickUpIs3D = true;

/// Whether picking up plays the pick-up sample: for everything but a rooted tree or forest (one not in the physics)
[[nodiscard]] constexpr bool PlaysPickUpSample(bool treeOrForest, bool inPhysics) noexcept
{
	return !(treeOrForest && !inPhysics);
}

/// A living villager picked up also screams: a random one of seven from the first scream of a child, else a woman,
/// else a man
inline constexpr int k_ScreamCount = 7;
[[nodiscard]] constexpr int ScreamFirst(bool child, bool woman) noexcept
{
	return child ? 180 : woman ? 194 : 187;
}

/// A tree put back in the ground: one of three planting samples, by the tick count
[[nodiscard]] constexpr int PlantTreeSample(uint32_t tickCount) noexcept
{
	return 83 + static_cast<int>(tickCount % 3);
}

/// A tree pulled out of the ground: a random one of three tree-break samples
inline constexpr int k_TreeBreakFirst = 32;
inline constexpr int k_TreeBreakCount = 3;

} // namespace openblack::hand_sounds
