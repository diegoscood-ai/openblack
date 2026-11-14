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

// The sound rules' data on each atom, as a save writes it (Particles/Rules/Sound.cpp). Wiki:
// docs/wiki/engine/saved-games.md#the-particles-rules-data.

namespace openblack::particles
{
struct Atom;
class Modifier;
} // namespace openblack::particles

namespace openblack::particles::sound_rules
{

/// Whether the sound removal rule has stopped the atom's sound, if the rule is the sound removal and has made its data on
/// the atom
[[nodiscard]] std::optional<bool> SoundRemovedOf(const Atom& atom, const Modifier& rule);

/// For a load: the sound removal's data on the atom as the save kept it, noted as made; false, with nothing changed,
/// for a rule of another class
bool RestoreSoundRemoved(Atom& atom, const Modifier& rule, bool removed);

} // namespace openblack::particles::sound_rules
