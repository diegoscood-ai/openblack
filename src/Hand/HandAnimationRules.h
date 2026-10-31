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

#include <string_view>

#include <glm/vec2.hpp>

#include "Enums.h"

/// How the player's hand is posed each frame: its lean with the trailing cursor, and which of its clips it shows over
/// what is under it and for what it holds.
///
/// Pure functions, tested on their own; the hand's system gathers the facts and plays the clips.
namespace openblack::hand_animation
{

/// The clips of the hand's animation file the choices below name
inline constexpr std::string_view k_Wiggle = "Cwiggle";
inline constexpr std::string_view k_CanPickUp = "Ccan_pickup";
inline constexpr std::string_view k_Stroke = "Cstroke";
inline constexpr std::string_view k_HoldAbove = "Chold_above";
inline constexpr std::string_view k_HoldSide = "Chold_side";
inline constexpr std::string_view k_HoldFingers = "Chold_fingers";

/// The interface's hand state that says the hand can pick up what is under it
inline constexpr int32_t k_CanPickUpState = 9;
/// The hand's drawn state for its normal (empty) pose, the only one that shows the camera's navigation poses
inline constexpr int32_t k_NormalState = 1;

/// Over an object the hand can stroke: the can-pick-up pose when the interface says the hand can pick it up, else the
/// stroke
[[nodiscard]] constexpr std::string_view OverObject(int32_t interfaceHandState) noexcept
{
	return interfaceHandState == k_CanPickUpState ? k_CanPickUp : k_Stroke;
}

/// The normal state's clip before what is under the hand is looked at: the fingers closing over the leash's rope when
/// the hand is on the tied leash's rope or pulls it, else the wiggle
[[nodiscard]] constexpr std::string_view EntryClip(bool onLeashRope) noexcept
{
	return onLeashRope ? k_HoldFingers : k_Wiggle;
}

/// Whether the empty hand over nothing shows the camera's navigation poses: only in its normal state, not in the camera
/// state (dragging the land), and not over a screen object, a spell icon or the tied leash's rope (nor while it pulls
/// it). Otherwise it keeps its entry clip
[[nodiscard]] constexpr bool ShowsNavigationPose(bool cameraState, int32_t renderHandState, bool overScreenObject,
                                                 bool overSpellIcon, bool onLeashRope) noexcept
{
	return !cameraState && renderHandState == k_NormalState && !overScreenObject && !overSpellIcon && !onLeashRope;
}

/// The normal state's time for its pose, from its clock gone on by the frame: the fingers over the leash's rope are
/// held at half their length while the leash is pulled, else at their start (which the clock then keeps); any other
/// pose at the clock's time
[[nodiscard]] constexpr uint32_t NormalPoseTime(uint32_t clockMs, std::string_view clip, uint32_t clipMs,
                                                bool leashPulled) noexcept
{
	if (clip == k_HoldFingers)
	{
		return leashPulled ? clipMs / 2 : 0;
	}
	return clockMs;
}

} // namespace openblack::hand_animation
