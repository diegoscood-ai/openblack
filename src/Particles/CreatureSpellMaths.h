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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The maths of the creature spells' particles: the wisps that fly from the hand and wind round the creature, the flies
// that circle an itchy creature's head, the hearts that come off a creature made nice and the steam off a frozen
// creature's feet. Free of state, so they are tested on their own.

namespace openblack::particles::maths
{

/// The box round a creature's bones the wisps wind about: its middle across the land, its foot and height, and half
/// its width across the land's diagonal
struct WispBox
{
	float centreX {0.0f};
	float centreZ {0.0f};
	float bottom {0.0f};
	float height {0.0f};
	float radius {0.0f};
};
[[nodiscard]] WispBox WispBoxOf(std::span<const glm::vec3> bones);

/// A wisp winds this much wider and higher than the box, wobbling by this share of its radius
inline constexpr float k_WispReach = 1.2f;
inline constexpr float k_WispWobble = 0.2f;
/// Up to this share of its height a wisp winds at full width; above it narrows by the taper
inline constexpr float k_WispFullWidthBelow = 0.7f;
/// Where a wisp winds about the box, at its angles round and up and its age in seconds; worked out in the original's
/// order and rounding (gutils::x87), each sine and cosine unrounded into the operation after it
[[nodiscard]] glm::vec3 WispOrbit(const WispBox& box, float theta, float phi, float taper, float age);

/// A wisp flies out of the hand to where it winds over this many seconds
inline constexpr float k_WispFlightSeconds = 2.0f;
/// Where a wisp is, flying from the hand to its place round the creature
[[nodiscard]] glm::vec3 WispPosition(glm::vec3 hand, glm::vec3 orbit, float age);

/// A wisp's opacity, 0 to 255: from 20 to 50 over its first half second
[[nodiscard]] uint8_t WispAlpha(float age);

/// How many wisps are due after a step: they come up to the most over the emitting time
[[nodiscard]] float WispsDue(float due, int most, float dt, float emitSeconds);

/// The hearts and the steam come at most atoms over the time each lasts, a second: whether that rate is above 0 (one
/// that is not a number is not), worked out at the original's precision (gutils::x87)
[[nodiscard]] bool HasAtomRate(int most, float lifetime);
/// How many hearts or puffs of steam are due after a step: the rate times the step, added to those due, in the
/// original's rounding (gutils::x87)
[[nodiscard]] float AtomsDue(float due, int most, float lifetime, float dt);
/// A heart or a puff of steam is flung this way, a random point of the unit ball, at its speed times the creature's
/// size: that product kept unrounded to a float's range, each part of the velocity rounded once
[[nodiscard]] glm::vec3 FlungVelocity(glm::vec3 random, float speed, float size);
/// A puff of steam starts within this share of the creature's size of a foot: the original's constant, a bit below 0.3
inline constexpr float k_SteamSpread = 0x1.333332p-2f;
/// Where a puff of steam starts: the foot, moved by a random point of the unit ball times the spread (the spread
/// stored to a float; each part's product rounded once and added to the foot's)
[[nodiscard]] glm::vec3 SteamStart(glm::vec3 foot, glm::vec3 random, float size);
/// A puff of steam kept off the land: the height it is raised to, the lift (its least height times the creature's
/// size) above the ground, when it is less than the lift above it or its height is not a number; none when it is high
/// enough
[[nodiscard]] std::optional<float> SteamOffLand(float y, float ground, float minHeight, float size);

/// The point an itchy creature's flies circle: round the middle of its eyes, half as far again as each eye is, the
/// circle level and turned at the orbiting speed. In the original's order and rounding (gutils::x87): both lengths
/// summed y² + z², then x²; the way across rescaled only when one of its parts is neither 0 nor not a number
inline constexpr float k_ItchReach = 1.5f;
[[nodiscard]] glm::vec3 ItchOrbit(glm::vec3 rightEye, glm::vec3 leftEye, float seconds, float orbitSpeed);

/// Whether the way from a creature casting to its target has swung too far from the way it began: the two ways, flat,
/// made unit length by the 3D engine's inverse square root (gutils::ModelInverseSquareRoot), meet at less than the
/// cosine of the limit, taken as radians. A way of no length comes out as none, so its dot is 0: it has swung too far
/// whenever the limit's cosine is above 0. A dot that is not a number never has.
[[nodiscard]] bool SwungTooFar(glm::vec2 began, glm::vec2 now, float limit);

} // namespace openblack::particles::maths
