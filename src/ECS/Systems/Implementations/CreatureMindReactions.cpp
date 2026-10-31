/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a creature's mind does about the miracles cast near it: it runs from a nasty one or goes to look at it, goes to
// look at a nice one, and learns them by doing so. Also what it does on taking up another creature's reaction, a flying
// thing's or the hand's picking something up, and about a fire. When it does so, and whether it learns, is the
// creatures' reaction handler's (CreatureReactions.cpp)

#define LOCATOR_IMPLEMENTATIONS

#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "Common/GameRandom.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFireReaction.h"
#include "Creature/CreatureMiracleReactions.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureReactionRules.h"
#include "CreatureMindSystem.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// Seeing a nasty miracle adds this much to its fear
constexpr float k_FearOfScaryMagic = 0.5f;
/// The game's actions for putting a fire out with the water miracle and for running from a thing
constexpr std::string_view k_PutOutFireAction = "PutOutFireWithMagicWater";
constexpr std::string_view k_RunAwayAction = "RunAwayFromObject";

/// On the learning leash
bool OnRope(entt::entity creature)
{
	if (!Locator::leashSystem::has_value())
	{
		return false;
	}
	const auto& leash = Locator::leashSystem::value();
	return leash.IsLeashed(creature) && leash.TypeOf(creature) == LeashType::Rope;
}
} // namespace

void CreatureMindSystem::ReactToNastyMagic(entt::entity creature, const glm::vec3& point, std::optional<size_t> learn)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	const auto* body = std::as_const(registry).TryGet<const Creature>(creature);
	if (mind == nullptr || body == nullptr || !mind->desires.has_value())
	{
		return;
	}
	auto& desires = *mind->desires;
	creature_desires::ChangeSource(desires, creature_desires::sources::k_FearFromScaryMagic, k_FearOfScaryMagic);
	// (inferred) the fear the game reads here is the desire to be afraid, read after it grew
	const bool curious =
	    creature_reaction_rules::CuriousAboutNastyMagic(OnRope(creature), desires[creature_desires::Desire::Fear].value);
	const float height = body->size * creature_morph::k_HeightAtSizeOne;
	const auto random = [](uint32_t range) { return Random(range); };
	const glm::vec2 at(point.x, point.z);
	Replan(creature, creature_mind::Activity::Planned,
	       curious ? creature_mind::ExamineMiracle(at, height, random) : creature_mind::RunAwayFromMiracle(at, height, random));
	if (learn.has_value())
	{
		WatchMiracle(creature, *learn);
	}
}

void CreatureMindSystem::ReactToNiceMagic(entt::entity creature, const glm::vec3& point, std::optional<size_t> learn)
{
	const auto* body = std::as_const(Locator::entitiesRegistry::value()).TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return;
	}
	const auto random = [](uint32_t range) { return Random(range); };
	Replan(creature, creature_mind::Activity::Planned,
	       creature_mind::ExamineMiracle({point.x, point.z}, body->size * creature_morph::k_HeightAtSizeOne, random));
	if (learn.has_value())
	{
		WatchMiracle(creature, *learn);
	}
}

void CreatureMindSystem::TakeUpReaction(entt::entity creature, entt::entity thing, const creature_reaction_rules::Setup& setup)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr)
	{
		return;
	}
	const bool there = thing != entt::null && registry.Valid(thing);
	if (setup.comesAcross && there)
	{
		ComeAcross(creature, *mind, thing);
	}
	// A plan done only to a thing of some use drops nothing for one the plan already acts on or one of no use
	const bool wanted =
	    !setup.plan.has_value() || !setup.plan->onlyIfUseful ||
	    (there &&
	     !(mind->planner.current.has_value() && mind->planner.current->object == std::optional(entt::to_integral(thing))) &&
	     ObjectToActOnUsefulness(creature, *mind, setup.plan->desire, thing) > 0.0f);
	// Not remembered as a plan its player forced on it, as no reaction's plan is
	if (setup.plan.has_value() && (setup.plan->onSelf || there) && wanted)
	{
		ForceActivity(creature, {.desire = setup.plan->desire,
		                         .action = setup.plan->action,
		                         .actionObject = setup.plan->onSelf ? creature : thing,
		                         .byPlayer = false});
	}
	// The hand's pick-up sets the watch it was just given to a minute and up to another
	if (setup.longHandWatch)
	{
		constexpr uint32_t k_TurnsPerSecond = 1000 / game_clock::k_MsPerTurn;
		const auto seconds = (static_cast<float>(Random(game_random::k_FloatRandRange)) *
		                      creature_reaction_rules::k_HandPickUpWatchSeconds * game_random::FloatRandScale()) +
		                     creature_reaction_rules::k_HandPickUpWatchSeconds;
		const auto turns = static_cast<uint32_t>(seconds * static_cast<float>(k_TurnsPerSecond));
		for (auto& step : mind->idle.agenda)
		{
			if (step.kind == creature_mind::Step::Kind::WatchHand)
			{
				step.turns = turns;
			}
		}
	}
}

void CreatureMindSystem::ReactToFire(entt::entity creature, entt::entity burning)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || !mind->desires.has_value() || !registry.Valid(burning))
	{
		return;
	}
	using creature_desires::Desire;
	// It comes across the burning thing, then once more what that belongs to; a burning creature is its own
	ComeAcross(creature, *mind, burning);
	const auto owner = BelongsTo(burning);
	if (owner.has_value() && *owner != burning)
	{
		ComeAcross(creature, *mind, *owner);
	}
	// What it weighs is what the burning thing belongs to, by how useful it has learnt that is to its compassion
	const auto usefulness =
	    owner.has_value() ? std::optional(ActivityUsefulness(creature, *mind, Desire::Compassion, *owner)) : std::nullopt;
	const float compassion = (*mind->desires)[Desire::Compassion].value;
	const auto sightings = MiracleSightings(creature, MagicType::Water);
	// (openblack, guard) without the mind's tables it can't tell how often it has seen the miracle: it runs
	const auto response = sightings.has_value()
	                          ? creature_fire::Choose(usefulness, compassion, sightings->first, sightings->second)
	                          : creature_fire::Response::RunAway;
	const bool putsOut = response == creature_fire::Response::PutOut;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} takes up the fire on {}: it {} (usefulness {}, compassion {})",
	                    entt::to_integral(creature), entt::to_integral(burning),
	                    putsOut ? "goes to put it out with water" : "runs from it", usefulness.value_or(-1.0f), compassion);
	// Neither plan is remembered as one its player forced on it
	if (putsOut)
	{
		std::ignore = ForceActivity(creature, {.desire = Desire::Compassion,
		                                       .activityObject = owner,
		                                       .action = k_PutOutFireAction,
		                                       .actionObject = burning,
		                                       .byPlayer = false});
		return;
	}
	std::ignore = ForceActivity(
	    creature, {.desire = Desire::Fear, .action = k_RunAwayAction, .actionObject = burning, .byPlayer = false});
}

bool CreatureMindSystem::NoticeCreatureToInspect(entt::entity creature, entt::entity other, uint32_t turn, uint32_t waitTurns)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || !registry.Valid(other))
	{
		return false;
	}
	ComeAcross(creature, *mind, other);
	auto* belief = mind->beliefs.Find(other);
	if (belief == nullptr || !creature_reaction_rules::InspectDue(belief->inspected, turn, waitTurns))
	{
		return false;
	}
	belief->inspected = turn;
	return true;
}
