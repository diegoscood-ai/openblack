/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LH3DRandom.h"

#include "Common/GameRandom.h"

using namespace openblack::graphics;

// (game_random phase A) forwarders to the one CRT and GRand state of Common/GameRandom; to be removed once no caller
// is left (unify2 game_random_PLAN_A.md, section 4.2)
float lh3d::Random(float a, float b) noexcept
{
	// ?Random@@YAMMM@Z 0x81D180
	return openblack::game_random::crt::Random(a, b);
}

int openblack::grand_local::LocalRand(int count) noexcept
{
	// GRand::LocalRand 0x6DE570
	return static_cast<int>(openblack::game_random::LocalRand(count));
}

float openblack::grand_local::LocalFloatRand(float max) noexcept
{
	// GRand::LocalFloatRand 0x6DE590
	return openblack::game_random::LocalFloatRand(max);
}
