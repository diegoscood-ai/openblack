/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The emergencies that a creature looks at each turn before it decides what to do: low life, a fire on its body, and a
// fire in the nearest town

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <iterator>
#include <optional>
#include <string_view>
#include <tuple>
#include <vector>

#include <spdlog/spdlog.h>

#include "Creature/CreatureEmergencies.h"
#include "Creature/CreatureSpellMind.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Town/TownStats.h"
#include "Enums.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using creature_desires::Desire;

namespace
{
/// The actions that a creature does in an emergency
constexpr std::string_view k_CastWaterOnItselfAction = "CastMagicWater";
constexpr std::string_view k_DrinkFromTheSeaAction = "DrinkFromTheSea";
constexpr std::string_view k_PutOutFireAction = "PutOutFireWithMagicWater";

/// The burning building of a town that the creature goes to, if a building burns: of the town's buildings, newest
/// first, the first burning built storage pit or town centre, else the last burning building
std::optional<entt::entity> BurningBuildingOf(const ecs::Registry& registry, entt::entity town)
{
	if (!Locator::fireSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& fires = Locator::fireSystem::value();
	const auto abodes = ecs::town_stats::AbodesOf(town);
	std::vector<creature_emergencies::Building> buildings;
	buildings.reserve(abodes.size());
	std::ranges::transform(abodes, std::back_inserter(buildings), [&](entt::entity abode) {
		const auto* data = registry.Valid(abode) ? registry.TryGet<const Abode>(abode) : nullptr;
		if (data == nullptr)
		{
			return creature_emergencies::Building {};
		}
		const bool builtPit = data->type == AbodeNumber::StoragePit && ecs::abodes::IsBuilt(abode);
		const bool centre = data->type == AbodeNumber::TownCentre;
		return creature_emergencies::Building {.onFire = fires.IsOnFire(abode), .comesFirst = builtPit || centre};
	});
	const auto picked = creature_emergencies::PickBurningBuilding(buildings);
	return picked.has_value() ? std::optional(abodes[*picked]) : std::nullopt;
}
} // namespace

void CreatureMindSystem::StepEmergencies(entt::entity creature, const Creature& body, CreatureMindState& mind, bool scripted,
                                         float dominantRate)
{
	auto& registry = Locator::entitiesRegistry::value();
	// A plan forced on it (an order, a script's action, an emergency of its own) is left to run
	const bool planForced = mind.planner.current.has_value() && mind.planner.current->forced;
	if (!mind.desires.has_value() ||
	    !creature_emergencies::LooksAtEmergencies({.emergencyRunning = mind.emergencyRunning,
	                                               .scripted = scripted,
	                                               .planForced = planForced,
	                                               .fighting = registry.AllOf<CreatureFighting>(creature)}))
	{
		return;
	}
	const auto sightings = MiracleSightings(creature, MagicType::Water);
	const bool onFire = Locator::fireSystem::has_value() && Locator::fireSystem::value().IsOnFire(creature);
	if (!onFire)
	{
		// Badly hurt, it wants above all to restore its health for a short time, with its body's needs held down too
		const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
		if (needs != nullptr && creature_emergencies::IsHurt(needs->needs.life))
		{
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} is badly hurt: it wants to restore its health",
			                    entt::to_integral(creature));
			mind.dominantDesire = creature_spell_mind::SetCheatDominant(
			    *mind.desires, Desire::RestoreHealth, true, dominantRate, mind_detail::DesireFloorOf(registry, creature),
			    creature_emergencies::k_RestoreHealthSeconds);
			mind.emergencyRunning = true;
			return;
		}
	}
	else
	{
		// On fire, it casts the water miracle on itself, or else it goes to drink from the sea without the leash
		const auto choice = sightings.has_value()
		                        ? creature_emergencies::ChooseOwnFire(sightings->first, sightings->second,
		                                                              mind_detail::CanPayForMiracle(creature, MagicType::Water))
		                        : creature_emergencies::OwnFire::DrinkFromTheSea;
		const bool drinks = choice == creature_emergencies::OwnFire::DrinkFromTheSea;
		if (drinks && Locator::leashSystem::has_value() && Locator::leashSystem::value().IsLeashed(creature))
		{
			Locator::leashSystem::value().TakeOff(creature);
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} is on fire: it {}", entt::to_integral(creature),
		                    drinks ? "goes to drink from the sea" : "casts water on itself");
		const bool planned = ForceActivity(creature, {.desire = Desire::RestoreHealth,
		                                              .action = drinks ? k_DrinkFromTheSeaAction : k_CastWaterOnItselfAction,
		                                              .actionObject = creature});
		if (planned && mind.idle.step < mind.idle.agenda.size())
		{
			// A creature of no player or of the computer keeps the run speed for each walk of the plan; a player's goes
			// back to its walk speed as it stops what it did, so it walks
			const auto owner = body.owner;
			const bool computer = Locator::playerSystem::has_value() && Locator::playerSystem::value().IsComputerPlayer(owner);
			if (creature_emergencies::KeepsRunSpeed(owner != PlayerNames::NEUTRAL, computer))
			{
				for (auto& step : mind.idle.agenda)
				{
					step.movement.run = true;
				}
			}
			// The forced plan holds Plan::forced already
			mind.emergencyRunning = true;
			return;
		}
	}
	// A thing that burns in the nearest town: it goes to put the fire out with the water miracle when it cares enough
	if (!sightings.has_value() || !mind.learnt.has_value())
	{
		return;
	}
	const auto& known = mind.learnt->knowledge.miraclesKnown;
	const auto water = static_cast<size_t>(MagicType::Water);
	const auto& desires = *mind.desires;
	const creature_emergencies::TownFireFacts facts {
	    .knowsWater = water < known.size() && known[water],
	    .seen = sightings->first,
	    .needed = sightings->second,
	    .canPay = mind_detail::CanPayForMiracle(creature, MagicType::Water),
	    .compassionActivated = desires[Desire::Compassion].activated,
	    .compassionSuppressed = desires[Desire::Compassion].suppressedTurns > 0,
	    .compassion = desires[Desire::Compassion].value,
	    .anger = desires[Desire::Anger].value,
	};
	// What it knows and wants is asked first, so the town is looked for only by a creature that would go
	if (!creature_emergencies::PutsOutTownFire(facts))
	{
		return;
	}
	const auto town = ecs::map_cells::GetNearestTown(ecs::object::MapCoordsOf(creature), creature_emergencies::k_TownReach);
	if (town == entt::null)
	{
		return;
	}
	const auto burning = BurningBuildingOf(registry, town);
	if (!burning.has_value())
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} goes to put out the fire on {}", entt::to_integral(creature),
	                    entt::to_integral(*burning));
	// The building comes from the town's own list, so the plan is about that town
	std::ignore = ForceActivity(
	    creature,
	    {.desire = Desire::Compassion, .activityObject = town, .action = k_PutOutFireAction, .actionObject = *burning});
	// The emergency starts also when the creature could not make the plan: it then takes no new emergency and no
	// reaction until it next stops what it does
	mind.emergencyRunning = true;
}
