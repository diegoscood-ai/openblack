/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/MiracleFxSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The miracles' looks, run by the modules that draw them: the hand's effects (magic::hand_fx, whose state is in the
/// hand's magic state), the globes (magic::one_off) and the piles (PotArchetype). It keeps no state of its own.
class MiracleFxSystem final: public MiracleFxSystemInterface
{
public:
	void UpdateGlobes(float gameSeconds) override;
	void UpdateHand(float gameSeconds) override;
	void UpdatePiles(float seconds) override;
	void SeedInHand(entt::entity seed, int powerUp, int previousPowerUp) override;
	void SeedLeftHand() override;
	void SeedShakenOff() override;
	void Reset() override;
	void SetInterface(const gui::GameInterface* interface) override;
	void StartTribalPowerRing(Tribe tribe) override;
	void StopTribalPowerRing() override;
	void ReleaseTribalPowerRing(Tribe tribe, glm::vec3 handPosition) override;
	void TribalPowerColumn(Tribe tribe, glm::vec3 position, PlayerNames player) override;
	[[nodiscard]] std::vector<const magic::tribal_spin::Runner*> GetTribalPowerRunners() const override;
	[[nodiscard]] std::vector<OrientedTextVertex> GetTribalPowerText(const magic::tribal_spin::Runner& runner) const override;
	[[nodiscard]] const graphics::Texture2D* GetTextTexture() const override;
};

} // namespace openblack::ecs::systems
