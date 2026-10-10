/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureReactions.h"

#include <cstdio>
#include <cstdlib>

#include <optional>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/CreatureBody.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureReactionRules.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureReaction.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"

using namespace openblack;
using namespace openblack::ecs;
using components::CreatureReaction;

namespace
{
namespace reactions = effects::reactions;
namespace rules = creature_reaction_rules;
using reactions::Reaction;

constexpr uint8_t k_FleeFromSpell = static_cast<uint8_t>(openblack::Reaction::FleeFromSpell);
constexpr uint8_t k_ReactToMagicShield = static_cast<uint8_t>(openblack::Reaction::ReactToMagicShield);
constexpr uint8_t k_LookAtNiceSpell = static_cast<uint8_t>(openblack::Reaction::LookAtNiceSpell);
constexpr uint8_t k_ReactToImpressiveSpell = static_cast<uint8_t>(openblack::Reaction::ReactToImpressiveSpell);

Registry& Entities()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		std::fputs("ecs::creature_reactions: no entities registry in the locator (Locator::entitiesRegistry)\n", stderr);
		std::abort();
	}
	return Locator::entitiesRegistry::value();
}

const InfoConstants& Constants()
{
	if (!Locator::infoConstants::has_value())
	{
		std::fputs("ecs::creature_reactions: no game constants in the locator (Locator::infoConstants)\n", stderr);
		std::abort();
	}
	return Locator::infoConstants::value();
}

const ReactionInfo& Info(uint8_t type)
{
	return Constants().reaction.at(type);
}

uint8_t TypeOf(const Reaction& reaction)
{
	return static_cast<uint8_t>(reaction.type);
}

/// The types a creature takes up so far; it scores every other one 0
bool Ported(uint8_t type)
{
	return type == k_FleeFromSpell || type == k_ReactToMagicShield || type == k_LookAtNiceSpell ||
	       type == k_ReactToImpressiveSpell;
}

/// The creature's living info (its species' row of the creature table): its isReacting[type]
bool IsReactingTo(const components::Creature& creature, uint8_t type)
{
	const auto& table = Constants().creature;
	const auto row = creature::InfoRow(creature.species);
	if (row >= table.size())
	{
		return false;
	}
	return ReactsTo(table.at(row).isReacting, type);
}

/// Where a thing is, as MapCoords in metres (x and z, y above the land): its Transform's, or a miracle's own (a miracle
/// has no Transform); none when it has neither
std::optional<glm::vec3> MapPositionOf(const Registry& registry, entt::entity thing)
{
	if (!registry.Valid(thing))
	{
		return std::nullopt;
	}
	if (const auto* transform = registry.TryGet<const components::Transform>(thing); transform != nullptr)
	{
		return magic::ToMap(transform->position);
	}
	if (const auto* spell = registry.TryGet<const components::Spell>(thing); spell != nullptr)
	{
		return spell->position;
	}
	return std::nullopt;
}

map_coords::MapCoords CoordsOf(const glm::vec3& mapPosition)
{
	return map_coords::FromMetres(glm::vec2(mapPosition.x, mapPosition.z));
}

const components::Spell* SpellOf(const Registry& registry, entt::entity thing)
{
	return registry.Valid(thing) ? registry.TryGet<const components::Spell>(thing) : nullptr;
}

/// The miracle's seed, if it still has one
const components::SpellSeed* SeedOf(const Registry& registry, const components::Spell& spell)
{
	return registry.Valid(spell.seed) ? registry.TryGet<const components::SpellSeed>(spell.seed) : nullptr;
}

bool CastByItself(const components::Spell* spell, entt::entity creature)
{
	return spell != nullptr && spell->creator.kind == components::SpellCreator::Kind::Creature &&
	       spell->creator.entity == creature;
}

/// The type table's priority function of each ported type
uint32_t Priority(const Registry& registry, uint8_t type, entt::entity creature, entt::entity initiator)
{
	const auto* spell = SpellOf(registry, initiator);
	switch (type)
	{
	case k_FleeFromSpell:
	{
		if (CastByItself(spell, creature))
		{
			return 0;
		}
		const auto here = MapPositionOf(registry, creature);
		const auto there = MapPositionOf(registry, initiator);
		// (openblack, guard) no position for either: as far as can be
		const int32_t d = here.has_value() && there.has_value() ? gutils::FastDistance(CoordsOf(*here), CoordsOf(*there))
		                                                        : rules::k_FleeFromSpellRange;
		return rules::FleeFromSpellPriority(false, d, Info(type).priority);
	}
	case k_ReactToMagicShield:
		return static_cast<uint8_t>(Info(type).priority);
	case k_LookAtNiceSpell:
	case k_ReactToImpressiveSpell:
		// the impressive miracle's reaction reads the nice miracle's row
		return rules::LookAtNiceSpellPriority(CastByItself(spell, creature), Info(k_LookAtNiceSpell).priority);
	default:
		return 0;
	}
}

/// The reaction's score for the creature at that distance (ECS/Effects/Reactions: Score)
uint32_t Score(const Registry& registry, uint8_t type, entt::entity creature, entt::entity initiator, float distance)
{
	const auto* body = registry.TryGet<const components::Creature>(creature);
	if (body == nullptr || !Ported(type) || !IsReactingTo(*body, type) || distance > Info(type).maxReactionDistance)
	{
		return 0;
	}
	return reactions::Score(type, true, Priority(registry, type, creature, initiator), distance);
}

/// The creature's turns of a reaction, and before it takes the same type again: the creature's own of the table, as for
/// all four ported types (the shield's forward to them)
int32_t TurnsToReact(uint8_t type, float distance)
{
	const auto& info = Info(type);
	return rules::TurnsToReact(info.maxReactionDistance, info.howImportantIsDistance, distance,
	                           info.numGameTurnsForCreatureToReact);
}

int32_t TurnsBeforeReactingAgain(uint8_t type, float distance)
{
	const auto& info = Info(type);
	return rules::TurnsBeforeReactingAgain(info.maxReactionDistance, info.howImportantIsDistance, distance,
	                                       info.numGameTurnsForCreatureBeforeReactingAgain);
}

/// Whether the creature takes up a reaction of the type now. Of the game's tests only these are known: available,
/// reactions on, not fighting, not out cold (fainted, or knocked out when its life ran out) and, while it mimics its
/// player, only a type whose priority is above 150. The game also needs some of the creature's and its mind's state
/// clear, not modelled as what it stands for is not known (the creature page's Pending), and it is not in the dance
/// editor (none here). A stilled mind, as by the freeze spell, is not among the tests: it still takes a reaction up and
/// is only kept from learning by it. Reactions are on for every creature openblack makes: the game sets it so when a
/// creature is made; only CREATURE_REACTION, which no land script calls, and the creatures the challenge script's
/// CREATE and CREATURE_CREATE_RELATIVE_TO_CREATURE make, neither of which makes a creature here, have them off
bool IsAvailableForReaction(const Registry& registry, entt::entity creature, uint8_t type)
{
	const auto* mind = registry.TryGet<const components::CreatureMindState>(creature);
	const rules::Availability state {
	    .available = ecs::IsAvailable(creature),
	    .reactionsOn = true,
	    .fighting = Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().IsFighting(creature),
	    .fainted = mind != nullptr && mind->idle.activity == creature_mind::Activity::Faint,
	    .knockedOut = registry.AllOf<components::CreatureKnockedOut>(creature),
	    .mimicking = mind != nullptr && mind->learnt.has_value() && mind->learnt->mimicry.has_value(),
	};
	return rules::IsAvailableForReaction(state, Info(type).priority);
}

/// What is known of the miracle that started a reaction
rules::SpellFacts FactsOf(const Registry& registry, const components::Spell& spell)
{
	using Kind = components::SpellCreator::Kind;
	const auto* seed = SeedOf(registry, spell);
	rules::SpellFacts facts {
	    .hasCreator = spell.creator.kind != Kind::None,
	    .creatorIsSpellIcon = spell.creator.kind == Kind::WorshipSpellIcon,
	    .creatorIsCreature = spell.creator.kind == Kind::Creature,
	    .hasSeed = seed != nullptr,
	    .seedLearnedFrom = seed != nullptr && seed->learnedFrom,
	};
	// the player's type is only asked for when it decides: a creator that is not a creature, and a player
	if (facts.hasCreator && !facts.creatorIsCreature && spell.hasPlayer)
	{
		facts.playerType = magic::players::MagicOf(spell.player).playerType;
	}
	return facts;
}

/// The creature follows the reaction, among its followers. (pending) How impressed the creature is by it is not
/// updated: the game adds 4 x the initiator's impressive value to how impressed it is by its own player for a reaction
/// of that player's, else 12 x it to how impressive it finds the creature that started it; the impressive values are
/// not ported
void AddReaction(entt::entity creature, const Reaction& reaction)
{
	auto& state = Entities().AssignOrReplace<CreatureReaction>(creature);
	state.reaction = reaction.id;
	state.type = TypeOf(reaction);
	state.object = reaction.initiator;
	state.follower = true;
}

/// No creature learns from the seed's miracles again
void MarkSeed(Registry& registry, entt::entity seed)
{
	if (registry.Valid(seed))
	{
		if (auto* component = registry.TryGet<components::SpellSeed>(seed); component != nullptr)
		{
			component->learnedFrom = true;
		}
	}
}

std::optional<size_t> LearnOf(bool learn, MagicType magic)
{
	const auto learnt = learn ? rules::LearntBySight(magic) : std::nullopt;
	return learnt.has_value() ? std::optional(static_cast<size_t>(*learnt)) : std::nullopt;
}

/// A nasty miracle: it grows afraid and runs or goes to look, then may learn it, then takes the reaction up. (pending)
/// On any leash, with the miracle known, the game then looks for the first thing near the leash's holder to cast it
/// at; the creature's own casting is not ported
void ReactToNastyMagic(entt::entity creature, const Reaction& reaction, MagicType magic, const glm::vec3& point)
{
	auto& registry = Entities();
	const auto* spell = SpellOf(registry, reaction.initiator);
	// the seed, kept before the mind acts
	const auto seed = spell != nullptr ? spell->seed : entt::null;
	const auto outcome =
	    rules::NastyMagic(spell != nullptr ? std::optional(FactsOf(registry, *spell)) : std::optional<rules::SpellFacts> {});
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().ReactToNastyMagic(creature, point, LearnOf(outcome.learn, magic));
	}
	if (outcome.markSeed)
	{
		MarkSeed(registry, seed);
	}
	if (outcome.takeUp)
	{
		AddReaction(creature, reaction);
	}
}

/// A nice miracle or a shield: nothing for a reaction of a player neither its own nor an ally's (openblack has no
/// alliances: of any other player), then it may go to look and learn, then takes the reaction up
void ReactToNiceMagic(entt::entity creature, const Reaction& reaction, MagicType magic, const glm::vec3& point)
{
	auto& registry = Entities();
	const auto* spell = SpellOf(registry, reaction.initiator);
	const auto& body = std::as_const(registry).Get<const components::Creature>(creature);
	// a miracle with no player makes a reaction with none; openblack's neutral player stands for none elsewhere
	const bool hasPlayer = spell != nullptr ? spell->hasPlayer : reaction.player != PlayerNames::NEUTRAL;
	const bool othersPlayer = hasPlayer && reaction.player != body.owner;
	const auto seed = spell != nullptr ? spell->seed : entt::null;
	const auto outcome = rules::NiceMagic(
	    othersPlayer, magic, spell != nullptr ? std::optional(FactsOf(registry, *spell)) : std::optional<rules::SpellFacts> {});
	if (outcome.examine && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().ReactToNiceMagic(creature, point, LearnOf(outcome.learn, magic));
	}
	if (outcome.markSeed)
	{
		MarkSeed(registry, seed);
	}
	if (outcome.takeUp)
	{
		AddReaction(creature, reaction);
	}
}

/// The type table's start function. (not modelled) The game first keeps the plan the creature had; nothing ported reads
/// it back
void StartReacting(entt::entity creature, const Reaction& reaction)
{
	const auto& registry = std::as_const(Entities());
	const auto* spell = SpellOf(registry, reaction.initiator);
	const auto point = MapPositionOf(registry, reaction.initiator);
	const uint8_t type = TypeOf(reaction);
	// (openblack, guard) the game reads the miracle's type from what must be a miracle for these types
	if (!point.has_value() || (type != k_ReactToMagicShield && spell == nullptr))
	{
		return;
	}
	switch (type)
	{
	case k_FleeFromSpell:
		ReactToNastyMagic(creature, reaction, spell->magicType, *point);
		break;
	case k_ReactToMagicShield:
		// a shield teaches the spiritual shield whatever its kind
		ReactToNiceMagic(creature, reaction, MagicType::Shield, *point);
		break;
	case k_LookAtNiceSpell:
	case k_ReactToImpressiveSpell:
		ReactToNiceMagic(creature, reaction, spell->magicType, *point);
		break;
	default:
		break;
	}
}

void CreatureReactionHandler(entt::entity creature, const Reaction& reaction, float d)
{
	const auto& registry = std::as_const(Entities());
	const uint8_t type = TypeOf(reaction);
	// every other type scores 0, and nothing the creature is asked changes anything: it is left alone
	if (!Ported(type) || !registry.AllOf<components::Creature>(creature) || !IsAvailableForReaction(registry, creature, type))
	{
		return;
	}
	const auto again = TurnsBeforeReactingAgain(type, d);
	const auto now = reactions::Turn();
	const auto* held = registry.TryGet<const CreatureReaction>(creature);
	if (held == nullptr || held->reaction == 0)
	{
		if (Score(registry, type, creature, reaction.initiator, d) > 0 &&
		    reactions::Records(creature, type, static_cast<uint32_t>(again), now))
		{
			reactions::MarkStarted(reaction.id, now);
			StartReacting(creature, reaction);
		}
		return;
	}
	// already reacting: the new one must score more, and the one it follows must have lasted long enough
	const auto* current = reactions::Find(held->reaction);
	// (openblack, guard) the one it follows has gone without its end reaching the creature
	if (current == nullptr)
	{
		return;
	}
	const uint8_t currentType = TypeOf(*current);
	if (currentType == type && (!reactions::SameTypeSwitch(type) || current->id == reaction.id))
	{
		return;
	}
	const float distance = reactions::DistanceToReactionCell(creature, *current);
	const auto cur = static_cast<float>(Score(registry, currentType, creature, current->initiator, distance));
	const auto score = static_cast<float>(Score(registry, type, creature, reaction.initiator, d));
	constexpr uint32_t k_TurnsPerSecond = 1000 / game_clock::k_MsPerTurn;
	const auto seconds = static_cast<float>((now - reactions::RecordTurn(creature, currentType)) / k_TurnsPerSecond);
	if (!reactions::MaySwitch(cur, score, seconds, currentType))
	{
		return;
	}
	reactions::SetReactionDoneWhen(creature, type, now);
	// it leaves the old reaction's followers, its record left as it is, and takes the new one up
	Entities().Get<CreatureReaction>(creature).follower = false;
	StartReacting(creature, reaction);
}

/// The end of a reaction stops the creatures following it
void ShutDownReaction(uint32_t reaction)
{
	auto& registry = Entities();
	std::vector<entt::entity> followers;
	std::as_const(registry).Each<const CreatureReaction>(
	    [&followers, reaction](entt::entity creature, const CreatureReaction& state) {
		    if (state.follower && state.reaction == reaction)
		    {
			    followers.push_back(creature);
		    }
	    });
	for (const auto creature : followers)
	{
		creature_reactions::StopReacting(creature);
	}
}
} // namespace

void creature_reactions::RegisterHandlers()
{
	reactions::SetLivingReactionHandler(reactions::LivingClass::Creature, &CreatureReactionHandler);
	reactions::SetLivingShutDownHandler(reactions::LivingClass::Creature, &ShutDownReaction);
}

void creature_reactions::HandleReaction(entt::entity creature, const effects::reactions::Reaction& reaction, float distance)
{
	CreatureReactionHandler(creature, reaction, distance);
}

void creature_reactions::ProcessReaction(entt::entity creature)
{
	const auto& registry = std::as_const(Entities());
	const auto* state = registry.TryGet<const CreatureReaction>(creature);
	if (state == nullptr || state->reaction == 0)
	{
		return;
	}
	const auto* reaction = reactions::Find(state->reaction);
	const auto here = MapPositionOf(registry, creature);
	const auto there = MapPositionOf(registry, state->object);
	// the reaction gone, or what started it: for a creature both stop it the same way
	if (!reactions::IsAvailable(reaction) || !ecs::IsAvailable(state->object) || !here.has_value() || !there.has_value())
	{
		StopReacting(creature);
		return;
	}
	const auto elapsed = static_cast<int32_t>(reactions::Turn() - reactions::RecordTurn(creature, state->type));
	if (elapsed > TurnsToReact(state->type, gutils::GetDistanceInMetres(CoordsOf(*here), CoordsOf(*there))))
	{
		StopReacting(creature);
	}
}

void creature_reactions::StopReacting(entt::entity creature)
{
	auto& registry = Entities();
	auto* state = registry.Valid(creature) ? registry.TryGet<CreatureReaction>(creature) : nullptr;
	if (state == nullptr)
	{
		return;
	}
	if (state->reaction != 0)
	{
		reactions::RefreshRecord(creature, state->type, reactions::Turn());
		state->reaction = 0;
		state->follower = false;
	}
	state->object = entt::null;
}

uint32_t creature_reactions::ReactionOf(entt::entity creature)
{
	const auto* state = std::as_const(Entities()).TryGet<const CreatureReaction>(creature);
	return state != nullptr ? state->reaction : 0;
}
