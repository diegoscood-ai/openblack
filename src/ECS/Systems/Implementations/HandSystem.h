/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdlib>

#include <memory>
#include <optional>

#include <L3DFile.h>
#include <entt/entity/entity.hpp>

#include "3D/HandAnimator.h"
#include "3D/HandCrossFade.h"
#include "3D/HandMorph.h"
#include "3D/HandNavigationPose.h"
#include "3D/HandPoseBuffers.h"
#include "3D/ScreenPick.h"
#include "Audio/Engine/SamplePlay.h"
#include "Common/Zoomer.h"
#include "Creature/CreatureGiving.h"
#include "ECS/Components/HandClicked.h"
#include "ECS/HandCreature.h"
#include "ECS/HandMeshRay.h"
#include "ECS/HandSeedStep.h"
#include "ECS/PotResource.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Enums.h"
#include "Hand/HandGrabRules.h"
#include "Hand/HandTricons.h"
#include "Help/ToolTips.h"
#include "Input/GamePackets.h"
#include "Magic/HandMotion.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs
{
struct PhysicsEntry;
struct ImpactInfo;
} // namespace openblack::ecs

namespace openblack::hand_demo
{
struct Record;
} // namespace openblack::hand_demo

namespace openblack::ecs::systems
{

class HandSystem final: public HandSystemInterface, public HandGrabSystemInterface
{
public:
	/// HandTurn.cpp: the packet handlers go with the system (game_packets::ClearHandlers)
	~HandSystem();
	bool Initialize() noexcept override;
	[[nodiscard]] std::array<entt::entity, static_cast<size_t>(Side::_Count)> GetPlayerHands() const noexcept override;
	[[nodiscard]] std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetPlayerHandPositions() const noexcept override;
	/// HandMorph.cpp: the render hand's preparation for drawing: the texture set, the alignment, the morph update. The
	/// picked point and whether it is held are not read: the influence is tested at the interaction point.
	void UpdateAlignmentMorph(std::optional<map_coords::MapCoords>, bool) noexcept override;

	void Place(std::optional<glm::vec3> groundPoint, glm::vec3 cameraForward, bool gripping,
	           std::chrono::microseconds dt) noexcept override;
	void PrepareState() noexcept override;
	void Update(std::chrono::microseconds dt, glm::vec2 mouseDelta, bool gripping, bool actionHeld) noexcept override;
	void ActionDoubleClick() noexcept override { _actionDoubleClick = true; }
	[[nodiscard]] MagicSystemInterface::HandResult PressSeedAction() noexcept override;
	[[nodiscard]] MagicSystemInterface::HandResult ReleaseSeedAction() noexcept override;
	std::optional<glm::vec3> ResolveCursorPoint(const glm::vec3& origin, const glm::vec3& direction, CursorLand land,
	                                            bool gripping, std::chrono::microseconds dt) noexcept override;
	/// The held object while it is available (IsInteractable): a deleted one is not held any more (ValidateHands)
	[[nodiscard]] std::optional<entt::entity> GetHeldObject() const noexcept override
	{
		return _held && Interactable(*_held) ? _held : std::nullopt;
	}
	[[nodiscard]] std::optional<entt::entity> GetCursorObject() const noexcept override;
	[[nodiscard]] bool IsInLockedSelect(entt::entity object) const noexcept override;
	[[nodiscard]] bool HoldsLockedSelect() const noexcept override
	{
		return _statusCreatureLocked || _statusLockedObject.has_value();
	}
	[[nodiscard]] bool IsCreatureLockedForHand() const noexcept override { return _statusCreatureLocked; }
	/// PlaceObjectInMagicHand for callers that already ran the object's InterfaceSetInMagicHand: only the hand's
	/// part, which plays no pick-up sound
	void PlaceObjectInMagicHand(entt::entity entity) noexcept override { PickUp(entity, false); }
	[[nodiscard]] std::optional<PlayerNames> GetSourceOwner() const noexcept override { return _sourceOwner; }
	// HandSpellSeed.cpp
	[[nodiscard]] bool IsHandReadyForObject() const noexcept override;
	void ForceDropHeld() noexcept override;
	// The hand grab's slot, served by this system until the hand grab is its own: what it holds counts a spell seed in
	// the hand, which his leaves to the magic
	[[nodiscard]] std::optional<entt::entity> GetHeld() const noexcept override { return GetHeldObject(); }
	[[nodiscard]] std::vector<entt::entity> FallingRootsNewestFirst() const override
	{
		std::vector<entt::entity> roots;
		roots.reserve(_fallingRoots.size());
		for (auto it = _fallingRoots.rbegin(); it != _fallingRoots.rend(); ++it)
		{
			roots.push_back(it->entity);
		}
		return roots;
	}
	void ForceDrop() noexcept override { ForceDropHeld(); }
	// HandGiving.cpp: giving what the hand holds to a creature
	bool HandOver(entt::entity object) noexcept override;
	bool TakeIntoHand(entt::entity object) noexcept override;
	bool GiveHeldTo(entt::entity creature) noexcept override;
	[[nodiscard]] std::optional<entt::entity> GetOfferedCreature() const noexcept override;
	[[nodiscard]] float GetOfferTurn() const noexcept override;
	void EndAction() noexcept override;
	// HandTurn.cpp
	void BoxTakesInput() noexcept override;
	void GetSpellInfo(glm::vec3& interfacePos, glm::vec3& handPos, glm::vec3& cameraForward,
	                  glm::vec3& velocity) const noexcept override;
	[[nodiscard]] float GetHandScale() const noexcept override { return _handScale; }
	[[nodiscard]] glm::mat4 GetHandMatrix() const noexcept override;
	void UpdateInterfaceHandState() noexcept override
	{
		const auto hand = InterfaceHandState();
		_interfaceHandState = hand.state;
		_crossHoldBacks += hand.holdsCrossBack ? 1u : 0u;
	}
	bool OfferScreenObject(uint32_t id, float depth) noexcept override;
	[[nodiscard]] std::optional<uint32_t> ScreenObjectUnderHand() const noexcept override { return _screenObject; }
	[[nodiscard]] std::optional<uint32_t> DraggedScreenObject() const noexcept override
	{
		// the bubble's own test: the interface hand state of the last turn is 0x1D (not 0x1E, the one in the
		// citadel)
		return _interfaceHandState == 0x1D ? _screenObject : std::nullopt;
	}
	[[nodiscard]] std::optional<glm::vec3> GetHoverLandPoint() const noexcept override { return _cursorLand; }
	[[nodiscard]] int32_t GetInterfaceHandState() const noexcept override { return _interfaceHandState; }
	/// HandTurn.cpp: the hand's part of the interface's processing, once a turn at the turn's start
	void ProcessTurn() noexcept override;
	void GameTurnUpdate() noexcept override;
	[[nodiscard]] std::optional<entt::entity> GetRenderHandObject() const noexcept override
	{
		return _renderHandHeld && Interactable(*_renderHandHeld) ? _renderHandHeld : std::nullopt;
	}
	void SetThrowBlock(const std::array<float, 12>& block) noexcept override;
	/// A hand demo record's hand position and angles, valid until the next per-frame write of the render hand's
	/// state (Place): the seed senders copy them as they are
	std::optional<std::pair<glm::vec3, glm::vec3>> _recordedHandPose;
	void ResetTurnState(bool newGame) noexcept override;
	void ProcessStatusAtClearMap(bool newGame) noexcept override;
	[[nodiscard]] glm::vec3 GetTurnHandVelocity() const noexcept override { return _turnVelocity; }
	[[nodiscard]] int32_t GetActionStateNumber() const noexcept override { return ActionStateNumber(); }
	[[nodiscard]] entt::entity GetClickedObject() const noexcept override;
	[[nodiscard]] std::optional<glm::vec3> GetActionPoint() const noexcept override { return _interactionPoint; }
	[[nodiscard]] map_coords::MapCoords GetSyncedPlace() const noexcept override { return _syncMapCoords; }
	void SetHandReach(float metres) noexcept override { _handReach = metres <= 1800.0f ? metres : 1800.0f; }
	[[nodiscard]] float GetHandReach() const noexcept override { return _handReach; }
	[[nodiscard]] uint32_t GetStillTurns() const noexcept override { return _stillTurns; }
	[[nodiscard]] std::vector<entt::entity> GetThrownObjects() const noexcept override;
	[[nodiscard]] const std::vector<glm::mat4>* GetBoneMatrices() const noexcept override;
	[[nodiscard]] CameraIconFacts GetCameraIconFacts() const noexcept override;
	[[nodiscard]] std::optional<glm::ivec2> GetCursorPin() const noexcept override { return _cursorPin; }
	[[nodiscard]] bool IsDrawPreparationStopped() const noexcept override { return _preparationStopped; }
	[[nodiscard]] std::vector<std::string> GetAnimationNames() const noexcept override;
	[[nodiscard]] const std::string& GetCurrentAnimation() const noexcept override;
	void SetAnimationOverride(const std::string& name) noexcept override;
	void StartFixedPosAnimation(const std::string& clip, glm::vec3 point) noexcept override;

	// ---- HandPhysics.cpp: the physics of trees, dead trees and pots (ecs::PhysicsGameHooks calls them) ----
	/// A tree or dead tree hitting a wood store turns into wood. False when it went into the store
	bool TreeReactToImpact(entt::entity entity, ecs::PhysicsEntry& po) override;
	/// A tree's end of physics: planted again, or a DeadTree. The entity that stays
	entt::entity TreeEndPhysics(entt::entity entity, ecs::PhysicsEntry& po) override;
	/// Every frame while a tree moves: its roots follow it
	void TreeMoved(entt::entity entity) override;
	/// The sound of a tree put down gently on land
	void TreeDropSound(entt::entity entity) override;
	/// A thrown pot or pile hitting a store or a pot of its type goes into it. False when it went
	bool PotReactToImpact(entt::entity entity, ecs::PhysicsEntry& po, const ecs::ImpactInfo& impact) override;
	/// A pot's end of physics: a hand pot on land becomes a pile, put down from the thrower's interface or from none. The
	/// entity that stays, or entt::null
	entt::entity PotEndPhysics(entt::entity entity, std::optional<PlayerNames> throwerPlayer) override;

private:
	void LoadAnimations() noexcept;
	void LoadGeometry() noexcept;
	[[nodiscard]] std::optional<entt::entity> FindObjectUnderHand() const noexcept;
	/// A hand demo's action click lands on the recorded kind of thing near the recorded place, or on no thing: it
	/// changes the object under the cursor and its distance before the press reads them (HandNearObject.cpp)
	void RetargetDemoClick(const hand_demo::Record& click) noexcept;
	/// `genericPickupSounds` false when the PlaceInHand path already played them at the send (GenericPickupSounds)
	void PickUp(entt::entity entity, bool genericPickupSounds = true,
	            std::optional<glm::vec3> statusPoint = std::nullopt) noexcept;
	/// The local player's interface as the one that puts things down or takes them, with the record of its last pick-up
	[[nodiscard]] pot_resource::Dropper InterfaceStatus() const noexcept
	{
		return {.hasInterface = true, .player = PlayerNames::PLAYER_ONE, .isMyInterface = true, .sourceOwner = _sourceOwner};
	}
	/// The interface of the player who threw a body: this hand's own when it is this hand's player, else that player's
	/// (pot_resource::DropperOf), and none for nobody
	[[nodiscard]] pot_resource::Dropper ThrowerStatus(std::optional<PlayerNames> throwerPlayer) const;
	/// The generic pick-up's sounds (the pick-up and a villager's scream)
	void GenericPickupSounds(entt::entity entity, bool inPhysics) noexcept;
	/// HandTurn.cpp: the generic pick-up sends the PlaceInHand packet, action state 7 (WAIT FOR PLACE IN HAND)
	void SendPlaceInHand(entt::entity entity) noexcept;
	/// HandHolding.cpp: the render hand takes the object at the press: the held object and the hold parameters
	void RenderHandPickUp(entt::entity entity) noexcept;
	/// HandHolding.cpp: the render hand lets its object go (no physics; it holds nothing). A release of an object asks
	/// for two draw-list rebuilds; a swap to the object a pick-up puts in the hand (`swapped`) does not, as the original
	/// swaps without a release
	void RenderHandRelease(bool swapped = false) noexcept;
	/// HandHolding.cpp: while the render hand holds an object the status's hand does not hold yet, its draw-only pose
	void UpdateRenderHandHeldPose() noexcept;
	/// The object the render hand holds, set at the press for a tuggable object, one or two turns before the
	/// PlaceInHand packet puts it in the status's hand (_held)
	std::optional<entt::entity> _renderHandHeld;
	/// The held object's need-sorting bit before the hand set it, put back when the render hand lets it go
	bool _renderHandHadNeedsSorting {false};
	/// HandHolding.cpp: the render hand's pick-up and hold on the object: the need-sorting bit saved and set
	void RenderHandSetHeldFlags(entt::entity entity) noexcept;
	/// The object the render hand let go last, until its ReleaseImpulse packet
	std::optional<entt::entity> _lastReleased;
	/// 180 each HOLDING frame, then down by the frame's game ms
	int32_t _releaseImpulseMs {0};
	/// The required hand position of the last HOLDING / GRAIN frame
	glm::vec3 _requiredHandPosition {0.0f};
	/// The cursor's ground point as the last normal, camera, holding or grain update left it
	glm::vec3 _heldGroundPoint {0.0f};
	/// The hand's land point under the cursor this frame (ResolveCursorPoint), which those updates take as the ground point
	std::optional<glm::vec3> _cursorLandPoint;
	/// The object of the render hand's state: the render hand's, else the status's
	[[nodiscard]] std::optional<entt::entity> RenderHeld() const noexcept { return _renderHandHeld ? _renderHandHeld : _held; }
	/// HandTurn.cpp: PlaceObjectInMagicHand, the PlaceInHand packet's handler; re-entered by the forest and the
	/// teleport stone
	void ApplyPlaceInHand(entt::entity object) noexcept;
	/// HandHolding.cpp: a gentle release (zero velocity) through Release
	void Drop() noexcept;
	/// HandHolding.cpp: the held object's ApplyThisToMapCoord: a pot, a tree, a dead tree or a fence over a building site
	/// that stores its resource goes into the site (3, ecs::held_apply::ApplyHeldToMapCoord); else ThrowObjectFromHand
	/// (mobile, mobile static and single map fixed objects: 0x16); any other store takes a held object only on the press
	/// (HandApplyToObject.cpp) or when its body hits it. Returns the result for HandleApplyResult; 0 with nothing held
	int Release(glm::vec3 velocity, std::optional<glm::vec3> mapPoint = std::nullopt, bool statusPose = false) noexcept;
	/// HandHolding.cpp: ThrowObjectFromHand(status, dont_replant): physics::from_hand::Throw with the object still in
	/// the hand; it returns 0x16 and HandleApplyResult takes it out (RemoveFirstFromHand). Release passes dont_replant
	/// 0, ForceDropHeld 1 (the ThrowHeld packet, no ApplyThisToMapCoord)
	/// With statusPose, the held object first takes the pose the ThrowData packet carried
	int ThrowObjectFromHand(glm::vec3 velocity, bool dontReplant, bool statusPose = false) noexcept;
	/// HandApplyToObject.cpp: HandleApplyResult(result, held, position) after the held object's apply
	/// (ApplyToObject, ApplyToMapCoord and ThrowHeld packets): 0x16 the hand lets it go (RemoveFirstFromHand), 0x17 it is
	/// put at the position, 0x18 only the status's hand lets it go; returns the result, 0 for 5 and a 0x16 / 0x17 that
	/// did not apply
	int HandleApplyResult(int result, entt::entity held, std::optional<glm::vec3> position) noexcept;
	/// HandTurn.cpp: the render hand's release countdown, each frame: 180 ms after it let its object go, the
	/// ReleaseImpulse packet with the hand's move since (_heldGroundPoint - _requiredHandPosition)
	void UpdateReleaseImpulse() noexcept;
	void UpdateHeldObject() noexcept;
	/// HandResources.cpp: one game turn of scooping from a pile
	bool ProcessInInteractPile() noexcept;
	/// HandTrees.cpp: a forest's InterfaceSetInMagicHand without its nested PlaceObjectInMagicHand: the wood taken and
	/// the new Conifer at the forest
	entt::entity CreateTreeFromForest(entt::entity forest) noexcept;
	/// HandFish.cpp: the splash of gripping the water (StartLandscapeGrip)
	void SplashHand(glm::vec3 point) noexcept;
	/// HandFish.cpp: the sound of gripping the land (StartLandscapeGrip, G_HandGrabLand_01..06)
	void GripLandSound(glm::vec3 point) noexcept;
	/// HandFish.cpp: the action over the water next to a fish starts catching from its farm (FishFarm locked select)
	bool TryPickUpFish(entt::entity farm, glm::vec3 point) noexcept;
	/// HandFish.cpp: FishFarm::ProcessInInteract per game turn; false if the source is not a fish farm
	bool ProcessInInteractFish() noexcept;
	/// HandFish.cpp: the action on a field starts taking its food (its network-friendly start)
	bool TryPickUpField(entt::entity field, std::optional<glm::vec3> point = std::nullopt) noexcept;
	/// HandFish.cpp: a field is valid for a locked select (growth > 0 and food > 1)
	[[nodiscard]] static bool FieldValidForLockedSelect(entt::entity field) noexcept;
	/// HandFish.cpp: FindObjectNearMapCoord's water branch: the FishFarm of a shown fish near the point
	[[nodiscard]] static std::optional<entt::entity> FishFarmUnderHand(glm::vec3 point) noexcept;
	/// HandFish.cpp: a field's ProcessInInteract per game turn; false if the source is not a field
	bool ProcessInInteractField() noexcept;
	/// Test hook OPENBLACK_TEST_SPLASH="x,z": a hand splash there every second
	void UpdateTestSplash(float seconds) noexcept;
	/// Pot::AddResourceToPos: a hand pot put down merges into a same-type pile or store nearby, else a new pile.
	void PutDownHandPot(entt::entity pot) noexcept;
	/// The same, put down by the interface given (none: no player for a new pile, no creature deed and no drop sound)
	void PutDownHandPot(entt::entity pot, const pot_resource::Dropper& dropper) noexcept;
	/// PileResource draw: a pile sinks into the ground as it empties (proportion of its maximum).
	void SinkPile(entt::entity pile) noexcept;
	[[nodiscard]] static PotInfo PotInfoOf(entt::entity entity) noexcept;
	[[nodiscard]] float HeldFill() const noexcept;
	/// A tree put down upright on land is planted again; the mimicry and the alignment are player's, none without one
	void Replant(entt::entity tree, std::optional<PlayerNames> player) noexcept;
	/// Tree -> DeadTree, made for `player` (the thrower's), which tells the villagers there is wood with that player. With
	/// placeLying it is laid on the ground towards direction; a physics body keeps its pose.
	void MakeDeadTree(entt::entity tree, glm::vec3 direction, bool placeLying = true,
	                  PlayerNames player = PlayerNames::NEUTRAL) noexcept;
	/// The hand's part of the physics system: the hand's hooks of ecs::physics::from_hand and the physics constants
	/// (the trees' and pots' physics are called by ecs::PhysicsGameHooks).
	void RegisterPhysicsHandlers() noexcept;
	/// The tap handlers (ecs::hand_tap) of the classes whose owners have not registered them yet: rocks, abodes, spell
	/// icons and one-shot orbs, through their public APIs
	void RegisterTapHandlers() noexcept;
	/// HandTurn.cpp: the hand's packet handlers (game_packets::SetHandler)
	void RegisterPacketHandlers() noexcept;
	/// HandTurn.cpp: the release state's ThrowData packet (the throw) and ApplyToMapCoord packet (DropOnMapCoord ->
	/// SendApplyToMapCoord)
	void SendRelease(glm::vec3 velocity) noexcept;
	/// HandSpellSeed.cpp: the ThrowHeld packet's handler (the held object's ThrowObjectFromHand(status, 1))
	void ApplyForceDropHeld() noexcept;
	/// HandTurn.cpp: the Tap packet's handler (InterfaceValidToTap again, InterfaceTap)
	void ApplyTap(entt::entity object) noexcept;
	/// HandTurn.cpp: the first part of the interface's processing: the HandAndCamera / Hand / Camera packets when the
	/// hand or the camera moved, with a throttle
	void SendHandSync() noexcept;
	/// HandTurn.cpp: the first step of the interface status's processing: the synced hand's motion this turn
	void UpdateTurnMovement() noexcept;
	/// HandTurn.cpp: a spell of the list needs continual packets
	[[nodiscard]] static bool HandCastNeedsContinualPackets() noexcept;
	/// The status's map point as a world point
	[[nodiscard]] glm::vec3 SyncMapPoint() const noexcept;
	/// HandSpellSeed.cpp: the hand's live position and the holding velocity, what a seed's ThrowData packet sends
	void LiveHandThrowData(glm::vec3& handPos, glm::vec3& velocity) const noexcept;
	/// HandSpellSeed.cpp: a seed sender's ThrowData packet, the render hand's throw block as it is
	void PushSeedThrowData(glm::vec3 velocity, glm::vec3 handPos) noexcept;
	/// The interface status's synced block (the local player's)
	map_coords::MapCoords _syncMapCoords {};      ///< From HandAndCamera / Hand
	glm::vec3 _syncHand {0.0f};                   ///< From HandAndCamera / Hand
	glm::vec3 _syncCameraPosition {0.0f};         ///< From HandAndCamera / Camera
	glm::vec3 _syncCameraFocus {0.0f};            ///< From HandAndCamera / Camera
	glm::vec3 _turnHand {0.0f};                   ///< The synced hand of this turn (UpdateTurnMovement's end)
	glm::vec3 _turnDelta {0.0f};
	glm::vec3 _stillMotion {0.0f};
	uint32_t _stillTurns {0};
	float _turnSpeed {0.0f};
	float _turnHeading {0.0f};
	float _turnRate {0.0f};
	glm::vec3 _turnVelocity {0.0f};
	float _turnSideAcceleration {0.0f};
	/// SendHandSync's state: what was sent last and the throttle's countdown
	map_coords::MapCoords _sentMapCoords {};
	glm::vec3 _sentHand {0.0f};
	glm::vec3 _sentCameraFocus {0.0f};
	glm::vec3 _sentCameraPosition {0.0f};
	uint8_t _syncCountdown {0};
	/// The action collide's MapCoords the hand sync compares
	map_coords::MapCoords _actionCoords {};
	/// HandTurn.cpp: a locked select starts: the StartLockedSelect packet and action state 3
	void SendStartLockedSelect(entt::entity object) noexcept;
	/// HandTurn.cpp: action state 3's end: the EndLockedSelect packet, and the multi pickup stops at once
	void SendEndLockedSelect() noexcept;
	/// HandTurn.cpp: the StartLockedSelect packet's handler (the object's network-friendly start)
	void ApplyStartLockedSelect(entt::entity object) noexcept;
	/// HandTurn.cpp: the EndLockedSelect packet's handler (the object's network-friendly end)
	void ApplyEndLockedSelect(entt::entity object) noexcept;
	/// HandTurn.cpp: the interface status's locked select: the influence and the source's per-turn processing
	void ProcessLockedSelect() noexcept;
	/// HandTurn.cpp: what the hand keeps of its player's influence past the border follows the synced place
	void ProcessVirtualInfluence() const noexcept;
	/// HandTurn.cpp: a deleted held object leaves the hand
	void ValidateHands() noexcept;
	/// HandTurn.cpp: the held object, deleted, leaves the hand with its out-of-hand calls
	void TakeDeletedOutOfHand() noexcept;
	/// HandTurn.cpp: the original's number of the action's state (ecs::hand_validate), from openblack's own fields
	[[nodiscard]] int32_t ActionStateNumber() const noexcept;
	/// HandTurn.cpp: the map's clear's check of the action (ecs::hand_validate::Validate) with every thing of the last
	/// land gone: an action that needs its objects ends, and an interface active under a script's wide screen is made
	/// inactive
	void ValidateAtClearMap() noexcept;
	/// The object of the action state 3 (the locked select sent with the StartLockedSelect packet, ended with the
	/// EndLockedSelect packet)
	std::optional<entt::entity> _lockedSelectAction;
	/// The multi pickup stopped at the interface: the particles and the looping sound stop while the
	/// EndLockedSelect packet waits for its turn
	bool _lockedSelectStopped {false};
	/// The creature the hand pressed on through a locked select, and how far that is (ECS/HandCreature.h)
	std::optional<entt::entity> _creatureLock;
	hand_creature::LockState _creatureLockState;
	/// The start packet locked the creature for the hand (the object's locked-select flag)
	bool _creatureLocked {false};
	/// The interface's own flag that a creature is locked for its hand: set by a creature's start applied, cleared only
	/// by a creature's end applied, so it stays set after a locked creature is gone (hand_creature::StepCreatureLockFlag)
	bool _statusCreatureLocked {false};
	/// The interface's locked object: set by the StartLockedSelect handler once it takes the object (whatever its kind),
	/// cleared by the EndLockedSelect handler for any object, by the turn's pass when the object is gone or a pick-up ends,
	/// and by the reset
	std::optional<entt::entity> _statusLockedObject;
	/// The locked creature is still finishing the body action it was doing when the hand took hold of it: the CREATURE
	/// state waits for it (hand_creature::StillFinishing)
	bool _creatureFinishing {false};
	/// The render hand's creature (the CREATURE state's), and the camera ms since the hand began holding it
	std::optional<entt::entity> _creatureInHand;
	hand_creature::Interaction _creatureInteraction;
	/// Where the creature hand placed the hand this frame, in the CREATURE state
	std::optional<CreatureHandSystemInterface::HandPose> _creaturePose;
	/// HandTurn.cpp: whether an empty hand's press on the object takes hold of a creature (creature_hand::TakesPress)
	[[nodiscard]] bool CreatureTakesPress(entt::entity object) const noexcept;
	/// HandTurn.cpp: the creature's locked select, every frame: held once locked, let go at the release, then ended
	void UpdateCreatureLock(bool actionReleased) noexcept;
	/// HandTurn.cpp: the render hand with a creature, every frame (hand_creature::StepFrame): its time with it, the
	/// click and the feedback packets; returns the render hand's state from this frame
	[[nodiscard]] int32_t UpdateCreatureFrame(uint32_t cameraMs) noexcept;
	/// HandTurn.cpp: the render hand leaves the CREATURE state: how the creature was treated, the feedback packet
	void LeaveCreatureState() noexcept;
	/// HandSpellSeed.cpp: the ApplyToMapCoord packet's handler for a seed (the seed's ApplyThisToMapCoord)
	void ApplySeedToMapCoord(const game_packets::Packet& packet) noexcept;
	/// HandSpellSeed.cpp: the ApplyToObject packet's handler for a seed (the seed's ApplyThisToObject)
	void ApplySeedToObject(const game_packets::Packet& packet) noexcept;
	/// The throw velocity the ThrowData packet set
	glm::vec3 _statusThrowVelocity {0.0f};
	/// The position the ThrowData packet set (the held object's at the release, the hand's for a seed)
	glm::vec3 _statusThrowHandPosition {0.0f};
	/// The throw's angular velocity, the angular momentum the ThrowData packet set, always in this port's sign
	/// (PredictRelease's L; a seed's raw h is sent negated, the original's sign being the opposite)
	glm::vec3 _statusThrowAngular {0.0f};
	/// HandAngles (x, y, z), the DecomposeYXZ angles the ThrowData packet set
	glm::vec3 _statusThrowAngles {0.0f};
	/// The hand's angular velocity h (read by the release state and the apply senders). Its writers: 0 at
	/// construction, 0 every frame while the Holding spring works, the grain state's update in state 8; (not ported) the
	/// replay's restore
	glm::vec3 _handAngularVelocity {0.0f};
	/// The grain state's smoothers (state 8, a held spell seed): the smoothed velocity (magic::FilterHandVelocity) and
	/// the spin with the last displacement and the smoothed sideways acceleration (magic::HandSpin); reset when the
	/// state is entered
	glm::vec3 _grainVelocity {0.0f};
	magic::HandSpin _grainSpin;
	bool _grainEntered {false};
	/// HandHolding.cpp: the throw block's per-frame writers (W2 / W3), at the end of Place with the hand's position
	/// before and after this frame's move (the grain update's old, then the Holding move, then new)
	void UpdateThrowBlock(glm::vec3 oldHand, glm::vec3 newHand) noexcept;
	/// SendHandSync's ping ring: the tick count at each own HandAndCamera / Hand / Camera send (32 entries); when an
	/// own sync packet comes back, now - ring[index] is the local interface's ping, 0 until the first one. The release
	/// state reads it
	std::array<uint32_t, 32> _pingRing {};
	uint8_t _pingIndex {0};
	uint32_t _pingMs {0};
	/// SendHandSync and SendApplyToMapCoord: the ring entry of a sync packet
	[[nodiscard]] int32_t NextPingIndex() noexcept;
	/// The ping of an own sync packet as it is processed
	void ReceivePing(int32_t index) noexcept;
	/// HandTurn.cpp: the ThrowData packet of a release (the release state) or of ForceDropHeld: the velocity, and the
	/// held object's position and rotation at the send
	void PushThrowData(glm::vec3 velocity, bool predict) noexcept;
	/// HandTurn.cpp: IsInteractable (IsAvailable; an abode: built > 0)
	[[nodiscard]] static bool Interactable(entt::entity object) noexcept;
	/// HandToolTips.cpp: the interface's hand state for the hand's state now, and whether it holds the camera's red
	/// cross back for a while (an object the hand may act on, a screen bubble, a lock starting or ending)
	struct InterfaceHand
	{
		int32_t state {3};
		bool holdsCrossBack {false};
	};
	[[nodiscard]] InterfaceHand InterfaceHandState() const noexcept;
	/// HandToolTips.cpp: the tooltip of the hand state, once per turn
	void SubmitToolTips() noexcept;
	/// The object the tooltips are about this turn: the one under the hand, else the one under the cursor
	[[nodiscard]] entt::entity ToolTipObject() const noexcept;
	/// The numbers the tooltip builder reads again every turn for a town's or a storage pit's text, from this turn's
	/// object (help::tooltips::SetObjectNumbers)
	[[nodiscard]] std::optional<help::tooltips::Numbers> ToolTipNumbers(uint32_t text) const noexcept;
	/// HandToolTips.cpp: the land tooltips, the leash's first, with the object under the hand (entt::null for none)
	void SubmitLandToolTips(entt::entity object) const noexcept;
	/// HandApplyToObject.cpp: the held object's ValidToApplyThisToObject on the target
	[[nodiscard]] bool HeldValidToApplyTo(entt::entity target) const noexcept;
	/// The interface's in-influence flag: the action position inside the player's influence
	[[nodiscard]] bool InInfluence() const noexcept;
	/// Whether an object may be placed in the hand, by its class: an allowlist (hand_grab::PlaceInHandRuleOf), so
	/// buildings, totems, features, creatures and everything not named never go in
	[[nodiscard]] bool ValidForPlaceInHand(entt::entity object) const noexcept;
	/// A tap on a thing: it is remembered as the last thing tapped (but a reward under the untied leash), then the leash
	/// may take the tap (the creature sent to act on the thing); otherwise SendTap. True when the tap was sent or the
	/// leash took it
	bool Tap(entt::entity object) noexcept;
	/// SendTap: in the influence (or not InterfaceMustBeInInfluenceForInteraction), the class valid to tap and not
	/// IsCannotBePickedUp -> the Tap packet -> ApplyTap -> InterfaceTap. True when it tapped.
	bool SendTap(entt::entity object) noexcept;
	/// A press on the land with nothing to take: the leash may send the creature to act on the point
	void TapLand(const glm::vec3& point) noexcept;
	/// The player's leash is on and not tied to anything
	[[nodiscard]] bool LeashIsOnUntied() const noexcept;
	/// The Action button's double click in the idle hand: the leash's tie, untie or a seed's tap on what the hand
	/// points at (ecs::leash_tie)
	void LeashDoubleClick() noexcept;
	/// DeleteObjectAndTakeResource: the store takes the tree's wood and the tree is deleted.
	void DepositInStore(entt::entity object, entt::entity store, const pot_resource::Dropper& is) noexcept;
	[[nodiscard]] bool IsHoldingTree() const noexcept;

	// ---- HandSpellSeed.cpp: a spell seed in the hand (the interface's apply states, the seed's interface calls) ----
	/// The interface's action states with a seed: 8/9 apply on release (to the land / an object), 10/11 locked apply
	enum class SeedAction : uint8_t
	{
		None = 0,
		ApplyOnReleaseMap = 8,
		ApplyOnReleaseObject = 9,
		LockedApplyMap = 10,
		LockedApplyObject = 11,
	};
	[[nodiscard]] bool IsHoldingSeed() const noexcept;
	/// ActionPressedHolding, the seed branches: the branch it took
	hand_seed::Step SeedActionPressed() noexcept;
	/// States 8..11 every frame: release applies / unlocks
	void UpdateSeedAction(bool actionHeld) noexcept;
	/// Whether a seed action is in progress; one whose seed has left the hand ends here
	bool SeedActionGoesOn() noexcept;
	/// The release of the action in progress: an armed seed's apply goes, a locked one is unlocked. The branch it took
	hand_seed::Step SeedActionReleased() noexcept;
	/// BeginApplyOnRelease / EndApplyOnRelease: the buffer reseeded, the G_HANDGESTURE_02 loop
	void BeginApplyOnRelease(SeedAction state) noexcept;
	void EndApplyOnRelease() noexcept;
	/// SendApplyToMapCoord (the seed part) -> the ApplyToMapCoord packet -> the seed's ApplyThisToMapCoord. Whether it
	/// was sent, failed, waits for an earlier one or did nothing
	hand_seed::Step SendSeedApplyToMapCoord() noexcept;
	/// SendApplyToObject (the seed part) -> the ApplyToObject packet -> the seed's ApplyThisToObject, with the same
	/// outcomes
	hand_seed::Step SendSeedApplyToObject() noexcept;
	/// FailApply: SPOT_VISUAL 4 (SF_FailedApply) at the point and G_SpellCastFailure
	int FailApply(glm::vec3 point) noexcept;
	/// HandleApplyResult: 0x16 -> the seed leaves the hand, 3 -> consumed
	void HandleSeedApplyResult(int result, entt::entity seed) noexcept;
	/// Every frame: the hold parameters (MAGIC until ready), the seed's own mesh (IsG3DObjectDrawnInHand), its coming in
	/// and out of the hand, and what the gesture system is told (magic::gestures::SetHandStatus)
	void UpdateSeedInHand(bool actionHeld) noexcept;
	/// The seed's InterfaceSetOutMagicHand; the hand stops drawing it (out of the hand, a seed that follows its spell is
	/// drawn by magic::seed::DrawSpells)
	void SeedLeftHand(entt::entity seed) noexcept;
	/// The locked apply's unlock, sent as a packet after the applies already sent
	void SendSeedUnlock(entt::entity seed) noexcept;
	/// Test hooks OPENBLACK_TEST_CAST / OPENBLACK_TEST_CAST_PATH / OPENBLACK_TEST_THROW_VEL (HandSpellSeed.cpp)
	bool TestCastActionHeld(float seconds, bool actionHeld) noexcept;
	[[nodiscard]] std::optional<glm::vec3> TestCastPathPoint() const noexcept;

	// ---- HandApplyToObject.cpp: the held object applied to the object under the hand, and seeds / stones picked up ----
	/// The seed an object out of the hand gives the hand when it is ValidForPlaceInHand: a spell seed itself, a
	/// teleport stone its spell's seed; entt::null otherwise
	[[nodiscard]] static entt::entity SeedToPlaceInHand(entt::entity object) noexcept;
	/// The generic pick-up (the 225 ms grab) of a seed or a stone: the checks, sound 10 and the PlaceInHand packet with
	/// the object (a stone's seed is found in the handler). False if the object is neither.
	bool SendSeedOrStonePickup(entt::entity object, bool inInfluence) noexcept;
	/// ActionPressedHolding for a held object that is not a seed: when the object under the hand takes it
	/// (ValidToApplyThisToObject, with the influence rule), SendApplyToObject -> the ApplyToObject packet ->
	/// ApplyHeldToObject -> ApplyThisToObject -> HandleApplyResult. False: nothing applied (the press arms the
	/// put down / throw as before)
	bool HeldActionPressedOnObject(bool inInfluence) noexcept;
	/// HandApplyToObject.cpp: the ApplyToObject packet's handler (the checks again, ApplyThisToObject and
	/// HandleApplyResult)
	void ApplyHeldToObject(entt::entity target) noexcept;
	/// RemoveFirstFromHand: the held object leaves the hand without physics (the magic hand's RemoveFromHand:
	/// fire::SetOutMagicHand), and the local player's interface keeps it as the last thing it let go of; the caller
	/// places it
	std::optional<entt::entity> RemoveFirstFromHand() noexcept;

	SeedAction _seedAction {SeedAction::None};
	/// The action state the power-up pass's apply of a fire seed to a fireball leaves: no press is taken while it lasts;
	/// a release, or the action's end (the ball gone at the turn), ends it
	bool _applyObjectState {false};
	/// The turn the locked apply started; its unlock packet carries the turns since
	uint32_t _seedLockTurn {0};
	/// The turns the last unlocked apply was held, as its unlock packet gave them (nothing reads them yet)
	int32_t _unlockTurnsHeld {0};
	/// the object under the cursor when the apply started (the action collide's object)
	entt::entity _seedTarget {entt::null};
	/// The turn an apply packet (ApplyToObject / ApplyToMapCoord) was sent, 0 when none waits; no other is sent while it
	/// is set
	uint32_t _applySentTurn {0};
	/// The turn the ApplyToObject / ApplyToMapCoord handler ran. Both go back to 0 in SendHandSync once the sent turn
	/// <= the handled turn, and in EndAction
	uint32_t _applyHandledTurn {0};
	/// the seed the hand held last frame (to see it come and go)
	entt::entity _seedInHand {entt::null};
	float _testCastTime {-1.0f};
	/// What GetHoldType returns for each class (the hold pose's functions take it: magic::hand_hold)
	using HoldType = openblack::HoldType;
	/// GetHoldType / GetHoldRadius / GetHoldLoweringMultiplier / GetHeight / IsARootedObject of an entity.
	void ComputeHoldParameters(entt::entity entity) noexcept;
	/// The tug state: a grounded tree is pulled until the hand is weight / 1000 away from the grab point.
	void UpdateTug(float seconds, bool actionHeld) noexcept;
	void Uproot(entt::entity tree) noexcept;
	/// Centre of the closed side grip (between palm and fingertips) in hand model space, for the current pose.
	[[nodiscard]] glm::vec3 GripCentre() const noexcept;
	/// The holding state's mouse sway: the held object's up turned about the view and side axes (magic::hand_hold)
	[[nodiscard]] glm::vec3 HeldSway(glm::vec3 at, float extraRoll = 0.0f) const noexcept;
	/// A tree out of the map draws MSH_T_ROOTS at the tree matrix.
	void UpdateRoots(entt::entity tree, bool dying = false) noexcept;
	void DropRoots(entt::entity tree, bool fall) noexcept;
	void UpdateRootsAndPiles(float seconds) noexcept;
	/// Environment-variable test hooks (HandDebugHooks.cpp), run once when the landscape exists.
	void RunDebugHooks() noexcept;
	void UpdateTestAbode(float seconds) noexcept;
	struct TestAbode
	{
		entt::entity abode;
		float speed;
		float scale;
		int count;
		float timer;
	};
	std::optional<TestAbode> _testAbode;
	float _testActionDelay {0.0f};
	float _testActionHold {0.0f};
	/// OPENBLACK_HAND_TEST_DROP: where (x, z) and in how many seconds the held object is put down
	std::optional<glm::vec3> _testDropAt;
	float _testDropIn {0.0f};
	float _testMouseMoveIn {-1.0f};
	void UpdatePickupParticles(float seconds, bool emitting) noexcept;
	[[nodiscard]] glm::vec3 ModelPosition(size_t vertex, const std::vector<glm::mat4>& bones) const noexcept;
	/// HandNearObject.cpp: the land behind the hand seen from the camera (found by the landscape draw)
	void UpdatePointBehindHand() noexcept;
	/// HandNearObject.cpp: FindObjectNearMapCoord for the action's point `at`
	[[nodiscard]] std::optional<entt::entity> FindObjectNearMapCoord(glm::vec3 at) const noexcept;
	/// HandMorph.cpp: the morph's base load: Base2, Evil2 and Good2 on the CPU
	void LoadMorphMeshes() noexcept;
	/// HandMorph.cpp: the morphed texture (a 4444 blend) into _morphTexels
	void MorphTexture(float a) noexcept;
	/// HandMorph.cpp: the morphed vertices into _morphVertices
	void MorphVertices() noexcept;
	/// HandMorph.cpp: the morph's only GPU upload (the hand mesh's skin and sub-meshes)
	void UploadMorph(bool texture, bool vertices) noexcept;
	/// HandFrame.cpp: the hand's heading H and its facing d taken again from the mouse ray, before the hand is drawn in
	/// any state
	void UpdateHeading() noexcept;
	/// HandFrame.cpp: the hand matrix's rotation (d from the mouse ray, side = d x up; X side, Y side x up, Z -up)
	[[nodiscard]] glm::mat3 HandMatrixRotation(glm::vec3 up) noexcept;
	/// HandFrame.cpp: the empty hand's up (the land normal, three 0.4 s Zoomers)
	glm::vec3 UpdateNormalUp(float seconds) noexcept;

	std::array<entt::entity, 2> _hands;
	std::unique_ptr<HandAnimator> _animator;
	std::string _override;
	/// The play-anim state: the flag StartFixedPosAnimation sets, its clip and point, and the ms since it was entered
	bool _playAnim {false};
	std::string _playAnimClip;
	glm::vec3 _playAnimPoint {0.0f};
	uint32_t _playAnimMs {0};
	// Smoothed directional motion for the L*_lr / L*_fb layers (-1..+1).
	glm::vec2 _motionTarget {0.0f};
	glm::vec2 _motion {0.0f};
	float _motionAge {1.0f};

	// Hand geometry from Hand_Boned_Base2.l3d (vertices live in the space of their bone).
	std::vector<glm::vec3> _vertices;
	std::vector<uint32_t> _vertexBones;
	glm::vec3 _handHeadingBack {0.0f, 0.0f, 1.0f}; ///< the level facing towards the camera, for the held object
	float _handHeading {0.0f};                     ///< the heading H, the mouse ray's yaw (UpdateHeading)
	/// HOLDING / GRAIN: the held object's up' (the sway, R1 R2) for its own matrix; nullopt in the other states (the
	/// held object then takes the hand's)
	std::optional<glm::vec3> _heldUp;
	openblack::Zoomer3 _up; ///< The up Zoomers
	glm::vec3 _normalUp {0.0f, 1.0f, 0.0f};
	float _upMouseX {-1e30f}; ///< the mouse's x when the up last took a target
	bool _inNormalState {false};
	/// The rest of the preparation for drawing is skipped: the INVISIBLE state (which also hides: NotDrawn, the render
	/// hand hidden), a box with the mouse or the interface inactive (hand_detail::DrawPreparationStops)
	bool _preparationStopped {false};
	/// The render hand's state (HAND_STATES: 0 INVISIBLE .. 0xA CITADEL), from RequiredHandState each frame
	int32_t _renderHandState {1};
	/// The held object this system keeps from being drawn because the hand's state does not draw it (none when none)
	std::optional<entt::entity> _heldHiddenByState;
	/// The 0.13 s state blend, from the hand's place when its state changed (the last hand matrix's), and its
	/// fraction as last moved on (none when not blending this frame)
	HandCrossFade _stateBlend;
	std::optional<float> _stateBlendT;
	/// The render hand's transformed matrices (world space): the buffer drawn and the one the state blend fades from,
	/// swapped at each change
	HandPoseBuffers _handWorld;
	/// The normal state's pose as it last showed it, for the blend at its change of pose
	HandStatePose _normalPose;
	/// The creature state's animation clock, which runs on through its changes of cycle
	HandStateClock _creatureClock;
	/// HandPlacement.cpp: the state blend in world space (draw data)
	void BlendHandWorld(const glm::mat4& handModel) noexcept;
	/// HandSystem.cpp: the render hand's required state
	[[nodiscard]] int32_t RequiredHandState() const noexcept;
	float _handReach {1800.0f};
	/// HandClicked.cpp: once an interface tick before the action states
	void UpdateTapMemory(bool actionReleased) noexcept;
	/// HandClicked.cpp: what the player last clicked, kept on the hand (Initialize gives it to the hand)
	[[nodiscard]] ecs::components::HandClicked& Clicked() noexcept;
	/// HandClicked.cpp: the thing tapped is clicked now
	void RememberTapped(entt::entity object) noexcept;
	std::optional<glm::vec3> _collidePoint; ///< The action collide's position: the land or object hit
	/// This frame's land under the cursor, as the interface finds it
	std::optional<glm::vec3> _cursorLand;
	/// HandPlacement.cpp: whether an action is in progress, which keeps the action point from being cleared
	[[nodiscard]] bool ActionInProgress(bool gripping) const noexcept;
	/// HandPlacement.cpp: the position of the object the action in progress holds on to
	[[nodiscard]] std::optional<glm::vec3> ActionObjectPosition() const noexcept;
	/// HandPlacement.cpp: this frame's action point (ecs::hand_action_point), after the hand is placed
	void UpdateActionPoint(bool gripping) noexcept;
	// the good / evil morph (Morphable): meshes [1] base, [2] evil, [3] good on the CPU, the blend results
	std::unique_ptr<l3d::L3DFile> _morphBase;
	std::unique_ptr<l3d::L3DFile> _morphEvil;
	std::unique_ptr<l3d::L3DFile> _morphGood;
	std::vector<uint16_t> _morphTexels;
	std::vector<l3d::L3DVertex> _morphVertices;
	/// The alignment last taken, the one drawn (zeroed when the morph is initialised) and whether the interface's
	/// texture set is the in-influence one (so at construction)
	hand_morph::State _morph;
	std::optional<glm::vec3> _pointBehindHand; ///< Valid while the landscape draw found one
	/// Palm centre (bind pose, model space): held objects sit under it.
	glm::vec3 _palmCenter {0.0f};

	std::optional<entt::entity> _hovered;
	/// The interface's screen-object collide: the pending offer and this frame's; _screenGrip is action state 17
	/// BUBBLE GRIP (the collide frozen)
	std::optional<uint32_t> _screenOffer;
	float _screenOfferDepth {0.0f};
	std::optional<uint32_t> _screenObject;
	float _screenObjectDepth {0.0f};
	bool _screenGrip {false};
	/// The interface collide's distances of the frame's cursor hits (pending: the object's, the land's)
	std::optional<float> _cursorObjectDistance;
	std::optional<float> _cursorLandDistance;
	std::optional<entt::entity> _held;
	/// The object UpdateHeldObject gave a HandDrawPose, to take it off when it leaves the hand
	std::optional<entt::entity> _heldPosed;
	/// Object under the hand while the action button is held, in action state 13 (the grab state)
	std::optional<entt::entity> _pendingPick;
	/// The grab's start: the frame's engine sample time and the turn
	uint32_t _pendingPressMs {0};
	uint32_t _pendingPressTurn {0};
	/// The press's object is free (IN_PHYSICS) or a forest: the 225 ms timer path
	bool _pendingTimer {false};
	/// The tug-like hold of a tuggable object at rest, cleared by the tug state's update; 0 at once for a seed
	bool _pendingTugHold {false};
	/// The tug state's first frame and the state blend timer (s), from the TUG state change
	bool _pendingTugFirst {false};
	float _pendingTugBlend {0.0f};
	bool _pendingTugStarted {false};
	/// HOLD_TYPE_TREE: height of the held tree and its hold radius (0.2 * 2D radius).
	float _heldHeight {0.0f};
	float _holdRadius {0.0f};
	HoldType _holdType {HoldType::None};
	float _loweringMultiplier {0.0f};
	bool _rooted {false};
	/// The holding state: the hand follows its required position with a spring in 10 ms steps.
	bool _springActive {false};
	/// The spring's clock (game ms of every holding frame) and its 10 ms steps, the original's two counters
	uint32_t _springClockMs {0};
	uint32_t _springStepMs {0};
	glm::vec3 _springVelocity {0.0f};
	/// The render hand's mouse smoothing, in 1024-wide reference pixels: drives the held object sway.
	glm::vec2 _mouse {0.0f};
	glm::vec2 _smoothMouse {0.0f};
	glm::vec2 _smoothMouseVelocity {0.0f};
	bool _smoothMouseValid {false};
	/// The cursor's image pinned by the TOTEM state: set as it begins, moved onto the hand each drawn frame, let go as it
	/// ends
	std::optional<glm::ivec2> _cursorPin;
	/// The hand scale from its distance from the view (1 between 10 and 150 units from the camera).
	float _handScale {1.0f};
	/// The distance from the view the scale was last taken from, kept between 2 and the reach (10 before the first)
	float _viewDistance {10.0f};
	/// The camera state (the hand dragging the land): its pose from the camera's hints, the pose its placement last
	/// saw, whether it holds on to the land it gripped, how far along the cursor's line it hovers until then (eased
	/// from where it was over 0.4 s), and its animation's clock
	struct CameraHand
	{
		hand_navigation_pose::Pose pose {hand_navigation_pose::Pose::Idle};
		hand_navigation_pose::Pose placedPose {hand_navigation_pose::Pose::Idle};
		bool holdsGrip {false};
		glm::vec3 gripPlace {0.0f};
		openblack::Zoomer distance;
		uint32_t animationMs {0};
	};
	std::optional<CameraHand> _cameraHand;
	/// Entering the camera state: its pose and clock start afresh and its distance from where the hand's scale was
	/// last taken
	CameraHand& EnterCameraHand() noexcept;
	/// The camera state's pose from the camera's hints and the camera help's features
	[[nodiscard]] static hand_navigation_pose::Pose NavigationPose() noexcept;
	/// The pose the empty hand shows hovering at the edge of the screen, from the camera's hints; none: its ordinary
	/// ones
	std::optional<hand_navigation_pose::Pose> _hoverPose;
	[[nodiscard]] static std::optional<hand_navigation_pose::Pose> HoverPose() noexcept;
	/// The point the empty hand aims at with nothing under it: its view distance along the cursor's line
	glm::vec3 _cursorRayPoint {0.0f};
	/// The camera state's land under the cursor: the cursor pixel's near point and the land's distance from the view
	glm::vec3 _cursorNearPoint {0.0f};
	float _cursorLandViewDistance {0.0f};
	/// Rotation of the held object when it was picked up (the sway is applied on top of it).
	glm::mat3 _heldRotation {1.0f};
	/// The tree being pulled out of the ground, from the press
	std::optional<entt::entity> _tug;
	/// The pull at it, set at the press: the tree's matrix as it leans (its base the point it turns about), the plane the
	/// hand moves on, how far up the tree the hand grips it, how far the tree is stretched towards the hand, and the
	/// state's fade in, which the pull waits for
	struct TugPull
	{
		hand_grab::Tug tug;
		hand_grab::TugPlane plane;
		float holdDistance {0.0f};
		openblack::Zoomer stretch;
		hand_grab::TugFade fade;
	};
	std::optional<TugPull> _tugPull;
	/// Where the hand's pulling state draws the hand: at the grip of the tree as it leans and stretches; none until the
	/// state's first frame
	std::optional<glm::vec3> _tugHandPoint;
	glm::vec3 _mouseRayOrigin {0.0f};
	glm::vec3 _mouseRayDirection {0.0f, 0.0f, 1.0f};
	void BeginTug(entt::entity tree) noexcept;
	/// Roots drawn under trees out of the map (tree -> roots entity).
	std::vector<std::pair<entt::entity, entt::entity>> _roots;
	struct FallingRoots
	{
		entt::entity entity;
		float age;
		float startY;
		float groundY;
	};
	std::vector<FallingRoots> _fallingRoots;
	float _heldAltitude {0.0f};
	float _heldTop {0.0f};
	bool _actionWasHeld {false};
	/// The buttons a hand demo's mouse lights, the move button as last frame held it, and whether what a press started
	/// ended this frame (a tap sent, the leash's double click)
	hand_tricons::LitButtons _litButtons {};
	bool _litMoveWasHeld {false};
	bool _litEnded {false};
	/// The action was pressed through PressSeedAction and not yet released through ReleaseSeedAction: Update counts it
	/// as held
	bool _drivenActionHeld {false};
	/// The Action button was double clicked this frame (ActionDoubleClick), until Update takes it
	bool _actionDoubleClick {false};

	/// Resource being gathered into the hand pile (HandWood / HandFood) while the action is held over it.
	/// The locked select's object (set by the StartLockedSelect handler, cleared by the EndLockedSelect one or when it
	/// ends)
	std::optional<entt::entity> _pickSource;
	/// The player who owned what the hand last picked up: every object put in the hand records the most influential
	/// player where it was; a locked select's start then records its source's player (a pile's, the most influential at
	/// a field, a fish farm's town's owner or none). None until the first pick-up
	std::optional<PlayerNames> _sourceOwner;
	/// Game turns since the locked select started.
	uint32_t _pickTurns {0};
	/// Hand x,z frozen over the pile while scooping (the holding state, locked interact): only the hand's draw;
	/// the influence check reads the synced turn hand (_turnHand). (inferred) the live point under the hand when the
	/// StartLockedSelect packet is applied: the original's lock point was not checked
	glm::vec3 _pickLock {0.0f};
	/// The press that picked the object up is still held: its release does not drop it (state 7).
	bool _pickPressHeld {false};
	/// The locked select is catching fish (_pickSource is a FishFarm)
	bool _pickFish {false};
	/// The locked select is taking food from a field (_pickSource is a Field)
	bool _pickField {false};
	/// Test hook (OPENBLACK_HAND_TEST_FISH): seconds the action button counts as held
	float _testActionSeconds {0.0f};
	/// The multi pick-up's looping G_PICKUPFOOD / G_PICKUPWOOD and its pitch t^2
	std::optional<audio::Channel> _pickupSound;
	uint32_t _toolTipTurn {0};       ///< the game turn the tooltips were last processed
	bool _toolTipScooping {false};   ///< a locked select was scooping last turn (its end forces 0xEEA once more)
	int32_t _interfaceHandState {3}; ///< The interface's hand state (3 when it is reset)
	bool _actionHeld {false};        ///< The action button is held, as the last frame's update had it
	uint32_t _crossHoldBacks {0};    ///< How many turns' hand states held the camera's red cross back
	float _pickupSoundFraction {0.0f};
	/// HandEffects.cpp: starts / re-pitches / stops the multi pick-up loop
	void UpdatePickupSound(bool active) noexcept;
	/// A later press while holding: its release drops / throws (state 12).
	bool _releaseArmed {false};
	/// HandGiving.cpp: giving what the hand holds to a creature: the offer, the press that gives, the creature reaching
	/// for it, and how far round the offered thing is turned
	creature_giving::HandGiving _giving;
	/// What the hand held as it last offered, to start afresh with another thing
	entt::entity _givingHeld {entt::null};
	/// Whether what the hand holds can be given to a thing: none when the thing isn't a creature
	[[nodiscard]] std::optional<creature_giving::Verdict> GiveVerdict(entt::entity target, entt::entity held) const noexcept;
	/// Gives what the hand holds to the creature it offers it to, or to a creature under the hand that would take it: the
	/// Give packet; the hand keeps hold of it, no longer ready to throw, until the creature takes it. True when the press
	/// went to the give, also when an apply packet still waiting kept the Give packet back
	bool Give(std::optional<entt::entity> under) noexcept;
	/// What is under the hand that it could give to: not what it holds, and still there
	[[nodiscard]] std::optional<entt::entity> GiveUnderHand() const noexcept;
	/// Each frame: the offer is kept up or runs down
	void UpdateGiving() noexcept;
	/// In the holding state's placement: while the creature reaches, where the hand is drawn before it rises, from where
	/// it should be
	[[nodiscard]] glm::vec3 GivingTarget(glm::vec3 target, float hang) noexcept;
	/// Hand velocity (world units/s), for throwing on release.
	glm::vec3 _handVelocity {0.0f};
	std::optional<glm::vec3> _lastHeldPosition;
	float _lastDt {0.0f};

	/// SF_MultiPickUpWood / SF_MultiPickUpFood atoms (ER_MultiPickup): pieces rising from the pile into the hand.
	struct PickupParticle
	{
		entt::entity entity;
		float age;
		glm::vec3 start;
		glm::vec3 previous;
		bool mesh;
		float frame {0.0f};     ///< The atom's frame: InitFrame 0, or RandomiseInitFrame (fish)
		float frameRate {0.0f}; ///< FrameRate, negated by RandomiseFrameDirection (fish)
	};
	std::vector<PickupParticle> _pickupParticles;
	/// ER_MultiPickup collection data: atoms owed (EmitRate * time) and atoms emitted.
	float _pickupOwed {0.0f};
	uint32_t _pickupEmitted {0};

	/// The interface's action point; empty stands for the game's cleared point, the map's corner (0, 0, 0)
	std::optional<glm::vec3> _interactionPoint;
	/// The interface's action collide: the object under the cursor this frame (exact triangle pick).
	std::optional<entt::entity> _cursorObject;
	/// The interface's object as it keeps it from frame to frame: the frame's object under the cursor while no action is
	/// in progress, kept through one (the land grip, the grab, the seed's apply on release, the bubble grip), the grabbed
	/// object from the grab's start, none after the action ends
	std::optional<entt::entity> _collideObject;
	/// The kept object of the frame before, which the frame is drawn with and the camera reads
	std::optional<entt::entity> _renderCollideObject;
	struct CursorHit
	{
		entt::entity entity;
		float t;
		glm::vec3 boxMin;
		glm::vec3 boxMax;
		glm::vec3 normal {0.0f, 1.0f, 0.0f}; ///< the hit face's world normal
		/// An object picked by its mesh (by its triangles, or a tree by its pixel): the hand is not placed at t but by its
		/// own test of the mesh along the same ray (PickingSystemInterface::FeelModel, served by ecs::hand_mesh_ray),
		/// nothing when that ray crosses none of its triangles. A hit without a mesh of its own (the creature, a teleport
		/// stone) places the hand at t. Only the point and the normal of the surface are read
		bool meshTested {false};
		std::optional<screen_pick::MeshHit> surface;
	};
	/// The up target over an object the hand feels, 0.25 d - n + (0, 0.5, 0); nullopt: the land's
	std::optional<glm::vec3> _feelUp;
	/// Nearest object whose triangles the ray origin + t * direction (unit) hits; a tree by its drawn pixels instead (the
	/// first of its triangles that holds the ray's pixel where its texture is not transparent)
	[[nodiscard]] std::optional<CursorHit> PickObjectAlongRay(const glm::vec3& origin,
	                                                          const glm::vec3& direction) const noexcept;

	/// The hand's distance from the camera along the mouse ray.
	openblack::Zoomer _handDistance;
	/// Entering the normal state resets the zoomer to the current distance from view.
	bool _handDistanceValid {false};
	/// Whether the zoomer has ever started; the first time it starts at 0
	bool _handDistanceStarted {false};
	std::optional<glm::vec3> _gripPoint;
	std::optional<glm::vec3> _smoothedPosition;
};
} // namespace openblack::ecs::systems
