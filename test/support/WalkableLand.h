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

#include "3D/LandAvoid.h"
#include "3D/LandAvoidState.h"
#include "ECS/Systems/LandAvoidSystemInterface.h"
#include "Locator.h"

namespace openblack::test
{
/// The cells a side of the walkable land a test gives the LandAvoid state
constexpr int32_t k_WalkableLandCells = 64;

/// Makes the LandAvoid state walkable land, so many cells a side, for the searches for clear ground (a fight's arena
/// among them); the state must be emplaced (test services do so)
inline void MakeWalkableLand(int32_t cells = k_WalkableLandCells)
{
	auto& land = Locator::landAvoidSystem::value().GetState();
	land.size = cells;
	land.avoid.assign(static_cast<size_t>(cells) * static_cast<size_t>(cells), land_avoid::k_Land);
}
} // namespace openblack::test
