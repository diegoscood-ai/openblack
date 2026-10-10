/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/ExplosionSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The rubble is our marks on the ground (ECS/GroundMarks), the shakes our camera shakes (Camera/CameraShake); it keeps
/// nothing of its own
class ExplosionSystem final: public ExplosionSystemInterface
{
public:
	void AddRubble(const glm::vec3& centre, float yaw) override;
	void AddShake(const glm::vec3& position, float radius, float strength, float seconds) override;
	[[nodiscard]] bool IsShaking() const override;
	void Update(float milliseconds) override;
	void Reset() override;
};

} // namespace openblack::ecs::systems
