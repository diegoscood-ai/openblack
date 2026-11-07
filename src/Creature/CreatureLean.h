/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// How a creature's body leans onto the slope it stands on: its placement is sheared so that every point of the body
/// rises with the land along the world's x and z axes. The lean is worked out again each time the body is placed, from
/// the land at four points around it; nothing smooths it
namespace openblack::creature_lean
{
/// The share of the land's rise the body takes, and the most it leans along either axis
inline constexpr float k_Share = 0.6f;
inline constexpr float k_Limit = 0.3f;

/// The rise per unit along the world's x and z axes
struct Lean
{
	float x {0.0f};
	float z {0.0f};
};

/// The land's height at the four points the lean samples, the creature's radius on the ground away from its middle
/// along the world's axes
struct Heights
{
	float xAhead {0.0f};
	float xBehind {0.0f};
	float zAhead {0.0f};
	float zBehind {0.0f};
};

/// The heights under the four points around `position`, `radius` away along +x, +z, -x and -z. A creature carried by
/// another takes its own height at all four, so it does not lean on the land below
[[nodiscard]] Heights Sample(glm::vec3 position, float radius, bool carried, const std::function<float(glm::vec2)>& ground);

/// The lean from the four heights: the share of the rise across the body, over twice its radius, kept within the limit.
/// Each step is a float. A lean that is not a number (a body of no radius on flat land) is the lowest
[[nodiscard]] Lean FromHeights(const Heights& heights, float radius);

/// The rotation with the lean on it: each of its axes rises by the lean along its x and z, summed in the order the
/// original sums a matrix product, so that the zeros keep their signs as there
[[nodiscard]] glm::mat3 Leaned(const glm::mat3& rotation, Lean lean);

/// The body's placement: the leaned rotation, then the scale, at the position
[[nodiscard]] glm::mat4 Model(glm::vec3 position, const glm::mat3& rotation, glm::vec3 scale, Lean lean);

} // namespace openblack::creature_lean
