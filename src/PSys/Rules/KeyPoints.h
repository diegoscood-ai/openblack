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

// KPSplineInterpolator<float>: the key-point curves of the spell files (UR_KPStretchHeight, UR_KPMoveAtoms,
// UR_ForestPath's RadiusSpline / HeightSpline). Wiki: docs/bw1-notes/miracles.md, "Explosión de rayo y clases de PSys que faltaban (M6b)" (las clases
// que faltaban).

namespace openblack::psys::key_points
{
/// The keys: (t, value, second derivative) per key, 12 bytes each like the original's array
struct Spline
{
	std::vector<glm::vec3> keys;
};

/// The array property's setter (0x6AE170 and its siblings): the file's flat (t, value) pairs (an odd last value is
/// dropped: count / 2 * 2), then fn_005B3760, the cubic spline's second derivatives (Numerical Recipes' spline():
/// first slope yp1 and last slope ypn). Every rule's ctor sets the array's flag (+8 = 1), which passes yp1 = ypn = 0
/// (zero slopes at the ends); without it 1e30 (0x7149F2CA) would make a natural spline.
[[nodiscard]] Spline Make(const std::vector<float>& pairs, bool zeroEndSlopes = true);

/// KPSplineInterpolator::EvalAtT 0x6A7EB0 (splint): bisection for the keys around t, then a y_lo + b y_hi + ((a^3 - a)
/// y2_lo + (b^3 - b) y2_hi) h^2 / 6, not clamped outside the keys. With keys at the same t (h = 0, or fewer than two
/// keys) nothing is written: the caller's value (`unchanged`) stays.
[[nodiscard]] float Evaluate(const Spline& spline, float t, float unchanged);
} // namespace openblack::psys::key_points
