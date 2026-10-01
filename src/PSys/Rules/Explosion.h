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

// The beam explosion's particle side (PSysExplosion.cpp of the original): UR_Explosion (the blast of SF_BeamExplosion*,
// MAGIC_TYPE 7-9), SetPSysCloseDown, and the rules of its spot visual SF_BeamExplosionFX (UR_MoveAtom, UR_ChangeScaleXYZ).
// Plus the two Object virtuals the blast asks of its targets. Research: dev\tmp_dis\miracles\impl\m6b\;
// wiki: docs/bw1-notes/miracles.md, "Explosión de rayo".

namespace openblack::psys::explosion
{
/// Object::CanBeDestroyedBySpell (vt 0x778, 0x639960): IsEffectReceiver(NULL) == 1, not flag +0x25 & 0x40, and an
/// object in a script only for a spell with +0x25 & 4. Overrides returning 0: Creature 0x47B1E0, Field 0x529FF0 and
/// CitadelPart 0x4695D0 (CitadelHeart, CitadelPart, CreaturePen, WorshipSite, WorshipTotem). Answers SpellEvent 7.
[[nodiscard]] bool CanBeDestroyedBySpell(entt::entity object, entt::entity spell);

/// Object::DestroyedByBeam (vt 0x500, 0x63AB20): ToBeDeleted(0). Abode 0x402CB0 (every Abode class: Creche, Field,
/// Football, Graveyard, PuzzleTotem, SpellDispenser, StoragePit, Totem, TownCentre, Windmill, Wonder, Workshop):
/// ReduceLife(GetLife(0)).
void DestroyedByBeam(entt::entity object);

/// fn_0067E8C0: the point a blast throws things away from, 5 m under its centre (read for the mesh-pieces call
/// fn_00681260, which is not ported: the exploding objects just vanish)
[[nodiscard]] inline glm::vec3 BlastOrigin(const glm::vec3& centre)
{
	return {centre.x, centre.y - 5.0f, centre.z};
}

/// UR_ChangeScaleXYZ::ModifyAtomCore 0x6A5240 on one atom's values: false when the atom is before StartTime or the
/// step after StopTime has passed (nothing written). ruleScale = the XZ lerp, stretch = Y / XZ (0 when XZ <= 0.0001).
bool ChangeScaleXYZ(float age, float dt, float startTime, float stopTime, float startXZ, float stopXZ, float startY,
                    float stopY, float& ruleScale, float& stretch);

/// UR_MoveAtom::ModifyAtomCore 0x6A5E50 on one atom: false outside [StartTime, StopTime]; t = (age - start) /
/// (stop - start), 1 when age + dt reaches StopTime, smoothstep t^2 (3 - 2t) with MoveSmoothly; start + (stop - start) t
bool MoveAtom(float age, float dt, float startTime, float stopTime, bool smoothly, const glm::vec3& start,
              const glm::vec3& stop, glm::vec3& out);
} // namespace openblack::psys::explosion
