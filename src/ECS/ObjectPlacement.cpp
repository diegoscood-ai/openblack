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
#include "ECS/Components/ObjectMatrix.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/TurnAngles.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"

namespace openblack::ecs::object_placement
{

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

Placed Of(const Registry& registry, entt::entity entity)
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
	return Of(registry, entity);
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
