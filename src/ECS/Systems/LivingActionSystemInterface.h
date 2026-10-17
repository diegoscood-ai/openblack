/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "3D/AllMeshes.h"
#include "ECS/Components/LivingAction.h"

namespace openblack::ecs::systems
{

class LivingActionSystemInterface
{
public:
	virtual ~LivingActionSystemInterface() = default;
	virtual void Update() = 0;

	[[nodiscard]] virtual VillagerStates VillagerGetState(const components::LivingAction& action,
	                                                      components::LivingAction::Index index) const = 0;
	/// The state the villager works towards: its top state when that is a final one, else its final state
	[[nodiscard]] virtual VillagerStates VillagerGetFinalState(const components::LivingAction& action) const = 0;
	virtual void VillagerSetState(components::LivingAction& action, components::LivingAction::Index index, VillagerStates state,
	                              bool skipTransition) const = 0;
	[[nodiscard]] virtual uint32_t VillagerCallState(components::LivingAction& action,
	                                                 components::LivingAction::Index index) const = 0;
	/// The entry function of the table row `row`, told the final state from before the change and the state
	/// entered: 1 = accepted, 0x23 = accepted and the states set by it, else refused. An empty slot is 1 (no function)
	virtual uint32_t VillagerCallEntryState(components::LivingAction& action, VillagerStates row, VillagerStates final,
	                                        VillagerStates next) const = 0;
	/// The exit function of the table row `row`, told the state that follows: 1 = it may leave. Empty: 1
	[[nodiscard]] virtual uint32_t VillagerCallExitState(components::LivingAction& action, VillagerStates row,
	                                                     VillagerStates next) const = 0;
	[[nodiscard]] virtual int VillagerCallOutOfAnimation(components::LivingAction& action,
	                                                     components::LivingAction::Index index) const = 0;
	virtual bool VillagerCallValidate(components::LivingAction& action, components::LivingAction::Index index) const = 0;
	/// Goes into a state on the way to another, as the game sets both at once: 1 when it went, else the code of the
	/// refusal, the exit's or the entry's (one the entry refused leaves the villager deciding what to do)
	virtual uint32_t VillagerSetCurrentAndDestinationState(components::LivingAction& action, VillagerStates current,
	                                                       VillagerStates destination) const = 0;
	/// Lets the clip the villager plays now play to its end, then goes into the next state
	virtual void VillagerPlayAnimThenSetState(components::LivingAction& action, VillagerStates next) const = 0;
	/// Goes on into the state the villager works towards
	virtual void VillagerSetTopStateToFinal(components::LivingAction& action) const = 0;
	/// Whether the villager's clip has played through so many times since its state changed
	[[nodiscard]] virtual bool VillagerIsReadyForNewAnimation(const components::LivingAction& action, uint32_t times) const = 0;

	/// Whether a script can direct the villager: it is available and in the map. The callers that also refuse a
	/// drowning villager test that themselves
	[[nodiscard]] virtual bool VillagerCanBeDirected(entt::entity villager) const = 0;
	/// A script puts the villager straight into a state
	virtual void VillagerSetScriptState(entt::entity villager, VillagerStates state) const = 0;
	/// A script sends the villager to a point, where it waits for the script; nothing for one out of the map or drowning
	virtual void VillagerScriptMoveTo(entt::entity villager, glm::vec2 goal) const = 0;
	/// The clip a script asks the villager to play in the playing state, and how many times
	virtual void VillagerSetScriptAnimation(entt::entity villager, AnimId clip, uint32_t plays) const = 0;
	/// Whether the villager has played the clip a script asked for
	[[nodiscard]] virtual bool VillagerHasPlayedScriptAnimation(entt::entity villager) const = 0;
	/// The villager turns at once to face a point
	virtual void VillagerFace(entt::entity villager, glm::vec2 point) const = 0;
	/// The way the villager faces, as an angle about the upright; none for a thing that isn't one
	[[nodiscard]] virtual std::optional<float> VillagerYAngle(entt::entity villager) const = 0;
	/// The villager turns at once to face the way of an angle about the upright
	virtual void VillagerSetYAngle(entt::entity villager, float angle) const = 0;
	/// A script makes the villager play a clip in place of its state's own, until its state next chooses one
	virtual void VillagerOverrideAnimation(entt::entity villager, int32_t clip) const = 0;
	/// A script gives the villager an age, making it a child or an adult
	virtual void VillagerSetAge(entt::entity villager, uint32_t age) const = 0;
};

} // namespace openblack::ecs::systems
