/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// game_random (Common/GameRandom.h): _LHRand 0x7DB600, GRand 0x6DE510..0x6DE5DA / GData 0x510650 / 0x5106B0, the
// seeds of GGame::Init 0x54F4AF and GData::Reset 0x510750, PSysManager's streams (0x672990..0x672B40, fn_00673340),
// the CRT rand 0x7C8837 and Random 0x81D180. The expected values come from a float32 emulation of those routines
// (unify2/game_random_PLAN_A.md §7.1), bit for bit.

#include <cmath>

#include <bit>
#include <limits>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"

namespace gr = openblack::game_random;
using gr::testing::ScopedState;

namespace
{
uint32_t Bits(float f)
{
	return std::bit_cast<uint32_t>(f);
}
} // namespace

TEST(GameRandom, LHRandFromInitialSeed)
{
	uint32_t seed = gr::k_InitialSeed;
	const std::vector<uint32_t> values = {45003, 48433, 38176, 51531, 21466};
	const std::vector<uint32_t> seeds = {0xE6F4C8D6, 0x5BAD6184, 0x47184E08, 0xBF390A12, 0x118A4250};
	for (size_t i = 0; i < values.size(); ++i)
	{
		EXPECT_EQ(gr::LHRand(0xFFFF, seed), values[i]) << i;
		EXPECT_EQ(seed, seeds[i]) << i;
	}
}

TEST(GameRandom, LHRandFromZero)
{
	uint32_t seed = 0;
	for (const uint32_t expected : {9977u, 23493u, 36908u, 57607u, 5622u})
	{
		EXPECT_EQ(gr::LHRand(0xFFFF, seed), expected);
	}
}

TEST(GameRandom, GameRandAfterInit)
{
	ScopedState state;
	gr::Reset();
	gr::Init();
	EXPECT_EQ(gr::Current().synced, 0x88F89Fu);
	EXPECT_EQ(gr::Current().local, 0x88F89Fu);
	for (const uint32_t expected : {78u, 48u, 76u, 86u, 16u})
	{
		EXPECT_EQ(gr::GameRand(100), expected);
	}
	// the local seed is another stream
	EXPECT_EQ(gr::Current().local, 0x88F89Fu);
}

TEST(GameRandom, GameRandAfterReset)
{
	ScopedState state;
	gr::Init();
	gr::Reset();
	EXPECT_EQ(gr::Current().synced, 0u);
	EXPECT_EQ(gr::Current().local, 0u);
	for (const uint32_t expected : {37u, 58u, 58u, 52u, 37u})
	{
		EXPECT_EQ(gr::GameRand(100), expected);
	}
}

TEST(GameRandom, GameFloatRand)
{
	ScopedState state;
	EXPECT_EQ(gr::GameFloatRand(2.0f), 1.3734035f);
	EXPECT_EQ(gr::GameFloatRand(2.0f), 1.4780804f);
	EXPECT_EQ(gr::GameFloatRand(2.0f), 1.1650568f);
}

TEST(GameRandom, LatticeFromZero)
{
	// fn_00590DF0's 256 x (1 - GameFloatRand(2)) from the seed before Init (0, inferido)
	uint32_t seed = 0;
	EXPECT_EQ(gr::FloatRand(2.0f, seed), 0.30447853f);
	EXPECT_EQ(gr::FloatRand(2.0f, seed), 0.71696043f);
	EXPECT_EQ(gr::FloatRand(2.0f, seed), 1.1263599f);
}

TEST(GameRandom, MultiplicationOrder)
{
	const float twoPi = std::bit_cast<float>(0x40C90FDBu);
	EXPECT_EQ(twoPi, glm::two_pi<float>());
	{
		// GData::FloatRand 0x5106B0: (u x x) x k
		ScopedState state;
		EXPECT_EQ(Bits(gr::GameFloatRand(twoPi)), 0x408A11D0u); // 4.314674377441406f
	}
	{
		// the PSys synced float 0x672AB0: (u x k) x x
		ScopedState state;
		const gr::psys::StepScope scope(gr::psys::NetGameType::Synced);
		EXPECT_EQ(Bits(gr::psys::FloatRand(twoPi)), 0x408A11D1u); // 4.3146748542785645f
	}
}

TEST(GameRandom, PSysRandIsNotFloorOfFloatRand)
{
	{
		ScopedState state;
		const gr::psys::StepScope scope(gr::psys::NetGameType::Synced);
		EXPECT_EQ(gr::psys::Rand(10), 8); // floor(FloatRand(10)) would be 6
	}
	{
		ScopedState state;
		const gr::psys::StepScope scope(gr::psys::NetGameType::Synced);
		EXPECT_EQ(gr::psys::Rand(256), 214); // floor(FloatRand(256)) would be 175
	}
}

TEST(GameRandom, ZeroForZero)
{
	ScopedState state;
	const auto before = gr::Current();
	EXPECT_EQ(gr::GameRand(0), 0u);
	EXPECT_EQ(gr::GameFloatRand(0.0f), 0.0f);
	EXPECT_EQ(gr::GameFloatRand(-0.0f), 0.0f);
	EXPECT_EQ(Bits(gr::GameFloatRand(-0.0f)), 0u); // fld [0x8AA398], +0
	EXPECT_EQ(gr::GameFloatRand(std::numeric_limits<float>::quiet_NaN()), 0.0f);
	EXPECT_EQ(gr::LocalRand(0), 0u);
	EXPECT_EQ(gr::LocalFloatRand(0.0f), 0.0f);
	EXPECT_EQ(gr::LocalFloatRand(std::numeric_limits<float>::quiet_NaN()), 0.0f);
	EXPECT_EQ(gr::Current().synced, before.synced);
	EXPECT_EQ(gr::Current().local, before.local);
}

TEST(GameRandom, NegativeRange)
{
	ScopedState state;
	EXPECT_EQ(gr::GameFloatRand(-2.0f), -1.3734035f);
}

TEST(GameRandom, LocalStream)
{
	ScopedState state;
	EXPECT_EQ(gr::LocalFloatRand(2.0f), 1.3734035f);
	EXPECT_EQ(gr::Current().synced, gr::k_InitialSeed);
	EXPECT_EQ(gr::Current().local, 0xE6F4C8D6u);
}

TEST(GameRandom, GameFloatRange)
{
	ScopedState state;
	// 0x5E1CE0: GameFloatRand(b - a) + a
	EXPECT_EQ(gr::GameFloatRange(3.0f, 5.0f), 1.3734035f + 3.0f);
}

TEST(GameRandom, PSysOutsideAStep)
{
	ScopedState state;
	ASSERT_EQ(gr::psys::Active(), gr::psys::Stream::None);
	const auto before = gr::Current();
	const auto outside = gr::testing::OutsideStepDraws();
	EXPECT_EQ(gr::psys::FloatRand(5.0f), 0.0f);
	EXPECT_EQ(gr::psys::Rand(5), 0);
	EXPECT_EQ(gr::psys::RandR3(), glm::vec3(-1.0f));
	EXPECT_EQ(gr::testing::OutsideStepDraws(), outside + 3);
	EXPECT_EQ(gr::Current().synced, before.synced);
	EXPECT_EQ(gr::Current().local, before.local);
}

TEST(GameRandom, PSysSyncedZeroStillDraws)
{
	ScopedState state;
	const gr::psys::StepScope scope(gr::psys::NetGameType::Synced);
	EXPECT_EQ(gr::psys::FloatRand(0.0f), 0.0f);
	EXPECT_EQ(gr::Current().synced, 0xE6F4C8D6u);
	EXPECT_EQ(gr::Current().local, gr::k_InitialSeed);
}

TEST(GameRandom, PSysLocalStream)
{
	ScopedState state;
	const gr::psys::StepScope scope(gr::psys::NetGameType::Local);
	EXPECT_EQ(gr::psys::Rand(10), 8);
	EXPECT_EQ(gr::Current().synced, gr::k_InitialSeed);
	EXPECT_EQ(gr::Current().local, 0xE6F4C8D6u);
}

TEST(GameRandom, PSysRandR3)
{
	ScopedState state;
	const gr::psys::StepScope scope(gr::psys::NetGameType::Synced);
	// the first point from 0x88F89F is already inside the ball: 3 draws
	const glm::vec3 p = gr::psys::RandR3();
	EXPECT_EQ(p.x, 0.37340355f);
	EXPECT_EQ(p.y, 0.4780804f);
	EXPECT_EQ(p.z, 0.16505682f);
	EXPECT_EQ(gr::Current().synced, 0x47184E08u);
}

TEST(GameRandom, PSysScopeDoesNotNest)
{
	ScopedState state;
	{
		const gr::psys::StepScope outer(gr::psys::NetGameType::Synced);
		EXPECT_EQ(gr::psys::Active(), gr::psys::Stream::Synced);
		{
			const gr::psys::StepScope inner(gr::psys::NetGameType::Local);
			EXPECT_EQ(gr::psys::Active(), gr::psys::Stream::Local);
		}
		// 0x67349B: the "0" pointers, not the outer step's
		EXPECT_EQ(gr::psys::Active(), gr::psys::Stream::None);
	}
	EXPECT_EQ(gr::psys::Active(), gr::psys::Stream::None);
}

TEST(GameRandom, PSysFloatRange)
{
	ScopedState state;
	const gr::psys::StepScope scope(gr::psys::NetGameType::Synced);
	// 0x6729C0: FloatRand(b - a) + a; u = 45003: (45003 k) x 2 + 3
	float expected = static_cast<float>(45003u);
	expected = expected * gr::FloatRandScale();
	expected = expected * 2.0f;
	expected = expected + 3.0f;
	EXPECT_EQ(gr::psys::FloatRand(3.0f, 5.0f), expected);
}

TEST(GameRandom, Crt)
{
	ScopedState state;
	EXPECT_EQ(Bits(gr::CrtRandomScale()), 0x38000100u);
	EXPECT_EQ(Bits(gr::FloatRandScale()), 0x37800080u);
	for (const int32_t expected : {41, 18467, 6334, 26500, 19169})
	{
		EXPECT_EQ(gr::crt::Rand(), expected);
	}
	gr::crt::Srand(1);
	EXPECT_EQ(gr::crt::Random(0.0f, 16.0f), 0.020020142f);
	gr::crt::Srand(1);
	EXPECT_EQ(gr::crt::Rand(), 41);
}

TEST(GameRandom, SaveLoad)
{
	ScopedState state;
	for (int i = 0; i < 3; ++i)
	{
		static_cast<void>(gr::GameRand(1000));
		static_cast<void>(gr::LocalRand(1000));
	}
	const auto saved = gr::Save();
	std::vector<uint32_t> first;
	for (int i = 0; i < 2; ++i)
	{
		first.push_back(gr::GameRand(1000));
		first.push_back(gr::LocalRand(1000));
	}
	gr::Load(saved);
	for (int i = 0; i < 2; ++i)
	{
		EXPECT_EQ(gr::GameRand(1000), first[static_cast<size_t>(2 * i)]);
		EXPECT_EQ(gr::LocalRand(1000), first[static_cast<size_t>(2 * i + 1)]);
	}
}

TEST(GameRandom, TestHooks)
{
	ScopedState state;
	std::vector<uint32_t> asked;
	gr::testing::SetGameRand(
	    [&asked](uint32_t n) {
		    asked.push_back(n);
		    return 7u;
	    },
	    [](float x) { return x * 0.5f; });
	// before the 0-for-0 test, as VillagerCore's hook was
	EXPECT_EQ(gr::GameRand(0), 7u);
	EXPECT_EQ(gr::GameRand(10), 7u);
	EXPECT_EQ(gr::GameFloatRand(4.0f), 2.0f);
	{
		// the PSys synced draws go through GameRand (0x672AC2)
		const gr::psys::StepScope scope(gr::psys::NetGameType::Synced);
		EXPECT_EQ(gr::psys::Rand(5), 7);
	}
	EXPECT_EQ(asked, (std::vector<uint32_t> {0, 10, 5}));
	EXPECT_EQ(gr::Current().synced, gr::k_InitialSeed);
}

TEST(GameRandom, ScopedStateRestores)
{
	gr::Load({123, 456});
	gr::crt::Srand(99);
	{
		ScopedState state({0, 0}, 1);
		EXPECT_EQ(gr::Current().synced, 0u);
		static_cast<void>(gr::GameRand(5));
		static_cast<void>(gr::crt::Rand());
	}
	EXPECT_EQ(gr::Current().synced, 123u);
	EXPECT_EQ(gr::Current().local, 456u);
	{
		ScopedState state({0, 0}, 99);
		const int32_t expected = gr::crt::Rand();
		gr::crt::Srand(99);
		EXPECT_EQ(gr::crt::Rand(), expected);
	}
}
