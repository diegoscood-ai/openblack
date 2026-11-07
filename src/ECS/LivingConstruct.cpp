/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LivingConstruct.h"

#include "Common/GameRandom.h"
#include "InfoConstants.h"

using namespace openblack;

uint32_t ecs::living::ConstructorAgeDraw(const GLivingInfo& info)
{
	const uint32_t half = info.startAge >> 1;
	// a draw below 0 is none: no number is taken
	const uint32_t drawn = half != 0 ? game_random::GameRand(half) : 0;
	return drawn + info.startAge - (info.startAge >> 2);
}
