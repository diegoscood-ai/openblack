/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "ECS/LivingFirstClip.h"

using namespace openblack;
using namespace openblack::ecs::living;

namespace
{
/// The game's generator on a seed of its own, counting the draws
struct SeededFake
{
	uint32_t seed;
	std::vector<uint32_t> limits;

	IntRandom Random()
	{
		return [this](uint32_t limit) {
			limits.push_back(limit);
			return game_random::SeededRandom(limit, seed);
		};
	}
};

/// A bird's first clip from a seed, and the seed after it, as the original's clip functions gave them
struct BirdCase
{
	AnimalInfo type;
	uint32_t seed;
	int32_t clip;
	uint32_t seedAfter;
};

constexpr std::array k_BirdCases {
    BirdCase {AnimalInfo::Dove, 0x0088F89Fu, 8, 0xE6F4C8D6u},     BirdCase {AnimalInfo::Dove, 0x12345678u, 9, 0x32BE789Bu},
    BirdCase {AnimalInfo::Dove, 0x0BADF00Du, 9, 0x8866712Fu},     BirdCase {AnimalInfo::Crow, 0x0088F89Fu, 3, 0xE6F4C8D6u},
    BirdCase {AnimalInfo::Crow, 0x12345678u, 4, 0x32BE789Bu},     BirdCase {AnimalInfo::Pigeon, 0xDEADBEEFu, 13, 0xA973D0F6u},
    BirdCase {AnimalInfo::Pigeon, 0x0BADF00Du, 14, 0x8866712Fu},  BirdCase {AnimalInfo::Seagull, 0x00000001u, 18, 0x4C000002u},
    BirdCase {AnimalInfo::Seagull, 0x12345678u, 20, 0x32BE789Bu}, BirdCase {AnimalInfo::Swallow, 0x0088F89Fu, 27, 0xE6F4C8D6u},
    BirdCase {AnimalInfo::Swallow, 0xCAFEBABEu, 26, 0x72EBE3B2u}, BirdCase {AnimalInfo::Swallow, 0x12345678u, 25, 0x32BE789Bu},
};
} // namespace

TEST(LivingFirstClip, ABirdsFirstClipIsItsMoveClipDrawnAsTheOriginalDraws)
{
	for (const auto& c : k_BirdCases)
	{
		SeededFake fake {.seed = c.seed, .limits = {}};
		const auto first = AnimalFirstClip(c.type, 42, fake.Random());
		ASSERT_TRUE(first.has_value());
		EXPECT_EQ(first->clip, c.clip) << static_cast<int>(c.type) << " seed " << c.seed;
		EXPECT_FALSE(first->fromStart);
		EXPECT_EQ(fake.seed, c.seedAfter);
		ASSERT_EQ(fake.limits.size(), 1u);
		EXPECT_EQ(fake.limits.front(), c.type == AnimalInfo::Swallow ? 3u : 2u);
	}
}

TEST(LivingFirstClip, TheSpellDoveAndTheBatsHaveOneClipAndDrawNothing)
{
	for (const auto& [type, clip] : std::array {std::pair {AnimalInfo::SpellDove, 23}, std::pair {AnimalInfo::Bat, 1},
	                                            std::pair {AnimalInfo::SpellBat, 1}})
	{
		SeededFake fake {.seed = 0x0088F89Fu, .limits = {}};
		const auto first = AnimalFirstClip(type, 42, fake.Random());
		ASSERT_TRUE(first.has_value());
		EXPECT_EQ(first->clip, clip);
		EXPECT_FALSE(first->fromStart);
		EXPECT_TRUE(fake.limits.empty());
	}
}

TEST(LivingFirstClip, TheVultureStartsWithNoClip)
{
	SeededFake fake {.seed = 0x0088F89Fu, .limits = {}};
	EXPECT_EQ(BirdMoveClip(AnimalInfo::Vulture, fake.Random()), -1);
	EXPECT_FALSE(AnimalFirstClip(AnimalInfo::Vulture, 42, fake.Random()).has_value());
	EXPECT_TRUE(fake.limits.empty());
}

TEST(LivingFirstClip, AnyOtherAnimalStandsFromTheStartWithNoDraw)
{
	for (const auto type : {AnimalInfo::Cow, AnimalInfo::Sheep, AnimalInfo::Horse, AnimalInfo::Lion, AnimalInfo::Tortoise})
	{
		SeededFake fake {.seed = 0x0088F89Fu, .limits = {}};
		const auto first = AnimalFirstClip(type, 42, fake.Random());
		ASSERT_TRUE(first.has_value());
		EXPECT_EQ(first->clip, 42);
		EXPECT_TRUE(first->fromStart);
		EXPECT_TRUE(fake.limits.empty());
		EXPECT_EQ(BirdMoveClip(type, fake.Random()), -1);
		EXPECT_TRUE(fake.limits.empty());
	}
	SeededFake fake {.seed = 0x0088F89Fu, .limits = {}};
	EXPECT_FALSE(AnimalFirstClip(AnimalInfo::Cow, -1, fake.Random()).has_value());
}

TEST(LivingFirstClip, AVillagerStartsWithItsBornStatesTableClipFromTheStart)
{
	// standing as just created, swimming as made on water
	const auto created = VillagerFirstClip(385);
	ASSERT_TRUE(created.has_value());
	EXPECT_EQ(created->clip, 385);
	EXPECT_TRUE(created->fromStart);
	const auto drowning = VillagerFirstClip(252);
	ASSERT_TRUE(drowning.has_value());
	EXPECT_EQ(drowning->clip, 252);
	EXPECT_FALSE(VillagerFirstClip(-4).has_value());
}

TEST(LivingFirstClip, TheBirdsAreTheNineWithTheirOwnClipRules)
{
	for (const auto type : {AnimalInfo::Dove, AnimalInfo::Crow, AnimalInfo::Pigeon, AnimalInfo::Seagull, AnimalInfo::Swallow,
	                        AnimalInfo::SpellDove, AnimalInfo::Bat, AnimalInfo::SpellBat, AnimalInfo::Vulture})
	{
		EXPECT_TRUE(IsBirdForClips(type)) << static_cast<int>(type);
	}
	for (const auto type : {AnimalInfo::Cow, AnimalInfo::Wolf, AnimalInfo::Tortoise, AnimalInfo::PuzzleSheep})
	{
		EXPECT_FALSE(IsBirdForClips(type)) << static_cast<int>(type);
	}
}
