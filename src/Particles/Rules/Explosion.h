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

#include "Particles/SpellLink.h"

// The beam explosion's particle side: UR_Explosion (the blast of SF_BeamExplosion*, MAGIC_TYPE 7-9), SetPSysCloseDown,
// and the rules of its spot visual SF_BeamExplosionFX (UR_MoveAtom, UR_ChangeScaleXYZ). Plus the two object questions
// the blast asks of its targets. Wiki: docs/wiki/magic/lightning-explosion.md#lightning-explosion-and-missing-psys-classes.

namespace openblack::particles::shields
{
struct DefensiveSphere;
} // namespace openblack::particles::shields

namespace openblack::particles::explosion
{
/// Whether a spell can destroy the object: it receives effects, an object flag is clear, and an object in a script
/// only for a spell flagged for it. Creatures, fields and the citadel (its heart and parts, the creature pen, worship
/// sites and totems) say no. Answers SpellEvent 7.
[[nodiscard]] bool CanBeDestroyedBySpell(entt::entity object, entt::entity spell);

/// An object destroyed by the beam is deleted. Every abode kind (creche, field, football, graveyard, puzzle totem,
/// spell dispenser, storage pit, totem, town centre, windmill, wonder, workshop) instead loses all its life.
void DestroyedByBeam(entt::entity object);

/// The point a blast throws things away from, 5 m under its centre (the origin of the pieces of the exploded objects
/// and of the five rocks: Rules/ExplodeObject.h)
[[nodiscard]] inline glm::vec3 BlastOrigin(const glm::vec3& centre)
{
	return {centre.x, centre.y - 5.0f, centre.z};
}

/// Where a blast inside a shield strikes it: where the ray from 200 m above its centre down to the centre meets the
/// sphere grown by the margin; the centre itself when it does not
[[nodiscard]] glm::vec3 BeamShieldImpact(const shields::DefensiveSphere& sphere, const glm::vec3& centre, float margin);
/// The hit a blast sends a shield's spell: at the point, pushing it by `push` (none for the beam, outward from the
/// blast's centre for its wave), strength 1, the shield's spell as the target
[[nodiscard]] SpellEventInfo ShieldHit(const glm::vec3& point, const glm::vec3& push, entt::entity shieldSpell);

/// What a blast does to a thing it may destroy, given how many it has shattered and removed so far: it shatters it into
/// flying pieces unless it is a creature or the most have been shattered, and removes it unless the most have been
/// removed
struct BlastHit
{
	bool shatter {false};
	bool remove {false};
};
[[nodiscard]] BlastHit HitOn(int exploded, int deleted, bool creature, int maxToExplode, int maxToDelete);
/// A blast that has shattered and removed the most it may takes no more things
[[nodiscard]] bool BlastSpent(int exploded, int deleted, int maxToExplode, int maxToDelete);

/// UR_ChangeScaleXYZ on one atom's values: false when the atom is before StartTime or the
/// step after StopTime has passed (nothing written). ruleScale = the XZ lerp, stretch = Y / XZ (0 when XZ <= 0.0001).
bool ChangeScaleXYZ(float age, float dt, float startTime, float stopTime, float startXZ, float stopXZ, float startY,
                    float stopY, float& ruleScale, float& stretch);

/// UR_MoveAtom on one atom: false outside [StartTime, StopTime]; t = (age - start) /
/// (stop - start), 1 when age + dt reaches StopTime, smoothstep t^2 (3 - 2t) with MoveSmoothly; start + (stop - start) t
bool MoveAtom(float age, float dt, float startTime, float stopTime, bool smoothly, const glm::vec3& start,
              const glm::vec3& stop, glm::vec3& out);
} // namespace openblack::particles::explosion
