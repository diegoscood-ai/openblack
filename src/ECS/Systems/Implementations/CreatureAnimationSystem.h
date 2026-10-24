/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs
{
class Registry;
}
namespace openblack::ecs::components
{
struct CreatureEyes;
struct CreatureTurnPose;
struct FallingCreature;
struct Transform;
} // namespace openblack::ecs::components
namespace openblack::creature_turn_pose
{
struct TurnClock;
}

namespace openblack::ecs::systems
{

class CreatureAnimationSystem final: public CreatureAnimationSystemInterface
{
public:
	void ProcessTurn() override;
	void Update(float turnFraction, std::chrono::duration<float, std::milli> gameTime) override;
	TurnMoments AdvanceTurn() override;
	void PoseTurn(const TurnInputs& inputs) override;
	void KickSway(entt::entity creature, glm::vec3 force, glm::vec3 point) override;
	[[nodiscard]] std::optional<glm::vec3> BoneInAnimation(entt::entity creature, size_t animation, float timeMs, uint32_t bone,
	                                                       bool mirrored) override;
	[[nodiscard]] std::optional<float> AnimationDuration(entt::entity creature, size_t animation) override;
	[[nodiscard]] std::optional<glm::vec3> AnimationTravel(entt::entity creature, size_t animation) override;
	[[nodiscard]] std::optional<glm::vec3> GraspInAnimation(entt::entity creature, size_t animation, float timeMs,
	                                                        uint32_t hand) override;
	bool EnsurePosed(entt::entity creature) override;
	void UpdateCopies(uint32_t frameMs) override;
	void StartFallingCopy(entt::entity copy) override;
	void AdvanceFallingCopy(entt::entity copy, uint32_t milliseconds) override;
	void PrepareFallingCopy(entt::entity copy) override;
	void PlaceFallingCopyEyes(entt::entity copy) override;
	void SetCreedPower(entt::entity creature, creature_hand_glow::Place place, float scale, float power) override;
	void PlaceHandGlows(entt::entity body, uint32_t realMs, uint32_t gameMs) override;
	void PlayCreedEvent(entt::entity creature, int32_t event) override;

	/// EnsurePosed for a body of a species, a creature's or the Creature Cave's copy of one
	static bool EnsurePosedAs(entt::entity entity, CreatureType species);

	/// A number from 0 to n - 1 on the game's local stream
	using LocalRandom = std::function<uint32_t(int32_t)>;
	/// One step of the falling film's copy's time, of `milliseconds` (kept as its last step's), in the original's order:
	/// its breath, its sparkles' draws while it is the body that sparkles (they are not shown yet), its blink on
	/// `localRand`, then its layers, the action, gesture and wobble by falling_spell::CreatureAnimationMs and the face
	/// and head by the milliseconds; then, given a `turnPose`, it is posed (FallingCopyPose, placed by `model`).
	/// Without the species' rig or stand animation nothing plays
	static void FallingCopyStep(components::CreatureAnimation& animation, components::CreatureEyes* eyes,
	                            const creature::CreatureRig* rig, const creature_morph::Morph& morph,
	                            const components::Transform& transform, components::FallingCreature& copy,
	                            uint32_t milliseconds, const LocalRandom& localRand,
	                            components::CreatureTurnPose* turnPose = nullptr, const glm::mat4& model = glm::mat4(1.0f));
	/// The falling film's copy's start: a first step of its time (falling_spell::k_CreatureFirstStepMs), then its time
	/// is the film's; its look is reset (its head straight and still, looking nowhere, its face relaxed) and its body
	/// stands from the start, and given a `turnPose` it is posed again; then it plays the falling action
	/// (falling_spell::k_CreatureAction) once, not mirrored, when its species has it, from the next step's pose on
	static void FallingCopyStart(components::CreatureAnimation& animation, components::CreatureEyes* eyes,
	                             const creature::CreatureRig* rig, const creature_morph::Morph& morph,
	                             const components::Transform& transform, components::FallingCreature& copy,
	                             const LocalRandom& localRand, components::CreatureTurnPose* turnPose = nullptr,
	                             const glm::mat4& model = glm::mat4(1.0f));
	/// The falling film's copy posed, as each step of a creature's time poses it: the body sampled as it plays
	/// (SampleBody, at its `drawnSize`) into its bone matrices, and those placed in the world by `model` become its latest
	/// pose, the latest but one moving to the one before (creature_turn_pose::Advance). Nothing without the species'
	/// stand animation
	static void FallingCopyPose(components::CreatureAnimation& animation, const creature::CreatureRig& rig,
	                            const creature_morph::Morph& morph, const components::Transform& transform, float drawnSize,
	                            const glm::mat4& model, components::CreatureTurnPose& turnPose);
	/// The bones the falling film's copy is drawn with, against its drawn matrix `model`: its last two poses blended
	/// `turnFraction` of the way from the one before to the last (creature_turn_pose::Drawn, the share taken up to 2).
	/// Empty, so that it is drawn as last posed, when it has no pose or a pose has not `boneCount` bones
	[[nodiscard]] static std::vector<glm::mat4> FallingCopyDrawnBones(const components::CreatureTurnPose& turnPose,
	                                                                  const glm::mat4& model, size_t boneCount,
	                                                                  float turnFraction);
	[[nodiscard]] bool AreSparklesShown() const override { return _sparklesShown; }
	void SetSparklesShown(bool shown) override { _sparklesShown = shown; }

	/// Whether a kick leaves the body as it is, by what it does: while it plays an action held between a start and an end
	/// (sitting, sleeping, a cast, fainting where it stands), while a fight has it faint, lie fainted or get up, and while
	/// it turns on the spot or steps off turning into a walk. A fight's other moves, an action played once, standing and
	/// walking all take it
	[[nodiscard]] static bool IgnoresKick(const creature_layers::BodyAction& body, std::optional<creature_fight::State> fight,
	                                      bool turningOrSteppingOff);

	/// Whether the rest pose and animations are to be built again: none yet, or built for an earlier change of the body
	[[nodiscard]] static bool RestIsStale(const components::CreatureAnimation& animation, uint32_t revision);

	/// The eyes placed for the frame on the body as it is drawn (ecs::DrawnBodyModel, turned as ecs::creature_pose::
	/// DrawnPlacementOf), posed by `bones`, sized by the `size` the body is drawn at. Only reads the registry
	static void PlaceEyes(const ecs::Registry& registry, entt::entity entity, components::CreatureEyes& eyes,
	                      const creature::CreatureRig::Eyes& rig, const creature_morph::Morph& morph,
	                      const std::vector<glm::mat4>& bones, float size, float seconds);

	/// The bones the body is drawn with this frame, against its drawn matrix (ecs::DrawnBodyModel): its last two turn
	/// poses blended `turnFraction` of the way from the one before to the last (creature_turn_pose::Drawn). Empty, so
	/// that it is drawn as its animations posed it this frame, when it has no turn pose, when a pose has not
	/// `boneCount` bones (its mesh's), and while it is drawn in the hand or in flight, which move it every frame. Only
	/// reads the registry
	[[nodiscard]] static std::vector<glm::mat4> DrawnBones(const ecs::Registry& registry, entt::entity entity, size_t boneCount,
	                                                       float turnFraction);
	/// The same blend in the world, not seen from the drawn matrix; empty when DrawnBones is for the same reasons
	[[nodiscard]] static std::vector<glm::mat4> DrawnInWorld(const ecs::Registry& registry, entt::entity entity,
	                                                         size_t boneCount, float turnFraction);

	/// The body posed for a frame: sampled as it plays (SampleBody, at its `drawnSize`) into its bone matrices, its layers
	/// where the turn's advance left them; nothing is moved on. Nothing is posed without the species' stand animation
	static void PoseBody(components::CreatureAnimation& animation, const creature::CreatureRig& rig,
	                     const creature_morph::Morph& morph, const components::Transform& transform, float drawnSize);
	/// What each layer's animation passed in a turn: the overlays in the order they are moved on, then the action
	using TurnPassed = creature_layers::TurnPassed;
	/// A turn of what the body plays, by the turn's clock, in the original's order: the face by the turn's
	/// milliseconds; the gesture, then a blow's wobble, by the step, each over once it reaches its end; the breath by the
	/// seconds (first in the original's state's step, which reads nothing of the body's action); the body's action by the
	/// step; then the head turns towards where the creature looks, from its eyes' height at its own `size`, by the
	/// seconds. Gives what each played passed, in that order. Nothing is moved on without the species' stand animation
	static TurnPassed AdvanceTurnLayers(components::CreatureAnimation& animation, const creature::CreatureRig& rig,
	                                    const creature_morph::Morph& morph, const components::Transform& transform, float size,
	                                    const creature_turn_pose::TurnClock& clock);
	/// How far each of the Creature Cave's copy's layers moves on in one of its frames
	struct LayerClock
	{
		/// The face
		float milliseconds;
		/// The head turning towards where it looks
		float seconds;
		/// The action, gesture and wobble, at the rate the body plays
		float playbackMs;
	};
	/// A frame's step: the milliseconds, their seconds, and the milliseconds at the rate of the size it is drawn at
	[[nodiscard]] static LayerClock FrameClock(float drawnSize, float milliseconds, float seconds);
	/// A step of what the Creature Cave's copy plays, by its frame: its action, face, gesture and wobble move on by the
	/// clock, and the head turns towards where it looks. The world's creatures move on once a turn (AdvanceTurnLayers)
	static void AdvanceLayers(components::CreatureAnimation& animation, const creature::CreatureRig& rig,
	                          const creature_morph::Morph& morph, const components::Transform& transform, float size,
	                          const LayerClock& clock);
	/// The bone matrices of the body playing `body`, or `slots` blended when there are any, its head, hands, feet and
	/// eyes as much bigger as its `drawnSize` makes them (creature::GrowParts), with the head turned, the sway's leans
	/// across the body as it faces (`rotation`, its Transform's), and the face, gesture and wobble on top as they stand:
	/// nothing is moved on, and only the cache of blended animations may grow. Empty without the species' stand
	/// animation
	[[nodiscard]] static std::vector<glm::mat4> SampleBody(components::CreatureAnimation& animation,
	                                                       const creature::CreatureRig& rig, const creature_morph::Morph& morph,
	                                                       const creature_layers::BodyAction& body,
	                                                       std::span<const components::CreatureAnimation::Slot> slots,
	                                                       float drawnSize, const glm::mat3& rotation = glm::mat3(1.0f));

	/// The glows on a body as it is drawn (components::CreatureHandGlows::placed), each by creature_hand_glow::PlaceOn on
	/// its bones in the world: the hands at the rig's right hand and its mirror, the belly at the rig's belly, the circling
	/// glow above `origin` by the drawn `size`, all at the drawn `scale`. Their textures slide on by `realMs` and the
	/// circling glow turns by `gameMs`
	static void PlaceGlows(components::CreatureHandGlows& glows, const components::CreatureAnimation& animation,
	                       const creature::CreatureRig& rig, std::span<const glm::mat4> bonesInWorld, glm::vec3 origin,
	                       float scale, float size, uint32_t realMs, uint32_t gameMs);

private:
	bool _sparklesShown {true};
};

} // namespace openblack::ecs::systems
