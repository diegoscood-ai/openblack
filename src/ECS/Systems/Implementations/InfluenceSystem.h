/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/InfluenceSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The influence service over the game's own influence code (ECS/Influence). It keeps nothing of its own: the rings,
/// the circles, the ripples and the borders shown are kept with the land, so a new land starts them afresh.
class InfluenceSystem final: public InfluenceSystemInterface
{
public:
	void ProcessTurn(uint32_t turn) override;
	void UpdateBorders() override;
	void Update(std::chrono::duration<float, std::milli> gameTime) override;

	[[nodiscard]] float PlayerInfluence(PlayerNames player, const glm::vec3& position) const override;
	[[nodiscard]] float PlayerInfluence(PlayerNames player, const glm::vec3& position, influence::CalcType type,
	                                    bool includeAllies) const override;
	[[nodiscard]] bool IsInAntiInfluence(PlayerNames player, const glm::vec3& position) const override;

	[[nodiscard]] std::span<const influence::Circle> GetCircles() const override;
	[[nodiscard]] bool IsBorderShown(PlayerNames player) const override;
	[[nodiscard]] std::span<const influence::Ripple> GetRipples() const override;
};

} // namespace openblack::ecs::systems
