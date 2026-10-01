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

// The particle side of the shields (PSysShield.cpp of the original): the DefensiveShield registry that projectile
// particles test (UR_AddDefensiveSphere puts the magic shield's sphere in it), the deflection helpers the fireball's
// UpdateRuleGravityWithFloor calls (DoAnyShieldDeflections), and the rules of SF_DefenseSphere / SF_PhysicalShieldFX
// (Shield.cpp: UpdateRuleShieldSpark, UR_InitialSpin, UR_VapourEndEffect, SetCollectionAlpha, UR_AtomsAtEPTarget,
// CheckShieldDeflections). Wiki: docs/bw1-notes/magic.md ("Escudos").

namespace openblack::psys
{
class Effect;
struct Atom;
} // namespace openblack::psys

namespace openblack::psys::shields
{

/// DefensiveShield (PSysBase, ctor 0x6D0A60; +0x14 prev, +0x18 next, +0x1C the owning PSysManager) and its only kind,
/// DefensiveSphere (0x30 bytes, fn_006D0D20): TSphere {radius +0x20, centre +0x24}
struct DefensiveSphere
{
	uint32_t id {0};
	const Effect* owner {nullptr}; ///< +0x1C
	glm::vec3 centre {0.0f};       ///< +0x24, world
	float radius {0.0f};           ///< +0x20
};

/// fn_006D0B20: a new sphere at the head of the global list 0xD4EE48 (count 0xD4EE4C); its id
uint32_t AddDefensiveSphere(const Effect& owner, const glm::vec3& centre, float radius);
/// fn_006D0B70: unlink and delete (CollectionData of UR_AddDefensiveSphere, fn_006A2A30)
void RemoveDefensiveSphere(uint32_t id);
/// Every sphere of an effect goes with it (the collection data are deleted with the collections)
void RemoveAllOf(const Effect* owner);
/// nullptr when gone
[[nodiscard]] DefensiveSphere* Find(uint32_t id);
/// The list, newest first
[[nodiscard]] const std::vector<DefensiveSphere>& All();

// ---- GJUtils on a TSphere (DefensiveSphere vt 0xFC..0x108) ----

/// GJUtils::PointIsInSphere 0x57C9F0 (vt 0x108 IsPointInShield 0x6D0DA0): |p - c|^2 < (r + margin)^2
[[nodiscard]] bool IsPointInShield(const DefensiveSphere& sphere, const glm::vec3& point, float margin);
/// vt 0x104 HasCrossedIntoShield 0x6D0DC0: `to` inside and `from` not (both with the margin)
[[nodiscard]] bool HasCrossedIntoShield(const DefensiveSphere& sphere, const glm::vec3& from, const glm::vec3& to, float margin);
/// vt 0xFC FindIntersect 0x6D0D50 -> GJUtils::FindIntersect 0x57CCD0: where the segment from -> to meets the sphere of
/// radius r + margin (out = `to` when nothing better; false when the segment misses it)
bool FindIntersect(const DefensiveSphere& sphere, const glm::vec3& from, const glm::vec3& to, float margin, glm::vec3& out);
/// vt 0x100 DeflectOffShield 0x6D0D80 -> GJUtils::DeflectOffSphere 0x57CFD0: v -= 2 (v.n) n, n = normalize(p - c)
void DeflectOffShield(const DefensiveSphere& sphere, const glm::vec3& point, glm::vec3& velocity);

/// fn_006D0BC0 / fn_006D0BE0: the first sphere of the list that contains the point
[[nodiscard]] const DefensiveSphere* FindShieldContainingPoint(const glm::vec3& point, float margin);
/// fn_006D0C20 / fn_006D0C40: the first sphere the move from -> to crossed into
[[nodiscard]] const DefensiveSphere* FindShieldCrossedInto(const glm::vec3& from, const glm::vec3& to, float margin);
/// fn_006D0AF0: an impact point on the shield's effect (its SpellTargets), where UpdateRuleShieldSpark makes a spark
void AddImpactTarget(const DefensiveSphere& sphere, const glm::vec3& point);
/// fn_006D0B10: the Spell of the shield's effect (PSysManager +0xA8), entt::null without one
[[nodiscard]] entt::entity SpellOf(const DefensiveSphere& sphere);

/// UpdateRuleGravityWithFloor::DoAnyShieldDeflections 0x6A1FA0, for the rules with CheckShieldDeflections (only the
/// fireball throws): after the atom moved from `oldGlobal`, the first shield it crossed into gets a spark at the
/// intersection and a SpellEvent 4 {hit, the move, 1, target = the shield's spell} goes to the atom's own spell. If the
/// spell answers 0 the atom is put at the hit point and its velocity reflected (true); an answer of 1 lets it through.
/// margin = 1.25 x baseScale x ruleScale.
bool DoAnyShieldDeflections(Effect& effect, Atom& atom, const glm::vec3& oldGlobal);

} // namespace openblack::psys::shields
