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

#include <vector>

#include "Creature/CreatureLightbulb.h"

namespace openblack::ecs::components
{

/// The light bulbs a creature shows over its head as it learns from what it sees, and the turn it last made one
struct CreatureLightbulbs
{
	uint32_t lastTurn {0};
	std::vector<creature_lightbulb::Bulb> bulbs;
	/// How each bulb is drawn this frame, in the same order, as the frame aged them (empty when the world is not drawn)
	std::vector<creature_lightbulb::Look> looks;
};

} // namespace openblack::ecs::components
