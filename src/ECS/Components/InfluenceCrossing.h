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

#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// Where the local player's hand was on the previous frame against the influence circles, to tell when it crosses an
/// edge. Clearing the map only empties the circle list, nothing clears this: it lasts the whole game, so the local
/// player's entity carries it while it exists, and the player system keeps it between lands
struct InfluenceCrossing
{
	static constexpr size_t k_Players = static_cast<size_t>(PlayerNames::_COUNT);

	/// The hand was inside a circle of this player on the previous frame
	std::array<bool, k_Players> wasInside {};
	/// wasInside is filled in
	bool haveState {false};
	/// The hand's point of the previous call (x and z; y is written 0), which every call overwrites with the current
	/// one, the first included
	glm::vec3 previousPoint {0.0f};
};

} // namespace openblack::ecs::components
