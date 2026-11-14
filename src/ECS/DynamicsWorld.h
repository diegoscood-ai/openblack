/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/vec3.hpp>

namespace openblack
{
class LandIslandInterface;
namespace v120
{
struct InfoConstants;
}
using InfoConstants = v120::InfoConstants;
namespace filesystem
{
class FileSystemInterface;
}
namespace resources
{
class ResourcesInterface;
}
namespace ecs
{
class Registry;
namespace systems
{
class BuildingDamageSystemInterface;
}
} // namespace ecs
} // namespace openblack

/// What the physics needs of the rest of the game: the entities, the land, the game's tables, the camera, the files and
/// resource caches it loads from, and the broken buildings its bodies strike. The game's own world works through the
/// game's services; tests give a fake with their own entities.
namespace openblack::ecs::dynamics
{

class World
{
public:
	virtual ~World() = default;

	[[nodiscard]] virtual Registry& Entities() = 0;
	[[nodiscard]] virtual const Registry& Entities() const = 0;
	/// The land, none before one is loaded
	[[nodiscard]] virtual const LandIslandInterface* Land() const = 0;
	/// The game's tables, none before they are loaded
	[[nodiscard]] virtual const InfoConstants* Info() const = 0;
	/// Where the camera is, none without a camera
	[[nodiscard]] virtual std::optional<glm::vec3> CameraOrigin() const = 0;
	/// The files the constants and the dust's texture are read from
	[[nodiscard]] virtual filesystem::FileSystemInterface& FileSystem() = 0;
	/// The caches of the meshes, textures and blobs
	[[nodiscard]] virtual resources::ResourcesInterface& Resources() = 0;
	/// The broken buildings: a body's blow breaks them, and their pieces come to rest
	[[nodiscard]] virtual systems::BuildingDamageSystemInterface& BuildingDamage() = 0;
	/// How deep the snow lies at a point, cut to a whole number and kept within 0 to 255
	[[nodiscard]] virtual int32_t SnowAt(glm::vec3 point) const = 0;
	/// The land's light of the moment (0xRRGGBB), which a dust in the snow takes on
	[[nodiscard]] virtual uint32_t LandColour() const = 0;
};

} // namespace openblack::ecs::dynamics
