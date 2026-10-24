/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "Creature/CreatureExplorationAgendas.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "Locator.h"
#include "ObjectMeasures.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using creature_exploration::Kind;
using creature_plan_actions::Build;
namespace land = openblack::ecs::creature_exploration_land;

void CreatureMindSystem::SituateExploration(entt::entity creature, const creature_plan_actions::Executor& executor,
                                            creature_plan_actions::Situation& situation)
{
	switch (executor.build)
	{
	case Build::GoToHillAndLook:
	case Build::GoToHillAndSit:
	case Build::WalkAlongRidge:
	case Build::ExploreCoast:
	case Build::ExploreTowns:
	case Build::SitDownOnBeach:
	case Build::LookAtMountains:
	case Build::LookOutToSea:
	case Build::ThrowInTheSea:
		break;
	default:
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const ecs::components::Transform>(creature);
	if (transform == nullptr)
	{
		return;
	}
	const glm::vec2 from {transform->position.x, transform->position.z};
	situation.height = object_measures::Height(registry, creature);
	// A region of the kind that the creature did not go into, else a region of land that it did not go into
	const auto unvisited = [&](Kind kind) {
		auto place = land::FindPlace(creature, kind, from, true);
		if (!place.has_value())
		{
			place = land::FindPlace(creature, Kind::Land, from, true);
		}
		return place;
	};
	const auto nearest = [&](Kind kind, glm::vec2 start) { return land::FindPlace(creature, kind, start, false); };
	// Where the map has no place at all, the creature goes to the low corner of the map, as in the game
	constexpr glm::vec2 k_Corner {0.0f};
	switch (executor.build)
	{
	case Build::GoToHillAndLook:
	case Build::WalkAlongRidge:
		situation.explorePlace = unvisited(Kind::Hill).value_or(k_Corner);
		break;
	case Build::GoToHillAndSit:
		// Where the map has no place at all, it stays where it is
		situation.explorePlace = unvisited(Kind::Hill).value_or(from);
		break;
	case Build::ExploreCoast:
		situation.explorePlace = unvisited(Kind::Coast).value_or(k_Corner);
		break;
	case Build::ExploreTowns:
		situation.explorePlace = unvisited(Kind::Town).value_or(k_Corner);
		break;
	case Build::LookAtMountains:
		situation.explorePlace = nearest(Kind::Hill, from);
		break;
	case Build::LookOutToSea:
		// The sea nearest to the coast, not to the creature
		situation.explorePlace = nearest(Kind::Coast, from);
		situation.exploreSea = situation.explorePlace.has_value() ? nearest(Kind::Sea, *situation.explorePlace) : std::nullopt;
		break;
	case Build::ThrowInTheSea:
		situation.explorePlace = nearest(Kind::Coast, from).value_or(k_Corner);
		situation.exploreSea = nearest(Kind::Sea, from).value_or(k_Corner);
		break;
	case Build::SitDownOnBeach:
	{
		// A clear place near the nearest sea, as wide as four times the creature's radius, with nothing living in it
		// unless a script controls the creature
		const auto sea = nearest(Kind::Sea, from).value_or(k_Corner);
		situation.exploreSea = sea;
		situation.explorePlace = mind_detail::FindClearArea(
		    registry, creature, sea, creature_exploration::k_BeachClearRadii * ecs::object::Get2DRadius(creature),
		    !ecs::script_held::IsControlledByScript(creature));
		break;
	}
	default:
		break;
	}
}
