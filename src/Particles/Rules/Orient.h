/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

// The orient-with-velocity rule's data on each sprite, as a save writes it (Particles/Rules/Orient.cpp). Wiki:
// docs/wiki/magic/lightning-explosion.md#lightning-explosion-and-missing-psys-classes.

namespace openblack::particles
{
struct Atom;
class Modifier;
} // namespace openblack::particles

namespace openblack::particles::orient_velocity
{

/// What the rule keeps on a sprite: whether its first step is still to come, then its smoothed velocity and the rate it
/// is smoothed at, both set on that first step
struct AtomData
{
	bool first {true};
	glm::vec3 velocity {0.0f};
	float rate {0.0f};
};

/// The rule's data on an atom, if the rule is the orient-with-velocity rule and has made it there
[[nodiscard]] const AtomData* DataOf(const Atom& atom, const Modifier& rule);

/// For a load: the rule's data on the atom as the save kept it, noted as made; false, with nothing changed, for a rule
/// that is not the orient-with-velocity rule
bool RestoreData(Atom& atom, const Modifier& rule, const AtomData& data);

} // namespace openblack::particles::orient_velocity
