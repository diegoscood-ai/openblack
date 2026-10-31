/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The PSys value noise (the lattice of Ebert et al., "Texturing and Modeling"), used by the rules that
// wiggle atoms (UR_GesturingRecognised, UR_GustyWind). Wiki:
// docs/wiki/magic/gestures.md#gestures-the-buffer-and-the-recogniser.

namespace openblack::particles::noise
{
/// The Catmull-Rom spline through `count` knots at x in 0..1
[[nodiscard]] float Spline(float x, int count, const float* knots);
/// The lattice value of i (the 256 values through the permutation table)
[[nodiscard]] float Lattice(int i);
/// The spline through the lattice values around x, about -1..1
[[nodiscard]] float SignedValueNoise(float x);
/// The lattice value of the cell (x, y, z): the permutation of z, plus y, permuted, plus x, permuted again
[[nodiscard]] float Lattice3(int x, int y, int z);
/// The lattice values of the eight cells about the point, blended linearly along x, then y, then z, each step rounded
/// to a float (UR_GustyWind's noise)
[[nodiscard]] float ValueNoise3(float x, float y, float z);
} // namespace openblack::particles::noise
