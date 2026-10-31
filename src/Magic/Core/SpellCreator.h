/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "ECS/Components/Spell.h"

// "Who pays": the calls the spell core makes on its creator, per creator kind.

namespace openblack::magic::creator
{
using ecs::components::SpellCreator;

/// The neutral (script) player's creator: a script cast or CastAtPos with no creator
[[nodiscard]] SpellCreator NeutralPlayer();
/// A player
[[nodiscard]] SpellCreator OfPlayer(PlayerNames player);
/// A creature, for its owner
[[nodiscard]] SpellCreator OfCreature(entt::entity creature);

/// MaintainSpell(spell, amount): the chants the creator gives the spell.
/// A player: all of it for the neutral player, else 0 (spells from seeds live on their initial chants);
/// a thing: all of it. A worship icon: its site's chants. A creature: what its body pays (its
/// energy and exhaustion in CreatureNeeds; creature_spell_casting::MaintainSpell), at the size it is drawn at.
[[nodiscard]] float MaintainSpell(const SpellCreator& creator, entt::entity spell, float amount);

/// Whether a creature's body has the energy to make a miracle that costs so much (creature_spell_casting::CanCast, at
/// the size it is drawn at); not for a creature with no body
[[nodiscard]] bool CreatureCanPayFor(entt::entity creature, float costToCreate);

/// UpdateSpellInfo(spell, info): a player forwards to the interface that cast the spell (the hand), the neutral
/// player and other things leave it alone. A creature puts its hands, from its body as posed for the turn
/// (creature_hands), and the way from them to the info's point, unless the spell (none for the one about to be cast)
/// is cast from above; nothing moves its hands before its body is posed.
void UpdateSpellInfo(const SpellCreator& creator, entt::entity spell, particles::ProcessInfo& info);

/// IsFunctional (= IsAvailable for players and things)
[[nodiscard]] bool IsFunctional(const SpellCreator& creator);

/// The creator is a creature
[[nodiscard]] bool IsCreature(const SpellCreator& creator);

/// A spell icon whose player is human, or a human player
[[nodiscard]] bool IsHumanPlayerCasting(const SpellCreator& creator);
} // namespace openblack::magic::creator
