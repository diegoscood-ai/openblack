/*******************************************************************************
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

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

namespace openblack::ecs::components
{
struct CameraBookmark
{
	uint8_t number;
	/// Where the camera stood when it was dropped, from the land point of its map position then: a flight to it puts the
	/// camera at the same offset from the land point of its map position at the flight
	glm::vec3 cameraOffset {0.0f};
	/// Where it was dropped, as a map position: its marker stands above that position's land point. None while it is
	/// not set
	std::optional<map_coords::MapCoords> position;
	/// The thing it was dropped on, which it follows once a turn: its marker stands on it, and once it is no longer
	/// available the bookmark stays where it last was
	std::optional<entt::entity> target;
};

} // namespace openblack::ecs::components
