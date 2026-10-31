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

#include <array>
#include <optional>
#include <span>

#include <glm/vec3.hpp>

#include "Creature/CaveCopyPick.h"

/// Where a line meets a creature's skin as it is drawn, as the game's mesh test finds it: every triangle of the body's
/// nearest level of detail, posed and shaped, tested from either side; the nearest hit ahead of the line's start
/// wins. The hit keeps its triangle and where on it the line met it, so that the same spot can be found on the body
/// as it moves. Wiki: docs/wiki/creature/fights.md#fights-the-players-presses.
namespace openblack::creature_skin_ray
{

/// A ray parallel to a triangle's plane to within this misses it: |unit normal . direction| must be larger
inline constexpr float k_Epsilon = 0.005f;

/// A spot on the skin: the part (sub-mesh) and the triangle's three corners in it, and how far along its first edge
/// (u) and its second (v) the spot is from its first corner
struct Spot
{
	uint32_t part {0};
	std::array<uint16_t, 3> corners {};
	float u {0.0f};
	float v {0.0f};
};

struct Hit
{
	Spot spot;
	/// How many of the direction's lengths along the line, and the point there
	float distance {0.0f};
	glm::vec3 point {0.0f};
};

/// The nearest triangle of the parts the line from start along direction meets ahead of its start. A triangle whose
/// spot on its two edges cannot be worked out (its second edge's x too small, or the two edges' too alike) is not
/// counted, as the game's test leaves it
[[nodiscard]] std::optional<Hit> Nearest(std::span<const cave_copy_pick::Part> parts, const glm::vec3& start,
                                         const glm::vec3& direction);

/// The spot on the parts as they are posed now: the first corner plus u of the first edge and v of the second; none
/// when the parts no longer hold it
[[nodiscard]] std::optional<glm::vec3> PointOf(std::span<const cave_copy_pick::Part> parts, const Spot& spot);

} // namespace openblack::creature_skin_ray
