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

// "Who pays": the virtuals the spell core calls on its creator (Spell +0xA0), per creator class.

namespace openblack::magic::creator
{
using ecs::components::SpellCreator;

/// The neutral (script) player's creator: GScript::CastSpellAtPos / GMagicInfo::CastAtPos with no creator
[[nodiscard]] SpellCreator NeutralPlayer();
/// A GPlayer
[[nodiscard]] SpellCreator OfPlayer(PlayerNames player);

/// GameThing vt 0x58 MaintainSpell(spell, amount): the chants the creator gives the spell.
/// GPlayer 0x64C430: all of it for the neutral player, else 0 (spells from seeds live on their initial chants);
/// GameThing 0x56FED0: all of it. WorshipSpellIcon 0x77F6F0 (M7) and Creature 0x4F8350 (M8) are not ported: 0.
[[nodiscard]] float MaintainSpell(const SpellCreator& creator, entt::entity spell, float amount);

/// GameThing vt 0x5C UpdateSpellInfo(spell, info): GPlayer 0x64C470 forwards to the interface that cast the spell
/// (GInterfaceStatus::UpdateSpellInfo 0x5DC8F0: the hand), the neutral player and other things leave it alone.
void UpdateSpellInfo(const SpellCreator& creator, entt::entity spell, psys::ProcessInfo& info);

/// GameThing vt 0xD4 IsFunctional (= IsAvailable for players and things)
[[nodiscard]] bool IsFunctional(const SpellCreator& creator);

/// vt 0x34 IsCreature
[[nodiscard]] bool IsCreature(const SpellCreator& creator);

/// Spell ctor 0x71FB40: a SpellIcon whose player has +0x8E0 == 1, or such a GPlayer
[[nodiscard]] bool IsHumanPlayerCasting(const SpellCreator& creator);
} // namespace openblack::magic::creator
