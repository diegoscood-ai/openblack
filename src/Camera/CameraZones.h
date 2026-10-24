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
#include <span>
#include <vector>

#include <glm/vec2.hpp>
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

/// Where across the ground a camera outside the fence is put back inside it: tested along the line towards where it
/// started the frame, or towards what it looks at when it has hardly moved; it goes past the fence where that line
/// crosses it by `searchRadius`, or the first of the points round there, turning a 64th of a turn at a time either
/// way, that is inside; onto the crossing itself when none is. The test also gives the inclusion distance that the
/// scripts read.
inline constexpr float k_SearchRadius = 20.0f;
/// While the player turns the camera by dragging round the edge of the screen it is put back further in
inline constexpr float k_EdgeTurnSearchRadius = 50.0f;
[[nodiscard]] constexpr float SearchRadius(bool edgeTurning)
{
	return edgeTurning ? k_EdgeTurnSearchRadius : k_SearchRadius;
}
struct FenceKeep
{
	/// The point that the camera goes to across the ground (its x and z), none when the camera is inside
	std::optional<glm::vec3> target;
	/// Inside the fence, the distance across the ground from the camera to the fence along the line of the test; 0
	/// when the fence puts the camera back, and k_NoFenceDistance when the fence is off
	float inclusionDistance {0.0f};
};
/// The inclusion distance when the fence is off
inline constexpr float k_NoFenceDistance = 1e10f;
[[nodiscard]] FenceKeep PushInsideFence(std::span<const glm::vec3> fence, bool fenceOn, const glm::vec3& originAtStart,
                                        const glm::vec3& origin, const glm::vec3& focus, float searchRadius);

/// What the fence does to a point that moves with the camera when it puts the camera back to `newOrigin`: the point
/// keeps its place from the camera, one axis at a time
[[nodiscard]] constexpr glm::vec3 MovedWithOrigin(const glm::vec3& point, const glm::vec3& origin, const glm::vec3& newOrigin)
{
	return (point - origin) + newOrigin;
}

/// Putting the camera back inside shakes it this hard within this distance of where it is put, for this long
inline constexpr float k_FenceShakeRadius = 100.0f;
inline constexpr float k_FenceShakeStrength = 1.0f;
inline constexpr int32_t k_FenceShakeMilliseconds = 400;
/// And sounds the in-game bank's out of bounds sample at the camera, 3D, played once (mode 2)
inline constexpr int k_OutOfBoundsSample = 43;

/// The disc round the map's middle that the player's camera is kept inside, across the ground whatever its height. On
/// the original maps, 512 cells a side, its middle is at 2560, 2560 and its radius is the map's side, 5120; maps of
/// another size scale both
struct MapDisc
{
	glm::vec2 centre {0.0f};
	float radius {0.0f};
};
[[nodiscard]] MapDisc MapDiscFor(uint16_t cellsPerSide);
/// How far a point is from the disc's middle across the ground
[[nodiscard]] float AcrossFromMiddle(const MapDisc& disc, const glm::vec3& point);
/// The player camera's origin and focus after the disc's step of its frame
struct MapDiscKeep
{
	glm::vec3 origin {0.0f};
	glm::vec3 focus {0.0f};
	bool broughtBack = false;
};
/// The disc's step of the player camera's frame. The origin and the focus are measured from the disc's middle (at
/// height 0) and put back in single precision, which rounds every coordinate below half the middle's, negative ones
/// included, and turns a height of minus zero into zero, even when the camera is inside. Beyond the disc the origin is
/// brought back across the ground to its edge, keeping its height
[[nodiscard]] MapDiscKeep KeepInsideMapDisc(const MapDisc& disc, const glm::vec3& origin, const glm::vec3& focus);

} // namespace openblack::camera_zones
