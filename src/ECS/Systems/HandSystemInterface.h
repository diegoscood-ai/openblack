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

	/// Advances the hand animation (hh.HBN). mouseDelta is the cursor motion in pixels since the last frame.
	/// actionHeld: the action (right) button; pressing it over an object picks it up, releasing drops it.
	virtual void Update(std::chrono::microseconds dt, glm::vec2 mouseDelta, bool gripping, bool actionHeld) noexcept = 0;
	/// Object currently held by the player hand, if any.
	[[nodiscard]] virtual std::optional<entt::entity> GetHeldObject() const noexcept = 0;
	/// Animated global bone matrices of the player hand, or nullptr when hh.HBN is not loaded.
	[[nodiscard]] virtual const std::vector<glm::mat4>* GetBoneMatrices() const noexcept = 0;
	[[nodiscard]] virtual std::vector<std::string> GetAnimationNames() const noexcept = 0;
	[[nodiscard]] virtual const std::string& GetCurrentAnimation() const noexcept = 0;
	/// Forces a C node for debugging; an empty name returns control to the gameplay state machine.
	virtual void SetAnimationOverride(const std::string& name) noexcept = 0;
};
} // namespace openblack::ecs::systems
