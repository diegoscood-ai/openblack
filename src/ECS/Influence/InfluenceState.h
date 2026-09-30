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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

// Private to ECS/Influence: the GGame fields the influence reads, kept on one registry entity so that a new land (the
// registry's Reset) puts them back to the GGame::Init defaults before the map script runs.

namespace openblack::influence::detail
{
struct InfluenceGlobals
{
	int32_t landNumber {0};              ///< g_game+0x205A08
	float townMultiplier {1.0f};         ///< g_game+0x250078
	float playerMultiplier {1.0f};       ///< g_game+0x25007C
	/// g_game+0x205C4C: the rings, newest first (the ctor pushes at the head of the list)
	std::vector<entt::entity> rings;
};

/// The land's globals (made on first use)
InfluenceGlobals& Globals();
/// Read only: the defaults when there is no registry entity yet
[[nodiscard]] const InfluenceGlobals& GlobalsOrDefault();

/// GetDistanceInMetres 0x74CD70: x,z only
[[nodiscard]] float DistanceXZ(const glm::vec3& a, const glm::vec3& b);
} // namespace openblack::influence::detail
