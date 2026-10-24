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

// UR_VortexAttract, the rule of the effect that draws things into a way out of a land (the vortex's ObjectMover,
// Particles/Rules/VortexAttract.cpp). Wiki: docs/wiki/world/vortex.md#taking-objects-in-the-in-vortex.

namespace openblack::particles::vortex_attract
{
/// Once a frame: each thing a vortex draws in takes its particle's drawn place, turn and size
void UpdateCarriedObjects();
/// How many things the vortices are drawing in now (the Vortices debug window)
[[nodiscard]] size_t CarriedCount();
} // namespace openblack::particles::vortex_attract
