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

#include <entt/entity/entity.hpp>

// The glowing shell rule's data on each atom it keeps on an object, as a save writes it
// (Particles/ParticleVolumeGlowRules.cpp). Wiki: docs/wiki/physics/particle-rules.md#the-glowing-shell-over-an-object.

namespace openblack::particles
{
struct Atom;
class Modifier;
} // namespace openblack::particles

namespace openblack::particles::volume_glow
{

/// What the rule keeps on an atom: the object it is kept on, and the game turn that object was last found there (as it
/// is taken, then at each step it is still there)
struct AtomData
{
	entt::entity object {entt::null};
	uint32_t validatedTurn {0};
};

/// The rule's data on an atom, if the rule is the glowing shell rule and has made it there
[[nodiscard]] const AtomData* DataOf(const Atom& atom, const Modifier& rule);

/// For a load: the rule's data on the atom as the save kept it, noted as made; false, with nothing changed, for a rule
/// that is not the glowing shell rule
bool RestoreData(Atom& atom, const Modifier& rule, const AtomData& data);

} // namespace openblack::particles::volume_glow
