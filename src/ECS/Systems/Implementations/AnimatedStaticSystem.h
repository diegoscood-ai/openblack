/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/
#pragma once

#ifndef LOCATOR_IMPLEMENTATIONS
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

#include <cstdint>

#include <optional>
#include <vector>

#include "ECS/Systems/AnimatedStaticSystemInterface.h"

namespace openblack::ecs::systems
{

class AnimatedStaticSystem final: public AnimatedStaticSystemInterface
{
public:
	bool SetOpenState(entt::entity object, int32_t openState) override;
	[[nodiscard]] std::optional<uint32_t> GateStoneValue(entt::entity object) const override;
	bool LayGateStone(entt::entity plinth, entt::entity stone) override;
	[[nodiscard]] std::optional<std::vector<animated_static::RouteCircle>> RouteCircles(entt::entity gate) const override;
	void Draw(entt::entity object, uint32_t frameMs) override;
	void EndDrawnFrame() override;
	/// The plinths' stones are placed by Draw; nothing calls it
	void Update(uint32_t turn, float turnFraction) override;
	void Reset() override;

private:
	/// The clip's own time follows the place, the model posed there
	static void PoseClip(entt::entity object, uint32_t place);
	/// The plinth's stones made, placed and unmade so that exactly the ones it draws this frame exist
	void PlaceStones(entt::entity plinth);
	/// A stone taken away, and with it its plinth's list of drawn stones
	static void DestroyStone(entt::entity stone);

	/// The plinths whose Draw ran this frame
	std::vector<entt::entity> _drawnPlinths;
	/// Every stone model made for a plinth and not yet taken away
	std::vector<entt::entity> _stones;
};

} // namespace openblack::ecs::systems
