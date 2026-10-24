/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreaturePose.h"

#include <cmath>
#include <cstdint>

#include <utility>

#include <glm/geometric.hpp>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Creature/CreatureLean.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureBodyMetrics.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// Below this the follower did not move far enough to tell its way
constexpr float k_HardlyMoved = 1e-4f;

/// Where a bone of the creature's action points is in the world, as its body is posed now and placed at its Transform
std::optional<glm::vec3> PosedPointOf(entt::entity creature, uint32_t creature::CreatureRig::ActionPoints::*point)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(creature))
	{
		return std::nullopt;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (body == nullptr || animation == nullptr || transform == nullptr)
	{
		return std::nullopt;
	}
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(body->species);
	if (!rigs.Contains(rigId) || !rigs.Handle(rigId)->actionPoints.has_value())
	{
		return std::nullopt;
	}
	const auto bone = (*rigs.Handle(rigId)->actionPoints).*point;
	if (bone >= animation->boneMatrices.size())
	{
		return std::nullopt;
	}
	const auto placement = creature::PlacementMatrix(transform->position, transform->rotation, transform->scale);
	return glm::vec3(creature::PosedBone(bone, animation->boneMatrices, placement)[3]);
}
} // namespace

void creature_pose::CommitTurnPose(entt::entity creature, glm::vec3 position, float heading)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !std::as_const(registry).AllOf<Transform>(creature))
	{
		return;
	}
	map_cells::MoveMapObject(creature, position);
	registry.Get<Transform>(creature).rotation = affine::AngleY(-heading);
}

float creature_pose::ReadHeading(const glm::mat3& rotation)
{
	return std::atan2(rotation[2].x, rotation[2].z);
}

float creature_pose::BodyYaw(const Registry& registry, entt::entity creature, const glm::mat3& rotation)
{
	const auto* moving = registry.TryGet<const components::CreatureLocomotion>(creature);
	const float heading = moving != nullptr && moving->started ? moving->heading : ReadHeading(rotation);
	return -heading;
}

float creature_pose::HeadingFromFollower(glm::vec2 before, glm::vec2 after, float previous)
{
	const auto way = after - before;
	return glm::length(way) > k_HardlyMoved ? creature_locomotion::HeadingOf(way) : previous;
}

std::optional<creature_pose::DrawnPlacement> creature_pose::BetweenTurns(const Registry& registry, entt::entity creature)
{
	// the drawn pose first: every other entity has none, so the walk over the rows stops there
	const auto* pose = registry.TryGet<const CreatureDrawPose>(creature);
	if (pose == nullptr)
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* locomotion = registry.TryGet<const CreatureLocomotion>(creature);
	// the Transform is where the locomotion's turn put it (an exact copy, so the compare is exact); moved there by
	// anything else, the pose is not that turn's
	if (transform == nullptr || locomotion == nullptr || !locomotion->started ||
	    locomotion->toPosition != transform->position || registry.AnyOf<HandDrawPose, PhysicsDrawPose>(creature))
	{
		return std::nullopt;
	}
	return DrawnPlacement {.position = pose->position, .rotation = pose->rotation};
}

creature_pose::DrawnPlacement creature_pose::DrawnPlacementOf(const Registry& registry, entt::entity creature)
{
	// the sources of ecs::DrawnModel in its order, so that its rotation is the one the body is drawn with
	if (const auto* inHand = registry.TryGet<const HandDrawPose>(creature))
	{
		return {.position = inHand->position, .rotation = inHand->rotation};
	}
	if (const auto* flying = registry.TryGet<const PhysicsDrawPose>(creature))
	{
		return {.position = flying->position, .rotation = flying->rotation};
	}
	if (const auto between = BetweenTurns(registry, creature))
	{
		return *between;
	}
	const auto& transform = registry.Get<const Transform>(creature);
	return {.position = transform.position, .rotation = transform.rotation};
}

glm::vec3 creature_pose::DrawnScale(const Registry& registry, entt::entity creature)
{
	const auto& transform = registry.Get<const Transform>(creature);
	const auto* pose = registry.TryGet<const CreatureDrawPose>(creature);
	return pose != nullptr && pose->scale.has_value() ? *pose->scale : transform.scale;
}

float creature_pose::DrawnSize(float ownSize, std::optional<float> givenSize)
{
	return givenSize.value_or(ownSize);
}

float creature_pose::DrawnSize(const Registry& registry, entt::entity creature)
{
	const auto own = registry.Get<const Creature>(creature).size;
	const auto* pose = registry.TryGet<const CreatureDrawPose>(creature);
	return DrawnSize(own, pose != nullptr ? pose->size : std::nullopt);
}

float creature_pose::GivenSize(const Registry& registry, entt::entity creature)
{
	const auto* pose = registry.TryGet<const CreatureDrawPose>(creature);
	return pose != nullptr && pose->given.has_value() ? *pose->given : registry.Get<const Creature>(creature).size;
}

creature_morph::Morph creature_pose::TargetMorph(const Registry& registry, entt::entity creature)
{
	const auto& body = registry.Get<const Creature>(creature);
	const auto* morph = registry.TryGet<const CreatureMorph>(creature);
	const float fatness = morph != nullptr ? morph->shownFatness : body.fatness;
	return creature_morph::FromAttributes(body.alignment, fatness, body.strength,
	                                      archetypes::CreatureArchetype::SpeciesStrength(body.species));
}

float creature_pose::DrawnSizeShare(const Registry& registry, entt::entity creature)
{
	const auto* pose = registry.TryGet<const CreatureDrawPose>(creature);
	const auto own = registry.Get<const Transform>(creature).scale.x;
	if (pose == nullptr || !pose->scale.has_value() || !(own > 0.0f))
	{
		return 1.0f;
	}
	return pose->scale->x / own;
}

glm::mat4 creature_pose::LeanedModel(const Registry& registry, entt::entity creature, glm::vec3 position,
                                     const glm::mat3& rotation, glm::vec3 scale)
{
	const auto* body = registry.TryGet<const CreatureBodyMetrics>(creature);
	if (body == nullptr || !Locator::terrainSystem::has_value())
	{
		return affine::Model(position, rotation, scale);
	}
	const auto& land = Locator::terrainSystem::value();
	const float radius = creature_morph::Radius(DrawnSize(registry, creature), body->restHeight, body->reach);
	const auto heights = creature_lean::Sample(position, radius, registry.AllOf<HeldByCreature>(creature),
	                                           [&land](glm::vec2 point) { return land.GetHeightAt(point); });
	return creature_lean::Model(position, rotation, scale, creature_lean::FromHeights(heights, radius));
}

std::optional<glm::vec3> creature_pose::GroinOf(entt::entity creature)
{
	return PosedPointOf(creature, &creature::CreatureRig::ActionPoints::groin);
}

std::optional<glm::vec3> creature_pose::BellyOf(entt::entity creature)
{
	return PosedPointOf(creature, &creature::CreatureRig::ActionPoints::belly);
}
