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

#include <span>
#include <vector>

#include <glm/vec3.hpp>

/// The camera zones a land's scripts set: a fence the world camera is kept inside, limits on its height and places it
/// is kept out of. Pure rules, tested without the game.
namespace openblack::camera_zones
{

/// Without a limit of its own the camera may go this high, and this high above the land
inline constexpr float k_NoLimit = 30000.0f;
/// The heights the zones hold until a file gives others
inline constexpr float k_DefaultLimit = 500.0f;

/// A place the camera is kept out of
struct Exclusion
{
	enum class Kind : uint8_t
	{
		Dome,
		Cylinder,
	};
	glm::vec3 position {0.0f};
	float radius {0.0f};
	float height {0.0f};
	Kind kind {Kind::Dome};
};

struct Zones
{
	/// Whether the camera is kept out of the exclusions and under its height limits
	bool exclusionsOn {true};
	/// Whether the camera is kept inside the fence
	bool fenceOn {false};
	/// Whether the highest the camera may go is `maxAltitude`, and the highest above the land `heightAboveLand`
	bool useMaxAltitude {false};
	bool useHeightAboveLand {false};
	float maxAltitude {k_DefaultLimit};
	float heightAboveLand {k_DefaultLimit};
	/// The fence's corners in order; only their ground's directions count
	std::vector<glm::vec3> fence;
	std::vector<Exclusion> exclusions;
};

/// Where a line from a point crosses the fence
struct Crossing
{
	/// Whether the point is inside: the line crosses the fence an odd number of times ahead of it. On the fence, or
	/// with fewer than three corners or the fence off, it is.
	bool inside {true};
	/// The nearest crossing ahead (or else behind) the point, or a corner nearer the point than that, or the nearest
	/// corner when the line crosses nowhere
	glm::vec3 closest {0.0f};
	/// Across the side crossed, flat on the ground
	glm::vec3 normal {0.0f, 0.0f, 1.0f};
};
[[nodiscard]] Crossing CrossFence(std::span<const glm::vec3> fence, bool fenceOn, const glm::vec3& point,
                                  const glm::vec3& direction);

} // namespace openblack::camera_zones
