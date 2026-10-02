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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
struct GMagicShieldInfo;
} // namespace openblack

// SpellShield (SpellWithObjects, 0x10C bytes, vtable 0x9828D8, GMagicShieldInfo::AllocSpell 0x5FBA70): MAGIC_TYPE
// SHIELD (19) and PHYSICAL_SHIELD (20). It makes one MapShield (Magic/Objects/MapShield) and an anti-influence ring for
// every other active player; the magic shield's dome is its PSys (SF_DefenseSphere). Upkeep = costPerGameTurn x
// (radius / radiusForNormalCost)^2. Wiki: docs/bw1-notes/magic.md ("Escudos").

namespace openblack::magic
{

/// SpellShield +0xF4..+0x108
struct SpellShieldData
{
	uint32_t struckReaction {0};      ///< +0xF4 REACTION_REACT_TO_MAGIC_SHIELD_STRUCK (35), made on the first hit
	uint32_t shieldReaction {0};      ///< +0xF8 REACTION_REACT_TO_MAGIC_SHIELD (13)
	entt::entity town {entt::null};   ///< +0xFC the nearest town within 250 m at the cast
	std::vector<entt::entity> rings;  ///< +0x104 / +0x108 the anti-influence rings, newest first
};

/// SpellShield::InitWithPos 0x72B5F0's clamp: max first (r >= max -> max), then min (r <= min -> min)
[[nodiscard]] float ClampShieldRadius(const GMagicShieldInfo& info, float radius);
/// SpellShield::CalculateCostToMaintain 0x72B7F0: base x (magnitude / radiusForNormalCost)^2
[[nodiscard]] float ShieldCostToMaintain(float baseCost, float magnitude, float radiusForNormalCost);

namespace spell_shield
{
/// vt 0x51C UpdateStruckReaction 0x72B780: the struck reaction (35) the first time, else its turn stamp refreshed
void UpdateStruckReaction(entt::entity spell);
/// vt 0x520 SetUpDestroyedReaction 0x72B7C0: the REACTION 13s it started go, then REACTION 36 (destroyed)
void SetUpDestroyedReaction(entt::entity spell);
/// SpellShield::IsUnder 0x72BD20: dist(p, castPos) < GetRadius() - margin (MapCoords)
[[nodiscard]] bool IsUnder(entt::entity spell, const glm::vec3& point, float margin);
/// fn_0072BA00 (GScript::SpellAtPoint 0x70C5F0 / fn_007217A0 with mask 3): the first available shield spell whose
/// 2D radius around originalCastPos (+0xC0) is strictly greater than its distance to the point. The type bit (19 -> 2, 20 -> 1) is tested as `(bit | mask) != 0`, so no mask filters.
[[nodiscard]] entt::entity FindShieldAt(const glm::vec3& point, uint32_t mask);
/// The town of the spell (+0xFC)
[[nodiscard]] entt::entity TownOf(entt::entity spell);
/// 0xDA07F0 / 0xDA07F4: the shield spells, newest first
[[nodiscard]] const std::vector<entt::entity>& Spells();
/// A land is loaded
void Clear();
} // namespace spell_shield
} // namespace openblack::magic
