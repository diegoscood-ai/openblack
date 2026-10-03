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
#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "PhysOb.h"

namespace openblack
{
struct GObjectInfo;
}

namespace openblack::ecs::physics
{
/// PhysicsObject (0x1DC bytes, list at 0xD47814): one body of the physics system, thrown or knocked (awake) or a
/// resting obstacle near a moving body (asleep proxy).
struct PhysicsObject
{
	enum Flags : uint32_t
	{
		Awake = 0x1,
		/// Object::PushObject 0x6396BA / Ball::KickBallAtDestination 0x435D9B: pushed by a Living (a villager is not
		/// raised over it by RaiseUntilNotIntersecting)
		PushedByLiving = 0x2,
		FromHand = 0x4,
		Landed = 0x8,
		NoObjectCollision = 0x10,
	};

	entt::entity entity {entt::null};
	entt::entity thrower {entt::null};
	PhysOb body;
	/// the body's rotation at the start of the turn (po+0xBC.. / +0xD8, what Animal::EndPhysics reads the landType from)
	glm::mat3 turnStartRotation {1.0f};
	uint32_t flags {0};
	bool villager {false};
	/// the player's hand threw it, directly or through what it hit (GInterfaceStatus +0x24, inherited by proxies)
	bool byPlayer {false};
	glm::vec3 forceSum {0.0f}; ///< +0x0C: the force of the touched substeps of this turn
	float impact {0.0f};       ///< +0x08: |forceSum| x 0.05, the mean force of the turn
	PhysicsObject* hitBy {nullptr};
	/// the squared distance to the camera at the last substep (the fly-by whoosh)
	float cameraDistance2 {1e9f};

	/// G of the turn: impact / (mass x g), 1 while lying on the ground
	[[nodiscard]] float GLoad() const { return impact / (body.Mass() * PhysOb::k_Gravity); }
};

/// The object classes whose physics virtuals (InitialisePhysics, ReactToPhysicsImpact vt +0x7AC, EndPhysics vt +0x790,
/// HasSunk vt +0x7B8) another system owns. One class per object (PhysicsObjects::ClassOf).
enum class PhysicsClass : uint8_t
{
	Villager,
	Animal,
	Tree,
	DeadTree,
	Pot,
	Rock,
	Fragment,
	Building, ///< Abode and StoragePit
	Shield,   ///< PhysicalShield (MapShield)
	Other,
	_Count
};

/// What the physics knows of a body's impact at the end of a game turn (PhysicsObject::GameTurnUpdate 0x644FC0).
struct ImpactInfo
{
	float g {0.0f};                    ///< impact / (mass x g), PhysicsObject::GLoad
	entt::entity hitBy {entt::null};   ///< the body that hit it (PhysicsObject +0x20), or null
	entt::entity thrower {entt::null}; ///< PhysicsObject +0x1C
	bool byPlayer {false};             ///< the hand threw it (GInterfaceStatus +0x24)
};

/// The physics system (PhysicsObject::GameTurnUpdate 0x644FC0 and friends).
class PhysicsObjects
{
public:
	/// A class's physics virtuals, set by the system that owns the class. An empty function keeps the physics' own
	/// code for that class (or nothing, when it has none).
	struct ClassHandlers
	{
		/// Living::InitialisePhysics / InitialisePhysicsFromHand (0x5EFD80): the body was just added (fromHand: by
		/// the hand's InitialisePhysicsFromHand 0x636F00).
		std::function<void(entt::entity, PhysicsObject&, bool fromHand)> initialisePhysics;
		/// ReactToPhysicsImpact (vt +0x7AC), the class's part. Returns false when the object is gone (consumed, dead,
		/// turned into something else).
		std::function<bool(entt::entity, PhysicsObject&, const ImpactInfo&)> reactToImpact;
		/// EndPhysics (vt +0x790), the class's part, after the transform is synced and the object's flying-object
		/// reactions are gone (Object::EndPhysics 0x6375A0). Returns the entity that stays (a tree becomes a
		/// DeadTree), or entt::null for none; putting it back in the map cells stays with the physics.
		std::function<entt::entity(entt::entity, PhysicsObject&)> endPhysics;
		/// HasSunk (vt +0x7B8), asked from 0x645A01 once the body is denser than the water: true ends its physics.
		std::function<bool(entt::entity, PhysicsObject&)> hasSunk;
		/// Every frame while the object moves (roots follow a tree...).
		std::function<void(entt::entity)> moved;
	};

	/// The class whose handlers an object takes.
	[[nodiscard]] static PhysicsClass ClassOf(entt::entity entity);
	static void SetClassHandlers(PhysicsClass type, ClassHandlers handlers);

	/// Loads Data\PhysicsConstants.txt (EditorPhysics::Load 0x5249D0).
	static void LoadConstants();
	[[nodiscard]] static const PhysicsData& Constants(int type);
	/// Object::GetPhysicsConstantsType
	[[nodiscard]] static int ConstantsType(entt::entity entity);
	/// Object::InteractsWithPhysicsObjects: moving bodies hit it (it becomes a proxy near them).
	[[nodiscard]] static bool InteractsWithPhysicsObjects(entt::entity entity);
	/// Object::CanBecomeAPhysicsObject
	[[nodiscard]] static bool CanBecomeAPhysicsObject(entt::entity entity);
	/// Object::GetWeight, at least 0.01
	[[nodiscard]] static float Weight(entt::entity entity);
	/// The object's info (GObjectInfo), or null.
	[[nodiscard]] static const GObjectInfo* ObjectInfo(entt::entity entity);

	/// AddObject (0x6443A0): the object flies from where its transform is.
	static PhysicsObject* AddObject(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity,
	                                entt::entity thrower = entt::null, bool fromHand = false);
	/// AddObject for Object::InitialisePhysicsFromHand 0x636F00 (physics::from_hand): PHYSICS_OBJECT_FLAG_FROM_HAND and
	/// the player are set, but the flying-object reaction is left to the caller (it is only spread when the object does
	/// not land, 0x637412).
	static PhysicsObject* AddObjectFromHand(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity);
	/// RemoveObject (0x646A00) without EndPhysics.
	static void RemoveObject(entt::entity entity);
	/// RemoveObject(obj, true, true) (0x646A00): the object takes the body's pose (angles, position, altitude), its
	/// EndPhysics runs (vt +0x790), then Tree::DropSfx if LANDED on land (0x646B2E; the tree's is in its replanting),
	/// the flying-object reactions go (TODO(reactions)) and the body is removed.
	static void RemoveObjectWithEndPhysics(entt::entity entity);
	/// RaiseUntilNotIntersecting (0x644800): resting bodies are made for the objects of the map cells under the body's
	/// square (C +- R) that InteractsWithPhysicsObjects (ShouldPhysicsRaiseObjectUntilNotIntersectingThis 0x6377D0),
	/// then the body goes up by max(fn_007FDD60 both ways) until no overlapping body pushes it more than 0.001.
	static void RaiseUntilNotIntersecting(PhysicsObject& po);
	[[nodiscard]] static PhysicsObject* Find(entt::entity entity);
	/// Is the object flying (in physics and not a resting proxy)?
	[[nodiscard]] static bool IsFlying(entt::entity entity);

	/// Runs the 0.005 s substeps for the elapsed time; every 20 of them close a game turn.
	static void Update(float seconds);
	static void Clear();
	/// Every physics object, read only (the renderer's shadows)
	static void ForEach(const std::function<void(const PhysicsObject&)>& func);

	PhysicsObjects() = delete;
};
} // namespace openblack::ecs::physics
