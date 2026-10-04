/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>

#include <chrono>
#include <string>
#include <vector>

#include <entt/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::systems
{
class HandSystemInterface
{
public:
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

	/// Places the player hand for this frame.
	/// @param groundPoint point of the landscape under the cursor (nullopt when the cursor is in the sky).
	/// @param cameraForward camera view direction; the fingers point along it projected on the ground.
	/// @param gripping true while the land is gripped: the fingertips stay dug into the grab point.
	virtual void Place(std::optional<glm::vec3> groundPoint, glm::vec3 cameraForward, bool gripping,
	                   std::chrono::microseconds dt) noexcept = 0;

	/// ObtainRequiredHandPosition (0x5B5E70): picks the object under the cursor ray (GInterface::SendObjectDrawCollision /
	/// UpdateInterfaceCollide) and returns the point on the mouse ray where the hand goes this frame, at the distance of
	/// the surface under the cursor smoothed by the hand distance Zoomer. land is the landscape (or sea) point under the
	/// cursor. gripping: the land is held (camera drag), only the landscape counts.
	virtual std::optional<glm::vec3> ResolveCursorPoint(const glm::vec3& origin, const glm::vec3& direction,
	                                                    std::optional<glm::vec3> land, bool gripping,
	                                                    std::chrono::microseconds dt) noexcept = 0;
	/// Advances the hand animation (hh.HBN). mouseDelta is the cursor motion in pixels since the last frame.
	/// actionHeld: the action (right) button; pressing it over an object picks it up, releasing drops it.
	virtual void Update(std::chrono::microseconds dt, glm::vec2 mouseDelta, bool gripping, bool actionHeld) noexcept = 0;
	/// Object currently held by the player hand, if any.
	[[nodiscard]] virtual std::optional<entt::entity> GetHeldObject() const noexcept = 0;
	/// GInterfaceStatus::PlaceObjectInMagicHand 0x5DC870: the hand takes the object (a spell seed from the magic code)
	virtual void PlaceObjectInMagicHand(entt::entity entity) noexcept = 0;
	/// GInterfaceStatus::IsHandReadyForObject 0x5DC890: space in the hand (and no hand action locked)
	[[nodiscard]] virtual bool IsHandReadyForObject() const noexcept = 0;
	/// GInterface::ForceDropHeld 0x5D4350: the held object leaves the hand with no velocity (packet 0x1D,
	/// ThrowObjectFromHand(status, 1)); a spell seed goes back to its worship site or is deleted
	virtual void ForceDropHeld() noexcept = 0;
	/// GInterface fn_005D1260 (EndAction): the action in progress ends (a seed that becomes ready ends its press)
	virtual void EndAction() noexcept = 0;
	/// GInterfaceStatus::UpdateSpellInfo 0x5DC8F0 (for a spell cast from this hand): the point under the hand (+0x00),
	/// the hand (+0x0C), the camera's forward (+0x18) and the hand's velocity (+0x24)
	virtual void GetSpellInfo(glm::vec3& interfacePos, glm::vec3& handPos, glm::vec3& cameraForward,
	                          glm::vec3& velocity) const noexcept = 0;
	/// The hand's scale (CHand +0x4834, SetDistanceFromView)
	[[nodiscard]] virtual float GetHandScale() const noexcept = 0;
	/// The player hand's model position (CHand +0x78) and matrix
	[[nodiscard]] virtual glm::mat4 GetHandMatrix() const noexcept = 0;
	/// CHand fn_0046BF20: the hand's reach (CHand +0x4838, 1800 from the ctor 0x46BC3E), at most 1800; SET_INTERFACE_INTERACTION
	/// sets it (session Intro, the table 0x70B220: 75 for JUST_GRAB)
	virtual void SetHandReach(float metres) noexcept = 0;
	[[nodiscard]] virtual float GetHandReach() const noexcept = 0;
	/// GInterface +0x45C: the object tapped or clicked in the last 15 s (GAME_THING_CLICKED 0x70AEB0); entt::null when none
	/// or it no longer exists
	[[nodiscard]] virtual entt::entity GetClickedObject() const noexcept = 0;
	/// +0x460 / +0x464 = 0 (GameThingClicked 0x70AF84, CLEAR_CLICKED_OBJECT 0x70B0E0)
	virtual void ClearClicked() noexcept = 0;
	/// RememberTapped fn_005D36D0: the object and the current turn
	virtual void RememberTapped(entt::entity object) noexcept = 0;
	/// fn_005D0460 (POSITION_CLICKED 0x70B120): GUtils::GetDistanceInMetres(+0x46C, pos) <= radius (`test ah, 0x41`). +0x46C
	/// is the land point of the last land tap in the last 15 s, (0, 0, 0) once cleared
	[[nodiscard]] virtual bool PositionClicked(const glm::vec3& position, float radius) const noexcept = 0;
	/// CLEAR_CLICKED_POSITION 0x70B100: +0x46C / +0x470 / +0x474 = 0, the turn +0x478 stays
	virtual void ClearClickedPosition() noexcept = 0;
	/// GInterface +0x3AC, the interface's hand state (fn_005D7E40) of the last turn: GET_HAND_STATE 413 (0x6FF730)
	[[nodiscard]] virtual int32_t GetInterfaceHandState() const noexcept = 0;
	/// Objects thrown by the hand that are still in flight (the original's physics objects)
	[[nodiscard]] virtual std::vector<entt::entity> GetThrownObjects() const noexcept = 0;
	/// Animated global bone matrices of the player hand, or nullptr when hh.HBN is not loaded.
	[[nodiscard]] virtual const std::vector<glm::mat4>* GetBoneMatrices() const noexcept = 0;
	[[nodiscard]] virtual std::vector<std::string> GetAnimationNames() const noexcept = 0;
	[[nodiscard]] virtual const std::string& GetCurrentAnimation() const noexcept = 0;
	/// Forces a C node for debugging; an empty name returns control to the gameplay state machine.
	virtual void SetAnimationOverride(const std::string& name) noexcept = 0;
};
} // namespace openblack::ecs::systems
