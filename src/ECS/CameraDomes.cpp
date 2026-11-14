/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraDomes.h"

#include <utility>
#include <vector>

#include <glm/vec3.hpp>

#include "Creature/CreatureMorph.h"
#include "ECS/Components/CameraDome.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBodyMetrics.h"
#include "ECS/Components/CreatureBodyPlacement.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
CameraDome FromExclusion(const camera_zones::Exclusion& dome)
{
	return {.position = dome.position, .radius = dome.radius, .height = dome.height};
}
} // namespace

void camera_domes::PlaceCreatures(Registry& registry)
{
	const auto& lookup = std::as_const(registry);
	std::vector<std::pair<entt::entity, CameraDome>> placed;
	lookup.Each<const Creature, const CreatureBodyMetrics, const Transform>(
	    [&lookup, &placed](entt::entity entity, const Creature&, const CreatureBodyMetrics& body, const Transform& transform) {
		    // where its body model was last placed, at the size it is drawn at
		    const auto* placement = lookup.TryGet<const CreatureBodyPlacement>(entity);
		    const glm::vec3 position = placement != nullptr ? glm::vec3(placement->cells[3]) : transform.position;
		    const float drawn = creature_pose::DrawnSize(lookup, entity);
		    const auto size = camera_zones::CreatureDomeSize(creature_morph::Radius(drawn, body.restHeight, body.reach),
		                                                     creature_morph::ClampScale(drawn));
		    const auto dome = lookup.AllOf<CameraDome>(entity) ? camera_zones::AdjustedDome(position, size.radius, size.height)
		                                                       : camera_zones::MadeDome(position, size.radius, size.height);
		    placed.emplace_back(entity, FromExclusion(dome));
	    });
	// Written once the creatures are walked, so that a first dome's new storage is not made during the walk
	for (const auto& [entity, dome] : placed)
	{
		registry.AssignOrReplaceState<CameraDome>(entity, dome);
	}
}

void camera_domes::PlaceTemple(Registry& registry, entt::entity heart)
{
	const auto* mesh = registry.TryGet<const Mesh>(heart);
	const auto* transform = registry.TryGet<const Transform>(heart);
	if (mesh == nullptr || transform == nullptr)
	{
		return;
	}
	const auto size = camera_zones::TempleDomeSize(object::MeshHalfHeight(mesh->id));
	const auto dome = registry.AllOf<CameraDome>(heart)
	                      ? camera_zones::AdjustedDome(transform->position, size.radius, size.height)
	                      : camera_zones::MadeDome(transform->position, size.radius, size.height);
	registry.AssignOrReplaceState<CameraDome>(heart, FromExclusion(dome));
}

void camera_domes::RemoveTemple(Registry& registry, entt::entity heart)
{
	if (registry.AllOf<CameraDome>(heart))
	{
		registry.RemoveState<CameraDome>(heart);
	}
}

std::vector<camera_zones::Exclusion> camera_domes::Exclusions(const Registry& registry)
{
	std::vector<camera_zones::Exclusion> domes;
	registry.Each<const CameraDome>([&domes](entt::entity, const CameraDome& dome) {
		domes.push_back({.position = dome.position,
		                 .radius = dome.radius,
		                 .height = dome.height,
		                 .kind = camera_zones::Exclusion::Kind::Dome,
		                 .saved = false});
	});
	return domes;
}
