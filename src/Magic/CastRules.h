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

#include "Enums.h"

namespace openblack
{
struct GMagicInfo;
} // namespace openblack

// Where a miracle may be cast: the magic's cast rule (castRuleType) and its class's own check (GMagicInfo vt 0x30 for a
// position, vt 0x2C for an object; the symbols have the two names swapped). Positions are map positions (x, z metres).

namespace openblack::magic::cast_rules
{
/// MapCoords::InBounds 0x6042C0: the 10 m cell is inside the map
[[nodiscard]] bool InBounds(const glm::vec3& position);
/// MapCoords::IsLand 0x603720
[[nodiscard]] bool IsLand(const glm::vec3& position);

/// fn_005FB5D0 (GMagicInfo cast rule): in bounds, then ANYWHERE 1, ON_LAND IsLand, IN_INFLUENCE
/// CalculatePlayerInfluence(pos, player, 0, 0, 1) > 0, ON_LAND_IN_INFLUENCE both
[[nodiscard]] bool CanCastRule(const GMagicInfo& info, const glm::vec3& position, PlayerNames player);

/// vt 0x30, the class's check at a position:
/// GMagicInfo 0x5FB420 = 1; GMagicHealInfo 0x5FBD20 = FindTargets(pos, NULL) for HEAL / HEAL_PU_ONE;
/// GMagicResourceInfo 0x5FBA00 = IsLand; GMagicCreatureSpellInfo 0x5FA7E0 = 0;
/// GMagicForestInfo 0x5FAE80 = InBounds, IsLand, fn_005FADF0, SpellForest::ValidPlaceForTree (Spells/SpellForest);
/// GMagicTeleportInfo 0x5FBE50 = no MultiMapFixed within fn_005FCCA0 = 6 m (Magic/Objects/MagicTeleport)
[[nodiscard]] bool CanCastAt(MagicType type, const glm::vec3& position);

/// vt 0x2C, the class's check on an object: GMagicInfo 0x5FB430 = vt 0x30 at the object's position;
/// GMagicObjectInfo 0x5FAC00 = IsLand there; GMagicForestInfo 0x42D8E0 = 0; GMagicCreatureSpellInfo 0x5FA7F0 = a
/// Creature whose mind allows it (fn_004F5230; M8: no creature can yet, 0)
[[nodiscard]] bool CanCastOn(MagicType type, entt::entity object);

/// GMagicHealInfo::FindTargets 0x5FBB00 (pos, spell): the available Living effect receivers that the heal can heal
/// (CanBeHealedByHealSpell) within R = dummyVar (x the spell's tribal power), searched in a spiral of ceil(2R / 10)^2
/// cells, at most maxToHeal (x tribal power, rounded). With a spell each one becomes a target of its PSys. Returns
/// the number found.
int FindHealTargets(const glm::vec3& position, entt::entity spell);
} // namespace openblack::magic::cast_rules
