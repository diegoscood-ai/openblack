/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <functional>

#include "ECS/Systems/SnowfallSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class SnowfallSystem final: public SnowfallSystemInterface
{
public:
	/// Where the frame's snow could be drawn about the camera, before any is taken
	using TileSource = std::function<std::vector<snowfall::Tile>(const glm::vec3& camera)>;

	/// The game's: the CRT random numbers, and the snow over the land blocks of the island
	SnowfallSystem();
	SnowfallSystem(snowfall::Random random, TileSource tiles);

	void Scatter() override;
	void Update(float seconds, float fallSpeed, float height) override;
	[[nodiscard]] std::vector<snowfall::Tile> TakeTiles(const glm::vec3& camera) override;
	[[nodiscard]] std::span<const snowfall::Flake> GetFlakes() const override { return _flakes; }

private:
	snowfall::Random _random;
	TileSource _tiles;
	std::array<snowfall::Flake, snowfall::k_Flakes> _flakes {};
	/// Whether the flakes were drawn since the last update, which moves them on only then
	bool _drawn {false};
	/// How high the flakes start, the rain's height as of the last update
	float _height {160.0f};
};

} // namespace openblack::ecs::systems
