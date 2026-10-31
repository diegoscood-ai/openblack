/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <optional>
#include <string_view>

#include "Creature/CreatureDesires.h"
#include "ECS/Components/PlayerAlliances.h"
#include "Enums.h"

// How a creature takes the reactions a miracle makes: how much each one matters to it, how long it lasts and how long
// before it takes the same kind again, and, as it starts, whether it runs from the miracle or goes to look at it,
// whether it learns the miracle, and whether it takes the reaction up at all. Also how urgent another creature, a thing
// flying through the air, the hand picking something up, the hand using a totem and the hand putting things in a storage
// pit are to it, and what it does on taking those up. Pure, tested on its own; the creatures' reaction handler
// (ECS/Systems/Implementations/CreatureReactions) gathers the facts.

namespace openblack::creature_reaction_rules
{

/// A miracle that strikes nearer than this, in map units, matters more the nearer it is
inline constexpr int32_t k_FleeFromSpellRange = 600000;
/// A creature already mimicking its player only takes up a reaction whose priority is above this
inline constexpr uint32_t k_MimicPriority = 150;
/// The one miracle no creature learns by seeing it
inline constexpr MagicType k_NotLearntBySight = MagicType::LightningBoltPowerUpTwo;

/// Running from a nasty miracle: none for a miracle the creature cast itself; else the table's priority, plus up to 100
/// the nearer it is within the range (by the fast distance in map units), the sum cut to a byte
[[nodiscard]] uint8_t FleeFromSpellPriority(bool castByItself, int32_t fastDistance, uint32_t priority);
/// Looking at a nice or an impressive miracle: none for a miracle the creature cast itself, else the table's priority
/// of the nice miracle's reaction (the impressive one reads the same row), cut to a byte
[[nodiscard]] uint8_t LookAtNiceSpellPriority(bool castByItself, uint32_t niceSpellPriority);

/// A creature's turns of a reaction: ((max - d) x importance / max + 1 - importance) x the table's turns for a
/// creature, every step in float precision, truncated (negative beyond the reaction's distance)
[[nodiscard]] int32_t TurnsToReact(float maxDistance, float importance, float distance, uint32_t turns);
/// And its turns before taking the same kind again: (importance x d / max + 1 - importance) x the table's turns
[[nodiscard]] int32_t TurnsBeforeReactingAgain(float maxDistance, float importance, float distance, uint32_t turns);

/// Whether a creature takes up reactions now, from what is known of its state. A creature mimicking its player only
/// takes those above k_MimicPriority
struct Availability
{
	bool available {true};
	/// Reactions are on (a script can turn them off)
	bool reactionsOn {true};
	/// A script controls it, it carries out an order its player gave it with the leash, its plan is to fight, or its
	/// opponent's is
	bool scriptControlled {false};
	bool orderedByPlayer {false};
	bool plansFight {false};
	bool opponentPlansFight {false};
	bool fighting {false};
	/// Out cold: fainted, or knocked out when its life ran out
	bool fainted {false};
	bool knockedOut {false};
	bool mimicking {false};
	/// In an emergency of its own (badly hurt, on fire, a fire in its town), until it stops what it does
	bool inEmergency {false};
};
[[nodiscard]] bool IsAvailableForReaction(const Availability& state, uint32_t typePriority);

/// What is known of what started the reaction, when it is a miracle
struct SpellFacts
{
	/// It has a creator, and the creator is a worship site's spell icon, or a creature
	bool hasCreator {false};
	bool creatorIsSpellIcon {false};
	bool creatorIsCreature {false};
	/// The seed it was cast from, and whether a creature already learnt from that seed
	bool hasSeed {false};
	bool seedLearnedFrom {false};
	/// Its player's type (1 a human at this computer, 2 a computer player), none when the miracle has no player
	std::optional<int32_t> playerType;
};

/// What a creature does as it starts reacting to a miracle
struct Outcome
{
	/// It goes to look at the miracle (a nice one); a nasty one always moves it, running away or going to look
	bool examine {false};
	/// It learns the miracle by seeing it (k_NotLearntBySight aside), and the seed is marked as learnt from
	bool learn {false};
	bool markSeed {false};
	/// It takes the reaction up, as the one it follows until it ends
	bool takeUp {false};
};

/// A nasty miracle (it runs from it or goes to look first, whatever follows): no miracle (another thing started it) ->
/// it learns and takes it up. A miracle cast by a spell icon with no seed -> nothing more. Else it learns unless the
/// miracle's creator is neither a creature nor nobody and the miracle's player is a computer player or of type 3, or
/// the seed was learnt from; then takes it up
[[nodiscard]] Outcome NastyMagic(const std::optional<SpellFacts>& spell);
/// Whether a reaction is another player's to a creature: it has a player (`hasPlayer`), not the creature's, and not one
/// allied to the creature's player (`reactionPlayers`: the alliances of the reaction's player)
[[nodiscard]] bool IsOthersPlayer(bool hasPlayer, PlayerNames reactionPlayer, PlayerNames creaturePlayer,
                                  const ecs::components::PlayerAlliances& reactionPlayers);
/// A nice miracle. `othersPlayer`: the reaction has a player that is neither the creature's nor an ally's -> nothing.
/// No miracle: wood -> nothing; else it goes to look, learns and takes it up. A miracle cast by a spell icon with no
/// seed -> nothing. Else it goes to look and learns (and the seed is marked) unless the creator is neither a creature
/// nor nobody and the player is a computer player, or the seed was learnt from; it takes the reaction up either way
[[nodiscard]] Outcome NiceMagic(bool othersPlayer, MagicType magic, const std::optional<SpellFacts>& spell);

/// The miracle learnt by seeing it, none for k_NotLearntBySight
[[nodiscard]] std::optional<MagicType> LearntBySight(MagicType magic);

/// Frightened by a nasty miracle it runs, unless it is on the learning leash or not afraid at all: then it goes to look
[[nodiscard]] bool CuriousAboutNastyMagic(bool onRope, float fear);

/// Turns a creature waits after seeing a flying thing before another can catch its eye: it heeds another only once more
/// than this many have gone by
inline constexpr uint32_t k_SightingTurns = 899;
/// A creature heeds a flying thing of another player's only when a draw below this comes under its chance
inline constexpr uint32_t k_OtherPlayersDraw = 100;
/// Of that draw, these lowest values let it heed the thing
inline constexpr uint32_t k_OtherPlayersChance = 3;
/// The hand picking something up is this many times as urgent to a creature as the table says, kept to a byte
inline constexpr uint32_t k_HandPickUpUrgency = 3;

/// What a creature knows as it weighs another creature, a flying thing or the hand picking something up
struct Facts
{
	/// The reaction table's urgency for the kind
	uint8_t tablePriority {0};
	/// The creature has a player, and a person plays it rather than the computer
	bool humanPlayer {false};
	/// The creature has no player, or the computer plays its player
	bool noPlayerOrComputers {false};
	/// The reaction belongs to a player, not the creature's own nor one allied to it (IsOthersPlayer)
	bool otherPlayers {false};
	/// It sees the reaction's thing
	bool seesThing {false};
	/// The reaction's thing is the creature itself, is gone, or is a creature
	bool thingIsSelf {false};
	bool thingGone {false};
	bool thingIsCreature {false};
	/// The game turn now, and when it last took up a flying thing
	uint32_t turn {0};
	std::optional<uint32_t> sawFlying;
	/// It is catching a thing
	bool catching {false};
	/// The flying thing was let go by a hand, and is still really in the air
	bool thrownFromHand {false};
	bool inTheAir {false};
	/// The hand picking something up is its own player's and holds something
	bool ownPlayersHand {false};
	bool handHolds {false};
	/// It carries out an order its player gave it with the leash, it has an opponent in a fight, or its plan's action is
	/// done to the reaction's thing
	bool orderedByPlayer {false};
	bool hasOpponent {false};
	bool planActsOnThing {false};
	/// The creature the reaction is about is due another look over: it never has looked it over, or not for long enough
	bool inspectDue {false};
};

/// Another creature is so urgent to a creature, once due another look over
inline constexpr uint8_t k_InspectCreatureUrgency = 0xff;

/// Whether a creature passes over another creature's reaction before even thinking of looking it over: the reaction is
/// its own, or isn't a creature's, or it is busy with its player's order, a fight, or the creature already
[[nodiscard]] bool BlocksInspecting(const Facts& facts);
/// Whether a creature last looked over at a turn (none for never) is due another look over now: only once more than
/// the wait has gone by
[[nodiscard]] bool InspectDue(std::optional<uint32_t> inspected, uint32_t turn, uint32_t waitTurns);
/// The wait in turns from the creature's wait in seconds, at whole turns a second: cut down to a whole turn
[[nodiscard]] uint32_t InspectWaitTurns(float seconds, uint32_t turnsPerSecond);

/// How urgent another creature, a flying thing, the hand picking something up, the hand using a totem or the hand putting
/// things in a storage pit is to a creature before its distance counts; none for the other kinds. A draw below a range is
/// asked for only where the rule draws one.
[[nodiscard]] std::optional<uint8_t> Priority(Reaction type, const Facts& facts,
                                              const std::function<uint32_t(uint32_t)>& random);

/// A plan a reaction makes the creature drop what it was doing for: an action of the game's table, done for a desire, to
/// the reaction's thing or to the creature itself
struct Plan
{
	creature_desires::Desire desire {creature_desires::Desire::Curiosity};
	std::string_view action;
	bool onSelf {false};
	/// Only when its plan isn't already done to the thing, and the thing is of some use to act on for the desire
	bool onlyIfUseful {false};
};

/// What the creature does on taking up a reaction, in order: it comes across the thing, drops what it was doing for a
/// plan, and keeps the reaction or not
struct Setup
{
	/// It keeps the reaction for its time; when it doesn't, it carries on with what it reacted to before, if anything
	bool keeps {true};
	/// It notes the turn, for the flying thing's wait
	bool notesFlying {false};
	/// It comes across the thing
	bool comesAcross {false};
	std::optional<Plan> plan;
	/// It watches the hand for a minute and up to another (as turns), rather than its plan's own while, which the watch
	/// caps at five seconds
	bool longHandWatch {false};
	/// It watches the skill done, as when it sees a villager practise it (the skills by their row in the game's table)
	std::optional<CreatureActionKnownAbout> watchesSkill;
};

/// Watching the hand pick something up, the creature's watch is set to this many seconds and a draw of up to as many
/// more
inline constexpr float k_HandPickUpWatchSeconds = 60.0f;

/// What a creature does on taking up another creature's reaction, a flying thing's, the hand's picking up, the hand's
/// using a totem or the hand's putting things in a storage pit; none for the other kinds. A flying thing makes it look
/// only while the thing is still in the air. The totem makes it look at the totem and watch the totem's use; the storage
/// pit makes it watch the storage pit's use.
[[nodiscard]] std::optional<Setup> SetupFor(Reaction type, bool thingInTheAir);

/// The value of a desire about a different creature. This is the strength of the desire, multiplied by the learnt
/// usefulness and the nearness. The value is between zero and one.
[[nodiscard]] float WorthAbout(float strength, float usefulness, float nearness);

} // namespace openblack::creature_reaction_rules
