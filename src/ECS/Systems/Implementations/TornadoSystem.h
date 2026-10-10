/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/TornadoSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The game's tornadoes behind the service: the take goes to the physics of carried objects, and what is carried is
/// kept by the tornado's particles (Particles/Rules/Storm.cpp), so this holds no state of its own
class TornadoSystem final: public TornadoSystemInterface
{
public:
	bool Carry(entt::entity object) override;

	void Update() override;
	[[nodiscard]] size_t CarriedCount() const override;
};

} // namespace openblack::ecs::systems
