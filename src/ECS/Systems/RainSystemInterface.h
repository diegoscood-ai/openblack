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

#include "3D/Rain.h"

namespace openblack::ecs::systems
{

/// The rain the world shows where the weather rains: its streaks, how high they reach and how fast they fall
class RainSystemInterface
{
public:
	virtual ~RainSystemInterface() = default;

	/// New streaks, falling as in calm air: what the atmosphere's start-up does, twice
	virtual void Reset() = 0;
	/// The Weather detail setting is off at the atmosphere's start-up: the game then makes no rain at all, so from now on
	/// no streak moves, no rain is drawn and no drop splashes. How high and fast the rain would fall still follows the
	/// storms
	virtual void Disable() = 0;
	/// Whether the game has its rain (not after Disable)
	[[nodiscard]] virtual bool IsPresent() const { return true; }
	/// Once a frame: the rain's fall follows the storm nearest the camera, and the streaks move on if they were drawn
	virtual void Update(float seconds, const glm::vec3& camera) = 0;
	/// The rain to draw this frame over the land blocks about the camera; any block's rain queued to be drawn, even one
	/// that shows nothing, counts the streaks as drawn
	[[nodiscard]] virtual std::vector<rain::Tile> TakeTiles(const glm::vec3& camera) = 0;
	/// As a tile is drawn: that many drops of its rain splash on the block, each a ring on the land or the sea, when the
	/// rain is strong enough and the splashes are on
	virtual void Splash(const rain::Tile& /*tile*/, uint8_t /*drops*/) {}
	/// The debug switch of the splashes, on unless turned off
	virtual void SetSplashesEnabled(bool /*enabled*/) {}
	[[nodiscard]] virtual bool AreSplashesEnabled() const { return false; }
	[[nodiscard]] virtual std::span<const rain::Streak> GetStreaks() const = 0;
	/// How high the streaks reach over the ground
	[[nodiscard]] virtual float GetHeight() const = 0;
	/// How high and how fast the rain falls, which the snow falls as too
	[[nodiscard]] virtual rain::Fall GetFall() const = 0;
};

} // namespace openblack::ecs::systems
