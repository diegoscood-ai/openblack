/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The particle side of the shields: the defensive shield registry that projectile particles test
// (the add-defensive-sphere rule puts the magic shield's sphere in it), the deflection helpers the fireball's
// UpdateRuleGravityWithFloor calls (DoAnyShieldDeflections), and the rules of the magic and physical shields' effects
// (Shield.cpp: UpdateRuleShieldSpark, initial spin, vapour end, SetCollectionAlpha, atoms at target,
// CheckShieldDeflections). Wiki: docs/wiki/magic/shields.md#the-particles.

namespace openblack::particles
{
class Effect;
struct Atom;
} // namespace openblack::particles

namespace openblack::particles::shields
{

/// A defensive shield, owned by an effect, and its only kind, a sphere {radius, centre}
struct DefensiveSphere
{
	uint32_t id {0};
	const Effect* owner {nullptr};
	glm::vec3 centre {0.0f}; ///< world
	float radius {0.0f};
};

/// A new sphere at the head of the list; its id
uint32_t AddDefensiveSphere(const Effect& owner, const glm::vec3& centre, float radius);
/// Every sphere of an effect goes with it: the only rule that makes one runs on the effect's root collection, whose
/// data go only with the effect (the shield's end closes its effect down; once finished, the effect is deleted)
void RemoveAllOf(const Effect* owner);
/// nullptr when gone
[[nodiscard]] DefensiveSphere* Find(uint32_t id);
/// The list, newest first
[[nodiscard]] const std::vector<DefensiveSphere>& All();

// ---- Sphere tests ----

/// |p - c|^2 < (r + margin)^2
[[nodiscard]] bool IsPointInShield(const DefensiveSphere& sphere, const glm::vec3& point, float margin);
/// `to` inside and `from` not (both with the margin)
[[nodiscard]] bool HasCrossedIntoShield(const DefensiveSphere& sphere, const glm::vec3& from, const glm::vec3& to,
                                        float margin);
/// Where the segment from -> to meets the sphere of
/// radius r + margin (out = `to` when nothing better; false when the segment misses it)
bool FindIntersect(const DefensiveSphere& sphere, const glm::vec3& from, const glm::vec3& to, float margin, glm::vec3& out);
/// Reflection off the sphere: v -= 2 (v.n) n, n = normalize(p - c)
void DeflectOffShield(const DefensiveSphere& sphere, const glm::vec3& point, glm::vec3& velocity);

/// The first sphere of the list that contains the point
[[nodiscard]] const DefensiveSphere* FindShieldContainingPoint(const glm::vec3& point, float margin);
/// The first sphere the move from -> to crossed into
[[nodiscard]] const DefensiveSphere* FindShieldCrossedInto(const glm::vec3& from, const glm::vec3& to, float margin);
/// An impact point on the shield's effect (its SpellTargets), where UpdateRuleShieldSpark makes a spark
void AddImpactTarget(const DefensiveSphere& sphere, const glm::vec3& point);
/// The Spell of the shield's effect, entt::null without one
[[nodiscard]] entt::entity SpellOf(const DefensiveSphere& sphere);

/// For the rules with CheckShieldDeflections (only the
/// fireball throws): after the atom moved from `oldGlobal`, the first shield it crossed into gets a spark at the
/// intersection and a SpellEvent 4 {hit, the move, 1, target = the shield's spell} goes to the atom's own spell. If the
/// spell answers 0 the atom is put at the hit point and its velocity reflected (true); an answer of 1 lets it through.
/// margin = 1.25 x baseScale x ruleScale.
bool DoAnyShieldDeflections(Effect& effect, Atom& atom, const glm::vec3& oldGlobal);

/// Where the atoms-at-target rule puts its atom `index`, in the world: the translation of the mesh's extra metric `index`
/// through the object's matrix (`rotation` times `scale`, then `position`), raised by the ground under that point less
/// the ground under the object, as for an object that follows the land (the physical shield's five points round its
/// base); the object's position when the mesh has no such metric.
[[nodiscard]] glm::vec3 ExtraPointOf(const glm::vec3& position, const glm::mat3& rotation, const glm::vec3& scale,
                                     std::span<const glm::mat4> metrics, size_t index,
                                     const std::function<float(glm::vec2)>& ground);

/// A rotation (each column one of its rows) turned by an angle about an axis, as the game turns a matrix by a
/// quaternion: the axis made unit (its squares added z first) unless all three parts are zero, the quaternion of the half
/// angle (its first part from the sine as the FPU holds it, the other two from the sine stored as a float, the cosine
/// stored as a float), the matrix of that quaternion, then each row times it, the products added z, y, x for the first
/// part and x, z, y for the others
[[nodiscard]] glm::mat3 TurnByAngle(const glm::mat3& rotation, float angle, const glm::vec3& axis);

/// The vapour-end patch's turn as its parent moves round the sphere's centre from `last` to `now`: by the arccosine of
/// the two points' dot product (added y, z, x) over the square root of their squared lengths' product (each added z, y,
/// x), about their cross product. The share is not held in [-1, 1]: past it the arccosine is not a number, as the
/// game's
[[nodiscard]] glm::mat3 VapourTurn(const glm::mat3& rotation, const glm::vec3& last, const glm::vec3& now);

} // namespace openblack::particles::shields
