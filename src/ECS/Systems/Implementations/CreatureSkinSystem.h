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

#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureSkinSystem final: public CreatureSkinSystemInterface
{
public:
	void Update() override;
	void ProcessTurn() override;
	void BeginCreatureTurns() override;
	void ProcessCreatureTurn(entt::entity creature) override;
	void EndCreatureTurns() override;
	void SetTattoo(entt::entity creature, size_t slot, const creature_tattoo::Slot& tattoo) override;
	void RemakeTattooMasks(entt::entity creature, creature_tattoo::Sheet sheet) override;
	void PutOnTattoos(entt::entity creature, const creature_tattoo::Slots& slots) override;
	[[nodiscard]] const creature_tattoo::Masks* TattooMasks(PlayerNames player) const override;
	void AddWound(entt::entity creature, const creature_marks::Mark& wound) override;
	void AddBlood(entt::entity creature, const creature_marks::Mark& blood) override;
	void Heal(entt::entity creature, uint32_t counts) override;
	void Bleed(entt::entity creature, const creature_marks::MeshCut& cut) override;
	void BleedTurn() override;
	void BleedTurnOf(entt::entity creature) override;

private:
	/// Each player's masks, by the player's number. They outlive the lands, and a mask never made stays none.
	std::array<creature_tattoo::Masks, static_cast<size_t>(PlayerNames::_COUNT)> _tattooMasks {};
};

} // namespace openblack::ecs::systems
