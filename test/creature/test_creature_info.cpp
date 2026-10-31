/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature records of the game's info.dat, read into InfoConstants as the game reads them

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "InfoConstants.h"

using namespace openblack;

namespace
{
/// Where the game's info.dat keeps its table, past the file's header
constexpr size_t k_InfoDatHeader = 0x2C;

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

/// A species' row as the game's data holds it, as float bits (the seconds per age tick a whole number): how fast it goes
/// slowly, walking and running; its comfortable temperature and its body's rates; and what its energy pays for a
/// miracle with
struct Species
{
	std::array<uint32_t, 3> speeds;
	std::array<uint32_t, 13> physiology;
	std::array<uint32_t, 2> casting;
};
constexpr std::array k_Species {
    Species {/* GiantApe */ {0x3F000000u, 0x3F333333u, 0x3F666666u},
             {0x41700000u, 3600u, 0x43700000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
    Species {/* Cow */ {0x3ECCCCCDu, 0x3F19999Au, 0x3F4CCCCDu},
             {0x41300000u, 3600u, 0x43480000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
    Species {/* Tiger */ {0x3F000000u, 0x3F4CCCCDu, 0x3F733333u},
             {0x41700000u, 3600u, 0x43820000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
    Species {/* Leopard */ {0x3F000000u, 0x3F4CCCCDu, 0x3F733333u},
             {0x41700000u, 3600u, 0x43340000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
    Species {/* Wolf */ {0x3F000000u, 0x3F4CCCCDu, 0x3F733333u},
             {0x00000000u, 3600u, 0x43700000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
    Species {/* Lion */ {0x3F000000u, 0x3F4CCCCDu, 0x3F733333u},
             {0x41700000u, 3600u, 0x43200000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
    Species {/* Horse */ {0x3F000000u, 0x3F4CCCCDu, 0x3F7D70A4u},
             {0x41300000u, 3600u, 0x43700000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
    Species {/* Tortoise */ {0x3F000000u, 0x3F19999Au, 0x3F4CCCCDu},
             {0x41300000u, 3600u, 0x43700000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
    Species {/* Zebra */ {0x3F19999Au, 0x3F4CCCCDu, 0x3F7D70A4u},
             {0x41700000u, 3600u, 0x43700000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
    Species {/* BrownBear */ {0x3F000000u, 0x3F333333u, 0x3F666666u},
             {0x41300000u, 3600u, 0x43700000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
    Species {/* PolarBear */ {0x3F000000u, 0x3F333333u, 0x3F6B851Fu},
             {0xC0A00000u, 3600u, 0x43700000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
    Species {/* Sheep */ {0x3F000000u, 0x3F333333u, 0x3F6B851Fu},
             {0xC0A00000u, 3600u, 0x43700000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
    Species {/* Chimp */ {0x3F000000u, 0x3F333333u, 0x3F6B851Fu},
             {0xC0A00000u, 3600u, 0x435C0000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
    Species {/* Ogre */ {0x3F000000u, 0x3F333333u, 0x3F6B851Fu},
             {0xC0A00000u, 3600u, 0x43700000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
    Species {/* Mandrill */ {0x3F000000u, 0x3F333333u, 0x3F6B851Fu},
             {0xC0A00000u, 3600u, 0x43700000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
    Species {/* Rhino */ {0x3F000000u, 0x3F333333u, 0x3F6B851Fu},
             {0xC0A00000u, 3600u, 0x43700000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
    Species {/* Gorilla */ {0x3F000000u, 0x3F333333u, 0x3F6B851Fu},
             {0xC0A00000u, 3600u, 0x43700000u, 0x3E4CCCCDu, 0x3827C5ACu, 0x459C4000u, 0x3F7FFEB0u, 0x40C00000u, 0x38F3B066u,
              0x38A7C5ACu, 0x3D75C28Fu, 0x3A917D6Bu, 0x3B23D70Au},
             {0x470CA000u, 0x3BA3D70Au}},
};

std::array<uint32_t, 3> Speeds(const GCreatureInfo& info)
{
	return {std::bit_cast<uint32_t>(info.slowSpeed), std::bit_cast<uint32_t>(info.walkSpeed),
	        std::bit_cast<uint32_t>(info.runSpeed)};
}

std::array<uint32_t, 13> Physiology(const GCreatureInfo& info)
{
	return {std::bit_cast<uint32_t>(info.comfortTemperature), info.secondsPerAgeTick,
	        std::bit_cast<uint32_t>(info.growUpMinutes),      std::bit_cast<uint32_t>(info.lowEnergyThreshold),
	        std::bit_cast<uint32_t>(info.exhaustionRate),     std::bit_cast<uint32_t>(info.secondsToDehydrate),
	        std::bit_cast<uint32_t>(info.strengthDecay),      std::bit_cast<uint32_t>(info.carryStrengthMinutes),
	        std::bit_cast<uint32_t>(info.energyDrain),        std::bit_cast<uint32_t>(info.fatBurn),
	        std::bit_cast<uint32_t>(info.overeatFatFactor),   std::bit_cast<uint32_t>(info.sleepHeal),
	        std::bit_cast<uint32_t>(info.sleepRecover)};
}

std::array<uint32_t, 2> Casting(const GCreatureInfo& info)
{
	return {std::bit_cast<uint32_t>(info.chantsPerEnergy), std::bit_cast<uint32_t>(info.spellEnergyFloor)};
}
} // namespace

// Each species' town totem icon is read from where the game's file keeps it
TEST(CreatureInfo, realInfoDatTotemIcons)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	// Each record knows its own row, the Giant Ape's first
	for (uint32_t row = 0; row < info->creature.size(); ++row)
	{
		EXPECT_EQ(info->creature.at(row).row, row);
	}
	EXPECT_EQ(info->creature.at(0).totemIcon, MeshId::BuildingPlayerIconApe);
	EXPECT_EQ(info->creature.at(1).totemIcon, MeshId::BuildingPlayerIconCow);
	EXPECT_EQ(info->creature.at(2).totemIcon, MeshId::BuildingPlayerIconTiger);
	EXPECT_EQ(info->creature.at(11).totemIcon, MeshId::BuildingPlayerIconSheep);
	// The Mandrill has no icon of its own and wears the ape's
	EXPECT_EQ(info->creature.at(14).totemIcon, MeshId::BuildingPlayerIconApe);
}

// Each species' speeds, body rates, comfortable temperature and the energy rates its miracles are paid with, read from
// the game's info.dat bit for bit, as the original (docs/wiki/formats/info-dat.md#the-creature-tables-in-infodat)
TEST(CreatureInfo, realInfoDatSpeciesSpeedsBodyRatesAndCasting)
{
	const auto data = RealInfoDat();
	if (!data.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	ASSERT_EQ(data->size(), k_InfoDatHeader + sizeof(InfoConstants));
	const auto info = ReadInfoDat(*data);
	ASSERT_EQ(info->creature.size(), k_Species.size());
	for (size_t row = 0; row < k_Species.size(); ++row)
	{
		const auto& species = info->creature.at(row);
		EXPECT_EQ(Speeds(species), k_Species.at(row).speeds) << row;
		EXPECT_EQ(Physiology(species), k_Species.at(row).physiology) << row;
		EXPECT_EQ(Casting(species), k_Species.at(row).casting) << row;
	}
}

// The synthetic twin: a file of the game's size, with a species' values written where the game's info.dat keeps them,
// reads into that species' fields and no other's, as the original
// (docs/wiki/formats/info-dat.md#the-creature-tables-in-infodat)
TEST(CreatureInfo, ASpeciesRowsSpeedsBodyRatesAndCastingAreReadFromWhereTheFileKeepsThem)
{
	constexpr size_t k_Rows = 0xBEC0;
	constexpr size_t k_RowSize = 0x384;
	constexpr std::array<size_t, 3> k_SpeedsAt {0x240, 0x244, 0x248};
	constexpr std::array<size_t, 13> k_PhysiologyAt {0x1FC, 0x200, 0x204, 0x20C, 0x210, 0x214, 0x218,
	                                                 0x21C, 0x220, 0x224, 0x228, 0x22C, 0x230};
	constexpr std::array<size_t, 2> k_CastingAt {0x284, 0x288};
	constexpr size_t k_Row = 2;
	const auto& expected = k_Species.at(k_Row);
	std::vector<char> data(k_InfoDatHeader + sizeof(InfoConstants), 0);
	const auto put = [&data](size_t at, uint32_t bits) {
		std::memcpy(data.data() + k_InfoDatHeader + k_Rows + k_Row * k_RowSize + at, &bits, sizeof(bits));
	};
	for (size_t i = 0; i < k_SpeedsAt.size(); ++i)
	{
		put(k_SpeedsAt.at(i), expected.speeds.at(i));
	}
	for (size_t i = 0; i < k_PhysiologyAt.size(); ++i)
	{
		put(k_PhysiologyAt.at(i), expected.physiology.at(i));
	}
	for (size_t i = 0; i < k_CastingAt.size(); ++i)
	{
		put(k_CastingAt.at(i), expected.casting.at(i));
	}
	const auto info = ReadInfoDat(data);
	const auto& species = info->creature.at(k_Row);
	EXPECT_EQ(Speeds(species), expected.speeds);
	EXPECT_EQ(Physiology(species), expected.physiology);
	EXPECT_EQ(Casting(species), expected.casting);
	// Its neighbours read nothing of it
	EXPECT_EQ(Speeds(info->creature.at(k_Row - 1)), (std::array<uint32_t, 3> {}));
	EXPECT_EQ(Speeds(info->creature.at(k_Row + 1)), (std::array<uint32_t, 3> {}));
}
