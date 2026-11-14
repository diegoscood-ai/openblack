/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The lion maze (ecs::lion_maze_game, through ecs/PuzzleGames.h as the scripts reach it) on a flat dry land of the
// game's systems, its walls, bells and pieces the game's models or, on synthetic data, boxes: the grid as it is made,
// a bell rung, the sheep and a wall put back, the maze started again and the solution. See docs/wiki/world/lion-maze.md

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/AudioManagerNoOp.h"
#include "Audio/Game/Banks.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LionMaze.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/PuzzlePiece.h"
#include "ECS/Components/PuzzleTotem.h"
#include "ECS/Components/Transform.h"
#include "ECS/HanoiRules.h"
#include "ECS/LionMaze.h"
#include "ECS/LionMazeRules.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PuzzleGames.h"
#include "ECS/PuzzleTotem.h"
#include "ECS/ScriptHeld.h"
#include "ECS/ToBeDeleted.h"
#include "Locator.h"
#include "support/ParticleFakes.h"
#include "support/WorldHarness.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using lion_maze::Square;

namespace
{
/// The puzzle's place, the middle of a map cell well inside the map
constexpr glm::vec3 k_Place {1005.0f, 0.0f, 1235.0f};

/// The spot visuals started, each an object of the registry so that it can be deleted
class RecordedSparkles final: public test::InertParticleSystem
{
public:
	struct Started
	{
		SpotVisualType type;
		glm::vec3 position;
		std::optional<int> turns;
		float magnitude;
	};

	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> turns, entt::entity /*owner*/,
	                             float magnitude) override
	{
		started.push_back({type, position, turns, magnitude});
		return Locator::entitiesRegistry::value().Create();
	}

	std::vector<Started> started;
};

/// The samples played
class RecordedSamples final: public audio::AudioManagerNoOp
{
public:
	using AudioManagerNoOp::PlaySoundEffect;
	audio::Channel PlaySoundEffect(const audio::PlayOptions& options) override
	{
		played.push_back(options);
		return audio::k_NoChannel;
	}
	std::vector<audio::PlayOptions> played;
};

class LionMazeWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		if (!GameData())
		{
			// the bell's row: a totem of no tribe; the rows' models are boxes
			auto& bell = Info().abode.at(static_cast<size_t>(AbodeInfo::Belltower));
			bell.abodeType = AbodeType::Totem;
			bell.abodeNumber = AbodeNumber::Totem;
		}
		// on synthetic data every model is a box a metre across and two high
		for (const auto mesh : {Info().abode.at(static_cast<size_t>(AbodeInfo::Belltower)).meshId,
		                        Info().mobileStatic.at(static_cast<size_t>(MobileStaticInfo::CeltFenceShort)).meshId,
		                        Info().mobileStatic.at(static_cast<size_t>(MobileStaticInfo::StreetLantern)).meshId})
		{
			LoadMesh(mesh, glm::vec3(-0.5f, 0.0f, -0.5f), glm::vec3(0.5f, 2.0f, 0.5f));
		}
		// the palms at the corners are trees, each of which needs its model in the cache (on synthetic data the same box)
		LoadMesh(Info().tree.at(static_cast<size_t>(TreeInfo::Palm)).normal, glm::vec3(-0.5f, 0.0f, -0.5f),
		         glm::vec3(0.5f, 2.0f, 0.5f));
		_sparkles = &static_cast<RecordedSparkles&>(Locator::particleSystem::emplace<RecordedSparkles>());
		_samples = &static_cast<RecordedSamples&>(Locator::audio::emplace<RecordedSamples>());
		_puzzle = CreatePuzzleGame(k_Place, script::PuzzleGameType::Tilt1, 0.0f, 1.0f);
	}

	void TearDown() override
	{
		Locator::audio::reset();
		Locator::particleSystem::reset();
		WorldHarness::TearDown();
	}

	[[nodiscard]] LionMaze& Maze() const { return Registry().Get<LionMaze>(_puzzle); }
	[[nodiscard]] PuzzleTotem& Bell(size_t index) const { return Registry().Get<PuzzleTotem>(Maze().bells.at(index)); }
	/// A bell rung: the height the hand's tap gives a tap-only totem (docs/wiki/magic/puzzle-totem.md)
	void Ring(size_t index) const { Bell(index).height = puzzle_totem::k_TapOnly; }
	[[nodiscard]] map_coords::MapCoords SquarePlace(Square square) const
	{
		return hanoi::GridPlace(Maze().place, {.cosine = Maze().cosineStep, .sine = Maze().sineStep},
		                        lion_maze::SquareSpot(square).across, lion_maze::SquareSpot(square).along);
	}

	entt::entity _puzzle {entt::null};
	RecordedSparkles* _sparkles {nullptr};
	RecordedSamples* _samples {nullptr};
};
} // namespace

// The grid as it is made: a wall wherever the map gives one, the four palms and lanterns, the wolf and the sheep on
// their squares (pieces twice their size, never picked up, unbreakable, out of the physics) and five bells only tapped
TEST_P(LionMazeWorld, TheGridIsMadeWithItsWallsBellsAndPieces)
{
	ASSERT_TRUE(_puzzle != entt::null);
	const auto& maze = Maze();
	EXPECT_EQ(maze.cosineStep, 65536 / 5);
	EXPECT_EQ(maze.sineStep, 0);
	size_t walls = 0;
	for (int32_t x = 0; x <= lion_maze::k_Squares; ++x)
	{
		for (int32_t y = 0; y <= lion_maze::k_Squares; ++y)
		{
			for (const auto wall : {lion_maze::Wall::AlongX, lion_maze::Wall::AlongY})
			{
				const auto& grid = wall == lion_maze::Wall::AlongX ? maze.wallsAlongX : maze.wallsAlongY;
				const auto entity = grid.at(static_cast<size_t>(x)).at(static_cast<size_t>(y));
				EXPECT_EQ(entity != entt::null, lion_maze::HasNearWall(x, y, wall)) << x << "," << y;
				if (entity != entt::null)
				{
					++walls;
					EXPECT_TRUE(object_flags::IsCannotBePickedUp(entity));
					EXPECT_TRUE(Registry().AllOf<Indestructible>(entity));
				}
			}
		}
	}
	EXPECT_GT(walls, 28U);
	for (const auto palm : maze.palms)
	{
		EXPECT_TRUE(Registry().Valid(palm));
	}
	for (const auto piece : {maze.wolf, maze.sheep})
	{
		ASSERT_TRUE(Registry().Valid(piece));
		EXPECT_TRUE(Registry().AllOf<PuzzlePiece>(piece));
		EXPECT_TRUE(object_flags::IsCannotBePickedUp(piece));
		EXPECT_TRUE(Registry().AllOf<Indestructible>(piece));
		EXPECT_EQ(Registry().Get<const Transform>(piece).scale.x, lion_maze::k_PieceScale);
		EXPECT_FALSE(Dynamics().CanBecomeAPhysicsObject(piece));
	}
	EXPECT_EQ(maze.wolfSquare, lion_maze::k_WolfStart);
	EXPECT_EQ(maze.sheepSquare, lion_maze::k_SheepStart);
	EXPECT_EQ(Registry().Get<const Transform>(maze.sheep).position.x,
	          map_coords::ToMetres(SquarePlace(lion_maze::k_SheepStart).x));
	for (size_t index = 0; index < lion_maze::k_Bells; ++index)
	{
		EXPECT_EQ(Bell(index).maxHeight, puzzle_totem::k_TapOnly);
		EXPECT_EQ(Bell(index).height, 0);
		EXPECT_TRUE(object_flags::IsImmovable(maze.bells.at(index)));
		EXPECT_TRUE(script_held::IsControlledByScript(maze.bells.at(index)));
	}
	// a sparkle of ten turns over the sheep and over the wolf
	ASSERT_EQ(_sparkles->started.size(), 2U);
	EXPECT_EQ(_sparkles->started.front().turns, lion_maze::k_StartSparkleTurns);
	EXPECT_EQ(_sparkles->started.front().magnitude, lion_maze::k_SparkleMagnitude);
	EXPECT_EQ(PuzzleGameStatusOf(_puzzle), script::PuzzleGameStatus::GameNotYetBegun);
}

// A bell rung sends the wolf sliding its way: the wolf's square and way are set, the bell sparkles and rings, every bell
// is unrung, and the puzzle has begun. A bell the way a wall stands moves nothing
TEST_P(LionMazeWorld, ARungBellSendsTheWolfSlidingItsWay)
{
	Ring(static_cast<size_t>(lion_maze::Way::MinusY));
	EXPECT_EQ(Bell(static_cast<size_t>(lion_maze::Way::MinusY)).height, puzzle_totem::k_TapOnly);
	ProcessPuzzleGame(_puzzle);
	EXPECT_EQ(Maze().wolfSquare, (Square {.x = 1, .y = 1}));
	EXPECT_EQ(Maze().wolfAngle, 0x600);
	EXPECT_EQ(Bell(static_cast<size_t>(lion_maze::Way::MinusY)).height, 0);
	EXPECT_EQ(PuzzleGameStatusOf(_puzzle), script::PuzzleGameStatus::GameInProgress);
	ASSERT_EQ(_sparkles->started.size(), 3U);
	EXPECT_EQ(_sparkles->started.back().turns, lion_maze::k_PressSparkleTurns);
	bool rang = false;
	for (const auto& options : _samples->played)
	{
		if (options.sample.bank == audio::Bank(audio::SfxBank::ScriptSfx) && options.sample.number == lion_maze::k_BellSample)
		{
			rang = true;
			EXPECT_TRUE(options.is3D);
			EXPECT_FALSE(options.track);
		}
	}
	EXPECT_TRUE(rang);
	// from (1, 1) a wall stands the way of that bell: the wolf keeps its square
	Maze().walk = lion_maze::Walk::There;
	animal_ai::SetStateRaw(Maze().wolf, animal_ai::AnimalState::DecideWhatToDo);
	Ring(static_cast<size_t>(lion_maze::Way::MinusY));
	ProcessPuzzleGame(_puzzle);
	EXPECT_EQ(Maze().wolfSquare, (Square {.x = 1, .y = 1}));
	EXPECT_EQ(Maze().walk, lion_maze::Walk::There);
}

// The sheep taken off its square is put back on it, and a wall moved off its place melts away and is made again
TEST_P(LionMazeWorld, TheSheepAndAWallArePutBack)
{
	const auto sheepAt = object::MapCoordsOf(Maze().sheep);
	map_cells::MoveMapObject(Maze().sheep,
	                         Registry().Get<const Transform>(Maze().sheep).position + glm::vec3(3.0f, 0.0f, 0.0f));
	const auto wall = Maze().wallsAlongX.at(0).at(3);
	ASSERT_TRUE(wall != entt::null);
	map_cells::MoveMapObject(wall, Registry().Get<const Transform>(wall).position + glm::vec3(0.0f, 0.0f, 2.0f));
	ProcessPuzzleGame(_puzzle);
	EXPECT_EQ(object::MapCoordsOf(Maze().sheep), sheepAt);
	EXPECT_FALSE(IsAvailable(wall));
	const auto remade = Maze().wallsAlongX.at(0).at(3);
	ASSERT_TRUE(IsAvailable(remade));
	EXPECT_EQ(object::MapCoordsOf(remade), Maze().wallsAlongXAt.at(0).at(3));
	EXPECT_TRUE(object_flags::IsCannotBePickedUp(remade));
}

// The corner bell starts the maze again: the pieces go and are made again on their squares
TEST_P(LionMazeWorld, TheCornerBellStartsTheMazeAgain)
{
	Ring(static_cast<size_t>(lion_maze::Way::PlusX));
	ProcessPuzzleGame(_puzzle);
	EXPECT_EQ(Maze().wolfSquare, (Square {.x = 5, .y = 3}));
	const auto wolf = Maze().wolf;
	const auto sheep = Maze().sheep;
	Maze().walk = lion_maze::Walk::There;
	animal_ai::SetStateRaw(wolf, animal_ai::AnimalState::DecideWhatToDo);
	Ring(lion_maze::k_ResetBell);
	ProcessPuzzleGame(_puzzle);
	EXPECT_FALSE(IsAvailable(wolf));
	EXPECT_FALSE(IsAvailable(sheep));
	EXPECT_TRUE(IsAvailable(Maze().wolf));
	EXPECT_TRUE(IsAvailable(Maze().sheep));
	EXPECT_EQ(Maze().wolfSquare, lion_maze::k_WolfStart);
	EXPECT_EQ(Bell(lion_maze::k_ResetBell).height, 0);
}

// The wolf walked to the sheep's square is the solution: a sparkle of three turns over the sheep, both go, the puzzle is
// played, and a later test says no, as the original's
TEST_P(LionMazeWorld, TheWolfOnTheSheepsSquareIsTheSolution)
{
	auto& maze = Maze();
	// the wolf sent to the sheep's square, its walk done there
	maze.wolfSquare = maze.sheepSquare;
	maze.walk = lion_maze::Walk::There;
	const auto to = map_coords::ToMetres(SquarePlace(maze.sheepSquare));
	animal_ai::MoveTo(maze.wolf, to, 0.0f, animal_ai::AnimalState::DecideWhatToDo);
	animal_ai::SetSpeedInMetresPerSecond(maze.wolf, lion_maze::k_WalkMetresPerSecond);
	// still walking: no
	EXPECT_FALSE(lion_maze_game::TestSolution(_puzzle));
	animal_ai::SetStateRaw(maze.wolf, animal_ai::AnimalState::DecideWhatToDo);
	map_cells::MoveMapObject(maze.wolf, map_coords::ToWorld(SquarePlace(maze.sheepSquare)));
	const auto wolf = maze.wolf;
	const auto sheep = maze.sheep;
	EXPECT_TRUE(IsPuzzleGamePlayed(_puzzle));
	EXPECT_FALSE(IsAvailable(wolf));
	EXPECT_FALSE(IsAvailable(sheep));
	EXPECT_TRUE(Registry().Get<const PuzzleGame>(_puzzle).played);
	EXPECT_EQ(_sparkles->started.back().turns, lion_maze::k_PressSparkleTurns);
	EXPECT_FALSE(lion_maze_game::TestSolution(_puzzle));
}

namespace
{
/// A Theseus maze's place, a grid east of the lion maze's
constexpr glm::vec3 k_TheseusPlace {1105.0f, 0.0f, 1235.0f};
} // namespace

// A Theseus maze is made with its walls, palms and lanterns, its villager (at his age's size) and the wolf on their
// squares, and no bells
TEST_P(LionMazeWorld, ATheseusMazeIsMadeWithItsVillagerAndWolf)
{
	const auto theseus = CreatePuzzleGame(k_TheseusPlace, script::PuzzleGameType::Thesius1, 0.0f, 1.0f);
	const auto& maze = Registry().Get<const LionMaze>(theseus);
	EXPECT_EQ(maze.kind.squares, lion_maze::k_TheseusOne.squares);
	ASSERT_TRUE(Registry().Valid(maze.villager));
	EXPECT_TRUE(Registry().AllOf<PuzzlePiece>(maze.villager));
	EXPECT_TRUE(maze.sheep == entt::null);
	EXPECT_EQ(maze.villagerSquare, lion_maze::k_TheseusOne.preyStart);
	EXPECT_EQ(maze.wolfSquare, lion_maze::k_TheseusOne.wolfStart);
	for (const auto bell : maze.bells)
	{
		EXPECT_TRUE(bell == entt::null);
	}
	EXPECT_EQ(maze.wallsAlongX.at(0).at(3) != entt::null, lion_maze::HasNearWall(0, 3, lion_maze::Wall::AlongX, maze.kind));
}

// Once both pieces stand on their squares, a click on the grid in the villager's row moves him a square that way, the
// wolf chases him, a marker shows the clicked square and the maze has begun
TEST_P(LionMazeWorld, AClickMovesTheVillagerAndTheWolfChasesHim)
{
	const auto theseus = CreatePuzzleGame(k_TheseusPlace, script::PuzzleGameType::Thesius1, 0.0f, 1.0f);
	auto& maze = Registry().Get<LionMaze>(theseus);
	maze.walk = lion_maze::Walk::There;
	maze.villagerWalk = lion_maze::Walk::There;
	const auto clicked = hanoi::GridPlace(maze.place, {.cosine = maze.cosineStep, .sine = maze.sineStep}, 11, 7);
	SetPuzzleAppliedMapPos(clicked);
	ProcessPuzzleGame(theseus);
	EXPECT_EQ(maze.villagerSquare, (Square {.x = 3, .y = 3}));
	EXPECT_EQ(maze.villagerAngle, 0);
	EXPECT_EQ(maze.wolfSquare, (Square {.x = 3, .y = 5}));
	EXPECT_EQ(maze.wolfAngle, 0);
	EXPECT_TRUE(maze.marker != entt::null);
	EXPECT_EQ(PuzzleGameStatusOf(theseus), script::PuzzleGameStatus::GameInProgress);
	// the turn's click goes with the turn
	ProcessPuzzleGamesTurn();
	EXPECT_EQ(map_coords::CellX(PuzzleAppliedMapPos()), 0);
	EXPECT_EQ(map_coords::CellZ(PuzzleAppliedMapPos()), 0);
}

// The wolf on the villager's square starts the maze again; the villager past the grid is the solution: a sparkle over
// him, both go, and the test still says yes
TEST_P(LionMazeWorld, TheWolfCatchesTheVillagerOrHeGetsOut)
{
	const auto theseus = CreatePuzzleGame(k_TheseusPlace, script::PuzzleGameType::Thesius1, 0.0f, 1.0f);
	auto& maze = Registry().Get<LionMaze>(theseus);
	maze.walk = lion_maze::Walk::There;
	maze.villagerWalk = lion_maze::Walk::There;
	const auto villager = maze.villager;
	map_cells::MoveMapObject(maze.wolf, Registry().Get<const Transform>(villager).position);
	ProcessPuzzleGame(theseus);
	EXPECT_FALSE(IsAvailable(villager));
	ASSERT_TRUE(IsAvailable(maze.villager));
	EXPECT_EQ(maze.villagerSquare, lion_maze::k_TheseusOne.preyStart);
	// out of the grid
	maze.villagerSquare = {.x = lion_maze::k_TheseusOne.squares, .y = 0};
	const auto out = maze.villager;
	const auto wolf = maze.wolf;
	EXPECT_TRUE(IsPuzzleGamePlayed(theseus));
	EXPECT_FALSE(IsAvailable(out));
	EXPECT_FALSE(IsAvailable(wolf));
	EXPECT_EQ(_sparkles->started.back().turns, lion_maze::k_PressSparkleTurns);
	EXPECT_TRUE(lion_maze_game::TestSolution(theseus));
}

INSTANTIATE_TEST_SUITE_P(Data, LionMazeWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
