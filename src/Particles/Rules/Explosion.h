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

#include <functional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Particles/SpellLink.h"

// The beam explosion's particle side: the explosion rule (the blast of MAGIC_TYPE 7-9), SetPSysCloseDown,
// and the rules of its spot visual effect (move-atom, per-axis scale). Plus the two object questions
// the blast asks of its targets. Wiki: docs/wiki/magic/lightning-explosion.md#lightning-explosion-and-missing-psys-classes.

namespace openblack::particles
{
class Effect;
struct Collection;
class Modifier;
} // namespace openblack::particles

namespace openblack::particles::shields
{
struct DefensiveSphere;
} // namespace openblack::particles::shields

namespace openblack::particles::explosion
{
/// What the explosion rule keeps on its collection, made on the rule's first call, which a save writes
struct BlastData
{
	glm::vec3 centre {0.0f}; ///< The parent's position at the land's altitude, every step
	bool notStarted {true};  ///< The blast has not started yet
	bool anyTarget {false};  ///< A target is still in the list
	float spread {0.0f};     ///< The ring of the blast, += SpreadSpeed x dt
	struct Target
	{
		unsigned int turn {0};            ///< the game turn it was last seen available
		entt::entity object {entt::null}; ///< NULL once handled
	};
	std::vector<Target> targets;
	int exploded {0}; ///< Up to MaxObjectsToExplode
	int deleted {0};  ///< Up to MaxObjectsToDelete
	bool smokeDone {false};
	bool beamDone {false};
	bool finished {false};            ///< A shield stopped the blast
	entt::entity beamFx {entt::null}; ///< The spot visual effect's container
	/// For a load: the number in the saved game of the beam's spot visual container, until the containers are made
	/// and LinkLoadedFx gives it (0 for none)
	uint32_t loadedFx {0};
};
/// The explosion rule's data on the collection, if the rule is that rule and has made its data there
[[nodiscard]] const BlastData* DataOf(const Collection& collection, const Modifier& rule);
/// For a load: the explosion rule's data on the collection as the save kept it, noted as made; false, with nothing
/// changed, for a rule of another class
bool RestoreData(Collection& collection, const Modifier& rule, const BlastData& data);
/// For a load, once the spot visuals' containers are made: each blast of the effect that names its beam's container
/// by its number takes the container's object `objectOf` gives (none for one not made)
void LinkLoadedFx(Effect& effect, const std::function<entt::entity(uint32_t)>& objectOf);

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

/// The per-axis scale rule on one atom's values: false when the atom is before StartTime or the
/// step after StopTime has passed (nothing written). ruleScale = the XZ lerp, stretch = Y / XZ (0 when XZ <= 0.0001).
bool ChangeScaleXYZ(float age, float dt, float startTime, float stopTime, float startXZ, float stopXZ, float startY,
                    float stopY, float& ruleScale, float& stretch);

/// The move-atom rule on one atom: false outside [StartTime, StopTime]; t = (age - start) /
/// (stop - start), 1 when age + dt reaches StopTime, smoothstep t^2 (3 - 2t) with MoveSmoothly; start + (stop - start) t
bool MoveAtom(float age, float dt, float startTime, float stopTime, bool smoothly, const glm::vec3& start,
              const glm::vec3& stop, glm::vec3& out);
} // namespace openblack::particles::explosion
