/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "ECS/Components/LivingAction.h"
#include "ECS/Systems/LivingActionSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class LivingActionSystem final: public LivingActionSystemInterface
{
public:
	/// After the one living list (living_turn::ProcessLiving, ECS/LivingTurn.h): the souls of the dead
	void Update() override;

	[[nodiscard]] VillagerStates VillagerGetState(const components::LivingAction& action,
	                                              components::LivingAction::Index index) const override;
	/// Our villager's GetFinalState (ECS/Villager/VillagerCore.h)
	[[nodiscard]] VillagerStates VillagerGetFinalState(const components::LivingAction& action) const override;
	void VillagerSetState(components::LivingAction& action, components::LivingAction::Index index, VillagerStates state,
	                      bool skipTransition) const override;
	uint32_t VillagerCallState(components::LivingAction& action, components::LivingAction::Index index) const override;
	/// The entry function of the table row `row`, told the final state from before the change and the state
	/// entered: 1 = accepted, 0x23 = accepted and the states set by it, else refused. An empty slot is 1 (no function)
	uint32_t VillagerCallEntryState(components::LivingAction& action, VillagerStates row, VillagerStates final,
	                                VillagerStates next) const override;
	/// The exit function of the table row `row`, told the state that follows: 1 = it may leave. Empty: 1
	uint32_t VillagerCallExitState(components::LivingAction& action, VillagerStates row, VillagerStates next) const override;
	int VillagerCallOutOfAnimation(components::LivingAction& action, components::LivingAction::Index index) const override;
	bool VillagerCallValidate(components::LivingAction& action, components::LivingAction::Index index) const override;
	/// Our villager's SetCurrentAndDestinationState (ECS/Villager/VillagerCore.h): whether its code says it went
	bool VillagerSetCurrentAndDestinationState(components::LivingAction& action, VillagerStates current,
	                                           VillagerStates destination) const override;
	/// Our villager's PlayAnimThenSetState (ECS/Villager/VillagerScript.h)
	void VillagerPlayAnimThenSetState(components::LivingAction& action, VillagerStates next) const override;
	/// Our villager's SetTopStateToFinal (ECS/Villager/VillagerCore.h)
	void VillagerSetTopStateToFinal(components::LivingAction& action) const override;
	/// Its clip as WAIT_FOR_ANIMATION reads it (villager_clips::ClipPlayed); always for one without a clip
	[[nodiscard]] bool VillagerIsReadyForNewAnimation(const components::LivingAction& action, uint32_t times) const override;
	/// villager_script's (Implementations/VillagerScript.h)
	[[nodiscard]] bool VillagerCanBeDirected(entt::entity villager) const override;
	void VillagerSetScriptState(entt::entity villager, VillagerStates state) const override;
	void VillagerScriptMoveTo(entt::entity villager, glm::vec2 goal, float goalAltitude) const override;
	void VillagerSetScriptAnimation(entt::entity villager, AnimId clip, uint32_t plays) const override;
	void VillagerSetScriptPlace(entt::entity villager, glm::vec2 place) const override;
	void VillagerSetScriptNumber(entt::entity villager, float number) const override;
	[[nodiscard]] bool VillagerHasPlayedScriptAnimation(entt::entity villager) const override;
	/// The script's focus on the point on the ground (living::SetFocus, ECS/LivingAngle.h)
	void VillagerFace(entt::entity villager, glm::vec2 point) const override;
	/// Its wall hug's Y angle
	[[nodiscard]] std::optional<float> VillagerYAngle(entt::entity villager) const override;
	/// Our villager's SetYAngle (ECS/Villager/VillagerCore.h)
	void VillagerSetYAngle(entt::entity villager, float angle) const override;
	/// The clip set on it from its start (villager_animate::OverrideAnim); the caller gives a clip the game has
	void VillagerOverrideAnimation(entt::entity villager, int32_t clip) const override;
	/// Our villager's SetAgeAndScale (ECS/Villager/VillagerAge.h)
	void VillagerSetAge(entt::entity villager, uint32_t age) const override;
};
} // namespace openblack::ecs::systems
