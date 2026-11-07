/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#include <optional>
#include <string_view>

#include <glm/vec3.hpp>

#include "Camera/CameraDrag.h"

/// The hand's pose and its up while it moves the camera, from the camera's hints: gripping the land it pans, turning
/// the camera round the edge of the screen, tilting it, and offering to do so with the cursor at the edge
namespace openblack::hand_navigation_pose
{

enum class Pose : uint8_t
{
	/// The hand's ordinary pose
	Idle,
	Grip,
	Rotate,
	Pitch,
	Zoom,
};

/// The hand animation that shows a pose (hndspec5.txt's names); the ordinary pose is the wiggle
[[nodiscard]] constexpr std::string_view ClipName(Pose pose)
{
	switch (pose)
	{
	case Pose::Grip:
		return "Cgrip";
	case Pose::Rotate:
		return "Crotate";
	case Pose::Pitch:
		return "Cpitch";
	case Pose::Zoom:
		return "Czoom";
	case Pose::Idle:
		break;
	}
	return "Cwiggle";
}

/// The hand's pose while it drags the land: the grip once the drag pans the land, else the camera's hint, turning
/// first, then zooming, then tilting, each only when the camera help allows it
[[nodiscard]] inline Pose WhileDragging(bool gripsLand, uint32_t tricons, uint32_t features = camera_drag::k_DefaultFeatures)
{
	namespace tricon = camera_drag::tricon;
	namespace feature = camera_drag::feature;
	if (gripsLand)
	{
		return Pose::Grip;
	}
	if ((tricons & tricon::k_Rotate) != 0)
	{
		return (features & feature::k_Rotate) != 0 ? Pose::Rotate : Pose::Idle;
	}
	if ((tricons & tricon::k_Zoom) != 0)
	{
		return (features & feature::k_Zoom) != 0 ? Pose::Zoom : Pose::Idle;
	}
	if ((tricons & tricon::k_Pitch) != 0 && (features & feature::k_Pitch) != 0)
	{
		return Pose::Pitch;
	}
	return Pose::Idle;
}

/// The hand's pose hovering with nothing under it, where the cursor's place offers turning or tilting the camera; none
/// leaves the hand to its ordinary poses
[[nodiscard]] inline std::optional<Pose> WhileHovering(uint32_t tricons, uint32_t features = camera_drag::k_DefaultFeatures)
{
	namespace tricon = camera_drag::tricon;
	namespace feature = camera_drag::feature;
	if ((tricons & tricon::k_Rotate) != 0)
	{
		return (features & feature::k_Rotate) != 0 ? std::optional(Pose::Rotate) : std::nullopt;
	}
	if ((tricons & tricon::k_Pitch) != 0 && (features & feature::k_Pitch) != 0)
	{
		return Pose::Pitch;
	}
	return std::nullopt;
}

/// How far the hand's up leans up towards the camera's focus, as a share of the distance to it
constexpr float k_UpLeanShare = 0.3f;

/// Offering to turn the camera, the hand stands up towards where the camera looks: the way from the hand to the focus,
/// raised by three tenths of its length, made a unit vector. Worked as the original does it, each step rounded to a
/// float: the length is the square root of (y² + x²) + z², and so is the raised way's. A way whose three parts all count
/// as zero (zero or not a number) is left as it is
[[nodiscard]] inline glm::vec3 UpTowardsFocus(glm::vec3 from, glm::vec3 focus)
{
	const auto towards = focus - from;
	const float xx = towards.x * towards.x;
	const float zz = towards.z * towards.z;
	const float length = std::sqrt((towards.y * towards.y + xx) + zz);
	glm::vec3 up(towards.x, length * k_UpLeanShare + towards.y, towards.z);
	const auto countsAsZero = [](float value) { return !(value < 0.0f) && !(value > 0.0f); };
	if (countsAsZero(up.x) && countsAsZero(up.y) && countsAsZero(up.z))
	{
		return up;
	}
	const float inverse = 1.0f / std::sqrt((up.y * up.y + xx) + zz);
	return {up.x * inverse, up.y * inverse, up.z * inverse};
}

/// Showing that it turns the camera while it drags the land, the hand is drawn this share of its height higher
constexpr float k_RotateLiftShare = 0.33f;

/// How much higher the hand is drawn showing that it turns the camera: its height (3.2 times its scale) times the
/// share, in that order
[[nodiscard]] inline float RotateLift(float handScale)
{
	constexpr float k_HandHeight = 3.2f;
	return k_HandHeight * handScale * k_RotateLiftShare;
}

} // namespace openblack::hand_navigation_pose
