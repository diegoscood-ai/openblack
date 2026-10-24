/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature records of the game's info.dat, read into InfoConstants as the game reads them

#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "InfoConstants.h"

using namespace openblack;

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
