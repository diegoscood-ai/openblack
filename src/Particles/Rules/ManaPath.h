/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Particles/ManaPathMaths.h"

// The mana path rule's data on each of its sparks, as a save writes it (Particles/Rules/ManaPath.cpp). Wiki:
// docs/wiki/physics/particle-rules.md#the-mana-path.

namespace openblack::particles
{
struct Atom;
class Modifier;
} // namespace openblack::particles

namespace openblack::particles::mana_path
{

/// What the rule keeps on a spark: its path, and how far along it the spark's last step left it
struct AtomData
{
	Path path;
	float progress {0.0f};
};

/// The rule's data on an atom, if the rule is the mana path rule and has made it there
[[nodiscard]] const AtomData* DataOf(const Atom& atom, const Modifier& rule);

/// For a load: the rule's data on the atom as the save kept it, noted as made; false, with nothing changed, for a rule
/// that is not the mana path rule
bool RestoreData(Atom& atom, const Modifier& rule, const AtomData& data);

} // namespace openblack::particles::mana_path
