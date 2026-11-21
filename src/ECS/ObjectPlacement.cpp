/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectPlacement.h"

#include <utility>

#include <glm/mat4x3.hpp>

#include "3D/ObjectMatrix.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBodyPlacement.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/LastDrawnMatrix.h"
#include "ECS/Components/ObjectMatrix.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/TurnAngles.h"
#include "ECS/Components/Villager.h"
#include "ECS/MobileDrawing.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerPhysics.h"
#include "Locator.h"

namespace openblack::ecs::object_placement
{

using components::LastDrawnMatrix;
using components::ObjectMatrix;
using components::Transform;

namespace
{

void Keep(Registry& registry, entt::entity entity, const glm::mat4& cells, float yAngle, float scale)
{
	const auto* transform = std::as_const(registry).TryGet<const Transform>(entity);
	// the renderer draws the model with it: the drawn instances are written again
	registry.AssignOrReplace<ObjectMatrix>(entity, ObjectMatrix {.cells = glm::mat4x3(cells),
	                                                             .scale = scale,
	                                                             .yAngle = yAngle,
	                                                             .madeFrom = transform != nullptr ? *transform : Transform {}});
}

/// The angles the thing keeps now (PlaceAgain), and whether it leans
struct Kept
{
	glm::vec3 angles;
	bool canLean;
};

Kept AnglesNow(const Registry& registry, entt::entity entity, const ObjectMatrix& placement, const Transform& transform)
{
	const bool canLean = placed_turn::CanLean(registry, entity);
	const auto* kept = registry.TryGet<const components::TurnAngles>(entity);
	if (kept != nullptr && kept->turn == transform.rotation)
	{
		return {placed_turn::KeptAngles(kept->angles, canLean), canLean};
	}
	if (!canLean && affine::AngleY(placement.yAngle) == transform.rotation)
	{
		return {glm::vec3(0.0f, placement.yAngle, 0.0f), canLean};
	}
	return {placed_turn::AnglesOfTurn(transform.rotation, canLean), canLean};
}

glm::mat4 Constructed(const glm::vec3& point, const Kept& kept, float scale)
{
	return kept.canLean ? affine::PlacementYXZ(point, kept.angles.y, kept.angles.x, kept.angles.z, scale)
	                    : affine::PlacementMatrix(point, kept.angles.y, scale);
}

bool SameTransform(const Transform& a, const Transform& b)
{
	return a.position == b.position && a.rotation == b.rotation && a.scale == b.scale;
}

} // namespace

void PlaceYaw(Registry& registry, entt::entity entity, const glm::vec3& point, float yAngle, float scale)
{
	Keep(registry, entity, affine::PlacementMatrix(point, yAngle, scale), yAngle, scale);
}

void PlaceYXZ(Registry& registry, entt::entity entity, const glm::vec3& point, const glm::vec3& angles, float scale)
{
	Keep(registry, entity, affine::PlacementYXZ(point, angles.y, angles.x, angles.z, scale), angles.y, scale);
}

void PlaceAgain(Registry& registry, entt::entity entity)
{
	const auto& lookup = std::as_const(registry);
	// a villager or an animal: placed where it is, at its own y angle and its scale
	if (const auto* transform = lookup.TryGet<const Transform>(entity);
	    transform != nullptr && lookup.AnyOf<components::Villager, components::Animal>(entity) &&
	    !lookup.AllOf<components::Creature>(entity))
	{
		PlaceLiving(registry, entity, transform->position, villager_physics::ObjectYAngle(lookup, entity), transform->scale.x);
		return;
	}
	const auto* placement = lookup.TryGet<const ObjectMatrix>(entity);
	const auto* transform = lookup.TryGet<const Transform>(entity);
	if (placement == nullptr || transform == nullptr)
	{
		return;
	}
	const auto kept = AnglesNow(lookup, entity, *placement, *transform);
	const float scale = transform->scale.x;
	Keep(registry, entity, Constructed(transform->position, kept, scale), kept.angles.y, scale);
}

void PlaceLiving(Registry& registry, entt::entity entity, const glm::vec3& point, float yAngle, float scale)
{
	registry.AssignOrReplaceState<LastDrawnMatrix>(
	    entity, LastDrawnMatrix {
	                .cells = glm::mat4x3(affine::PlacementMatrix(point, yAngle, scale)), .scale = scale, .yAngle = yAngle});
}

void PlaceLivingTurned(Registry& registry, entt::entity entity, float before, float yAngle)
{
	const auto& lookup = std::as_const(registry);
	const auto* transform = lookup.TryGet<const Transform>(entity);
	// the angle setter does nothing for the angle it has
	if (transform == nullptr || before == yAngle || !lookup.AnyOf<components::Villager, components::Animal>(entity) ||
	    lookup.AllOf<components::Creature>(entity))
	{
		return;
	}
	PlaceLiving(registry, entity, transform->position, yAngle, transform->scale.x);
}

void PlaceLivingTurnedOf(const Transform& transform, float before, float yAngle)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = std::as_const(registry).EntityOf(transform);
	if (entity == entt::null || !registry.Valid(entity))
	{
		return;
	}
	PlaceLivingTurned(registry, entity, before, yAngle);
}

void KeepHeld(Registry& registry, entt::entity entity)
{
	const auto& lookup = std::as_const(registry);
	const auto* transform = lookup.TryGet<const Transform>(entity);
	if (transform == nullptr || !lookup.AllOf<components::HandDrawPose>(entity) ||
	    !lookup.AnyOf<components::Villager, components::Animal>(entity) || lookup.AllOf<components::Creature>(entity))
	{
		return;
	}
	// its 3D object's matrix set whole by the hand's draw: the angle it was last placed with stays
	const auto* before = lookup.TryGet<const LastDrawnMatrix>(entity);
	const float yAngle = before != nullptr ? before->yAngle : 0.0f;
	registry.AssignOrReplaceState<LastDrawnMatrix>(
	    entity, LastDrawnMatrix {
	                .cells = glm::mat4x3(DrawnModel(lookup, entity, false)), .scale = transform->scale.x, .yAngle = yAngle});
}

void KeepDrawn(Registry& registry, entt::entity entity)
{
	const auto& lookup = std::as_const(registry);
	const auto* transform = lookup.TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return;
	}
	// a creature's body is placed by every frame of the world's view (creature_pose::PlaceBodies)
	if (!lookup.AnyOf<components::Villager, components::Animal>(entity) || lookup.AllOf<components::Creature>(entity))
	{
		return;
	}
	// placed as the original's draw places it (not the debug switch's smooth pose)
	if (const auto drawn = DrawnLivingPlacement(lookup, entity); drawn.has_value())
	{
		registry.AssignOrReplaceState<LastDrawnMatrix>(
		    entity, LastDrawnMatrix {.cells = glm::mat4x3(drawn->cells), .scale = drawn->scale, .yAngle = drawn->yAngle});
	}
}

Placed Of(const Registry& registry, entt::entity entity)
{
	// a creature: its body model where the last frame placed it (creature_pose::PlaceBody)
	if (const auto* body = registry.TryGet<const components::CreatureBodyPlacement>(entity); body != nullptr)
	{
		return {body->cells, body->scale};
	}
	// held by the hand: the matrix the hand draws it with
	if (registry.AllOf<components::HandDrawPose>(entity))
	{
		return {DrawnModel(registry, entity, false), registry.Get<const Transform>(entity).scale.x};
	}
	if (const auto* drawn = registry.TryGet<const LastDrawnMatrix>(entity); drawn != nullptr)
	{
		return {glm::mat4(drawn->cells), drawn->scale};
	}
	return OwnOf(registry, entity);
}

Placed OwnOf(const Registry& registry, entt::entity entity)
{
	const auto& transform = registry.Get<const Transform>(entity);
	const auto* placement = registry.TryGet<const ObjectMatrix>(entity);
	if (placement == nullptr)
	{
		return {affine::Model(transform), transform.scale.x};
	}
	if (SameTransform(placement->madeFrom, transform))
	{
		return {glm::mat4(placement->cells), placement->scale};
	}
	// moved since by a move that made no placement: what the game's constructor makes now, when the turn is its own
	const auto kept = AnglesNow(registry, entity, *placement, transform);
	const float scale = transform.scale.x;
	if (placed_turn::TurnOf(kept.angles, kept.canLean) != transform.rotation)
	{
		return {affine::Model(transform), scale};
	}
	return {Constructed(transform.position, kept, scale), scale};
}

Placed DrawnOf(const Registry& registry, entt::entity entity)
{
	// the physics' frame pose writes the matrix of what it carries every frame, before anything reads it, but not the
	// object's scale, which is kept apart from the matrix
	if (registry.AllOf<components::PhysicsDrawPose>(entity) &&
	    !registry.AnyOf<components::HandDrawPose, LastDrawnMatrix, components::CreatureBodyPlacement>(entity))
	{
		return {DrawnModel(registry, entity, false), OwnOf(registry, entity).scale};
	}
	return Of(registry, entity);
}

glm::mat4 Cells(const Registry& registry, entt::entity entity)
{
	return Of(registry, entity).cells;
}

std::optional<Placed> KeptBefore(const Registry& registry, entt::entity entity)
{
	if (!registry.AllOf<ObjectMatrix, Transform>(entity))
	{
		return std::nullopt;
	}
	return OwnOf(registry, entity);
}

void LeaveBehind(Registry& registry, entt::entity entity, const std::optional<Placed>& kept)
{
	const auto* placement = std::as_const(registry).TryGet<const ObjectMatrix>(entity);
	const auto* transform = std::as_const(registry).TryGet<const Transform>(entity);
	if (!kept.has_value() || placement == nullptr || transform == nullptr)
	{
		return;
	}
	// the cells it had, for the Transform it has now (the drawn instances are written again)
	registry.AssignOrReplace<ObjectMatrix>(entity, ObjectMatrix {.cells = glm::mat4x3(kept->cells),
	                                                             .scale = kept->scale,
	                                                             .yAngle = placement->yAngle,
	                                                             .madeFrom = *transform});
}

} // namespace openblack::ecs::object_placement
