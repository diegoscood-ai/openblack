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
#include "Common/Zoomer.h"
#include "Enums.h"
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
	std::optional<glm::vec3> ResolveCursorPoint(const glm::vec3& origin, const glm::vec3& direction,
	                                            std::optional<glm::vec3> land, bool gripping,
	                                            std::chrono::microseconds dt) noexcept override;
	[[nodiscard]] std::optional<entt::entity> GetHeldObject() const noexcept override { return _held; }
	[[nodiscard]] std::vector<entt::entity> GetThrownObjects() const noexcept override
	{
		std::vector<entt::entity> entities;
		entities.reserve(_thrown.size());
		for (const auto& thrown : _thrown)
		{
			entities.push_back(thrown.entity);
		}
		return entities;
	}
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
	/// Pot::AddResourceToPos: a hand pot put down merges into a same-type pile or store nearby, else a new pile.
	void PutDownHandPot(entt::entity pot) noexcept;
	/// PileResource draw: a pile sinks into the ground as it empties (proportion of its maximum).
	void SinkPile(entt::entity pile) noexcept;
	[[nodiscard]] static PotInfo PotInfoOf(entt::entity entity) noexcept;
	[[nodiscard]] float HeldFill() const noexcept;
	void Throw(glm::vec3 velocity) noexcept;
	/// Tree released gently: replanted on land (Tree::EndPhysics), a DeadTree over water.
	void ReleaseTree(entt::entity tree) noexcept;
	void Replant(entt::entity tree) noexcept;
	void MakeDeadTree(entt::entity tree, glm::vec3 direction) noexcept;
	/// Wood store (village store pit) under the point, if any.
	[[nodiscard]] std::optional<entt::entity> FindWoodStore(glm::vec3 point) const noexcept;
	/// DeleteObjectAndTakeResource: the store takes the tree's wood and the tree is deleted.
	void DepositInStore(entt::entity object, entt::entity store) noexcept;
	[[nodiscard]] bool IsHoldingTree() const noexcept;
	/// HOLD_TYPE of the original (Enum.h): what Object::GetHoldType returns for each class.
	enum class HoldType
	{
		None = 0,
		Above = 1,
		Magic = 2,
		Grain = 3,
		Fingers = 4,
		Tree = 5,
		Side = 6,
		Villager = 7,
	};
	/// GetHoldType / GetHoldRadius / GetHoldLoweringMultiplier / GetHeight / IsARootedObject of an entity.
	void ComputeHoldParameters(entt::entity entity) noexcept;
	/// HandStateTug: a grounded tree is pulled until the hand is weight / 1000 away from the grab point.
	void UpdateTug(float seconds, bool actionHeld) noexcept;
	void Uproot(entt::entity tree) noexcept;
	/// Centre of the closed side grip (between palm and fingertips) in hand model space, for the current pose.
	[[nodiscard]] glm::vec3 GripCentre() const noexcept;
	/// HandStateHolding::Update mouse sway: rotation of the held object's up about the view and side axes.
	[[nodiscard]] glm::mat3 HeldSway(glm::vec3 at) const noexcept;
	/// Tree::DrawOutOfMap draws MSH_T_ROOTS at the tree matrix while it is out of the map.
	void UpdateRoots(entt::entity tree, bool dying = false) noexcept;
	void DropRoots(entt::entity tree, bool fall) noexcept;
	void UpdateRootsAndPiles(float seconds) noexcept;
	void UpdateThrown(float seconds) noexcept;
	void EmitGripDust(glm::vec3 point) noexcept;
	void UpdateGripDust(float seconds) noexcept;
	/// Environment-variable test hooks (HandDebugHooks.cpp), run once when the landscape exists.
	void RunDebugHooks() noexcept;
	void UpdatePickupParticles(float seconds, bool emitting) noexcept;
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
	/// Object under the hand while the action button is held, picked up after k_PickUpHoldSeconds (CHand +0x4908).
	std::optional<entt::entity> _pendingPick;
	float _pendingPickTime {0.0f};
	/// HOLD_TYPE_TREE: height of the held tree and its hold radius (0.2 * 2D radius).
	float _heldHeight {0.0f};
	float _holdRadius {0.0f};
	HoldType _holdType {HoldType::None};
	float _loweringMultiplier {0.0f};
	bool _rooted {false};
	/// Hand roll towards the side grip (+-pi/2); the original only rolls while giving an object to the creature.
	float _roll {0.0f};
	/// HandStateHolding::Update: the hand follows its required position with a spring in 10 ms steps.
	bool _springActive {false};
	float _springTime {0.0f};
	glm::vec3 _springVelocity {0.0f};
	/// CHand mouse smoothing (+0x48B8..+0x48C4), in 1024-wide reference pixels: drives the held object sway.
	glm::vec2 _mouse {0.0f};
	glm::vec2 _smoothMouse {0.0f};
	glm::vec2 _smoothMouseVelocity {0.0f};
	bool _smoothMouseValid {false};
	/// CHand::SetDistanceFromView hand scale (1 between 10 and 150 units from the camera).
	float _handScale {1.0f};
	/// Rotation of the held object when it was picked up (the sway is applied on top of it).
	glm::mat3 _heldRotation {1.0f};
	/// Tree being tugged out of the ground, the point where it was grabbed and its planted rotation.
	std::optional<entt::entity> _tug;
	glm::vec3 _tugPoint {0.0f};
	glm::mat3 _tugRotation {1.0f};
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
	/// Roots piles left where trees were pulled up (lifetime left, seconds).
	std::vector<std::pair<entt::entity, float>> _rootsPiles;
	float _heldAltitude {0.0f};
	float _heldTop {0.0f};
	bool _actionWasHeld {false};

	/// Resource being gathered into the hand pile (HandWood / HandFood) while the action is held over it.
	std::optional<entt::entity> _pickSource;
	float _pickTime {0.0f};
	float _pickTurnAccumulator {0.0f};
	/// Game turns since the locked select started (GInterfaceStatus::Process counter).
	uint32_t _pickTurns {0};
	/// Hand x,z frozen over the pile while scooping (HandStateHolding::Update, locked interact).
	glm::vec3 _pickLock {0.0f};
	/// The press that picked the object up is still held: its release does not drop it (state 7).
	bool _pickPressHeld {false};
	/// A later press while holding: its release drops / throws (state 12).
	bool _releaseArmed {false};
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

	/// SF_MultiPickUpWood / SF_MultiPickUpFood atoms (ER_MultiPickup): pieces rising from the pile into the hand.
	struct PickupParticle
	{
		entt::entity entity;
		float age;
		glm::vec3 start;
		glm::vec3 previous;
		bool mesh;
	};
	std::vector<PickupParticle> _pickupParticles;
	/// ER_MultiPickup collection data: atoms owed (+0x20, EmitRate * time) and atoms emitted (+0x24).
	float _pickupOwed {0.0f};
	uint32_t _pickupEmitted {0};

	std::optional<glm::vec3> _interactionPoint;
	/// GInterface::m_ActionCollide: the object under the cursor this frame (exact triangle pick).
	std::optional<entt::entity> _cursorObject;
	struct CursorHit
	{
		entt::entity entity;
		float t;
		glm::vec3 boxMin;
		glm::vec3 boxMax;
	};
	/// Nearest object whose triangles the ray origin + t * direction (unit) hits.
	[[nodiscard]] std::optional<CursorHit> PickObjectAlongRay(const glm::vec3& origin, const glm::vec3& direction) const noexcept;

	/// g_HandDistZoomer: the hand's distance from the camera along the mouse ray.
	openblack::Zoomer _handDistance;
	/// HandStateNormal::Enter resets the zoomer to the current distance from view.
	bool _handDistanceValid {false};
	std::optional<glm::vec3> _gripPoint;
	glm::mat3 _gripRotation {1.0f};
	std::optional<glm::vec3> _smoothedPosition;
	float _tipClearance {0.45f};
	float _vertexClearance {0.05f};
	float _clawDepth {0.12f};
};
} // namespace openblack::ecs::systems
