/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LionMaze.h"

#include <cstddef>
#include <cstdint>

#include <optional>
#include <utility>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "Audio/Audio.h"
#include "Audio/Game/BankTables.h"
#include "Audio/Game/Banks.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PuzzleTotemArchetype.h"
#include "ECS/Archetypes/StreetLanternArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LionMaze.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/PuzzleTotem.h"
#include "ECS/Components/Transform.h"
#include "ECS/HanoiRules.h"
#include "ECS/LionMazeRules.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectGhosts.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PuzzleGames.h"
#include "ECS/PuzzleTotem.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "Enums.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using lion_maze::GridSpot;
using lion_maze::Square;
using lion_maze::Wall;

namespace
{
/// The half squares a puzzle's steps are divided into
constexpr int32_t k_HalfSquaresPerStep = 5;
/// The size a bell made again takes
constexpr float k_RemadeBellScale = 1.0f;

/// The squares and map of a maze type: the Theseus mazes of types 10 and 11, else the lion maze
const lion_maze::Kind& KindOf(script::PuzzleGameType type)
{
	switch (type)
	{
	case script::PuzzleGameType::Thesius1:
		return lion_maze::k_TheseusOne;
	case script::PuzzleGameType::Thesius2:
		return lion_maze::k_TheseusTwo;
	default:
		return lion_maze::k_LionMaze;
	}
}

bool IsTheseus(const LionMaze& maze)
{
	return maze.kind.squares != lion_maze::k_LionMaze.squares || maze.kind.map != lion_maze::k_LionMaze.map;
}

LionMaze* MazeOf(entt::entity puzzle)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(puzzle) ? registry.TryGet<LionMaze>(puzzle) : nullptr;
}

map_coords::MapCoords PlaceOf(const LionMaze& maze, GridSpot spot)
{
	return hanoi::GridPlace(maze.place, {.cosine = maze.cosineStep, .sine = maze.sineStep}, spot.across, spot.along);
}

/// The point of a thing as its MapCoords give it in the world
glm::vec3 PointOf(entt::entity thing)
{
	return Locator::entitiesRegistry::value().Get<const Transform>(thing).position;
}

/// A sparkle over a point, for some turns: the creature's target visual at the maze's magnitude
void Sparkle(glm::vec3 at, int32_t turns)
{
	if (Locator::particleSystem::has_value())
	{
		static_cast<void>(Locator::particleSystem::value().StartSpotVisual(SpotVisualType::CreatureTarget, at, turns,
		                                                                   entt::null, lion_maze::k_SparkleMagnitude));
	}
}

/// A wall of fencing, unbreakable, at its place on the near side of square (x, y)
entt::entity MakeWall(LionMaze& maze, int32_t x, int32_t y, Wall wall)
{
	const auto at = PlaceOf(maze, lion_maze::WallSpot(x, y, wall));
	const auto entity =
	    archetypes::MobileStaticArchetype::CreateWithXYZAngles(map_coords::ToWorld(at), MobileStaticInfo::CeltFenceShort, 0.0f,
	                                                           0.0f, lion_maze::WallAngle(maze.angle, wall), 0.0f, maze.scale);
	if (entity != entt::null)
	{
		Locator::entitiesRegistry::value().AssignOrReplaceState<Indestructible>(entity);
		auto& placedAt = wall == Wall::AlongX ? maze.wallsAlongXAt : maze.wallsAlongYAt;
		placedAt.at(static_cast<size_t>(x)).at(static_cast<size_t>(y)) = object::MapCoordsOf(entity);
	}
	return entity;
}

entt::entity& WallSlot(LionMaze& maze, int32_t x, int32_t y, Wall wall)
{
	auto& walls = wall == Wall::AlongX ? maze.wallsAlongX : maze.wallsAlongY;
	return walls.at(static_cast<size_t>(x)).at(static_cast<size_t>(y));
}

/// A wall kept up: never hot; one that went is made again, and one moved off its place melts away and is made again,
/// never to be picked up
void KeepWall(LionMaze& maze, int32_t x, int32_t y, Wall wall)
{
	auto& slot = WallSlot(maze, x, y, wall);
	if (slot == entt::null)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (Locator::fireSystem::has_value() && registry.Valid(slot))
	{
		Locator::fireSystem::value().SetTemperature(slot, 0.0f, entt::null);
	}
	if (!IsAvailable(slot))
	{
		slot = MakeWall(maze, x, y, wall);
		return;
	}
	const auto& placedAt =
	    (wall == Wall::AlongX ? maze.wallsAlongXAt : maze.wallsAlongYAt).at(static_cast<size_t>(x)).at(static_cast<size_t>(y));
	if (object::MapCoordsOf(slot) != placedAt)
	{
		object_ghosts::Add(slot);
		ToBeDeleted(slot);
		slot = MakeWall(maze, x, y, wall);
		if (slot != entt::null)
		{
			object_flags::SetPickupable(slot, false);
		}
	}
}

/// A piece of the maze on its square: of the row, at the piece's age and the size given (none: its age's), never picked
/// up
entt::entity MakePiece(const LionMaze& maze, AnimalInfo type, Square square, std::optional<float> scale)
{
	const auto piece = archetypes::AnimalArchetype::ConstructPiece(
	    map_coords::ToWorld(PlaceOf(maze, lion_maze::SquareSpot(square))), type, lion_maze::k_PieceAge, scale);
	if (piece != entt::null)
	{
		object_flags::SetPickupable(piece, false);
	}
	return piece;
}

/// The creation's end of a piece: into the map, then unbreakable
void FinishPiece(entt::entity piece)
{
	if (piece == entt::null)
	{
		return;
	}
	archetypes::AnimalArchetype::FinishCreation(piece);
	Locator::entitiesRegistry::value().AssignOrReplaceState<Indestructible>(piece);
}

/// A bell that went is made again where it stood, on the land, at no angle and at a size of 1, with its highest step and
/// height (once it is destroyed, the bell's own: only tapped and unrung), held by the scripts
void RemakeBell(LionMaze& maze, size_t index)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto old = maze.bells.at(index);
	if (old == entt::null || IsAvailable(old))
	{
		return;
	}
	const auto* piece = registry.Valid(old) ? registry.TryGet<const PuzzleTotem>(old) : nullptr;
	const auto& stood = maze.bellPlaces.at(index);
	const auto maxHeight = piece != nullptr ? piece->maxHeight : puzzle_totem::k_TapOnly;
	const auto height = piece != nullptr ? piece->height : 0;
	const auto bell = archetypes::PuzzleTotemArchetype::Create(
	    map_coords::ToWorld(map_coords::MapCoords {.x = stood.x, .z = stood.z, .altitude = 0.0f}), AbodeInfo::Belltower, 0.0f,
	    k_RemadeBellScale);
	maze.bells.at(index) = bell;
	if (bell == entt::null)
	{
		return;
	}
	auto& made = registry.Get<PuzzleTotem>(bell);
	made.maxHeight = maxHeight;
	made.height = height;
	object_flags::SetMoveable(bell, false);
	script_held::SetControlledByScript(bell, true);
}

/// A bell rung: a sparkle over it and its sound, placed there and not following it
void RingBell(LionMaze& maze, size_t index)
{
	const auto bell = maze.bells.at(index);
	const auto at = PointOf(bell);
	Sparkle(at, lion_maze::k_PressSparkleTurns);
	if (!Locator::audio::has_value())
	{
		return;
	}
	auto& owner = maze.bellSoundOwners.at(index);
	if (!owner.has_value())
	{
		owner = audio::NewObjectId();
	}
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::ScriptSfx), lion_maze::k_BellSample};
	options.owner = audio::Owner::Object(*owner);
	options.is3D = true;
	options.track = false;
	options.position = at;
	static_cast<void>(audio::PlaySoundEffect(options));
}

/// A bell's totem; none for a bell that is not there
PuzzleTotem* BellOf(entt::entity bell)
{
	auto& registry = Locator::entitiesRegistry::value();
	return bell != entt::null && registry.Valid(bell) ? registry.TryGet<PuzzleTotem>(bell) : nullptr;
}

bool Available(entt::entity thing)
{
	return thing != entt::null && IsAvailable(thing);
}

bool Walks(entt::entity piece)
{
	return animal_ai::TopState(piece) == animal_ai::AnimalState::MoveToPos;
}
} // namespace

namespace
{
/// A piece's walk to its square once it faces its way: at 4 m/s, then there
void Walk(const LionMaze& maze, entt::entity piece, Square square, uint16_t angle, lion_maze::Walk& walk)
{
	animal_ai::TurnTowards(piece, angle);
	if (angle != animal_ai::GameAngle(piece))
	{
		return;
	}
	if (walk == lion_maze::Walk::Turning)
	{
		animal_ai::SetSpeedInMetresPerSecond(piece, lion_maze::k_WalkMetresPerSecond);
		const auto to = map_coords::ToMetres(PlaceOf(maze, lion_maze::SquareSpot(square)));
		animal_ai::MoveTo(piece, to, 0.0f, animal_ai::AnimalState::DecideWhatToDo);
		walk = lion_maze::Walk::Walking;
	}
	else if (walk == lion_maze::Walk::Walking && animal_ai::AreWeThere(piece, 0.0f))
	{
		walk = lion_maze::Walk::There;
	}
}

/// A Theseus maze's turn: the walls kept up; either piece gone starts it again; once both have walked to their squares
/// the marker goes, the wolf on the villager starts it again, and the turn's click on the grid moves the villager a
/// square its way (or lets him wait) and the wolf up to two squares after him; then both walk
void ProcessTheseus(entt::entity puzzle, LionMaze& maze)
{
	if (maze.marker != entt::null && !IsAvailable(maze.marker))
	{
		maze.marker = entt::null;
	}
	for (int32_t x = 0; x < static_cast<int32_t>(LionMaze::k_WallLines); ++x)
	{
		for (int32_t y = 0; y < static_cast<int32_t>(LionMaze::k_WallLines); ++y)
		{
			KeepWall(maze, x, y, Wall::AlongX);
			KeepWall(maze, x, y, Wall::AlongY);
		}
	}
	if ((maze.villager != entt::null && !IsAvailable(maze.villager)) || (maze.wolf != entt::null && !IsAvailable(maze.wolf)))
	{
		lion_maze_game::Reset(puzzle);
		return;
	}
	if (!Walks(maze.villager) && !Walks(maze.wolf) && maze.walk == lion_maze::Walk::There &&
	    maze.villagerWalk == lion_maze::Walk::There)
	{
		if (const auto marker = std::exchange(maze.marker, entt::null); marker != entt::null)
		{
			ToBeDeleted(marker);
		}
		if (object::MapCoordsOf(maze.villager) == object::MapCoordsOf(maze.wolf))
		{
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Theseus maze: the wolf caught the villager, the maze starts again");
			lion_maze_game::Reset(puzzle);
			return;
		}
		// the turn's click on the land, none when its cell is the map's first
		const auto& click = PuzzleAppliedMapPos();
		if (map_coords::CellX(click) == 0 && map_coords::CellZ(click) == 0)
		{
			return;
		}
		const auto far = PlaceOf(maze, {.across = maze.kind.squares * 2 + 2, .along = maze.kind.squares * 2 + 2});
		const auto spot =
		    lion_maze::ClickedSpot(maze.place.x, maze.place.z, far.x, far.z, {.cosine = maze.cosineStep, .sine = maze.sineStep},
		                           maze.scale, click.x, click.z);
		if (!spot.has_value())
		{
			return;
		}
		if (Locator::particleSystem::has_value())
		{
			maze.marker = Locator::particleSystem::value().StartSpotVisual(
			    SpotVisualType::CreatureTarget, map_coords::ToWorld(PlaceOf(maze, *spot)), std::nullopt, entt::null,
			    lion_maze::k_MarkerMagnitude);
		}
		const auto step = lion_maze::ClickedStep(maze.villagerSquare, *spot, maze.kind);
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Theseus maze: a click on half square ({}, {}), step {}", spot->across,
		                    spot->along, step);
		if (step == lion_maze::k_StepNone)
		{
			return;
		}
		if (step != lion_maze::k_StepWait)
		{
			const auto way = static_cast<lion_maze::Way>(step);
			switch (way)
			{
			case lion_maze::Way::PlusY:
				++maze.villagerSquare.y;
				break;
			case lion_maze::Way::PlusX:
				++maze.villagerSquare.x;
				break;
			case lion_maze::Way::MinusY:
				--maze.villagerSquare.y;
				break;
			case lion_maze::Way::MinusX:
				--maze.villagerSquare.x;
				break;
			}
			maze.villagerAngle = lion_maze::StepAngle(way);
			maze.villagerWalk = lion_maze::Walk::Turning;
		}
		maze.started = true;
		const auto chase = lion_maze::ChaseOf(maze.wolfSquare, maze.villagerSquare, maze.kind);
		maze.wolfSquare = chase.wolf;
		if (const auto angle = lion_maze::ChaseAngle(chase.ways); angle.has_value())
		{
			maze.wolfAngle = *angle;
			maze.walk = lion_maze::Walk::Turning;
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Theseus maze: the villager to ({}, {}), the wolf to ({}, {}), ways {}",
		                    maze.villagerSquare.x, maze.villagerSquare.y, maze.wolfSquare.x, maze.wolfSquare.y, chase.ways);
	}
	Walk(maze, maze.wolf, maze.wolfSquare, maze.wolfAngle, maze.walk);
	Walk(maze, maze.villager, maze.villagerSquare, maze.villagerAngle, maze.villagerWalk);
}

/// A Theseus maze's solution: the villager on the row or column past the grid. A sparkle of three turns over him, the
/// villager and the wolf go, and the marker. Nothing tests it again in play once it is solved
bool TestTheseusSolution(entt::entity puzzle, LionMaze& maze)
{
	if (!lion_maze::Escaped(maze.villagerSquare, maze.kind))
	{
		return false;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Theseus maze: the villager is out at ({}, {})", maze.villagerSquare.x,
	                    maze.villagerSquare.y);
	if (maze.villager != entt::null)
	{
		Sparkle(PointOf(maze.villager), lion_maze::k_PressSparkleTurns);
		ToBeDeleted(std::exchange(maze.villager, entt::null));
	}
	if (const auto wolf = std::exchange(maze.wolf, entt::null); wolf != entt::null)
	{
		ToBeDeleted(wolf);
	}
	if (const auto marker = std::exchange(maze.marker, entt::null); marker != entt::null && IsAvailable(marker))
	{
		ToBeDeleted(marker);
	}
	Locator::entitiesRegistry::value().Get<PuzzleGame>(puzzle).played = true;
	return true;
}
} // namespace

void lion_maze_game::Start(entt::entity puzzle, const map_coords::MapCoords& place, float yAngleRadians, float scale,
                           script::PuzzleGameType type)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& maze = registry.Assign<LionMaze>(puzzle);
	maze.kind = KindOf(type);
	const auto angle = static_cast<uint16_t>(hanoi::GameAngleOf(yAngleRadians));
	const auto steps = hanoi::StepsOf(angle, scale);
	maze.place = place;
	maze.angle = angle;
	maze.scale = scale;
	// the steps of a half square
	maze.cosineStep = steps.cosine / k_HalfSquaresPerStep;
	maze.sineStep = steps.sine / k_HalfSquaresPerStep;
	// the walls, square by square, the one across x first
	for (int32_t x = 0; x <= maze.kind.squares; ++x)
	{
		for (int32_t y = 0; y <= maze.kind.squares; ++y)
		{
			for (const auto wall : {Wall::AlongX, Wall::AlongY})
			{
				if (!lion_maze::HasNearWall(x, y, wall, maze.kind))
				{
					continue;
				}
				auto& slot = WallSlot(maze, x, y, wall);
				slot = MakeWall(maze, x, y, wall);
				if (slot != entt::null)
				{
					object_flags::SetPickupable(slot, false);
				}
			}
		}
	}
	// the palms, held by the scripts, and the lanterns
	for (size_t corner = 0; corner < maze.palms.size(); ++corner)
	{
		const auto at = map_coords::ToWorld(PlaceOf(maze, lion_maze::PalmSpots(maze.kind).at(corner)));
		maze.palms.at(corner) = archetypes::TreeArchetype::Create(0, at, TreeInfo::Palm, false, 0.0f, 1.0f, 1.0f);
	}
	for (const auto palm : maze.palms)
	{
		if (palm != entt::null)
		{
			script_held::SetControlledByScript(palm, true);
		}
	}
	for (size_t side = 0; side < maze.lanterns.size(); ++side)
	{
		maze.lanterns.at(side) = archetypes::StreetLanternArchetype::Create(
		    map_coords::ToWorld(PlaceOf(maze, lion_maze::LanternSpots(maze.kind).at(side))), MobileStaticInfo::StreetLantern);
	}
	Reset(puzzle);
	// the lion maze's bells, only tapped, at no height, unmovable and held by the scripts
	if (IsTheseus(maze))
	{
		return;
	}
	for (size_t index = 0; index < lion_maze::k_Bells; ++index)
	{
		const auto& bell = lion_maze::k_BellSpots.at(index);
		maze.bellPlaces.at(index) = PlaceOf(maze, bell.spot);
		maze.bells.at(index) = archetypes::PuzzleTotemArchetype::Create(
		    map_coords::ToWorld(maze.bellPlaces.at(index)), AbodeInfo::Belltower, bell.yAngle, lion_maze::k_BellScale);
	}
	for (const auto bell : maze.bells)
	{
		if (bell != entt::null)
		{
			auto& piece = registry.Get<PuzzleTotem>(bell);
			piece.maxHeight = puzzle_totem::k_TapOnly;
			piece.height = 0;
		}
	}
	for (const auto bell : maze.bells)
	{
		if (bell != entt::null)
		{
			object_flags::SetMoveable(bell, false);
			script_held::SetControlledByScript(bell, true);
		}
	}
}

void lion_maze_game::Reset(entt::entity puzzle)
{
	auto* maze = MazeOf(puzzle);
	if (maze == nullptr)
	{
		return;
	}
	// the sheep, the villager, then the wolf, melt away
	for (const auto piece : {maze->sheep, maze->villager, maze->wolf})
	{
		if (Available(piece))
		{
			object_ghosts::Add(piece);
			ToBeDeleted(piece);
		}
	}
	maze->wolf = entt::null;
	maze->sheep = entt::null;
	maze->villager = entt::null;
	maze->wolfSquare = maze->kind.wolfStart;
	// made again: a Theseus maze's villager (at his age's size), else the sheep, then the wolf; then the villager, the
	// wolf and the sheep into the map in that order
	if (IsTheseus(*maze))
	{
		maze->villagerSquare = maze->kind.preyStart;
		maze->villager = MakePiece(*maze, AnimalInfo::PuzzleVillager, maze->villagerSquare, std::nullopt);
	}
	else
	{
		maze->sheepSquare = maze->kind.preyStart;
		maze->sheep = MakePiece(*maze, AnimalInfo::PuzzleSheep, maze->sheepSquare, lion_maze::k_PieceScale);
	}
	maze->wolf = MakePiece(*maze, AnimalInfo::PuzzleWolf, maze->wolfSquare, lion_maze::k_PieceScale);
	FinishPiece(maze->villager);
	FinishPiece(maze->wolf);
	FinishPiece(maze->sheep);
	if (maze->sheep != entt::null)
	{
		maze->sheepPlacedAt = object::MapCoordsOf(maze->sheep);
	}
	// a sparkle of ten turns over the villager or the sheep, then over the wolf
	for (const auto piece : {IsTheseus(*maze) ? maze->villager : maze->sheep, maze->wolf})
	{
		if (piece != entt::null)
		{
			Sparkle(PointOf(piece), lion_maze::k_StartSparkleTurns);
		}
	}
}

void lion_maze_game::Process(entt::entity puzzle)
{
	auto* maze = MazeOf(puzzle);
	if (maze == nullptr)
	{
		return;
	}
	// a palm or a lantern that went is let go of
	for (auto* things : {&maze->palms, &maze->lanterns})
	{
		for (auto& thing : *things)
		{
			if (thing != entt::null && !IsAvailable(thing))
			{
				thing = entt::null;
			}
		}
	}
	if (IsTheseus(*maze))
	{
		ProcessTheseus(puzzle, *maze);
		return;
	}
	// the walls, square by square, the one across x first
	for (int32_t x = 0; x < static_cast<int32_t>(LionMaze::k_WallLines); ++x)
	{
		for (int32_t y = 0; y < static_cast<int32_t>(LionMaze::k_WallLines); ++y)
		{
			KeepWall(*maze, x, y, Wall::AlongX);
			KeepWall(*maze, x, y, Wall::AlongY);
		}
	}
	if (maze->sheep == entt::null || maze->wolf == entt::null)
	{
		return;
	}
	if (!IsAvailable(maze->sheep) || !IsAvailable(maze->wolf))
	{
		Reset(puzzle);
		maze = MazeOf(puzzle);
	}
	if (!Walks(maze->sheep) && !Walks(maze->wolf))
	{
		// the sheep put back on its square
		if (object::MapCoordsOf(maze->sheep) != maze->sheepPlacedAt)
		{
			const auto place = PlaceOf(*maze, lion_maze::SquareSpot(maze->sheepSquare));
			map_cells::MoveMapObject(maze->sheep, map_coords::ToWorld(place));
			maze->sheepPlacedAt = object::MapCoordsOf(maze->sheep);
		}
		// the first bell of the four ways rung sends the wolf sliding that way
		for (size_t index = 0; index < lion_maze::k_Ways; ++index)
		{
			RemakeBell(*maze, index);
			const auto* piece = BellOf(maze->bells.at(index));
			if (piece == nullptr || piece->height == 0)
			{
				continue;
			}
			RingBell(*maze, index);
			maze->started = true;
			const auto way = static_cast<lion_maze::Way>(index);
			if (const auto to = lion_maze::Slide(maze->wolfSquare, way, maze->kind); to.has_value())
			{
				maze->wolfSquare = *to;
				maze->wolfAngle = lion_maze::k_WayAngles.at(index);
				maze->walk = lion_maze::Walk::Turning;
			}
			break;
		}
		// the wolf turns to its way, then walks to its square
		animal_ai::TurnTowards(maze->wolf, maze->wolfAngle);
		if (maze->wolfAngle == animal_ai::GameAngle(maze->wolf))
		{
			if (maze->walk == lion_maze::Walk::Turning)
			{
				animal_ai::SetSpeedInMetresPerSecond(maze->wolf, lion_maze::k_WalkMetresPerSecond);
				const auto to = map_coords::ToMetres(PlaceOf(*maze, lion_maze::SquareSpot(maze->wolfSquare)));
				animal_ai::MoveTo(maze->wolf, to, 0.0f, animal_ai::AnimalState::DecideWhatToDo);
				maze->walk = lion_maze::Walk::Walking;
			}
			else if (maze->walk == lion_maze::Walk::Walking && animal_ai::AreWeThere(maze->wolf, 0.0f))
			{
				maze->walk = lion_maze::Walk::There;
			}
		}
		// the corner bell starts the maze again
		if (const auto* piece = BellOf(maze->bells.at(lion_maze::k_ResetBell)); piece != nullptr && piece->height != 0)
		{
			Reset(puzzle);
			maze = MazeOf(puzzle);
		}
	}
	// every bell unrung
	for (const auto bell : maze->bells)
	{
		if (auto* piece = BellOf(bell); piece != nullptr)
		{
			piece->height = 0;
		}
	}
}

bool lion_maze_game::TestSolution(entt::entity puzzle)
{
	auto* maze = MazeOf(puzzle);
	if (maze != nullptr && IsTheseus(*maze))
	{
		return TestTheseusSolution(puzzle, *maze);
	}
	if (maze == nullptr || maze->wolf == entt::null || maze->sheep == entt::null)
	{
		return false;
	}
	if (Walks(maze->wolf) || maze->walk != lion_maze::Walk::There || !animal_ai::AreWeThere(maze->wolf, 0.0f) ||
	    maze->wolfSquare != maze->sheepSquare)
	{
		return false;
	}
	// solved: a sparkle of three turns over the sheep, and the sheep and the wolf go
	Sparkle(PointOf(maze->sheep), lion_maze::k_PressSparkleTurns);
	ToBeDeleted(std::exchange(maze->sheep, entt::null));
	ToBeDeleted(std::exchange(maze->wolf, entt::null));
	Locator::entitiesRegistry::value().Get<PuzzleGame>(puzzle).played = true;
	return true;
}

bool lion_maze_game::Started(entt::entity puzzle)
{
	const auto* maze = MazeOf(puzzle);
	return maze != nullptr && maze->started;
}
