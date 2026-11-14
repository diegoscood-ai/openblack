/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The listing of every reason a save would not be written, in one pass, against the write that stops at the first
// (docs/wiki/engine/saved-games.md#writing-the-world-file, the refusals)

#include <cstdint>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include <SavedRecords.h>
#include <gtest/gtest.h>

#include "ECS/Components/StreetLantern.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/SavedSlot.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;

namespace
{
/// A new game whose lanterns refuse to be written: the game object's and the lists' writers, and a lanterns' writer
/// that refuses each
ClassWriters RefusingWriters()
{
	ClassWriters writers;
	RegisterGameObjectWriter(writers, [] { return ecs::saved_game_save::GameValues {}; });
	RegisterGameListWriters(writers);
	writers.Register(
	    gamesave::StreetLanternRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) { return registry.AllOf<components::StreetLantern>(entity); },
	    [](WriteContext& context, entt::entity entity) -> std::optional<gamesave::SavedRecord> {
		    context.Fail("lantern " + std::to_string(entt::to_integral(entity)) + " refuses");
		    return std::nullopt;
	    });
	return writers;
}

entt::entity Made(ecs::Registry& registry, uint32_t creationIndex)
{
	const auto entity = registry.Create();
	registry.Assign<components::ObjectCreationIndex>(entity, components::ObjectCreationIndex {.value = creationIndex});
	return entity;
}

} // namespace

// The write stops at the first refusal; the listing goes on past it and gives every one, each lantern's own refusal
// among them, the first of them the write's
TEST(SavedWorldWriteRefusals, TheListingGivesEveryRefusalTheWriteStopsAtTheFirst)
{
	ecs::Registry registry;
	const auto writers = RefusingWriters();
	for (uint32_t i = 0; i < 3; ++i)
	{
		registry.Assign<components::StreetLantern>(Made(registry, 10 + i));
	}
	const WorldSource source {.registry = registry};
	const auto written = WriteWorld(writers, source);
	ASSERT_FALSE(written.Complete());

	const auto refusals = ListRefusals(writers, source);
	ASSERT_FALSE(refusals.empty());
	EXPECT_EQ(refusals.front().reason, written.failure);
	EXPECT_EQ(std::ranges::count_if(refusals,
	                                [](const Refusal& refusal) { return refusal.reason.find("refuses") != std::string::npos; }),
	          3);
	// each lantern is named by the game object's list of them
	for (const auto& refusal : refusals)
	{
		if (refusal.reason.find("refuses") != std::string::npos)
		{
			EXPECT_EQ(refusal.namedBy, "game 1");
		}
	}
}

// A refusal of an object that a record names is grouped by the class of that record too, and its line gives the
// object that named the group's first, to find what still holds an object the game has destroyed
TEST(SavedWorldWriteRefusals, TheLinesTellWhoNamedTheObjectRefused)
{
	const std::vector<Refusal> refusals {
	    {.type = "creche", .reason = "entity 4 of creche (object 30) is gone", .namedBy = "villager 12"},
	    {.type = "creche", .reason = "entity 5 of creche (object 31) is gone", .namedBy = "villager 13"},
	    {.type = "creche", .reason = "entity 6 of creche (object 32) is gone", .namedBy = "town 2"}};
	EXPECT_EQ(
	    ecs::saved_slot::GroupRefusals(refusals),
	    (std::vector<std::string> {
	        "creche x2 (first: creche 4, named by villager 12): entity N of creche (object N) is gone (named by villager N)",
	        "creche x1 (first: creche 6, named by town 2): entity N of creche (object N) is gone (named by town N)"}));
}

// The listing's lines group the refusals by class and reason, an object's number read as N, the most given first;
// each line gives the number its group's first refusal names, to find that object by
TEST(SavedWorldWriteRefusals, TheLinesGroupByClassAndReason)
{
	const std::vector<Refusal> refusals {{.type = "tree", .reason = "entity 4 has no writer"},
	                                     {.type = "street lantern", .reason = "lantern 7 refuses"},
	                                     {.type = "tree", .reason = "entity 9 has no writer"},
	                                     {.type = "game", .reason = "the writer of game made no record"}};
	EXPECT_EQ(ecs::saved_slot::GroupRefusals(refusals),
	          (std::vector<std::string> {"tree x2 (first: tree 4): entity N has no writer",
	                                     "game x1: the writer of game made no record",
	                                     "street lantern x1 (first: street lantern 7): lantern N refuses"}));
}

// The readout names where the game object's record names an object it refuses: a global list by its place, a slot of
// the scripts' object table, the tail's fields; nothing for an object it does not name
TEST(SavedWorldWriteRefusals, TheGameRecordsPartThatNamesAnObjectIsNamed)
{
	gamesave::GameHead game;
	game.lists.at(5).numbers = {40, 41};
	game.tail.scriptObjects.at(3).object = {41};
	game.tail.scriptRefs.at(1) = {42};
	game.tail.handObjects.at(0) = {43};
	EXPECT_EQ(GameRecordPartsNaming(game, 40), "global list 5");
	EXPECT_EQ(GameRecordPartsNaming(game, 41), "global list 5, script table slot 3");
	EXPECT_EQ(GameRecordPartsNaming(game, 42), "script's object 1");
	EXPECT_EQ(GameRecordPartsNaming(game, 43), "hand object 0");
	EXPECT_EQ(GameRecordPartsNaming(game, 44), "");
	EXPECT_EQ(GameRecordPartsNaming(game, 0), "");
}
