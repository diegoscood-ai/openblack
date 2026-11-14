/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFollowUp.h"

using namespace openblack;
using namespace openblack::creature_follow_up;
using creature_mind::Step;

namespace
{
Step Clip(size_t animation)
{
	return {.kind = Step::Kind::Action, .animation = animation};
}
} // namespace

uint32_t creature_follow_up::StareTurns(float share, uint32_t turnsPerSecond)
{
	// The product is exact in double, as the game's wider arithmetic keeps it before cutting it to whole turns
	return static_cast<uint32_t>((static_cast<double>(share) + 1.0) * static_cast<double>(turnsPerSecond));
}

std::vector<Step> creature_follow_up::ShowSlapFeeling(const Feelings& feelings, const std::function<uint32_t(uint32_t)>& random,
                                                      const std::function<float()>& randomShare, uint32_t turnsPerSecond)
{
	switch (random(4))
	{
	case 0:
		if (feelings.anger <= feelings.compassion)
		{
			return {Clip(k_Frightened)};
		}
		return {Clip(random(2) == 0 ? k_Angry : k_RudeGesture)};
	case 1:
		return {Clip(k_Sad)};
	case 2:
		return {Clip(k_Cry)};
	default:
	{
		const auto turns = StareTurns(randomShare(), turnsPerSecond);
		// The look counts whole seconds of the turns it is given
		return {{.kind = Step::Kind::LookAtCamera,
		         .seconds = static_cast<float>(turns) / static_cast<float>(turnsPerSecond),
		         .face = creature_face::Cue::Sad,
		         .heldEyes = creature_mind::Eyes::Stoned}};
	}
	}
}
