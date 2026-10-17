/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureObjectActionSystem.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandAvoid.h"
#include "3D/LandIslandInterface.h"
#include "Creature/CreatureCatch.h"
#include "Creature/CreatureEating.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreaturePickUpRules.h"
#include "Creature/CreatureReach.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureThrow.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreaturePhysics.h"
#include "ECS/CreaturePose.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerPhysics.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/VillagerSpeed.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Physics/LivingRules.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
using openblack::creature_object_actions::Kind;
using openblack::creature_object_actions::Status;
using Phase = openblack::ecs::components::CreatureObjectAction::Phase;

namespace
{
constexpr float k_TurnSeconds = game_clock::k_TurnSeconds;
constexpr auto k_TurnMs = static_cast<float>(game_clock::k_MsPerTurn);
/// The meshes look back along their z axis: ahead is -z
constexpr glm::vec3 k_MeshBack {0.0f, 0.0f, 1.0f};
/// It walks or turns this many times to get something in reach before giving up
constexpr uint32_t k_ReachAttempts = 4;
/// Turning to face what it throws at or points at, it tries this many times
constexpr uint32_t k_TurnAttempts = 2;
/// Walking up to something it walks to within this share of the distance to the middle of its reach, and stops no
/// nearer than this share of it
constexpr float k_ApproachFar = 1.0f;
constexpr float k_ApproachNear = 0.6f;
/// Near enough to reach once turned: within this share of the distance to the middle of its reach, either way
constexpr float k_TurnInReachShare = 0.5f;
/// Pointing, the high animation counts fully this far above level, in radians
constexpr float k_PointHighRadians = std::numbers::pi_v<float> / 4.0f;
/// Pointing, the hand is about this share of the creature's height up
constexpr float k_PointFromHeightShare = 0.6f;
/// Nothing smaller than this counts as a distance
constexpr float k_Tiny = 1e-4f;

const CreatureRig* RigOf(const Creature& creature)
{
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(creature.species);
	return rigs.Contains(id) ? &*rigs.Handle(id) : nullptr;
}

const CreatureRig::ActionPoints* PointsOf(const Creature& creature)
{
	const auto* rig = RigOf(creature);
	return rig != nullptr && rig->actionPoints.has_value() ? &*rig->actionPoints : nullptr;
}

glm::mat4 PlacementOf(const Transform& transform)
{
	return creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
}

/// A point of the world in the creature's own space, kept at the world's scale
glm::vec3 ToLocal(const Transform& transform, const glm::vec3& world)
{
	return glm::transpose(transform.rotation) * (world - transform.position);
}

/// How far round from ahead a point is, along the ground
float BearingTo(const Transform& transform, const glm::vec3& target)
{
	const auto local = ToLocal(transform, target);
	const glm::vec2 ahead {0.0f, -1.0f};
	const glm::vec2 towards {local.x, local.z};
	if (glm::length(towards) < k_Tiny)
	{
		return 0.0f;
	}
	return std::acos(std::clamp(glm::dot(ahead, glm::normalize(towards)), -1.0f, 1.0f));
}

/// The hand a creature acts with: the right, or its mirror
uint32_t HandBone(const CreatureRig::ActionPoints& points, const CreatureAnimation& animation, bool mirrored)
{
	return mirrored && points.rightHand < animation.mirror.size() ? animation.mirror[points.rightHand] : points.rightHand;
}

/// Where the palm is: among the hand bone and the fingers that hang from it, posed by `bones`
glm::vec3 PalmOf(uint32_t hand, const CreatureAnimation& animation, std::span<const glm::mat4> bones,
                 const glm::mat4& placement)
{
	auto sum = glm::vec3(creature::PosedBone(hand, bones, placement)[3]);
	float count = 1.0f;
	const auto& parents = animation.skeleton.parents;
	for (uint32_t bone = 0; bone < parents.size(); ++bone)
	{
		if (parents[bone] == hand)
		{
			sum += glm::vec3(creature::PosedBone(bone, bones, placement)[3]);
			count += 1.0f;
		}
	}
	return sum / count;
}

/// Where the palm is as the body is posed this frame
glm::vec3 PalmOf(uint32_t hand, const CreatureAnimation& animation, const glm::mat4& placement)
{
	return PalmOf(hand, animation, animation.boneMatrices, placement);
}

/// Where the middle of something is from where it stands, by its mesh's bounds, in its own space at the world's scale
glm::vec3 MiddleOf(const ecs::Registry& registry, entt::entity entity)
{
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == nullptr || transform == nullptr || !meshes.Contains(mesh->id))
	{
		return glm::vec3(0.0f);
	}
	return meshes.Handle(mesh->id)->GetBoundingBox().Center() * transform->scale;
}

/// Where the right hand is in an animation at a time, in the creature's own space at the world's scale
std::optional<glm::vec3> HandAt(entt::entity creature, const Transform& transform, const CreatureRig::ActionPoints& points,
                                size_t animation, float timeMs)
{
	if (!Locator::creatureAnimationSystem::has_value())
	{
		return std::nullopt;
	}
	const auto bone =
	    Locator::creatureAnimationSystem::value().BoneInAnimation(creature, animation, timeMs, points.rightHand, false);
	return bone.has_value() ? std::optional(*bone * transform.scale) : std::nullopt;
}

/// Where the palm is at a moment of the action's animations, as the body plays them blended, in the world: the hand
/// bone and the fingers that hang from it, in each animation at that time, weighed as the animations are. Nothing when
/// the creature hasn't been posed.
std::optional<glm::vec3> PalmAtMoment(entt::entity creature, const CreatureObjectAction& action,
                                      const CreatureRig::ActionPoints& points, const CreatureAnimation& animation,
                                      const Transform& transform)
{
	if (!Locator::creatureAnimationSystem::has_value() || action.animationCount == 0)
	{
		return std::nullopt;
	}
	auto& animations = Locator::creatureAnimationSystem::value();
	const auto hand = HandBone(points, animation, action.mirrored);
	std::vector<uint32_t> bones {hand};
	for (uint32_t bone = 0; bone < animation.skeleton.parents.size(); ++bone)
	{
		if (animation.skeleton.parents[bone] == hand)
		{
			bones.push_back(bone);
		}
	}
	glm::vec3 sum {0.0f};
	float weights = 0.0f;
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		glm::vec3 palm {0.0f};
		float count = 0.0f;
		for (const auto bone : bones)
		{
			if (const auto at =
			        animations.BoneInAnimation(creature, action.animations.at(i), action.eventMs, bone, action.mirrored))
			{
				palm += *at;
				count += 1.0f;
			}
		}
		if (count > 0.0f)
		{
			sum += (palm / count) * action.weights.at(i);
			weights += action.weights.at(i);
		}
	}
	if (weights <= 0.0f)
	{
		return std::nullopt;
	}
	return glm::vec3(PlacementOf(transform) * glm::vec4(sum / weights, 1.0f));
}

/// Where the hand takes hold in each of four reaching animations
std::optional<creature_reach::Points> MeasureReach(entt::entity creature, const Transform& transform,
                                                   const CreatureRig::ActionPoints& points,
                                                   const std::array<size_t, creature_reach::k_CornerCount>& animations,
                                                   float timeMs)
{
	creature_reach::Points reach {};
	for (size_t i = 0; i < animations.size(); ++i)
	{
		const auto hand = HandAt(creature, transform, points, animations.at(i), timeMs);
		if (!hand.has_value())
		{
			return std::nullopt;
		}
		reach.at(i) = *hand;
	}
	return reach;
}

float DurationOf(entt::entity creature, size_t animation)
{
	if (!Locator::creatureAnimationSystem::has_value())
	{
		return 0.0f;
	}
	return Locator::creatureAnimationSystem::value().AnimationDuration(creature, animation).value_or(0.0f);
}

/// How long each of the animations an action plays lasts
std::array<float, 4> DurationsOf(entt::entity creature, const CreatureObjectAction& action)
{
	std::array<float, 4> durations {};
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		durations.at(i) = DurationOf(creature, action.animations.at(i));
	}
	return durations;
}

bool IsVillager(const ecs::Registry& registry, std::optional<entt::entity> entity)
{
	return entity.has_value() && registry.Valid(*entity) && registry.AllOf<Villager>(*entity);
}

/// How heavy something is: a creature's mass, anything else from the game's tables
std::optional<float> WeightOf(const ecs::Registry& registry, entt::entity entity)
{
	if (registry.AllOf<Creature>(entity))
	{
		return ecs::creature_physics::Weight(registry, entity);
	}
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	if (registry.AllOf<Villager>(entity))
	{
		const auto* villager = ecs::VillagerInfoOf(entity);
		return villager != nullptr ? std::optional(villager->weight) : std::nullopt;
	}
	if (const auto* animal = registry.TryGet<const Animal>(entity))
	{
		const auto kind = static_cast<size_t>(animal->type);
		return kind < info.animal.size() ? std::optional(info.animal.at(kind).weight) : std::nullopt;
	}
	if (const auto* object = registry.TryGet<const MobileObject>(entity))
	{
		const auto kind = static_cast<size_t>(object->type);
		return kind < info.mobileObject.size() ? std::optional(info.mobileObject.at(kind).weight) : std::nullopt;
	}
	return std::nullopt;
}

/// Something eaten is gone: a villager or an animal dies, which takes it out of its home, its town and its flock;
/// anything else is deleted
void Consume(const ecs::Registry& registry, entt::entity food)
{
	if (registry.AnyOf<Villager, Animal>(food))
	{
		ecs::life::Kill(food, "eaten by a creature");
	}
	else
	{
		ecs::ToBeDeleted(food);
	}
}

/// The class of a mobile object as its row of the game's table makes it, none without the table
std::optional<MobileObjectInfo> MobileClassOf(const ecs::Registry& registry, entt::entity object)
{
	const auto* mobile = registry.TryGet<const MobileObject>(object);
	if (mobile == nullptr || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& rows = Locator::infoConstants::value().mobileObject;
	const auto row = static_cast<size_t>(mobile->type);
	return row < rows.size() ? std::optional(rows.at(row).mobileType) : std::nullopt;
}

/// A mushroom, which a creature always eats: a mobile object of the magic mushroom or toadstool class, or the magic
/// mushroom feature
bool IsMushroom(const ecs::Registry& registry, entt::entity object)
{
	if (const auto* feature = registry.TryGet<const Feature>(object))
	{
		return feature->type == FeatureInfo::MagicMushroom;
	}
	const auto mobileClass = MobileClassOf(registry, object);
	return mobileClass == MobileObjectInfo::MagicMushroom || mobileClass == MobileObjectInfo::Toadstool;
}

/// Something poisoned, which a creature spits out: a poisoned pot or pile, a toadstool, a poisoned villager or animal
bool IsPoisonedFood(const ecs::Registry& registry, entt::entity object)
{
	if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		return pot->poisoned;
	}
	if (const auto* mobile = registry.TryGet<const MobileObject>(object))
	{
		return mobile->type == MobileObjectInfo::Toadstool;
	}
	return registry.AnyOf<Villager, Animal>(object) && ecs::life::IsPoisoned(object);
}

/// The blend of the four reaching animations for a target now, mirrored when it lies to the creature's left
struct Reach
{
	bool mirrored;
	creature_reach::Blend blend;
};
Reach ReachFor(const creature_reach::Points& points, const glm::vec3& local)
{
	const bool mirrored = creature_reach::ReachesMirrored(points, local);
	return {.mirrored = mirrored, .blend = creature_reach::Solve(mirrored ? creature_reach::Mirrored(points) : points, local)};
}

void SetSlots(CreatureObjectAction& action, std::span<const size_t> animations, std::span<const float> weights)
{
	action.animationCount = static_cast<uint8_t>(std::min(animations.size(), action.animations.size()));
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		action.animations.at(i) = animations[i];
		action.weights.at(i) = weights[i];
	}
}

/// The body stops whatever else it plays and walking, and the animations start from their beginning
void BeginPlaying(entt::entity creature, CreatureObjectAction& action, CreatureAnimation& animation)
{
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	animation.body = {};
	action.phase = Phase::Playing;
	action.timeMs = 0.0f;
	action.durationMs = 0.0f;
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		action.durationMs = std::max(action.durationMs, DurationOf(creature, action.animations.at(i)));
	}
	// Whatever it does at a moment happens by the end at the latest
	action.eventMs = std::min(action.eventMs, std::max(action.durationMs - 1.0f, 0.0f));
}

void Fail(CreatureObjectAction& action, std::string reason)
{
	action.status = Status::Failed;
	action.failure = std::move(reason);
}

/// The flat and high throws blended by how high the target is against where the hand lets go in each
std::array<float, 2> ThrowWeights(entt::entity creature, const Transform& transform, const CreatureRig::ActionPoints& points,
                                  const glm::vec3& target)
{
	const auto slope = [](const glm::vec3& local) { return local.y / std::max(-local.z, k_Tiny); };
	const auto flat = HandAt(creature, transform, points, creature_throw::k_HurlFlat, points.throwMs);
	const auto high = HandAt(creature, transform, points, creature_throw::k_HurlHigh, points.throwMs);
	if (!flat.has_value() || !high.has_value())
	{
		return {1.0f, 0.0f};
	}
	const auto weight = creature_throw::HighThrowWeight(slope(ToLocal(transform, target)), slope(*flat), slope(*high));
	return {1.0f - weight, weight};
}

/// Pointing: low and high to the side the target is on, or with the hand that is free, blended by how high it is
void SetPointing(CreatureObjectAction& action, const Transform& transform, const Creature& body, const CreatureHeldObject* held)
{
	const auto local = ToLocal(transform, action.point);
	// The left side is mirrored in the meshes' space: the right is +x
	const bool right = held != nullptr ? held->mirrored : local.x >= 0.0f;
	const auto height = creature_throw::k_HeightAtSizeOne * body.size * k_PointFromHeightShare;
	const auto elevation = std::atan2(local.y - height, std::max(glm::length(glm::vec2(local.x, local.z)), k_Tiny));
	const auto high = std::clamp(elevation / k_PointHighRadians, 0.0f, 1.0f);
	const std::array<size_t, 2> animations {
	    creature_object_actions::k_PointAnimations.at(right ? 1 : 0),
	    creature_object_actions::k_PointAnimations.at(right ? 3 : 2),
	};
	const std::array<float, 2> weights {1.0f - high, high};
	SetSlots(action, animations, weights);
	action.mirrored = false;
}

/// A vector along the ground turned by an angle
glm::vec2 Turned(glm::vec2 v, float angle)
{
	const auto c = std::cos(angle);
	const auto s = std::sin(angle);
	return {(v.x * c) - (v.y * s), (v.x * s) + (v.y * c)};
}

/// The point to turn to face so that a target lies in the direction of the middle of the reach, on whichever side
/// needs the smaller turn
glm::vec2 FacingToReach(const Transform& transform, const creature_reach::Points& reach, const glm::vec3& target)
{
	const auto from = glm::xz(transform.position);
	const auto towards = glm::xz(target) - from;
	const auto centre = creature_reach::Centre(reach);
	const glm::vec2 ahead {0.0f, -1.0f};
	const auto aheadInWorld = glm::xz(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
	std::optional<glm::vec2> best;
	float bestTurn = 0.0f;
	for (const auto side : {1.0f, -1.0f})
	{
		const glm::vec2 middle {centre.x * side, centre.z};
		if (glm::length(middle) < k_Tiny)
		{
			continue;
		}
		// How far round from ahead the middle of the reach is, and so which way to face for it to lie on the target
		const auto offset = std::atan2((ahead.x * middle.y) - (ahead.y * middle.x), glm::dot(ahead, middle));
		const auto facing = Turned(towards, -offset);
		const auto turn = std::acos(std::clamp(glm::dot(glm::normalize(facing), glm::normalize(aheadInWorld)), -1.0f, 1.0f));
		if (!best.has_value() || turn < bestTurn)
		{
			best = facing;
			bestTurn = turn;
		}
	}
	return from + best.value_or(towards);
}

/// One game turn of walking or turning up to what the creature acts on
void Approach(entt::entity creature, CreatureObjectAction& action, const Creature& body, const Transform& transform,
              CreatureAnimation& animation, const CreatureHeldObject* held)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	if (locomotion.IsMoving(creature))
	{
		return;
	}
	const auto* points = PointsOf(body);
	if (points == nullptr)
	{
		Fail(action, "its species has nothing to act with");
		return;
	}

	if (action.kind == Kind::Throw || action.kind == Kind::Point)
	{
		const auto limit = action.kind == Kind::Throw ? creature_object_actions::k_ThrowTurnRadians
		                                              : creature_object_actions::k_PointTurnRadians;
		if (BearingTo(transform, action.point) > limit && action.attempts < k_TurnAttempts)
		{
			++action.attempts;
			locomotion.TurnToFace(creature, glm::xz(action.point));
			return;
		}
		if (action.kind == Kind::Throw)
		{
			action.flightSeconds = creature_throw::FlightTime(glm::distance(transform.position, action.point));
			const auto weights = ThrowWeights(creature, transform, *points, action.point);
			const std::array<size_t, 2> animations {creature_throw::k_HurlFlat, creature_throw::k_HurlHigh};
			SetSlots(action, animations, weights);
			action.mirrored = held != nullptr && held->mirrored;
			action.eventMs = points->throwMs;
		}
		else
		{
			SetPointing(action, transform, body, held);
			action.holdMs = creature_object_actions::k_PointSeconds * 1000.0f;
		}
		BeginPlaying(creature, action, animation);
		return;
	}

	// Walking up to something and reaching for it
	const auto* at = action.target.has_value() && ecs::IsAvailable(*action.target)
	                     ? registry.TryGet<const Transform>(*action.target)
	                     : nullptr;
	if (at == nullptr)
	{
		Fail(action, "it has gone");
		return;
	}
	if (!action.reach.has_value())
	{
		const bool pickingUp = action.kind == Kind::PickUp;
		action.eventMs = pickingUp ? points->pickUpMs : points->destroyMs;
		action.reach =
		    MeasureReach(creature, transform, *points,
		                 pickingUp ? creature_reach::k_PickUpAnimations : creature_reach::k_DestroyAnimations, action.eventMs);
		if (!action.reach.has_value())
		{
			// Not posed yet: it tries again next turn
			return;
		}
		action.maxReach = creature_reach::MaxReach(*action.reach);
	}
	const auto local = ToLocal(transform, at->position);
	const auto reach = ReachFor(*action.reach, local);
	if (reach.blend.inRange)
	{
		const auto& animations =
		    action.kind == Kind::PickUp ? creature_reach::k_PickUpAnimations : creature_reach::k_DestroyAnimations;
		SetSlots(action, animations, reach.blend.weights);
		action.mirrored = reach.mirrored;
		BeginPlaying(creature, action, animation);
		return;
	}
	if (action.attempts >= k_ReachAttempts)
	{
		Fail(action, "it couldn't get in reach");
		return;
	}
	++action.attempts;
	const auto centre = creature_reach::Centre(*action.reach);
	const auto ideal = std::max(glm::length(glm::vec2(centre.x, centre.z)), k_Tiny);
	const auto distance = glm::length(glm::vec2(local.x, local.z));
	// Near enough, it only needs to turn so that it lies where its reach is middling
	if (std::abs(distance - ideal) <= ideal * k_TurnInReachShare)
	{
		locomotion.TurnToFace(creature, FacingToReach(transform, *action.reach, at->position));
		return;
	}
	using MoveResult = CreatureLocomotionSystemInterface::MoveResult;
	const auto result = locomotion.MoveTo(creature, glm::xz(at->position), CreatureLocomotionSystemInterface::Pace::Walk,
	                                      ideal * k_ApproachNear, ideal * k_ApproachFar);
	if (result != MoveResult::Started)
	{
		Fail(action, "navigation failed");
	}
}
} // namespace

bool CreatureObjectActionSystem::Start(entt::entity creature, CreatureObjectAction action)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation, Transform>(creature))
	{
		return false;
	}
	if (IsCatching(creature))
	{
		return false;
	}
	if (std::as_const(registry).AllOf<CreatureCatching>(creature))
	{
		registry.RemoveState<CreatureCatching>(creature);
	}
	const auto& body = registry.Get<const Creature>(creature);
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	action.status = Status::Running;
	action.phase = Phase::Approach;
	switch (creature_object_actions::HandsFor(action.kind))
	{
	case creature_object_actions::Hands::Holding:
		if (held == nullptr)
		{
			Fail(action, "it has nothing in its hand");
		}
		break;
	case creature_object_actions::Hands::Empty:
		if (held != nullptr)
		{
			Fail(action, "its hands are full");
		}
		break;
	case creature_object_actions::Hands::Either:
		break;
	}
	const auto* points = PointsOf(body);
	if (action.status == Status::Running && points == nullptr)
	{
		Fail(action, "its species has nothing to act with");
	}
	if (action.status == Status::Running && action.target.has_value())
	{
		if (!registry.Valid(*action.target) || !registry.AllOf<Transform>(*action.target))
		{
			Fail(action, "there is nothing there");
		}
		else if (action.kind == Kind::PickUp &&
		         (!CanPickUp(creature, *action.target) || registry.AllOf<HeldByCreature>(*action.target)))
		{
			Fail(action, "it can't be picked up");
		}
		else if (action.kind == Kind::Destroy && !CanDestroy(*action.target))
		{
			Fail(action, "it can't be knocked down");
		}
	}
	if (action.status == Status::Running && action.kind == Kind::Throw)
	{
		const auto& transform = registry.Get<const Transform>(creature);
		const auto ground = glm::distance(glm::xz(transform.position), glm::xz(action.point));
		if (!creature_throw::FarEnoughToThrow(ground, body.size))
		{
			Fail(action, "too close to throw at");
		}
	}

	if (action.status == Status::Running && points != nullptr)
	{
		// Acting on what it holds starts at once, with the hand it holds it in
		const auto single = [&](size_t animation, float eventMs) {
			const std::array<size_t, 1> animations {animation};
			const std::array<float, 1> weights {1.0f};
			SetSlots(action, animations, weights);
			action.mirrored = held != nullptr && held->mirrored;
			action.eventMs = eventMs;
		};
		bool now = true;
		switch (action.kind)
		{
		case Kind::PutDown:
			single(creature_throw::k_PutDown, points->putDownMs);
			break;
		case Kind::Discard:
			single(creature_throw::k_Discard, points->discardMs);
			break;
		case Kind::Lob:
		{
			// The game times the gentle lob by nothing of its own: it lets go as far through as tossing away does
			const auto discard = DurationOf(creature, creature_throw::k_Discard);
			const auto lob = DurationOf(creature, creature_throw::k_GentleLob);
			single(creature_throw::k_GentleLob, discard > 0.0f ? points->discardMs * lob / discard : points->discardMs);
			break;
		}
		case Kind::Eat:
			single(creature_throw::k_Eat, points->eatMs);
			break;
		case Kind::Keep:
			// The animation was chosen as the action was asked for
			single(action.animations.front(), 0.0f);
			action.eventDone = true;
			break;
		case Kind::TakeFromHand:
		{
			// It reaches with the hand it was asked to, whatever it holds
			const bool mirrored = action.mirrored;
			single(creature_giving::k_TakeAnimation, points->takeFromHandMs);
			action.mirrored = mirrored;
			break;
		}
		case Kind::PickUp:
		case Kind::Destroy:
		case Kind::Throw:
		case Kind::Point:
			now = false;
			break;
		}
		if (now)
		{
			BeginPlaying(creature, action, registry.Get<CreatureAnimation>(creature));
		}
		// The meal's energy comes as the eating starts, by the food value of what it holds (none: nothing)
		if (action.kind == Kind::Eat && held != nullptr && Locator::creaturePhysiologySystem::has_value())
		{
			Locator::creaturePhysiologySystem::value().Eat(creature, FoodValueOf(held->object).value_or(0.0f));
		}
	}
	const bool started = action.status == Status::Running;
	registry.AssignOrReplaceState<CreatureObjectAction>(creature, std::move(action));
	return started;
}

bool CreatureObjectActionSystem::TakeFromHand(entt::entity creature, entt::entity object, bool mirrored)
{
	return Start(creature, {.kind = Kind::TakeFromHand, .target = object, .mirrored = mirrored});
}

std::optional<CreatureObjectActionSystemInterface::TakingFromHand>
CreatureObjectActionSystem::GetTakingFromHand(entt::entity creature) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr || action->kind != Kind::TakeFromHand || action->phase != Phase::Playing ||
	    (action->status != Status::Running && action->status != Status::Contact) || !action->target.has_value())
	{
		return std::nullopt;
	}
	return TakingFromHand {.object = *action->target, .msFromTake = action->timeMs - action->eventMs};
}

std::optional<glm::vec3> CreatureObjectActionSystem::TakePoint(entt::entity creature, glm::vec3 facing) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(creature) || !registry.AllOf<Creature, Transform>(creature))
	{
		return std::nullopt;
	}
	const auto* points = PointsOf(registry.Get<const Creature>(creature));
	const auto& transform = registry.Get<const Transform>(creature);
	if (points == nullptr)
	{
		return std::nullopt;
	}
	// The right hand at the moment of taking, with the creature turned to face the point
	const auto hand = HandAt(creature, transform, *points, creature_giving::k_TakeAnimation, points->takeFromHandMs);
	if (!hand.has_value())
	{
		return std::nullopt;
	}
	const glm::vec2 towards {facing.x - transform.position.x, facing.z - transform.position.z};
	const auto heading = glm::length(towards) > 0.0f ? std::atan2(-towards.x, -towards.y) : 0.0f;
	return transform.position + glm::vec3(glm::eulerAngleY(heading) * glm::vec4(*hand, 0.0f));
}

bool CreatureObjectActionSystem::PickUp(entt::entity creature, entt::entity object)
{
	return Start(creature, {.kind = Kind::PickUp, .target = object});
}

bool CreatureObjectActionSystem::PickUpPortion(entt::entity creature, entt::entity source, PotInfo kind)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, Transform>(creature) || !registry.Valid(source) ||
	    !registry.AllOf<Transform>(source))
	{
		// as picking up a thing that is not there
		return Start(creature, {.kind = Kind::PickUp, .target = source});
	}
	// A portion of its kind, at most of its drawn size's worth, made towards the source at nearly the creature's reach,
	// and as big as its drawn size makes it
	const float drawnSize = creature_morph::ClampScale(ecs::creature_pose::DrawnSize(std::as_const(registry), creature));
	const auto& from = registry.Get<const Transform>(creature).position;
	const auto& to = registry.Get<const Transform>(source).position;
	const auto at = creature_eating::PortionPoint(glm::xz(from), glm::xz(to), PickUpReach(creature).value_or(0.0f));
	const float ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : from.y;
	const auto amount =
	    creature_eating::PortionAmount(drawnSize, ecs::object_resources::GetResource(source, ResourceType::Food));
	// (pending) the game also gives the portion the town nearest to it
	const auto portion =
	    ecs::archetypes::PotArchetype::Create(glm::vec3(at.x, ground, at.y), 0.0f, kind, static_cast<int32_t>(amount), true);
	if (portion == entt::null)
	{
		return Start(creature, {.kind = Kind::PickUp, .target = entt::null});
	}
	registry.Get<Transform>(portion).scale = glm::vec3(creature_eating::PortionScale(drawnSize));
	registry.SetDirty();
	return Start(creature, {.kind = Kind::PickUp, .target = portion, .source = source});
}

bool CreatureObjectActionSystem::PutDown(entt::entity creature)
{
	return Start(creature, {.kind = Kind::PutDown});
}

bool CreatureObjectActionSystem::Discard(entt::entity creature)
{
	return Start(creature, {.kind = Kind::Discard});
}

bool CreatureObjectActionSystem::Lob(entt::entity creature)
{
	return Start(creature, {.kind = Kind::Lob});
}

bool CreatureObjectActionSystem::EatHeld(entt::entity creature)
{
	// Whatever it holds is eaten, food or not: what is not food it finds out about when the meal ends
	return Start(creature, {.kind = Kind::Eat, .target = GetHeld(creature)});
}

bool CreatureObjectActionSystem::Keep(entt::entity creature, size_t animation)
{
	CreatureObjectAction action {.kind = Kind::Keep};
	action.animations.front() = animation;
	return Start(creature, std::move(action));
}

bool CreatureObjectActionSystem::Throw(entt::entity creature, const glm::vec3& target)
{
	return Start(creature, {.kind = Kind::Throw, .point = target});
}

bool CreatureObjectActionSystem::Destroy(entt::entity creature, entt::entity target)
{
	return Start(creature, {.kind = Kind::Destroy, .target = target});
}

bool CreatureObjectActionSystem::PointAt(entt::entity creature, const glm::vec3& point)
{
	return Start(creature, {.kind = Kind::Point, .point = point});
}

void CreatureObjectActionSystem::Cancel(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(creature) && std::as_const(registry).AllOf<CreatureObjectAction>(creature))
	{
		registry.RemoveState<CreatureObjectAction>(creature);
		if (auto* animation = registry.TryGet<CreatureAnimation>(creature))
		{
			animation->slots.clear();
		}
	}
}

void CreatureObjectActionSystem::Drop(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto held = GetHeld(creature);
	if (!held.has_value())
	{
		return;
	}
	const auto* at = registry.TryGet<const Transform>(*held);
	Release(creature, at != nullptr ? at->position : registry.Get<const Transform>(creature).position, glm::vec3(0.0f));
}

void CreatureObjectActionSystem::Release(entt::entity creature, const glm::vec3& position, const glm::vec3& velocity,
                                         bool atTarget)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	if (held == nullptr)
	{
		return;
	}
	const auto object = held->object;
	registry.RemoveState<CreatureHeldObject>(creature);
	ReleaseHeld(object, position, velocity, creature, atTarget);
}

void CreatureObjectActionSystem::ReleaseHeld(entt::entity object, const glm::vec3& position, const glm::vec3& velocity,
                                             entt::entity creature, bool atTarget)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	const auto& lookup = std::as_const(registry);
	if (lookup.AllOf<HeldByCreature>(object))
	{
		registry.RemoveState<HeldByCreature>(object);
	}
	if (lookup.AllOf<PhysicsDrawPose>(object))
	{
		registry.Remove<PhysicsDrawPose>(object);
	}
	auto* transform = registry.TryGet<Transform>(object);
	if (transform != nullptr)
	{
		transform->position = position;
	}
	ecs::NotifyTeleported(object);
	// It flies from where it was let go of; the physics puts it back on the map as it comes to rest
	bool placed = false;
	if (transform != nullptr)
	{
		const auto release = ecs::creature_physics::ReleaseOf(atTarget, creature);
		switch (release)
		{
		case ecs::creature_physics::Release::AtTarget:
			placed = Locator::dynamicsSystem::value().AddObject(object, velocity, glm::vec3(0.0f), creature, false) != nullptr;
			break;
		case ecs::creature_physics::Release::LetGo:
			placed =
			    ecs::physics::from_hand::InitialisePhysicsFromHand(object, velocity, false, std::nullopt, true).has_value();
			break;
		}
		if (!placed)
		{
			// No body was made for it. Let go, a thing a script made immovable hangs where it was let go, as from the
			// player's hand; anything else is put down where it is
			if (release == ecs::creature_physics::Release::LetGo)
			{
				ecs::physics::from_hand::LetGoWithoutBody(object);
			}
			else
			{
				ecs::physics::from_hand::PlaceWithoutBody(object);
			}
			placed = true;
		}
	}
	if (!placed)
	{
		ecs::map_cells::InsertMapObject(object);
	}
	// Out of the hand
	ecs::villager_physics::InterfaceSetOutMagicHand(object);
}

CreatureObjectActionSystem::State CreatureObjectActionSystem::GetState(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr)
	{
		// A catch's hands' part is over as soon as it starts: its body carries it on by itself
		return registry.Valid(creature) && registry.AllOf<CreatureCatching>(creature) ? State::Done : State::Idle;
	}
	switch (action->status)
	{
	case Status::Running:
	case Status::Contact:
		return State::Busy;
	case Status::Done:
		return State::Done;
	case Status::Failed:
		break;
	}
	return State::Failed;
}

std::optional<float> CreatureObjectActionSystem::GetProgress(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr || action->phase != Phase::Playing ||
	    (action->status != Status::Running && action->status != Status::Contact))
	{
		return std::nullopt;
	}
	if (action->holdMs > 0.0f || action->durationMs <= 0.0f)
	{
		return 0.0f;
	}
	return std::clamp(action->timeMs / action->durationMs, 0.0f, 1.0f);
}

std::optional<entt::entity> CreatureObjectActionSystem::GetHeld(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* held = registry.Valid(creature) ? registry.TryGet<const CreatureHeldObject>(creature) : nullptr;
	return held != nullptr && registry.Valid(held->object) ? std::optional(held->object) : std::nullopt;
}

std::optional<float> CreatureObjectActionSystem::FoodValueOf(entt::entity object) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::infoConstants::has_value() || !ecs::IsAvailable(object))
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	float value = 0.0f;
	if (registry.AllOf<Villager>(object))
	{
		const auto* villager = ecs::VillagerInfoOf(object);
		value = villager != nullptr ? villager->foodValue : 0.0f;
	}
	else if (const auto* animal = registry.TryGet<const Animal>(object))
	{
		const auto kind = static_cast<size_t>(animal->type);
		value = kind < info.animal.size() ? info.animal.at(kind).foodValue : 0.0f;
	}
	else if (const auto* mobile = registry.TryGet<const MobileObject>(object))
	{
		const auto kind = static_cast<size_t>(mobile->type);
		value = kind < info.mobileObject.size() ? info.mobileObject.at(kind).foodValue : 0.0f;
	}
	else if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		// A pot or pile of food is worth as much as the food in it. A storage pit's pile is eaten from where it lies,
		// which isn't done here.
		const auto kind = static_cast<size_t>(pot->type);
		const bool food = kind < info.pot.size() && info.pot.at(kind).resourceType == ResourceType::Food &&
		                  pot->type != PotInfo::StoragePitFoodPile;
		value = food ? static_cast<float>(pot->amount) : 0.0f;
	}
	return value > 0.0f ? std::optional(value) : std::nullopt;
}

bool CreatureObjectActionSystem::CanPickUp(entt::entity creature, entt::entity object) const
{
	return PickUpTest(creature, object, false);
}

bool CreatureObjectActionSystem::CanPickUpIgnoringMap(entt::entity creature, entt::entity object) const
{
	return PickUpTest(creature, object, true);
}

bool CreatureObjectActionSystem::PickUpTest(entt::entity creature, entt::entity object, bool ignoringMap)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(object) || !registry.AllOf<Transform>(object))
	{
		return false;
	}
	// A bird by its own test, by its height above the land; another animal by the test of things in general
	if (const auto* animal = registry.TryGet<const Animal>(object); animal != nullptr)
	{
		if (ecs::animal_ai::IsDoveClass(animal->type))
		{
			return Locator::animalSystem::has_value() && Locator::animalSystem::value().CanBePickedUpByCreature(object);
		}
		if (!Locator::infoConstants::has_value())
		{
			return false;
		}
		const auto kind = static_cast<size_t>(animal->type);
		const auto& info = Locator::infoConstants::value();
		return PassesPickUpTest(creature, object, kind < info.animal.size() ? info.animal.at(kind).weight : 0.0f, false,
		                        !ecs::object_flags::IsCannotBePickedUp(object), ignoringMap);
	}
	// A villager by the same test: not one worshipping at its own player's site, nor one inside its home
	if (registry.AllOf<Villager>(object))
	{
		if (!Locator::infoConstants::has_value())
		{
			return false;
		}
		const auto* villagerInfo = ecs::VillagerInfoOf(object);
		const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
		const bool own = body != nullptr && creature_pick_up::IsOwnWorshipper(ecs::villager::GetFinalState(object) ==
		                                                                          VillagerStates::WorshippingAtWorshipSite,
		                                                                      ecs::villager::GetPlayerOf(object), body->owner);
		return PassesPickUpTest(
		    creature, object, villagerInfo != nullptr ? villagerInfo->weight : 0.0f, own,
		    creature_pick_up::VillagerAllows(registry.AllOf<AtHome>(object), ecs::object_flags::IsCannotBePickedUp(object)),
		    ignoringMap);
	}
	// A mobile object by the same test, by the class the game makes it as: a creed (the creed and the eyes) always, a
	// scaffold only while its building may still be moved, and by the scaffolds' weight
	if (const auto* mobile = registry.TryGet<const MobileObject>(object); mobile != nullptr)
	{
		if (!Locator::infoConstants::has_value())
		{
			return false;
		}
		const auto& info = Locator::infoConstants::value();
		const auto kind = static_cast<size_t>(mobile->type);
		const auto* row = kind < info.mobileObject.size() ? &info.mobileObject.at(kind) : nullptr;
		if (row != nullptr && row->mobileType == MobileObjectInfo::Creed)
		{
			return true;
		}
		const bool allows = !ecs::object_flags::IsCannotBePickedUp(object);
		if (registry.AllOf<Scaffold>(object))
		{
			return ecs::scaffolds::ValidForPlaceInHand(object) &&
			       PassesPickUpTest(creature, object, info.scaffold.weight, false, allows, ignoringMap);
		}
		return PassesPickUpTest(creature, object, row != nullptr ? row->weight : 0.0f, false, allows, ignoringMap);
	}
	// A pot by the same test, by its row's weight; a pile by its own test alone
	if (const auto* pot = registry.TryGet<const Pot>(object); pot != nullptr)
	{
		if (!Locator::infoConstants::has_value())
		{
			return false;
		}
		const auto& info = Locator::infoConstants::value();
		const auto kind = static_cast<size_t>(pot->type);
		if (kind >= info.pot.size())
		{
			return false;
		}
		const auto& row = info.pot.at(kind);
		const bool cannot = ecs::object_flags::IsCannotBePickedUp(object);
		if (row.potType == PotType::Pot)
		{
			return PassesPickUpTest(creature, object, row.weight, false, !cannot, ignoringMap);
		}
		return creature_pick_up::PileAllows(cannot, row.canBecomeAPhysicsObject != 0);
	}
	return false;
}

std::optional<float> CreatureObjectActionSystem::PickUpReach(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation, Transform>(creature) ||
	    !Locator::creatureAnimationSystem::has_value())
	{
		return std::nullopt;
	}
	// The hand's points in the four pick-up animations, at size one, measured once for the body as it is built: the
	// body posed first when it has not been yet, as the game poses it when its test asks
	auto& animations = Locator::creatureAnimationSystem::value();
	if (!animations.EnsurePosed(creature))
	{
		return std::nullopt;
	}
	const auto revision = registry.Get<const CreatureAnimation>(creature).builtRevision;
	const auto* kept = registry.TryGet<const CreaturePickUpReach>(creature);
	if (kept == nullptr || kept->bodyRevision != revision)
	{
		const auto* points = PointsOf(registry.Get<const Creature>(creature));
		if (points == nullptr)
		{
			return std::nullopt;
		}
		auto unit = registry.Get<const Transform>(creature);
		unit.scale = glm::vec3(1.0f);
		const auto measured = MeasureReach(creature, unit, *points, creature_reach::k_PickUpAnimations, points->pickUpMs);
		if (!measured.has_value())
		{
			return std::nullopt;
		}
		kept = &registry.AssignOrReplace<CreaturePickUpReach>(
		    creature, CreaturePickUpReach {.points = *measured, .bodyRevision = revision});
	}
	// scaled to the size the body is drawn at
	return creature_reach::MaxReach3D(kept->points, ecs::creature_pose::DrawnScale(std::as_const(registry), creature).x);
}

bool CreatureObjectActionSystem::PassesPickUpTest(entt::entity creature, entt::entity object, float kindWeight,
                                                  bool ownWorshipper, bool thingAllows, bool ignoringMap)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	const auto reach = body != nullptr ? PickUpReach(creature) : std::nullopt;
	if (body == nullptr || !reach.has_value())
	{
		// (openblack) without the species' meshes, as in tests, the creature has no reach to measure
		return false;
	}
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	const auto* leash = registry.TryGet<const CreatureLeash>(creature);
	const auto* morph = registry.TryGet<const CreatureMorph>(creature);
	// the weight the creature's body is to have: its size drawn, and the thin to fat and weak to strong it is becoming
	const auto target = morph != nullptr
	                        ? creature_morph::FromAttributes(body->alignment, morph->shownFatness, body->strength,
	                                                         archetypes::CreatureArchetype::SpeciesStrength(body->species))
	                        : creature_morph::Morph {};
	const float drawnSize = creature_morph::ClampScale(ecs::creature_pose::DrawnSize(std::as_const(registry), creature));
	const creature_pick_up::Check check {
	    .computerPlayer = magic::players::ReadMagicOf(body->owner).playerType == PlayerMagic::k_ComputerPlayerType,
	    .creatureScripted = ecs::script_held::IsControlledByScript(creature),
	    .thingInScript = ecs::script_held::IsInScript(object),
	    .ownWorshipper = ownWorshipper,
	    .inMap = ignoringMap || ecs::map_cells::IsObjectInMap(object),
	    .inThisHand = held != nullptr && held->object == object,
	    .leashTiedTo = leash != nullptr && leash->worn.has_value() && leash->worn->tiedTo == object,
	    .thingRadius = ecs::object::Get2DRadius(object),
	    .creatureRadius = ecs::object::Get2DRadius(creature),
	    .reach = *reach,
	    .thingWeight = creature_pick_up::ThingWeight(ecs::object::GetScale(object), kindWeight),
	    .creatureWeight = openblack::physics::living::CreatureMass(drawnSize, target.thinFat, target.weakStrong),
	    .thingAllows = thingAllows,
	};
	return creature_pick_up::CreatureMayPickUp(check);
}

bool CreatureObjectActionSystem::CanDestroy(entt::entity target) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return ecs::IsAvailable(target) && registry.AllOf<Transform>(target) && registry.AnyOf<Tree, Abode, MobileObject>(target);
}

void CreatureObjectActionSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!Locator::creatureLocomotionSystem::has_value() || !Locator::creatureAnimationSystem::has_value())
	{
		return;
	}

	// What is held by a creature that is gone, or no longer holds it, falls where it is
	std::vector<entt::entity> loose;
	lookup.Each<const HeldByCreature>([&](entt::entity entity, const HeldByCreature& by) {
		const auto* held = lookup.Valid(by.creature) ? lookup.TryGet<const CreatureHeldObject>(by.creature) : nullptr;
		if (held == nullptr || held->object != entity)
		{
			loose.push_back(entity);
		}
	});
	for (const auto entity : loose)
	{
		const auto* at = lookup.TryGet<const Transform>(entity);
		ReleaseHeld(entity, at != nullptr ? at->position : glm::vec3(0.0f), glm::vec3(0.0f));
	}
	std::vector<entt::entity> emptyHanded;
	lookup.Each<const CreatureHeldObject>([&](entt::entity entity, const CreatureHeldObject& held) {
		if (!ecs::IsAvailable(held.object))
		{
			emptyHanded.push_back(entity);
		}
	});
	for (const auto entity : emptyHanded)
	{
		registry.RemoveState<CreatureHeldObject>(entity);
	}

	std::vector<entt::entity> acting;
	lookup.Each<const Creature, const Transform, const CreatureAnimation, const CreatureObjectAction>(
	    [&acting](entt::entity entity, const auto&...) { acting.push_back(entity); });

	// Walking or turning up to what they act on
	for (const auto entity : acting)
	{
		auto& action = registry.Get<CreatureObjectAction>(entity);
		if (action.status == Status::Running && action.phase == Phase::Approach)
		{
			Approach(entity, action, lookup.Get<Creature>(entity), lookup.Get<Transform>(entity),
			         registry.Get<CreatureAnimation>(entity), lookup.TryGet<const CreatureHeldObject>(entity));
		}
	}

	// The actions play on by a turn
	for (const auto entity : acting)
	{
		if (!lookup.AllOf<CreatureObjectAction>(entity))
		{
			continue;
		}
		auto& action = registry.Get<CreatureObjectAction>(entity);
		if (action.phase != Phase::Playing || (action.status != Status::Running && action.status != Status::Contact))
		{
			continue;
		}
		const auto& transform = lookup.Get<Transform>(entity);
		// at the animation rate of the size it is drawn at (the pen's in its pen)
		action.timeMs += k_TurnMs * creature_layers::PlaybackRate(ecs::creature_pose::DrawnSize(lookup, entity));

		// Until it has hold, the reach follows what it reaches for, and fails when it gets out of reach
		if ((action.kind == Kind::PickUp || action.kind == Kind::Destroy) && !action.eventDone && action.reach.has_value())
		{
			const auto* at = action.target.has_value() && ecs::IsAvailable(*action.target)
			                     ? lookup.TryGet<const Transform>(*action.target)
			                     : nullptr;
			if (at == nullptr)
			{
				Fail(action, "it has gone");
				continue;
			}
			const auto local = ToLocal(transform, at->position);
			const auto reach =
			    creature_reach::Solve(action.mirrored ? creature_reach::Mirrored(*action.reach) : *action.reach, local);
			if (!reach.inRange)
			{
				Fail(action, "it got out of reach");
				continue;
			}
			action.weights = reach.weights;
		}

		if (action.holdMs > 0.0f)
		{
			action.holdMs -= k_TurnMs;
			if (action.holdMs <= 0.0f)
			{
				action.status = Status::Done;
			}
		}
		else if (action.timeMs >= action.durationMs && (action.eventDone || action.eventMs > action.durationMs))
		{
			action.status = Status::Done;
		}
	}

	// The moments things are taken hold of, knocked down, let go of and eaten
	for (const auto entity : acting)
	{
		const auto* action = lookup.TryGet<const CreatureObjectAction>(entity);
		if (action != nullptr && action->phase == Phase::Playing && action->status == Status::Running && !action->eventDone &&
		    action->timeMs >= action->eventMs)
		{
			Moment(entity);
		}
	}

	// The meals whose eating is over
	for (const auto entity : acting)
	{
		auto* action = registry.TryGet<CreatureObjectAction>(entity);
		if (action != nullptr && action->kind == Kind::Eat && action->status == Status::Done && action->meal.has_value())
		{
			const auto meal = *std::exchange(action->meal, std::nullopt);
			EndMeal(entity, meal);
		}
	}

	ProcessCatches();

	// What is held rides in the hand, turned with it; out of the map cells while it is held
	lookup.Each<const Creature, const Transform, const CreatureAnimation, const CreatureHeldObject>(
	    [&](entt::entity entity, const Creature&, const Transform& transform, const CreatureAnimation& animation,
	        const CreatureHeldObject& held) {
		    if (!ecs::IsAvailable(held.object) || !lookup.AllOf<Transform>(held.object) || animation.boneMatrices.empty())
		    {
			    return;
		    }
		    auto& at = registry.Get<Transform>(held.object);
		    // Held by its middle in the palm, turned with the creature. The palm is the one of the body as posed for the
		    // last turn, in the world, where what it holds is placed as it lets go of it; a body not posed for a turn
		    // yet, or posed with other bones, as the frame poses it under its Transform
		    at.rotation = transform.rotation * held.rotation;
		    const auto* pose = lookup.TryGet<const CreatureTurnPose>(entity);
		    const auto palm = pose != nullptr && pose->current.size() == animation.boneMatrices.size()
		                          ? PalmOf(held.bone, animation, pose->current, glm::mat4(1.0f))
		                          : PalmOf(held.bone, animation, PlacementOf(transform));
		    at.position = palm - (at.rotation * held.middle);
	    });

	// Carrying something heavy makes it stronger as it walks about
	std::vector<entt::entity> bodies;
	lookup.Each<const Creature, const CreatureNeeds>(
	    [&bodies](entt::entity entity, const auto&...) { bodies.push_back(entity); });
	for (const auto entity : bodies)
	{
		auto& needs = registry.Get<CreatureNeeds>(entity);
		needs.carriedWeight.reset();
		const auto held = GetHeld(entity);
		if (!held.has_value())
		{
			continue;
		}
		if (const auto weight = WeightOf(lookup, *held))
		{
			needs.carriedWeight =
			    openblack::physics::living::CarriedShare(*weight, ecs::creature_physics::Weight(lookup, entity));
		}
	}

	// The town sees what the creature does with fear or respect, and keeps that view a while after
	std::vector<entt::entity> creatures;
	lookup.Each<const Creature>([&creatures](entt::entity entity, const Creature&) { creatures.push_back(entity); });
	for (const auto entity : creatures)
	{
		const auto* action = lookup.TryGet<const CreatureObjectAction>(entity);
		const bool busy = action != nullptr && (action->status == Status::Running || action->status == Status::Contact);
		const auto held = GetHeld(entity);
		const bool holdingVillager = IsVillager(lookup, held);
		auto attitude = creature_object_actions::TownAttitude::None;
		if (busy)
		{
			attitude = creature_object_actions::AttitudeTo(action->kind, IsVillager(lookup, action->target), holdingVillager);
		}
		else if (holdingVillager)
		{
			attitude = creature_object_actions::TownAttitude::Fear;
		}
		if (attitude != creature_object_actions::TownAttitude::None)
		{
			registry.AssignOrReplaceState<CreatureTownAttitude>(
			    entity,
			    CreatureTownAttitude {.attitude = attitude, .secondsLeft = creature_object_actions::AttitudeSeconds(attitude)});
		}
		else if (lookup.AllOf<CreatureTownAttitude>(entity))
		{
			auto& seen = registry.Get<CreatureTownAttitude>(entity);
			if (seen.attitude != creature_object_actions::TownAttitude::None)
			{
				seen.secondsLeft -= k_TurnSeconds;
				if (seen.secondsLeft <= 0.0f)
				{
					seen = {};
				}
			}
		}
	}
}

namespace
{
/// The creature's hand takes hold of a thing at the palm: taken out of the physics, into the hand and off the map, as the
/// player's hand takes things, and drawn in the palm by its middle at once
void TakeIntoHand(ecs::Registry& registry, entt::entity creature, entt::entity object, bool mirrored, glm::vec3 handPosition,
                  const CreatureRig::ActionPoints& points, const CreatureAnimation& animation, const Transform& transform)
{
	const auto& lookup = std::as_const(registry);
	const auto bone = HandBone(points, animation, mirrored);
	const auto middle = MiddleOf(registry, object);
	const auto rotation = glm::transpose(transform.rotation) * lookup.Get<Transform>(object).rotation;
	// Taken out of the physics, into the hand and off the map, as the player's hand takes things
	Locator::dynamicsSystem::value().RemoveObject(object);
	ecs::villager_physics::InterfaceSetInMagicHand(object);
	// an artefact leaves its town, and the creature's player owns it
	ecs::object_physics::ArtefactTaken(object, lookup.Get<Creature>(creature).owner);
	if (lookup.AllOf<Pot>(object))
	{
		ecs::animal_ai::RemovePotReaction(object);
	}
	if (ecs::map_cells::IsObjectInMap(object))
	{
		ecs::map_cells::RemoveMapObject(object);
	}
	if (lookup.AllOf<MorphWithTerrain>(object))
	{
		registry.Remove<MorphWithTerrain>(object);
	}
	registry.AssignOrReplaceState<CreatureHeldObject>(
	    creature,
	    CreatureHeldObject {.object = object, .mirrored = mirrored, .bone = bone, .rotation = rotation, .middle = middle});
	registry.AssignOrReplaceState<HeldByCreature>(object, HeldByCreature {.creature = creature});
	// Into the palm, by its middle, drawn there at once
	auto& at = registry.Get<Transform>(object);
	at.rotation = transform.rotation * rotation;
	at.position = handPosition - (at.rotation * middle);
	ecs::NotifyTeleported(object);
}
} // namespace

void CreatureObjectActionSystem::Moment(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	auto& action = registry.Get<CreatureObjectAction>(creature);
	const auto& body = lookup.Get<Creature>(creature);
	const auto& transform = lookup.Get<Transform>(creature);
	const auto& animation = lookup.Get<CreatureAnimation>(creature);
	const auto* points = PointsOf(body);
	action.eventDone = true;
	if (points == nullptr)
	{
		return;
	}
	// The palm as the animations have it at the moment, else as the body was last drawn
	const auto handPosition =
	    PalmAtMoment(creature, action, *points, animation, transform)
	        .value_or(animation.boneMatrices.empty()
	                      ? transform.position
	                      : PalmOf(HandBone(*points, animation, action.mirrored), animation, PlacementOf(transform)));
	switch (action.kind)
	{
	case Kind::PickUp:
	{
		const auto object = *action.target;
		if (!ecs::IsAvailable(object) || lookup.AllOf<HeldByCreature>(object) || !lookup.AllOf<Transform>(object))
		{
			Fail(action, "it has gone");
			break;
		}
		action.status = Status::Contact;
		TakeIntoHand(registry, creature, object, action.mirrored, handPosition, *points, animation, transform);
		// A portion's source loses what the portion holds, when it is a pot or a pile
		if (action.source.has_value() && ecs::IsAvailable(*action.source) && lookup.AllOf<Pot>(*action.source) &&
		    lookup.AllOf<Pot>(object))
		{
			const auto type = ecs::object_resources::GetResourceType(object);
			static_cast<void>(
			    ecs::object_resources::RemoveResource(*action.source, type, ecs::object_resources::GetResource(object, type)));
		}
		break;
	}
	case Kind::Destroy:
	{
		const auto target = *action.target;
		action.status = Status::Contact;
		if (!ecs::IsAvailable(target))
		{
			break;
		}
		// (approximate) homes take a blow as a thrown thing gives one; trees are felled; things are knocked to pieces
		if (lookup.AllOf<Abode>(target))
		{
			ecs::abodes::OnPhysicalDamage(
			    target, {.remaining = std::nullopt,
			             .hitter = creature,
			             .player = body.owner != PlayerNames::NEUTRAL ? std::optional(body.owner) : std::nullopt,
			             .byCreature = true});
		}
		else if (lookup.AllOf<Tree>(target))
		{
			ecs::FellTree(target, creature);
		}
		else
		{
			ecs::ToBeDeleted(target);
		}
		break;
	}
	case Kind::PutDown:
		Release(creature, handPosition, glm::vec3(0.0f));
		break;
	case Kind::Discard:
	case Kind::Lob:
	{
		const auto animationIndex = action.animations.front();
		const auto now = HandAt(creature, transform, *points, animationIndex, action.eventMs);
		const auto before =
		    HandAt(creature, transform, *points, animationIndex, action.eventMs - creature_throw::k_HandSpeedSpanMs);
		const auto velocity =
		    now.has_value() && before.has_value()
		        ? creature_throw::TossVelocity(creature_throw::HandVelocity(*now, *before, creature_throw::k_HandSpeedSpanMs),
		                                       action.mirrored, transform.rotation, creature_throw::k_DiscardSpeedShare)
		        : glm::vec3(0.0f);
		Release(creature, handPosition, velocity);
		break;
	}
	case Kind::Throw:
		Release(creature, handPosition,
		        creature_throw::ReleaseVelocity(action.point, handPosition, std::max(action.flightSeconds, k_Tiny)), true);
		break;
	case Kind::Eat:
	{
		const auto held = GetHeld(creature);
		if (!held.has_value())
		{
			break;
		}
		// The hand lets go of it as it is eaten: neither in the hand nor on the land, and not drawn, until the eating
		// ends and it is used up
		registry.RemoveState<CreatureHeldObject>(creature);
		if (lookup.AllOf<HeldByCreature>(*held))
		{
			registry.RemoveState<HeldByCreature>(*held);
		}
		registry.AssignOrReplace<NotDrawn>(*held);
		action.meal = *held;
		break;
	}
	case Kind::TakeFromHand:
	{
		// It takes the thing only if the player's hand still gives it; otherwise its hand closes on nothing
		const auto object = *action.target;
		if (!ecs::IsAvailable(object) || lookup.AllOf<HeldByCreature>(object) || !lookup.AllOf<Transform>(object) ||
		    !Locator::handGrabSystem::has_value() || !Locator::handGrabSystem::value().HandOver(object))
		{
			Fail(action, "the hand no longer gives it");
			break;
		}
		action.status = Status::Contact;
		TakeIntoHand(registry, creature, object, action.mirrored, handPosition, *points, animation, transform);
		break;
	}
	case Kind::Keep:
	case Kind::Point:
		break;
	}
}

void CreatureObjectActionSystem::EndMeal(entt::entity creature, entt::entity meal) const
{
	const auto& lookup = std::as_const(Locator::entitiesRegistry::value());
	if (!lookup.Valid(meal))
	{
		return;
	}
	// What it was, read before it is used up
	const float value = FoodValueOf(meal).value_or(0.0f);
	const bool mushroom = IsMushroom(lookup, meal);
	const bool poisoned = IsPoisonedFood(lookup, meal);
	const bool pileFood = ecs::object::IsPileFood(meal);
	const auto type = static_cast<uint32_t>(ecs::map_cells::TypeOf(meal));
	// Used up, food or not
	if (ecs::IsAvailable(meal))
	{
		Consume(lookup, meal);
	}
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().EndMeal(creature, type, value,
		                                             creature_eating::JudgeMeal(value, mushroom, pileFood, poisoned));
	}
}

void CreatureObjectActionSystem::UpdateDraw(float turnFraction)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	const auto f = std::clamp(turnFraction, 0.0f, 1.0f);

	// The actions' animations, a share of the way through the turn on
	std::vector<entt::entity> acting;
	lookup.Each<const Creature, const CreatureAnimation, const CreatureObjectAction>(
	    [&acting](entt::entity entity, const auto&...) { acting.push_back(entity); });
	for (const auto entity : acting)
	{
		const auto& action = lookup.Get<CreatureObjectAction>(entity);
		auto& animation = registry.Get<CreatureAnimation>(entity);
		if (!IsDrawn(action))
		{
			continue;
		}
		animation.slots = SlotsAt(action, ecs::creature_pose::DrawnSize(lookup, entity), DurationsOf(entity, action), f);
	}
	std::vector<entt::entity> catching;
	lookup.Each<const Creature, const CreatureAnimation, const CreatureCatching>(
	    [&catching](entt::entity entity, const auto&...) { catching.push_back(entity); });
	for (const auto entity : catching)
	{
		const auto& catch_ = lookup.Get<CreatureCatching>(entity);
		if (catch_.stage != CreatureCatching::Stage::Over)
		{
			registry.Get<CreatureAnimation>(entity).slots = CatchSlotsAt(catch_, f);
		}
	}
}

void CreatureObjectActionSystem::AnimationAt(entt::entity creature, float turnFraction, CreatureAnimationInputs& inputs) const
{
	const auto& lookup = std::as_const(Locator::entitiesRegistry::value());
	if (const auto* catching = lookup.TryGet<const CreatureCatching>(creature);
	    catching != nullptr && catching->stage != CreatureCatching::Stage::Over)
	{
		inputs.slots = CatchSlotsAt(*catching, std::clamp(turnFraction, 0.0f, 1.0f));
		return;
	}
	const auto* action = lookup.TryGet<const CreatureObjectAction>(creature);
	if (action == nullptr || !lookup.AllOf<Creature, CreatureAnimation>(creature) || !IsDrawn(*action))
	{
		return;
	}
	inputs.slots = SlotsAt(*action, ecs::creature_pose::DrawnSize(lookup, creature), DurationsOf(creature, *action),
	                       std::clamp(turnFraction, 0.0f, 1.0f));
}

bool CreatureObjectActionSystem::IsDrawn(const CreatureObjectAction& action)
{
	return action.phase == Phase::Playing && (action.status == Status::Running || action.status == Status::Contact);
}

std::vector<CreatureAnimation::Slot> CreatureObjectActionSystem::SlotsAt(const CreatureObjectAction& action, float drawnSize,
                                                                         const std::array<float, 4>& durationsMs, float share)
{
	std::vector<CreatureAnimation::Slot> slots;
	const auto drawnMs = action.timeMs + (share * k_TurnMs * creature_layers::PlaybackRate(drawnSize));
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		const auto duration = durationsMs.at(i);
		// Pointing loops; everything else holds its last frame
		const auto time = action.holdMs > 0.0f && duration > 0.0f ? std::fmod(drawnMs, duration)
		                                                          : std::clamp(drawnMs, 0.0f, std::max(duration - 1.0f, 0.0f));
		slots.push_back({.animation = action.animations.at(i),
		                 .timeMs = time,
		                 .weight = action.weights.at(i),
		                 .mirrored = action.mirrored});
	}
	return slots;
}

void CreatureObjectActionSystem::UpdateHeldDraw()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);

	// What is held is drawn in the hand as the body is drawn this frame
	bool moved = false;
	std::vector<entt::entity> holding;
	lookup.Each<const Creature, const Transform, const CreatureAnimation, const CreatureHeldObject>(
	    [&holding](entt::entity entity, const auto&...) { holding.push_back(entity); });
	for (const auto entity : holding)
	{
		const auto& held = lookup.Get<CreatureHeldObject>(entity);
		const auto& animation = lookup.Get<CreatureAnimation>(entity);
		if (!ecs::IsAvailable(held.object) || !lookup.AllOf<Transform>(held.object) || animation.boneMatrices.empty())
		{
			continue;
		}
		// in the hand as the body is drawn this frame (its own drawn matrix and the bones it is drawn with: between its
		// turns), turned with the body; the object's Transform stays where the turn put it
		const auto rotation = ecs::creature_pose::DrawnPlacementOf(lookup, entity).rotation * held.rotation;
		const auto position =
		    PalmOf(held.bone, animation, creature_turn_pose::BonesDrawn(animation), ecs::DrawnBodyModel(lookup, entity)) -
		    (rotation * held.middle);
		auto& pose = lookup.AllOf<PhysicsDrawPose>(held.object) ? registry.Get<PhysicsDrawPose>(held.object)
		                                                        : registry.Assign<PhysicsDrawPose>(held.object);
		if (pose.position != position || pose.rotation != rotation)
		{
			pose.position = position;
			pose.rotation = rotation;
			moved = true;
		}
	}
	if (moved)
	{
		registry.SetDirty();
	}
}

namespace
{
using Catching = CreatureCatching;

/// The physics entry of the thing a creature catches, while the thing is in the physics
ecs::PhysicsEntry* FlightOf(entt::entity object)
{
	if (!Locator::dynamicsSystem::has_value() || object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		return nullptr;
	}
	return Locator::dynamicsSystem::value().Find(object);
}

/// How long one of the creature's animations lasts, in whole milliseconds
int32_t WholeDurationOf(entt::entity creature, size_t animation)
{
	return static_cast<int32_t>(DurationOf(creature, animation));
}

/// The catch is over: the body stands again
void EndCatch(Catching& catching, CreatureAnimation& animation)
{
	catching.stage = Catching::Stage::Over;
	animation.slots.clear();
}

/// A turn of the step across: the creature is carried by the turn's share of the step's travel, sized and turned as it
/// is, and put at the nearest place it may stand when the step took it too near ground it may not; the step plays on
void StepTurn(const ecs::Registry& registry, entt::entity creature, Catching& catching, const Transform& transform)
{
	const auto travel = Locator::creatureAnimationSystem::value().AnimationTravel(creature, creature_catch::k_CatchStep);
	if (travel.has_value() && catching.durationMs > 0 && Locator::creatureLocomotionSystem::has_value())
	{
		auto position =
		    transform.position + creature_catch::StepMove(*travel, transform.rotation,
		                                                  ecs::creature_pose::DrawnScale(registry, creature).x,
		                                                  static_cast<float>(catching.stepMs),
		                                                  static_cast<float>(catching.durationMs), catching.mirrored);
		if (!land_avoid::IsPosValid(position, creature_catch::k_StepClearance))
		{
			if (const auto valid = land_avoid::NearestValid(glm::xz(position), creature_catch::k_StepSearchClearance,
			                                                creature_catch::k_StepSearchDistance))
			{
				position.x = valid->x;
				position.z = valid->y;
			}
		}
		Locator::creatureLocomotionSystem::value().Place(creature, position);
	}
	catching.timeMs = creature_catch::AdvanceCyclic(catching.timeMs, catching.stepMs, catching.durationMs);
}

/// The caught thing is taken out of the physics into the hand it was caught with, as the creature's pick-up takes things
void TakeCaught(ecs::Registry& registry, entt::entity creature, entt::entity object, const Catching& catching,
                const CreatureRig::ActionPoints& points, const CreatureAnimation& animation, const Transform& transform)
{
	const auto& lookup = std::as_const(registry);
	const auto bone = HandBone(points, animation, catching.mirrored);
	const auto middle = MiddleOf(registry, object);
	const auto rotation = glm::transpose(transform.rotation) * lookup.Get<Transform>(object).rotation;
	Locator::dynamicsSystem::value().RemoveObject(object);
	ecs::villager_physics::InterfaceSetInMagicHand(object);
	// an artefact leaves its town, and the creature's player owns it
	ecs::object_physics::ArtefactTaken(object, lookup.Get<Creature>(creature).owner);
	if (ecs::map_cells::IsObjectInMap(object))
	{
		ecs::map_cells::RemoveMapObject(object);
	}
	if (lookup.AllOf<MorphWithTerrain>(object))
	{
		registry.Remove<MorphWithTerrain>(object);
	}
	registry.AssignOrReplaceState<CreatureHeldObject>(
	    creature, CreatureHeldObject {
	                  .object = object, .mirrored = catching.mirrored, .bone = bone, .rotation = rotation, .middle = middle});
	registry.AssignOrReplaceState<HeldByCreature>(object, HeldByCreature {.creature = creature});
	// Into the palm, by its middle, drawn there at once
	auto& at = registry.Get<Transform>(object);
	at.rotation = transform.rotation * rotation;
	if (!animation.boneMatrices.empty())
	{
		at.position = PalmOf(bone, animation, PlacementOf(transform)) - (at.rotation * middle);
	}
	ecs::NotifyTeleported(object);
}

/// A turn of the reach: the four catching animations blended by where the thing is against where the hand closes in
/// each, the catch played on, or back once the thing is out of the physics; at its moment the hand closes on the thing,
/// unless the blend had to lean too far
void ReachTurn(ecs::Registry& registry, entt::entity creature, Catching& catching, const Transform& transform,
               const CreatureRig::ActionPoints& points, const CreatureAnimation& animation, const CatchHands& hands)
{
	const auto& lookup = std::as_const(registry);
	const auto* flight = FlightOf(catching.object);
	creature_catch::Blend blend {};
	if (flight != nullptr)
	{
		blend = creature_catch::Weigh(ToLocal(transform, flight->body.Centre()), hands.hands,
		                              ecs::creature_pose::DrawnScale(lookup, creature).x, catching.mirrored);
		catching.height = blend.height;
	}
	else
	{
		blend = creature_catch::WeighWithout(catching.height);
		catching.hand = Catching::Hand::Missed;
	}
	catching.weights = blend.weights;
	catching.timeMs = creature_catch::CatchClock(catching.hand == Catching::Hand::Missed, catching.timeMs, catching.stepMs,
	                                             catching.durationMs);
	if (catching.hand != Catching::Hand::Reaching || static_cast<float>(catching.timeMs) < points.catchMs)
	{
		return;
	}
	if (blend.clamped || !ecs::IsAvailable(catching.object) || lookup.AllOf<HeldByCreature>(catching.object) ||
	    !lookup.AllOf<Transform>(catching.object))
	{
		catching.hand = Catching::Hand::Missed;
		return;
	}
	catching.hand = Catching::Hand::Caught;
	TakeCaught(registry, creature, catching.object, catching, points, animation, transform);
}

/// A turn of waiting for the thing, once turned to face it: it steps across when the thing will pass out of reach,
/// reaches for it when it will arrive as the hand closes, and gives up when the thing is behind it, out of the physics
/// or too late
void ReadyTurn(ecs::Registry& registry, entt::entity creature, Catching& catching, const Transform& transform,
               const CreatureRig::ActionPoints& points, CreatureAnimation& animation, const CatchHands& hands)
{
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	if (!catching.turned)
	{
		if (locomotion.IsMoving(creature))
		{
			return;
		}
		catching.turned = true;
		catching.timeMs = 0;
	}
	const auto* flight = FlightOf(catching.object);
	if (flight == nullptr)
	{
		EndCatch(catching, animation);
		return;
	}
	const auto& lookup = std::as_const(registry);
	const auto lead = creature_catch::LeadSeconds(points.catchMs, ecs::creature_pose::DrawnSize(lookup, creature));
	const auto ready = creature_catch::ReadyToCatch(ToLocal(transform, flight->body.Centre()),
	                                                glm::transpose(transform.rotation) * flight->body.velocity, hands.hands,
	                                                ecs::creature_pose::DrawnScale(lookup, creature).x, lead);
	switch (ready.readiness)
	{
	case creature_catch::Readiness::Wait:
		break;
	case creature_catch::Readiness::Step:
		// Not while it moves. (pending) the game also keeps the step's end out of the route planner's circles of the
		// things about it, which openblack's routes don't lay out
		if (locomotion.IsMoving(creature))
		{
			EndCatch(catching, animation);
			break;
		}
		catching.stage = Catching::Stage::Stepping;
		catching.mirrored = ready.mirrored;
		catching.timeMs = 0;
		catching.durationMs = WholeDurationOf(creature, creature_catch::k_CatchStep);
		StepTurn(lookup, creature, catching, transform);
		break;
	case creature_catch::Readiness::Catch:
		catching.stage = Catching::Stage::Reaching;
		catching.hand = Catching::Hand::Reaching;
		catching.mirrored = ready.mirrored;
		catching.timeMs = 0;
		catching.durationMs = WholeDurationOf(creature, creature_catch::k_CatchAnimations.front());
		ReachTurn(registry, creature, catching, transform, points, animation, hands);
		break;
	case creature_catch::Readiness::GiveUp:
		EndCatch(catching, animation);
		break;
	}
}
} // namespace

bool CreatureObjectActionSystem::Catch(entt::entity creature, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(creature) || !lookup.AllOf<Creature, CreatureAnimation, Transform>(creature) ||
	    !Locator::creatureAnimationSystem::has_value() || !Locator::creatureLocomotionSystem::has_value())
	{
		return false;
	}
	auto& animations = Locator::creatureAnimationSystem::value();
	const bool hasAnimations = std::ranges::all_of(creature_catch::k_CatchAnimations, [&](size_t clip) {
		return animations.AnimationDuration(creature, clip).has_value();
	});
	const auto* points = PointsOf(lookup.Get<Creature>(creature));
	// Not without the four catching animations, nor while its body acts something else out
	if (!hasAnimations || points == nullptr || IsCatching(creature) || GetState(creature) == State::Busy)
	{
		return false;
	}
	// The thing must be flying across and up or down fast enough
	const auto* flight = FlightOf(object);
	if (flight == nullptr || !creature_catch::FastEnoughToStart(flight->body.velocity))
	{
		return false;
	}
	// Where the hand closes in each catching animation, measured at its first catch and kept
	if (!lookup.AllOf<CatchHands>(creature))
	{
		CatchHands measured;
		for (size_t i = 0; i < creature_catch::k_CatchAnimations.size(); ++i)
		{
			const auto grasp = animations.GraspInAnimation(creature, creature_catch::k_CatchAnimations.at(i), points->catchMs,
			                                               points->rightHand);
			if (!grasp.has_value())
			{
				return false;
			}
			measured.hands.at(i) = *grasp;
		}
		registry.Assign<CatchHands>(creature, measured);
	}
	if (lookup.AllOf<CreatureObjectAction>(creature))
	{
		registry.RemoveState<CreatureObjectAction>(creature);
	}
	// It faces back along the thing's flight, turning first when that is too far round
	const auto& transform = lookup.Get<Transform>(creature);
	const auto& velocity = flight->body.velocity;
	const auto back = transform.position - glm::vec3(velocity.x, 0.0f, velocity.z);
	const bool turns = BearingTo(transform, back) > creature_catch::k_TurnAngle;
	if (turns)
	{
		Locator::creatureLocomotionSystem::value().TurnToFace(creature, glm::xz(back));
	}
	registry.AssignOrReplaceState<CreatureCatching>(creature, CreatureCatching {.object = object, .turned = !turns});
	return true;
}

bool CreatureObjectActionSystem::IsCatching(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* catching = registry.Valid(creature) ? registry.TryGet<const CreatureCatching>(creature) : nullptr;
	return catching != nullptr && catching->stage != CreatureCatching::Stage::Over;
}

void CreatureObjectActionSystem::ProcessCatches()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	std::vector<entt::entity> catchers;
	lookup.Each<const Creature, const Transform, const CreatureAnimation, const CreatureCatching>(
	    [&catchers](entt::entity entity, const auto&...) { catchers.push_back(entity); });
	for (const auto entity : catchers)
	{
		auto& catching = registry.Get<CreatureCatching>(entity);
		if (catching.stage == Catching::Stage::Over)
		{
			continue;
		}
		// Started in the turn's mind, the body takes it up from the next turn
		if (catching.fresh)
		{
			catching.fresh = false;
			continue;
		}
		auto& animation = registry.Get<CreatureAnimation>(entity);
		const auto* points = PointsOf(lookup.Get<Creature>(entity));
		const auto* hands = lookup.TryGet<const CatchHands>(entity);
		if (points == nullptr || hands == nullptr)
		{
			EndCatch(catching, animation);
			continue;
		}
		const auto& transform = lookup.Get<Transform>(entity);
		// The turn's step of its animations, as the game takes it for the size it is drawn at
		catching.stepMs = creature_turn_pose::Step(static_cast<int32_t>(game_clock::MsPerTurn()),
		                                           ecs::creature_pose::DrawnSize(lookup, entity), false, false);
		// An animation the turn's step would take past its end ends first: the step back to waiting, the catch over
		if (catching.stage == Catching::Stage::Stepping &&
		    creature_catch::StepEnds(catching.timeMs, catching.stepMs, catching.durationMs))
		{
			catching.stage = Catching::Stage::Ready;
			catching.timeMs = 0;
		}
		if (catching.stage == Catching::Stage::Reaching &&
		    creature_catch::CatchEnds(catching.hand == Catching::Hand::Missed, catching.timeMs, catching.stepMs,
		                              catching.durationMs))
		{
			EndCatch(catching, animation);
			continue;
		}
		switch (catching.stage)
		{
		case Catching::Stage::Ready:
			ReadyTurn(registry, entity, catching, transform, *points, animation, *hands);
			break;
		case Catching::Stage::Stepping:
			StepTurn(lookup, entity, catching, transform);
			break;
		case Catching::Stage::Reaching:
			ReachTurn(registry, entity, catching, transform, *points, animation, *hands);
			break;
		case Catching::Stage::Over:
			break;
		}
	}
}

std::vector<CreatureAnimation::Slot> CreatureObjectActionSystem::CatchSlotsAt(const CreatureCatching& catching, float share)
{
	std::vector<CreatureAnimation::Slot> slots;
	const bool back = catching.stage == Catching::Stage::Reaching && catching.hand == Catching::Hand::Missed;
	const auto ahead = share * static_cast<float>(catching.stepMs);
	const auto time = std::clamp(static_cast<float>(catching.timeMs) + (back ? -ahead : ahead), 0.0f,
	                             static_cast<float>(std::max(catching.durationMs - 1, 0)));
	switch (catching.stage)
	{
	case Catching::Stage::Stepping:
		slots.push_back(
		    {.animation = creature_catch::k_CatchStep, .timeMs = time, .weight = 1.0f, .mirrored = catching.mirrored});
		break;
	case Catching::Stage::Reaching:
		for (size_t i = 0; i < creature_catch::k_CatchAnimations.size(); ++i)
		{
			slots.push_back({.animation = creature_catch::k_CatchAnimations.at(i),
			                 .timeMs = time,
			                 .weight = catching.weights.at(i),
			                 .mirrored = catching.mirrored});
		}
		break;
	case Catching::Stage::Ready:
	case Catching::Stage::Over:
		break;
	}
	return slots;
}
