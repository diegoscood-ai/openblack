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

// The belief sprite rule's data on each sprite, as a save writes it (Particles/Rules/Belief.cpp). Wiki:
// docs/wiki/physics/particle-rules.md#the-belief-sprites.

namespace openblack::particles
{
struct Atom;
class Modifier;
} // namespace openblack::particles

namespace openblack::particles::belief_sprite
{

/// What the rule keeps on a sprite: where it started and the alpha it started with
struct AtomData
{
	glm::vec3 start {0.0f};
	uint8_t alpha {0};
};

/// The rule's data on an atom, if the rule is the belief sprite rule and has made it there
[[nodiscard]] const AtomData* DataOf(const Atom& atom, const Modifier& rule);

/// For a load: the rule's data on the atom as the save kept it, noted as made; false, with nothing changed, for a rule
/// that is not the belief sprite rule
bool RestoreData(Atom& atom, const Modifier& rule, const AtomData& data);

} // namespace openblack::particles::belief_sprite
