/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace openblack::ecs::events
{
/// The ground under one thing before a land change
struct GroundBefore
{
	entt::entity thing {entt::null};
	float ground {0.0f};
};

/// The land's altitudes changed in a box of corners (the vortex's and the temple's flattening). Published once the
/// land has been rebuilt, so the height under a point is already the new one. It carries the ground under each thing
/// that follows the land, read just before the altitudes were rewritten (ecs::land_reseat::RecordGrounds)
struct LandAltitudesChanged
{
	/// The first and last corner (cell coordinates) whose altitude may have changed, both included
	glm::ivec2 minCorner {0};
	glm::ivec2 maxCorner {0};
	std::vector<GroundBefore> groundsBefore {};
};

/// LOAD_MAP: a script asks for another land, by its map script's path in the game folder. The game changes land while
/// the call is handled, so the script that asked goes on on the new land
struct LandChangeRequested
{
	std::filesystem::path path;
};
} // namespace openblack::ecs::events
