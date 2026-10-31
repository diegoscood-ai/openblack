/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::physics
{
/// One row of Data\PhysicsConstants.txt, indexed by the object's
/// physics constants type.
struct PhysicsData
{
	float density;     ///< relative density: buoyancy = frac m g / density, sinks above 1
	float contact;     ///< contact spring k per unit of mass
	float penetration; ///< penetration-rate stiffness per unit of mass
	float friction;    ///< Coulomb mu against the landscape (0.3 min(mu) between two bodies)
	float angularKeep; ///< fraction of the angular momentum kept after one second
	float drag;        ///< quadratic air drag (x 0.3 R^2 for dynamic bodies)
};
} // namespace openblack::physics
