/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Particles/PlasmaCommand.h"

// The plasma rule's data on each carrier atom, as a save writes it: the arc it was asked to fire
// (Particles/ParticleBeamRules.cpp). Wiki: docs/wiki/physics/particle-rules.md#the-plasma.

namespace openblack::particles
{
struct Atom;
class Modifier;
} // namespace openblack::particles

namespace openblack::particles::plasma
{

/// The plasma rule's arc on a carrier atom, if the rule is the plasma rule and has made it there
[[nodiscard]] const PlasmaCommand* DataOf(const Atom& atom, const Modifier& rule);

/// For a load: the arc on the carrier atom as the save kept it, noted as made; false, with nothing changed, for a rule
/// that is not the plasma rule
bool RestoreData(Atom& atom, const Modifier& rule, const PlasmaCommand& arc);

} // namespace openblack::particles::plasma
