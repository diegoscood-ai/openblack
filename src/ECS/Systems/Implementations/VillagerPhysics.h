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

#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/Effects/EffectValues.h"
#include "Enums.h"

namespace openblack::ecs
{
struct PhysicsEntry;
struct ImpactInfo;
} // namespace openblack::ecs

/// The physics and hand virtuals of the Living classes (Villager and Animal): what a villager or an animal does when it
/// starts flying, is hit, lands or is picked up. The physics (ECS/Systems/Implementations/DynamicsSystem) and the hand call
/// them through the class hooks (ecs::PhysicsGameHooks); the body, its G and its sinking stay with the physics. The pure
/// rules of the landing are physics::living's (Physics/LivingRules).
namespace openblack::ecs::villager_physics
{

// ---- the Villager and Animal classes' physics hooks (ecs::PhysicsGameHooks) ---------------------------------------

/// The villager's start of physics in the world (not from the hand), before its body is made: it drops its resource,
/// then unless already flying it keeps its state as its previous one (not when held) and must take FLYING; false
/// refuses, and no physics starts
[[nodiscard]] bool VillagerStartPhysicsInWorld(entt::entity entity, glm::vec3 velocity, glm::vec3 spin);
/// The Living's part of a start from a hand (the player's or a creature's), before the object's: a dancer leaves its
/// dance, then unless IN_HAND the villager keeps its state as its previous one
void VillagerBeforeStartFromHand(entt::entity entity);
/// The villager's start of physics once its body is built: from the hand, FLYING through its old state's exit (a
/// reaction it follows ends); in the world it is already flying, and its walk is dropped
void VillagerStartPhysics(entt::entity entity, PhysicsEntry& po, bool fromHand);
/// The Living's reaction to an impact. False when the villager went away
bool VillagerReactToImpact(entt::entity entity, const ImpactInfo& impact);
/// The villager's landing, the class's part. The entity that stays, or entt::null
entt::entity VillagerEndPhysicsOfClass(entt::entity entity, PhysicsEntry& po);
/// The velocity and the spin a particle system's take starts a living's physics with (the spin in the body's axes and
/// the original's sign): a villager's dropped log gets them
constexpr glm::vec3 k_ParticleTakeVelocity {0.0f, 0.0f, 0.0f};
constexpr glm::vec3 k_ParticleTakeSpin {0.0f, 1.0f, 0.0f};
/// The villager's start of physics without a body (a particle system takes it): its resource dropped with the take's
/// velocity and spin, then the Living's start; false refuses the take
bool VillagerInitialisePhysicsWithoutBody(entt::entity entity);
/// The villager's start of physics as a knocked resting proxy: true when it started
bool VillagerInitialisePhysicsKnocked(entt::entity entity);
/// The animal's reaction to an impact (a food store may take it), then the Living's. False when it went away
bool AnimalReactToImpact(entt::entity entity, PhysicsEntry& po, const ImpactInfo& impact);
/// The animal's landing, the class's part. Always entt::null
entt::entity AnimalEndPhysicsOfClass(entt::entity entity, PhysicsEntry& po);
/// The animal's start of physics as a knocked resting proxy: true when it started
bool AnimalInitialisePhysicsKnocked(entt::entity entity);

/// A villager that lands alive on dry land: LANDED, then straight back to the state it was in when it began to fly when
/// a script controls it or when that state's row in the info's state table says so (state_info::GoesBackAfterLanding).
/// Both changes are set straight, not through the states' exits and entries
void SetLandedState(entt::entity villager);

/// What a villager or an animal hit at `g` G takes: the info's physics impact effect (crush 1) times (g - 2) x 0.03,
/// its positive crush and hit each times the Living's defence multiplier for it. None at 2 G or less, and when it does
/// not take the effect: a villager takes an effect with a heal while it is not dead, one without while it can be
/// reached; an animal always takes it
[[nodiscard]] std::optional<float> ImpactDamage(entt::entity entity, float g);

/// The effect a villager or an animal takes from an impact, as ImpactDamage, with who it comes from: applied by what
/// struck it (none for a fall onto the land), a creature's when that is a creature; the caused player the local
/// player when its hand threw it; and the effect's player (effects::EffectPlayer) what struck it's own player (none
/// when it has none), else the thrower's, else the neutral player
[[nodiscard]] std::optional<effects::EffectValues> ImpactEffectOf(entt::entity entity, const ImpactInfo& impact);

/// A villager or an animal put in the hand: marked as held, then the villager goes IN_HAND through its old state's exit
/// (a reaction it follows ends), with its SCARED_STIFF clip, and the animal leaves its flock and goes IN_HAND. Nothing
/// for an object that is not a Living. A villager of breeding age put in its own player's hand (`handPlayer`, the
/// player of the hand that takes it; none for a creature's take) calls to the villagers of the other sex
/// (villager_mate::HeldByHand)
void InterfaceSetInMagicHand(entt::entity entity, std::optional<PlayerNames> handPlayer = std::nullopt);

/// A Living taken out of the hand: Villagers and Animals do what every object does: the object's fire effect, if any,
/// is told it left the hand (its reaction goes)
void InterfaceSetOutMagicHand(entt::entity entity);

/// The y angle kept while a Living is in the hand until its landing sets it again: the hand never changes it, and
/// openblack keeps no such angle of its own (ObjectYAngle)
struct HeldYAngle
{
	float value {0.0f};
};

// ---- the landing: the object's y angle and the bodiless EndPhysics -----------------------------------------------

/// The object's y angle in radians as the original keeps it: written when the object's y angle or game angle is set
/// (villager::SetYAngle / animal_ai::SetYAngle; a game angle through its 3D conversion) and by SetXYZAngles, read when
/// the world matrix is built as AngleY(angle) (no pi / 2; the original's body is built from it). openblack keeps no
/// such angle: the Transform is the drawn rotation AngleY(angle + pi / 2) (physics::living::DrawnRotation). Exact when
/// that Transform is still the one drawn from the game angle (villager WallHug::yAngle, set with it; the animal's from
/// AnimalBrain::angle); otherwise (inferred) physics::living::YawFromRotation(OriginalRows(Transform)), to the rounding
/// of the matrix's floats. 0 without a Transform
[[nodiscard]] float ObjectYAngle(entt::entity entity);

/// A Living from the hand with no body built lands at once. The villager: no pose, no empathy, no SetYAngle, still
/// marked as held; then the object's landing, landType 3, altitude 0, the water branch (alive: DROWNING, its timer
/// 0; dead: VillagerDead 6), the land death (VillagerDead 5, no player) or LANDED. The animal: no SetYAngle, landType
/// 3, the object's landing, the life / state work. The same code as the body's EndPhysics from there on. The Transform
/// is redrawn from ObjectYAngle (the hand's tilt goes) and grounded. Nothing for an entity that is not a Living. For
/// physics::from_hand::PlaceWithoutBody: the object goes back on the map here (EndPhysicsAsObject), so the caller does not
/// insert a Living itself
void EndPhysicsWithoutBody(entt::entity entity);

} // namespace openblack::ecs::villager_physics
