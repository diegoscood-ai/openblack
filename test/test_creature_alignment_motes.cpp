/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A creature's alignment motes and smoke (Creature/CreatureAlignmentMotes): which bodies make them, the order of their
// local random numbers, and where, how large, what colour and what kind each one is. The pinned cases were produced by
// running the original game's own code natively on a body of five bones, from the given local seed: the same seed must
// give the same puffs, bit for bit, and leave the seed where the original left it.

#include <cstddef>
#include <cstdint>

#include <array>
#include <bit>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Creature/CreatureAlignmentMotes.h"
#include "Physics/TurnRules.h"

using namespace openblack;
using namespace openblack::creature_alignment_motes;
using physics::turn::PuffKind;

namespace
{
/// Gives the queued numbers in order and records each limit it was asked for
struct ScriptedRandom
{
	std::vector<uint32_t> values;
	std::vector<int32_t> limits;
	size_t next {0};

	LocalRandom Get()
	{
		return [this](int32_t n) {
			limits.push_back(n);
			return next < values.size() ? values[next++] : 0u;
		};
	}
};

/// Bone i is at (i, 10 i, 100 i)
glm::vec3 BoneAt(uint32_t bone)
{
	const auto i = static_cast<float>(bone);
	return {i, 10.0f * i, 100.0f * i};
}

/// The pinned cases' body: bone b at (10 + 1.5 b, 20 - 0.25 b, -30 + 0.125 b)
glm::vec3 PinnedBoneAt(uint32_t bone)
{
	const auto b = static_cast<float>(bone);
	return {10.0f + (b * 1.5f), 20.0f - (b * 0.25f), -30.0f + (b * 0.125f)};
}
constexpr uint32_t k_PinnedBones = 5;

struct PinnedPuff
{
	std::array<uint32_t, 3> position;
	uint32_t argb;
	uint32_t size;
	int kind; ///< 1 a mote, 2 a reversed mote, 3 smoke
};

struct PinnedCase
{
	uint32_t alignment;
	uint32_t size;
	uint32_t seed;
	uint32_t seedAfter;
	size_t count;
	std::array<PinnedPuff, 3> puffs;
};

constexpr std::array<PinnedCase, 8> k_PinnedCases {{
    {0x3F000000u,
     0x3F800000u,
     0x12345u,
     0x1EC3BE48u,
     3,
     {{{{0x416FEB85u, 0x4195CCCDu, 0xC1E8147Bu}, 0x1090FFFFu, 0x40A00000u, 1},
       {{0x4143851Fu, 0x4198B852u, 0xC1EB6666u}, 0x10FF90FFu, 0x40A00000u, 2},
       {{0x413428F6u, 0x41A3A3D7u, 0xC1E9999Au}, 0x1090FF90u, 0x40A00000u, 2}}}},
    {0x3E4CCCD4u,
     0x3F333333u,
     0x88F89Fu,
     0x182985AFu,
     3,
     {{{{0x41301AA0u, 0x419D3E77u, 0xC1F16F9Eu}, 0x0690FFFFu, 0x40600000u, 2},
       {{0x413F0E56u, 0x41A19CACu, 0xC1F0C395u}, 0x069090FFu, 0x40600000u, 2},
       {{0x4183C7AEu, 0x4194A3D7u, 0xC1EBD4FEu}, 0x0690FFFFu, 0x40600000u, 1}}}},
    {0x3F800000u,
     0x40200000u,
     0x1u,
     0x1D864FCBu,
     3,
     {{{{0x4188B333u, 0x418D8000u, 0xC1DDE666u}, 0x20FFFF90u, 0x41480000u, 1},
       {{0x41556666u, 0x41A83333u, 0xC1EE6666u}, 0x20FF90FFu, 0x41480000u, 1},
       {{0x4135999Au, 0x41A28000u, 0xC1FA6666u}, 0x2090FF90u, 0x41480000u, 1}}}},
    {0xBF000000u,
     0x3F800000u,
     0x12345u,
     0xBF877911u,
     3,
     {{{{0x416FEB85u, 0x4195CCCDu, 0xC1E8147Bu}, 0x24003030u, 0x40000000u, 3},
       {{0x412BC28Fu, 0x41A5C28Fu, 0xC1F547AEu}, 0x24300000u, 0x40000000u, 3},
       {{0x415BC28Fu, 0x41A08F5Cu, 0xC1EFEB85u}, 0x24300030u, 0x40000000u, 3}}}},
    {0xBF800000u,
     0x40400000u,
     0xABCDEFu,
     0x664540D4u,
     3,
     {{{{0x41878F5Cu, 0x418C7AE1u, 0xC1DC28F6u}, 0x48303000u, 0x40C00000u, 3},
       {{0x41346666u, 0x4197851Fu, 0xC1E8E148u}, 0x48303000u, 0x40C00000u, 3},
       {{0x415A8F5Cu, 0x41AFA3D7u, 0xC1FF8F5Cu}, 0x48300000u, 0x40C00000u, 3}}}},
    {0x3E4CCCCDu, 0x3F800000u, 0x5u, 0x00000005u, 0, {{}}},
    {0xBE4CCCCDu, 0x3F800000u, 0x5u, 0x00000005u, 0, {{}}},
    {0x3F3AE148u,
     0x3EA8F5C3u,
     0xDEADu,
     0xFBE57B0Fu,
     3,
     {{{{0x41652618u, 0x419BFAE1u, 0xC1EB0F42u}, 0x1790FF90u, 0x3FD33334u, 2},
       {{0x4180AC57u, 0x4196DA02u, 0xC1ED3A44u}, 0x179090FFu, 0x3FD33334u, 1},
       {{0x417F7213u, 0x4196A752u, 0xC1EA88E9u}, 0x1790FF90u, 0x3FD33334u, 1}}}},
}};

PuffKind KindOf(int kind)
{
	return kind == 1 ? PuffKind::Mote : kind == 2 ? PuffKind::MoteReversed : PuffKind::Smoke;
}
} // namespace

TEST(CreatureAlignmentMotes, ANearlyNeutralBodyMakesNone)
{
	ScriptedRandom random;
	EXPECT_TRUE(PuffsOf(0.2f, 1.0f, 30, BoneAt, random.Get()).empty());
	EXPECT_TRUE(PuffsOf(-0.2f, 1.0f, 30, BoneAt, random.Get()).empty());
	EXPECT_TRUE(PuffsOf(0.0f, 1.0f, 30, BoneAt, random.Get()).empty());
	EXPECT_TRUE(random.limits.empty());
	// A body with no bones makes none either
	EXPECT_TRUE(PuffsOf(1.0f, 1.0f, 0, BoneAt, random.Get()).empty());
	EXPECT_TRUE(random.limits.empty());
}

TEST(CreatureAlignmentMotes, AGoodBodyMakesThreeStillPaleMotesNearRandomBones)
{
	ScriptedRandom random;
	// Per mote: x, y, z offsets, bone, colour, frame order
	random.values = {100, 50, 0, 3, 0, 1, 50, 50, 50, 7, 5, 0, 0, 0, 0, 0, 2, 1};
	const auto puffs = PuffsOf(0.5f, 2.0f, 30, BoneAt, random.Get());
	ASSERT_EQ(puffs.size(), 3u);
	const std::vector<int32_t> limits {101, 101, 101, 30, 6, 2, 101, 101, 101, 30, 6, 2, 101, 101, 101, 30, 6, 2};
	EXPECT_EQ(random.limits, limits);
	// Offsets of (r - 50) x 0.02 x 0.75 x size on each axis
	EXPECT_NEAR(puffs[0].position.x, 3.0f + 1.5f, 1e-5f);
	EXPECT_NEAR(puffs[0].position.y, 30.0f, 1e-5f);
	EXPECT_NEAR(puffs[0].position.z, 300.0f - 1.5f, 1e-4f);
	EXPECT_EQ(puffs[0].velocity, glm::vec3(0.0f));
	EXPECT_FLOAT_EQ(puffs[0].size, 10.0f);
	// Alpha 0.5 x 32 = 16
	EXPECT_EQ(puffs[0].argb, 0x10FF9090u);
	EXPECT_EQ(puffs[0].kind, PuffKind::Mote);
	EXPECT_EQ(puffs[1].position, BoneAt(7));
	EXPECT_EQ(puffs[1].argb, 0x10FF90FFu);
	EXPECT_EQ(puffs[1].kind, PuffKind::MoteReversed);
	EXPECT_EQ(puffs[2].argb, 0x109090FFu);
	EXPECT_EQ(puffs[2].kind, PuffKind::Mote);
}

TEST(CreatureAlignmentMotes, AnEvilBodyMakesThreeDarkRisingPuffs)
{
	ScriptedRandom random;
	// Per puff: x, y, z offsets, bone, colour
	random.values = {50, 50, 50, 1, 0, 50, 50, 50, 2, 3, 50, 50, 50, 4, 5};
	const auto puffs = PuffsOf(-1.0f, 1.5f, 10, BoneAt, random.Get());
	ASSERT_EQ(puffs.size(), 3u);
	const std::vector<int32_t> limits {101, 101, 101, 10, 6, 101, 101, 101, 10, 6, 101, 101, 101, 10, 6};
	EXPECT_EQ(random.limits, limits);
	EXPECT_EQ(puffs[0].position, BoneAt(1));
	EXPECT_EQ(puffs[0].velocity, glm::vec3(0.0f, 2.0f, 0.0f));
	EXPECT_FLOAT_EQ(puffs[0].size, 3.0f);
	// Alpha 1 x 72 = 0x48
	EXPECT_EQ(puffs[0].argb, 0x48300000u);
	EXPECT_EQ(puffs[0].kind, PuffKind::Smoke);
	EXPECT_EQ(puffs[1].argb, 0x48303000u);
	EXPECT_EQ(puffs[2].argb, 0x48300030u);
}

TEST(CreatureAlignmentMotes, TheAlphaDropsItsFraction)
{
	ScriptedRandom random;
	const auto good = PuffsOf(0.99f, 1.0f, 1, BoneAt, random.Get());
	ASSERT_FALSE(good.empty());
	EXPECT_EQ(good[0].argb >> 24u, 31u);
	const auto evil = PuffsOf(-0.21f, 1.0f, 1, BoneAt, random.Get());
	ASSERT_FALSE(evil.empty());
	EXPECT_EQ(evil[0].argb >> 24u, 15u);
}

TEST(CreatureAlignmentMotes, TheSameSeedGivesTheOriginalsPuffs)
{
	for (const auto& c : k_PinnedCases)
	{
		uint32_t seed = c.seed;
		const LocalRandom random = [&seed](int32_t n) {
			return n == 0 ? 0u : game_random::SeededRandom(static_cast<uint32_t>(n), seed);
		};
		const auto alignment = std::bit_cast<float>(c.alignment);
		const auto puffs = PuffsOf(alignment, std::bit_cast<float>(c.size), k_PinnedBones, PinnedBoneAt, random);
		ASSERT_EQ(puffs.size(), c.count) << "alignment " << alignment;
		EXPECT_EQ(seed, c.seedAfter) << "alignment " << alignment;
		for (size_t i = 0; i < puffs.size(); ++i)
		{
			const auto& want = c.puffs.at(i);
			const auto& puff = puffs[i];
			EXPECT_EQ(std::bit_cast<uint32_t>(puff.position.x), want.position[0]) << "alignment " << alignment << " puff " << i;
			EXPECT_EQ(std::bit_cast<uint32_t>(puff.position.y), want.position[1]) << "alignment " << alignment << " puff " << i;
			EXPECT_EQ(std::bit_cast<uint32_t>(puff.position.z), want.position[2]) << "alignment " << alignment << " puff " << i;
			EXPECT_EQ(puff.argb, want.argb) << "alignment " << alignment << " puff " << i;
			EXPECT_EQ(std::bit_cast<uint32_t>(puff.size), want.size) << "alignment " << alignment << " puff " << i;
			EXPECT_EQ(puff.kind, KindOf(want.kind)) << "alignment " << alignment << " puff " << i;
			const auto velocity = puff.kind == PuffKind::Smoke ? glm::vec3(0.0f, 2.0f, 0.0f) : glm::vec3(0.0f);
			EXPECT_EQ(puff.velocity, velocity);
		}
	}
}
