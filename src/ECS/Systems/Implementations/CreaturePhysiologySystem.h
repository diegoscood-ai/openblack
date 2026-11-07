/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include "ECS/Systems/CreaturePhysiologySystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreaturePhysiologySystem final: public CreaturePhysiologySystemInterface
{
public:
	void ProcessTurn() override;
	void ProcessShownFatness() override;

	void Eat(entt::entity creature, float foodValue) override;
	[[nodiscard]] float HungerAfterMeal(entt::entity creature, float hunger, float foodValue) const override;
	void Drink(entt::entity creature) override;
	void Poo(entt::entity creature) override;
	void WakeFromFaint(entt::entity creature) override;
	void Sit(entt::entity creature) override;
	void FinishAction(entt::entity creature, std::string_view action) override;
	void ModifyStrength(entt::entity creature, float amount) override;
	[[nodiscard]] std::optional<creature_physiology::Needs> NeedsOf(entt::entity creature) const override;
	void SetNeeds(entt::entity creature, const creature_physiology::Needs& needs) override;
	[[nodiscard]] std::optional<float> GrowthOf(entt::entity creature) const override;
	void GrowNow(entt::entity creature) override;
	[[nodiscard]] std::optional<float> LongEnoughAsleepOf(entt::entity creature) const override;

	void SetTimeScale(float scale) override;
	[[nodiscard]] float GetTimeScale() const override;
	void SetFaintingEnabled(bool enabled) override;
	[[nodiscard]] bool IsFaintingEnabled() const override;

private:
	float _timeScale {1.0f};
	/// Turns of the body owed from a fractional time scale
	float _owedTurns {0.0f};
	bool _fainting {true};
};

} // namespace openblack::ecs::systems
