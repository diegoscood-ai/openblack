/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

#include "ECS/BuildingDamageWorld.h"

namespace openblack::ecs::systems
{

/// Breaking buildings in the game: the game's entities, resource caches and physics, through the locator
class GameBuildingDamageWorld final: public building_world::World
{
public:
	[[nodiscard]] Registry& Entities() override;
	[[nodiscard]] const Registry& Entities() const override;
	[[nodiscard]] resources::ResourcesInterface& Resources() override;
	[[nodiscard]] DynamicsSystemInterface& Dynamics() override;
};

} // namespace openblack::ecs::systems
