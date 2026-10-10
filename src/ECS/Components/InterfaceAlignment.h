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

/// The alignment the local player's interface shows, from 0, evil, to 1, good: that of the player with the most
/// influence at the camera. The sky stores (1 - x) x 2 of it and starts at 1 (neutral), so it starts at 0.5. It lasts
/// the whole game: the local player's entity carries it while it exists, and the player system keeps it between lands
struct InterfaceAlignment
{
	float value {0.5f};
};

} // namespace openblack::ecs::components
