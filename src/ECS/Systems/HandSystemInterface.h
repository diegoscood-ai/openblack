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
	/// ToolTips::ForceToolTips(0xEEA, amount) of a locked select: the amount in the hand while taking food or wood, and
	/// for 12 turns after the last turn of it (afterFocus 0.5); nullopt when not shown
	[[nodiscard]] virtual std::optional<float> GetAmountInHandToolTip() const noexcept = 0;
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
