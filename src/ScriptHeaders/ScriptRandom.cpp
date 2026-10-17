/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptRandom.h"

#include "Common/GameRandom.h"

namespace openblack::script::random
{

uint32_t WholeNumberBetween(uint32_t low, uint32_t high, GameRandomInterface& random, std::source_location where)
{
	const uint32_t width = high - low + 1u;
	return random.GameRand(width, where) + low;
}

} // namespace openblack::script::random
