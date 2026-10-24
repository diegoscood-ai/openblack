/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureAnimationSystem.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <chrono>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include <glm/gtx/transform.hpp>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureAlignmentMotes.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureHandGlow.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSway.h"
#include "Creature/CreatureTurnPose.h"
#include "Creature/TattooEditor.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Components/FallingCreature.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FallingSpellSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "GameClock.h"
#include "Locator.h"
#include "Magic/Objects/FallingSpell.h"
#include "Magic/Objects/FallingSpellCreature.h"
#include "Physics/LivingRules.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
using openblack::skeletal_animation::Animation;

namespace
{
/// The meshes look back along their z axis, so the eyes look ahead along -z when the creature looks nowhere in
/// particular
constexpr glm::vec3 k_MeshBack {0.0f, 0.0f, 1.0f};
/// How fast the head speeds up turning to look, in radians a second each second. The game reads its rate from the
/// species' tables; this stands in until that field is known.
constexpr float k_HeadAcceleration = 4.0f;
/// The head is turned when more than this far round, in radians
constexpr float k_LookSettled = 1e-4f;

CreatureRig::Mesh EvilGoodMesh(float value)
{
	return value < 0.0f ? CreatureRig::Mesh::Evil : CreatureRig::Mesh::Good;
}

CreatureRig::Mesh ThinFatMesh(float value)
{
	return value < 0.0f ? CreatureRig::Mesh::Thin : CreatureRig::Mesh::Fat;
}

/// A variant's animation, the base's adjusted to the variant's stand where the variant has none of its own, and the
/// variant's stand
struct VariantAnimation
{
	std::optional<Animation> adjusted;
	const Animation* animation {nullptr};
	const Animation* stand {nullptr};
};

VariantAnimation VariantOf(const CreatureRig& rig, CreatureRig::Mesh mesh, size_t index)
{
	VariantAnimation variant;
	const auto slot = static_cast<size_t>(mesh);
	if (!rig.hasMesh.at(slot) || slot >= rig.animations.size())
	{
		return variant;
	}
	const auto* base = rig.GetAnimation(CreatureRig::Mesh::Base, index);
	const auto* baseStand = rig.GetAnimation(CreatureRig::Mesh::Base, creature_animation::k_StandAnimation);
	variant.stand = rig.GetAnimation(mesh, creature_animation::k_StandAnimation);
	const auto& own = rig.animations.at(slot);
	if (index < own.size() && own[index].has_value())
	{
		variant.animation = &*own[index];
	}
	else if (index == creature_animation::k_StandAnimation)
	{
		variant.animation = variant.stand;
	}
	else if (base != nullptr && baseStand != nullptr && variant.stand != nullptr)
	{
		variant.adjusted = creature_animation::AdjustFromStand(*base, *baseStand, *variant.stand);
		variant.animation = &*variant.adjusted;
	}
	return variant;
}

/// The species' animation blended as the body is drawn
std::optional<Animation> BlendAnimation(const CreatureRig& rig, const creature_morph::Morph& morph, size_t index)
{
	const auto* base = rig.GetAnimation(CreatureRig::Mesh::Base, index);
	if (base == nullptr)
	{
		return std::nullopt;
	}
	const auto evilGood = VariantOf(rig, EvilGoodMesh(morph.evilGood), index);
	const auto thinFat = VariantOf(rig, ThinFatMesh(morph.thinFat), index);
	return creature_animation::Blend(*base,
	                                 {.animation = evilGood.animation,
	                                  .stand = evilGood.stand,
	                                  .weight = evilGood.animation != nullptr ? std::abs(morph.evilGood) : 0.0f},
	                                 {.animation = thinFat.animation,
	                                  .stand = thinFat.stand,
	                                  .weight = thinFat.animation != nullptr ? std::abs(morph.thinFat) : 0.0f});
}

/// An eye's point on the body as drawn: each vertex blended as the body is, placed by its bone, in the world
std::optional<creature_eyes::SurfacePoint> EyePointOf(const CreatureRig::EyePoint& point, const creature_morph::Morph& morph,
                                                      const std::vector<glm::mat4>& bones, const glm::mat4& model)
{
	if (!point.enabled)
	{
		return std::nullopt;
	}
	const auto world = creature::PosedTriangle(point.vertices, point.bones, morph, bones, model);
	return creature_eyes::PointOnTriangle(world[0], world[1], world[2], point.u, point.v);
}

/// The eyes on a body placed by `model`, turned by `rotation`, drawn at `size`: the eyes are sized by the size the body
/// is drawn at, and sit as deep in it at every size
void EyesOnModel(CreatureEyes& eyes, const CreatureRig::Eyes& rig, const creature_morph::Morph& morph,
                 const std::vector<glm::mat4>& bones, const glm::mat4& model, const glm::mat3& rotation, float size,
                 float seconds)
{
	const auto eyeSize = creature_eyes::EyeSize(size, rig.scale, morph.evilGood, eyes.mode);
	const auto ahead = glm::normalize(rotation * k_MeshBack);
	bool lidColourSet = false;
	for (size_t i = 0; i < eyes.drawn.size(); ++i)
	{
		auto& drawn = eyes.drawn.at(i);
		drawn = {};
		const auto point = EyePointOf(rig.points.at(i), morph, bones, model);
		if (!point || glm::length(point->normal) <= 0.0f)
		{
			continue;
		}
		const auto normal = glm::normalize(point->normal);
		const auto centre = point->position - (rig.points.at(i).depth * normal);

		const bool looksAtPoint =
		    eyes.lookAt.has_value() && eyes.mode != creature_eyes::Mode::Ahead && glm::length(centre - *eyes.lookAt) > 0.0f;
		const auto away = creature_eyes::ClampLook(looksAtPoint ? glm::normalize(centre - *eyes.lookAt) : ahead, normal);
		auto& look = eyes.look.at(i);
		if (!eyes.lookStarted.at(i))
		{
			look.SetPosition(away);
			eyes.lookStarted.at(i) = true;
		}
		look.SetDestinationWithTime(away, creature_eyes::LookSeconds(eyes.openness, eyes.mode));
		look.Update(seconds);
		const auto eyeball = creature_eyes::EyeballFrame(centre, look.GetCurrentValue());
		drawn.eyeball = creature_eyes::ToMatrix(eyeball, eyeSize);

		const auto anchor = EyePointOf(rig.points.at(i + 2), morph, bones, model);
		if (!anchor)
		{
			continue;
		}
		// Both lids take the skin's colour under the first one drawn
		if (!lidColourSet)
		{
			eyes.lidColour = rig.points.at(i + 2).skinColour;
			lidColourSet = true;
		}
		const auto lid = creature_eyes::EyelidFrame(centre, normal, anchor->position, i == 1);
		const auto angle = creature_eyes::EyelidAngle(eyes.mode, rig.lidAngles, eyes.openness,
		                                              creature_eyes::LidPitch(lid, eyeball), eyes.blink);
		drawn.eyelid = creature_eyes::ToMatrix(creature_eyes::Turned(lid, angle), eyeSize);
	}
}
/// A blended animation of the creature's, blended the first time it is wanted, or nothing when the species has none
const Animation* AnimationOf(CreatureAnimation& animation, const CreatureRig& rig, const creature_morph::Morph& morph,
                             size_t index)
{
	auto found = animation.animations.find(index);
	if (found == animation.animations.end())
	{
		// A species without the animation keeps an empty one, so as not to look for it again
		auto blended = BlendAnimation(rig, morph, index).value_or(Animation {});
		found = animation.animations.emplace(index, std::move(blended)).first;
	}
	return found->second.frames.empty() ? nullptr : &found->second;
}

std::optional<uint32_t> DurationOf(const Animation* animation)
{
	return animation != nullptr ? std::optional(animation->duration) : std::nullopt;
}

/// Plays a head turning animation on top of the pose for how far the head is turned, unless it looks straight on
void AddLook(std::vector<skeletal_animation::Pose>& poses, const Animation* look, float angle, float limit,
             const CreatureAnimation& animation, std::span<const uint32_t> mirror)
{
	if (look == nullptr || std::abs(angle) <= k_LookSettled)
	{
		return;
	}
	const auto reference = (look->frames.size() - 1) / 2;
	skeletal_animation::AddLayer(poses, *look, creature_layers::LookTime(angle, limit, look->duration), reference,
	                             animation.skeleton, mirror);
}

/// The swaying body leaning on top of the pose, the lower body then the upper: its offset along the body's own
/// sideways and front to back axes (the columns of its rotation) picks how far through each lean to draw, relative to
/// the lean's middle frame. A lean is drawn as the body faces, whichever way round its action plays
void AddSway(std::vector<skeletal_animation::Pose>& poses, const CreatureAnimation& animation, const glm::mat3& rotation,
             const auto& animationOf)
{
	if (!animation.sway.active)
	{
		return;
	}
	const auto lean = [&](size_t clip, float amount) {
		if (std::abs(amount) <= creature_sway::k_DrawnLean)
		{
			return;
		}
		if (const auto* played = animationOf(clip))
		{
			skeletal_animation::AddLayer(poses, *played, creature_sway::LeanTime(amount, played->duration),
			                             (played->frames.size() - 1) / 2, animation.skeleton);
		}
	};
	for (const auto& [offset, sideClip, frontClip] :
	     {std::tuple {animation.sway.lowerOffset, creature_sway::k_LowerSide, creature_sway::k_LowerFrontBack},
	      std::tuple {animation.sway.upperOffset, creature_sway::k_UpperSide, creature_sway::k_UpperFrontBack}})
	{
		lean(sideClip, (offset.x * rotation[0].x) + (offset.z * rotation[0].z));
		lean(frontClip, (offset.x * rotation[2].x) + (offset.z * rotation[2].z));
	}
}

/// How heavy a creature is to the sway, as to a strike: by the size it is drawn at and the shape it is to become
float SwayMass(const ecs::Registry& registry, entt::entity creature)
{
	const auto target = ecs::creature_pose::TargetMorph(registry, creature);
	return physics::living::CreatureMass(ecs::creature_pose::DrawnSize(registry, creature), target.thinFat, target.weakStrong);
}

} // namespace

void CreatureAnimationSystem::PoseBody(CreatureAnimation& animation, const CreatureRig& rig, const creature_morph::Morph& morph,
                                       const Transform& transform, float drawnSize)
{
	if (AnimationOf(animation, rig, morph, creature_layers::animations::k_Stand) == nullptr)
	{
		return;
	}
	animation.boneMatrices = SampleBody(animation, rig, morph, animation.body, animation.slots, drawnSize, transform.rotation);
}

CreatureAnimationSystem::TurnPassed CreatureAnimationSystem::AdvanceTurnLayers(CreatureAnimation& animation,
                                                                               const CreatureRig& rig,
                                                                               const creature_morph::Morph& morph,
                                                                               const Transform& transform, float size,
                                                                               const creature_turn_pose::TurnClock& clock)
{
	TurnPassed passed;
	if (AnimationOf(animation, rig, morph, creature_layers::animations::k_Stand) == nullptr)
	{
		return passed;
	}
	const auto lengthOf = [&](std::optional<size_t> index) -> std::optional<int32_t> {
		const auto* played = index.has_value() ? AnimationOf(animation, rig, morph, *index) : nullptr;
		return played != nullptr ? std::optional(static_cast<int32_t>(played->duration)) : std::nullopt;
	};
	const auto keep = [&passed](const std::optional<creature_layers::PassedAnimation>& layer) {
		if (layer.has_value())
		{
			passed.overlays.push_back(*layer);
		}
	};

	// The face by the turn's own milliseconds
	auto face = creature_layers::AdvanceFaceLayerByTurn(animation.face, clock.ms, lengthOf(animation.face.current));
	animation.face = face.layer;
	keep(face.passed);
	// The gesture, then a blow's wobble, by the step
	auto gesture = creature_layers::AdvanceGestureByTurn(animation.gesture, clock.step, lengthOf(animation.gesture.animation));
	animation.gesture = gesture.layer;
	keep(gesture.passed);
	auto wobble = creature_layers::AdvanceGestureByTurn(animation.wobble, clock.step, lengthOf(animation.wobble.animation));
	animation.wobble = wobble.layer;
	keep(wobble.passed);
	// The breath by the seconds, then the body's action by the step
	animation.breathPhase = creature_animation::AdvanceBreath(animation.breathPhase, clock.seconds, animation.breathPeriod);
	// A species without the start, the first move or the finish of the dance does not dance
	if (animation.body.kind == creature_layers::BodyAction::Kind::Dance &&
	    (!lengthOf(creature_layers::animations::k_DanceStart).has_value() ||
	     !lengthOf(creature_layers::animations::k_FirstDanceMove).has_value() ||
	     !lengthOf(creature_layers::animations::k_DanceFinish).has_value()))
	{
		animation.body = {};
	}
	const auto parts = animation.body.animations;
	auto body = creature_layers::AdvanceBodyByTurn(animation.body, clock.step,
	                                               {lengthOf(parts[0]), lengthOf(parts[1]), lengthOf(parts[2])}, clock.ms);
	animation.body = body.layer;
	passed.action = body.passed;
	passed.nextAction = body.then;
	// A dance that wants its next move draws one of the moves the species has, by the game's random numbers
	if (animation.body.chooseDanceMove)
	{
		const auto move = creature_layers::ChooseDanceMove(
		    animation.body.firstDanceMove, [](uint32_t range) { return game_random::GameRand(range); },
		    [&](size_t index) { return lengthOf(index).has_value(); });
		animation.body.animations[1] = move.value_or(creature_layers::animations::k_Stand);
		animation.body.chooseDanceMove = false;
	}

	// Last, the head turns towards where the creature looks, from its eyes' height, by the seconds
	const auto ahead = -(transform.rotation * k_MeshBack);
	const auto head = transform.position + glm::vec3(0.0f, creature_look::k_HeadHeight * size, 0.0f);
	const auto angles = animation.lookAt.has_value() ? creature_layers::AnglesTowards(head, ahead, *animation.lookAt)
	                                                 : creature_layers::LookAngles {.yaw = 0.0f, .pitch = 0.0f};
	animation.yaw =
	    creature_layers::TurnHead(animation.yaw, angles.yaw, k_HeadAcceleration, clock.seconds, creature_layers::k_YawLimit);
	animation.pitch = creature_layers::TurnHead(animation.pitch, angles.pitch, k_HeadAcceleration, clock.seconds,
	                                            creature_layers::k_PitchLimit);
	return passed;
}

CreatureAnimationSystem::LayerClock CreatureAnimationSystem::FrameClock(float drawnSize, float milliseconds, float seconds)
{
	return {
	    .milliseconds = milliseconds,
	    .seconds = seconds,
	    .playbackMs = milliseconds * creature_layers::PlaybackRate(drawnSize),
	};
}

void CreatureAnimationSystem::AdvanceLayers(CreatureAnimation& animation, const CreatureRig& rig,
                                            const creature_morph::Morph& morph, const Transform& transform, float size,
                                            const LayerClock& clock)
{
	const auto& [milliseconds, seconds, playbackMs] = clock;
	const auto animationOf = [&](size_t index) { return AnimationOf(animation, rig, morph, index); };

	const auto parts = animation.body.animations;
	animation.body = creature_layers::AdvanceBody(
	    animation.body, playbackMs,
	    {DurationOf(animationOf(parts[0])), DurationOf(animationOf(parts[1])), DurationOf(animationOf(parts[2]))});
	animation.face = creature_layers::AdvanceFace(
	    animation.face, milliseconds,
	    DurationOf(animation.face.current.has_value() ? animationOf(*animation.face.current) : nullptr));
	animation.gesture = creature_layers::AdvanceGesture(
	    animation.gesture, playbackMs,
	    DurationOf(animation.gesture.animation.has_value() ? animationOf(*animation.gesture.animation) : nullptr));
	animation.wobble = creature_layers::AdvanceGesture(
	    animation.wobble, playbackMs,
	    DurationOf(animation.wobble.animation.has_value() ? animationOf(*animation.wobble.animation) : nullptr));

	// The head turns towards where the creature looks, from its eyes' height
	const auto ahead = -(transform.rotation * k_MeshBack);
	const auto head = transform.position + glm::vec3(0.0f, creature_look::k_HeadHeight * size, 0.0f);
	const auto angles = animation.lookAt.has_value() ? creature_layers::AnglesTowards(head, ahead, *animation.lookAt)
	                                                 : creature_layers::LookAngles {.yaw = 0.0f, .pitch = 0.0f};
	animation.yaw =
	    creature_layers::TurnHead(animation.yaw, angles.yaw, k_HeadAcceleration, seconds, creature_layers::k_YawLimit);
	animation.pitch =
	    creature_layers::TurnHead(animation.pitch, angles.pitch, k_HeadAcceleration, seconds, creature_layers::k_PitchLimit);
}

std::vector<glm::mat4> CreatureAnimationSystem::SampleBody(CreatureAnimation& animation, const CreatureRig& rig,
                                                           const creature_morph::Morph& morph,
                                                           const creature_layers::BodyAction& body,
                                                           std::span<const CreatureAnimation::Slot> slots, float drawnSize,
                                                           const glm::mat3& rotation)
{
	const auto* stand = AnimationOf(animation, rig, morph, creature_layers::animations::k_Stand);
	if (stand == nullptr)
	{
		return {};
	}
	const auto animationOf = [&](size_t index) { return AnimationOf(animation, rig, morph, index); };

	const std::span<const uint32_t> mirror = body.mirrored ? animation.mirror : std::span<const uint32_t> {};
	std::vector<skeletal_animation::Pose> poses;
	// Each animation sampled has the head, hands, feet and eyes grown by the drawn size before anything is added to it
	if (!slots.empty())
	{
		std::vector<std::vector<skeletal_animation::Pose>> sampled;
		std::vector<float> weights;
		for (const auto& slot : slots)
		{
			const auto* played = animationOf(slot.animation);
			sampled.push_back(skeletal_animation::SampleCycle(
			    played != nullptr ? *played : *stand, *stand, static_cast<uint32_t>(std::max(slot.timeMs, 0.0f)),
			    animation.skeleton, slot.mirrored ? animation.mirror : std::span<const uint32_t> {}));
			creature::GrowParts(sampled.back(), rig, drawnSize);
			weights.push_back(slot.weight);
		}
		poses = skeletal_animation::WeightedSum(sampled, weights);
	}
	else if (const auto* played =
	             creature_layers::IsPlaying(body) ? animationOf(creature_layers::CurrentAnimation(body)) : nullptr;
	         played != nullptr)
	{
		// a dance move's animation stretched over the move's length
		const auto time = creature_layers::DanceMoveTime(body, played->duration);
		poses = skeletal_animation::SampleCycle(*played, *stand, static_cast<uint32_t>(std::max(time, 0.0f)),
		                                        animation.skeleton, mirror);
		creature::GrowParts(poses, rig, drawnSize);
	}
	else
	{
		const auto time = creature_animation::BreathTime(animation.breathPhase, stand->duration);
		poses = skeletal_animation::SampleCycle(*stand, *stand, time, animation.skeleton);
		creature::GrowParts(poses, rig, drawnSize);
	}

	// Sitting, the head turns by the sitting versions. The head turns the other way when the body plays mirrored.
	const bool sitting = creature_layers::CurrentAnimation(body) == creature_layers::animations::k_Sit;
	AddLook(poses,
	        animationOf(sitting ? creature_layers::animations::k_SitLookDownUp : creature_layers::animations::k_LookDownUp),
	        animation.pitch.angle, creature_layers::k_PitchLimit, animation, mirror);
	AddLook(
	    poses,
	    animationOf(sitting ? creature_layers::animations::k_SitLookRightLeft : creature_layers::animations::k_LookRightLeft),
	    mirror.empty() ? animation.yaw.angle : -animation.yaw.angle, creature_layers::k_YawLimit, animation, mirror);
	// The sway leans the body between the head's turning and the face
	AddSway(poses, animation, rotation, animationOf);
	// The face and gestures, relative to their first frames
	if (animation.face.current.has_value())
	{
		if (const auto* face = animationOf(*animation.face.current))
		{
			skeletal_animation::AddLayer(poses, *face, static_cast<uint32_t>(std::max(animation.face.timeMs, 0.0f)), 0,
			                             animation.skeleton);
		}
	}
	if (animation.gesture.animation.has_value())
	{
		if (const auto* gesture = animationOf(*animation.gesture.animation))
		{
			skeletal_animation::AddLayer(poses, *gesture, static_cast<uint32_t>(std::max(animation.gesture.timeMs, 0.0f)), 0,
			                             animation.skeleton);
		}
	}
	// A blow's wobble, relative to its first frame, the way the body plays
	if (animation.wobble.animation.has_value())
	{
		if (const auto* wobble = animationOf(*animation.wobble.animation))
		{
			skeletal_animation::AddLayer(poses, *wobble, static_cast<uint32_t>(std::max(animation.wobble.timeMs, 0.0f)), 0,
			                             animation.skeleton, mirror);
		}
	}
	return skeletal_animation::ComposeBoneMatrices(poses, animation.skeleton.parents);
}

void CreatureAnimationSystem::PlaceEyes(const ecs::Registry& registry, entt::entity entity, CreatureEyes& eyes,
                                        const CreatureRig::Eyes& rig, const creature_morph::Morph& morph,
                                        const std::vector<glm::mat4>& bones, float size, float seconds)
{
	// the body's own drawn matrix, so that the eyes sit on the body wherever and however it is drawn this frame
	const auto model = ecs::DrawnBodyModel(registry, entity);
	const auto rotation = ecs::creature_pose::DrawnPlacementOf(registry, entity).rotation;
	EyesOnModel(eyes, rig, morph, bones, model, rotation, size, seconds);
}

std::vector<glm::mat4> CreatureAnimationSystem::DrawnInWorld(const ecs::Registry& registry, entt::entity entity,
                                                             size_t boneCount, float turnFraction)
{
	const auto* pose = registry.TryGet<const CreatureTurnPose>(entity);
	if (pose == nullptr || !creature_turn_pose::HasPose(*pose) || pose->current.size() != boneCount ||
	    pose->previous.size() != boneCount || registry.AnyOf<HandDrawPose, PhysicsDrawPose>(entity))
	{
		return {};
	}
	// the drawing reads the live turn poses, also while the hand keeps a copy for the game
	return creature_turn_pose::Drawn(*pose, turnFraction);
}

std::vector<glm::mat4> CreatureAnimationSystem::DrawnBones(const ecs::Registry& registry, entt::entity entity, size_t boneCount,
                                                           float turnFraction)
{
	const auto world = DrawnInWorld(registry, entity, boneCount, turnFraction);
	return world.empty() ? world : creature_turn_pose::RelativeTo(ecs::DrawnBodyModel(registry, entity), world);
}

void CreatureAnimationSystem::ProcessTurn()
{
	// The creatures are found through the const registry, which makes no storage when there is none
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	// Breathing settles towards the period it should be at a step a turn; standing, that is its resting period, by the
	// size it is drawn at
	lookup.Each<const Creature, const CreatureAnimation>(
	    [&registry, &lookup](entt::entity entity, const Creature&, const CreatureAnimation&) {
		    auto& animation = registry.Get<CreatureAnimation>(entity);
		    const auto resting = creature_animation::BreathPeriod(ecs::creature_pose::DrawnSize(lookup, entity));
		    animation.breathPeriod =
		        creature_animation::EaseBreathPeriod(animation.breathPeriod, resting, resting, game_clock::k_TurnSeconds);
	    });
}

void CreatureAnimationSystem::Update(float turnFraction, std::chrono::duration<float, std::milli> gameTime)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& resources = Locator::resources::value();
	const auto& meshes = resources.GetMeshes();
	const auto& rigs = resources.GetCreatureRigs();
	const auto seconds = std::chrono::duration<float>(gameTime).count();

	std::as_const(registry).Each<const Creature, const CreatureMorph, const CreatureAnimation, const Transform>(
	    [&](entt::entity entity, const Creature& creature, const CreatureMorph&, const CreatureAnimation&,
	        const Transform& transform) {
		    auto& morph = registry.Get<CreatureMorph>(entity);
		    auto& animation = registry.Get<CreatureAnimation>(entity);

		    // The rest pose and the animations, built again when the body has changed
		    if (!EnsurePosed(entity))
		    {
			    return;
		    }
		    const auto bodyMeshes = creature_morph::MeshesOf(creature.species, morph.drawn,
		                                                     [&meshes](entt::id_type id) { return meshes.Contains(id); });
		    const auto base = meshes.Handle(bodyMeshes.base);
		    const auto rigId = creature::GetRigId(creature.species);
		    const CreatureRig* rig = rigs.Contains(rigId) ? &*rigs.Handle(rigId) : nullptr;

		    // Standing, the creature breathes in and out over its stand animation, from its first frame at its resting
		    // period. Its breath and its layers move on once a turn (AdvanceTurn): the frame only poses them as they stand
		    if (animation.breathPeriod <= 0.0f)
		    {
			    animation.breathPeriod =
			        creature_animation::BreathPeriod(ecs::creature_pose::DrawnSize(std::as_const(registry), entity));
		    }
		    if (rig != nullptr)
		    {
			    PoseBody(animation, *rig, morph.drawn, transform,
			             ecs::creature_pose::DrawnSize(std::as_const(registry), entity));
		    }
		    if (animation.boneMatrices.size() != base->GetBoneMatrices().size())
		    {
			    animation.boneMatrices = base->GetBoneMatrices();
		    }
		    // The body is drawn between its last two turn poses, one turn behind the game, as far through as the turn is;
		    // the blend in the world is kept for the footsteps the next drawing reaches
		    animation.drawnInWorld = DrawnInWorld(registry, entity, base->GetBoneMatrices().size(), turnFraction);
		    animation.drawnBones =
		        animation.drawnInWorld.empty()
		            ? std::vector<glm::mat4> {}
		            : creature_turn_pose::RelativeTo(ecs::DrawnBodyModel(registry, entity), animation.drawnInWorld);

		    auto* eyes = registry.TryGet<CreatureEyes>(entity);
		    if (eyes == nullptr)
		    {
			    return;
		    }
		    eyes->drawn = {};
		    if (rig != nullptr && rig->eyes.has_value())
		    {
			    // The eyes sit on the body as it is drawn this frame. The look of the head above stays on the
			    // Transform: the turn reads those bones for what the hand holds
			    PlaceEyes(registry, entity, *eyes, *rig->eyes, morph.drawn, creature_turn_pose::BonesDrawn(animation),
			              ecs::creature_pose::DrawnSize(std::as_const(registry), entity), seconds);
		    }
	    });

	// The glows are placed by the draws of this frame alone (PlaceHandGlows): a body not drawn has none placed, and its
	// glows do not move on
	std::vector<entt::entity> glowing;
	std::as_const(registry).Each<const CreatureHandGlows>(
	    [&glowing](entt::entity entity, const CreatureHandGlows&) { glowing.push_back(entity); });
	for (const auto entity : glowing)
	{
		registry.Get<CreatureHandGlows>(entity).placed = {};
	}
}

void CreatureAnimationSystem::PlaceHandGlows(entt::entity body, uint32_t realMs, uint32_t gameMs)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(body) || !lookup.AllOf<CreatureHandGlows, CreatureAnimation, Transform>(body) ||
	    !Locator::resources::has_value())
	{
		return;
	}
	// A creature, or the falling film's copy of one, which is not a creature
	const auto* living = lookup.TryGet<const Creature>(body);
	const auto* copy = lookup.TryGet<const FallingCreature>(body);
	if (living == nullptr && copy == nullptr)
	{
		return;
	}
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(living != nullptr ? living->species : copy->species);
	if (!rigs.Contains(rigId))
	{
		return;
	}
	const auto& animation = lookup.Get<const CreatureAnimation>(body);
	// The body's bones in the world as it is drawn: between its turn poses, else as it is drawn against its drawn matrix
	std::vector<glm::mat4> placedBones;
	std::span<const glm::mat4> bones = animation.drawnInWorld;
	if (bones.empty())
	{
		const auto model = ecs::DrawnBodyModel(lookup, body);
		const auto& drawn = creature_turn_pose::BonesDrawn(animation);
		placedBones.reserve(drawn.size());
		for (uint32_t bone = 0; bone < drawn.size(); ++bone)
		{
			placedBones.push_back(creature::PosedBone(bone, drawn, model));
		}
		bones = placedBones;
	}
	const float size = living != nullptr ? ecs::creature_pose::DrawnSize(lookup, body) : copy->size;
	PlaceGlows(registry.Get<CreatureHandGlows>(body), animation, *rigs.Handle(rigId), bones, ecs::DrawnPosition(lookup, body),
	           ecs::creature_pose::DrawnScale(lookup, body).x, size, realMs, gameMs);
}

void CreatureAnimationSystem::PlaceGlows(CreatureHandGlows& glows, const CreatureAnimation& animation,
                                         const creature::CreatureRig& rig, std::span<const glm::mat4> bonesInWorld,
                                         glm::vec3 origin, float scale, float size, uint32_t realMs, uint32_t gameMs)
{
	const auto& points = rig.actionPoints;
	// (openblack guard) a species without its action points has only the circling glow
	const auto none = static_cast<uint32_t>(bonesInWorld.size());
	const creature_hand_glow::Body body {
	    .bones = bonesInWorld,
	    .parents = animation.skeleton.parents,
	    .mirror = animation.mirror,
	    .rightHand = points.has_value() ? points->rightHand : none,
	    .belly = points.has_value() ? points->belly : none,
	    .origin = origin,
	    .scale = scale,
	    .size = size,
	};
	for (size_t place = 0; place < creature_hand_glow::k_PlaceCount; ++place)
	{
		if (auto& glow = glows.glows.at(place); glow.has_value())
		{
			glows.placed.at(place) =
			    creature_hand_glow::PlaceOn(*glow, static_cast<creature_hand_glow::Place>(place), body, realMs, gameMs);
		}
	}
}

namespace
{
/// While the falling film plays, the creature it shows is the only one to sparkle, so the game's own creatures make no
/// motes or smoke
bool AnotherBodySparkles()
{
	if (!Locator::magicSystem::has_value())
	{
		return false;
	}
	const auto& spell = Locator::magicSystem::value().FallingSpellStore().Spell();
	return spell.has_value() && spell->IsActive();
}

/// Each creature whose body is to look good or evil makes its motes or its smoke for the turn, near the bones of the pose
/// it was given the turn before, on the game's local random numbers. They go into the physics' pool of puffs; without
/// a physics (a test's world) the numbers are drawn all the same
void AddAlignmentMotes(const ecs::Registry& lookup)
{
	if (AnotherBodySparkles())
	{
		return;
	}
	const creature_alignment_motes::LocalRandom random = [](int32_t n) { return game_random::LocalRand(n); };
	auto* dynamics = Locator::dynamicsSystem::has_value() ? &Locator::dynamicsSystem::value() : nullptr;
	lookup.Each<const Creature, const CreatureTurnPose>([&lookup, &random, dynamics](entt::entity entity, const Creature&,
	                                                                                 const CreatureTurnPose& pose) {
		const float alignment = ecs::creature_pose::TargetMorph(lookup, entity).evilGood;
		if (!creature_alignment_motes::MakesAny(alignment))
		{
			return;
		}
		const auto& bones = pose.current;
		const creature_alignment_motes::BonePoint bonePoint = [&bones](uint32_t bone) { return glm::vec3(bones.at(bone)[3]); };
		const auto size = ecs::creature_pose::DrawnSize(lookup, entity);
		const auto puffs =
		    creature_alignment_motes::PuffsOf(alignment, size, static_cast<uint32_t>(bones.size()), bonePoint, random);
		for (const auto& puff : puffs)
		{
			if (dynamics != nullptr)
			{
				dynamics->AddPuff(puff.position, puff.velocity, puff.size, puff.argb, puff.kind);
			}
		}
	});
}
} // namespace

CreatureAnimationSystem::TurnMoments CreatureAnimationSystem::AdvanceTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto msPerTurn = static_cast<int32_t>(game_clock::MsPerTurn());
	// Each body moves on by the whole milliseconds it lives this turn, its animations at the rate of the size it is
	// drawn at (the pen's in its pen). What each layer passed is where the turn's sounds are to be found
	TurnMoments moments;
	lookup.Each<const Creature, const CreatureMorph, const CreatureAnimation, const Transform>(
	    [&registry, &lookup, &rigs, &moments, msPerTurn](entt::entity entity, const Creature& creature,
	                                                     const CreatureMorph& morph, const CreatureAnimation& played,
	                                                     const Transform& transform) {
		    const auto rigId = creature::GetRigId(creature.species);
		    if (RestIsStale(played, morph.revision) || !rigs.Contains(rigId))
		    {
			    return;
		    }
		    const auto clock = creature_turn_pose::ClockOfTurn(played.playbackScale, msPerTurn,
		                                                       ecs::creature_pose::DrawnSize(lookup, entity), false, false);
		    moments.emplace_back(entity, AdvanceTurnLayers(registry.Get<CreatureAnimation>(entity), *rigs.Handle(rigId),
		                                                   morph.drawn, transform, creature.size, clock));
	    });
	return moments;
}

void CreatureAnimationSystem::PoseTurn(const TurnInputs& inputs)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	// First the shape the body is drawn with takes the attributes the turn has left it, once a turn and before the
	// pose: an axis is redrawn when it is 0.03 or more from what is drawn, and is drawn as it is until then
	lookup.Each<const Creature, const CreatureMorph>(
	    [&registry, &lookup](entt::entity entity, const Creature&, const CreatureMorph&) {
		    auto& morph = registry.Get<CreatureMorph>(entity);
		    const auto target = ecs::creature_pose::TargetMorph(lookup, entity);
		    const auto refresh = creature_morph::RefreshDrawn(morph.drawn, target);
		    morph.drawn = refresh.drawn;
		    if (refresh.animations)
		    {
			    ++morph.revision;
		    }
	    });
	// Then the eyes blink by the milliseconds the creature lives this turn, after its mind has set how slowly they blink:
	// the wait until the next blink is drawn from the game's local stream
	const auto msPerTurn = static_cast<int32_t>(game_clock::MsPerTurn());
	lookup.Each<const CreatureEyes>([&registry, &lookup, msPerTurn](entt::entity entity, const CreatureEyes&) {
		const auto* played = lookup.TryGet<const CreatureAnimation>(entity);
		const auto livedMs = creature_turn_pose::LivedMs(played != nullptr ? played->playbackScale : 1.0f, msPerTurn);
		auto& eyes = registry.Get<CreatureEyes>(entity);
		eyes.blink = creature_eyes::AdvanceBlink(
		    eyes.blink, livedMs, [](uint32_t range) { return game_random::LocalRand(static_cast<int32_t>(range)); });
	});
	// The sway's spring takes the seconds the creature lives this turn, which a kick later in the turn reads, and while
	// it sways is stepped once before the pose. Nothing in the game drags it by the leash, so nothing pushes it here
	lookup.Each<const Creature, const CreatureAnimation>(
	    [&registry, &lookup, msPerTurn](entt::entity entity, const Creature&, const CreatureAnimation& played) {
		    auto& sway = registry.Get<CreatureAnimation>(entity).sway;
		    sway.frameSeconds = static_cast<float>(creature_turn_pose::LivedMs(played.playbackScale, msPerTurn)) * 0.001f;
		    if (sway.active)
		    {
			    creature_sway::Step(sway, glm::vec3(0.0f), sway.frameSeconds, SwayMass(lookup, entity));
		    }
	    });
	// Then, before the new pose, the motes or the smoke of the creatures that look good or evil, unless turned off
	if (_sparklesShown)
	{
		AddAlignmentMotes(lookup);
	}
	// The body as it plays at the turn's end, placed in the world where the turn leaves it, at the scale it is drawn at,
	// leaned onto the land there.
	// The pose is sampled, not advanced: the turn's advance has just moved the animations on. A body that can't be
	// sampled (no rig, no skeleton yet, or a different number of bones) keeps the pose the frames last gave it
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	std::vector<std::pair<entt::entity, std::vector<glm::mat4>>> posed;
	lookup.Each<const Creature, const CreatureAnimation, const Transform>(
	    [&registry, &lookup, &rigs, &inputs, &posed](entt::entity entity, const Creature& creature,
	                                                 const CreatureAnimation& played, const Transform& transform) {
		    if (played.boneMatrices.empty())
		    {
			    return;
		    }
		    auto bones = played.boneMatrices;
		    const auto rigId = creature::GetRigId(creature.species);
		    const auto* morph = lookup.TryGet<const CreatureMorph>(entity);
		    if (morph != nullptr && !played.skeleton.Empty() && rigs.Contains(rigId))
		    {
			    const auto found = std::ranges::find(inputs, entity, &TurnInputs::value_type::first);
			    const auto& atEnd = found != inputs.end()
			                            ? found->second
			                            : CreatureAnimationInputs {.body = played.body, .slots = played.slots};
			    auto sampled =
			        SampleBody(registry.Get<CreatureAnimation>(entity), *rigs.Handle(rigId), morph->drawn, atEnd.body,
			                   atEnd.slots, ecs::creature_pose::DrawnSize(lookup, entity), transform.rotation);
			    if (sampled.size() == bones.size())
			    {
				    bones = std::move(sampled);
			    }
		    }
		    const auto world = ecs::creature_pose::LeanedModel(lookup, entity, transform.position, transform.rotation,
		                                                       ecs::creature_pose::DrawnScale(lookup, entity));
		    posed.emplace_back(entity, creature_turn_pose::InWorld(bones, world));
	    });
	// Written once the walk is done, so that the first pose's new storage is not made while the creatures are walked
	for (auto& [entity, bones] : posed)
	{
		auto* pose = registry.TryGet<CreatureTurnPose>(entity);
		creature_turn_pose::Advance(pose != nullptr ? *pose : registry.AssignState<CreatureTurnPose>(entity), std::move(bones));
	}
}

bool CreatureAnimationSystem::IgnoresKick(const creature_layers::BodyAction& body, std::optional<creature_fight::State> fight,
                                          bool turningOrSteppingOff)
{
	using creature_fight::State;
	if (fight == State::Faint || fight == State::Lying || fight == State::GetUp)
	{
		return true;
	}
	// A held action plays a start, a loop and an end; a fight shows its moves the same way, timed by the fight
	const bool held = body.kind == creature_layers::BodyAction::Kind::Sequence && !body.timedByPlayer;
	return held || turningOrSteppingOff;
}

void CreatureAnimationSystem::KickSway(entt::entity creature, glm::vec3 force, glm::vec3 point)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!lookup.Valid(creature) || !lookup.AllOf<Creature, CreatureAnimation>(creature))
	{
		return;
	}
	const auto* fighting = lookup.TryGet<const CreatureFighting>(creature);
	const auto* locomotion = lookup.TryGet<const CreatureLocomotion>(creature);
	const bool turning = locomotion != nullptr && (locomotion->motion == CreatureLocomotion::Motion::Turning ||
	                                               locomotion->motion == CreatureLocomotion::Motion::Stepping);
	auto& animation = registry.Get<CreatureAnimation>(creature);
	if (IgnoresKick(animation.body, fighting != nullptr ? std::optional(fighting->fighter.state) : std::nullopt, turning))
	{
		return;
	}
	// The height of the point above the land under it picks the upper or the lower body
	const float ground =
	    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(point.x, point.z)) : 0.0f;
	creature_sway::Kick(animation.sway, force, point.y - ground,
	                    creature_sway::k_UpperHeightPerSize * ecs::creature_pose::DrawnSize(lookup, creature),
	                    SwayMass(lookup, creature));
}

namespace
{
/// A creature's animation blended as its body is drawn, if it has the animation and has been posed
const Animation* PosedAnimationOf(entt::entity creature, size_t index)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* morph = registry.TryGet<const CreatureMorph>(creature);
	auto* animation = registry.TryGet<CreatureAnimation>(creature);
	if (body == nullptr || morph == nullptr || animation == nullptr || animation->skeleton.Empty())
	{
		return nullptr;
	}
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(body->species);
	if (!rigs.Contains(rigId))
	{
		return nullptr;
	}
	return AnimationOf(*animation, *rigs.Handle(rigId), morph->drawn, index);
}
} // namespace

std::optional<glm::vec3> CreatureAnimationSystem::BoneInAnimation(entt::entity creature, size_t animation, float timeMs,
                                                                  uint32_t bone, bool mirrored)
{
	const auto* played = PosedAnimationOf(creature, animation);
	const auto* stand = PosedAnimationOf(creature, creature_layers::animations::k_Stand);
	if (played == nullptr || stand == nullptr)
	{
		return std::nullopt;
	}
	const auto& body = Locator::entitiesRegistry::value().Get<CreatureAnimation>(creature);
	if (bone >= body.skeleton.parents.size())
	{
		return std::nullopt;
	}
	const auto time = static_cast<uint32_t>(std::clamp(timeMs, 0.0f, static_cast<float>(std::max(played->duration, 1u) - 1)));
	const auto poses = skeletal_animation::SampleCycle(
	    *played, *stand, time, body.skeleton, mirrored ? std::span<const uint32_t>(body.mirror) : std::span<const uint32_t> {});
	const auto matrices = skeletal_animation::ComposeBoneMatrices(poses, body.skeleton.parents);
	return bone < matrices.size() ? std::optional(glm::vec3(matrices[bone][3])) : std::nullopt;
}

bool CreatureAnimationSystem::RestIsStale(const CreatureAnimation& animation, uint32_t revision)
{
	return animation.builtRevision != revision || animation.skeleton.Empty();
}

bool CreatureAnimationSystem::EnsurePosed(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return false;
	}
	const auto* creature = registry.TryGet<const Creature>(entity);
	return creature != nullptr && EnsurePosedAs(entity, creature->species);
}

bool CreatureAnimationSystem::EnsurePosedAs(entt::entity entity, CreatureType species)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* morph = registry.TryGet<const CreatureMorph>(entity);
	auto* animation = registry.TryGet<CreatureAnimation>(entity);
	if (morph == nullptr || animation == nullptr)
	{
		return false;
	}
	auto& resources = Locator::resources::value();
	const auto& meshes = resources.GetMeshes();
	const auto& rigs = resources.GetCreatureRigs();
	const auto bodyMeshes =
	    creature_morph::MeshesOf(species, morph->drawn, [&meshes](entt::id_type id) { return meshes.Contains(id); });
	if (!meshes.Contains(bodyMeshes.base))
	{
		return false;
	}
	if (!RestIsStale(*animation, morph->revision))
	{
		return true;
	}
	// The rest pose and the animations follow the evil to good and thin to fat axes
	const auto base = meshes.Handle(bodyMeshes.base);
	const auto rigId = creature::GetRigId(species);
	const CreatureRig* rig = rigs.Contains(rigId) ? &*rigs.Handle(rigId) : nullptr;
	const auto evilGood = meshes.Handle(bodyMeshes.evilGood);
	const auto thinFat = meshes.Handle(bodyMeshes.thinFat);
	const auto rest =
	    creature_animation::BlendRest(base->GetBoneMatrices(), evilGood->GetBoneMatrices(), std::abs(morph->drawn.evilGood),
	                                  thinFat->GetBoneMatrices(), std::abs(morph->drawn.thinFat));
	animation->skeleton = skeletal_animation::Skeleton::FromRestMatrices(base->GetBoneParents(), rest);
	animation->animations.clear();
	animation->builtRevision = morph->revision;
	animation->boneMatrices = rest;
	// Each bone's mirror as the species' file has it, else as the rest pose pairs the bones
	auto fromFile = rig != nullptr ? rig->MirrorBones(rest.size()) : std::nullopt;
	animation->mirror = fromFile.has_value() ? std::move(*fromFile) : skeletal_animation::MirrorJoints(rest);
	return true;
}

std::optional<float> CreatureAnimationSystem::AnimationDuration(entt::entity creature, size_t animation)
{
	const auto* played = PosedAnimationOf(creature, animation);
	return played != nullptr ? std::optional(static_cast<float>(played->duration)) : std::nullopt;
}

void CreatureAnimationSystem::UpdateCopies(uint32_t frameMs)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& resources = Locator::resources::value();
	const auto& meshes = resources.GetMeshes();
	const auto& rigs = resources.GetCreatureRigs();
	const auto localRand = [](int32_t range) { return game_random::LocalRand(range); };

	registry.Each<CreatureCopy, const CreatureMorph, CreatureAnimation, Transform>([&](entt::entity entity, CreatureCopy& copy,
	                                                                                   const CreatureMorph& shape,
	                                                                                   CreatureAnimation& animation,
	                                                                                   Transform& transform) {
		if (!EnsurePosedAs(entity, copy.species))
		{
			return;
		}
		const auto& morph = shape.drawn;
		auto* eyes = registry.TryGet<CreatureEyes>(entity);
		const auto base = meshes.Handle(
		    creature_morph::MeshesOf(copy.species, morph, [&meshes](entt::id_type id) { return meshes.Contains(id); }).base);
		const auto rigId = creature::GetRigId(copy.species);
		const CreatureRig* rig = rigs.Contains(rigId) ? &*rigs.Handle(rigId) : nullptr;
		const auto bones = static_cast<int32_t>(animation.skeleton.parents.size());
		const auto lengthOf = [&](size_t index) -> std::optional<int32_t> {
			const auto* played = rig != nullptr ? AnimationOf(animation, *rig, morph, index) : nullptr;
			return played != nullptr ? std::optional(static_cast<int32_t>(played->duration)) : std::nullopt;
		};

		// A moment of its time, in the original's order: its breath, its sparkles' draws (unless the room's start has
		// made them), its blink on the local stream, then its layers at its own size's rate. Both of the layers'
		// clocks, the milliseconds and the seconds, are the frame's real time
		const auto moment = [&](uint32_t milliseconds, bool sparkles) {
			const auto ms = static_cast<float>(milliseconds);
			if (animation.breathPeriod <= 0.0f)
			{
				animation.breathPeriod = creature_animation::BreathPeriod(copy.size);
			}
			animation.breathPhase =
			    creature_animation::AdvanceBreath(animation.breathPhase, ms * 0.001f, animation.breathPeriod);
			if (sparkles)
			{
				creature_tattoo_editor::DrawCopyMoment(copy.alignment, bones, localRand);
			}
			if (eyes != nullptr)
			{
				eyes->blink = creature_eyes::AdvanceBlink(eyes->blink, static_cast<int32_t>(milliseconds), [](uint32_t range) {
					return game_random::LocalRand(static_cast<int32_t>(range));
				});
			}
			if (rig != nullptr && AnimationOf(animation, *rig, morph, creature_layers::animations::k_Stand) != nullptr)
			{
				AdvanceLayers(animation, *rig, morph, transform, copy.size, FrameClock(copy.size, ms, ms * 0.001f));
			}
			// and its turn on the spot, by its animations' whole milliseconds
			creature_tattoo_editor::StepCopyTurn(copy.turn, copy.heading,
			                                     creature_tattoo_editor::CopyAnimationStep(copy.size, milliseconds),
			                                     lengthOf(creature_layers::animations::k_Stand).value_or(0));
		};

		// The room's start: a millisecond standing, then its rest, sitting looking at the fire or asleep, and another
		// millisecond. Their draws were made as the copy was
		if (!copy.started)
		{
			moment(1, false);
			const auto rest = creature_tattoo_editor::CopyRestAnimations(copy.sleeping);
			if (const auto resting = creature_layers::PlaySequence(animation.body, rest[0], rest[1], rest[2]))
			{
				animation.body = *resting;
			}
			animation.lookAt = copy.sleeping ? std::nullopt : std::optional(copy.fire);
			if (eyes != nullptr)
			{
				eyes->lookAt = animation.lookAt;
			}
			moment(1, false);
			copy.started = true;
		}
		// A turn asked for after the room was last drawn starts now, by the lengths of the species' spins
		if (copy.turnRequested.has_value())
		{
			copy.turn = creature_tattoo_editor::StartCopyTurn(copy.heading, *copy.turnRequested, lengthOf);
			copy.turnRequested.reset();
		}
		// Each frame the room is drawn: sitting, a yawn one time in a thousand, played on top of the rest when the
		// species has it and none plays yet; then it moves on by the frame
		if (creature_tattoo_editor::CopyStartsYawn(copy.sleeping, false, localRand) && rig != nullptr &&
		    AnimationOf(animation, *rig, morph, creature_layers::animations::k_Yawn) != nullptr)
		{
			if (const auto yawn = creature_layers::PlayGesture(animation.gesture, creature_layers::animations::k_Yawn))
			{
				animation.gesture = *yawn;
			}
		}
		moment(frameMs, true);
		// Turning, it plays its two spins blended at the turn's time; it faces its heading. Its rest and yawn play on
		// the body and gesture layers, and nothing else gives the copy a slot, so they are only the turn's
		animation.slots.clear();
		if (copy.turn.has_value() && copy.turn->spins.has_value())
		{
			const auto& spins = *copy.turn->spins;
			const auto time = static_cast<float>(copy.turn->timeMs);
			animation.slots.push_back(
			    {.animation = spins.from, .timeMs = time, .weight = 1.0f - spins.weight, .mirrored = false});
			animation.slots.push_back({.animation = spins.to, .timeMs = time, .weight = spins.weight, .mirrored = false});
		}
		transform.rotation = affine::AngleY(copy.heading);

		// Posed as it plays, drawn as posed: it has no turn poses
		if (rig != nullptr)
		{
			animation.boneMatrices = SampleBody(animation, *rig, morph, animation.body, animation.slots, copy.size);
		}
		if (animation.boneMatrices.size() != base->GetBoneMatrices().size())
		{
			animation.boneMatrices = base->GetBoneMatrices();
		}
		animation.drawnBones.clear();
		animation.drawnInWorld.clear();
		if (eyes != nullptr)
		{
			eyes->drawn = {};
			if (rig != nullptr && rig->eyes.has_value())
			{
				PlaceEyes(registry, entity, *eyes, *rig->eyes, morph, animation.boneMatrices, copy.size,
				          static_cast<float>(frameMs) * 0.001f);
			}
		}
	});
}

void CreatureAnimationSystem::FallingCopyStep(CreatureAnimation& animation, CreatureEyes* eyes, const CreatureRig* rig,
                                              const creature_morph::Morph& morph, const Transform& transform,
                                              FallingCreature& copy, uint32_t milliseconds, const LocalRandom& localRand,
                                              CreatureTurnPose* turnPose, const glm::mat4& model)
{
	const auto ms = static_cast<float>(milliseconds);
	// the step's milliseconds, which the eyes' look moves on by while it is drawn
	copy.lastStepMs = milliseconds;
	// its breath
	if (animation.breathPeriod <= 0.0f)
	{
		animation.breathPeriod = creature_animation::BreathPeriod(copy.size);
	}
	animation.breathPhase = creature_animation::AdvanceBreath(animation.breathPhase, ms * 0.001f, animation.breathPeriod);
	// its sparkles' draws, while it is the body that sparkles, one of them picking from its body's bones
	if (copy.sparkling)
	{
		creature_tattoo_editor::DrawCopyMoment(copy.alignment, static_cast<int32_t>(animation.skeleton.parents.size()),
		                                       localRand);
	}
	// its blink
	if (eyes != nullptr)
	{
		const auto blinkRand = [&localRand](uint32_t range) { return localRand(static_cast<int32_t>(range)); };
		eyes->blink = creature_eyes::AdvanceBlink(eyes->blink, static_cast<int32_t>(milliseconds), blinkRand);
	}
	// then what it plays: its action, gesture and wobble by its animations' milliseconds, its face and head by the step's
	if (rig != nullptr && AnimationOf(animation, *rig, morph, creature_layers::animations::k_Stand) != nullptr)
	{
		const auto played =
		    magic::falling_spell::CreatureAnimationMs(static_cast<int32_t>(milliseconds), copy.size, copy.filmTimed);
		AdvanceLayers(animation, *rig, morph, transform, copy.size,
		              {.milliseconds = ms, .seconds = ms * 0.001f, .playbackMs = static_cast<float>(played)});
	}
	// and it is posed as it now plays
	if (rig != nullptr && turnPose != nullptr)
	{
		FallingCopyPose(animation, *rig, morph, transform, copy.size, model, *turnPose);
	}
}

void CreatureAnimationSystem::FallingCopyStart(CreatureAnimation& animation, CreatureEyes* eyes, const CreatureRig* rig,
                                               const creature_morph::Morph& morph, const Transform& transform,
                                               FallingCreature& copy, const LocalRandom& localRand, CreatureTurnPose* turnPose,
                                               const glm::mat4& model)
{
	// a first step of its time, at its size's rate, which poses it; from now on its time is the film's
	FallingCopyStep(animation, eyes, rig, morph, transform, copy,
	                static_cast<uint32_t>(magic::falling_spell::k_CreatureFirstStepMs), localRand, turnPose, model);
	copy.filmTimed = true;
	// its look reset: the head straight and still, looking nowhere, the face relaxed, and the body standing from the
	// start
	animation.yaw = {};
	animation.pitch = {};
	animation.lookAt.reset();
	animation.face = {};
	animation.body = {};
	if (eyes != nullptr)
	{
		eyes->lookAt.reset();
	}
	// the reset poses it again
	if (rig != nullptr && turnPose != nullptr)
	{
		FallingCopyPose(animation, *rig, morph, transform, copy.size, model, *turnPose);
	}
	// then the falling action, once and not mirrored, when its species has it
	const auto action = static_cast<size_t>(magic::falling_spell::k_CreatureAction);
	if (rig != nullptr && AnimationOf(animation, *rig, morph, action) != nullptr)
	{
		if (const auto once = creature_layers::PlayOnce(animation.body, action, false))
		{
			animation.body = *once;
		}
	}
}

void CreatureAnimationSystem::FallingCopyPose(CreatureAnimation& animation, const CreatureRig& rig,
                                              const creature_morph::Morph& morph, const Transform& transform, float drawnSize,
                                              const glm::mat4& model, CreatureTurnPose& turnPose)
{
	auto sampled = SampleBody(animation, rig, morph, animation.body, animation.slots, drawnSize, transform.rotation);
	if (sampled.empty())
	{
		return;
	}
	animation.boneMatrices = std::move(sampled);
	creature_turn_pose::Advance(turnPose, creature_turn_pose::InWorld(animation.boneMatrices, model));
}

std::vector<glm::mat4> CreatureAnimationSystem::FallingCopyDrawnBones(const CreatureTurnPose& turnPose, const glm::mat4& model,
                                                                      size_t boneCount, float turnFraction)
{
	if (!creature_turn_pose::HasPose(turnPose) || turnPose.current.size() != boneCount || turnPose.previous.size() != boneCount)
	{
		return {};
	}
	return creature_turn_pose::RelativeTo(model, creature_turn_pose::Drawn(turnPose, turnFraction));
}

namespace
{
/// The falling film's copy's body and its species' rig, once its rest pose and animations are built: nothing when it is
/// not a copy or its species' body is not loaded
struct FallingCopyBody
{
	FallingCreature& copy;
	CreatureAnimation& animation;
	const CreatureMorph& shape;
	const Transform& transform;
	CreatureEyes* eyes;
	const CreatureRig* rig;
};
std::optional<FallingCopyBody> FallingCopyBodyOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(entity) || !lookup.AllOf<FallingCreature, CreatureMorph, CreatureAnimation, Transform>(entity))
	{
		return std::nullopt;
	}
	const auto species = lookup.Get<const FallingCreature>(entity).species;
	if (!CreatureAnimationSystem::EnsurePosedAs(entity, species))
	{
		return std::nullopt;
	}
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(species);
	return FallingCopyBody {
	    .copy = registry.Get<FallingCreature>(entity),
	    .animation = registry.Get<CreatureAnimation>(entity),
	    .shape = lookup.Get<const CreatureMorph>(entity),
	    .transform = lookup.Get<const Transform>(entity),
	    .eyes = registry.TryGet<CreatureEyes>(entity),
	    .rig = rigs.Contains(rigId) ? &*rigs.Handle(rigId) : nullptr,
	};
}

uint32_t FallingCopyRand(int32_t range)
{
	return game_random::LocalRand(range);
}

/// The poses the falling film's copy is drawn between, made the first time it is posed
CreatureTurnPose& FallingCopyPoses(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* poses = registry.TryGet<CreatureTurnPose>(entity);
	return poses != nullptr ? *poses : registry.AssignState<CreatureTurnPose>(entity);
}
} // namespace

void CreatureAnimationSystem::StartFallingCopy(entt::entity copy)
{
	if (const auto body = FallingCopyBodyOf(copy))
	{
		auto& poses = FallingCopyPoses(copy);
		FallingCopyStart(body->animation, body->eyes, body->rig, body->shape.drawn, body->transform, body->copy,
		                 FallingCopyRand, &poses, ecs::DrawnBodyModel(std::as_const(Locator::entitiesRegistry::value()), copy));
	}
}

void CreatureAnimationSystem::AdvanceFallingCopy(entt::entity copy, uint32_t milliseconds)
{
	if (const auto body = FallingCopyBodyOf(copy))
	{
		auto& poses = FallingCopyPoses(copy);
		FallingCopyStep(body->animation, body->eyes, body->rig, body->shape.drawn, body->transform, body->copy, milliseconds,
		                FallingCopyRand, &poses, ecs::DrawnBodyModel(std::as_const(Locator::entitiesRegistry::value()), copy));
	}
}

void CreatureAnimationSystem::PrepareFallingCopy(entt::entity copy)
{
	if (const auto body = FallingCopyBodyOf(copy))
	{
		// drawn between its last two poses, as far through as the turn the film paused (the share is kept in pause)
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		const auto* poses = registry.TryGet<const CreatureTurnPose>(copy);
		body->animation.drawnBones =
		    poses != nullptr ? FallingCopyDrawnBones(*poses, ecs::DrawnBodyModel(registry, copy),
		                                             body->animation.boneMatrices.size(), game_clock::TurnFraction())
		                     : std::vector<glm::mat4> {};
	}
}

void CreatureAnimationSystem::PlaceFallingCopyEyes(entt::entity copy)
{
	const auto body = FallingCopyBodyOf(copy);
	if (!body || body->eyes == nullptr)
	{
		return;
	}
	body->eyes->drawn = {};
	if (body->rig != nullptr && body->rig->eyes.has_value())
	{
		PlaceEyes(std::as_const(Locator::entitiesRegistry::value()), copy, *body->eyes, *body->rig->eyes, body->shape.drawn,
		          creature_turn_pose::BonesDrawn(body->animation), body->copy.size,
		          static_cast<float>(body->copy.lastStepMs) * 0.001f);
	}
}

void CreatureAnimationSystem::SetCreedPower(entt::entity creature, creature_hand_glow::Place place, float scale, float power)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	auto* glows = registry.TryGet<CreatureHandGlows>(creature);
	if (glows == nullptr)
	{
		glows = &registry.Assign<CreatureHandGlows>(creature);
	}
	const auto hum = creature_hand_glow::SetCreedPower(glows->glows, place, scale, power);
	if (hum == creature_hand_glow::Hum::Unchanged || !Locator::soundTagSystem::has_value())
	{
		return;
	}
	// The creed hum: a looping 3D sound kept up at the creature while its glows have power. When they lose it, the
	// creature's hum tags go and the sound finishes the loop it is in
	constexpr int k_CreedHumSample = 166;
	if (hum == creature_hand_glow::Hum::Start)
	{
		audio::tags::Create(creature, k_CreedHumSample, true, 2, -1, false, true, audio::SfxBank::InGame, 0);
	}
	else
	{
		audio::tags::Remove(creature, k_CreedHumSample, audio::SfxBank::InGame);
	}
}

void CreatureAnimationSystem::PlayCreedEvent(entt::entity creature, int32_t event)
{
	const auto glow = creature_hand_glow::CreedEventOf(event);
	auto& registry = Locator::entitiesRegistry::value();
	if (!glow.has_value() || !registry.Valid(creature))
	{
		return;
	}
	// The glows are made on the first moment, even one that puts a glow out; the glow is set with no hum
	auto* glows = registry.TryGet<CreatureHandGlows>(creature);
	if (glows == nullptr)
	{
		glows = &registry.Assign<CreatureHandGlows>(creature);
	}
	creature_hand_glow::Set(glows->glows, glow->place, creature_hand_glow::k_CreedEventScale, glow->power);
}

std::optional<glm::vec3> CreatureAnimationSystem::AnimationTravel(entt::entity creature, size_t animation)
{
	const auto* played = PosedAnimationOf(creature, animation);
	return played != nullptr ? std::optional(played->displacement) : std::nullopt;
}

std::optional<glm::vec3> CreatureAnimationSystem::GraspInAnimation(entt::entity creature, size_t animation, float timeMs,
                                                                   uint32_t hand)
{
	const auto* played = PosedAnimationOf(creature, animation);
	const auto* stand = PosedAnimationOf(creature, creature_layers::animations::k_Stand);
	if (played == nullptr || stand == nullptr)
	{
		return std::nullopt;
	}
	const auto& body = Locator::entitiesRegistry::value().Get<CreatureAnimation>(creature);
	if (hand >= body.skeleton.parents.size())
	{
		return std::nullopt;
	}
	const auto time = static_cast<uint32_t>(std::clamp(timeMs, 0.0f, static_cast<float>(std::max(played->duration, 1u) - 1)));
	const auto poses = skeletal_animation::SampleCycle(*played, *stand, time, body.skeleton, {});
	const auto matrices = skeletal_animation::ComposeBoneMatrices(poses, body.skeleton.parents);
	return creature_turn_pose::LeafMean(body.skeleton.parents, matrices, hand);
}
