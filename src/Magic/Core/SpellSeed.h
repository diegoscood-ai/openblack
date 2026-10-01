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
#include <glm/vec3.hpp>

#include "ECS/Components/SpellSeed.h"
#include "PSys/SpellLink.h"

namespace openblack
{
struct GSpellSeedInfo;
} // namespace openblack

// The miracle in the hand (SpellSeed.cpp 0x727FF0..0x729C40): its charge, power-up, readiness and the cast. Casting
// from the hand (the interface's states and gestures) is M2; these are the seed's own functions.

namespace openblack::magic::seed
{
using ecs::components::SpellSeed;

[[nodiscard]] const GSpellSeedInfo& InfoOf(const SpellSeed& seed);

/// fn_00728300 -> the ctor 0x7280A0 (pos, seed info, interface status, pu, multiplier): no icon, the creator is the
/// interface's player, scale = info.scale. The entity is an Object (a creation index).
entt::entity Create(const glm::vec3& worldPosition, SpellSeedType seedType, PlayerNames player, int powerUp,
                    float multiplier);

/// SpellSeed::GetMagicInfo 0x7290F0: the seed's magic at its power-up level (the base type for an empty level)
[[nodiscard]] MagicType MagicTypeOf(const SpellSeed& seed);

/// GetChantNeededForSpellSeed 0x729860: costToCreate(pu) - the store
[[nodiscard]] float GetChantNeeded(const SpellSeed& seed, int powerUp);
/// SpellSeed::GetPower 0x7298B0: min(store / costToCreate, 1)
[[nodiscard]] float GetPower(const SpellSeed& seed);
/// SetChantStore 0x729A50 (symbol "GetChantStore"): +0x74 = +0x78 = chants
void SetChantStore(SpellSeed& seed, float chants);
/// AddToChantStore 0x729A30
void AddToChantStore(SpellSeed& seed, float chants);

/// SpellSeed::SetPowerUp 0x729B30: the level; a charge above its cost goes back to the worship site (M7). For the
/// local interface: the tooltip, the hand FX, the PSys preload and the sound (fn_00729C40): M2.
void SetPowerUp(entt::entity seed, int powerUp);

/// fn_00729900: 0 -> ready (+0x90 = 1); else inactive (+0x90 = 0, +0x94 = 0). Worship icons pass 1 (the seed waits
/// delayBeforeSeedActive), one-shots and caught fireballs 0 (ready at once).
void SetInactive(SpellSeed& seed, bool inactive);

/// SpellSeed::InterfaceSetInMagicHand 0x728810: the seed comes into the hand. Returns 1, or 3 when the seed was deleted
/// (its last spell cannot be recast, or flag bit 1).
int InterfaceSetInMagicHand(entt::entity seed);

/// SpellSeed::ProcessInHand 0x729930, every game turn while held: ready after delayBeforeSeedActive (1.5 s); a seed
/// whose spell closed goes
void ProcessInHand(entt::entity seed);

/// SpellSeed::StoreChantsAndAgeFromSpell 0x728780: the last spell's chants, age and object count go into the seed and
/// the link is cleared; its HasEnoughChantsAndLifeForRecast (1 without a spell)
bool StoreChantsAndAgeFromSpell(entt::entity seed);

/// SpellSeed::ClearSpellLink 0x728200: StopImmersion (M2), seed.spell = none; a spell still bound to this seed closes
/// down, else only its PSys does
void ClearSpellLink(entt::entity seed);

/// SpellSeed::ApplyUnlockProcess 0x728EB0: deleteSeedOnceCast -> the seed goes; else it keeps the spell's chants (and
/// goes if they are not enough for a recast)
void ApplyUnlockProcess(entt::entity seed);

/// SpellSeed::ProcessFromSpell 0x728F70 (Spell::ProcessSpellSeed, every turn): a seed that follows its spell
/// (fn_00728FC0) closes it when it is out of its player's influence. Returns 1.
int ProcessFromSpell(entt::entity seed);

/// SpellSeed::CanCast 0x729150: the magic's cast rule for the seed's player, then its class check (vt 0x30)
[[nodiscard]] bool CanCast(entt::entity seed, const glm::vec3& position);

/// SpellSeed::Cast 0x729520 (pos, out, gesture packet): an in-hand seed keeps one live spell; else DoPreCastThings,
/// GMagicInfo::CastAtPos and DoPostCastThings. handInfo is the interface's spell info (UpdateSpellInfo: hand position
/// +0x5C, velocity +0x44, +0x54), magnitude the gesture packet's +0x14. Returns 1 with the spell in `out`.
int Cast(entt::entity seed, const glm::vec3& position, entt::entity* out, float magnitude,
         const psys::ProcessInfo& handInfo);

/// SpellSeed::ToBeDeleted 0x728280: ClearSpellIconLink, ClearSpellLink, and the object goes
void ToBeDeleted(entt::entity seed);
} // namespace openblack::magic::seed
