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
#include <utility>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

// The glints' maths: how fast a target's glints come, how each one pulses, grows and shrinks with its age, and which
// point of the target's model it sits on. Free of state, so they are tested on their own.

namespace openblack::particles::maths
{

/// One part of a model as the glints read it: the file's vertex positions of the part, and each of its primitives'
/// vertices among them (first, count), in the file's order
struct GlintModelPart
{
	std::span<const glm::vec3> positions;
	std::span<const std::pair<uint32_t, uint32_t>> ranges;
};

/// How many glints are due a second: the most allowed over the age at which they vanish. A rate that is not above 0
/// (a NaN too) makes no glints and leaves the old ones as they are
[[nodiscard]] float GlintRate(int maxAtoms, float ageZeroSize);

/// The pulse at an age: cos(age x speed x 2 pi) x magnitude x 0.5 + 1, and 0 when it is below 0 or not a number
[[nodiscard]] float GlintPulse(float age, float speed, float magnitude);

/// The size at an age: growing from 0 to 1 until ageMaxSize, then shrinking to 0 at ageZeroSize; 0 when it is not above
/// 0 or not a number, 1 when it is 1 or more
[[nodiscard]] float GlintSize(float age, float ageMaxSize, float ageZeroSize);

/// Every vertex of every primitive of every part
[[nodiscard]] uint32_t GlintPointCount(std::span<const GlintModelPart> parts);

/// The index-th of those vertices in the file's order (part by part, primitive by primitive); none past the last
[[nodiscard]] std::optional<glm::vec3> GlintLocalPoint(std::span<const GlintModelPart> parts, uint32_t index);

/// A model point through the model's matrix, each axis summed as the game does: ((z row2 + y row1) + x row0) + the
/// translation, where the rows are glm's columns
[[nodiscard]] glm::vec3 GlintThroughModel(const glm::mat4& model, const glm::vec3& point);

} // namespace openblack::particles::maths
