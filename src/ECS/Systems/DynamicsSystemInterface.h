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

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "ECS/PhysicsEntry.h"
#include "Graphics/GraphicsHandle.h"
#include "Physics/Body.h"

namespace openblack
{
struct GObjectInfo;
}

namespace openblack::physics::turn
{
enum class PuffKind : uint8_t;
}

namespace openblack::ecs::physics
{
struct State;

/// One puff as the renderer draws it (openblack, for the draw snapshot; components::Sprite + Transform before): a
/// camera-facing sprite with angle 0 and no origin (Renderer.cpp drawSprite), alpha blended or added
struct DustParticleDraw
{
	graphics::TextureHandle texture; ///< blobsa.raw
	glm::vec3 position;
	float halfSize;     ///< the quad's half width and half height (the Transform's scale x / y before)
	glm::vec2 uvMin;    ///< the cell's corner in the 8 x 8 sheet
	glm::vec2 uvExtent; ///< one cell, 1 / 8
	glm::vec4 tint;     ///< the colour premultiplied by its alpha
	bool additive {false}; ///< added to what is behind it (a mote), else blended over it
};
} // namespace openblack::ecs::physics

namespace openblack::ecs::systems
{
class DynamicsSystemInterface;

/// What each kind of object does as the physics moves it, beyond what any object does. The physics calls these at the
/// points the kinds' own parts take over, for the object's class (DynamicsSystemInterface::ClassOf); each default is
/// what an ordinary object does.
class PhysicsClassHooks
{
public:
	virtual ~PhysicsClassHooks() = default;

	/// The class's part of starting physics: the object flies (FLYING). AddObject calls it once the body is built
	/// (fromHand: AddObject's own flag, the debug throws; (approximate, debug path) for those the original would run the
	/// class's start with no hand, a scaffold's move included, but they come with fromHand). From the hand
	/// (from_hand::InitialisePhysicsFromHand, through InitialisePhysicsOfClass) it comes last, after the whole of the
	/// object's start from the hand (AddObject, SettleOnLand, RaiseClearOfWhatIsUnder, the LANDED test,
	/// RemoveObject or the dropped resource and the flying-object reaction) and only when the object did not land: FLYING
	/// is set only when the body is not LANDED and still belongs to this object (a put-down villager or animal never
	/// enters FLYING). An ordinary object does nothing more.
	virtual void StartPhysics(DynamicsSystemInterface& dynamics, PhysicsEntry& entry, bool fromHand);
	/// The class's part of the impact reaction: false when the object is gone (consumed, dead, turned into something
	/// else). None for an ordinary object, whose reaction is the physics' own.
	virtual std::optional<bool> ReactToImpact(DynamicsSystemInterface& dynamics, PhysicsEntry& entry, const ImpactInfo& impact);
	/// The class's part of the end of physics, after the transform is synced and the object's flying-object reactions
	/// are gone: the entity that stays (a tree becomes a DeadTree), or entt::null for none; putting it back in the map
	/// cells stays with the physics, unless CallsBackInMap. None for an ordinary object, whose end is the physics' own.
	virtual std::optional<entt::entity> EndPhysics(DynamicsSystemInterface& dynamics, PhysicsEntry& entry);
	/// The class's EndPhysics calls EndPhysicsAsObject itself, in the middle of its work, as Villager, Animal and
	/// Scaffold do; false: the physics calls it after EndPhysics
	[[nodiscard]] virtual bool CallsBackInMap(DynamicsSystemInterface& dynamics, entt::entity object);
	/// Asked once the body is low in the water and denser than it: true ends its physics (an ordinary object asks
	/// ecs::HasSunk)
	virtual bool HasSunk(DynamicsSystemInterface& dynamics, PhysicsEntry& entry);
	/// Every frame while the object moves (roots follow a tree...)
	virtual void Moved(DynamicsSystemInterface& dynamics, entt::entity object);
	/// The landing sound: RemoveObjectWithEndPhysics asks it for the object EndPhysics returned, when LANDED (only a
	/// tree on land has one)
	virtual void DropSound(DynamicsSystemInterface& dynamics, entt::entity object);
	/// The body of a class that builds its own, not from its mesh (a creature: its bounding sphere, mass 1000, not
	/// dynamic, a vertex per skeleton element). False for no body
	virtual bool SetUpBody(DynamicsSystemInterface& dynamics, entt::entity object, openblack::physics::Body& body);
	/// The class's own body mass when it overrides it, unclamped (a creature's 1000)
	virtual std::optional<float> BodyMassOf(DynamicsSystemInterface& dynamics, entt::entity object);
	/// The class's own weight as an object when it overrides it, unclamped (a building's by its info, a creature's)
	virtual std::optional<float> ObjectWeightOf(DynamicsSystemInterface& dynamics, entt::entity object);
	/// The class's start of physics for the call that adds no body (a particle system takes the object; with a body the
	/// physics calls StartPhysics): when not FLYING, StorePreviousState unless IN_HAND, and SetTopState(FLYING)
	/// must take; a villager drops its resource first. False refuses the take; an ordinary object takes it.
	virtual bool InitialisePhysicsWithoutBody(DynamicsSystemInterface& dynamics, entt::entity object);
	/// The class's start of physics in the world (anything but the hand), before the body is made, with the velocity
	/// and the spin as the caller gives them (the spin in the body's axes and the original's sign): a villager drops its
	/// resource, then a villager or an animal keeps its state as its previous one (not when held) and must take FLYING.
	/// False refuses: no physics at all. Already flying: taken, nothing changed. An ordinary object takes it.
	virtual bool StartPhysicsInWorld(DynamicsSystemInterface& dynamics, entt::entity object, glm::vec3 velocity,
	                                 glm::vec3 spin);
	/// The class's end of physics for an object that has no body (a particle system lets go of what it carried), called
	/// while the object is still marked carried, after it was put on the ground. There is no physics record, so nothing
	/// is read of one: no thrower, no player, no hand. The classes that need only the entities: a villager or an animal
	/// lands (villager_physics::EndPhysicsWithoutBody); a tree does nothing while it is carried (it stays in the physics,
	/// out of the map); any other object goes back in the map, or is deleted outside it (EndPhysicsAsObject)
	virtual void EndPhysicsWithoutBody(DynamicsSystemInterface& dynamics, entt::entity object);
	/// The class's start of physics for a resting proxy a moving body pushed (ProcessTurn): true when it started, and
	/// only then is the body woken. Villager: drops its resource, then the Living part (StorePreviousState unless
	/// IN_HAND, SetTopState(FLYING) must take); Animal: the Living part; the object part (CanWakeKnockedProxy) last.
	/// An ordinary object has only the object part.
	virtual bool InitialisePhysicsKnocked(DynamicsSystemInterface& dynamics, PhysicsEntry& entry);
	/// The class's part of a throw from the hand, before the object's own: a villager thrown with dontReplant that
	/// belongs to no player or to the hand's player stops being a disciple; the hook tests membership with the hand's
	/// player (PLAYER_ONE).
	virtual void ThrowFromHand(DynamicsSystemInterface& dynamics, entt::entity object, bool dontReplant);
	/// The class's override of CanBecomeAPhysicsObject (Scaffold); an ordinary object of the class can
	[[nodiscard]] virtual bool CanBecomePhysicsObject(DynamicsSystemInterface& dynamics, entt::entity object);
	/// The class's part of the start from the hand, before the object's own (Scaffold: DestroyThingsInWay when its flag
	/// is set)
	virtual void BeforeInitialisePhysicsFromHand(DynamicsSystemInterface& dynamics, entt::entity object);
	/// The class's part of the start from the hand, after the object's own returned a body, with its flags (FromHand,
	/// Landed) final (Scaffold: sets a flag when Landed and not dontReplant). Not called when no body was made
	virtual void InitialisedPhysicsFromHand(DynamicsSystemInterface& dynamics, entt::entity object, const PhysicsEntry& entry,
	                                        bool dontReplant);
	/// The creatures' part of a thing starting to fly (CheckAllCreaturesForCatching): every creature that can catch it
	/// sets about catching it. None for the physics alone
	virtual void OfferToCatchingCreatures(DynamicsSystemInterface& dynamics, entt::entity object, PhysicsEntry& entry);
};

/// The game's physics: thrown, dropped, knocked and pushed objects and the resting bodies near them, simulated once
/// a game turn (Locator::dynamicsSystem). Its state (physics::State) is the system's.
class DynamicsSystemInterface
{
public:
	virtual ~DynamicsSystemInterface() = default;

	/// The class whose hooks an object takes.
	[[nodiscard]] virtual PhysicsClass ClassOf(entt::entity entity) = 0;
	/// The kinds' own parts of the physics. Until they are set, every kind does what an ordinary object does
	virtual void SetClassHooks(std::unique_ptr<PhysicsClassHooks> hooks) = 0;
	/// The kinds' own parts; the physics' own dispatch goes through the same hooks
	[[nodiscard]] virtual PhysicsClassHooks& ClassHooks() = 0;

	/// Loads Data\PhysicsConstants.txt.
	virtual void LoadConstants() = 0;
	[[nodiscard]] virtual const openblack::physics::PhysicsData& Constants(int type) = 0;
	/// The row of the physics constants the object uses
	[[nodiscard]] virtual int ConstantsType(entt::entity entity) = 0;
	/// Whether it breaks buildings: rocks (physics row 3) and the toy with row 20.
	[[nodiscard]] virtual bool PhysicallyDestroysAbodes(entt::entity entity) = 0;
	/// Moving bodies hit it (it becomes a proxy near them).
	[[nodiscard]] virtual bool InteractsWithPhysicsObjects(entt::entity entity) = 0;
	/// Whether the object can be thrown or knocked into the physics
	[[nodiscard]] virtual bool CanBecomeAPhysicsObject(entt::entity entity) = 0;
	/// A knocked resting proxy is woken only when its start of physics succeeds: it can become a physics object and no
	/// particle system carries it
	[[nodiscard]] virtual bool CanWakeKnockedProxy(entt::entity entity) = 0;
	/// The class's body setup into a body that is not in the list: Body::Initialise, the constants, the vertices and
	/// the pose from the object's Transform (the hand's release prediction body)
	[[nodiscard]] virtual bool SetUpBody(entt::entity entity, openblack::physics::Body& body, bool dynamic) = 0;
	/// The object's weight (scale^3 x its info's weight, at least 0.01), as catching, pushing and the hand's tug weigh it
	[[nodiscard]] virtual float WeightOf(entt::entity entity) = 0;
	/// The mass its body is set up with: a building's and a fragment's own, a creature's, else its weight
	[[nodiscard]] virtual float BodyMassOf(entt::entity entity) = 0;
	/// The object's info (GObjectInfo), or null.
	[[nodiscard]] virtual const GObjectInfo* ObjectInfo(entt::entity entity) = 0;

	/// The object flies from where its transform is. throwerPlayer: the player who threw it, if any
	virtual PhysicsEntry* AddObject(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity,
	                                entt::entity thrower = entt::null, bool fromHand = false,
	                                std::optional<PlayerNames> throwerPlayer = std::nullopt) = 0;
	/// The original's angular velocity: in the body's axes and with its sign
	struct BodySpin
	{
		glm::vec3 w {0.0f};
	};
	/// AddObject with the angular velocity as the original takes it (Body::SetAngularVelocityFromBody), bit-exact; the
	/// world-omega overload above goes through R^T.
	virtual PhysicsEntry* AddObject(entt::entity entity, glm::vec3 velocity, BodySpin spin, entt::entity thrower = entt::null,
	                                bool fromHand = false, std::optional<PlayerNames> throwerPlayer = std::nullopt) = 0;
	/// AddObject for the start from the hand (physics::from_hand): the FromHand flag, the throwing player and the thrower
	/// (a creature that lets go of it; none for a hand) are set, but the flying-object reaction (only spread when the
	/// object does not land) and the class's StartPhysics (after the landing test) are left to the caller.
	virtual PhysicsEntry* InitialisePhysics(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity,
	                                        std::optional<PlayerNames> throwerPlayer, entt::entity thrower) = 0;
	/// The class's PhysicsClassHooks::StartPhysics for a body InitialisePhysics built (the caller only calls
	/// it when the object did not land).
	virtual void InitialisePhysicsOfClass(PhysicsEntry& po, bool fromHand) = 0;
	/// The class's PhysicsClassHooks::ThrowFromHand (from_hand::Throw)
	virtual void ThrowFromHandOfClass(entt::entity entity, bool dontReplant) = 0;
	/// The turn-start matrix becomes the current one, as at the end of SetUpPos and SettleOnLand:
	/// turnStartRotation / turnStartCentre = the body's. Called by Add, AddProxy, BeginTurn, RaiseClearOfWhatIsUnder
	/// after each raise, and by every caller of Body::SettleOnLand right after it.
	virtual void SyncTurnStart(PhysicsEntry& po) = 0;
	/// What a villager drops when released without landing: AddObject with the velocities and no thrower, then with a
	/// body: the angular momentum when given (L in world space), NoObjectCollision, Body::SettleOnLand(false,
	/// true) (SyncTurnStart) and RaiseClearOfWhatIsUnder.
	virtual PhysicsEntry* AddDroppedObject(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity,
	                                       std::optional<glm::vec3> angularMomentum) = 0;
	/// The same with the dropped object's w as the original passes it (body axes, its sign: BodySpin); the
	/// angularMomentum, when given, is in this port's sign
	virtual PhysicsEntry* AddDroppedObject(entt::entity entity, glm::vec3 velocity, BodySpin spin,
	                                       std::optional<glm::vec3> angularMomentum) = 0;
	/// Takes the object out of the physics without EndPhysics. Nothing during a turn's steps and its end: the entry stays,
	/// and goes at the next turn start once its object is no longer available.
	virtual void RemoveObject(entt::entity entity) = 0;
	/// The dropped object's part after the body is added, shared by both AddDroppedObject
	virtual PhysicsEntry* FinishDroppedObject(PhysicsEntry& body, std::optional<glm::vec3> angularMomentum) = 0;
	/// The object takes the body's pose (angles, position, altitude), its EndPhysics runs, then the landing sound
	/// (PhysicsClassHooks::DropSound) of the object it returned if LANDED and on land, the flying-object reactions go
	/// (TODO(reactions)) and the body is removed. Nothing during a turn's steps and its end, as RemoveObject.
	virtual void RemoveObjectWithEndPhysics(entt::entity entity) = 0;
	/// The map part of the end of physics: inside the map the object goes back in the map cells, outside it is deleted.
	/// Villager and Animal call it themselves in the middle of their end of physics. False when deleted.
	virtual bool EndPhysicsAsObject(entt::entity entity) = 0;
	/// Resting bodies are made for the objects of the map cells under the body's square (C +- R) that
	/// InteractsWithPhysicsObjects, then the body goes up by the larger penetration of both ways until no overlapping
	/// body pushes it more than 0.001. Each raise goes through Body::SetUpPos, so the turn-start matrix follows
	/// (SyncTurnStart).
	virtual void RaiseClearOfWhatIsUnder(PhysicsEntry& po) = 0;
	/// The object's entry, none for an object no longer valid (its entry waits for the next turn start)
	[[nodiscard]] virtual PhysicsEntry* Find(entt::entity entity) = 0;
	/// Is the object flying (in physics and not a resting proxy)?
	[[nodiscard]] virtual bool IsFlying(entt::entity entity) const = 0;
	/// A Living pushes the object (a villager clearing its way): its body (the one in the physics, else a new one when
	/// no particle system carries it) gets PushedByLiving and external force += F d / |d|, d = T - the object's
	/// MapCoords point, F = weight x 9.81 (the pusher is not read). Returns 0.0 (the only caller drops it).
	virtual float PushObject(entt::entity object) = 0;
	/// The object's rotation from a body's (or an atom's) rows, as the original sets its angles from them: a Villager,
	/// Animal, Tree or Feature takes only the Y angle and stands upright; a thing that can lean (placed_turn::CanLean:
	/// the mobile statics and the mobile objects) takes RotationYXZ(y, x, z); anything else keeps the rows (pending: the
	/// other plain objects, which stand upright on their Y angle in the original).
	[[nodiscard]] virtual glm::mat3 RotationFromRows(entt::entity entity, const glm::mat3& rows) = 0;
	/// The draw's quarter turn of an animated mesh: a Villager's or an Animal's drawn rotation (openblack's Transform)
	/// from the original's rows (their bodies are in the original's axes).
	[[nodiscard]] virtual glm::mat3 DrawQuarterTurn(const glm::mat3& rows) = 0;
	/// From AddObject and the start from the hand (physics::from_hand): every creature that can catch the object starts
	/// catching it. The creatures' part is the class hooks' (PhysicsClassHooks::OfferToCatchingCreatures).
	virtual void CheckAllCreaturesForCatching(entt::entity object, PhysicsEntry& po) = 0;

	/// Once a game turn, unpaused: the turn's start, its 20 substeps of 0.005 s in one go and its end (impacts, sounds,
	/// reactions, deeds and the scripts' hit record).
	virtual void ProcessTurn() = 0;
	/// The scripts' record of the last thing hit in the physics and what hit it. The end of each turn writes it for
	/// every body something hit, in list order, so the last one wins; the scripts clear it with entt::null. Each is kept
	/// only when it is still available as it is written (not gone, not marked for deletion, and not a dying villager),
	/// and read back as it was kept.
	virtual void RecordHit(entt::entity hit, entt::entity hitter) = 0;
	/// The record forgets each of its two that is no longer available (as RecordHit asks it), and keeps the
	/// other: at the start of the scripts' turn, and at a land change once the last land's things are gone
	virtual void ForgetUnavailableHits() = 0;
	/// The record forgets both of its two, available or not: the scripts' restart, before the program is read
	virtual void ForgetHits() = 0;
	/// The last thing hit, entt::null for none
	[[nodiscard]] virtual entt::entity GetHitObject() const = 0;
	/// What hit it, entt::null for none
	[[nodiscard]] virtual entt::entity GetObjectWhichHit() const = 0;
	/// Every frame: the dust, and the pose each moving body is drawn at, between the start and the end of the last turn
	/// by the turn fraction (components::PhysicsDrawPose).
	virtual void UpdateFrame(float turnFraction, float seconds) = 0;
	virtual void ResetSimulation() = 0;

	/// The hand starts a release prediction: the old prediction ends and the new one is active with the held object
	virtual void BeginPrediction(entt::entity object) = 0;
	/// The prediction body and its history from from_hand::PredictRelease, for the drawing; the turn counter restarts
	/// at 0
	virtual void SetPrediction(entt::entity object, openblack::physics::Body body,
	                           std::vector<std::pair<glm::mat3, glm::vec3>> history) = 0;
	/// When the prediction is this object's, it ends (the start from the hand).
	virtual void EndPredictionOf(entt::entity object) = 0;
	/// The prediction is active and its object is this one: the hand skips drawing the held object then (its
	/// PhysicsDrawPose is the prediction's)
	[[nodiscard]] virtual bool IsPredictionObject(entt::entity object) = 0;
	/// Every physics object, read only (the renderer's shadows), but not an entry whose object is no longer valid
	virtual void ForEachEntry(const std::function<void(const PhysicsEntry&)>& func) const = 0;

	/// The dust puffs of the physics (fragments and the ground impacts of the collision sounds): smoke cells of
	/// data\blobs.raw (rows 2-3 of the 8 x 8 sheet), a camera-facing alpha-blended quad of constant colour that grows in
	/// 0.125 s and shrinks to nothing at 1 s, moving at its velocity with no gravity. Game time: they stop while paused.
	/// They are no entities (the original's are particles, not game objects): the renderer draws SnapshotDust's list.
	/// A creature's alignment smoke and motes share the pool, with their own kinds.
	/// One puff: argb as in the original (0xAARRGGBB); size is the quad's half size at its largest.
	virtual void AddPuff(glm::vec3 position, glm::vec3 velocity, float size, uint32_t argb) = 0;
	/// A puff of a kind in the same pool as the dust (physics::turn::PuffKind): it lives, grows and shows its frames as
	/// its kind does. `size` is the largest half size of its kind's curve (physics::turn::DustPuffSize)
	virtual void AddPuff(glm::vec3 position, glm::vec3 velocity, float size, uint32_t argb,
	                     openblack::physics::turn::PuffKind kind) = 0;
	/// The velocity of a fragment's puffs: (GameRand(201) - 100) x 0.02 on each axis, +-2 units per second, drawn z, y, x
	/// on the synced stream.
	[[nodiscard]] virtual glm::vec3 SyncedPuffVelocity() = 0;
	/// Every puff goes (a new map)
	virtual void ClearDust() = 0;
	/// `out` is cleared and gets every live puff in emission order, as the frame's update left it (a puff added since
	/// has its first cell and size 0, as its Sprite had)
	virtual void SnapshotDust(std::vector<physics::DustParticleDraw>& out) = 0;
	/// audio::PlayAnimationEffect on editor.sad as the physics calls it (collisions, building hits and destruction):
	/// the distance is the camera's to `at`, the row of editor.sad's table that fits the 5 columns picks the sample, and
	/// the channel belongs to the object (so the .sad play mode applies per object: G_BigSplash's mode 2 plays nothing
	/// while the same object's splash plays) at its point, following it every turn with `track`.
	virtual void PlayAnimEffect(const std::array<int32_t, 5>& key, entt::entity owner, glm::vec3 at, bool track) = 0;

	/// The constants, the bodies, the release prediction, the class hooks and the other hooks, the dust puffs and the collision
	/// sound pairs. from_hand works on it too
	[[nodiscard]] virtual physics::State& GetState() noexcept = 0;
};
} // namespace openblack::ecs::systems
