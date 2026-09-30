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

struct BigForest
{
	int type;
	/// +0x84 (Tree::GetWoodValue): the wood left; the forest is scaled to wood / its info's woodValue
	float wood {0.0f};
	float woodValue {1.0f}; ///< the info's woodValue
};

struct Forest
{
	int type;
};

} // namespace openblack::ecs::components
