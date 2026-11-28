/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTurnOrder.h"

#include <cstddef>

using namespace openblack;

void creature_turn_order::HoldBodyDesires(creature_desires::Desires& desires,
                                          const std::array<bool, creature_desires::k_DesireCount>& body, uint32_t phase)
{
	if (phase >= k_FirstStageWithBody)
	{
		return;
	}
	for (size_t d = 0; d < body.size(); ++d)
	{
		if (body.at(d))
		{
			desires.desires.at(d).value = 0.0f;
		}
	}
}

bool creature_turn_order::RestartsAfterHold(bool heldSinceStop, bool planHasSteps)
{
	return heldSinceStop && planHasSteps;
}

bool creature_turn_order::SetsOff(bool planHasSteps, creature_desires::Desire desire)
{
	return planHasSteps && desire != creature_desires::Desire::FollowPlayerDesire;
}

bool creature_turn_order::StepStuck(uint32_t& turnsStuck, bool setOffSinceStop, uint32_t turnsPerSecond)
{
	if (!setOffSinceStop)
	{
		++turnsStuck;
	}
	if (turnsStuck <= static_cast<uint32_t>(static_cast<float>(turnsPerSecond) * k_StuckSeconds))
	{
		return false;
	}
	turnsStuck = 0;
	return true;
}
