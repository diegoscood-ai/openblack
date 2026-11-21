/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A creature's mind when the first search of a walk finds it shut in: what it can clear out of its way, the thing it
// clears as its player's order, and the walk on afterwards (creature_clear_way). The creature's walk calls them.

#define LOCATOR_IMPLEMENTATIONS

#include <optional>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "Creature/CreatureClearWay.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureWatching.h"
#include "CreatureMindSystem.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Temple.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "Enums.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using creature_desires::Desire;

namespace
{
/// The actions of the game's table that throw a thing and kick it, and the one of the walk on
constexpr std::string_view k_ThrowAroundAction = "ThrowAround";
constexpr std::string_view k_KickAction = "Kick";
constexpr std::string_view k_WalkOnAction = "MoveToPos";
/// The plans forced on it here are as pressing as the game's own forced plans of a priority of one
constexpr float k_ForcedPriority = 1.0f;
} // namespace

std::optional<uint32_t> CreatureMindSystem::ActionToClear(entt::entity creature, entt::entity thing)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* tables = GetTables();
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	if (tables == nullptr || mind == nullptr || !mind->learnt.has_value() || !registry.Valid(thing) ||
	    !Locator::creatureObjectActionSystem::has_value())
	{
		return std::nullopt;
	}
	const auto throwAround = creature_mind_tables::FindAction(*tables, k_ThrowAroundAction);
	const auto kick = creature_mind_tables::FindAction(*tables, k_KickAction);
	if (!throwAround.has_value() || !kick.has_value())
	{
		return std::nullopt;
	}
	const auto type = ecs::abodes::TypeOf(thing);
	const auto& hands = Locator::creatureObjectActionSystem::value();
	const creature_clear_way::Obstacle obstacle {
	    .templeHeart = registry.AllOf<Temple>(thing),
	    .storagePit = type == AbodeType::StoragePit && ecs::abodes::IsBuilt(thing),
	    .townCentre = type == AbodeType::TownCentre,
	    .canPickUp = hands.CanPickUp(creature, thing),
	    .canKick = hands.CanKick(creature, thing),
	};
	// The miracles in the order of the magic types, as it knows them
	std::vector<creature_clear_way::Miracle> miracles;
	miracles.reserve(tables->miracles.size());
	for (size_t magic = 0; magic < tables->miracles.size(); ++magic)
	{
		miracles.push_back({
		    .known = creature_watching::Knows(mind->learnt->knowledge, static_cast<uint32_t>(CreatureActionLearningType::Magic),
		                                      static_cast<uint32_t>(magic)),
		    .desire = tables->miracles[magic].desire,
		    .action = tables->miracles[magic].action,
		});
	}
	const auto action = creature_clear_way::ClearingAction(obstacle, miracles, {.throwAround = *throwAround, .kick = *kick});
	return action.has_value() && *action < tables->actions.size() ? action : std::nullopt;
}

void CreatureMindSystem::ShutIn(entt::entity creature, std::span<const entt::entity> met, glm::vec2 destination)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr)
	{
		return;
	}
	// Where the walk was going is kept whether or not it clears anything
	mind->keptWalkPoint = destination;
	if (mind->trapped || met.empty())
	{
		return;
	}
	const auto chosen = creature_clear_way::Choose(met.size(), [](uint32_t range) { return Random(range); });
	const auto thing = met[*chosen];
	const auto action = ActionToClear(creature, thing);
	const auto* tables = GetTables();
	// It comes across the thing, then is given its clearing as its player's order, what it was doing ended
	ComeAcross(creature, *mind, thing);
	// A plan with no action is no plan: nothing is forced, but it is shut in all the same. A plan forced is commanded
	// with the walk on kept, whether or not its agenda could be made
	if (action.has_value() && tables != nullptr)
	{
		std::ignore = ForceActivity(creature, {.desire = Desire::ObeyPlayer,
		                                       .action = tables->actions[*action].name,
		                                       .actionObject = thing,
		                                       .score = k_ForcedPriority});
		mind->commanded = true;
		// once that is done, it walks on to where it was going
		KeepAfterFeedback(creature, *mind, CreatureMindState::AfterFeedback::WalkToKeptPoint);
		if (const auto logger = spdlog::get("game"))
		{
			logger->debug("Creature {} is shut in and clears {} out of its way: {}", entt::to_integral(creature),
			              entt::to_integral(thing), tables->actions[*action].name);
		}
	}
	mind->trapped = true;
}

void CreatureMindSystem::WalkToKeptPoint(entt::entity creature, CreatureMindState& mind)
{
	if (!mind.keptWalkPoint.has_value())
	{
		return;
	}
	const auto point = *mind.keptWalkPoint;
	std::ignore = ForceActivity(creature, {.desire = Desire::ObeyPlayer,
	                                       .action = k_WalkOnAction,
	                                       .actionObject = creature,
	                                       .score = k_ForcedPriority,
	                                       .agenda = creature_clear_way::WalkOn(point, ecs::object::GetHeight(creature))});
	// carried out as an order is
	mind.commanded = true;
}
