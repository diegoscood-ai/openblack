/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/MagicShieldSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The shields behind the service: each method hands on to the shields under src/Magic, which keep their lists in the
/// magic object and spell services
class MagicShieldSystem final: public MagicShieldSystemInterface
{
public:
	void ProcessTurn() override;
	void Update(float seconds) override;
	void Reset() override;
	[[nodiscard]] bool KeepsReactionOff(glm::vec3 watcher, glm::vec3 initiator) const override;
};

} // namespace openblack::ecs::systems
