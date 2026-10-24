/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>

namespace openblack::ecs::systems
{

/// Animates the map's banks of mist (components::Mist)
class MistSystemInterface
{
public:
	virtual ~MistSystemInterface() = default;
	/// Moves the animation of every mist in view on by the game time that has passed, which is 0 while the game is
	/// paused. A mist off screen keeps its frame, so the mists drift out of step with each other.
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;
};

} // namespace openblack::ecs::systems
