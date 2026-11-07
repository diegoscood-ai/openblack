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

#include <optional>
#include <stdexcept>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "3D/AllMeshes.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Systems/LivingActionSystemInterface.h"

namespace openblack::test
{
/// The base of the tests' fake state tables: the methods that only the scripts and the other villager code call, which
/// the code under those tests never reaches. Each one throws, so that a test that does reach one fails loudly
class LivingActionFakeBase: public ecs::systems::LivingActionSystemInterface
{
public:
	bool VillagerSetCurrentAndDestinationState(ecs::components::LivingAction& /*action*/, VillagerStates /*current*/,
	                                           VillagerStates /*destination*/) const override
	{
		throw std::logic_error("VillagerSetCurrentAndDestinationState is not faked");
	}
	void VillagerPlayAnimThenSetState(ecs::components::LivingAction& /*action*/, VillagerStates /*next*/) const override
	{
		throw std::logic_error("VillagerPlayAnimThenSetState is not faked");
	}
	void VillagerSetTopStateToFinal(ecs::components::LivingAction& /*action*/) const override
	{
		throw std::logic_error("VillagerSetTopStateToFinal is not faked");
	}
	[[nodiscard]] bool VillagerIsReadyForNewAnimation(const ecs::components::LivingAction& /*action*/,
	                                                  uint32_t /*times*/) const override
	{
		throw std::logic_error("VillagerIsReadyForNewAnimation is not faked");
	}
	[[nodiscard]] bool VillagerCanBeDirected(entt::entity /*villager*/) const override
	{
		throw std::logic_error("VillagerCanBeDirected is not faked");
	}
	void VillagerSetScriptState(entt::entity /*villager*/, VillagerStates /*state*/) const override
	{
		throw std::logic_error("VillagerSetScriptState is not faked");
	}
	void VillagerScriptMoveTo(entt::entity /*villager*/, glm::vec2 /*goal*/, float /*goalAltitude*/) const override
	{
		throw std::logic_error("VillagerScriptMoveTo is not faked");
	}
	void VillagerSetScriptAnimation(entt::entity /*villager*/, AnimId /*clip*/, uint32_t /*plays*/) const override
	{
		throw std::logic_error("VillagerSetScriptAnimation is not faked");
	}
	[[nodiscard]] bool VillagerHasPlayedScriptAnimation(entt::entity /*villager*/) const override
	{
		throw std::logic_error("VillagerHasPlayedScriptAnimation is not faked");
	}
	void VillagerFace(entt::entity /*villager*/, glm::vec2 /*point*/) const override
	{
		throw std::logic_error("VillagerFace is not faked");
	}
	[[nodiscard]] std::optional<float> VillagerYAngle(entt::entity /*villager*/) const override
	{
		throw std::logic_error("VillagerYAngle is not faked");
	}
	void VillagerSetYAngle(entt::entity /*villager*/, float /*angle*/) const override
	{
		throw std::logic_error("VillagerSetYAngle is not faked");
	}
	void VillagerOverrideAnimation(entt::entity /*villager*/, int32_t /*clip*/) const override
	{
		throw std::logic_error("VillagerOverrideAnimation is not faked");
	}
	void VillagerSetAge(entt::entity /*villager*/, uint32_t /*age*/) const override
	{
		throw std::logic_error("VillagerSetAge is not faked");
	}
};
} // namespace openblack::test
