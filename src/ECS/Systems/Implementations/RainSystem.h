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
	void Disable() override { _present = false; }
	[[nodiscard]] bool IsPresent() const override { return _present; }
	void Update(float seconds, const glm::vec3& camera) override;
	[[nodiscard]] std::vector<rain::Tile> TakeTiles(const glm::vec3& camera) override;
	void Splash(const rain::Tile& tile, uint8_t drops) override;
	void SetSplashesEnabled(bool enabled) override { _splashes = enabled; }
	[[nodiscard]] bool AreSplashesEnabled() const override { return _splashes; }
	[[nodiscard]] std::span<const rain::Streak> GetStreaks() const override { return _streaks; }
	[[nodiscard]] float GetHeight() const override { return _fall.height; }
	[[nodiscard]] rain::Fall GetFall() const override { return _fall; }

private:
	rain::Random _random;
	std::array<rain::Streak, rain::k_Streaks> _streaks {};
	rain::Fall _fall;
	/// Whether a block's rain was queued to be drawn since the last update, which moves the streaks on only then
	bool _drawn {false};
	/// The splashes' debug switch
	bool _splashes {true};
	/// Whether the game has its rain: not when the Weather detail setting was off at the start-up
	bool _present {true};
};

} // namespace openblack::ecs::systems
