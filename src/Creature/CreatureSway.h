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

/// How a creature's body sways: things thrown at it and the leash dragging it kick its lower or upper body, which
/// swings back on a damped spring and is drawn leaning by four lean animations played on top of its pose
namespace openblack::creature_sway
{
/// The lean animations in the creature spec file: the upper and lower body leaning sideways and front to back
inline constexpr uint32_t k_UpperSide = 194;
inline constexpr uint32_t k_UpperFrontBack = 195;
inline constexpr uint32_t k_LowerSide = 196;
inline constexpr uint32_t k_LowerFrontBack = 197;

/// A kick is taken by the upper body when it lands higher above the land than this many times the creature's size
inline constexpr float k_UpperHeightPerSize = 9.0f;
/// No kick leaves the body swinging faster than this
inline constexpr float k_MaxKickSpeed = 8.0f;
/// Within the spring the body never swings faster than this
inline constexpr float k_MaxSwingSpeed = 6.0f;
/// The spring's stiffness and damping, each for half the creature's mass
inline constexpr float k_Stiffness = -30.0f;
inline constexpr float k_Damping = 5.0f;
/// Each turn is stepped in this many parts
inline constexpr int k_Substeps = 10;
/// How fast the push of the leash comes through, a share of the gap a turn
inline constexpr float k_DriveEase = 0.15f;
/// The sway settles, and stops being stepped and drawn, once every offset and speed is this small
inline constexpr float k_SettledLength = 0.05f;
/// A lean this small isn't drawn
inline constexpr float k_DrawnLean = 0.0001f;
/// The leash drags with this many times the creature's mass, by how hard it pulls (components::CreatureLeash::pull);
/// the body leans against it at this share of that pull
inline constexpr float k_LeashForcePerMass = 15.0f;
inline constexpr float k_LeashLean = -0.9f;
/// The leash only drags the body while it pulls harder than this
inline constexpr float k_LeashPulling = 0.05f;

/// The body's two swinging parts and what drives them
struct Sway
{
	glm::vec3 lowerVelocity {0.0f};
	glm::vec3 upperVelocity {0.0f};
	glm::vec3 lowerOffset {0.0f};
	glm::vec3 upperOffset {0.0f};
	/// The steady push of the leash, eased towards each turn's
	glm::vec3 drive {0.0f};
	/// Something kicked it and it hasn't settled yet
	bool active {false};
	/// The seconds of the creature's last turn, which turn a kick's force into speed
	float frameSeconds {0.0f};
};

/// A force kicks the body at a point: the upper body when the point is higher above the land than its split height,
/// else the lower; only across the land, by the force over the last turn for half the creature's mass, no faster than
/// the kick's limit
void Kick(Sway& sway, glm::vec3 force, float heightAboveLand, float splitHeight, float mass);

/// The leash's pull on the creature this turn, towards where it is held, by how hard it pulls
[[nodiscard]] glm::vec3 LeashForce(glm::vec3 creature, glm::vec3 holder, float mass, float pull);

/// One turn of the spring: the drive eases towards the turn's push, then each part is stepped in parts, its speed
/// kept within the limit, until both settle. Every length is summed from z to x, as the original sums it
void Step(Sway& sway, glm::vec3 framePush, float seconds, float mass);

/// How far through a lean animation to draw a lean along one of the creature's axes, by its duration in milliseconds
[[nodiscard]] uint32_t LeanTime(float lean, uint32_t duration);
} // namespace openblack::creature_sway
