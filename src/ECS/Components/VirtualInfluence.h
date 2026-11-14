/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Common/VirtualInfluence.h"

namespace openblack::ecs::components
{

/// What the local player's hand keeps of their influence past the border, on the local player's entity. It lasts the
/// whole game, not the land: the player system keeps it between lands, until the next land's local player takes it
struct VirtualInfluence
{
	virtual_influence::State state;
};

} // namespace openblack::ecs::components
