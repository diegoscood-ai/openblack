/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>

// The creature miracles (freeze, small and big, weak and strong, fat and thin, invisible, nice and nasty, itchy and the
// unfinished ones). Cast at a place, one plays as a plain spell. Cast on a creature, the creature takes it on: the
// spell then runs until the creature lets it go, and each turn its effect eases in, holds and wears off
// (creature_spells). Wiki: docs/wiki/creature/body.md#the-size-spells-the-start-delay-and-the-targets.

namespace openblack::magic
{
/// A creature miracle's own part, on its spell: the creature it was cast on, and whether the hand has poured onto it
struct SpellCreatureData
{
	entt::entity creature {entt::null};
	bool handPoured {false};
};
} // namespace openblack::magic

namespace openblack::magic::spell_creature
{
/// The creature takes the spell on, held for the spell's time made longer by the caster's tribal power, after which
/// the spell runs with no time limit until the creature lets it go. Another of its kind already on the creature is
/// cut short, and its spell closed down. Nothing for anything but a creature, or for a magic type that is not one of
/// the creature miracles.
void Receive(entt::entity creature, entt::entity spell);
/// Whether a creature spell cast at a thing takes: only a creature that is there takes it; at anything else the cast
/// fails
[[nodiscard]] bool TakesCastOnObject(entt::entity object);

/// A creature miracle's seed stays in the hand that cast it until the miracle is older than this many seconds
inline constexpr float k_SeedInHandSeconds = 4.0f;
/// Whether the hand lets go of the seed a creature miracle came from: once the miracle is older than
/// k_SeedInHandSeconds, while that seed is still the one in the hand
[[nodiscard]] constexpr bool LetsGoOfSeed(bool hasSeed, float age, bool handHoldsSeed)
{
	return hasSeed && age > k_SeedInHandSeconds && handHoldsSeed;
}

/// The hand's pour onto the creature: four seconds, the hand raised by 0.4 of the creature's height and tipped by one
/// radian, held at the creature and not repeated
inline constexpr float k_HandPourSeconds = 4.0f;
inline constexpr float k_HandPourHeightShare = 0.4f;
inline constexpr float k_HandPourTilt = 1.0f;
struct HandPour
{
	float totalTime {0.0f};
	float heightToRaise {0.0f};
	float angleToRaise {0.0f};
	bool loops {false};
};
/// The pour a creature miracle's turn starts in the caster's hand: once, while the miracle is on a creature and still
/// has its seed, only when the local player's interface cast it and its hand still holds that seed
[[nodiscard]] constexpr std::optional<HandPour> HandPourOnto(bool poured, bool onCreature, bool hasSeed,
                                                             bool myInterfaceCasting, bool handHoldsSeed, float creatureHeight)
{
	if (poured || !onCreature || !hasSeed || !myInterfaceCasting || !handHoldsSeed)
	{
		return std::nullopt;
	}
	return HandPour {.totalTime = k_HandPourSeconds,
	                 .heightToRaise = creatureHeight * k_HandPourHeightShare,
	                 .angleToRaise = k_HandPourTilt,
	                 .loops = false};
}

/// Once a game turn: every creature's spells move on by a turn, their effects applied to it, and the spells that have
/// worn off closed down
void ProcessTurn();

/// Whether the creature's spells put it back as it was when they end (they do unless a script says not). False, and
/// nothing changed, for anything but a creature.
[[nodiscard]] bool SetReversion(entt::entity creature, bool reversion);
} // namespace openblack::magic::spell_creature
