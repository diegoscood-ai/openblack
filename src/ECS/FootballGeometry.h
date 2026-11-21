/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/vec3.hpp>

#include "3D/AffineMatrix.h"

/// The football pitch's own space and its goals: the pitch's matrix and its inverse, a point taken into the pitch's
/// space, a goal mouth crossed between two turns, and whether the ball is still along the pitch. Every function keeps
/// the original's operation order, one float rounding per operation (the game's 24-bit FPU). Pure maths
/// (docs/wiki/world/buildings.md#the-football-pitch).
namespace openblack::ecs::football_geometry
{

/// A goal mouth of the goal model, in the pitch's own space: its centre and its half-extents
struct GoalBox
{
	glm::vec3 centre {0.0f};
	glm::vec3 halfSize {0.0f};
};

/// The pitch's own matrix, from its point (x and z in metres, y its ground plus its altitude), its turn about the
/// vertical and its scale: with no turn, the scale on the diagonal (the identity at scale 1); else the rows
/// (c s, 0, s s), (0, s, 0), (-s s, 0, c s), c and s the turn's cosine and sine, each product rounded once. The
/// translation is the point
[[nodiscard]] affine::AffineMatrix PitchMatrix(const glm::vec3& point, float yAngle, float scale);

/// The inverse the pitch's tests take: the cofactors over the determinant (clamped to +-1e-10 below it), in the order
/// of the original's in-place inverse, whose translation sums differ from affine::Inverse's
[[nodiscard]] affine::AffineMatrix InverseInPlace(const affine::AffineMatrix& m);

/// The ball's point in the pitch's space as the ball's turn takes it: x = ((m6 z + m3 y) + m0 x) + m9,
/// y = ((m7 z + m4 y) + m1 x) + m10, z = ((m8 z + m5 y) + m2 x) + m11
[[nodiscard]] glm::vec3 BallInPitchSpace(const affine::AffineMatrix& inverse, const glm::vec3& p);

/// A point in the pitch's space as the goal test takes it: x = ((m3 y + m6 z) + m0 x) + m9,
/// y = ((m1 x + m7 z) + m4 y) + m10, z = ((m5 y + m2 x) + m8 z) + m11
[[nodiscard]] glm::vec3 PointInPitchSpaceForGoal(const affine::AffineMatrix& inverse, const glm::vec3& p);

/// Whether the ball is along the pitch: its x in the pitch's space within the pitch model's half-length, both ends
/// included. Only the length is tested
[[nodiscard]] bool AlongThePitch(float pitchSpaceX, float halfLength);

/// The ball's heading between two turns, from and to in the pitch's space: the difference over its length
/// sqrt((dz dz + dx dx) + dy dy); none when the length is under 0.0001
[[nodiscard]] std::optional<glm::vec3> BallHeading(const glm::vec3& from, const glm::vec3& to);

/// Whether the ball went into a goal between two turns, from and to in the pitch's space: it crossed the plane
/// x = the box's centre x (from either side; when it starts exactly on the plane and ends short of it, at once), and
/// where it crossed, y is at most the box's half-height and z within its half-width. y has no lower bound
[[nodiscard]] bool CrossesGoalMouth(const glm::vec3& from, const glm::vec3& to, const GoalBox& box);

/// A goal's point in the world: the box's centre through the pitch's drawn matrix, x = ((m6 z + m3 y) + m0 x) + m9,
/// y = ((m7 z + m1 x) + m4 y) + m10, z = ((m8 z + m2 x) + m5 y) + m11
[[nodiscard]] glm::vec3 GoalPointInWorld(const affine::AffineMatrix& placement, const GoalBox& box);

} // namespace openblack::ecs::football_geometry
