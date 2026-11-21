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

#include <glm/vec3.hpp>

/// The world's camera as the player comes out of the temple (docs/wiki/interface/temple-interior.md): the way in leaves
/// the camera's modes as they are, and the way out starts only the player's own camera again
namespace openblack::world_camera
{

/// The model that the world's camera has as the player comes out of the temple
enum class Model : uint8_t
{
	/// The player's own camera of the world
	World,
	/// Another model, for example the camera that follows the creature
	Other,
};

/// A place for the camera: where it is and where it looks
struct Place
{
	glm::vec3 origin;
	glm::vec3 focus;
};

/// What the world's camera does as the player comes out of the temple
struct Leave
{
	/// The camera goes to this place at once, with no movement
	std::optional<Place> cutTo;
	/// A new camera of the player's own takes over at the place
	bool newWorldModel {false};
	/// The player's camera starts again from where it is
	bool startAgain {false};
};

/// Coming out of the temple. With a place picked on the map, the camera goes to the place at once, and the player's own
/// camera starts again there, or a new one takes over from another model. With no place picked, only the player's own
/// camera starts again; another model, as the camera that follows the creature, continues.
[[nodiscard]] constexpr Leave OnLeave(std::optional<Place> picked, Model model)
{
	return {.cutTo = picked, .newWorldModel = picked.has_value() && model == Model::Other, .startAgain = model == Model::World};
}

} // namespace openblack::world_camera
