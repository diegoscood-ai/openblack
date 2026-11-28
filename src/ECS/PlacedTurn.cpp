/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlacedTurn.h"

#include <utility>

#include "3D/ObjectMatrix.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/TurnAngles.h"
#include "ECS/ObjectPlacement.h"
#include "ECS/Registry.h"
#include "ECS/ScriptSetPosition.h"

namespace openblack::ecs::placed_turn
{

using components::Transform;
using components::TurnAngles;

bool CanLean(const Registry& registry, entt::entity entity)
{
	if (entity == entt::null || !registry.Valid(entity))
	{
		return false;
	}
	// a mobile static, but the singing stone's base, which is a plain object
	if (const auto* statics = registry.TryGet<const components::MobileStatic>(entity); statics != nullptr)
	{
		return statics->type != MobileStaticInfo::SingingStoneBase;
	}
	return registry.AnyOf<components::DeadTree, components::FelledTree, components::Fragment, components::TeleportStone,
	                      components::LandscapeVortex, components::MobileObject, components::Pot, components::Ball,
	                      components::Shark, components::OneOffSpellSeed, components::Reward, components::Scaffold>(entity);
}

glm::mat3 TurnOf(const glm::vec3& kept, bool canLean)
{
	return canLean ? affine::RotationYXZ(kept.y, kept.x, kept.z) : affine::AngleY(kept.y);
}

glm::vec3 AnglesOfTurn(const glm::mat3& turn, bool canLean)
{
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	affine::DecomposeYXZ(turn, y, x, z);
	return KeptAngles(glm::vec3(x, y, z), canLean);
}

glm::vec3 AnglesOf(const TurnAngles* kept, const glm::mat3& turn, bool canLean)
{
	if (kept != nullptr && kept->turn == turn)
	{
		return kept->angles;
	}
	return AnglesOfTurn(turn, canLean);
}

glm::vec3 AnglesOf(const Registry& registry, entt::entity entity)
{
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return glm::vec3(0.0f);
	}
	return AnglesOf(registry.TryGet<const TurnAngles>(entity), transform->rotation, CanLean(registry, entity));
}

void KeepAngles(Registry& registry, entt::entity entity, const glm::vec3& angles)
{
	const auto* transform = std::as_const(registry).TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return;
	}
	// only the simulation reads the kept angles: the drawn things stay as they are
	registry.AssignOrReplaceState<TurnAngles>(
	    entity, TurnAngles {.angles = KeptAngles(angles, CanLean(registry, entity)), .turn = transform->rotation});
}

bool SetAngles(Registry& registry, entt::entity entity, const glm::vec3& angles)
{
	auto* transform = registry.TryGet<Transform>(entity);
	if (transform == nullptr)
	{
		return false;
	}
	const bool canLean = CanLean(registry, entity);
	const auto kept = KeptAngles(angles, canLean);
	transform->rotation = TurnOf(kept, canLean);
	transform->scale = glm::vec3(transform->scale.x);
	KeepAngles(registry, entity, kept);
	// and its model is placed again with them
	object_placement::PlaceAgain(registry, entity);
	registry.SetDirty();
	return true;
}

void StandUpright(Registry& registry, entt::entity entity)
{
	auto* transform = registry.TryGet<Transform>(entity);
	if (transform == nullptr)
	{
		return;
	}
	transform->scale = glm::vec3(transform->scale.x);
	// read through the const registry: no storage is made for a thing that keeps no angles
	const auto* kept = std::as_const(registry).TryGet<const TurnAngles>(entity);
	if (kept == nullptr || kept->turn != transform->rotation)
	{
		transform->rotation = script_set_position::UprightRotation(transform->rotation);
		// and its model is placed again, upright
		object_placement::PlaceAgain(registry, entity);
		return;
	}
	// the kept angles stay as they are, leans and all: only the turn is made again
	const auto angles = kept->angles;
	transform->rotation = affine::AngleY(angles.y);
	registry.AssignOrReplaceState<TurnAngles>(entity, TurnAngles {.angles = angles, .turn = transform->rotation});
	object_placement::PlaceAgain(registry, entity);
}

} // namespace openblack::ecs::placed_turn
