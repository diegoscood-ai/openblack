/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHandSight.h"

#include <algorithm>

float openblack::creature_hand_sight::IncreaseSecondsAfterSeen(float seconds, float divisor, float least, float most)
{
	// the division at a float's precision, then the bounds, least first, as the game clamps it
	const float divided = seconds / divisor;
	return std::max(least, std::min(divided, most));
}
