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

#include <optional>

#include <entt/entity/entity.hpp>

// The creature spells' rules' data on their collections, as a save writes it: the first rule's creature, the itch's
// first update, and the hearts' and the steam's counts (Particles/ParticleCreatureSpellRules.cpp). The wisps' are in
// Particles/CreatureSpellWispData.h. Wiki: docs/wiki/creature/casting.md#the-creature-spells-particles.

namespace openblack::particles
{
struct Atom;
struct Collection;
class Modifier;
} // namespace openblack::particles

namespace openblack::particles::creature_spell
{

/// What the first rule keeps on its collection: whether its first update is still to come, the creature it took (none
/// once it has gone), and the game turn that creature was last found available (as it is taken, then at each step it
/// still is)
struct CreatureHeld
{
	bool first {true};
	entt::entity creature {entt::null};
	uint32_t validatedTurn {0};
};

/// What the hearts' and the steam's rules keep on their collections: the atoms due, a fraction, and the atoms made
struct AtomsMade
{
	float due {0.0f};
	int32_t made {0};
};

/// The first rule's atom's particle, standing for its creature: whether it is enabled, the creature (none once it has
/// gone) and the game turn it was last found there
struct CreatureRefData
{
	bool enabled {true};
	entt::entity creature {entt::null};
	uint32_t validatedTurn {0};
};

/// The creature particle of the first rule's atom, none for any other atom
[[nodiscard]] std::optional<CreatureRefData> CreatureRefOf(const Atom& atom);

/// For a load: the atom's particle is the creature's, as the save kept it; false, with nothing changed, for an atom
/// whose collection has no first rule
bool RestoreCreatureRef(Atom& atom, const CreatureRefData& data);

/// The first rule's data on its collection, if the rule is the first rule and is on the collection
[[nodiscard]] std::optional<CreatureHeld> HeldOf(const Collection& collection, const Modifier& rule);

/// The hearts' or the steam's count on its collection, if the rule is one of them and has made it there
[[nodiscard]] std::optional<AtomsMade> AtomsMadeOf(const Collection& collection, const Modifier& rule);

/// Whether the itch's first update is still to come, if the rule is the itch and is on the collection
[[nodiscard]] std::optional<bool> ItchFirstOf(const Collection& collection, const Modifier& rule);

/// For a load: each rule's data on its collection as the save kept it, noted as made; false, with nothing changed, for
/// a rule of another class or not on the collection
bool RestoreHeld(Collection& collection, const Modifier& rule, const CreatureHeld& held);
bool RestoreAtomsMade(Collection& collection, const Modifier& rule, const AtomsMade& made);
bool RestoreItchFirst(Collection& collection, const Modifier& rule, bool first);

} // namespace openblack::particles::creature_spell
