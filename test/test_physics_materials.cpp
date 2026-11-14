/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The physics materials of PhysicsConstants.txt (Physics/Materials), read from made-up bytes over a made-up table

#include <cstdint>

#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Physics/Materials.h"

using namespace openblack;
using openblack::physics::MaterialTable;
using openblack::physics::ParseMaterials;

namespace
{
std::vector<uint8_t> Bytes(std::string_view text)
{
	return {text.begin(), text.end()};
}

MaterialTable Base()
{
	MaterialTable table {};
	for (size_t i = 0; i < table.size(); ++i)
	{
		const auto f = static_cast<float>(i);
		table.at(i) = {0.1f + f * 0.01f, f, f * 0.1f, 0.5f, 0.5f, 0.25f};
	}
	return table;
}
} // namespace

TEST(PhysicsMaterials, WithoutTheFileEveryMaterialIsZero)
{
	for (const auto& row : physics::k_NoMaterials)
	{
		EXPECT_EQ(row.density, 0.0f);
		EXPECT_EQ(row.contact, 0.0f);
		EXPECT_EQ(row.penetration, 0.0f);
		EXPECT_EQ(row.friction, 0.0f);
		EXPECT_EQ(row.angularKeep, 0.0f);
		EXPECT_EQ(row.drag, 0.0f);
	}
}

TEST(PhysicsMaterials, TheFileGivesItsVersionAndRows)
{
	const auto file = ParseMaterials(Bytes("3 2\n1 2 3 1 0.5 1\n0.5 10 1 2 0.9 2\n"), Base());
	EXPECT_EQ(file.version, 3);
	EXPECT_EQ(file.rows, 2);
	EXPECT_EQ(file.table.at(0).density, 1.0f);
	EXPECT_EQ(file.table.at(0).contact, 2.0f);
	EXPECT_EQ(file.table.at(1).friction, 2.0f);
	EXPECT_EQ(file.table.at(1).angularKeep, 0.9f);
}

TEST(PhysicsMaterials, EachColumnIsClampedToItsRange)
{
	const auto file = ParseMaterials(Bytes("1 1\n0.01 300 11 4 0.1 5\n"), Base());
	const auto& row = file.table.at(0);
	EXPECT_EQ(row.density, 0.05f);
	EXPECT_EQ(row.contact, 240.0f);
	EXPECT_EQ(row.penetration, 10.0f);
	EXPECT_EQ(row.friction, 3.0f);
	EXPECT_EQ(row.angularKeep, 0.2f);
	EXPECT_EQ(row.drag, 4.0f);
}

TEST(PhysicsMaterials, TheRowsTheFileLacksAreCopiesOfRowZero)
{
	const auto file = ParseMaterials(Bytes("1 1\n1 2 3 1 0.5 1\n"), Base());
	for (size_t i = 1; i < file.table.size(); ++i)
	{
		EXPECT_EQ(file.table.at(i).density, 1.0f) << i;
		EXPECT_EQ(file.table.at(i).drag, 1.0f) << i;
	}
}

TEST(PhysicsMaterials, WithoutRowsEveryRowIsTheBaseRowZero)
{
	const auto base = Base();
	const auto file = ParseMaterials(Bytes(""), base);
	EXPECT_EQ(file.rows, 0);
	for (const auto& row : file.table)
	{
		EXPECT_EQ(row.contact, base.at(0).contact);
		EXPECT_EQ(row.density, base.at(0).density);
	}
}
