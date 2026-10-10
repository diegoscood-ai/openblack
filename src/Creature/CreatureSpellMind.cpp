/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSpellMind.h"

#include <algorithm>

#include "CreatureLearning.h"

using namespace openblack;
using openblack::creature_desires::Desire;

namespace
{
/// The game counts in whole turns a second
uint32_t WholeTurnsPerSecond(float turnsPerSecond)
{
	return turnsPerSecond > 0.0f ? static_cast<uint32_t>(turnsPerSecond) : 0;
}
} // namespace

bool creature_spell_mind::HeldDownByCheat(Desire other, bool all)
{
	return creature_desires::detail::HeldDownByDominant(other, all);
}

creature_spell_mind::Cheat creature_spell_mind::SetCheatDominant(creature_desires::Desires& desires, Desire desire, bool all,
                                                                 float turnsPerSecond, float floor, float seconds)
{
	Cheat cheat;
	creature_desires::detail::SetDominant(cheat, desires, desire, seconds, all, WholeTurnsPerSecond(turnsPerSecond), floor);
	return cheat;
}

bool creature_spell_mind::StepCheat(creature_desires::Desires& desires, Cheat& cheat, float turnsPerSecond)
{
	creature_desires::detail::StepDominant(cheat, desires, WholeTurnsPerSecond(turnsPerSecond));
	return cheat.desire.has_value();
}

void creature_spell_mind::ClearCheatDominance(creature_desires::Desires& desires, Cheat& cheat)
{
	creature_desires::detail::ClearDominant(cheat, desires);
}

uint32_t creature_spell_mind::CheatTurnsLeft(const Cheat& cheat, float turnsPerSecond)
{
	return creature_desires::detail::DominantTurnsLeft(cheat, WholeTurnsPerSecond(turnsPerSecond));
}

void creature_spell_mind::MakeLeastDominant(creature_desires::Desires& desires, Desire desire, float factor)
{
	creature_learning::MakeLeastDominant(desires, desire, factor);
	auto& state = desires[desire];
	state.value = std::min(state.value, state.max);
}

void creature_spell_mind::MakeFullyDominantOverOthers(creature_desires::Desires& desires, Desire desire, float floor)
{
	auto& state = desires[desire];
	state.suppressedTurns = 0;
	if (!state.activated)
	{
		return;
	}
	state.value = state.max;
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		if (i != static_cast<size_t>(desire))
		{
			desires.desires.at(i).value = floor;
		}
	}
}
