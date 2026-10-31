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

#include <span>
#include <vector>

#include <glm/vec2.hpp>

namespace openblack::ecs::systems
{

/// The snow lying on the island (see snow_cover): the storms that snow pile it up and it melts away
/// (Locator::snowSystem). The weather's storms lay their snow as they move on, then the atmosphere melts it, once a
/// game turn.
/// What a saved game keeps of the snow lying, as Restore takes it back
struct SavedSnow
{
	std::vector<float> depths;
	std::vector<float> blockChanges;
	float meltClock {0.0f};
	int32_t meltBand {0};
};

class SnowSystemInterface
{
public:
	virtual ~SnowSystemInterface() = default;

	/// As a new land opens: no snow anywhere
	virtual void Reset() = 0;
	/// A storm snowing `amount` within its inner radius of its centre, less out to its outer one (snow_cover::AddStorm)
	virtual void AddStorm(glm::vec2 centre, float innerRadius, float outerRadius, float amount) = 0;
	/// Once a game turn, after the storms have moved on: some of the snow melts
	virtual void Melt(float seconds) = 0;
	/// A loaded game's snow: the depth of every cell (row by row along z), the change over every land block since its
	/// textures last took it in, and the melting clock and band, as the game keeps them; a span of another size is
	/// left out
	virtual void Restore(std::span<const float> depths, std::span<const float> blockChanges, float meltClock,
	                     int32_t meltBand) = 0;

	/// The snow as a saved game writes it: every cell's depth, every land block's change, the melting clock and band
	[[nodiscard]] virtual SavedSnow Saved() const = 0;

	/// How deep the snow lies in each cell of the grid, row by row along z
	[[nodiscard]] virtual std::span<const float> GetDepths() const = 0;
	/// How deep the snow lies at a point
	[[nodiscard]] virtual float GetDepth(glm::vec2 xz) const = 0;
	/// Whether snow lies anywhere: without it nothing is drawn differently
	[[nodiscard]] virtual bool HasSnow() const = 0;
	/// Changes whenever the snow does, so that what is drawn from it is only refreshed then
	[[nodiscard]] virtual uint32_t GetRevision() const = 0;
	/// The land blocks (x, z) whose textures the snow's changes ask to be painted again since the last call
	/// (snow_cover::MarkChangedBlocks, as the melt lists them)
	[[nodiscard]] virtual std::vector<glm::ivec2> TakeMarkedBlocks() { return {}; }
};

} // namespace openblack::ecs::systems
