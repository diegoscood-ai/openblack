/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptDestination.h"

#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/WalkGoal.h"

using namespace openblack;
using namespace openblack::ecs::components;

std::optional<map_coords::MapCoords> ecs::script_destination::Of(const Registry& registry, entt::entity thing)
{
	if (thing == entt::null || !registry.Valid(thing))
	{
		return std::nullopt;
	}
	if (const auto* locomotion = registry.TryGet<const CreatureLocomotion>(thing))
	{
		return locomotion->lastDestination;
	}
	if (const auto* brain = registry.TryGet<const AnimalBrain>(thing); brain != nullptr && registry.AllOf<Animal>(thing))
	{
		auto goal = map_coords::FromMetres(brain->goal);
		goal.altitude = brain->goalAltitude;
		return goal;
	}
	if (const auto* walk = registry.TryGet<const WallHug>(thing); walk != nullptr && registry.AllOf<Villager>(thing))
	{
		auto goal = map_coords::FromMetres(walk->goal);
		const auto* height = registry.TryGet<const WalkGoalAltitude>(thing);
		goal.altitude = height != nullptr ? height->altitude : 0.0f;
		return goal;
	}
	return std::nullopt;
}
