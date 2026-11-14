/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <vector>

#include "Particles/ParticleEffect.h"

namespace openblack::test
{
/// The draw takes an atom only once it has been stepped twice, or once when it is drawn on its first update
/// (particles::Atom::Shown, its own tests in test_particle_atom_shown.cpp). The tests that read through the draw's
/// collects what a rule left on its atoms in the step that made them check the rule, not the step the atom is first
/// drawn at: they call this first, so that the collects take every atom of the effect now, as after its second step.
/// Nothing but the draw reads the atom's steps, so the effect steps on as it would.
inline void ShowEveryAtom(const std::vector<std::unique_ptr<particles::Collection>>& collections)
{
	for (const auto& collection : collections)
	{
		for (const auto& atom : collection->atoms)
		{
			atom->steps = 2;
			ShowEveryAtom(atom->subCollections);
		}
	}
}

inline void ShowEveryAtom(const particles::Effect& effect)
{
	ShowEveryAtom(effect.GetRoots());
}
} // namespace openblack::test
