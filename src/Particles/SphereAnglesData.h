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

// The data the sphere surface tracer and the forest path keep on each atom they move round a sphere, as a save writes it
// (Particles/ParticleEffect.cpp, Particles/Rules/Forest.cpp). Wiki: docs/wiki/engine/saved-games.md#the-particles-rules-data.

namespace openblack::particles
{
struct Atom;
class Modifier;
} // namespace openblack::particles

namespace openblack::particles::sphere_angles
{

/// What the rule keeps on an atom: whether its first step is still to come, then the three angles it draws on that step,
/// in 0..2 pi (the third read by neither rule)
struct AtomData
{
	bool first {true};
	float theta {0.0f};
	float phi {0.0f};
	float psi {0.0f};
};

/// The sphere surface tracer's data on an atom, if the rule is that rule and has made it there
[[nodiscard]] std::optional<AtomData> SphereTracerOf(const Atom& atom, const Modifier& rule);

/// The forest path's data on an atom, if the rule is that rule and has made it there
[[nodiscard]] std::optional<AtomData> ForestPathOf(const Atom& atom, const Modifier& rule);

/// For a load: each rule's data on the atom as the save kept it, noted as made; false, with nothing changed, for a rule
/// of another class
bool RestoreSphereTracer(Atom& atom, const Modifier& rule, const AtomData& data);
bool RestoreForestPath(Atom& atom, const Modifier& rule, const AtomData& data);

} // namespace openblack::particles::sphere_angles
