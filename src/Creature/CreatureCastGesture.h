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

// How a creature draws a miracle's gesture in the air before it casts the miracle, as the game works it out. It points
// at the start of the gesture's shape, and once its arm points and the step has gone on eight turns, the point moves
// along the shape a share each turn: a gesture as long as the circle takes thirty turns. At the end of the shape it
// holds still until eight more turns have passed, then stops pointing. The shape stands upright on a square as wide as
// 1.3 times the creature's radius, its top at the height of the creature's head, two radii ahead of the head; the
// shape's left goes to the creature's left. Docs: docs/wiki/creature/casting.md#casting-miracles-what-openblack-does.

namespace openblack::creature_cast_gesture
{

/// The highest gesture a creature can draw: past it the drawing fails
inline constexpr uint32_t k_LastGesture = 23;
/// The turns a gesture as long as the circle takes to draw
inline constexpr float k_TurnsForTheCircle = 30.0f;
/// The turns the step must have gone on before the point starts to move, and that must pass at the end of the shape
inline constexpr uint32_t k_HoldTurns = 8;
/// The side of the shape's square, in the creature's radii
inline constexpr float k_SizeInRadii = 1.3f;
/// A quarter turn, the float the game takes off the heading to go across
inline constexpr float k_QuarterTurn = 1.5707964f;

/// Where the creature points for a point of the gesture's shape (its x across, 0 on the left, its z down, 0 at the
/// top): `radius` the creature's radius on the ground, `heading` the way it faces as the game measures it (it faces
/// (sin, -cos) of it), `head` where its head is. Each operation is rounded to a float in the game's order, the sines and
/// cosines as the game's FPU gives them
[[nodiscard]] glm::vec3 PointOf(const glm::vec3& shapePoint, float radius, float heading, const glm::vec3& head);

/// The share of the shape drawn each turn, for a gesture `lengthRatio` times as long as the circle
[[nodiscard]] float ShareOfATurn(float lengthRatio);

enum class Outcome : uint8_t
{
	/// Not drawing yet: the arm does not point or the step is too young; the point stays as it is
	Waiting,
	/// The point moves on along the shape, or holds at its end
	Drawing,
	/// The shape is drawn and held: the creature stops pointing
	Done,
	/// The creature cannot draw the gesture
	Failed,
};

/// One turn of the drawing, its turn already counted in `turns` (the turns of the step). Before the shape starts it
/// waits while the arm does not point or the step has gone on fewer than k_HoldTurns turns. Then a gesture past
/// k_LastGesture fails; else the drawn share `progress` goes on by `share` (ShareOfATurn), up to 1, and the step's
/// count goes back to 0 for each turn the shape is not yet drawn. Done once the shape is drawn and the count is past
/// k_HoldTurns
[[nodiscard]] Outcome Turn(float& progress, uint32_t& turns, uint32_t gesture, bool pointing, float share);

} // namespace openblack::creature_cast_gesture
