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
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureReactionRules.h"
#include "Creature/CreatureTeleportAgendas.h"
#include "Creature/CreatureWatching.h"
#include "CreatureMindSystem.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "GameClock.h"
#include "Input/LocalPlayer.h"
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
/// The game's actions for running away from a place and for going to look at one
constexpr std::string_view k_RunFromPlaceAction = "RunAwayFromPos";
constexpr std::string_view k_ExaminePlaceAction = "ExaminePos";

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
	if (mind == nullptr || !registry.AllOf<Creature>(creature) || !mind->desires.has_value())
	{
		return;
	}
	auto& desires = *mind->desires;
	creature_desires::ChangeSource(desires, creature_desires::sources::k_FearFromScaryMagic, k_FearOfScaryMagic);
	// (inferred) the fear the game reads here is the desire to be afraid, read after it grew
	const bool curious =
	    creature_reaction_rules::CuriousAboutNastyMagic(OnRope(creature), desires[creature_desires::Desire::Fear].value);
	// The plan is forced on it about itself, where the miracle stands kept on its mind, and not by its player
	mind->miraclePlace = glm::vec2(point.x, point.z);
	ForceActivity(creature, {
	                            .desire = curious ? creature_desires::Desire::Curiosity : creature_desires::Desire::Fear,
	                            .action = curious ? k_ExaminePlaceAction : k_RunFromPlaceAction,
	                            .actionObject = creature,
	                            .byPlayer = false,
	                        });
	if (learn.has_value())
	{
		WatchMiracle(creature, *learn);
	}
}

void CreatureMindSystem::CastOnLeashLater(entt::entity creature, MagicType magic)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* body = mind != nullptr ? std::as_const(registry).TryGet<const Creature>(creature) : nullptr;
	if (body == nullptr || !mind->learnt.has_value() || !Locator::leashSystem::has_value() ||
	    !Locator::handSystem::has_value() || !Locator::entitiesMap::has_value())
	{
		return;
	}
	// A leash of its player's must be on, held or tied: on the one creature its player leads, whether or not that is
	// this one. Only the local player's hand keeps a place here, so only that player's creatures cast
	const auto& leashes = Locator::leashSystem::value();
	const auto led = leashes.PlayersCreature(body->owner);
	if (body->owner != creature::LocalPlayer() || !led.has_value() || !leashes.IsLeashed(*led))
	{
		return;
	}
	// It must know the miracle
	if (!creature_watching::Knows(mind->learnt->knowledge, 1, static_cast<uint32_t>(magic)))
	{
		return;
	}
	// The first thing other than itself in the cells out from the place of its player's hand
	const auto& map = Locator::entitiesMap::value();
	const auto thing = creature_reaction_rules::LeashCastTarget(Locator::handSystem::value().GetSyncedPlace(), creature,
	                                                            [&map](glm::ivec2 cell) { return map.GetAllInCell(cell); });
	if (!thing.has_value())
	{
		return;
	}
	// The cast takes the place of anything else it was left to do next
	mind->leashCast = CreatureMindState::LeashCast {.thing = *thing, .magic = magic};
	mind->afterFeedback.reset();
	mind->useWhatItHolds = false;
	mind->highPending = false;
	if (auto* casting = registry.TryGet<CreatureCasting>(creature))
	{
		casting->fizzle.reset();
	}
	// and it is marked as led by the leash. Only a creature that wears a leash keeps the mark
	if (auto* leash = registry.TryGet<CreatureLeash>(creature); leash != nullptr && leashes.IsLeashed(creature))
	{
		leash->control = CreatureLeash::Control::Led;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} will cast miracle {} at {} for the leash",
	                    entt::to_integral(creature), static_cast<uint32_t>(magic), entt::to_integral(*thing));
}

bool CreatureMindSystem::CastOnLeash(entt::entity creature, CreatureMindState& mind)
{
	if (!mind.leashCast.has_value())
	{
		return false;
	}
	const auto cast = *mind.leashCast;
	mind.leashCast.reset();
	// It plans nothing for itself meanwhile, as when an order is carried out, whether or not it can cast
	mind.commanded = true;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	const auto row = static_cast<size_t>(cast.magic);
	if (tables == nullptr || row >= tables->miracles.size() || !registry.Valid(cast.thing))
	{
		return true;
	}
	const auto action = tables->miracles[row].action;
	if (action >= tables->actions.size() || tables->actions[action].desire >= creature_desires::k_DesireCount)
	{
		return true;
	}
	// The action its body plays ends at twice the usual speed, then what it was doing stops as a failure and it casts at
	// the thing, for what the thing belongs to, or for itself when the thing belongs to nothing
	if (auto* animation = registry.TryGet<CreatureAnimation>(creature))
	{
		animation->body = creature_layers::Hurry(animation->body);
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} casts miracle {} at {} for the leash", entt::to_integral(creature),
	                    row, entt::to_integral(cast.thing));
	std::ignore = ForceActivity(creature, {.desire = static_cast<creature_desires::Desire>(tables->actions[action].desire),
	                                       .activityObject = BelongsTo(cast.thing).value_or(creature),
	                                       .action = tables->actions[action].name,
	                                       .actionObject = cast.thing,
	                                       .byPlayer = false});
	mind.commanded = true;
	return true;
}

void CreatureMindSystem::ReactToNiceMagic(entt::entity creature, const glm::vec3& point, std::optional<size_t> learn)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (mind == nullptr || !registry.AllOf<Creature>(creature))
	{
		return;
	}
	// The plan to go and look is forced on it about itself, where the miracle stands kept on its mind, and not by its
	// player
	mind->miraclePlace = glm::vec2(point.x, point.z);
	ForceActivity(creature, {
	                            .desire = creature_desires::Desire::Curiosity,
	                            .action = k_ExaminePlaceAction,
	                            .actionObject = creature,
	                            .byPlayer = false,
	                        });
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

void CreatureMindSystem::TakeUpTeleport(entt::entity creature, entt::entity stone)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || !registry.Valid(stone))
	{
		return;
	}
	// It comes across the stone and, curious, goes to it and asks it for a jump, a plan not remembered as its player's;
	// whatever came of the plan, it is commanded and the stone controls it
	ComeAcross(creature, *mind, stone);
	const bool planned = ForceActivity(creature, {.desire = creature_desires::Desire::Curiosity,
	                                              .action = creature_mind::k_GoToTeleportAction,
	                                              .actionObject = stone,
	                                              .byPlayer = false});
	mind->commanded = true;
	mind->controlledByTeleport = true;
	if (const auto logger = spdlog::get("game"))
	{
		logger->trace("Creature {} turns aside into the teleport stone {}: {}", entt::to_integral(creature),
		              entt::to_integral(stone), planned ? "it goes to it" : "no plan");
	}
}
