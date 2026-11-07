/******************************************************************************
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

/// A creature a script has set to follow the local player's creature's size (CREATURE_AUTOSCALE): each turn, while it
/// is on, it grows or shrinks halfway towards that creature's size times the factor (creature_size::AutoscaleStep)
struct CreatureAutoscale
{
	bool enabled {false};
	float factor {0.0f};
};

} // namespace openblack::ecs::components
