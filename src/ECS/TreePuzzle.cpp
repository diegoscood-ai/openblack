/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TreePuzzle.h"

#include <cstdint>

#include <utility>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "Common/GameRandom.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/MistArchetype.h"
#include "ECS/Archetypes/StreetLanternArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/TreePuzzle.h"
#include "ECS/HanoiRules.h"
#include "ECS/Life.h"
#include "ECS/ObjectGhosts.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/TreePuzzleRules.h"
#include "ECS/Trees.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// A tree on its spot grows by this much a turn, at once (Tree::Grow's instant step)
constexpr float k_TurnGrowth = tree_puzzle::k_GrowthStep;

hanoi::Steps StepsOf(const TreePuzzle& grid)
{
	return {.cosine = grid.cosineStep, .sine = grid.sineStep};
}

map_coords::MapCoords SpotPlace(const TreePuzzle& grid, int32_t across, int32_t along)
{
	return hanoi::GridPlace(grid.place, StepsOf(grid), across, along);
}

/// A new small tree on a spot, of the kind its state shows, held by the scripts
void MakeTree(TreePuzzle& grid, script::PuzzleGameType type, int32_t across, int32_t along)
{
	const auto spot = tree_puzzle::SpotIndex(across, along);
	const auto world = map_coords::ToWorld(SpotPlace(grid, across, along));
	const auto count = grid.counts.at(spot);
	const auto tree = archetypes::TreeArchetype::Create(0, world, tree_puzzle::TreeKindOf(type, count), false, 0.0f,
	                                                    tree_puzzle::FullSizeOf(type, count), tree_puzzle::k_StartSize);
	script_held::SetControlledByScript(tree, true);
	grid.trees.at(spot) = tree;
	grid.placedAt.at(spot) = object::MapCoordsOf(tree);
}

/// A spot on the grid changes its state: its old tree melts away as a ghost and goes, and a new small tree is planted
void ChangeSpot(TreePuzzle& grid, script::PuzzleGameType type, int32_t across, int32_t along)
{
	if (!tree_puzzle::OnGrid(across, along))
	{
		return;
	}
	const auto spot = tree_puzzle::SpotIndex(across, along);
	grid.counts.at(spot) = tree_puzzle::Changed(grid.counts.at(spot));
	if (const auto old = grid.trees.at(spot); old != entt::null)
	{
		if (Locator::entitiesRegistry::value().Valid(old))
		{
			object_ghosts::Add(old);
			ToBeDeleted(old);
		}
	}
	MakeTree(grid, type, across, along);
}

/// A pull of a spot changes it and the four spots beside it
void Pull(TreePuzzle& grid, script::PuzzleGameType type, int32_t across, int32_t along)
{
	for (const auto& [changedAcross, changedAlong] : tree_puzzle::PulledSpots(across, along))
	{
		ChangeSpot(grid, type, changedAcross, changedAlong);
	}
}

/// Whether a tree of the puzzle is available and stands at its spot's place: its MapCoords equal to the place it was
/// planted at, x, z and altitude, as the original's MapCoords comparison
bool OnItsSpot(const TreePuzzle& grid, size_t spot, entt::entity tree)
{
	return IsAvailable(tree) && object::MapCoordsOf(tree) == grid.placedAt.at(spot);
}

TreePuzzle* GridOf(entt::entity puzzle)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(puzzle) ? registry.TryGet<TreePuzzle>(puzzle) : nullptr;
}
} // namespace

void tree_puzzle_game::Start(entt::entity puzzle, const map_coords::MapCoords& place, float yAngleRadians, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto type = registry.Get<const PuzzleGame>(puzzle).type;
	const auto steps = hanoi::StepsOf(static_cast<uint16_t>(hanoi::GameAngleOf(yAngleRadians)), scale);
	auto& grid = registry.Assign<TreePuzzle>(puzzle);
	grid.place = place;
	grid.cosineStep = steps.cosine;
	grid.sineStep = steps.sine;
	// a small tree on each spot, then seven pulls of random spots: the first draw is the spot along, the second across
	for (int32_t across = 0; across < tree_puzzle::k_Side; ++across)
	{
		for (int32_t along = 0; along < tree_puzzle::k_Side; ++along)
		{
			MakeTree(grid, type, across, along);
		}
	}
	for (int32_t move = 0; move < tree_puzzle::k_StartMoves; ++move)
	{
		const auto along = static_cast<int32_t>(game_random::GameRand(tree_puzzle::k_Side));
		const auto across = static_cast<int32_t>(game_random::GameRand(tree_puzzle::k_Side));
		Pull(grid, type, across, along);
	}
	for (int32_t across = 0; across < tree_puzzle::k_Side; ++across)
	{
		for (int32_t along = 0; along < tree_puzzle::k_Side; ++along)
		{
			const auto at = map_coords::ToMetres(SpotPlace(grid, across, along));
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Tree puzzle {}: spot ({}, {}) at ({:.3f}, {:.3f}), count {}",
			                    static_cast<int>(type), across, along, at.x, at.y,
			                    grid.counts.at(tree_puzzle::SpotIndex(across, along)));
		}
	}
	// a lantern one step out from each corner
	for (size_t corner = 0; corner < tree_puzzle::k_LanternSpots.size(); ++corner)
	{
		const auto& [across, along] = tree_puzzle::k_LanternSpots.at(corner);
		grid.lanterns.at(corner) = archetypes::StreetLanternArchetype::Create(
		    map_coords::ToWorld(SpotPlace(grid, across, along)), MobileStaticInfo::StreetLantern);
	}
	// a flock over the middle spot, the neutral player's, and over each spot a puff of mist and one bat of the flock
	const auto middle =
	    map_coords::ToMetres(SpotPlace(grid, tree_puzzle::k_MiddleSpot.front(), tree_puzzle::k_MiddleSpot.back()));
	auto* animals = Locator::animalSystem::has_value() ? &Locator::animalSystem::value() : nullptr;
	const auto flock = animals != nullptr ? animals->CreateScriptFlock(tree_puzzle::k_FlockNumber, middle, middle,
	                                                                   tree_puzzle::k_FlockReach, tree_puzzle::k_FlockDistance)
	                                      : entt::null;
	for (int32_t across = 0; across < tree_puzzle::k_Side; ++across)
	{
		for (int32_t along = 0; along < tree_puzzle::k_Side; ++along)
		{
			const auto spot = tree_puzzle::SpotIndex(across, along);
			const auto at = SpotPlace(grid, across, along);
			grid.mists.at(spot) =
			    archetypes::MistArchetype::Create(map_coords::ToWorld(at), at.altitude, tree_puzzle::k_MistColour,
			                                      tree_puzzle::k_MistSize, tree_puzzle::k_MistEdgeShrink);
			if (animals == nullptr)
			{
				continue;
			}
			const auto bat = animals->CreateBird(tree_puzzle::k_BatKind, middle, tree_puzzle::k_BatAge, flock);
			if (bat != entt::null)
			{
				animal_ai::SetState(bat, static_cast<animal_ai::AnimalState>(tree_puzzle::k_BatStartState));
			}
			grid.bats.at(spot) = bat;
		}
	}
}

void tree_puzzle_game::Process(entt::entity puzzle)
{
	auto* grid = GridOf(puzzle);
	if (grid == nullptr)
	{
		return;
	}
	const auto type = Locator::entitiesRegistry::value().Get<const PuzzleGame>(puzzle).type;
	for (int32_t across = 0; across < tree_puzzle::k_Side; ++across)
	{
		for (int32_t along = 0; along < tree_puzzle::k_Side; ++along)
		{
			const auto spot = tree_puzzle::SpotIndex(across, along);
			// a puff of mist or a bat that has gone is let go of
			for (auto* thing : {&grid->mists.at(spot), &grid->bats.at(spot)})
			{
				if (*thing != entt::null && !IsAvailable(*thing))
				{
					*thing = entt::null;
				}
			}
			const auto tree = grid->trees.at(spot);
			if (tree == entt::null)
			{
				continue;
			}
			if (OnItsSpot(*grid, spot, tree))
			{
				const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(tree);
				const auto* growing = Locator::entitiesRegistry::value().TryGet<const Tree>(tree);
				if (transform != nullptr && growing != nullptr && transform->scale.x < growing->maxSize)
				{
					static_cast<void>(GrowTree(tree, k_TurnGrowth, false));
				}
				continue;
			}
			// taken or moved off its spot: a pull of that spot, which plants it and its neighbours again
			grid->started = true;
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Tree puzzle {}: spot ({}, {}) pulled", static_cast<int>(type), across,
			                    along);
			Pull(*grid, type, across, along);
		}
	}
}

bool tree_puzzle_game::TestSolution(entt::entity puzzle)
{
	auto* grid = GridOf(puzzle);
	if (grid == nullptr)
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& game = registry.Get<PuzzleGame>(puzzle);
	for (const auto tree : grid->trees)
	{
		if (tree == entt::null || !registry.Valid(tree))
		{
			return false;
		}
		const auto* transform = registry.TryGet<const Transform>(tree);
		const auto* growing = registry.TryGet<const Tree>(tree);
		if (transform == nullptr || growing == nullptr || transform->scale.x < growing->maxSize)
		{
			return false;
		}
	}
	if (!tree_puzzle::SameState(game.type, grid->counts))
	{
		return false;
	}
	// solved: each tree keeps half its life and catches fire, and the puzzle lets go of it; the mist goes; the bats
	// start to die
	for (size_t spot = 0; spot < tree_puzzle::k_Spots; ++spot)
	{
		const auto tree = std::exchange(grid->trees.at(spot), entt::null);
		life::SetLife(tree, tree_puzzle::k_SolvedTreeLife);
		if (Locator::fireSystem::has_value() && registry.Valid(tree))
		{
			Locator::fireSystem::value().SetOnFire(tree, tree_puzzle::k_SolvedTreeFireSpeed);
		}
		if (const auto mist = std::exchange(grid->mists.at(spot), entt::null); mist != entt::null && registry.Valid(mist))
		{
			ToBeDeleted(mist);
		}
		if (const auto bat = std::exchange(grid->bats.at(spot), entt::null); bat != entt::null && registry.Valid(bat))
		{
			animal_ai::SetState(bat, static_cast<animal_ai::AnimalState>(tree_puzzle::k_BatSolvedState));
		}
	}
	game.played = true;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Tree puzzle {}: solved", static_cast<int>(game.type));
	return true;
}

bool tree_puzzle_game::Started(entt::entity puzzle)
{
	const auto* grid = GridOf(puzzle);
	return grid != nullptr && grid->started;
}
