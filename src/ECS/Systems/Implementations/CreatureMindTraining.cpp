/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the creature's training scripts read of its mind and steer it by: how the player last rewarded or punished it,
// how often it carried out each action, a desire made its only one, and pointing out a lesson highlight.

#define LOCATOR_IMPLEMENTATIONS

#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureTraining.h"
#include "Creature/LeashRules.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Transform.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHighlight.h"
#include "Enums.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using creature_desires::Desire;

namespace
{
/// The whole turns a second a desire made dominant for a while is counted in, as the leash and the mind's turn count it
float DominantTurnsPerSecond()
{
	const auto msPerTurn = game_clock::MsPerTurn();
	return msPerTurn > 0 ? static_cast<float>(1000 / msPerTurn) : 0.0f;
}
} // namespace

float CreatureMindSystem::GetInteractionMagnitude(entt::entity creature) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	return mind != nullptr ? mind->interactionMagnitude : 0.0f;
}

void CreatureMindSystem::ClearInteractionMagnitude(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr)
	{
		mind->interactionMagnitude = 0.0f;
	}
}

uint32_t CreatureMindSystem::GetActionCount(entt::entity creature, uint32_t action) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	return mind != nullptr ? creature_training::TimesCarriedOut(mind->actionCounts, action) : 0;
}

void CreatureMindSystem::SetOnlyDesire(entt::entity creature, Desire desire, float seconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* body = mind != nullptr ? std::as_const(registry).TryGet<const Creature>(creature) : nullptr;
	if (body == nullptr)
	{
		return;
	}
	// A creature told before it has first thought has its desires set up for it
	SetUpMind(creature, body->species, *mind);
	if (!mind->desires.has_value())
	{
		return;
	}
	// The creature keeps one desire made dominant at a time, which the leash and the mood spells share; the body's needs
	// are held down too
	mind->dominantDesire =
	    creature_spell_mind::SetCheatDominant(*mind->desires, desire, true, DominantTurnsPerSecond(),
	                                          mind_detail::DesireFloorOf(std::as_const(registry), creature), seconds);
}

void CreatureMindSystem::ClearOnlyDesire(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || !mind->desires.has_value())
	{
		return;
	}
	// The desires held down are let go only for a creature of no player, or whose player's leash in use is the learning
	// leash: the one worn, or with none on the one last worn or picked
	const auto* body = std::as_const(registry).TryGet<const Creature>(creature);
	const auto* leashes = std::as_const(registry).TryGet<const CreatureLeash>(creature);
	const auto leashInUse = leashes == nullptr          ? creature_leash::k_LeashInUseAtStart
	                        : leashes->worn.has_value() ? leashes->worn->type
	                                                    : leashes->selected;
	const bool letGo = creature_leash::OnlyDesireLetsGo(body != nullptr && body->owner != PlayerNames::NEUTRAL, leashInUse);
	creature_spell_mind::ClearOnlyDesire(*mind->desires, mind->dominantDesire, letGo);
}

bool CreatureMindSystem::PointOutHighlight(entt::entity creature)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* transform = registry.Valid(creature) ? registry.TryGet<const Transform>(creature) : nullptr;
	if (transform == nullptr || !Locator::entitiesMap::has_value())
	{
		return false;
	}
	const auto& map = Locator::entitiesMap::value();
	const auto highlight = creature_training::HighlightToPointOut(
	    map_coords::FromMetres({transform->position.x, transform->position.z}), [&registry, &map](glm::ivec2 cell) {
		    std::vector<creature_training::HighlightInCell> highlights;
		    for (const auto entity : map.GetAllInCell(cell))
		    {
			    if (const auto* found = registry.TryGet<const ScriptHighlight>(entity))
			    {
				    highlights.push_back(
				        {.entity = entity, .tipSign = ecs::script_highlight::IsDidYouKnowInfo(found->infoIndex)});
			    }
		    }
		    return highlights;
	    });
	if (!highlight.has_value())
	{
		return false;
	}
	// It is to obey its player by pointing the highlight out. openblack's creature can't carry that action out yet, so
	// it is only logged and the creature goes on with what it was doing
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} can't point out highlight {} yet", entt::to_integral(creature),
	                    entt::to_integral(*highlight));
	return true;
}
