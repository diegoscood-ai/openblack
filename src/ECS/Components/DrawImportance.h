/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{
/// How important it is to keep a thing of one mesh in view: the importance widens the distance bands of the thing. A
/// thing without this component has an importance of 0 (raffclar's component). As the original's 3D object keeps it from
/// the thing's making: a tree's -0.1, a dead tree's 1, a villager's age in years then times 0.001
/// (villager_age::DrawImportanceFor), taken again when a load makes it (the original's load makes every 3D object again).
/// openblack draws no fade with the distance yet; a vortex's take reads it for a save (docs/wiki/world/vortex.md)
struct DrawImportance
{
	float importance {0.0f};
};

/// The importance of a tree that stands, is held or flies. Its bands are a tenth nearer than those of a thing with no
/// importance (raffclar's constant)
inline constexpr float k_TreeImportance = -0.1f;
/// The importance of a dead tree (raffclar's constant)
inline constexpr float k_DeadTreeImportance = 1.0f;
} // namespace openblack::ecs::components
