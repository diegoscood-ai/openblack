/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The flocks written from the game's flocks: made in play, field by field, with the original's constant words, and the
// game's list of them newest first (docs/wiki/engine/saved-games.md#the-town-centre-the-flocks-and-the-building-sites)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <optional>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <VillageRecords.h>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Flock.h"
#include "ECS/Flocks.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using ecs::components::Flock;
using gamesave::FlockRecord;

namespace
{
/// A place in the map units a record keeps
gamesave::MapCoords SavedCoords(glm::vec3 point)
{
	const auto coords = map_coords::FromWorld(point);
	return {.x = coords.x, .z = coords.z, .altitude = coords.altitude};
}

class FlockWriteTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		RegisterFlockWriters(_writers);
	}
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	ClassWriters _writers;
};
} // namespace

// A flock made in play, with no player: its domain's centre as its place, no info, the three worked-out words, no
// shepherd, town or temple, no members; its own values as it holds them, the centre it was made at twice, and its
// script id
TEST_F(FlockWriteTest, AFlockMadeInPlayIsWrittenFromItsValues)
{
	const auto entity = ecs::flocks::Create(glm::vec3(1500.0f, 0.0f, 1600.0f), std::nullopt, std::nullopt, 7);
	auto& flock = Reg().Get<Flock>(entity);
	flock.domainCentre = glm::vec3(1510.0f, 0.0f, 1620.0f);
	flock.maxMembers = 3;
	flock.leaderTurns = 129;
	flock.altitude = 45.0f;
	flock.calm = 2;
	flock.followState = 45;
	flock.afterMove = 43;
	flock.followMode = 3;
	const WorldSource source {.registry = Reg()};
	WriteContext context(_writers, source);
	EXPECT_EQ(_writers.ClaimsOf(source.registry, entity), std::vector {FlockRecord::k_Type});
	const auto written = (*_writers.WriterOf(FlockRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<FlockRecord>(*written);
	EXPECT_EQ(record.container.position.position, SavedCoords(glm::vec3(1510.0f, 0.0f, 1620.0f)));
	EXPECT_EQ(record.container.info, 0xFFFFFFFFu);
	EXPECT_EQ(record.container.owner.number, 0u);
	EXPECT_EQ(record.computed, (std::array<uint32_t, 3> {0, 0xFFFFFFFF, 0xFFFFFFFF}));
	EXPECT_EQ(record.shepherd.number, 0u);
	EXPECT_EQ(record.town.number, 0u);
	EXPECT_EQ(record.citadelHeart.number, 0u);
	EXPECT_TRUE(record.members.numbers.empty());
	EXPECT_EQ(record.maxMembers, 3u);
	EXPECT_EQ(record.word, 129u);
	EXPECT_EQ(record.domainRadius, 0x50);
	EXPECT_EQ(record.flockDistance, 0x1E);
	EXPECT_EQ(record.flyingHeight, 45.0f);
	EXPECT_EQ(record.followShepherd, 0u);
	EXPECT_EQ(record.secondWord, 2u);
	EXPECT_EQ(record.savedDomainCentre, SavedCoords(glm::vec3(1500.0f, 0.0f, 1600.0f)));
	EXPECT_EQ(record.otherCentre, record.savedDomainCentre);
	EXPECT_EQ(record.words, (std::array<uint32_t, 3> {45, 43, 3}));
	EXPECT_EQ(record.savedThing.number, 0u);
	EXPECT_EQ(record.lastWord, 7u);
}

// An animal's own flock has no script id: 0, as every saved one
TEST_F(FlockWriteTest, AnAnimalsOwnFlockIsWrittenWithIdZero)
{
	const auto entity = ecs::flocks::Create(glm::vec3(0.0f), std::nullopt, std::nullopt, -1);
	const WorldSource source {.registry = Reg()};
	WriteContext context(_writers, source);
	const auto written = (*_writers.WriterOf(FlockRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	EXPECT_EQ(std::get<FlockRecord>(*written).lastWord, 0u);
}

// The flocks are stamped in the order they are made, and the game's list holds the newest first
TEST_F(FlockWriteTest, TheListHoldsTheNewestFirst)
{
	const auto first = ecs::flocks::Create(glm::vec3(0.0f), std::nullopt, std::nullopt, 1);
	const auto second = ecs::flocks::Create(glm::vec3(0.0f), std::nullopt, std::nullopt, 2);
	EXPECT_LT(Reg().Get<Flock>(first).creationStamp, Reg().Get<Flock>(second).creationStamp);
	const WorldSource source {.registry = Reg()};
	WriteContext context(_writers, source);
	const auto list = (*_writers.ListWriterOf(GlobalList::Flocks))(context);
	ASSERT_EQ(list.size(), 2u);
	EXPECT_EQ(list.at(0), context.Reference(second));
	EXPECT_EQ(list.at(1), context.Reference(first));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}
