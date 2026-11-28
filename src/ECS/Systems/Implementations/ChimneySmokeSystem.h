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

#include <glm/vec3.hpp>

#include "3D/ChimneySmoke.h"
#include "ECS/Systems/ChimneySmokeSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class ChimneySmokeSystem final: public ChimneySmokeSystemInterface
{
public:
	void Attach(entt::entity abode, const graphics::L3DMesh& mesh, const components::Transform& transform,
	            bool workshop) override;
	void UpdateHandWind() override;
	[[nodiscard]] glm::vec3 Drift(const glm::vec3& chimney) override;

private:
	chimney_smoke::HandWind _handWind;
};

} // namespace openblack::ecs::systems
