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

#include <entt/entity/entity.hpp>

#include "3D/HandAnimator.h"
#include "Audio/SamplePlay.h"
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
	void PlaceObjectInMagicHand(entt::entity entity) noexcept override { PickUp(entity); }
	// HandSpellSeed.cpp
	[[nodiscard]] bool IsHandReadyForObject() const noexcept override;
	void ForceDropHeld() noexcept override;
	void EndAction() noexcept override;
	void GetSpellInfo(glm::vec3& interfacePos, glm::vec3& handPos, glm::vec3& cameraForward,
	                  glm::vec3& velocity) const noexcept override;
	[[nodiscard]] float GetHandScale() const noexcept override { return _handScale; }
	[[nodiscard]] glm::mat4 GetHandMatrix() const noexcept override;
	[[nodiscard]] std::optional<float> GetAmountInHandToolTip() const noexcept override
	{
		// test hook OPENBLACK_TEST_TOOLTIP=<amount>: always shown
		if (static const char* test = std::getenv("OPENBLACK_TEST_TOOLTIP"); test != nullptr)
		{
			return static_cast<float>(std::atof(test));
		}
		return _amountToolTipTime > 0.0f ? std::optional(_amountToolTip) : std::nullopt;
	}
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
	/// HandHolding.cpp: a gentle release (zero velocity) through Release
	void Drop() noexcept;
	/// HandHolding.cpp: the hand opens with the holding spring's velocity: ApplyThisToMapCoord (a tree on a wood
	/// store, a hand pot put down at |v|^2 <= 5), then ThrowObjectFromHand -> InitialisePhysicsFromHand
	void Release(glm::vec3 velocity) noexcept;
	/// HandHolding.cpp: Object::ThrowObjectFromHand(status, dont_replant) 0x6385E0: the held object leaves the hand
	/// (RemoveFromHand), a hand pot slow enough is put down, else InitialisePhysicsFromHand. Release passes
	/// dont_replant 0, ForceDropHeld 1 (packet 0x1D, no ApplyThisToMapCoord)
	void ThrowObjectFromHand(glm::vec3 velocity, bool dontReplant) noexcept;
	/// HandHolding.cpp: Object::InitialisePhysicsFromHand 0x636F00 (AddObject, AdjustToGroundLevel,
	/// RaiseUntilNotIntersecting, the LANDED rule); false when no body could be made
	bool InitialisePhysicsFromHand(entt::entity entity, glm::vec3 velocity, bool dontReplant) noexcept;
	/// HandHolding.cpp: openblack's placement of an object that has no physics body
	void PlaceWithoutBody(entt::entity entity) noexcept;
	void UpdateHeldObject() noexcept;
	void UpdateMultiPickUp(float seconds, bool actionHeld) noexcept;
	/// HandTrees.cpp: BigForest::InterfaceSetInMagicHand 0x4393C0: the forest gives a Conifer to the hand
	bool TakeTreeFromForest(entt::entity forest) noexcept;
	/// HandFish.cpp: the splash of gripping the water (StartLandscapeGrip fn_005D1AB0)
	void SplashHand(glm::vec3 point) noexcept;
	/// HandFish.cpp: the sound of gripping the land (StartLandscapeGrip, G_HandGrabLand_01..06)
	void GripLandSound(glm::vec3 point) noexcept;
	/// HandFish.cpp: the action over the water next to a fish starts catching from its farm (FishFarm locked select)
	bool TryPickUpFish(glm::vec3 point) noexcept;
	/// HandFish.cpp: FishFarm::ProcessInInteract per game turn; false if the source is not a fish farm
	bool UpdateFishPickUp(float seconds) noexcept;
	/// HandFish.cpp: the action on a field starts taking its food (Field::NetworkFriendlyStartLockedSelect 0x529900)
	bool TryPickUpField(entt::entity field) noexcept;
	/// HandFish.cpp: Field::ProcessInInteract 0x529730 per game turn; false if the source is not a field
	bool UpdateFieldPickUp(float seconds) noexcept;
	/// Test hook OPENBLACK_TEST_SPLASH="x,z": a hand splash there every second
	void UpdateTestSplash(float seconds) noexcept;
	/// Pot::AddResourceToPos: a hand pot put down merges into a same-type pile or store nearby, else a new pile.
	void PutDownHandPot(entt::entity pot) noexcept;
	/// PileResource draw: a pile sinks into the ground as it empties (proportion of its maximum).
	void SinkPile(entt::entity pile) noexcept;
	[[nodiscard]] static PotInfo PotInfoOf(entt::entity entity) noexcept;
	[[nodiscard]] float HeldFill() const noexcept;
	void Replant(entt::entity tree) noexcept;
	/// Tree -> DeadTree. With placeLying it is laid on the ground towards direction; a physics body keeps its pose.
	void MakeDeadTree(entt::entity tree, glm::vec3 direction, bool placeLying = true) noexcept;
	/// The hand's part of the physics system: EndPhysics and ReactToPhysicsImpact of trees, pots and stores.
	void RegisterPhysicsHandlers() noexcept;
	/// Wood store (village store pit) under the point, if any.
	[[nodiscard]] std::optional<entt::entity> FindWoodStore(glm::vec3 point) const noexcept;
	/// DeleteObjectAndTakeResource: the store takes the tree's wood and the tree is deleted.
	void DepositInStore(entt::entity object, entt::entity store) noexcept;
	[[nodiscard]] bool IsHoldingTree() const noexcept;

	// ---- HandSpellSeed.cpp: a spell seed in the hand (GInterface's apply states, SpellSeed's interface virtuals) ----
	/// GInterface action states with a seed: 8/9 apply on release (to the land / an object), 10/11 locked apply
	enum class SeedAction : uint8_t
	{
		None = 0,
		ApplyOnReleaseMap = 8,
		ApplyOnReleaseObject = 9,
		LockedApplyMap = 10,
		LockedApplyObject = 11,
	};
	[[nodiscard]] bool IsHoldingSeed() const noexcept;
	/// ActionPressedHolding 0x5D1560, the seed branches
	void SeedActionPressed() noexcept;
	/// States 8..11 (0x5D48D0, 0x5D4C10, 0x5D4D00) every frame: release applies / unlocks
	void UpdateSeedAction(bool actionHeld) noexcept;
	/// BeginApplyOnRelease fn_005D2730 / EndApplyOnRelease 0x5D27B0: the buffer reseeded, the G_HANDGESTURE_02 loop
	void BeginApplyOnRelease(SeedAction state) noexcept;
	void EndApplyOnRelease() noexcept;
	/// SendApplyToMapCoord 0x5D3340 (the seed part) -> packet 0x12 -> SpellSeed::ApplyThisToMapCoord 0x728E20
	int SendSeedApplyToMapCoord() noexcept;
	/// SendApplyToObject 0x5D30D0 (the seed part) -> packet 0x11 -> SpellSeed::ApplyThisToObject 0x728D10
	int SendSeedApplyToObject() noexcept;
	/// FailApply fn_005D18F0: SPOT_VISUAL 4 (SF_FailedApply) at the point and G_SpellCastFailure
	int FailApply(glm::vec3 point) noexcept;
	/// HandleApplyResult fn_005DA100: 0x16 -> the seed leaves the hand, 3 -> consumed
	void HandleSeedApplyResult(int result, entt::entity seed) noexcept;
	/// Every frame: the hold parameters (MAGIC until ready), the seed's own mesh (IsG3DObjectDrawnInHand), its coming in
	/// and out of the hand, and what the gesture system is told (magic::gestures::SetHandStatus)
	void UpdateSeedInHand(bool actionHeld) noexcept;
	/// SpellSeed::InterfaceSetOutMagicHand 0x728940; the hand stops drawing it (out of the hand Spell::DrawSpellSeed
	/// 0x721360 -> 0x729020 draws a seed that follows its spell: magic::seed::DrawSpells)
	void SeedLeftHand(entt::entity seed) noexcept;
	/// Test hooks OPENBLACK_TEST_CAST / OPENBLACK_TEST_CAST_PATH / OPENBLACK_TEST_THROW_VEL (HandSpellSeed.cpp)
	bool TestCastActionHeld(float seconds, bool actionHeld) noexcept;
	[[nodiscard]] std::optional<glm::vec3> TestCastPathPoint() const noexcept;

	// ---- HandApplyToObject.cpp: the held object applied to the object under the hand, and seeds / stones picked up ----
	/// The seed an object out of the hand gives the hand when it is ValidForPlaceInHand (vt 0x6FC): a SpellSeed itself
	/// (0x728580), a MagicTeleport its spell's seed (0x5FC440); entt::null otherwise
	[[nodiscard]] static entt::entity SeedToPlaceInHand(entt::entity object) noexcept;
	/// GenericPickup 0x5D2800 (the 225 ms grab) of a seed or a stone: packet 0x13 -> GInterface::PlaceObjectInMagicHand
	/// 0x5DA6F0 (a stone's InterfaceSetInMagicHand 0x5FC470 places its seed). False if the object is neither.
	bool PickUpSeedOrStone(entt::entity object, bool inInfluence) noexcept;
	/// ActionPressedHolding 0x5D1560 for a held object that is not a seed: when the object under the hand takes it
	/// (vt 0x71C at 0x5D1607, the influence rule 0x5D15D6..0x5D15E9), SendApplyToObject 0x5D30D0 -> packet 0x11 ->
	/// 0x5DA1A0 -> vt 0x720 ApplyThisToObject -> HandleApplyResult fn_005DA100. False: nothing applied (the press arms the
	/// put down / throw as before)
	bool HeldActionPressedOnObject(bool inInfluence) noexcept;
	/// fn_005CED60 RemoveFirstFromHand: the held object leaves the hand without physics (GMagicHand::RemoveFromHand
	/// 0x5FB0B0: FireEffect::SetOutMagicHand); the caller places it
	std::optional<entt::entity> RemoveFirstFromHand() noexcept;

	SeedAction _seedAction {SeedAction::None};
	/// the object under the cursor when the apply started (m_ActionCollide.object)
	entt::entity _seedTarget {entt::null};
	/// m_ApplySentTurn: one apply packet per game turn
	std::optional<uint32_t> _applySentTurn;
	/// the seed the hand held last frame (to see it come and go)
	entt::entity _seedInHand {entt::null};
	float _testCastTime {-1.0f};
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
	glm::vec3 _tugPoint {0.0f};           ///< CHand+0x492C tugAnchor: the tree's base, the pivot
	glm::mat3 _tugRotation {1.0f};        ///< as it stood, for a tug let go
	glm::vec3 _tugGrab {0.0f};            ///< where the hand took hold of it
	float _tugDepth {0.0f};               ///< that point's distance along the mouse ray
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
	/// The locked select is catching fish (_pickSource is a FishFarm)
	bool _pickFish {false};
	/// The locked select is taking food from a field (_pickSource is a Field)
	bool _pickField {false};
	/// Test hook (OPENBLACK_HAND_TEST_FISH): seconds the action button counts as held
	float _testActionSeconds {0.0f};
	/// UpdateMultiPickup fn_0068F930: the looping G_PICKUPFOOD / G_PICKUPWOOD of a multi pick-up and its pitch t^2
	std::optional<audio::Channel> _pickupSound;
	/// The forced tooltip 0xEEA: the amount and how long it is still shown (1.2 s after the last pick-up turn)
	float _amountToolTip {0.0f};
	float _amountToolTipTime {0.0f};
	float _pickupSoundFraction {0.0f};
	/// HandEffects.cpp: starts / re-pitches / stops the multi pick-up loop
	void UpdatePickupSound(bool active) noexcept;
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
		float frame; ///< the atom's +0x10C, from RandomiseInitFrame (frame_anim::PSysFrameAdvance)
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
		float frame {0.0f};     ///< the atom's +0x10C: InitFrame 0, or RandomiseInitFrame (fish)
		float frameRate {0.0f}; ///< +0x110: FrameRate, negated by RandomiseFrameDirection (fish)
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
