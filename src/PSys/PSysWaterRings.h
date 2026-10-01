/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

/// The water rings that particle effects leave on the sea (the ring pool of ECS/WaterRings, 0xEAB7C8). Called by
/// UR_Explosion (Rules/Explosion.cpp, the original's PSysExplosion.cpp) and UpdateRuleGravityWithFloor (Rules/Fireball.cpp, 0x6A1CC2).
namespace openblack::psys::water_rings
{

inline constexpr float k_ExplosionRingGrowth = 10.0f; ///< [0x9357D8]
inline constexpr float k_ExplosionScorchSize = 8.0f;  ///< [0x9357D4]: the crater RootsPile's scale (ECS/RootsPile), unused here

/// UR_Explosion::InitCollection 0x67E200 (0x67E347..0x67E55B): MapCoords(point).IsDryLand() (altitude >= 4) -> false,
/// and the caller puts the crater (RootsPile, pack mesh 0x251, scale 8) at a random angle PSysFloatRand(2 pi)
/// (fn_008251C0, ECS/RootsPile), no ring.
/// Otherwise (water or altitude < 4) three rings at the point, growth 10 x 0.5, x 0.7 and x 1, cell 0x30, colour
/// 0xFFFFFFFF, angle 0, rate and aspect 1; each is skipped when the 1024 slots are full. Returns true then.
bool AddExplosionRings(const glm::vec3& point);

/// UpdateRuleGravityWithFloor::AtomDataRipple (fn_006A1630, from ModifyAtomCollection 0x6A1CC2), the ripple of a
/// particle that touches the floor, once the rule's own tests passed (alpha >= rule+0x6C, an ImpactSound, the speed
/// against rule+0x60, ImpactSoundCondition, the ripple flag rule+0x71 = 1): only where MapCoords(position).IsWater(),
/// and only when the particle is farther than minDistance
/// (rule+0x40) in x, z from its last ripple, which is then moved there (lastRipple: the atom's AtomDataRipple +0x20,
/// (0, 0, 0) when it is made). The ring: at the particle, growth 4 x Object::GetRadius of the atom, cell 0x30, colour
/// 0xFFFFFFFF, angle 0, rate and aspect 1 (nothing when the pool is full). Returns true when a ring was added.
bool AddParticleRipple(const glm::vec3& position, float atomRadius, glm::vec3& lastRipple, float minDistance);

} // namespace openblack::psys::water_rings
