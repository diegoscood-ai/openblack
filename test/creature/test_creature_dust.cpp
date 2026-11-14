/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The dust a creature's steps throw up (Creature/CreatureDust): how many puffs a sound makes, and each puff's speed and
// size from the local draws, in the game's order

#include <cstdint>

#include <array>
#include <bit>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Creature/CreatureDust.h"

using namespace openblack;
using audio::SoundAction;

TEST(CreatureDust, TheHeavierTheStepTheMorePuffs)
{
	EXPECT_EQ(creature_dust::PuffsFor(SoundAction::FootstepLight), 3);
	EXPECT_EQ(creature_dust::PuffsFor(SoundAction::FootstepNormal), 6);
	EXPECT_EQ(creature_dust::PuffsFor(SoundAction::FootStamp), 12);
	EXPECT_FALSE(creature_dust::PuffsFor(SoundAction::BreatheIn).has_value());
	EXPECT_FALSE(creature_dust::PuffsFor(SoundAction::Scream).has_value());
}

TEST(CreatureDust, APuffsSpeedTakesTheDrawsDepthFirstAndItsSizeIsFourTimesTheCreatures)
{
	// three draws of 201, each taken about 0 and times a twentieth of the drawn size: the first the depth's, the
	// second the height's, the third the width's
	std::vector<uint32_t> draws {200, 100, 0};
	std::vector<int32_t> asked;
	size_t next = 0;
	const auto puff = creature_dust::Make(2.0f, [&](int32_t n) {
		asked.push_back(n);
		return draws.at(next++);
	});
	EXPECT_EQ(asked, (std::vector<int32_t> {201, 201, 201}));
	EXPECT_FLOAT_EQ(puff.velocity.z, 10.0f);
	EXPECT_FLOAT_EQ(puff.velocity.y, 0.0f);
	EXPECT_FLOAT_EQ(puff.velocity.x, -10.0f);
	EXPECT_FLOAT_EQ(puff.size, 8.0f);
}

TEST(CreatureDust, ThreePuffsAreTheOriginals)
{
	// Measured on the original game: a creature of drawn size 2 at (10, 20, 30), its local seed 0x12345, three puffs
	uint32_t seed = 0x12345u;
	const auto localRand = [&seed](int32_t n) { return game_random::SeededRandom(static_cast<uint32_t>(n), seed); };
	constexpr std::array<std::array<uint32_t, 3>, 3> k_Velocities {{
	    {0xC0C66667u, 0xC0D66667u, 0x410CCCCDu},
	    {0xC104CCCDu, 0x3FC00000u, 0x40600000u},
	    {0xC0F9999Au, 0x404CCCCDu, 0x40B33333u},
	}};
	for (const auto& expected : k_Velocities)
	{
		const auto puff = creature_dust::Make(2.0f, localRand);
		EXPECT_EQ(std::bit_cast<uint32_t>(puff.velocity.x), expected[0]);
		EXPECT_EQ(std::bit_cast<uint32_t>(puff.velocity.y), expected[1]);
		EXPECT_EQ(std::bit_cast<uint32_t>(puff.velocity.z), expected[2]);
		EXPECT_EQ(std::bit_cast<uint32_t>(puff.size), 0x41000000u);
	}
	EXPECT_EQ(seed, 0x86A730C9u);
}
