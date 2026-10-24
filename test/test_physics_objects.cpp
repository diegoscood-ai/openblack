/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The pure rules for rocks and for where the physics puts what it lets go of (Physics/ObjectRules), on plain values
// and a fake land. The rock case is the upstream physics objects test's; its other cases wait for the rules they test.
// Which things are rocks (ECS/ObjectPhysics), on a registry and an info table of the test's own.

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Mobile.h"
#include "ECS/Components/Tree.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Physics/ObjectRules.h"
#include "support/LandFakes.h"

using namespace openblack::physics::objects;

TEST(PhysicsObjects, OnlyTallRocksBreakWhenTapped)
{
	EXPECT_FALSE(RockBreaksWhenTapped(0.7f));
	EXPECT_TRUE(RockBreaksWhenTapped(0.71f));
}

TEST(PhysicsObjects, AKnockWearsATallRockHarderThanFourGUnlessFromAThingThatIsARock)
{
	EXPECT_TRUE(KnockWearsRock(4.01f, 0.71f, false));
	EXPECT_FALSE(KnockWearsRock(4.0f, 0.71f, false));
	EXPECT_FALSE(KnockWearsRock(4.01f, 0.7f, false));
	// from a rock, the bonfire or a fragment it wears nothing; from a dead tree or anything else it does
	EXPECT_FALSE(KnockWearsRock(100.0f, 5.0f, true));
	EXPECT_TRUE(KnockWearsRock(100.0f, 5.0f, false));
}

TEST(PhysicsObjects, WhatThePhysicsLetsGoOfStandsOnTheMapsGrid)
{
	// x and z through the map's 16.16 coordinates: x 6553.6, truncated towards 0, back to metres (x 10 / 65536)
	const auto at = PlacedOnTheMap(nullptr, glm::vec3(1.0f, 2.5f, -3.0f));
	EXPECT_EQ(at.x, 32765.0f / 32768.0f);  // 6553 units
	EXPECT_EQ(at.z, -98300.0f / 32768.0f); // -19660.8 truncated to -19660 units
	// without a land the height is above 0, so it is the point's
	EXPECT_EQ(at.y, 2.5f);
	// a point already on the grid stays where it is
	EXPECT_EQ(PlacedOnTheMap(nullptr, glm::vec3(10.0f, 0.0f, 0.0f)).x, 10.0f);
	EXPECT_EQ(PlacedOnTheMap(nullptr, glm::vec3(2.25f, 0.0f, 0.0f)).x, 73725.0f / 32768.0f); // 14745 units
}

TEST(PhysicsObjects, ItsHeightIsKeptAboveTheLandWhereItStands)
{
	// the land is asked once, at the point on the grid, and the height is the land's there plus the height above it,
	// each in float: 3.3 above a land at 12.7 comes back one float higher
	const openblack::test::HeightFieldIsland land([](glm::vec2) { return 12.7f; });
	const auto at = PlacedOnTheMap(&land, glm::vec3(1.0f, 3.3f, -3.0f));
	ASSERT_EQ(land.asked.size(), 1u);
	EXPECT_EQ(land.asked.front(), glm::vec2(32765.0f / 32768.0f, -98300.0f / 32768.0f));
	const float above = 3.3f - 12.7f;
	EXPECT_EQ(at.y, 12.7f + above);
	EXPECT_NE(at.y, 3.3f);
	// on a sloping land the height is the land's at the point on the grid, not at the point itself
	const openblack::test::HeightFieldIsland slope([](glm::vec2 xz) { return xz.x; });
	const auto onSlope = PlacedOnTheMap(&slope, glm::vec3(1.0f, 5.0f, 0.0f));
	EXPECT_EQ(onSlope.y, 32765.0f / 32768.0f + (5.0f - 32765.0f / 32768.0f));
}

namespace
{
using openblack::Locator;
using openblack::MobileStaticInfo;
namespace object_physics = openblack::ecs::object_physics;

/// The kind (mobileType) of every mobile static row, as the game's info.dat has them: 2 is the Rock kind
constexpr std::array<int, static_cast<size_t>(MobileStaticInfo::_COUNT)> k_InfoDatKinds {
    0, 1, 2, 3, 4, 5, 6, 6, 8, 9, 10, 10, 12, 13, 14, 14, 2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  2,  2,  2,  2,  2,  2, 48, 49, 50, 49, 49, 53, 53, 53, 53, 53, 58, 6, 2};

/// The rows of the singing stone, the lanterns, the idol, the weeping stone, the altar, the gate totems, the toys and
/// the sculpture, with their rewards: none of them is of the Rock kind
constexpr std::array k_OtherKinds {MobileStaticInfo::SingingStone_1,
                                   MobileStaticInfo::StreetLantern,
                                   MobileStaticInfo::Idol,
                                   MobileStaticInfo::IdolReward,
                                   MobileStaticInfo::WeepingStone,
                                   MobileStaticInfo::WeepingStoneReward,
                                   MobileStaticInfo::StandaloneAltar,
                                   MobileStaticInfo::GateTotemApe,
                                   MobileStaticInfo::GateTotemBlank,
                                   MobileStaticInfo::GateTotemCow,
                                   MobileStaticInfo::GateTotemTiger,
                                   MobileStaticInfo::ToyBall,
                                   MobileStaticInfo::ToyCuddly,
                                   MobileStaticInfo::ToyDie,
                                   MobileStaticInfo::ToySkittle,
                                   MobileStaticInfo::ToyBowlingBall,
                                   MobileStaticInfo::SculptureInProgress,
                                   MobileStaticInfo::CountryLantern};

/// The rows the game makes a Rock of: the plain rock, every boulder and shaped rock, and the meteor
[[nodiscard]] bool IsRockRow(size_t row)
{
	return row == static_cast<size_t>(MobileStaticInfo::Rock) || row == static_cast<size_t>(MobileStaticInfo::Meteor) ||
	       (row >= static_cast<size_t>(MobileStaticInfo::Boulder1Chalk) &&
	        row <= static_cast<size_t>(MobileStaticInfo::SquarerockVolcanic));
}

/// A registry of the test's own and the given info table, put back as they were when it goes
class RockKinds: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<openblack::ecs::Registry>(); }
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		Locator::infoConstants::reset();
	}

	static void UseInfo(std::unique_ptr<openblack::InfoConstants> info) { Locator::infoConstants::reset(info.release()); }

	/// An info table zeroed but for each mobile static row's kind, as the game's file has it
	static std::unique_ptr<openblack::InfoConstants> InfoDatKinds()
	{
		auto info = std::make_unique<openblack::InfoConstants>();
		for (size_t row = 0; row < k_InfoDatKinds.size(); ++row)
		{
			info->mobileStatic.at(row).mobileType = static_cast<MobileStaticInfo>(k_InfoDatKinds.at(row));
		}
		return info;
	}

	/// A mobile static of the row
	static entt::entity Static(MobileStaticInfo row)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<openblack::ecs::components::MobileStatic>(entity, row);
		return entity;
	}
};
} // namespace

TEST_F(RockKinds, OnlyRowsOfTheRockKindAreRocks)
{
	// A mobile static is a Rock when its row's kind is the Rock kind: the plain rock, the boulders, the shaped rocks and
	// the meteor. The singing stones, the lanterns, the idol, the weeping stone, the altar, the gate totems, the toys and
	// the sculpture are of other kinds, so a knock, a tap or a creature's blow never splits them
	UseInfo(InfoDatKinds());
	for (size_t row = 0; row < k_InfoDatKinds.size(); ++row)
	{
		if (IsRockRow(row))
		{
			EXPECT_TRUE(object_physics::IsRockObject(Static(static_cast<MobileStaticInfo>(row)))) << "row " << row;
			EXPECT_TRUE(object_physics::IsRock(Static(static_cast<MobileStaticInfo>(row)))) << "row " << row;
		}
	}
	for (const auto row : k_OtherKinds)
	{
		EXPECT_FALSE(object_physics::IsRockObject(Static(row))) << "row " << static_cast<int>(row);
		EXPECT_FALSE(object_physics::IsRock(Static(row))) << "row " << static_cast<int>(row);
	}
	// it is the row's kind that decides, not the row: a toy's row of the Rock kind would be a rock
	auto info = InfoDatKinds();
	info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::ToyBall)).mobileType = MobileStaticInfo::Rock;
	UseInfo(std::move(info));
	EXPECT_TRUE(object_physics::IsRockObject(Static(MobileStaticInfo::ToyBall)));
}

TEST_F(RockKinds, ADeadTreeIsNoRock)
{
	// A dead tree answers that it is no rock: a knock or a creature's blow never splits it, and the hand's tap, which
	// breaks rocks, is not offered on it
	UseInfo(InfoDatKinds());
	auto& registry = Locator::entitiesRegistry::value();
	const auto deadTree = registry.Create();
	registry.Assign<openblack::ecs::components::DeadTree>(
	    deadTree, openblack::ecs::components::DeadTree {.type = openblack::TreeInfo::Beech});
	EXPECT_FALSE(object_physics::IsRockObject(deadTree));
}

/// With OPENBLACK_GAME_PATH set to the install: the real mobile static rows' kinds, and the rocks they make
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST_F(RockKinds, realInfoDatRockRows)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(openblack::InfoConstants));
	auto info = std::make_unique<openblack::InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(openblack::InfoConstants));
	for (size_t row = 0; row < k_InfoDatKinds.size(); ++row)
	{
		EXPECT_EQ(static_cast<int>(info->mobileStatic.at(row).mobileType), k_InfoDatKinds.at(row)) << "row " << row;
	}
	UseInfo(std::move(info));
	for (size_t row = 0; row < k_InfoDatKinds.size(); ++row)
	{
		if (row != static_cast<size_t>(MobileStaticInfo::Bonfire))
		{
			EXPECT_EQ(object_physics::IsRockObject(Static(static_cast<MobileStaticInfo>(row))), IsRockRow(row))
			    << "row " << row;
		}
	}
}
