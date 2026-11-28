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

namespace openblack::ecs::components
{

struct BigForest
{
	int type;
	/// The wood left; the forest is scaled to wood / its info's woodValue
	float wood {0.0f};
	float woodValue {1.0f}; ///< the info's woodValue
	/// Its Forest (made at creation; that forest points back at the BigForest)
	uint32_t forestId {0};
};

/// A forest a creature can believe in: each forest of the forest system has an entity with this, made with the forest
/// and destroyed with it (ForestSystemInterface::Create)
struct Forest
{
	/// Its key in the forest system's list
	uint32_t key {0};
};

} // namespace openblack::ecs::components
