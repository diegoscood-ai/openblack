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

#include <chrono>
#include <functional>
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
		/// A kind the file gives other than those two: no point is inside it
		Other,
	};
	glm::vec3 position {0.0f};
	float radius {0.0f};
	float height {0.0f};
	Kind kind {Kind::Dome};
	/// Marked in its file as one that goes with the map: the map's clear takes it away, where the others stay until the
	/// scripts restart or a script sets zones again
	bool saved {false};
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

/// The zones once the map is cleared with its land: the switches and limits back to their start, with no fence, and the
/// exclusions marked to go with the map gone; the others are kept
[[nodiscard]] Zones AfterMapClear(Zones zones);

/// The domes the game itself puts over things, apart from the zones' files: one over every creature, which follows its
/// body each turn, and one over every temple, which follows the temple's size. They never go with the map's clear, and
/// no saved game holds them: each is made again by its thing.
/// A dome's radius and height are each held between 0 and this, a value that is not above 0 (or not a number) being 0
inline constexpr float k_DomeLimit = 10000.0f;
[[nodiscard]] float HeldDomeSize(float value);
/// A dome as it is first made: a height of 0 is the radius; both then held
[[nodiscard]] Exclusion MadeDome(const glm::vec3& position, float radius, float height);
/// A dome moved and sized again: both held, a height of 0 kept
[[nodiscard]] Exclusion AdjustedDome(const glm::vec3& position, float radius, float height);
/// A creature's dome is its body's radius and its standing height (15 times its size), each times this
inline constexpr float k_CreatureDomeScale = 1.35f;
inline constexpr float k_StandingHeightPerSize = 15.0f;
/// The radius and height of a creature's dome: the body's radius times 1.35, and 1.35 times its standing height, from
/// the size it is drawn at (already held between its limits)
struct DomeSize
{
	float radius;
	float height;
};
[[nodiscard]] DomeSize CreatureDomeSize(float bodyRadius, float size);
/// A temple's dome is this wide whatever its size, and as high as its mesh (twice the mesh's half height)
inline constexpr float k_TempleDomeRadius = 15.0f;
[[nodiscard]] DomeSize TempleDomeSize(float meshHalfHeight);

/// How far beyond its radius, and its height for a dome, an exclusion reaches
inline constexpr float k_ExclusionMargin = 3.0f;
/// Whether a point is inside one of the exclusions, the margin added: across the ground strictly within the radius,
/// and for a dome also strictly inside the ellipsoid of that radius across and that height up (a point that is not a
/// number counts as inside the dome); an exclusion of another kind has nothing inside
[[nodiscard]] bool InsideExclusion(std::span<const Exclusion> exclusions, const glm::vec3& point);

/// The exclusions near enough to matter this frame, from where the camera was heading at its start: strictly within
/// twice the margin of their edge across the ground, and for a dome, with the camera less than its height and twice the
/// margin above the dome's base. The camera's land probe and its push out of the cylinders read only these
[[nodiscard]] std::vector<Exclusion> NearbyExclusions(std::span<const Exclusion> exclusions, const glm::vec3& originAtStart);

/// The camera pushed out of the cylinders, one after the other: one it is strictly within the margin of, across the
/// ground, sends it straight out from the cylinder's axis to the margin, at the same height (its distance from the axis
/// taken as at least 0.01). How many pushed it
struct CylinderPush
{
	glm::vec3 origin;
	uint8_t pushes {0};
};
inline constexpr float k_NearestToCylinderAxis = 0.01f;
[[nodiscard]] CylinderPush PushOutOfCylinders(std::span<const Exclusion> nearby, const glm::vec3& origin);

/// The land under the camera: its height and its normal at a point across the ground
struct Ground
{
	std::function<float(glm::vec2)> height;
	std::function<glm::vec3(glm::vec2)> normal;
};
/// The height the camera's land probe feels at a point: the land, raised to the roof of any dome over the point while
/// `withDomes` (the zones keep the camera out of their exclusions). A dome's roof is half an ellipsoid, as high as the
/// dome at its middle and down to its base at its edge; a point strictly within the radius is under it, and one whose
/// distance is not a number counts as under it too
[[nodiscard]] float SurfaceHeight(const Ground& ground, std::span<const Exclusion> nearby, bool withDomes, glm::vec2 point);

/// The camera after its land probe
struct GroundPush
{
	glm::vec3 origin;
	glm::vec3 focus;
	/// Whether any point pushed it, and whether what it looks at moved too
	bool pushed {false};
	bool focusMoved {false};
	/// How many of the sixteen points pushed it, and how many of those were pushed off a dome's roof
	uint8_t points {0};
	uint8_t onDomes {0};
};
/// The camera's land probe: sixteen points on a ball of the margin's radius round the camera, turned to face the way
/// it looks (rings an eighth of a turn apart from straight below it to straight above, of 1, 4, 6, 4 and 1 points).
/// Each point below what the probe feels (SurfaceHeight) pushes the camera out along the surface's normal by how deep
/// it is, at most half the margin: the land's normal, or, where a dome's roof is above the land, the way from the dome's
/// middle to the point, whether or not the zones keep the camera out of their exclusions. A push with a part away from
/// what the camera looks at moves that too, by the push times that part. Each point is felt from where the pushes
/// before it left the camera, the ball keeping the way it faced at the start
inline constexpr int k_ProbePoints = 16;
[[nodiscard]] GroundPush PushOffTheGround(const Ground& ground, std::span<const Exclusion> nearby, bool exclusionsOn,
                                          const glm::vec3& origin, const glm::vec3& focus);

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
/// Whether a point is strictly inside the disc across the ground (the squares compared; a point that is not a number
/// is not)
[[nodiscard]] bool InsideMapDisc(const MapDisc& disc, const glm::vec3& point);
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

/// The furthest the camera may go in a frame: 1.6 times the distance to what it looks at each second, but at least
/// 300 (raffclar's, from his camera limits)
inline constexpr float k_MinFrameMove = 300.0f;
inline constexpr float k_FrameMovePerFocusDistance = 1.6f;
[[nodiscard]] float FrameMoveLimit(float focusDistance, float seconds);
/// Where the camera goes when it would move further than `limit` from where it was at the start of the frame: that far
/// along the way it was going. Nothing when it moves no further than that.
[[nodiscard]] std::optional<glm::vec3> LimitFrameMove(const glm::vec3& start, const glm::vec3& origin, float limit);

/// The highest the zones let the camera go, and the highest above the land, when they set none
inline constexpr float k_NoHeightLimit = 30000.0f;
/// The slide under the highest height moves along the view only when the view rises or falls more than this, a float's
/// hundredth compared as a double
inline constexpr double k_SlideLeastRise = static_cast<double>(0.01f);

/// The lowest and the highest the camera may stand in a frame, from the land under it before the frame's moves
struct HeightBounds
{
	/// The land plus the floating height
	float lowest;
	/// The zones' highest (maxAltitude, else k_NoHeightLimit), or the land plus the most above it (heightAboveLand,
	/// else k_NoHeightLimit) when that is not above it or the two don't compare
	float highest;
};
[[nodiscard]] HeightBounds HeightBoundsAt(float ground, float floatingHeight, const Zones& zones);

/// The camera, what it looks at and the point it turns about, kept between the heights
struct HeightKeep
{
	glm::vec3 origin;
	glm::vec3 focus;
	glm::vec3 turningPoint;
	/// Below the lowest (or not a number), the camera alone was lifted to it
	bool raised;
	/// With the exclusions on and above the highest: the three slid together along the view, from the camera towards
	/// what it looks at, until the camera is at the highest; a view that hardly rises or falls is left where it is,
	/// but this still counts
	bool slid;
};
/// The lowest height first, then the slide under the highest
[[nodiscard]] HeightKeep KeepBetweenHeights(const HeightBounds& bounds, bool exclusionsOn, const glm::vec3& origin,
                                            const glm::vec3& focus, const glm::vec3& turningPoint);

/// How long the player's camera eases to where its frame put it once a flight's start is past: 0.3 seconds, twice that
/// when the frame's limits moved what it looks at, and twice again when they pushed the camera
inline constexpr int64_t k_EaseMicroseconds = 300'000;
[[nodiscard]] constexpr std::chrono::microseconds EaseMicroseconds(bool pushed, bool focusMoved)
{
	return std::chrono::microseconds {k_EaseMicroseconds * (focusMoved ? 2 : 1) * (pushed ? 2 : 1)};
}

/// The distance between two points as the game works it out: the root of the squares added z, y then x
[[nodiscard]] float DistanceZYX(const glm::vec3& a, const glm::vec3& b);

/// What the camera looks at, set again at a distance along its view from it
[[nodiscard]] glm::vec3 FocusAlongView(const glm::vec3& origin, const glm::vec3& focus, float distance);

} // namespace openblack::camera_zones
