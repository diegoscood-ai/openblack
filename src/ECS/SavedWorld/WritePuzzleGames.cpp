/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A puzzle game in a saved game's world file, in the original's order, written from its entity
// (docs/wiki/engine/saved-games.md#the-puzzle-games).

#include <cstdint>

#include <bit>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include <SavedRecords.h>
#include <WorldListRecords.h>
#include <fmt/format.h>

#include "ECS/Components/LionMaze.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/TotemPuzzle.h"
#include "ECS/Components/TreePuzzle.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/HanoiRules.h"
#include "ECS/PuzzleGames.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::PuzzleGameRecord;
namespace components = openblack::ecs::components;

namespace
{
/// The places in the record of the puzzle's type, of whether its play has begun, of its size, of its two grid steps and
/// of the Hanoi puzzle's first ring
constexpr size_t k_TypeWord = 0;
constexpr size_t k_BegunWord = 2;
constexpr size_t k_SizeWord = 1;
constexpr size_t k_CosineStepWord = 0;
constexpr size_t k_SineStepWord = 1;
constexpr size_t k_FirstRing = ecs::hanoi::k_Bases;
/// The places in a tree puzzle's cell of its tree, its mist and its bat
constexpr size_t k_CellTree = 0;
constexpr size_t k_CellMist = 1;
constexpr size_t k_CellBat = 2;
/// The places in the record of a walled maze's two squares (the sheep's or the villager's, then the wolf's), of its
/// wolf, sheep and villager, and of the wolf's and the villager's walks
constexpr size_t k_PreySquareWord = 2;
constexpr size_t k_WolfSquareWord = 4;
constexpr size_t k_WolfPiece = 0;
constexpr size_t k_SheepPiece = 1;
constexpr size_t k_VillagerPiece = 2;
constexpr size_t k_WolfWalk = 0;
constexpr size_t k_VillagerWalk = 1;
/// The thing's mark of a hidden name, which the scripts' things carry
constexpr uint8_t k_NameHidden = 0x40;
/// The lengths the game writes of a puzzle game's kept words and of its four arrays of objects, whole whatever they
/// hold
constexpr size_t k_KeptWords = 6;
constexpr size_t k_FirstArray = 6;
constexpr size_t k_SecondArray = 4;
constexpr size_t k_Lanterns = 4;
constexpr size_t k_FourthArray = 20;

/// The number by which the record names a thing that is there; 0 for none or one that has gone
gamesave::ObjectNumber ReferenceOrNone(WriteContext& context, entt::entity entity)
{
	return entity != entt::null && context.GetRegistry().Valid(entity) ? context.Reference(entity) : 0;
}

/// A puzzle game: the place it was made at with its marks, its type, size, angle and grid steps, whether its play has
/// begun and whether it is played. A loaded one keeps the rest of what its load read; one made since writes the rest as
/// a new puzzle game holds it, every object none and every word 0. The Hanoi puzzle also its bases and rings, a tree
/// puzzle its spots' counts, trees, mist and bats, and its lanterns; the totem puzzle its kept heights, its totems and
/// whether one has been moved (the sparkles over its totems are not written); a walled maze its bells, palms and
/// lanterns, its walls across y (twice each, as the game writes them: its walls across x are not written), its steps,
/// its two squares, its wolf, sheep and villager, its two walks and whether a bell has been rung or a step made (its
/// marker and the two ways its pieces face are not written); the fish puzzle whether its bait and shoals were made
/// (they are not written). None, and the write fails, for a puzzle game made
/// since the load of a type whose state the writer does not write
std::optional<gamesave::SavedRecord> WritePuzzleGame(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& puzzle = registry.Get<const components::PuzzleGame>(entity);
	const auto type = static_cast<uint32_t>(puzzle.type);
	std::optional<PuzzleGameRecord> kept;
	if (auto record = context.KeptRecordOf(entity, PuzzleGameRecord::k_Type); record.has_value())
	{
		kept = std::get<PuzzleGameRecord>(std::move(*record));
	}
	if (!kept.has_value() && !ecs::saved_world_load::PuzzleGameTypeLoaded(type))
	{
		context.Fail(
		    fmt::format("puzzle game {} of type {} keeps a state the writer does not write", entt::to_integral(entity), type));
		return std::nullopt;
	}
	PuzzleGameRecord record = kept.value_or(PuzzleGameRecord {});
	if (!kept.has_value())
	{
		record.thing.flags = registry.AllOf<components::NameHidden>(entity) ? k_NameHidden : 0;
		record.values.values.assign(k_KeptWords, 0);
		record.first.numbers.assign(k_FirstArray, 0);
		record.second.numbers.assign(k_SecondArray, 0);
		record.third.numbers.assign(k_Lanterns, 0);
		record.fourth.numbers.assign(k_FourthArray, 0);
		// the grid's steps, the game's cosine and sine of its angle times its size
		const auto steps = ecs::hanoi::StepsOf(static_cast<uint16_t>(puzzle.angle), puzzle.scale);
		record.thirdWords.at(k_CosineStepWord) = std::bit_cast<uint32_t>(steps.cosine);
		record.thirdWords.at(k_SineStepWord) = std::bit_cast<uint32_t>(steps.sine);
	}
	record.position.position = {.x = puzzle.place.x, .z = puzzle.place.z, .altitude = puzzle.place.altitude};
	record.position.flags = PositionFlagsOf(registry, entity, kept.has_value() ? kept->position.flags : uint16_t {0});
	record.words.at(k_TypeWord) = type;
	record.secondWords.at(k_SizeWord) = std::bit_cast<uint32_t>(puzzle.scale);
	record.half = static_cast<uint16_t>(puzzle.angle);
	record.lastWord = puzzle.played ? 1 : 0;
	if (puzzle.type == script::PuzzleGameType::Fishes1)
	{
		// whether its bait and shoals were made; they are not written
		record.words.at(k_BegunWord) = puzzle.begun || puzzle.bait != entt::null ? 1 : 0;
	}
	else if (puzzle.type == script::PuzzleGameType::Hanoi)
	{
		record.words.at(k_BegunWord) = puzzle.begun ? 1 : 0;
		record.thirdWords.at(k_CosineStepWord) = std::bit_cast<uint32_t>(puzzle.cosineStep);
		record.thirdWords.at(k_SineStepWord) = std::bit_cast<uint32_t>(puzzle.sineStep);
		for (size_t base = 0; base < ecs::hanoi::k_Bases; ++base)
		{
			record.objects.at(base) = {ReferenceOrNone(context, puzzle.bases.at(base))};
		}
		for (size_t ring = 0; ring < ecs::hanoi::k_Rings; ++ring)
		{
			record.objects.at(k_FirstRing + ring) = {ReferenceOrNone(context, puzzle.rings.at(ring))};
		}
	}
	else if (const auto* grid =
	             ecs::IsTreePuzzle(puzzle.type) ? registry.TryGet<const components::TreePuzzle>(entity) : nullptr;
	         grid != nullptr)
	{
		record.words.at(k_BegunWord) = grid->started ? 1 : 0;
		record.thirdWords.at(k_CosineStepWord) = std::bit_cast<uint32_t>(grid->cosineStep);
		record.thirdWords.at(k_SineStepWord) = std::bit_cast<uint32_t>(grid->sineStep);
		for (size_t spot = 0; spot < record.cells.size(); ++spot)
		{
			auto& cell = record.cells.at(spot);
			cell.byte = static_cast<uint8_t>(grid->counts.at(spot));
			cell.objects.at(k_CellTree) = {ReferenceOrNone(context, grid->trees.at(spot))};
			cell.objects.at(k_CellMist) = {ReferenceOrNone(context, grid->mists.at(spot))};
			cell.objects.at(k_CellBat) = {ReferenceOrNone(context, grid->bats.at(spot))};
		}
		record.third.numbers.clear();
		for (const auto lantern : grid->lanterns)
		{
			record.third.numbers.push_back(ReferenceOrNone(context, lantern));
		}
	}
	else if (const auto* maze = ecs::IsLionMaze(puzzle.type) ? registry.TryGet<const components::LionMaze>(entity) : nullptr;
	         maze != nullptr)
	{
		const bool theseus = puzzle.type == script::PuzzleGameType::Thesius1 || puzzle.type == script::PuzzleGameType::Thesius2;
		record.words.at(k_BegunWord) = maze->started ? 1 : 0;
		record.half = maze->angle;
		record.first.numbers.assign(k_FirstArray, 0);
		for (size_t bell = 0; bell < maze->bells.size() && bell < k_FirstArray; ++bell)
		{
			record.first.numbers.at(bell) = ReferenceOrNone(context, maze->bells.at(bell));
		}
		record.second.numbers.clear();
		for (const auto palm : maze->palms)
		{
			record.second.numbers.push_back(ReferenceOrNone(context, palm));
		}
		record.third.numbers.clear();
		for (const auto lantern : maze->lanterns)
		{
			record.third.numbers.push_back(ReferenceOrNone(context, lantern));
		}
		for (size_t x = 0; x < maze->wallsAlongY.size(); ++x)
		{
			auto& pairs = x < record.rows.size() ? record.rows.at(x).pairs : record.lastPairs;
			for (size_t y = 0; y < maze->wallsAlongY.at(x).size(); ++y)
			{
				const gamesave::Ref wall {ReferenceOrNone(context, maze->wallsAlongY.at(x).at(y))};
				pairs.at(2 * y) = wall;
				pairs.at(2 * y + 1) = wall;
			}
		}
		const auto& prey = theseus ? maze->villagerSquare : maze->sheepSquare;
		record.thirdWords.at(k_CosineStepWord) = std::bit_cast<uint32_t>(maze->cosineStep);
		record.thirdWords.at(k_SineStepWord) = std::bit_cast<uint32_t>(maze->sineStep);
		record.thirdWords.at(k_PreySquareWord) = std::bit_cast<uint32_t>(prey.x);
		record.thirdWords.at(k_PreySquareWord + 1) = std::bit_cast<uint32_t>(prey.y);
		record.thirdWords.at(k_WolfSquareWord) = std::bit_cast<uint32_t>(maze->wolfSquare.x);
		record.thirdWords.at(k_WolfSquareWord + 1) = std::bit_cast<uint32_t>(maze->wolfSquare.y);
		record.others.at(k_WolfPiece) = {ReferenceOrNone(context, maze->wolf)};
		record.others.at(k_SheepPiece) = {ReferenceOrNone(context, maze->sheep)};
		record.others.at(k_VillagerPiece) = {ReferenceOrNone(context, maze->villager)};
		record.halves.at(k_WolfWalk) = static_cast<uint16_t>(maze->walk);
		record.halves.at(k_VillagerWalk) = static_cast<uint16_t>(maze->villagerWalk);
	}
	else if (const auto* arc =
	             ecs::IsTotemArcPuzzle(puzzle.type) ? registry.TryGet<const components::TotemPuzzle>(entity) : nullptr;
	         arc != nullptr)
	{
		record.words.at(k_BegunWord) = arc->started ? 1 : 0;
		record.values.values.clear();
		for (const auto height : arc->heights)
		{
			record.values.values.push_back(static_cast<uint32_t>(height));
		}
		record.first.numbers.clear();
		for (const auto totem : arc->totems)
		{
			record.first.numbers.push_back(ReferenceOrNone(context, totem));
		}
	}
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterPuzzleGameWriters(ClassWriters& writers)
{
	writers.Register(
	    PuzzleGameRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::PuzzleGame>(entity); },
	    WritePuzzleGame);
}
