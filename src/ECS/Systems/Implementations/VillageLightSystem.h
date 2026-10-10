/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "3D/NightLightsState.h"
#include "ECS/Systems/VillageLightSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The night lights (night_lights::Update) over the land light of the frame, with their state as the game starts it
class VillageLightSystem final: public VillageLightSystemInterface
{
public:
	/// The lights draw from the C runtime's stream (game_random::crt::Random); a test gives a fake one
	VillageLightSystem();
	explicit VillageLightSystem(openblack::night_lights::Draw random);

	void Update(std::chrono::duration<float, std::milli> gameTime) override;
	void AddLight(entt::entity lantern, const glm::vec3& position, uint8_t type) override;
	[[nodiscard]] bool IsDark() const override { return _dark; }

	[[nodiscard]] openblack::night_lights::State& GetState() noexcept override { return _state; }

private:
	openblack::night_lights::Draw _random;
	openblack::night_lights::State _state;
	bool _dark {false};
};
} // namespace openblack::ecs::systems
