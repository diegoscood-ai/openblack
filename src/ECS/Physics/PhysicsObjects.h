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
#include <optional>
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
	/// the turn-start matrix's rows (PhysOb +0xAC / +0xB8 / +0xC4 = po+0xD4 / +0xE0 / +0xEC, right / up / fwd = glm's
	/// columns 0 / 1 / 2): Villager::EndPhysics 0x5F0A60 and Animal::EndPhysics 0x5F0D80 read the landType from its
	/// right.y (po+0xD8), the villager's heading from its up / fwd rows (po+0xE0 / +0xEC). The current matrix is the
	/// body's (PhysOb +0x7C / +0x88 / +0x94 = po+0xA4 / +0xB0 / +0xBC: po+0xBC is the current fwd row, not this one).
	/// Written with turnStartCentre by SyncTurnStart
	glm::mat3 turnStartRotation {1.0f};
	/// the turn-start matrix's translation (PhysOb +0xD0 = po+0xF8; the current one, the centre of mass, is +0xA0 =
	/// po+0xC8): the matrix the drawing interpolates from (fn_007FCE80)
	glm::vec3 turnStartCentre {0.0f};
	/// a game turn has started since the body was added: until then it is drawn where it is. Before the first turn the
	/// two matrices are equal anyway: SetUpPos 0x7FC760 (0x7FC94C..0x7FC98F), AdjustToGroundLevel 0x7FCB80
	/// (0x7FCE56..0x7FCE6B) and so RaiseUntilNotIntersecting when it raises the body (SetUpPos at 0x644BE7) all copy
	/// the 12 floats of the current matrix into the turn-start one (PhysicsObjects::SyncTurnStart)
	bool turnStarted {false};
	uint32_t flags {0};
	bool villager {false};
	/// +0x1A4: 1 a villager (AddObject 0x64476D, AddProxy 0x644E67), 2 a felled tree (FelledTree::Create 0x511883), 3 a
	/// felled tree that has toppled (GameTurnUpdate 0x6460F8), else 0
	uint8_t kind {0};
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
		/// Living::InitialisePhysics / InitialisePhysicsFromHand (0x5EFD80): the object flies (FLYING). AddObject
		/// calls it once the body is built (fromHand: AddObject's own flag, the debug throws). From the hand
		/// (from_hand::InitialisePhysicsFromHand, through InitialisePhysicsOfClass) it comes last, after the whole of
		/// Object::InitialisePhysicsFromHand 0x636F00 (AddObject, AdjustToGroundLevel, RaiseUntilNotIntersecting, the
		/// LANDED test, RemoveObject or CreateDroppedResource and the flying-object reaction) and only when the object
		/// did not land: Living 0x5EFDD7..0x5EFDEB sets FLYING only when the po is not LANDED and po->object is still
		/// this one (a put-down villager or animal never enters FLYING).
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
		/// DropSfx (vt +0x794): RemoveObject 0x646B2E..0x646B48 plays it for the object EndPhysics returned, when LANDED
		/// and on land (only Tree has one, 0x74BC60; Object::DropSfx 0x63A7B0 is nothing)
		std::function<void(entt::entity)> dropSfx;
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
	/// the player are set, but the flying-object reaction (only spread when the object does not land, 0x637412) and the
	/// class's initialisePhysics (Living 0x5EFDD7, after the landing test) are left to the caller.
	static PhysicsObject* AddObjectFromHand(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity);
	/// The class's ClassHandlers::initialisePhysics, if any, for a body AddObjectFromHand built (the Living part of
	/// InitialisePhysicsFromHand 0x5EFDD7..0x5EFDEB: the caller only calls it when the object did not land).
	static void InitialisePhysicsOfClass(PhysicsObject& po, bool fromHand);
	/// The turn-start matrix becomes the current one (the `rep movsd` of 12 dwords PhysOb +0x7C -> +0xAC at the end of
	/// SetUpPos 0x7FC98F and of AdjustToGroundLevel 0x7FCE6B): turnStartRotation / turnStartCentre = the body's. Called
	/// by Add, AddProxy, BeginTurn (0x645187..0x64519B), RaiseUntilNotIntersecting after each raise, and by every caller
	/// of PhysOb::AdjustToGroundLevel right after it.
	static void SyncTurnStart(PhysicsObject& po);
	/// What a villager drops when released without landing (Villager::CreateDroppedResource 0x750A05..0x750A89):
	/// Object::InitialisePhysics(velocity, angularVelocity, thrower 0, add 1, status 0) (vt +0x784 -> AddObject), then
	/// with a body: po+0x90 = angularMomentum when given (PhysOb +0x68, L in world space), flag 0x10
	/// (NoObjectCollision), PhysOb::AdjustToGroundLevel(false, true) 0x7FCB80 (SyncTurnStart) and
	/// RaiseUntilNotIntersecting 0x644800.
	static PhysicsObject* AddDroppedObject(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity,
	                                       std::optional<glm::vec3> angularMomentum);
	/// RemoveObject (0x646A00) without EndPhysics.
	static void RemoveObject(entt::entity entity);
	/// RemoveObject(obj, true, true) (0x646A00): the object takes the body's pose (angles, position, altitude), its
	/// EndPhysics runs (vt +0x790), then the DropSfx (ClassHandlers::dropSfx) of the object it returned if LANDED and on
	/// land (0x646B2E..0x646B48), the flying-object reactions go (TODO(reactions)) and the body is removed.
	static void RemoveObjectWithEndPhysics(entt::entity entity);
	/// Object::EndPhysics 0x6375A0, its map part (0x637613..0x63763A): inside MapCoords::InBounds 0x6042C0 the object goes
	/// back in the map cells (InsertMapObject vt +0x544), outside it is deleted (ToBeDeleted(0) vt +0xC). Villager
	/// (0x5F0B81) and Animal (0x5F0E01) call it themselves in the middle of their EndPhysics. False when deleted.
	static bool BackInMap(entt::entity entity);
	/// RaiseUntilNotIntersecting (0x644800): resting bodies are made for the objects of the map cells under the body's
	/// square (C +- R) that InteractsWithPhysicsObjects (ShouldPhysicsRaiseObjectUntilNotIntersectingThis 0x6377D0),
	/// then the body goes up by max(fn_007FDD60 both ways) until no overlapping body pushes it more than 0.001. Each
	/// raise is fn_007FD140 + PhysOb::SetUpPos (0x644BE0 / 0x644BE7), so the turn-start matrix follows (SyncTurnStart).
	static void RaiseUntilNotIntersecting(PhysicsObject& po);
	[[nodiscard]] static PhysicsObject* Find(entt::entity entity);
	/// Is the object flying (in physics and not a resting proxy)?
	[[nodiscard]] static bool IsFlying(entt::entity entity);

	/// PhysicsObject::GameTurnUpdate 0x644FC0, once a game turn (GGame::ProcessTurn 0x54E67E, unpaused): the turn's
	/// start, its 20 substeps of 0.005 s in one go (0x64576F..0x64604D) and its end (impacts, sounds, reactions).
	static void GameTurnUpdate();
	/// Every frame: the dust, and the pose each moving body is drawn at, between the start and the end of the last turn
	/// by the turn fraction (fn_00646FE0 0x5E49DC -> fn_007FCE80; components::PhysicsDrawPose).
	static void UpdateFrame(float turnFraction, float seconds);
	static void Clear();
	/// Every physics object, read only (the renderer's shadows)
	static void ForEach(const std::function<void(const PhysicsObject&)>& func);

	PhysicsObjects() = delete;
};
} // namespace openblack::ecs::physics
