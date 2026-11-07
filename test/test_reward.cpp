/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <numbers>
#include <optional>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/RewardRules.h"
#include "InfoConstants.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// Where the game's info.dat keeps its table, past the file's header
constexpr size_t k_InfoDatHeader = 0x2C;
/// Where the reward table's first row starts in the game's info.dat, and the size of a row
constexpr size_t k_RewardRowsAt = 0x88C20;
constexpr size_t k_RewardRowSize = 0x120;

/// The game's info.dat, or nothing without OPENBLACK_GAME_PATH or the file
std::optional<std::vector<char>> RealInfoDat()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		return std::nullopt;
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	if (!file.is_open())
	{
		return std::nullopt;
	}
	return std::vector<char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

/// The table read from a file's bytes as the game reads it
std::unique_ptr<InfoConstants> ReadInfoDat(const std::vector<char>& data)
{
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + k_InfoDatHeader, sizeof(InfoConstants));
	return info;
}

/// What a kind of reward names, as the game's info.dat holds it: the seed, the gesture, the power-up, the help text,
/// the belief (as float bits), the object and the scaffold
struct RewardRow
{
	int32_t seed;
	uint32_t gesture;
	uint32_t powerUp;
	uint32_t helpText;
	uint32_t belief;
	int32_t object;
	uint32_t scaffold;

	bool operator==(const RewardRow&) const = default;
};

RewardRow RowOf(const GRewardInfo& row)
{
	return {.seed = static_cast<int32_t>(row.seed),
	        .gesture = static_cast<uint32_t>(row.gestureType),
	        .powerUp = static_cast<uint32_t>(row.powerUp),
	        .helpText = static_cast<uint32_t>(row.helpTextEnum),
	        .belief = std::bit_cast<uint32_t>(row.beliefValue),
	        .object = static_cast<int32_t>(row.mobileStatic),
	        .scaffold = row.scaffold};
}

/// Every row of the game's reward table, the kind "none" first, read from the frozen Scripts\info.dat
constexpr std::array<RewardRow, 61> k_RewardRows {{
    {-1, 0, 0, 0u, 0x00000000u, -1, 0u},     // 0 Reward None
    {7, 0, 0, 2607u, 0x00000000u, -1, 0u},   // 1 Reward PU1
    {7, 0, 0, 2608u, 0x00000000u, -1, 0u},   // 2 Reward PU2
    {-1, 0, 0, 2597u, 0x00000000u, -1, 0u},  // 3 Reward Small Food
    {-1, 0, 0, 2597u, 0x00000000u, -1, 0u},  // 4 Reward Medium Food
    {-1, 0, 0, 2597u, 0x00000000u, -1, 0u},  // 5 Reward Large Food
    {-1, 0, 0, 2598u, 0x00000000u, -1, 0u},  // 6 Reward Small Wood
    {-1, 0, 0, 2598u, 0x00000000u, -1, 0u},  // 7 Reward Medium Wood
    {-1, 0, 0, 2598u, 0x00000000u, -1, 0u},  // 8 Reward Large Wood
    {2, 0, 0, 2609u, 0x00000000u, -1, 0u},   // 9 Reward FireBall Spell
    {-1, 0, 2, 2610u, 0x00000000u, -1, 0u},  // 10 Reward Gesture Fireball PU1
    {-1, 0, 2, 2611u, 0x00000000u, -1, 0u},  // 11 Reward Gesture Fireball PU1
    {6, 0, 0, 2612u, 0x00000000u, -1, 0u},   // 12 Reward Bolt Spell
    {-1, 0, 5, 2613u, 0x00000000u, -1, 0u},  // 13 Reward Gesture Bolt PU1
    {-1, 0, 6, 2614u, 0x00000000u, -1, 0u},  // 14 Reward Gesture Bot PU1
    {29, 0, 0, 2615u, 0x00000000u, -1, 0u},  // 15 Reward Explosion Spell
    {-1, 0, 8, 2616u, 0x00000000u, -1, 0u},  // 16 Reward Gesture Explosion PU1
    {7, 0, 0, 2607u, 0x00000000u, -1, 0u},   // 17 Reward Heal Spell
    {-1, 0, 11, 2608u, 0x00000000u, -1, 0u}, // 18 Reward Gesture Heal PU1
    {28, 0, 0, 2604u, 0x00000000u, -1, 0u},  // 19 Reward Teleport Spell
    {3, 0, 0, 2629u, 0x00000000u, -1, 0u},   // 20 Reward Food Spell
    {0, 0, 16, 2600u, 0x00000000u, -1, 0u},  // 21 Reward Storm Spell
    {-1, 0, 17, 2601u, 0x00000000u, -1, 0u}, // 22 Reward Gesture Storm PU1
    {-1, 0, 18, 2602u, 0x00000000u, -1, 0u}, // 23 Reward Gesture Strom PU2
    {-1, 0, 0, 0u, 0x00000000u, -1, 0u},     // 24 Reward Gesture Strom PU3
    {4, 0, 0, 2605u, 0x00000000u, -1, 0u},   // 25 Reward Shield Spell
    {-1, 0, 0, 0u, 0x00000000u, -1, 0u},     // 26 Reward Gesture Shield PU1
    {5, 0, 0, 2606u, 0x00000000u, -1, 0u},   // 27 Reward Physical Shield Spell
    {8, 0, 0, 2630u, 0x00000000u, -1, 0u},   // 28 Reward Wood Spell
    {-1, 0, 0, 0u, 0x00000000u, -1, 0u},     // 29 Reward Skeleton Spell
    {-1, 0, 0, 0u, 0x00000000u, -1, 0u},     // 30 Reward Gesture Skeleton PU1
    {12, 0, 0, 2620u, 0x00000000u, -1, 0u},  // 31 Reward Creature Freeze Spell
    {13, 0, 0, 2622u, 0x00000000u, -1, 0u},  // 32 Reward Creature Small Spell
    {14, 0, 0, 2623u, 0x00000000u, -1, 0u},  // 33 Reward Creature Big Spell
    {15, 0, 0, 2628u, 0x00000000u, -1, 0u},  // 34 Reward Creature Weak Spell
    {16, 0, 0, 2627u, 0x00000000u, -1, 0u},  // 35 Reward Creature Strong Spell
    {17, 0, 0, 2631u, 0x00000000u, -1, 0u},  // 36 Reward Creature Fat Spell
    {18, 0, 0, 2632u, 0x00000000u, -1, 0u},  // 37 Reward Creature Thin Spell
    {19, 0, 0, 2621u, 0x00000000u, -1, 0u},  // 38 Reward Creature Invisible Spell
    {20, 0, 0, 2625u, 0x00000000u, -1, 0u},  // 39 Reward Creature Compassion Spell
    {21, 0, 0, 2626u, 0x00000000u, -1, 0u},  // 40 Reward Creature Angry Spell
    {22, 0, 0, 2633u, 0x00000000u, -1, 0u},  // 41 Reward Creature Hungry Spell
    {23, 0, 0, 2634u, 0x00000000u, -1, 0u},  // 42 Reward Creature Frightene Spell
    {24, 0, 0, 2635u, 0x00000000u, -1, 0u},  // 43 Reward Creature Tired Spell
    {25, 0, 0, 2636u, 0x00000000u, -1, 0u},  // 44 Reward Creature Ill Spell
    {26, 0, 0, 2637u, 0x00000000u, -1, 0u},  // 45 Reward Creature Thirsty Spell
    {27, 0, 0, 2638u, 0x00000000u, -1, 0u},  // 46 Reward Creature Itchy Spell
    {-1, 0, 0, 2639u, 0x3E4CCCCDu, -1, 0u},  // 47 Reward Small Belief
    {-1, 0, 0, 2640u, 0x3F000000u, -1, 0u},  // 48 Reward Medium Belief
    {-1, 0, 0, 2641u, 0x3F4CCCCDu, -1, 0u},  // 49 Reward Large Belief
    {-1, 0, 0, 0u, 0x00000000u, -1, 0u},     // 50 Reward Increment Type
    {27, 0, 0, 2642u, 0x00000000u, 5, 0u},   // 51 Reward Singing Stone
    {-1, 0, 0, 2643u, 0x00000000u, 11, 0u},  // 52 Reward Idol
    {-1, 0, 0, 2644u, 0x00000000u, 15, 0u},  // 53 Reward Weeping Stone
    {-1, 0, 0, 2645u, 0x00000000u, -1, 1u},  // 54 Reward Scaffold
    {9, 0, 0, 2646u, 0x00000000u, -1, 0u},   // 55 Reward Magic Water
    {10, 0, 0, 2647u, 0x00000000u, -1, 0u},  // 56 Reward Magic Flock Flying
    {11, 0, 0, 2648u, 0x00000000u, -1, 0u},  // 57 Reward Magic Flock Ground
    {-1, 0, 0, 2649u, 0x00000000u, 53, 0u},  // 58 Reward Toy - Ball
    {-1, 0, 0, 2650u, 0x00000000u, 54, 0u},  // 59 Reward Toy - Cuddly
    {-1, 0, 0, 2651u, 0x00000000u, 55, 0u},  // 60 Reward Toy - Die
}};
} // namespace

// The chest weighs 789, whatever its size and whatever its row of the reward table says, as the original
// (docs/wiki/scripts/map-script-objects.md#reward-chests-create_reward-239-create_reward_in_town-240)
TEST(Reward, ItWeighsSevenHundredAndEightyNineWhateverItsRow)
{
	EXPECT_EQ(reward::k_Weight, 789.0f);
}

// The game's reward table, read from its info.dat as the original reads it: 61 rows, the kind "none" and the 60 kinds a
// script may give, each naming its seed, gesture, power-up, help text, belief, object and scaffold, and the food and
// wood rows their amounts (docs/wiki/scripts/map-script-objects.md#the-reward-table)
TEST(RewardTable, realInfoDatSixtyKindsAndWhatEachNames)
{
	const auto data = RealInfoDat();
	if (!data.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	ASSERT_EQ(data->size(), k_InfoDatHeader + sizeof(InfoConstants));
	const auto info = ReadInfoDat(*data);
	ASSERT_EQ(info->reward.size(), k_RewardRows.size());
	for (size_t row = 0; row < k_RewardRows.size(); ++row)
	{
		const auto& reward = info->reward.at(row);
		EXPECT_EQ(std::string_view(reward.debugString.data(), 7), "Reward ") << row;
		EXPECT_EQ(RowOf(reward), k_RewardRows.at(row)) << row;
	}
	EXPECT_EQ(std::string_view(info->reward.at(0).debugString.data()), "Reward None");
	EXPECT_EQ(std::string_view(info->reward.back().debugString.data()), "Reward Toy - Die");
	// the food and wood kinds, small, medium and large
	constexpr std::array k_Amounts {100u, 250u, 500u};
	for (size_t size = 0; size < k_Amounts.size(); ++size)
	{
		const auto& food = info->reward.at(static_cast<size_t>(RewardObjectInfo::SmallFood) + size);
		const auto& wood = info->reward.at(static_cast<size_t>(RewardObjectInfo::SmallWood) + size);
		EXPECT_EQ(food.foodValue, static_cast<float>(k_Amounts.at(size))) << size;
		EXPECT_EQ(food.woodValue, 0u) << size;
		EXPECT_EQ(wood.woodValue, k_Amounts.at(size)) << size;
		EXPECT_EQ(wood.foodValue, 0.0f) << size;
	}
}

// The synthetic twin: a file of the game's size with one kind's row written where the game's info.dat keeps it reads
// into that kind's fields and no other's (docs/wiki/scripts/map-script-objects.md#the-reward-table)
TEST(RewardTable, AKindsRowIsReadFromWhereTheFileKeepsIt)
{
	// the singing stone's row, its seed, help text and object, with marks of no meaning in the fields it leaves empty so
	// that every field is told apart
	constexpr size_t k_Row = static_cast<size_t>(RewardObjectInfo::SingingStone);
	constexpr std::array<size_t, 7> k_FieldsAt {0x104, 0x108, 0x10C, 0x110, 0x114, 0x118, 0x11C};
	const auto& expected = k_RewardRows.at(k_Row);
	const std::array<uint32_t, 7> values {static_cast<uint32_t>(expected.seed),   3u, 9u, expected.helpText, 0x3F000000u,
	                                      static_cast<uint32_t>(expected.object), 1u};
	std::vector<char> data(k_InfoDatHeader + sizeof(InfoConstants), 0);
	for (size_t i = 0; i < k_FieldsAt.size(); ++i)
	{
		const auto bits = values.at(i);
		std::memcpy(data.data() + k_RewardRowsAt + k_Row * k_RewardRowSize + k_FieldsAt.at(i), &bits, sizeof(bits));
	}
	const auto info = ReadInfoDat(data);
	EXPECT_EQ(RowOf(info->reward.at(k_Row)),
	          (RewardRow {expected.seed, 3u, 9u, expected.helpText, 0x3F000000u, expected.object, 1u}));
	// its neighbours read nothing of it
	EXPECT_EQ(RowOf(info->reward.at(k_Row - 1)), (RewardRow {}));
	EXPECT_EQ(RowOf(info->reward.at(k_Row + 1)), (RewardRow {}));
}

TEST(Reward, AChestFromTheSkyFallsAHundredAndFiftyMetresInThreeSecondsTurningHalfARoundASecond)
{
	EXPECT_FLOAT_EQ(reward::FallHeight(0.0f), 150.0f);
	EXPECT_FLOAT_EQ(reward::FallHeight(1.0f), 100.0f);
	EXPECT_FLOAT_EQ(reward::FallHeight(3.0f), 0.0f);
	EXPECT_FLOAT_EQ(reward::FallYaw(1.0f), std::numbers::pi_v<float>);
}

TEST(Reward, ItsDustSpreadsAboutItInSandColoursAndGrowsAsItFades)
{
	const auto dust = reward::MakeDust([](float a, float b) { return b + 0.0f * a; });
	EXPECT_EQ(dust.size(), 27u);
	EXPECT_FLOAT_EQ(dust[0].offset.x, 2.0f);
	EXPECT_FLOAT_EQ(dust[0].offset.y, 2.0f);
	EXPECT_EQ(dust[0].frame, 15u);
	EXPECT_EQ(dust[0].rgb, (std::array<uint8_t, 3> {212, 180, 140}));
	const auto fresh = reward::LookOfDust(2500.0f);
	EXPECT_EQ(fresh.alpha, 255);
	EXPECT_FLOAT_EQ(fresh.size, 0.7f);
	const auto gone = reward::LookOfDust(0.0f);
	EXPECT_EQ(gone.alpha, 0);
	EXPECT_FLOAT_EQ(gone.size, 2.5f);
}

TEST(Reward, ItsDustIsPlacedAcrossThenUpThenAlong)
{
	// Each draw is told apart by its order
	float next = 0.0f;
	const auto dust = reward::MakeDust([&next](float /*from*/, float /*to*/) { return next += 1.0f; });
	EXPECT_FLOAT_EQ(dust[0].offset.z, 1.0f);
	EXPECT_FLOAT_EQ(dust[0].offset.y, 2.0f);
	EXPECT_FLOAT_EQ(dust[0].offset.x, 3.0f);
}

TEST(Reward, ItsDustShowsOnTheFrameItLandsThenOnceItIsInTheMap)
{
	// The frame it lands, then not until the next turn puts it in the map's cells
	EXPECT_TRUE(reward::DrawsDust(true, 2500.0f, false));
	EXPECT_FALSE(reward::DrawsDust(false, 2500.0f, false));
	EXPECT_TRUE(reward::DrawsDust(false, 2400.0f, true));
	// Not once its time is up, nor for a chest that was put on the land
	EXPECT_FALSE(reward::DrawsDust(false, 0.0f, true));
	EXPECT_FALSE(reward::DrawsDust(true, 0.0f, false));
}

TEST(Reward, ItsDustsTimeGoesDownByTheFramesWholeMilliseconds)
{
	EXPECT_FLOAT_EQ(reward::DustAfterFrame(2500.0f, 16.6f), 2484.0f);
	EXPECT_FLOAT_EQ(reward::DustAfterFrame(10.0f, 16.0f), 0.0f);
}
