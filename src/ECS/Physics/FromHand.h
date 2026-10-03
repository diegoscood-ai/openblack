/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::physics::from_hand
{
/// What the hand owns that a throw needs (the hand's pots, trees and stores), set by the hand system.
struct HandHooks
{
	/// Pot::AddResourceToPos: a hand pot (HandWood / HandFood) put down becomes a pile or goes into a store
	std::function<void(entt::entity)> putDownHandPot;
	/// the object is a hand pot (PotInfo HandWood / HandFood)
	std::function<bool(entt::entity)> isHandPot;
};
void SetHandHooks(HandHooks hooks);

/// Object::ThrowObjectFromHand(status, dont_replant) 0x6385E0 for an object already out of the hand (the hand has
/// done GMagicHand::RemoveFromHand 0x5FB0B0): a hand pot slow enough is put down at once (Pot::InitialisePhysicsFromHand
/// 0x66DF00), else InitialisePhysicsFromHand with the holding spring's velocity. Release passes dont_replant 0,
/// ForceDropHeld 1 (packet 0x1D). Returns true when the object landed (put down, or LANDED out of the hand) instead of
/// flying.
bool Throw(entt::entity object, glm::vec3 springVelocity, bool dontReplant);
/// GInterface::ForceDropHeld 0x5D4350: packet 0x4D with zero velocity, then 0x1D -> ThrowObjectFromHand(status, 1)
bool ForceDrop(entt::entity object);

/// Object::InitialisePhysicsFromHand 0x636F00 (AddObject, AdjustToGroundLevel, RaiseUntilNotIntersecting, the LANDED
/// rule); nullopt when no body could be made, else whether it landed
std::optional<bool> InitialisePhysicsFromHand(entt::entity object, glm::vec3 velocity, bool dontReplant);
/// (approximate) openblack's placement of an object that gets no physics body. In the original AddObject 0x6443A0 only
/// fails for a class that cannot become a physics object, an IMMOVABLE one or one already flying, and a body is always
/// built (PhysOb::Initialise reads the mesh unchecked); openblack also fails for an object with no mesh.
void PlaceWithoutBody(entt::entity object);

/// GameThingWithPos::IsFence (vt +0x3CC) = MobileStatic::IsFence 0x609110: its GMobileStaticInfo's mesh is MESH_LIST
/// 0x38 (BuildingAmericanFence) or 0x51..0x52 (the Celtic fences).
[[nodiscard]] bool IsFence(entt::entity entity);
} // namespace openblack::ecs::physics::from_hand
