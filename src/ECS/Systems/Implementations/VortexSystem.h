/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/VortexSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// Our vortices (ECS/Vortex), which keep their list in the script state; it keeps nothing of its own
class VortexSystem final: public VortexSystemInterface
{
public:
	entt::entity Create(glm::vec3 position, VortexType type) override;
	bool StartFadeOut(entt::entity vortex) override;
	void SetParameters(entt::entity vortex, entt::entity town, glm::vec3 position, float a, float b,
	                   entt::entity flock) override;
	void ProcessTurn() override;
	void UpdateGroundEffects(float seconds) override;
	void UpdateOverLandEffects(float seconds) override;
};

} // namespace openblack::ecs::systems
