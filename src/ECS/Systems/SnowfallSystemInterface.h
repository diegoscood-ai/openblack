/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>
#include <vector>

#include <glm/vec3.hpp>

#include "3D/Snowfall.h"

namespace openblack::ecs::systems
{

/// The snow the world shows falling where the weather snows (see snowfall), Locator::snowfallSystem
class SnowfallSystemInterface
{
public:
	virtual ~SnowfallSystemInterface() = default;

	/// Every flake at a new place, each at a height of its own: what the atmosphere's start-up does, twice
	virtual void Scatter() = 0;
	/// Once a frame, after the rain: the flakes fall as the rain does, from as high and as fast, if they were drawn the
	/// frame before
	virtual void Update(float seconds, float fallSpeed, float height) = 0;
	/// The snow to draw this frame over the land blocks about the camera. Taking some counts the flakes as drawn.
	[[nodiscard]] virtual std::vector<snowfall::Tile> TakeTiles(const glm::vec3& camera) = 0;
	[[nodiscard]] virtual std::span<const snowfall::Flake> GetFlakes() const = 0;
};

} // namespace openblack::ecs::systems
