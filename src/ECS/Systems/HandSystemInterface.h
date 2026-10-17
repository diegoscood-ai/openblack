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
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <entt/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "Enums.h"

namespace openblack::ecs
{
struct PhysicsEntry;
struct ImpactInfo;
} // namespace openblack::ecs

namespace openblack::ecs::systems
{
/// The land (or sea) under the cursor, as the hand and the interface each find it
struct CursorLand
{
	/// The land the hand's distance from the view comes from
	std::optional<glm::vec3> hand;
	/// The land the interface weighs the objects under the cursor against
	std::optional<glm::vec3> interface;
	/// The cursor's pixel on the near plane: with no land under the cursor, a hand holding with its spring on goes its
	/// reach along the line from the eye through it
	glm::vec3 nearPoint {0.0f};
};

class HandSystemInterface
{
public:
	virtual ~HandSystemInterface() = default;

	enum class Side : uint8_t
	{
		Left,
		Right,
		_Count
	};
	virtual bool Initialize() noexcept = 0;
	[[nodiscard]] virtual std::array<entt::entity, static_cast<size_t>(Side::_Count)> GetPlayerHands() const noexcept = 0;
	[[nodiscard]] virtual std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetPlayerHandPositions() const noexcept = 0;
	/// Each frame the hands catch up with their player's alignment once it has moved far enough, and blend their skin
	/// again as they go into or out of the player's influence. The point picked under the cursor and whether it is held
	/// (see hand_morph::NextPoint) are not read yet: the influence is tested at the hand's interaction point.
	virtual void UpdateAlignmentMorph(std::optional<map_coords::MapCoords> picked, bool holdPoint) = 0;

	/// Places the player hand for this frame.
	/// @param groundPoint point of the landscape under the cursor (nullopt when the cursor is in the sky).
	/// @param cameraForward camera view direction; the fingers point along it projected on the ground.
	/// @param gripping true while the land is gripped: the fingertips stay dug into the grab point.
	virtual void Place(std::optional<glm::vec3> groundPoint, glm::vec3 cameraForward, bool gripping,
	                   std::chrono::microseconds dt) noexcept = 0;

	/// Picks the object under the cursor ray and returns the point on the mouse ray where the hand goes this frame, at
	/// the distance of the surface under the cursor smoothed by the hand distance Zoomer. land is the landscape (or sea)
	/// point under the cursor, the hand's and the interface's. gripping: the land is held (camera drag), only the
	/// landscape counts.
	[[nodiscard]] virtual std::optional<glm::vec3> ResolveCursorPoint(const glm::vec3& origin, const glm::vec3& direction,
	                                                                  CursorLand land, bool gripping,
	                                                                  std::chrono::microseconds dt) noexcept = 0;
	/// The hand's state change, first in the frame's hand: the required state, the old state's exit, the new one's
	/// enter (the hide of the invisible state) and the start of the state blend. Called before ResolveCursorPoint and
	/// the interface's frame pass, which then see this frame's state
	virtual void PrepareState() noexcept = 0;
	/// Advances the hand animation (hh.HBN). mouseDelta is the cursor motion in pixels since the last frame.
	/// actionHeld: the action (right) button; pressing it over an object picks it up, releasing drops it.
	virtual void Update(std::chrono::microseconds dt, glm::vec2 mouseDelta, bool gripping, bool actionHeld) noexcept = 0;
	/// The Action button was double clicked this frame; the next Update takes it, in the idle hand in place of the
	/// action press (the leash's tie)
	virtual void ActionDoubleClick() noexcept = 0;
	/// The action pressed with a spell seed in the hand, as the button's press does it: armed until the release, cast
	/// at once, kept casting while held, or failed. Until ReleaseSeedAction the hand's Update counts the action as held.
	/// None when nothing happened, or with no seed in the hand
	[[nodiscard]] virtual MagicSystemInterface::HandResult PressSeedAction() noexcept = 0;
	/// The action released with a spell seed in the hand, as the button's release does it: an armed seed is cast, one
	/// cast while held is let go. None when nothing happened
	[[nodiscard]] virtual MagicSystemInterface::HandResult ReleaseSeedAction() noexcept = 0;
	/// Object currently held by the player hand, if any.
	[[nodiscard]] virtual std::optional<entt::entity> GetHeldObject() const noexcept = 0;
	/// The object under the hand as the interface keeps it for the frame: the object it kept as the hand's previous frame
	/// left it (copied as the hand's frame starts, before the cursor is looked under again), kept through an action in
	/// progress such as the land grip; none over the land, when a screen object takes the cursor, and once the object is
	/// destroyed or can no longer be interacted with. The camera's controls, which run before the hand's frame, read the
	/// object found two hand frames before
	[[nodiscard]] virtual std::optional<entt::entity> GetCursorObject() const noexcept = 0;
	/// The roots of uprooted trees falling to the ground, the newest first, as the frame's draw takes them
	[[nodiscard]] virtual std::vector<entt::entity> FallingRootsNewestFirst() const { return {}; }
	/// The hand takes the object (a spell seed from the magic code)
	virtual void PlaceObjectInMagicHand(entt::entity entity) noexcept = 0;
	/// The player who owned what the hand last picked up, recorded at the pick-up (the interface's own record, which
	/// every deposit of that interface carries); none until something records one
	[[nodiscard]] virtual std::optional<PlayerNames> GetSourceOwner() const noexcept = 0;
	/// Space in the hand (and no hand action locked)
	[[nodiscard]] virtual bool IsHandReadyForObject() const noexcept = 0;
	/// The held object leaves the hand with no velocity (packet 0x1D, thrown from the hand with no speed); a spell seed
	/// goes back to its worship site or is deleted
	virtual void ForceDropHeld() noexcept = 0;
	/// The action in progress ends (a seed that becomes ready ends its press)
	virtual void EndAction() noexcept = 0;
	// The physics of trees, dead trees and pots, which the physics class hooks call
	/// A tree or dead tree hitting a wood store turns into wood. False when it went into the store
	virtual bool TreeReactToImpact(entt::entity entity, ecs::PhysicsEntry& po) = 0;
	/// A tree's end of physics: planted again, or a DeadTree. The entity that stays
	virtual entt::entity TreeEndPhysics(entt::entity entity, ecs::PhysicsEntry& po) = 0;
	/// Every frame while a tree moves: its roots follow it
	virtual void TreeMoved(entt::entity entity) = 0;
	/// The sound of a tree put down gently on land
	virtual void TreeDropSound(entt::entity entity) = 0;
	/// A thrown pot or pile hitting a store or a pot of its type goes into it. False when it went
	virtual bool PotReactToImpact(entt::entity entity, ecs::PhysicsEntry& po, const ecs::ImpactInfo& impact) = 0;
	/// A pot's end of physics: a hand pot on land becomes a pile. The entity that stays, or entt::null. byHand: the pile
	/// is put down from the hand's interface, which its physics record names (PhysicsEntry::byPlayer: a hand threw it,
	/// directly or through what struck it); false: from no interface, for a pot knocked by what no hand threw, or one a
	/// particle system lets go of, which has no physics record to name one
	virtual entt::entity PotEndPhysics(entt::entity entity, bool byHand) = 0;
	/// A box (the escape menu, the SkipBox, a debug window) takes the input this frame: the action ends with its state's
	/// end, and the buttons the box took from the hand send nothing
	virtual void BoxTakesInput() noexcept = 0;
	/// For a spell cast from this hand: the point under the hand, the hand, the camera's forward and the hand's velocity
	virtual void GetSpellInfo(glm::vec3& interfacePos, glm::vec3& handPos, glm::vec3& cameraForward,
	                          glm::vec3& velocity) const noexcept = 0;
	/// The hand's scale (set from its distance to the view)
	[[nodiscard]] virtual float GetHandScale() const noexcept = 0;
	/// The player hand's model position and matrix
	[[nodiscard]] virtual glm::mat4 GetHandMatrix() const noexcept = 0;
	/// The hand's reach (1800 to begin with), at most 1800; SET_INTERFACE_INTERACTION sets it (75 for JUST_GRAB)
	virtual void SetHandReach(float metres) noexcept = 0;
	[[nodiscard]] virtual float GetHandReach() const noexcept = 0;
	/// The game turns the hand has stayed still: its moves since it last moved, added up, still shorter than one unit.
	/// A creature looking at its player's camera stops once the hand has stayed still too long
	[[nodiscard]] virtual uint32_t GetStillTurns() const noexcept = 0;
	/// The object tapped or clicked in the last 15 s (the hand's HandClicked); entt::null when none or it no longer
	/// exists
	[[nodiscard]] virtual entt::entity GetClickedObject() const noexcept = 0;
	/// The interface's action point: the land or the object the hand is acting on, as the last draw or the action in
	/// progress left it; none until it is first set
	[[nodiscard]] virtual std::optional<glm::vec3> GetActionPoint() const noexcept = 0;
	/// The place the interface status was synced to at the last turn
	[[nodiscard]] virtual map_coords::MapCoords GetSyncedPlace() const noexcept = 0;
	/// The interface's hand part, once a turn right after game_packets::DispatchQueuedPackets
	virtual void ProcessTurn() noexcept = 0;
	/// The hand's turn update, its hold part (step 28, magic::ProcessHandTurn): the hand lets go of an object that is
	/// no longer available, and forgets a released one that is gone
	virtual void GameTurnUpdate() noexcept = 0;
	/// The object the render hand holds, from the press until it is thrown; the status's GetHeldObject only from the
	/// 0x13. The draw's hand-held readers (shadows, reflection) ask this one
	[[nodiscard]] virtual std::optional<entt::entity> GetRenderHandObject() const noexcept = 0;
	/// A hand demo's record sets the hand's throw block (velocity, angular velocity, HandPos, angles) before its message
	virtual void SetThrowBlock(const std::array<float, 12>& block) noexcept = 0;
	/// On a new land: the synced hand, the turn motion and the throw data
	virtual void ResetTurnState() noexcept = 0;
	/// The synced hand's velocity, units per second (the landscape's hand wind reads it)
	[[nodiscard]] virtual glm::vec3 GetTurnHandVelocity() const noexcept = 0;
	/// The interface's hand state of the last turn: GET_HAND_STATE 413
	[[nodiscard]] virtual int32_t GetInterfaceHandState() const noexcept = 0;
	/// Once a turn, before the tooltips: the hand state GetInterfaceHandState (GET_HAND_STATE) and the tooltips read
	virtual void UpdateInterfaceHandState() noexcept = 0;
	/// From the «Did you know?» bubble's draw: a screen object under the mouse offers itself with its depth into the
	/// pending collide; the nearest wins (a tie keeps the first). While the hand grips one nothing changes and it
	/// returns true; else true when it was taken
	[[nodiscard]] virtual bool OfferScreenObject(uint32_t id, float depth) noexcept = 0;
	/// The screen object under the hand this frame (last frame's offer)
	[[nodiscard]] virtual std::optional<uint32_t> ScreenObjectUnderHand() const noexcept = 0;
	/// The screen object under the hand while the hand grips it (action state 17 BUBBLE GRIP, hand state 0x1D): the
	/// bubble scrolls with the mouse
	[[nodiscard]] virtual std::optional<uint32_t> DraggedScreenObject() const noexcept = 0;
	/// The interface's hover point: this frame's land under the cursor, none when the cursor finds no land (never an
	/// object)
	[[nodiscard]] virtual std::optional<glm::vec3> GetHoverLandPoint() const noexcept = 0;
	/// Objects thrown by the hand that are still in flight (the original's physics objects)
	[[nodiscard]] virtual std::vector<entt::entity> GetThrownObjects() const noexcept = 0;
	/// Animated global bone matrices of the player hand, or nullptr when hh.HBN is not loaded.
	[[nodiscard]] virtual const std::vector<glm::mat4>* GetBoneMatrices() const noexcept = 0;
	[[nodiscard]] virtual std::vector<std::string> GetAnimationNames() const noexcept = 0;
	[[nodiscard]] virtual const std::string& GetCurrentAnimation() const noexcept = 0;
	/// Forces a C node for debugging; an empty name returns control to the gameplay state machine.
	virtual void SetAnimationOverride(const std::string& name) noexcept = 0;
	/// The hand plays `clip` once, pinned upright at `point` (the play-animation state); a call
	/// during the clip moves the point and keeps the time
	virtual void StartFixedPosAnimation(const std::string& clip, glm::vec3 point) noexcept = 0;
};
} // namespace openblack::ecs::systems
