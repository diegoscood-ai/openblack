/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PuzzleGames.h"

#include <array>
#include <vector>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Transform.h"
#include "ECS/FishPuzzle.h"
#include "ECS/HanoiPuzzle.h"
#include "ECS/HanoiRules.h"
#include "ECS/LionMaze.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/TotemPuzzle.h"
#include "ECS/TreePuzzle.h"
#include "Locator.h"

namespace openblack::ecs
{
namespace
{
using namespace components;

/// The puzzle list, with what each puzzle made: the parts outlive a puzzle the scripts delete in no other way here
struct Tracked
{
	entt::entity puzzle;
	entt::entity bait;
	std::array<entt::entity, 2> shoals;
};
/// What this module keeps between calls (Locator::worldEffects)
struct PuzzleGamesState
{
	std::vector<Tracked> puzzles {};
	/// The land the hand clicked this turn
	map_coords::MapCoords appliedMapPos {};
};

PuzzleGamesState& PuzzleGamesData()
{
	return openblack::Locator::worldEffects::value().Get<PuzzleGamesState>();
}

void DestroyParts(const Tracked& tracked)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the bait (with its fish plot) and the two shoals (off the shoal list, their 15 fish, then the shoal)
	if (tracked.bait != entt::null && registry.Valid(tracked.bait))
	{
		registry.Destroy(tracked.bait);
	}
	for (const auto shoal : tracked.shoals)
	{
		if (shoal != entt::null && registry.Valid(shoal))
		{
			registry.Destroy(shoal);
		}
	}
	registry.SetDirty();
}
} // namespace

entt::entity CreatePuzzleGame(const glm::vec3& position, script::PuzzleGameType type, float yAngleRadians, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	auto& puzzle = registry.Assign<PuzzleGame>(entity);
	puzzle.type = type;
	puzzle.position = position;
	if (Locator::terrainSystem::has_value())
	{
		puzzle.position.y += Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	}
	// (int)(angle x 2048 x (1 / 2 pi))
	puzzle.angle = static_cast<int32_t>(yAngleRadians * 2048.0f * 0.159155f);
	puzzle.scale = scale;
	// the place on the map, at the altitude the script gives
	puzzle.place = {map_coords::ToFixed(position.x), map_coords::ToFixed(position.z), position.y};
	registry.Assign<Transform>(entity, puzzle.position, glm::mat3(1.0f), glm::vec3(scale));
	PuzzleGamesData().puzzles.push_back({entity, entt::null, {entt::null, entt::null}});
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "PuzzleGame {} at ({:.2f}, {:.2f}, {:.2f})", static_cast<int>(type),
	                   puzzle.position.x, puzzle.position.y, puzzle.position.z);
	if (type == script::PuzzleGameType::Hanoi)
	{
		// its game angle from the script's radians, then its pieces, made with it
		registry.Get<PuzzleGame>(entity).angle = hanoi::GameAngleOf(yAngleRadians);
		hanoi_puzzle::Start(entity);
	}
	if (IsTreePuzzle(type))
	{
		// its grid of trees, made with it, at the place on the map and the altitude the script gives
		tree_puzzle_game::Start(entity, {map_coords::ToFixed(position.x), map_coords::ToFixed(position.z), position.y},
		                        yAngleRadians, scale);
	}
	if (IsLionMaze(type))
	{
		// its walled grid, bells and pieces, made with it, at the place on the map and the altitude the script gives
		lion_maze_game::Start(entity, {map_coords::ToFixed(position.x), map_coords::ToFixed(position.z), position.y},
		                      yAngleRadians, scale, type);
	}
	if (IsTotemArcPuzzle(type))
	{
		// its arc of totems, made with it, round the place on the map at the altitude the script gives
		totem_puzzle_game::Start(entity, {map_coords::ToFixed(position.x), map_coords::ToFixed(position.z), position.y}, scale);
	}
	return entity;
}

bool IsPuzzleGamePlayed(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* puzzle = registry.TryGet<PuzzleGame>(entity);
	if (puzzle == nullptr)
	{
		return false;
	}
	if (CompletesAtOnce(puzzle->type))
	{
		puzzle->played = true;
		return true;
	}
	if (puzzle->type == script::PuzzleGameType::Hanoi)
	{
		return hanoi_puzzle::TestSolution(entity);
	}
	if (IsTreePuzzle(puzzle->type))
	{
		return tree_puzzle_game::TestSolution(entity);
	}
	if (IsTotemArcPuzzle(puzzle->type))
	{
		return totem_puzzle_game::TestSolution(entity);
	}
	if (IsLionMaze(puzzle->type))
	{
		return lion_maze_game::TestSolution(entity);
	}
	if (puzzle->type != script::PuzzleGameType::Fishes1)
	{
		return false; // not ported
	}
	// no bait yet -> 0; bait done -> played, 1
	const auto* bait = puzzle->bait != entt::null ? registry.TryGet<const FishBait>(puzzle->bait) : nullptr;
	if (bait == nullptr || !bait->done)
	{
		return false;
	}
	puzzle->played = true;
	return true;
}

script::PuzzleGameStatus PuzzleGameStatusOf(entt::entity entity)
{
	const bool complete = IsPuzzleGamePlayed(entity);
	const auto* puzzle = Locator::entitiesRegistry::value().TryGet<const PuzzleGame>(entity);
	const bool begun = puzzle != nullptr && ((puzzle->type == script::PuzzleGameType::Fishes1 && puzzle->bait != entt::null) ||
	                                         (puzzle->type == script::PuzzleGameType::Hanoi && puzzle->begun) ||
	                                         (IsTreePuzzle(puzzle->type) && tree_puzzle_game::Started(entity)) ||
	                                         (IsTotemArcPuzzle(puzzle->type) && totem_puzzle_game::Started(entity)) ||
	                                         (IsLionMaze(puzzle->type) && lion_maze_game::Started(entity)));
	return PuzzleGameStatusFrom(complete, begun);
}

void ProcessPuzzleGame(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* puzzle = registry.TryGet<PuzzleGame>(entity);
	// nothing once played; the played test marks it
	if (puzzle == nullptr || puzzle->played)
	{
		return;
	}
	if (IsPuzzleGamePlayed(entity))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "PuzzleGame {} played", static_cast<int>(puzzle->type));
		return;
	}
	if (puzzle->type == script::PuzzleGameType::Hanoi)
	{
		hanoi_puzzle::Process(entity);
		return;
	}
	if (IsTreePuzzle(puzzle->type))
	{
		tree_puzzle_game::Process(entity);
		return;
	}
	if (IsTotemArcPuzzle(puzzle->type))
	{
		totem_puzzle_game::Process(entity);
		return;
	}
	if (IsLionMaze(puzzle->type))
	{
		lion_maze_game::Process(entity);
		return;
	}
	if (puzzle->type == script::PuzzleGameType::Fishes1 && puzzle->bait == entt::null)
	{
		const auto parts = CreateFishPuzzle(puzzle->position);
		puzzle = registry.TryGet<PuzzleGame>(entity);
		puzzle->bait = parts.bait;
		puzzle->shoals = parts.shoals;
		for (auto& tracked : PuzzleGamesData().puzzles)
		{
			if (tracked.puzzle == entity)
			{
				tracked.bait = parts.bait;
				tracked.shoals = parts.shoals;
			}
		}
	}
}

void ProcessPuzzleGamesTurn()
{
	auto& state = PuzzleGamesData();
	if (state.puzzles.empty())
	{
		// the land the hand clicked counts for this turn only
		state.appliedMapPos = {};
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto puzzles = state.puzzles;
	state.puzzles.clear();
	for (const auto& tracked : puzzles)
	{
		if (!ecs::IsAvailable(tracked.puzzle) || !registry.AllOf<PuzzleGame>(tracked.puzzle))
		{
			DestroyParts(tracked);
			// a beach temple puzzle's rings belong to no puzzle any more
			hanoi_puzzle::Forget(tracked.puzzle);
			continue;
		}
		state.puzzles.push_back(tracked);
	}
	for (const auto& tracked : std::vector<Tracked>(state.puzzles))
	{
		ProcessPuzzleGame(tracked.puzzle);
	}
	// the land the hand clicked counts for this turn only
	PuzzleGamesData().appliedMapPos = {};
}

void SetPuzzleAppliedMapPos(const map_coords::MapCoords& point)
{
	PuzzleGamesData().appliedMapPos = point;
}

const map_coords::MapCoords& PuzzleAppliedMapPos()
{
	return PuzzleGamesData().appliedMapPos;
}

entt::entity PuzzleGameBait(entt::entity entity)
{
	const auto* puzzle = Locator::entitiesRegistry::value().TryGet<const PuzzleGame>(entity);
	return puzzle != nullptr ? puzzle->bait : entt::null;
}

} // namespace openblack::ecs
