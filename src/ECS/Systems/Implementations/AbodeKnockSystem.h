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
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

#include <cstdint>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/AbodeKnock.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"

namespace openblack::ecs::systems
{

class AbodeKnockSystem final: public AbodeKnockSystemInterface
{
public:
	bool Tap(entt::entity abode, glm::vec3 handPoint, bool ownHand) override;
	void Update(std::chrono::milliseconds frame, const glm::vec3& cameraRight) override;
	[[nodiscard]] entt::entity GetReadoutTown() const override { return _knockedTown; }
	[[nodiscard]] float GetReadoutScale() const override { return _scale; }
	[[nodiscard]] std::vector<KnockReadoutPerson> PeopleOver(entt::entity abode) const override;

private:
	/// The town last knocked on, null for none
	entt::entity _knockedTown {entt::null};
	/// Milliseconds left of the read-out, 0 when it isn't showing
	int32_t _remaining {0};
	/// The steps and the scale they make, as the game starts them: full
	int32_t _steps {abode_knock::k_ReadoutSteps};
	float _scale {1.0f};
	/// The camera's right, as the last frame that ran the read-out down left it; the world's x at the start
	glm::vec3 _right {1.0f, 0.0f, 0.0f};
};

} // namespace openblack::ecs::systems
