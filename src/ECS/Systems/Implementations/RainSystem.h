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

#include "ECS/Systems/RainSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The streaks start all at 0, and are placed at the engine's start-up (Reset), not when a land opens
class RainSystem final: public RainSystemInterface
{
public:
	/// The game's: the CRT random numbers
	RainSystem();
	explicit RainSystem(rain::Random random);

	void Reset() override;
	void Update(float seconds, const glm::vec3& camera) override;
	[[nodiscard]] std::vector<rain::Tile> TakeTiles(const glm::vec3& camera) override;
	[[nodiscard]] std::span<const rain::Streak> GetStreaks() const override { return _streaks; }
	[[nodiscard]] float GetHeight() const override { return _fall.height; }
	[[nodiscard]] rain::Fall GetFall() const override { return _fall; }

private:
	rain::Random _random;
	std::array<rain::Streak, rain::k_Streaks> _streaks {};
	rain::Fall _fall;
	/// Whether the streaks were drawn since the last update, which moves them on only then
	bool _drawn {false};
};

} // namespace openblack::ecs::systems
