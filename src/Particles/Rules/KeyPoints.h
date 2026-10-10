/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <glm/vec3.hpp>

// The key-point curves of the spell files (UR_KPStretchHeight, UR_KPMoveAtoms, UR_ForestPath's RadiusSpline /
// HeightSpline). Wiki: docs/bw1-notes/miracles.md, beam explosion and the missing PSys classes.

namespace openblack::psys::key_points
{
/// The keys: (t, value, second derivative) per key, 12 bytes each like the original's array
struct Spline
{
	std::vector<glm::vec3> keys;
};

/// The array property's setter: the file's flat (t, value) pairs (an odd last value is dropped: count / 2 * 2), then
/// the cubic spline's second derivatives (Numerical Recipes' spline(): first slope yp1 and last slope ypn). Every
/// rule's constructor sets the array's flag, which passes yp1 = ypn = 0 (zero slopes at the ends); without it 1e30
/// would make a natural spline. The float operations are the game's: every division of the recurrence is a
/// multiplication by a reciprocal.
[[nodiscard]] Spline Make(const std::vector<float>& pairs, bool zeroEndSlopes = true);

/// The spline at t (splint): bisection for the keys around t, then, in the game's order for one value,
/// ((S x ((h x h) x 1/6)) + a y_lo) + b y_hi with S = (b^3 - b) y2_hi + (a^3 - a) y2_lo, not clamped outside the keys.
/// With keys at the same t (h = 0 or not a number, or fewer than two keys) nothing is written: the caller's value
/// (`unchanged`) stays, as in the game's reader (UR_KPStretchHeight's own copy of it does otherwise: see the wiki).
[[nodiscard]] float Evaluate(const Spline& spline, float t, float unchanged);

/// The same curve read in the order of the game's three-axis spline (the simple beam's): (a y_lo + b y_hi) + S x ((h x
/// h) x 1/6), with S = (a^3 - a) y2_lo + (b^3 - b) y2_hi. Each axis of such a curve is a Spline of its own on the same
/// t; with keys at the same t all three keep their values, as in the game.
[[nodiscard]] float EvaluateVector(const Spline& spline, float t, float unchanged);
} // namespace openblack::psys::key_points
