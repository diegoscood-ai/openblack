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

#include <optional>

#include "Enums.h"

// How a creature takes the reactions a miracle makes: how much each one matters to it, how long it lasts and how long
// before it takes the same kind again, and, as it starts, whether it runs from the miracle or goes to look at it,
// whether it learns the miracle, and whether it takes the reaction up at all. Pure, tested on its own; the creatures'
// reaction handler (ECS/Systems/Implementations/CreatureReactions) gathers the facts.

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
	bool fighting {false};
	/// Out cold: fainted, or knocked out when its life ran out
	bool fainted {false};
	bool knockedOut {false};
	bool mimicking {false};
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
/// A nice miracle. `othersPlayer`: the reaction has a player that is neither the creature's nor an ally's -> nothing.
/// No miracle: wood -> nothing; else it goes to look, learns and takes it up. A miracle cast by a spell icon with no
/// seed -> nothing. Else it goes to look and learns (and the seed is marked) unless the creator is neither a creature
/// nor nobody and the player is a computer player, or the seed was learnt from; it takes the reaction up either way
[[nodiscard]] Outcome NiceMagic(bool othersPlayer, MagicType magic, const std::optional<SpellFacts>& spell);

/// The miracle learnt by seeing it, none for k_NotLearntBySight
[[nodiscard]] std::optional<MagicType> LearntBySight(MagicType magic);

/// Frightened by a nasty miracle it runs, unless it is on the learning leash or not afraid at all: then it goes to look
[[nodiscard]] bool CuriousAboutNastyMagic(bool onRope, float fear);

} // namespace openblack::creature_reaction_rules
