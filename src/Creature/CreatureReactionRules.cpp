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

uint8_t creature_reaction_rules::ReactToFoodPriority(bool hasPlayer, bool computerPlayer, bool sees, uint32_t foodPriority)
{
	return hasPlayer && !computerPlayer && sees ? static_cast<uint8_t>(foodPriority) : 0;
}

uint8_t creature_reaction_rules::ReactToFightPriority(bool ownFight, bool fighting, bool sees, bool fighterIsCreature,
                                                      bool fightersOpponent, uint32_t fightPriority)
{
	if (ownFight || fighting || !sees || !fighterIsCreature || fightersOpponent)
	{
		return 0;
	}
	return static_cast<uint8_t>(fightPriority);
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
	    state.opponentPlansFight || state.fighting || state.fainted || state.knockedOut || state.inEmergency ||
	    state.controlledByTeleport)
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

std::optional<entt::entity> creature_reaction_rules::LeashCastTarget(const map_coords::MapCoords& hand, entt::entity self,
                                                                     const ThingsInCell& inCell)
{
	auto coords = hand;
	map_coords::Spiral spiral;
	for (int32_t cells = k_LeashCastCells; cells != 0; --cells)
	{
		if (map_coords::InBounds(coords))
		{
			const auto things = inCell(map_coords::Cell(coords));
			if (const auto found = std::ranges::find_if(things, [self](entt::entity thing) { return thing != self; });
			    found != things.end())
			{
				return *found;
			}
		}
		map_coords::AddCells(coords, spiral.Next());
	}
	return std::nullopt;
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
	       facts.hasPartner || facts.planActsOnThing;
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
	case Reaction::ReactToHandUsingTotem:
		// A creature with a player the computer does not play, that sees the totem
		return !facts.noPlayerOrComputers && facts.seesThing ? facts.tablePriority : 0;
	case Reaction::ReactToHandPuttingStuffInStoragePit:
		// A creature with a player the computer does not play, wherever it looks
		return !facts.noPlayerOrComputers ? facts.tablePriority : 0;
	case Reaction::ReactToObjectCrushed:
		// Another player's thing crushed is mostly passed over, by a draw made only then; a turn of 0 is as never
		if (facts.noPlayerOrComputers || (facts.othersReaction && random(k_OtherPlayersDraw) >= k_OtherPlayersChance) ||
		    (facts.sawCrushed.has_value() && *facts.sawCrushed != 0 && !PastWait(facts.sawCrushed, facts.turn)) || facts.angry)
		{
			return 0;
		}
		return !facts.thingIsSelf && facts.seesThing ? facts.tablePriority : 0;
	case Reaction::ReactToDeath:
		return facts.thingIsCreature ? facts.tablePriority : 0;
	case Reaction::Fainting:
		return facts.tablePriority;
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
	case Reaction::ReactToHandUsingTotem:
		// It comes across the totem, drops what it was doing to look at it, and watches the totem used
		return Setup {.comesAcross = true,
		              .plan = Plan {.desire = Desire::Curiosity, .action = "ExamineByLooking"},
		              .watchesSkill = CreatureActionKnownAbout::UseTotem};
	case Reaction::ReactToHandPuttingStuffInStoragePit:
		// It comes across the storage pit and watches it used, carrying on with what it was doing
		return Setup {.comesAcross = true, .watchesSkill = CreatureActionKnownAbout::UseStoragePit};
	case Reaction::ReactToObjectCrushed:
		// It notes the turn, comes across the thing and drops what it was doing to look at it
		return Setup {.notesCrushed = true,
		              .comesAcross = true,
		              .plan = Plan {.desire = Desire::Curiosity, .action = "ExamineByLooking"}};
	case Reaction::ReactToDeath:
		return Setup {.desireSource = std::pair(creature_desires::sources::k_Sadness, k_SadnessFromDeath)};
	case Reaction::Fainting:
		// It sleeps (the table's row 17), whoever fainted, and follows the reaction about itself
		return Setup {.aboutItself = true, .plan = Plan {.desire = Desire::Tiredness, .action = "SleepAtHome", .onSelf = true}};
	default:
		return std::nullopt;
	}
}

float creature_reaction_rules::WorthAbout(float strength, float usefulness, float nearness)
{
	return std::clamp(usefulness * strength * nearness, 0.0f, 1.0f);
}

creature_reaction_rules::WatchVerdict creature_reaction_rules::WeighWatch(
    std::optional<creature_desires::Desire> mine, std::optional<creature_desires::Desire> theirs,
    const std::function<std::pair<float, float>()>& angerAndFearWorth, const std::function<bool()>& dominant)
{
	using creature_desires::Desire;
	if (!mine.has_value())
	{
		return {};
	}
	if (*mine == Desire::Anger)
	{
		return {.act = Desire::Anger};
	}
	if (theirs == Desire::Anger)
	{
		const auto [anger, fear] = angerAndFearWorth();
		return {.act = fear < anger ? Desire::Anger : Desire::Fear};
	}
	if (*mine != Desire::Fear)
	{
		return {.act = mine};
	}
	if (theirs == Desire::Fear && dominant())
	{
		return {.weighAgainWithoutFear = true};
	}
	return {.act = Desire::Fear};
}

bool creature_reaction_rules::IsDominant(uint32_t age, uint32_t otherAge, bool laterInTheList)
{
	constexpr uint32_t k_DominantYears = 10;
	const auto gap = age > otherAge ? age - otherAge : otherAge - age;
	return gap > k_DominantYears ? otherAge < age : laterInTheList;
}
