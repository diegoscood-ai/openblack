/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureReactionFacts.h"

#include <optional>

#include "Common/GUtilsDistance.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/Effects/FleeGeometry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs::creature_reaction_facts
{
namespace rules = effects::creature_reaction;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// How impressive a row of the creature's action table is; a number past the table reads as not impressive
uint32_t ImpressiveTypeOf(uint32_t action)
{
	const auto& actions = Locator::infoConstants::value().creatureAction;
	return action < actions.size() ? actions.at(action).impressiveType : rules::k_NotImpressive;
}

/// How impressive the last action the creature carried out to its end was
uint32_t LastImpressiveType(entt::entity creature)
{
	const auto* mind = Entities().TryGet<const components::CreatureMindState>(creature);
	return ImpressiveTypeOf(mind != nullptr ? mind->lastCompletedAction : 0);
}

/// The action of the plan the creature holds as others see it (none: 0)
uint32_t CurrentAction(entt::entity creature)
{
	const auto plan =
	    Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().HeldPlanOf(creature) : std::nullopt;
	return plan.has_value() ? plan->action : 0;
}
} // namespace

bool IsCreature(entt::entity thing)
{
	auto& registry = Entities();
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<components::Creature>(thing);
}

rules::TurnsFacts TurnsFactsOf(entt::entity object, uint8_t type, float distance, bool again)
{
	const auto& info = Locator::infoConstants::value().reaction.at(type);
	rules::TurnsFacts facts {
	    .isCreature = IsCreature(object),
	    .distance = distance,
	    .metresPerCell = gutils::k_MetresPerCell,
	    .maxReactionDistance = info.maxReactionDistance,
	    .howImportantIsDistance = info.howImportantIsDistance,
	    .turns = again ? info.numGameTurnsForNormalThingsBeforeReactingAgain : info.numGameTurnsForNormalThingsToReact,
	};
	if (!facts.isCreature)
	{
		return facts;
	}
	facts.impressiveType = LastImpressiveType(object);
	const auto action = CurrentAction(object);
	if (action < rules::k_ActionRows)
	{
		facts.currentActionImpressiveType = ImpressiveTypeOf(action);
	}
	return facts;
}

glm::vec3 MovementOf(entt::entity creature)
{
	const auto& registry = Entities();
	const auto& transform = registry.Get<const components::Transform>(creature);
	return effects::flee::CreatureMovement(creature_pose::BodyYaw(registry, creature, transform.rotation));
}
} // namespace openblack::ecs::creature_reaction_facts
