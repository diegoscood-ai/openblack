/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMimic.h"

#include <cstddef>

#include <limits>
#include <tuple>
#include <utility>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Common/EventManager.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureDeeds.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Events/CreatureMimicEvents.h"
#include "ECS/Events/Publish.h"
#include "ECS/LastDropped.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Town/AbodeVillagers.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs::creature_mimic
{
static_assert(std::tuple_size_v<decltype(InfoConstants::creatureMimic)> == creature_watching::k_DeedCount,
              "one deed per row of the mimicry table");

namespace
{
/// Where something is, read without making any storage: the origin for something with no place
glm::vec3 PlaceOfObject(entt::entity object)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (object == entt::null || !registry.Valid(object))
	{
		return glm::vec3(0.0f);
	}
	const auto* transform = registry.TryGet<const components::Transform>(object);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}

/// The minds learn of the deed, if the game has them
void PassDeedToMinds(const events::PlayerDeedForMimic& event)
{
	if (!Locator::creatureMindSystem::has_value())
	{
		return;
	}
	const auto object = event.object == entt::null ? std::nullopt : std::optional(event.object);
	auto& minds = Locator::creatureMindSystem::value();
	if (event.magic.has_value())
	{
		minds.PlayerCastDeed(event.player, static_cast<size_t>(event.deed), event.point, object, *event.magic);
		return;
	}
	minds.PlayerDid(event.player, static_cast<size_t>(event.deed), event.point, object);
}

/// The player's creature may come to share the town's need, if the game has minds
void PassTownDesireToMinds(const events::CreatureEmpathyWithTownDesire& event)
{
	if (!Locator::creatureMindSystem::has_value())
	{
		return;
	}
	Locator::creatureMindSystem::value().EmpathiseWithTownDesire(event.player, event.desire, event.weight, event.point);
}
} // namespace

void Consider(PlayerNames player, creature_watching::Deed deed, entt::entity object, std::optional<MagicType> magic)
{
	events::Publish(events::PlayerDeedForMimic {
	    .player = player,
	    .deed = deed,
	    .object = object,
	    .point = PlaceOfObject(object),
	    .magic = magic,
	});
}

void ConsiderLanding(entt::entity object, PlayerNames player)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (object == entt::null || !registry.Valid(object))
	{
		return;
	}
	const auto* transform = registry.TryGet<const components::Transform>(object);
	if (transform == nullptr)
	{
		return;
	}
	const glm::vec2 here(transform->position.x, transform->position.z);
	creature_watching::Landing landing {.player = player};
	registry.Each<const components::Temple, const components::Transform>(
	    [&](entt::entity, const components::Temple& temple, const components::Transform& at) {
		    if (temple.owner == player &&
		        gutils::GetDistanceInMetres(glm::vec2(at.position.x, at.position.z), here) < creature_watching::k_TempleReach)
		    {
			    landing.nearOwnTemple = true;
		    }
	    });
	// the town the thing belongs to: a villager's, or a building's
	entt::entity town = entt::null;
	if (const auto* villager = registry.TryGet<const components::Villager>(object); villager != nullptr)
	{
		town = villager->town;
	}
	else if (registry.AllOf<components::Abode>(object))
	{
		town = abode_villagers::TownOf(object);
	}
	if (town != entt::null && registry.Valid(town))
	{
		if (const auto* t = registry.TryGet<const components::Town>(town); t != nullptr)
		{
			landing.thingsTown = t->owner;
		}
	}
	const auto nearest = map_cells::GetNearestTown(map_coords::FromMetres(here), std::numeric_limits<float>::max());
	if (nearest != entt::null)
	{
		if (const auto* t = registry.TryGet<const components::Town>(nearest); t != nullptr)
		{
			landing.nearestTown = t->owner;
		}
	}
	if (const auto deed = creature_watching::LandingDeed(landing); deed.has_value())
	{
		Consider(player, *deed, object);
	}
}

void ConsiderObjectLands(entt::entity object)
{
	// the player whose interface let go of it last, if one did
	if (const auto dropper = last_dropped::WhoLastDropped(std::as_const(Locator::entitiesRegistry::value()), object))
	{
		ConsiderLanding(object, *dropper);
	}
}

void ConsiderThrownInTheSea(entt::entity thing)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(thing))
	{
		return;
	}
	if (const auto dropper = last_dropped::WhoLastDropped(registry, thing))
	{
		Consider(*dropper, creature_watching::Deed::ThrowInTheSea, thing);
	}
}

bool ShouldMimicBuildingHit(std::optional<PlayerNames> player, uint32_t buildingBodyFlags)
{
	return player.has_value() && (buildingBodyFlags & PhysicsEntry::k_FromHand) != 0;
}

void EmpathiseWithTownDesire(std::optional<PlayerNames> player, TownDesireInfo desire, float weight, entt::entity villager)
{
	if (!player.has_value())
	{
		return;
	}
	events::Publish(events::CreatureEmpathyWithTownDesire {
	    .player = *player,
	    .desire = desire,
	    .weight = weight,
	    .point = PlaceOfObject(villager),
	});
}

void AddMimicEventHandlers(EventManager& manager)
{
	manager.AddHandler<events::PlayerDeedForMimic>(PassDeedToMinds);
	manager.AddHandler<events::CreatureEmpathyWithTownDesire>(PassTownDesireToMinds);
}
} // namespace openblack::ecs::creature_mimic
