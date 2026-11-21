/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The puzzle games and the Hanoi puzzle's blocks in a saved game: planned from the game's list, made again with the
// state their records hold (played, begun, the Hanoi puzzle's stacks, a tree puzzle's grid) and written back as read,
// in a small world (the test's registry, the world's lists with the real map cells, zeroed tables and no land, the
// ground at 0). Each service is put back when the test ends. See docs/wiki/engine/saved-games.md#the-puzzle-games

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <variant>
#include <vector>

#include <ForestRecords.h>
#include <SavedRecords.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/TreePuzzle.h"
#include "ECS/HanoiRules.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PuzzleGames.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/Implementations/WorldEffects.h"
#include "ECS/TreePuzzleRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace load = openblack::ecs::saved_world_load;
namespace world_write = openblack::ecs::saved_world_write;

namespace
{
/// Land 2's beach temple: the puzzle's spot, as HanoiFlood makes it, and its half turn
constexpr glm::vec3 k_Spot {2386.968f, 0.0f, 2633.286f};
constexpr auto k_PuzzleList = static_cast<size_t>(world_write::GlobalList::PuzzleGame);

class SavedPuzzleGamesTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		for (const auto* name : {"game", "scripting"})
		{
			if (spdlog::get(name) == nullptr)
			{
				spdlog::create<spdlog::sinks::null_sink_mt>(name);
			}
		}
		Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
		Fresh();
	}

	void TearDown() override
	{
		Clear();
		Locator::infoConstants::reset();
	}

	/// A world with nothing in it yet, and no puzzle game in the puzzles' turn
	static void Fresh()
	{
		Locator::worldEffects::emplace<systems::WorldEffects>();
		test::EmplaceMapAndVillagerDefaults();
		Locator::entitiesRegistry::emplace<Registry>();
		test::EmplaceWorldSystems();
	}

	static void Clear()
	{
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}

	static Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static world_write::ClassWriters PuzzleWriters()
	{
		world_write::ClassWriters writers;
		world_write::RegisterPuzzleGameWriters(writers);
		world_write::RegisterPotWriters(writers);
		return writers;
	}

	static load::Makers PuzzleMakers()
	{
		return {.hanoiBlock = load::MakeHanoiBlockEntity, .puzzleGame = load::MakePuzzleGameEntity};
	}

	test::RestoreService<Locator::worldEffects> _effects;

	/// A world file holding the records given by their numbers, the puzzle games listed in the game's list in the order
	/// given (newest first), read in the order of their numbers
	static gamesave::WorldRead WorldOf(const std::map<gamesave::ObjectNumber, gamesave::SavedRecord>& records,
	                                   const std::vector<gamesave::ObjectNumber>& puzzles)
	{
		gamesave::WorldRead world;
		gamesave::GameHead game;
		game.lists.at(k_PuzzleList).numbers = puzzles;
		const auto last =
		    records.empty() ? gamesave::ObjectNumber {1} : std::max<gamesave::ObjectNumber>(1, records.rbegin()->first);
		world.ResizeObjects(last);
		gamesave::PutRecord(world.objects.front(), game);
		for (const auto& [number, record] : records)
		{
			gamesave::PutRecord(world.objects.at(number - 1), record);
			world.loadOrder.push_back(number);
		}
		return world;
	}

	/// A puzzle game's record of a type, played or not, its play begun or not
	static gamesave::PuzzleGameRecord PuzzleRecord(script::PuzzleGameType type, bool played, bool begun)
	{
		gamesave::PuzzleGameRecord record;
		record.position.position = {.x = map_coords::ToFixed(k_Spot.x), .z = map_coords::ToFixed(k_Spot.z), .altitude = 0.0f};
		record.position.flags = 0;
		record.words = {static_cast<uint32_t>(type), 0, begun ? 1U : 0U, 0};
		record.values.values.assign(6, 0);
		record.first.numbers.assign(6, 0);
		record.second.numbers.assign(4, 0);
		record.third.numbers.assign(4, 0);
		record.fourth.numbers.assign(20, 0);
		record.secondWords = {0, std::bit_cast<uint32_t>(1.0f)};
		record.half = 1024;
		record.lastWord = played ? 1 : 0;
		return record;
	}

	[[nodiscard]] static const load::CensusLine* LineOf(const load::Census& census, uint32_t type)
	{
		const auto line = std::ranges::find(census, type, &load::CensusLine::type);
		return line != census.end() ? &*line : nullptr;
	}
};
} // namespace

// The game's list holds the puzzle games newest first: the load plans them from its last to its first, so that the
// newest is made last and processed last again; the Hanoi blocks that stand on the map in the order they were read
TEST_F(SavedPuzzleGamesTest, ThePuzzleGamesArePlannedFromTheListsLastToItsFirst)
{
	std::map<gamesave::ObjectNumber, gamesave::SavedRecord> records;
	records.emplace(2, PuzzleRecord(script::PuzzleGameType::Hanoi, false, false));
	records.emplace(3, PuzzleRecord(script::PuzzleGameType::Tree1, false, true));
	// a puzzle game no longer in the list is not made
	records.emplace(4, PuzzleRecord(script::PuzzleGameType::Tree2, false, false));
	gamesave::HanoiBlockRecord block;
	block.base.object.info = 1970 - static_cast<uint32_t>(MobileObjectInfo::HanoiPuzzlePart1);
	records.emplace(5, block);
	auto held = block;
	held.thing.flags = 1;
	records.emplace(6, held);
	const auto plan = load::PlanOf(WorldOf(records, {3, 2}));
	ASSERT_EQ(plan.puzzleGames.size(), 2u);
	EXPECT_EQ(plan.puzzleGames.at(0).number, 2u);
	EXPECT_EQ(plan.puzzleGames.at(1).number, 3u);
	ASSERT_EQ(plan.hanoiBlocks.size(), 1u);
	EXPECT_EQ(plan.hanoiBlocks.front().number, 5u);
	EXPECT_EQ(plan.hanoiBlocks.front().type, MobileObjectInfo::HanoiPuzzlePart1);
}

// The load makes the types whose state the record holds whole and no other: a log line says why one is not made
TEST_F(SavedPuzzleGamesTest, OnlyTheTypesWhoseStateTheRecordHoldsAreMade)
{
	using enum script::PuzzleGameType;
	for (const auto type : {Tree1, Tree2, Hanoi, Maze1, Maze2, Fishes2, None})
	{
		EXPECT_TRUE(load::PuzzleGameTypeLoaded(static_cast<uint32_t>(type))) << static_cast<int>(type);
	}
	for (const auto type : {Totem1, Totem3, Thesius1, Tilt1, Fishes1, Immersion, Lionsheep, Chess})
	{
		EXPECT_FALSE(load::PuzzleGameTypeLoaded(static_cast<uint32_t>(type))) << static_cast<int>(type);
	}
	const auto made = load::MakePuzzleGameEntity({.number = 2, .record = PuzzleRecord(Fishes1, false, false)},
	                                             [](gamesave::ObjectNumber) { return entt::entity {entt::null}; });
	EXPECT_EQ(made, entt::entity(entt::null));
}

// A puzzle game made again keeps its saved played mark and its script link: the scripts' PLAYED of a played Hanoi
// puzzle answers 1, of a tree puzzle not played the test of its state, and the load's census counts each with its
// number kept
TEST_F(SavedPuzzleGamesTest, APuzzleGameComesBackPlayedAsSavedAndLinkedToItsNumber)
{
	std::map<gamesave::ObjectNumber, gamesave::SavedRecord> records;
	records.emplace(2, PuzzleRecord(script::PuzzleGameType::Maze1, true, false));
	// a tree puzzle whose slots hold no tree: not played, its test says no
	records.emplace(3, PuzzleRecord(script::PuzzleGameType::Tree1, false, false));
	records.emplace(4, PuzzleRecord(script::PuzzleGameType::Hanoi, true, true));
	const auto world = WorldOf(records, {4, 3, 2});
	world_write::LoadedObjects loaded;
	const auto census = load::Apply(load::PlanOf(world), PuzzleMakers(), &loaded);
	ASSERT_TRUE(loaded.contains(2));
	ASSERT_TRUE(loaded.contains(3));
	EXPECT_EQ(loaded.at(2).type, gamesave::PuzzleGameRecord::k_Type);
	const auto maze = loaded.at(2).entity;
	const auto tree = loaded.at(3).entity;
	EXPECT_TRUE(Reg().Get<const PuzzleGame>(maze).played);
	EXPECT_TRUE(IsPuzzleGamePlayed(maze));
	EXPECT_FALSE(Reg().Get<const PuzzleGame>(tree).played);
	EXPECT_FALSE(IsPuzzleGamePlayed(tree));
	EXPECT_EQ(PuzzleGameStatusOf(tree), script::PuzzleGameStatus::GameNotYetBegun);
	ASSERT_TRUE(loaded.contains(4));
	const auto hanoi = loaded.at(4).entity;
	EXPECT_TRUE(Reg().Get<const PuzzleGame>(hanoi).begun);
	EXPECT_TRUE(IsPuzzleGamePlayed(hanoi));
	EXPECT_EQ(PuzzleGameStatusOf(hanoi), script::PuzzleGameStatus::Won);
	const auto* line = LineOf(census, gamesave::PuzzleGameRecord::k_Type);
	ASSERT_NE(line, nullptr);
	EXPECT_EQ(line->saved, 3u);
	EXPECT_EQ(line->made, 3u);
}

// A tree puzzle comes back with its grid as saved: its place and steps, each spot's count, tree, mist and bat, its
// lanterns and whether its play has begun; a tree saved on its spot is on it. Written back, the record is the one read
TEST_F(SavedPuzzleGamesTest, ATreePuzzleComesBackWithItsGridAndIsWrittenBackAsRead)
{
	auto record = PuzzleRecord(script::PuzzleGameType::Tree1, false, true);
	const auto steps = hanoi::StepsOf(1024, 1.0f);
	record.thirdWords.at(0) = std::bit_cast<uint32_t>(steps.cosine);
	record.thirdWords.at(1) = std::bit_cast<uint32_t>(steps.sine);
	const auto place = map_coords::MapCoords {.x = record.position.position.x, .z = record.position.position.z};
	// the things it names, each a loaded object of its class at its number from 10 on
	world_write::LoadedObjects loaded;
	std::map<gamesave::ObjectNumber, entt::entity> things;
	gamesave::ObjectNumber next = 10;
	const auto thing = [&](auto saved, std::optional<map_coords::MapCoords> at) {
		const auto entity = Reg().Create();
		if (at.has_value())
		{
			saved.base.position.position = {.x = at->x, .z = at->z, .altitude = at->altitude};
			Reg().Assign<Transform>(entity, map_coords::ToWorld(*at), glm::mat3(1.0f), glm::vec3(1.0f));
		}
		Reg().Assign<LoadedRecord<decltype(saved)>>(entity, saved);
		loaded.emplace(next, world_write::LoadedObject {.entity = entity, .type = decltype(saved)::k_Type});
		things.emplace(next, entity);
		return next++;
	};
	for (int32_t across = 0; across < tree_puzzle::k_Side; ++across)
	{
		for (int32_t along = 0; along < tree_puzzle::k_Side; ++along)
		{
			const auto spot = tree_puzzle::SpotIndex(across, along);
			auto& cell = record.cells.at(spot);
			cell.byte = static_cast<uint8_t>(spot % 3);
			// the last spot's tree was moved off it
			const auto at = hanoi::GridPlace(place, steps, across, along);
			auto moved = at;
			moved.x += spot == 8 ? map_coords::ToFixed(2.0f) : 0;
			cell.objects.at(0) = {thing(gamesave::TreeRecord {}, moved)};
			cell.objects.at(1) = {spot % 2 == 0 ? thing(gamesave::TreeRecord {}, std::nullopt) : 0};
		}
	}
	const auto objectOf = [&things](gamesave::ObjectNumber number) {
		const auto found = things.find(number);
		return found != things.end() ? found->second : entt::entity {entt::null};
	};
	const auto puzzle = load::MakePuzzleGameEntity({.number = 2, .record = record}, objectOf);
	ASSERT_NE(puzzle, entt::entity(entt::null));
	loaded.emplace(2, world_write::LoadedObject {.entity = puzzle, .type = gamesave::PuzzleGameRecord::k_Type});
	const auto& grid = Reg().Get<const TreePuzzle>(puzzle);
	EXPECT_EQ(grid.place, place);
	EXPECT_EQ(grid.cosineStep, steps.cosine);
	EXPECT_EQ(grid.sineStep, steps.sine);
	EXPECT_TRUE(grid.started);
	for (size_t spot = 0; spot < grid.counts.size(); ++spot)
	{
		EXPECT_EQ(grid.counts.at(spot), static_cast<int8_t>(spot % 3));
		EXPECT_EQ(grid.trees.at(spot), objectOf(record.cells.at(spot).objects.at(0).number));
		EXPECT_EQ(grid.mists.at(spot), objectOf(record.cells.at(spot).objects.at(1).number));
		EXPECT_EQ(grid.bats.at(spot), entt::entity(entt::null));
		// a tree saved on its spot stands where the puzzle keeps it; the moved one does not
		EXPECT_EQ(object::MapCoordsOf(grid.trees.at(spot)) == grid.placedAt.at(spot), spot != 8) << spot;
	}
	EXPECT_EQ(PuzzleGameStatusOf(puzzle), script::PuzzleGameStatus::GameInProgress);

	std::map<gamesave::ObjectNumber, gamesave::SavedRecord> records {{2, record}};
	const auto world = WorldOf(records, {2});
	const auto writers = PuzzleWriters();
	const world_write::WorldSource source {.registry = Reg(), .landscape = {}, .kept = &world, .loaded = loaded};
	world_write::WriteContext context(writers, source);
	const auto written = (*writers.WriterOf(gamesave::PuzzleGameRecord::k_Type))(context, puzzle);
	ASSERT_TRUE(written.has_value());
	EXPECT_EQ(std::get<gamesave::PuzzleGameRecord>(*written), record);
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// The beach temple's Hanoi puzzle as a script makes it, written, then loaded into a new world: the puzzle and its seven
// blocks come back on their stacks, linked as they were, where they stood, not played and not begun; the puzzle's next
// turn leaves every piece where it is, and the second write gives the records of the first
TEST_F(SavedPuzzleGamesTest, AHanoiPuzzleWrittenAndLoadedComesBackOnItsStacks)
{
	const auto made = CreatePuzzleGame(k_Spot, script::PuzzleGameType::Hanoi, glm::radians(180.0f), 1.0f);
	const auto& game = Reg().Get<const PuzzleGame>(made);
	std::vector<entt::entity> pieces(game.bases.begin(), game.bases.end());
	pieces.insert(pieces.end(), game.rings.begin(), game.rings.end());
	std::vector<glm::vec3> places;
	for (const auto piece : pieces)
	{
		places.push_back(Reg().Get<const Transform>(piece).position);
	}

	// the first write, of a world that was not loaded
	std::map<gamesave::ObjectNumber, gamesave::SavedRecord> records;
	gamesave::ObjectNumber puzzleNumber = 0;
	std::vector<gamesave::ObjectNumber> pieceNumbers;
	{
		const auto writers = PuzzleWriters();
		const world_write::WorldSource source {.registry = Reg()};
		world_write::WriteContext context(writers, source);
		puzzleNumber = context.Reference(made);
		for (const auto piece : pieces)
		{
			pieceNumbers.push_back(context.Reference(piece));
		}
		const auto puzzle = (*writers.WriterOf(gamesave::PuzzleGameRecord::k_Type))(context, made);
		ASSERT_TRUE(puzzle.has_value());
		records.emplace(puzzleNumber, *puzzle);
		for (size_t slot = 0; slot < pieces.size(); ++slot)
		{
			const auto block = (*writers.WriterOf(gamesave::HanoiBlockRecord::k_Type))(context, pieces.at(slot));
			ASSERT_TRUE(block.has_value());
			records.emplace(pieceNumbers.at(slot), *block);
		}
		EXPECT_TRUE(context.Failure().empty()) << context.Failure();
		// the moving objects' list of the game holds the seven blocks, the newest first
		const auto listed = (*writers.ListWriterOf(world_write::GlobalList::MobileObjects))(context);
		EXPECT_EQ(std::set<gamesave::ObjectNumber>(listed.begin(), listed.end()),
		          std::set<gamesave::ObjectNumber>(pieceNumbers.begin(), pieceNumbers.end()));
	}
	const auto& saved = std::get<gamesave::PuzzleGameRecord>(records.at(puzzleNumber));
	EXPECT_EQ(saved.words.at(0), static_cast<uint32_t>(script::PuzzleGameType::Hanoi));
	for (size_t slot = 0; slot < pieces.size(); ++slot)
	{
		EXPECT_EQ(saved.objects.at(slot).number, pieceNumbers.at(slot));
	}

	// a new world, loaded from those records
	Clear();
	Fresh();
	const auto world = WorldOf(records, {puzzleNumber});
	world_write::LoadedObjects loaded;
	const auto census = load::Apply(load::PlanOf(world), PuzzleMakers(), &loaded);
	const auto* blocksLine = LineOf(census, gamesave::HanoiBlockRecord::k_Type);
	ASSERT_NE(blocksLine, nullptr);
	EXPECT_EQ(blocksLine->made, 7u);
	ASSERT_TRUE(loaded.contains(puzzleNumber));
	const auto puzzle = loaded.at(puzzleNumber).entity;
	const auto& loadedGame = Reg().Get<const PuzzleGame>(puzzle);
	EXPECT_EQ(loadedGame.type, script::PuzzleGameType::Hanoi);
	EXPECT_FALSE(loadedGame.played);
	EXPECT_FALSE(loadedGame.begun);
	EXPECT_EQ(loadedGame.calmTurns, hanoi::k_CalmTop);
	std::vector<entt::entity> back;
	for (size_t slot = 0; slot < pieces.size(); ++slot)
	{
		ASSERT_TRUE(loaded.contains(pieceNumbers.at(slot)));
		back.push_back(loaded.at(pieceNumbers.at(slot)).entity);
		EXPECT_EQ(Reg().Get<const Transform>(back.back()).position, places.at(slot));
		EXPECT_EQ(Reg().Get<const HanoiBlock>(back.back()).puzzle, puzzle);
	}
	EXPECT_TRUE(std::ranges::equal(loadedGame.bases, std::span(back).first(hanoi::k_Bases)));
	EXPECT_TRUE(std::ranges::equal(loadedGame.rings, std::span(back).subspan(hanoi::k_Bases)));
	// the rings stacked on the first base, the largest at the bottom
	auto below = loadedGame.bases.front();
	for (const auto ring : loadedGame.rings)
	{
		EXPECT_EQ(Reg().Get<const HanoiBlock>(ring).below, below);
		EXPECT_EQ(Reg().Get<const HanoiBlock>(below).above, ring);
		below = ring;
	}
	EXPECT_FALSE(IsPuzzleGamePlayed(puzzle));
	EXPECT_EQ(PuzzleGameStatusOf(puzzle), script::PuzzleGameStatus::GameNotYetBegun);
	ProcessPuzzleGamesTurn();
	EXPECT_TRUE(std::ranges::equal(Reg().Get<const PuzzleGame>(puzzle).rings, std::span(back).subspan(hanoi::k_Bases)));
	for (size_t slot = 0; slot < back.size(); ++slot)
	{
		EXPECT_TRUE(Reg().Valid(back.at(slot)));
		EXPECT_EQ(Reg().Get<const Transform>(back.at(slot)).position, places.at(slot));
	}

	// the second write gives back what the first wrote
	const auto writers = PuzzleWriters();
	const world_write::WorldSource source {.registry = Reg(), .landscape = {}, .kept = &world, .loaded = loaded};
	world_write::WriteContext context(writers, source);
	const auto puzzleAgain = (*writers.WriterOf(gamesave::PuzzleGameRecord::k_Type))(context, puzzle);
	ASSERT_TRUE(puzzleAgain.has_value());
	EXPECT_EQ(std::get<gamesave::PuzzleGameRecord>(*puzzleAgain), saved);
	for (size_t slot = 0; slot < back.size(); ++slot)
	{
		const auto block = (*writers.WriterOf(gamesave::HanoiBlockRecord::k_Type))(context, back.at(slot));
		ASSERT_TRUE(block.has_value());
		EXPECT_EQ(std::get<gamesave::HanoiBlockRecord>(*block),
		          std::get<gamesave::HanoiBlockRecord>(records.at(pieceNumbers.at(slot))))
		    << slot;
	}
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// A puzzle game made since a load of a type whose state the writer does not write stops the write; the game's list
// of puzzle games runs newest first by the order they were made
TEST_F(SavedPuzzleGamesTest, ThePuzzleGamesListRunsNewestFirstAndAnUnwrittenTypeStopsTheWrite)
{
	const auto first = CreatePuzzleGame(k_Spot, script::PuzzleGameType::Maze1, 0.0f, 1.0f);
	const auto second = CreatePuzzleGame(k_Spot, script::PuzzleGameType::Maze2, 0.0f, 1.0f);
	auto writers = PuzzleWriters();
	world_write::RegisterGameListWriters(writers);
	const world_write::WorldSource source {.registry = Reg()};
	world_write::WriteContext context(writers, source);
	const auto listed = (*writers.ListWriterOf(world_write::GlobalList::PuzzleGame))(context);
	EXPECT_EQ(listed, (std::vector {context.Reference(second), context.Reference(first)}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();

	world_write::WriteContext refused(writers, source);
	const auto fish = CreatePuzzleGame(k_Spot, script::PuzzleGameType::Fishes1, 0.0f, 1.0f);
	EXPECT_FALSE((*writers.WriterOf(gamesave::PuzzleGameRecord::k_Type))(refused, fish).has_value());
	EXPECT_FALSE(refused.Failure().empty());
}

namespace
{
std::optional<gamesave::WorldRead> FrozenWorld(const std::filesystem::path& relative)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	std::ifstream file(std::filesystem::path(game) / "Profiles" / relative, std::ios::binary);
	if (!file)
	{
		return std::nullopt;
	}
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
	return gamesave::ReadWorld(bytes);
}
} // namespace

// Integration test: needs the frozen game's profile saves (OPENBLACK_TEST_GAME_PATH); skipped without them. Each of the
// original's Land 2 saves holds the beach temple's Hanoi puzzle and the first tree puzzle, neither played nor begun,
// and the Hanoi puzzle's seven blocks. Made again, the Hanoi puzzle answers PLAYED with its state (not played, not
// begun) and its record is written back as read
TEST_F(SavedPuzzleGamesTest, TheOriginalsLand2SavesPuzzleGamesComeBackAsSaved)
{
	size_t read = 0;
	for (const auto* slot : {"15/2/15.sav", "16/2/16.sav", "20/20.sav"})
	{
		const auto world = FrozenWorld(std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / slot);
		if (!world.has_value())
		{
			continue;
		}
		++read;
		Clear();
		Fresh();
		const auto plan = load::PlanOf(*world);
		ASSERT_EQ(plan.puzzleGames.size(), 2u) << slot;
		ASSERT_EQ(plan.hanoiBlocks.size(), 7u) << slot;
		std::set<uint32_t> types;
		const load::PlannedPuzzleGame* hanoi = nullptr;
		for (const auto& puzzle : plan.puzzleGames)
		{
			types.insert(puzzle.record.words.at(0));
			EXPECT_EQ(puzzle.record.lastWord, 0u) << slot;
			EXPECT_EQ(puzzle.record.words.at(2), 0u) << slot;
			if (puzzle.record.words.at(0) == static_cast<uint32_t>(script::PuzzleGameType::Hanoi))
			{
				hanoi = &puzzle;
			}
		}
		EXPECT_EQ(types, (std::set {static_cast<uint32_t>(script::PuzzleGameType::Tree1),
		                            static_cast<uint32_t>(script::PuzzleGameType::Hanoi)}))
		    << slot;
		ASSERT_NE(hanoi, nullptr);

		// the blocks, then the puzzle, each kept by its number
		world_write::LoadedObjects loaded;
		std::map<gamesave::ObjectNumber, entt::entity> blocks;
		for (const auto& block : plan.hanoiBlocks)
		{
			const auto entity = load::MakeHanoiBlockEntity(block);
			ASSERT_NE(entity, entt::entity(entt::null)) << slot;
			blocks.emplace(block.number, entity);
			loaded.emplace(block.number,
			               world_write::LoadedObject {.entity = entity, .type = gamesave::HanoiBlockRecord::k_Type});
		}
		const auto puzzle = load::MakePuzzleGameEntity(*hanoi, [&blocks](gamesave::ObjectNumber number) {
			const auto found = blocks.find(number);
			return found != blocks.end() ? found->second : entt::entity {entt::null};
		});
		ASSERT_NE(puzzle, entt::entity(entt::null)) << slot;
		loaded.emplace(hanoi->number, world_write::LoadedObject {.entity = puzzle, .type = gamesave::PuzzleGameRecord::k_Type});
		EXPECT_FALSE(IsPuzzleGamePlayed(puzzle)) << slot;
		EXPECT_EQ(PuzzleGameStatusOf(puzzle), script::PuzzleGameStatus::GameNotYetBegun) << slot;
		for (const auto piece : Reg().Get<const PuzzleGame>(puzzle).rings)
		{
			ASSERT_NE(piece, entt::entity(entt::null)) << slot;
			EXPECT_EQ(Reg().Get<const HanoiBlock>(piece).puzzle, puzzle) << slot;
		}

		const auto writers = PuzzleWriters();
		const world_write::WorldSource source {.registry = Reg(), .landscape = {}, .kept = &*world, .loaded = loaded};
		world_write::WriteContext context(writers, source);
		const auto written = (*writers.WriterOf(gamesave::PuzzleGameRecord::k_Type))(context, puzzle);
		ASSERT_TRUE(written.has_value()) << slot;
		EXPECT_EQ(std::get<gamesave::PuzzleGameRecord>(*written), hanoi->record) << slot;
		for (const auto& block : plan.hanoiBlocks)
		{
			const auto again = (*writers.WriterOf(gamesave::HanoiBlockRecord::k_Type))(context, blocks.at(block.number));
			ASSERT_TRUE(again.has_value()) << slot;
			const auto& record = std::get<gamesave::HanoiBlockRecord>(*again);
			EXPECT_EQ(record.point, block.record.point) << slot;
			EXPECT_EQ(record.objects, block.record.objects) << slot;
		}
		EXPECT_TRUE(context.Failure().empty()) << context.Failure();
	}
	if (read == 0)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 2 saves";
	}
}
