/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureReactionRules.h"

#include <cmath>

#include <algorithm>

#include "ECS/Components/PlayerMagic.h"
#include "ECS/PlayerAlliances.h"

using namespace openblack;
using namespace openblack::creature_reaction_rules;

namespace
{
/// The player of the miracle's creator is a computer player (or, for a nasty miracle, of type 3 too), the creator being
/// neither a creature nor nobody: then the creature does not learn from it
bool CastByComputer(const SpellFacts& spell, bool orType3)
{
	if (!spell.hasCreator || spell.creatorIsCreature || !spell.playerType.has_value())
	{
		return false;
	}
	return *spell.playerType == ecs::components::PlayerMagic::k_ComputerPlayerType || (orType3 && *spell.playerType == 3);
}

/// Cast by a spell icon and no longer from a seed
bool IconWithoutSeed(const SpellFacts& spell)
{
	return spell.hasCreator && spell.creatorIsSpellIcon && !spell.hasSeed;
}

/// The float truncated as the game's conversion does
int32_t Truncate(float value)
{
	return static_cast<int32_t>(static_cast<int64_t>(std::trunc(value)));
}
} // namespace

uint8_t creature_reaction_rules::FleeFromSpellPriority(bool castByItself, int32_t fastDistance, uint32_t priority)
{
	if (castByItself)
	{
		return 0;
	}
	if (fastDistance >= k_FleeFromSpellRange)
	{
		return static_cast<uint8_t>(priority);
	}
	const int32_t nearer = (k_FleeFromSpellRange - fastDistance) * 100 / k_FleeFromSpellRange;
	return static_cast<uint8_t>(priority + static_cast<uint32_t>(nearer));
}

uint8_t creature_reaction_rules::LookAtNiceSpellPriority(bool castByItself, uint32_t niceSpellPriority)
{
	return castByItself ? 0 : static_cast<uint8_t>(niceSpellPriority);
}

int32_t creature_reaction_rules::TurnsToReact(float maxDistance, float importance, float distance, uint32_t turns)
{
	float f = maxDistance - distance;
	f = f * importance;
	f = f / maxDistance;
	f = f + (1.0f - importance);
	f = f * static_cast<float>(static_cast<int32_t>(turns));
	return Truncate(f);
}

int32_t creature_reaction_rules::TurnsBeforeReactingAgain(float maxDistance, float importance, float distance, uint32_t turns)
{
	float f = importance * distance;
	f = f / maxDistance;
	f = f + (1.0f - importance);
	f = f * static_cast<float>(static_cast<int32_t>(turns));
	return Truncate(f);
}

bool creature_reaction_rules::IsAvailableForReaction(const Availability& state, uint32_t typePriority)
{
	if (!state.available || !state.reactionsOn || state.scriptControlled || state.orderedByPlayer || state.plansFight ||
	    state.opponentPlansFight || state.fighting || state.fainted || state.knockedOut || state.inEmergency)
	{
		return false;
	}
	return !state.mimicking || typePriority > k_MimicPriority;
}

Outcome creature_reaction_rules::NastyMagic(const std::optional<SpellFacts>& spell)
{
	if (!spell.has_value())
	{
		return {.learn = true, .takeUp = true};
	}
	if (IconWithoutSeed(*spell))
	{
		return {};
	}
	const bool learn = !CastByComputer(*spell, true) && !spell->seedLearnedFrom;
	return {.learn = learn, .markSeed = learn, .takeUp = true};
}

bool creature_reaction_rules::IsOthersPlayer(bool hasPlayer, PlayerNames reactionPlayer, PlayerNames creaturePlayer,
                                             const ecs::components::PlayerAlliances& reactionPlayers)
{
	return hasPlayer && reactionPlayer != creaturePlayer && !ecs::alliances::IsAllied(reactionPlayers, creaturePlayer);
}

Outcome creature_reaction_rules::NiceMagic(bool othersPlayer, MagicType magic, const std::optional<SpellFacts>& spell)
{
	if (othersPlayer)
	{
		return {};
	}
	if (!spell.has_value())
	{
		if (magic == MagicType::Wood)
		{
			return {};
		}
		return {.examine = true, .learn = true, .takeUp = true};
	}
	if (IconWithoutSeed(*spell))
	{
		return {};
	}
	const bool look = !CastByComputer(*spell, false) && !spell->seedLearnedFrom;
	return {.examine = look, .learn = look, .markSeed = look, .takeUp = true};
}

std::optional<MagicType> creature_reaction_rules::LearntBySight(MagicType magic)
{
	return magic == k_NotLearntBySight ? std::nullopt : std::optional(magic);
}

bool creature_reaction_rules::CuriousAboutNastyMagic(bool onRope, float fear)
{
	// not afraid: no fear above nothing (not a number counts as not afraid)
	return onRope || !(fear > 0.0f);
}

namespace
{
/// Whether a thing seen at a turn is past its wait now
bool PastWait(std::optional<uint32_t> saw, uint32_t turn)
{
	return !saw.has_value() || static_cast<int32_t>(turn - *saw) > static_cast<int32_t>(k_SightingTurns);
}
} // namespace

bool creature_reaction_rules::BlocksInspecting(const Facts& facts)
{
	return facts.thingIsSelf || facts.thingGone || !facts.thingIsCreature || facts.orderedByPlayer || facts.hasOpponent ||
	       facts.planActsOnThing;
}

bool creature_reaction_rules::InspectDue(std::optional<uint32_t> inspected, uint32_t turn, uint32_t waitTurns)
{
	return !inspected.has_value() || turn - *inspected > waitTurns;
}

uint32_t creature_reaction_rules::InspectWaitTurns(float seconds, uint32_t turnsPerSecond)
{
	return static_cast<uint32_t>(static_cast<float>(turnsPerSecond) * seconds);
}

std::optional<uint8_t> creature_reaction_rules::Priority(Reaction type, const Facts& facts,
                                                         const std::function<uint32_t(uint32_t)>& random)
{
	// Another player's flying thing is mostly passed over, by a draw made only then
	const auto othersPassedOver = [&]() { return facts.otherPlayers && random(k_OtherPlayersDraw) >= k_OtherPlayersChance; };
	switch (type)
	{
	case Reaction::ReactToCreature:
		return !BlocksInspecting(facts) && facts.inspectDue ? k_InspectCreatureUrgency : 0;
	case Reaction::ReactToFlyingObject:
		if (!facts.humanPlayer || othersPassedOver())
		{
			return 0;
		}
		return PastWait(facts.sawFlying, facts.turn) && !facts.catching && facts.thrownFromHand && facts.inTheAir &&
		               facts.seesThing
		           ? facts.tablePriority
		           : 0;
	case Reaction::ReactToHandPickUp:
		return facts.humanPlayer && facts.ownPlayersHand && facts.handHolds && facts.seesThing
		           ? static_cast<uint8_t>(facts.tablePriority * k_HandPickUpUrgency)
		           : 0;
	default:
		return std::nullopt;
	}
}

std::optional<Setup> creature_reaction_rules::SetupFor(Reaction type, bool thingInTheAir)
{
	using creature_desires::Desire;
	switch (type)
	{
	case Reaction::ReactToCreature:
		return Setup {.plan = Plan {.desire = Desire::Curiosity, .action = "InspectCreature", .onlyIfUseful = true}};
	case Reaction::ReactToFlyingObject:
		// It looks up at a thing still flying; one already down it doesn't take up at all
		if (!thingInTheAir)
		{
			return Setup {.keeps = false, .notesFlying = true};
		}
		return Setup {.notesFlying = true,
		              .comesAcross = true,
		              .plan = Plan {.desire = Desire::Curiosity, .action = "LookAtFlyingObject"}};
	case Reaction::ReactToHandPickUp:
		return Setup {.plan = Plan {.desire = Desire::Curiosity, .action = "LookAtHand", .onSelf = true},
		              .longHandWatch = true};
	default:
		return std::nullopt;
	}
}

float creature_reaction_rules::WorthAbout(float strength, float usefulness, float nearness)
{
	return std::clamp(usefulness * strength * nearness, 0.0f, 1.0f);
}
