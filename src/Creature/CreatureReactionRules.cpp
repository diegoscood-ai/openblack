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

#include "ECS/Components/PlayerMagic.h"

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
	if (!state.available || !state.reactionsOn || state.fighting || state.fainted || state.knockedOut)
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
