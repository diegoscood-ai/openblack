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
/// A script made the thing, or asked for it to stay in view: however far off it is, it is never faded out and left
/// undrawn (raffclar's tag). As the original's 3D object whose disappear flag is off, also while it flies from a throw of
/// the hand (until its physics ends), while it is a super villager, and once a vortex has taken it. Every thing's 3D
/// object fades by default, and a load makes it fade again, so this is never saved. openblack draws no fade with the
/// distance yet; a vortex's take reads it for a save (docs/wiki/world/vortex.md)
struct NeverFades
{
};
} // namespace openblack::ecs::components
