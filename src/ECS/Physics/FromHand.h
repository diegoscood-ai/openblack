/*******************************************************************************
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
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
enum class PlayerNames : uint8_t;
}

namespace openblack::ecs::physics::from_hand
{
/// What the hand owns that a throw needs (the hand's pots, trees and stores), set by the hand system.
struct HandHooks
{
	/// a pot of any class put down becomes a pile or goes into a store
	std::function<void(entt::entity)> putDownHandPot;
	/// the object is a pot, of any class
	std::function<bool(entt::entity)> isPot;
};
void SetHandHooks(HandHooks hooks);

/// Throws an object that is still in the hand (the hand lets go of it afterwards): a hand
/// pot slow enough is put down at once, else InitialisePhysicsFromHand with the holding spring's velocity. A release
/// passes dontReplant false, a forced drop true. Returns true when the object landed (put down, or LANDED out of the
/// hand) instead of flying. throwerPlayer: the player it is credited to (PhysicsEntry::throwerPlayer).
/// angularMomentum: the release prediction's (from PredictRelease, this port's sign), set on the new body after
/// AddObject and the paper / disappear block; nullopt keeps AddObject's zero.
/// (pending) the original first sets the object's matrix from the release pose (the hand's angles and position):
/// the caller sets the Transform from that pose before Throw; HandHolding.cpp passes no pose and no L yet.
bool Throw(entt::entity object, glm::vec3 springVelocity, bool dontReplant, std::optional<PlayerNames> throwerPlayer,
           std::optional<glm::vec3> angularMomentum = std::nullopt);
/// Sent by the hand about 180 ms after a release, handDelta = the hand's displacement since: with a body,
/// |v| = sqrt((vz vz + vy vy) + vx vx) of its velocity, m = its mass; the external torque
/// += (-(((m dz) 1.6) |v|), 0, ((m dx) 1.6) |v|), in the original's sign (its
/// torque is F x r: stored negated here). ClearForces copies it every substep and ProcessTurn zeroes it at the turn's end.
/// Nothing without a body.
void ApplyReleaseSpin(entt::entity object, glm::vec3 handDelta);
/// A forced drop: released with zero velocity, then thrown with dontReplant set
bool ForceDrop(entt::entity object, std::optional<PlayerNames> throwerPlayer);

/// AddObject, SettleOnLand, RaiseClearOfWhatIsUnder and the LANDED
/// rule; nullopt when no body could be made, else whether it landed. throwerPlayer: the player it is credited to.
/// byCreature: a creature lets go of it, which throws it at a lower speed (hand_grab::IsThrow). thrower: the creature
/// that lets go of it, kept as the body's thrower (none for a hand)
std::optional<bool> InitialisePhysicsFromHand(entt::entity object, glm::vec3 velocity, bool dontReplant,
                                              std::optional<PlayerNames> throwerPlayer,
                                              std::optional<glm::vec3> angularMomentum = std::nullopt, bool byCreature = false,
                                              entt::entity thrower = entt::null);

/// What the hand predicts at the release, sent with it.
struct ReleasePrediction
{
	glm::vec3 velocity {0.0f};        ///< the velocity after the turns
	glm::vec3 angularMomentum {0.0f}; ///< the angular momentum after the turns, this port's sign
	glm::vec3 origin {0.0f};          ///< the body's translation
	glm::mat3 rotation {1.0f};        ///< the body's rotation rows (glm's columns), for its yaw, pitch and roll
	/// the body's rotation and centre at the start of each turn and at the
	/// end, turns + 1 entries (for drawing the prediction)
	std::vector<std::pair<glm::mat3, glm::vec3>> history;
};
/// The release prediction, the physics part: a body that is not in the list, built by
/// the held object's physics setup at the pose the hand drew it with, L from the hand's angular
/// velocity, the hand's velocity capped at 124, SettleOnLand(1, !(tree || l2 > 1))
/// and min(turns, 15) turns of 20 substeps ClearForces, TouchGround, ApplyContacts,
/// Integrate, its result ignored; no other body, no game logic. nullopt when no body could be built. turns is the
/// caller's min(ping / 100, 5).
/// The release velocity cap: l2 = (z z + y y) + x x of the hand's velocity; above 124 x 124
/// and not all three 0 it is scaled to 124 (k = 124 / sqrt(l2), in single precision); fast = the uncapped l2 > 1
struct CappedReleaseVelocity
{
	glm::vec3 velocity {0.0f};
	bool fast {false};
};
[[nodiscard]] CappedReleaseVelocity CapReleaseVelocity(glm::vec3 handVelocity);
/// The body starts where the hand drew the object (where it hangs from the hand), not at its turn place
[[nodiscard]] std::optional<ReleasePrediction> PredictRelease(entt::entity object, glm::vec3 handVelocity,
                                                              glm::vec3 handAngularVelocity, uint32_t turns,
                                                              const glm::vec3& drawnPosition, const glm::mat3& drawnRotation);
/// The placement of an object let go that gets no physics body. In the original AddObject only fails for a class that
/// cannot become a physics object, an IMMOVABLE one or one already flying, and a body is always built (the body setup
/// reads the mesh unchecked); openblack also fails for an object with no mesh. A villager or animal ends its physics at
/// once with no body (villager_physics::EndPhysicsWithoutBody), as in the original. (approximate) anything else is put
/// on the ground and back in the map, where the original leaves it in the air, in the physics and out of the map.
void PlaceWithoutBody(entt::entity object);
/// Whether a thing let go of may land where it is (then on dry land or a high enough cell): not thrown, and not raised
/// over what was under it, unless it is a villager let go of for a computer player (by its hand or its creature)
[[nodiscard]] bool MayLand(bool thrown, bool raised, bool computersVillager);
/// Whether a let-go is through my interface, the local player's: its throw is the local player's (the local hand's, or
/// the local player's creature's); another player's, or no one's, is not
[[nodiscard]] bool IsMyLetGo(std::optional<PlayerNames> throwerPlayer, PlayerNames local);
/// The player whose creature may copy the player playing with a toy let go of: the player whose hand let go of it (the
/// local hand's or a computer player's); none for a creature's let-go or a let-go credited to no one
[[nodiscard]] std::optional<PlayerNames> ToyPlayer(std::optional<PlayerNames> throwerPlayer, bool byCreature);
/// Whether a tree that lands on land as it is let go of stays in the physics, not landed, instead of being replanted:
/// held tilted, more than 0.2 about either of its two tilts (the pitch and the roll of its matrix), or let go of not to
/// be replanted (dontReplant), unless it has a thrower, a creature letting go of it, which replants it whatever its tilt
[[nodiscard]] bool LandedTreeStaysInPhysics(bool byThrower, bool dontReplant, glm::vec2 tilt);
/// Whether an object the physics gives no body when a hand lets go of it hangs where it was let go: a thing that is
/// not living and is immovable (a script's SET_ID_MOVEABLE false), or a creed, which never becomes a physics object.
/// The original's start from the hand marks it in the physics and takes it out of the map before the physics refuses
/// it, and undoes neither, so it stays in the air, in the physics with no body, out of the map. A villager or an
/// animal ends its physics at once instead
[[nodiscard]] bool HangsWhereLetGo(entt::entity object);
/// The end of a let go (by the player's hand or a creature's) that the physics gave no body: a thing that hangs where
/// it was let go (HangsWhereLetGo) is left there, in the physics; anything else is put down (PlaceWithoutBody)
void LetGoWithoutBody(entt::entity object);
/// The physics' part of an object going into a hand: whether it may go in. One in the physics with no body there to
/// take out (a thing left hanging where a hand let go of it, or one a particle system carries) may not: the failed
/// pick files it back in the map's cells while it is available, and it stays in the physics, where it is
[[nodiscard]] bool TakeOutOfPhysicsForHand(entt::entity object);

/// Whether the entity is a fence: its mesh is
/// BuildingAmericanFence or one of the Celtic fences.
[[nodiscard]] bool IsFence(entt::entity entity);
} // namespace openblack::ecs::physics::from_hand
