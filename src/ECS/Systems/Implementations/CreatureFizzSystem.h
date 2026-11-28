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

#include <vector>

#include "ECS/Systems/CreatureFizzSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureFizzSystem final: public CreatureFizzSystemInterface
{
public:
	void SetFizz(entt::entity creature, float target, float seconds, bool goesForGood) override;
	void ProcessTurn() override;
	void BeginCreatureTurns() override;
	void ProcessCreatureTurn(entt::entity creature) override;
	void EndCreatureTurns() override;
	[[nodiscard]] bool FizzedOut(entt::entity creature) const override;
	void UpdateFrame(float seconds) override;
	[[nodiscard]] float FizzOf(entt::entity creature) const override;
	[[nodiscard]] glm::vec2 EyeStaticScroll() const override;

private:
	glm::vec2 _eyeStaticScroll {0.0f};
	/// The turn's length, and the creatures that fizzed right out for good this turn (removed at its end)
	uint32_t _turnMs {0};
	std::vector<entt::entity> _gone;
};

} // namespace openblack::ecs::systems
