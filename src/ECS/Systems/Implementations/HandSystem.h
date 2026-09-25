/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include "3D/HandAnimator.h"
#include "ECS/Systems/HandSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class HandSystem final: public HandSystemInterface
{
public:
	bool Initialize() noexcept override;
	[[nodiscard]] std::array<entt::entity, static_cast<size_t>(Side::_Count)> GetPlayerHands() const noexcept override;
	[[nodiscard]] std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetPlayerHandPositions() const noexcept override;

	void Place(std::optional<glm::vec3> groundPoint, glm::vec3 cameraForward, bool gripping,
	           std::chrono::microseconds dt) noexcept override;
	void Update(std::chrono::microseconds dt, glm::vec2 mouseDelta, bool gripping, bool actionHeld) noexcept override;
	[[nodiscard]] std::optional<entt::entity> GetHeldObject() const noexcept override { return _held; }
	[[nodiscard]] const std::vector<glm::mat4>* GetBoneMatrices() const noexcept override;
	[[nodiscard]] std::vector<std::string> GetAnimationNames() const noexcept override;
	[[nodiscard]] const std::string& GetCurrentAnimation() const noexcept override;
	void SetAnimationOverride(const std::string& name) noexcept override;

private:
	void LoadAnimations() noexcept;
	void LoadGeometry() noexcept;
	[[nodiscard]] std::optional<entt::entity> FindObjectUnderHand() const noexcept;
	void PickUp(entt::entity entity) noexcept;
	void Drop() noexcept;
	void UpdateHeldObject() noexcept;
	void UpdateMultiPickUp(float seconds, bool actionHeld) noexcept;
	[[nodiscard]] float HeldFill() const noexcept;
	void Throw(glm::vec3 velocity) noexcept;
	void UpdateThrown(float seconds) noexcept;
	void EmitGripDust(glm::vec3 point) noexcept;
	void UpdateGripDust(float seconds) noexcept;
	[[nodiscard]] glm::vec3 ModelPosition(size_t vertex, const std::vector<glm::mat4>& bones) const noexcept;
	[[nodiscard]] glm::mat3 FrameRotation(glm::vec3 cameraForward) const noexcept;

	std::array<entt::entity, 2> _hands;
	std::unique_ptr<HandAnimator> _animator;
	std::string _override;
	// Smoothed directional motion for the L*_lr / L*_fb layers (-1..+1).
	glm::vec2 _motionTarget {0.0f};
	glm::vec2 _motion {0.0f};
	float _motionAge {1.0f};

	// Hand geometry from Hand_Boned_Base2.l3d (vertices live in the space of their bone).
	std::vector<glm::vec3> _vertices;
	std::vector<uint32_t> _vertexBones;
	/// Local frame measured on the mesh: palm -> fingers, palm normal, lateral (thumb side).
	glm::vec3 _frameFingers {0.0f, 1.0f, 0.0f};
	glm::vec3 _frameNormal {0.0f, 0.0f, 1.0f};
	glm::vec3 _frameLateral {1.0f, 0.0f, 0.0f};
	/// Index fingertip in the pointing pose (Ccan_pickup), model space: the interaction point.
	glm::vec3 _hotspot {0.0f};
	/// Front-most vertex of each fingertip: dug into the ground while gripping.
	std::vector<size_t> _tipVertices;
	/// Palm centre (bind pose, model space): held objects sit under it.
	glm::vec3 _palmCenter {0.0f};

	std::optional<entt::entity> _hovered;
	std::optional<entt::entity> _held;
	float _heldAltitude {0.0f};
	float _heldTop {0.0f};
	bool _actionWasHeld {false};

	/// Resource being gathered into the hand pile (HandWood / HandFood) while the action is held over it.
	std::optional<entt::entity> _pickSource;
	float _pickTime {0.0f};
	float _pickTurnAccumulator {0.0f};
	/// Hand velocity (world units/s), for throwing on release.
	glm::vec3 _handVelocity {0.0f};
	std::optional<glm::vec3> _lastHeldPosition;
	struct Thrown
	{
		entt::entity entity;
		glm::vec3 velocity;
		float altitude;
	};
	std::vector<Thrown> _thrown;
	float _lastDt {0.0f};

	/// SF_GripLandscape particles (dust thrown up when the land is gripped).
	struct DustParticle
	{
		entt::entity entity;
		float age;
		uint32_t initialFrame;
	};
	std::vector<DustParticle> _dust;
	uint32_t _dustSeed {1};

	std::optional<glm::vec3> _interactionPoint;
	std::optional<glm::vec3> _gripPoint;
	glm::mat3 _gripRotation {1.0f};
	std::optional<glm::vec3> _smoothedPosition;
	float _tipClearance {0.45f};
	float _vertexClearance {0.05f};
	float _clawDepth {0.12f};
};
} // namespace openblack::ecs::systems
