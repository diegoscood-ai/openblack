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

#include "ECS/Components/PuzzleGame.h"
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
/// puzzle its spots' counts, trees, mist and bats, and its lanterns. None, and the write fails, for a puzzle game made
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
	if (puzzle.type == script::PuzzleGameType::Hanoi)
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
