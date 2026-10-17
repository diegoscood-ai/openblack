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

#include <glm/vec3.hpp>

/// The player camera watching a creature fight: how far from the arena it gives up, and how it turns round the two
/// fighters. Pure maths, tested on its own; the world camera keeps the state.
namespace openblack::fight_orbit
{

/// The watch starts this many of the second fighter's radii out, turned a quarter turn and tilted down this much
constexpr float k_StartDistance = 5.5f;
constexpr float k_StartYaw = 1.57079637f;
constexpr float k_StartPitch = 0.408407032f;
/// Zooming out past this many radii ends the watch; the distance stays within 0 and it
constexpr float k_MaxDistance = 40.0f;
/// The tilt stays within these
constexpr float k_MinPitch = 0.241660982f;
constexpr float k_MaxPitch = 1.32913542f;
/// A unit of zoom moves the camera this much
constexpr float k_ZoomShare = 0.3f;
/// The camera's focus and its turn follow the fighters over this many seconds
constexpr float k_FollowSeconds = 5.0f;
/// Once the fight is over the camera lingers on it this many milliseconds
constexpr int32_t k_LingerMs = 3000;
/// Dragging the land away from the arena ends the watch this long after the camera's last flight, with the arena's
/// radius counted at this share
constexpr float k_DragAwaySeconds = 1.5f * 3.0f;
constexpr float k_DragAwayShare = 0.75f;
/// How many arena radii away the camera is too far: its focus and itself, or itself alone
constexpr float k_QuitFocusRadii = 3.2f;
constexpr float k_QuitCameraRadii = 4.2f;
constexpr float k_QuitFarRadii = 6.0f;
/// The camera eases into the orbit's place over 2.5 times a pace: 2 right after the camera's mode changed, 0.8 once the
/// mode is more than 2 seconds old, and in between from one towards the other
constexpr float k_EaseScale = 2.5f;
constexpr float k_EaseStartPace = 2.0f;
constexpr float k_EaseSettledPace = 0.8f;
constexpr float k_EaseSettleSeconds = 2.0f;

/// Whether a camera at a point, looking at a point on the land, is too far from an arena to watch a fight in it: its
/// focus more than 3.2 and itself more than 4.2 radii away across the land, or itself more than 6. The radius counts
/// at a share; the heights are not looked at
[[nodiscard]] bool WantToQuit(glm::vec3 arenaCentre, float arenaRadius, glm::vec3 camera, glm::vec3 focus, float share);

/// An angle brought within -pi and pi: as it is when already there, else the part of a turn left over, wrapped by a
/// turn at most twice
[[nodiscard]] float WrapAngle(float angle);

/// The middle of the two fighters
[[nodiscard]] glm::vec3 Middle(glm::vec3 fighterA, glm::vec3 fighterB);
/// How far apart the two fighters are across the land
[[nodiscard]] float Spacing(glm::vec3 fighterA, glm::vec3 fighterB);
/// How far the camera is from the fighters' middle: the distance in the second fighter's radii, plus their spacing and
/// the first fighter's radius
[[nodiscard]] float OrbitDistance(float distance, float spacing, float radiusA, float radiusB);
/// The distance, in the second fighter's radii, after a zoom moved the camera by 0.3 a unit of zoom
[[nodiscard]] float Zoom(float distance, float zoom, float spacing, float radiusA, float radiusB);
/// The distance kept within 0 and 40, and the tilt within its bounds. Not a number goes to the lower bound, as the
/// original's two comparisons (not above the lower bound, then below the upper one) send it
[[nodiscard]] float ClampDistance(float distance);
[[nodiscard]] float ClampPitch(float pitch);

/// The heading of the line from the first fighter to the second, or the turn the camera has when they are within a
/// hundredth of each other across the land
[[nodiscard]] float FightersHeading(glm::vec3 fighterA, glm::vec3 fighterB, float current);
/// Where the camera's turn goes to follow the fighters' heading: the shortest way round from where it is
[[nodiscard]] float TurnTowards(float current, float heading);

/// How many seconds the camera takes to the orbit's place, from the seconds since the camera's mode last changed: 5
/// right after the change, 2 once the mode is more than 2 seconds old
[[nodiscard]] float EaseSeconds(float modeSeconds);

} // namespace openblack::fight_orbit
