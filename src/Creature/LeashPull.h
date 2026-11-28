/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/AffineMatrix.h"
#include "3D/MapCoords.h"

/// Pulling a tied leash. With the creature's leash tied to something, the empty hand can take hold of the rope itself:
/// pressed on the rope it holds it, two more ropes run from the hand to the creature's collar and to what the leash is
/// tied to while it pulls, and let go, the tied leash takes the hand's distance from what it is tied to as its length and
/// the creature is sent where the hand let go.
///
/// The hand finds the rope on the screen: the rope's points as the camera shows them, each stretch between two of them
/// as wide as the rope is drawn there, at least two pixels either side. The rope takes the press from what else is under
/// the hand when it is nearer the camera: an object (but never a creature), a screen object, or the land.
namespace openblack::creature_leash
{

/// The rope is found this many times its drawn half width either side of it
inline constexpr float k_RopePickWidthScale = 2.0f;
/// ... and never less than this many pixels either side
inline constexpr int32_t k_RopePickLeastWidth = 2;
/// A rope point no further than this in front of the camera is never picked
inline constexpr float k_RopePickNearest = 1.0f;

/// The hand's action state while it holds and pulls a tied leash's rope (HandSystemInterface::GetActionStateNumber)
inline constexpr int32_t k_PullActionState = 21;

/// The rope from the hand to the collar while the hand pulls is this many times as long as the tied rope
inline constexpr float k_PullCollarRopeScale = 1.5f;
/// The rope from the hand to what the leash is tied to while the hand pulls: its slack and full lengths
inline constexpr float k_PullTiedRopeSlack = 5.0f;
inline constexpr float k_PullTiedRopeFull = 2.0f;

/// Where a pull meets the tied thing: its place on the map across the ground, and the land's height there plus its
/// altitude over the land, in that order
[[nodiscard]] glm::vec3 TiedPointOf(const map_coords::MapCoords& place, float landHeight);

/// The tied leash's full length a pull sets: the hand's distance from where the tied thing stands, but never less than
/// twice that thing's radius across the ground
[[nodiscard]] float PullLength(const glm::vec3& hand, const glm::vec3& tied, float radius2D);
/// The slack length that goes with a pull's full length
[[nodiscard]] constexpr float PullSlackLength(float full)
{
	return full * 0.5f;
}

/// Whether the mouse is on the stretch of rope from pixel a to pixel b, each end that many pixels wide either side; and
/// how far along it, as the game measures it: 0 within a's end, 1 within b's, else the cosine between the stretch and
/// the line from a to the mouse
[[nodiscard]] std::optional<float> RopeSegmentHit(glm::ivec2 mouse, glm::ivec2 a, int32_t aWidth, glm::ivec2 b, int32_t bWidth);

/// A point of the rope as the camera shows it
struct RopeScreenPoint
{
	/// Its pixel
	glm::ivec2 pixel {0};
	/// How far in front of the camera it is
	float depth {0.0f};
	/// Whether it is in front of the near clip, on the screen or off it
	bool projected {false};
};
/// A rope point through the camera's world-to-clipping matrix, as the game projects a point to the screen: its depth
/// is the clip depth, and it counts as shown when that is at least the near clip, on the screen or off it. Its pixel is
/// ((X / Z + 1) half the width, (1 - Y / Z) half the height), each truncated towards zero
[[nodiscard]] RopeScreenPoint ProjectRopePoint(const affine::AffineMatrix& worldToClipping, float nearClip, glm::ivec2 screen,
                                               const glm::vec3& point);
/// The depth in front of the camera of the point the mouse is on, along the stretch from `from` to `to` as the pick
/// measures it, through the world-to-camera matrix
[[nodiscard]] float RopeHitDepth(const affine::AffineMatrix& worldToCamera, const glm::vec3& from, const glm::vec3& to,
                                 float along);

/// The stretch of rope the mouse is on: between the points `segment` and `segment + 1`, and how far along it
struct RopeHit
{
	size_t segment {0};
	float along {0.0f};
};
/// The first stretch of the rope the mouse is on, from its first point: each between two points both shown and
/// more than k_RopePickNearest in front of the camera, each point as wide as the rope is drawn there (the screen's
/// width times the rope's half width times k_RopePickWidthScale, over its depth, truncated), at least
/// k_RopePickLeastWidth pixels
[[nodiscard]] std::optional<RopeHit> PickRope(glm::ivec2 mouse, std::span<const RopeScreenPoint> points, uint16_t screenWidth,
                                              float halfWidth);

/// What else the hand would take the press on, as the rope's pick weighs it
enum class RopeRival : uint8_t
{
	/// An object under the hand, at its depth
	Object,
	/// A creature under the hand: it always keeps the press
	Creature,
	/// A screen object under the hand, at its depth
	ScreenObject,
	/// The land under the hand, at its depth plus the land's margin; with no land, an endless depth
	Land,
};
/// Whether the rope takes the press from its rival: when it is strictly nearer the camera, and the rival is no creature
[[nodiscard]] constexpr bool RopeTakesThePick(RopeRival rival, float rivalDepth, float ropeDepth)
{
	return rival != RopeRival::Creature && rivalDepth > ropeDepth;
}

} // namespace openblack::creature_leash
