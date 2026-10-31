/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <vector>

#include "ECS/ScaffoldGhostRules.h"
#include "Particles/Creators/Mesh.h"

// The see-through ghost of the building a scaffold in the player's hand would make, drawn at the hand, turning and
// growing in or shrinking away as the hand's turn changes the building on offer. Draw state only: no entity and nothing
// the game turns read. Wiki: docs/wiki/world/building-sites.md#the-phantom-in-the-hand

namespace openblack::graphics::scaffold_ghosts
{
/// What the debug window shows of the ghosts and the switch that leaves them undrawn (Locator::debugHooks)
struct Debug
{
	/// Leaves every scaffold ghost undrawn (off by default; drawing only)
	bool hidden {false};
	/// The held scaffold's ghost as the last frame drew it: its size and angle, its alpha; none when it drew none
	std::optional<ecs::scaffold_ghost::HeldPose> held;
	uint8_t heldAlpha {0};
};

/// This frame's draws: the ghost of the scaffold the player's hand holds, a Z object of its own at its place, blended by
/// its alpha with the land's cell light at the hand as its colour. Only the player's own hand shows it
[[nodiscard]] std::vector<particles::mesh_atoms::Instance> Instances();
} // namespace openblack::graphics::scaffold_ghosts
