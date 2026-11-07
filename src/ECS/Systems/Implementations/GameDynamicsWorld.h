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

#include "ECS/DynamicsWorld.h"

namespace openblack::ecs::systems
{

/// The physics' world in the game: the game's entities, land, tables, camera, files, resource caches and broken
/// buildings, through the locator
class GameDynamicsWorld final: public dynamics::World
{
public:
	[[nodiscard]] Registry& Entities() override;
	[[nodiscard]] const Registry& Entities() const override;
	[[nodiscard]] const LandIslandInterface* Land() const override;
	[[nodiscard]] const InfoConstants* Info() const override;
	[[nodiscard]] std::optional<glm::vec3> CameraOrigin() const override;
	[[nodiscard]] int32_t SnowAt(glm::vec3 point) const override;
	[[nodiscard]] uint32_t LandColour() const override;
	[[nodiscard]] filesystem::FileSystemInterface& FileSystem() override;
	[[nodiscard]] resources::ResourcesInterface& Resources() override;
	[[nodiscard]] BuildingDamageSystemInterface& BuildingDamage() override;
};

} // namespace openblack::ecs::systems
