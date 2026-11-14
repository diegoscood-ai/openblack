/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>

/// The rules of the lion maze (PuzzleGame type 12, Land 5's Stanley the wolf): a grid of 7 by 7 squares walled by its
/// map, a wolf that slides one way until a wall stops it, and a sheep it is to reach. The grid's places are counted in
/// half squares from the puzzle's place: square (x, y) stands at (2x + 1, 2y + 1). Pure.
/// Wiki: docs/wiki/world/lion-maze.md
namespace openblack::ecs::lion_maze
{

/// The squares along each side of the lion maze
inline constexpr int32_t k_Squares = 7;
/// The map's rows and the bytes of each (eight squares and an end): the top row is the far side, y = 7
inline constexpr size_t k_MapRows = 8;
inline constexpr size_t k_MapRowBytes = 9;
/// What a square's byte says: a wall on its far side along y, on its far side along x, or both
inline constexpr uint8_t k_WallAlongY = 0xAF;
inline constexpr uint8_t k_WallAlongX = 0x7C;
inline constexpr uint8_t k_WallsBoth = 0xAC;

/// The map of type 12, row by row from y = 7 down to y = 0
inline constexpr std::array<uint8_t, k_MapRows * k_MapRowBytes> k_Map {
    0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x00, //
    0xAF, 0xAF, 0xAF, 0xAF, 0xAC, 0xAF, 0xAC, 0x2E, 0x00, //
    0x7C, 0xAF, 0xAC, 0x2E, 0x2E, 0x2E, 0xAC, 0x2E, 0x00, //
    0x2E, 0x2E, 0xAF, 0x2E, 0xAC, 0x2E, 0x7C, 0x2E, 0x00, //
    0xAF, 0x2E, 0xAF, 0x2E, 0x2E, 0xAC, 0x7C, 0x2E, 0x00, //
    0xAF, 0x2E, 0x7C, 0xAF, 0x2E, 0xAC, 0x7C, 0x2E, 0x00, //
    0x2E, 0x2E, 0x2E, 0x2E, 0xAF, 0x2E, 0x7C, 0x2E, 0x00, //
    0x2E, 0xAC, 0x2E, 0x7C, 0x2E, 0x2E, 0xAC, 0x2E, 0x00, //
};

/// A square of the grid
struct Square
{
	int32_t x {0};
	int32_t y {0};
	bool operator==(const Square&) const = default;
};

/// Where the sheep and the wolf start
inline constexpr Square k_SheepStart {.x = 4, .y = 4};
inline constexpr Square k_WolfStart {.x = 1, .y = 3};

/// A walled grid of its own: its squares along each side, its map, where the one the wolf goes for (the sheep, the
/// villager) and the wolf start. The lion maze (type 12) and the two Theseus mazes (types 10 and 11) share the rules
struct Kind
{
	int32_t squares;
	std::array<uint8_t, k_MapRows * k_MapRowBytes> map;
	Square preyStart;
	Square wolfStart;
};
inline constexpr Kind k_LionMaze {.squares = k_Squares, .map = k_Map, .preyStart = k_SheepStart, .wolfStart = k_WolfStart};
inline constexpr Kind k_TheseusOne {
    .squares = 7,
    .map = {0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x00, //
            0xAF, 0xAF, 0xAF, 0xAF, 0xAF, 0xAF, 0xAC, 0x2E, 0x00, //
            0x7C, 0xAF, 0xAF, 0xAF, 0xAF, 0xAF, 0x7C, 0x2E, 0x00, //
            0x2E, 0x2E, 0xAF, 0xAF, 0xAF, 0xAC, 0x7C, 0x2E, 0x00, //
            0x2E, 0xAF, 0xAF, 0x7C, 0x2E, 0x7C, 0x7C, 0x2E, 0x00, //
            0x7C, 0x7C, 0x2E, 0x2E, 0x7C, 0x7C, 0x7C, 0x2E, 0x00, //
            0x7C, 0x7C, 0x2E, 0x2E, 0xAF, 0xAF, 0x7C, 0x2E, 0x00, //
            0x2E, 0xAF, 0xAF, 0xAF, 0xAF, 0xAF, 0xAF, 0x2E, 0x00},
    .preyStart = {.x = 2, .y = 3},
    .wolfStart = {.x = 2, .y = 5},
};
inline constexpr Kind k_TheseusTwo {
    .squares = 8,
    .map = {0xAF, 0xAF, 0xAF, 0xAF, 0xAF, 0xAF, 0x2E, 0xAC, 0x00, //
            0x7C, 0xAF, 0xAF, 0xAF, 0xAF, 0x7C, 0x2E, 0x7C, 0x00, //
            0x2E, 0xAF, 0x2E, 0x2E, 0x7C, 0xAF, 0x2E, 0x7C, 0x00, //
            0x2E, 0x2E, 0x2E, 0x7C, 0x2E, 0x2E, 0x2E, 0x7C, 0x00, //
            0x2E, 0x7C, 0x7C, 0x2E, 0x2E, 0x2E, 0x2E, 0x7C, 0x00, //
            0x7C, 0xAC, 0xAF, 0x2E, 0x2E, 0x2E, 0x2E, 0x7C, 0x00, //
            0x7C, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x7C, 0x00, //
            0x2E, 0xAF, 0xAF, 0x2E, 0x2E, 0x2E, 0x2E, 0x7C, 0x00},
    .preyStart = {.x = 2, .y = 7},
    .wolfStart = {.x = 1, .y = 6},
};

/// The way a wall stands: across y (on a square's far side along y) or across x
enum class Wall : uint8_t
{
	AlongY = 0,
	AlongX = 1,
};

/// Whether a wall stands on the far side of square (x, y): any square off the map's eight by eight has one
[[nodiscard]] constexpr bool WallAt(int32_t x, int32_t y, Wall wall, const Kind& kind = k_LionMaze) noexcept
{
	if (x < 0 || x >= 8 || y < 0 || y >= 8)
	{
		return true;
	}
	const auto cell = kind.map.at(static_cast<size_t>(7 - y) * k_MapRowBytes + static_cast<size_t>(x));
	return cell == (wall == Wall::AlongY ? k_WallAlongY : k_WallAlongX) || cell == k_WallsBoth;
}

/// The four ways the bells send the wolf, the bells' order: +y, +x, -y, -x
enum class Way : uint8_t
{
	PlusY = 0,
	PlusX = 1,
	MinusY = 2,
	MinusX = 3,
};
inline constexpr size_t k_Ways = 4;

/// Whether a wall stops a step from square (x, y) the way given
[[nodiscard]] constexpr bool Blocked(Square from, Way way, const Kind& kind = k_LionMaze) noexcept
{
	switch (way)
	{
	case Way::PlusY:
		return WallAt(from.x, from.y, Wall::AlongY, kind);
	case Way::PlusX:
		return WallAt(from.x, from.y, Wall::AlongX, kind);
	case Way::MinusY:
		return WallAt(from.x, from.y - 1, Wall::AlongY, kind);
	case Way::MinusX:
		return WallAt(from.x - 1, from.y, Wall::AlongX, kind);
	}
	return false;
}

/// The game angle the wolf faces going each way (2048 to the circle)
inline constexpr std::array<uint16_t, k_Ways> k_WayAngles {0x200, 0, 0x600, 0x400};

/// The wolf's slide: from the square, step after step the way given until a wall of the maze's kind stops it. None when
/// a wall stops the first step
[[nodiscard]] constexpr std::optional<Square> Slide(Square from, Way way, const Kind& kind = k_LionMaze) noexcept
{
	if (Blocked(from, way, kind))
	{
		return std::nullopt;
	}
	auto at = from;
	do
	{
		switch (way)
		{
		case Way::PlusY:
			++at.y;
			break;
		case Way::PlusX:
			++at.x;
			break;
		case Way::MinusY:
			--at.y;
			break;
		case Way::MinusX:
			--at.x;
			break;
		}
	} while (!Blocked(at, way, kind));
	return at;
}

/// A place of the grid in half squares: across and along
struct GridSpot
{
	int32_t across {0};
	int32_t along {0};
};

/// The middle of a square
[[nodiscard]] constexpr GridSpot SquareSpot(Square square) noexcept
{
	return {.across = square.x * 2 + 1, .along = square.y * 2 + 1};
}

/// Where the wall on the near side of square (x, y) stands: across y at (2x + 1, 2y), across x at (2x, 2y + 1)
[[nodiscard]] constexpr GridSpot WallSpot(int32_t x, int32_t y, Wall wall) noexcept
{
	return wall == Wall::AlongY ? GridSpot {.across = x * 2 + 1, .along = y * 2}
	                            : GridSpot {.across = x * 2, .along = y * 2 + 1};
}

/// Whether the square (x, y) of the grid's walls (x and y 0 to the squares) has a wall on its near side: across x for y
/// below the squares, across y for x below them, as the near square's map says
[[nodiscard]] constexpr bool HasNearWall(int32_t x, int32_t y, Wall wall, const Kind& kind = k_LionMaze) noexcept
{
	if (wall == Wall::AlongX)
	{
		return y != kind.squares && WallAt(x - 1, y, Wall::AlongX, kind);
	}
	return x != kind.squares && WallAt(x, y - 1, Wall::AlongY, kind);
}

/// The palms at the grid's corners and the street lanterns half way along its sides, by its squares (the lion maze's
/// are k_PalmSpots and k_LanternSpots)
[[nodiscard]] constexpr std::array<GridSpot, 4> PalmSpots(const Kind& kind) noexcept
{
	const int32_t far = kind.squares * 2;
	return {{{0, 0}, {far, far}, {far, 0}, {0, far}}};
}
[[nodiscard]] constexpr std::array<GridSpot, 4> LanternSpots(const Kind& kind) noexcept
{
	const int32_t far = kind.squares * 2;
	const int32_t half = (kind.squares * 2 + 2) / 2 - 2;
	return {{{half, 0}, {far, half}, {half, far}, {0, half}}};
}

/// A wall's angle about y from the puzzle's game angle, as the game works it: the angle times pi, a 512th of that, and
/// a quarter turn more for a wall across x, each a float
[[nodiscard]] constexpr float WallAngle(uint16_t puzzleAngle, Wall wall) noexcept
{
	const float turned = (static_cast<float>(puzzleAngle) * 3.1415927f) * 0.001953125f;
	return wall == Wall::AlongX ? turned + 1.5707964f : turned;
}

/// The bells round the grid, the four ways and then the one that starts the maze again: their place, their angle
struct Bell
{
	GridSpot spot;
	float yAngle;
};
inline constexpr size_t k_Bells = 5;
inline constexpr size_t k_ResetBell = 4;
inline constexpr std::array<Bell, k_Bells> k_BellSpots {{
    {.spot = {.across = 7, .along = 17}, .yAngle = 0.0f},
    {.spot = {.across = 17, .along = 7}, .yAngle = 4.5f},
    {.spot = {.across = 7, .along = -3}, .yAngle = 3.0f},
    {.spot = {.across = -3, .along = 7}, .yAngle = 1.5f},
    {.spot = {.across = -3, .along = -3}, .yAngle = 2.4f},
}};
/// The bells' size
inline constexpr float k_BellScale = 0.5f;

/// The palms at the corners and the street lanterns half way along the sides
inline constexpr std::array<GridSpot, 4> k_PalmSpots {{{0, 0}, {14, 14}, {14, 0}, {0, 14}}};
inline constexpr std::array<GridSpot, 4> k_LanternSpots {{{6, 0}, {14, 6}, {6, 14}, {0, 6}}};

/// The pieces' age and size, the sparkle's magnitude, its turns at a press and at the start, and the wolf's walk
inline constexpr uint32_t k_PieceAge = 20;
inline constexpr float k_PieceScale = 2.0f;
inline constexpr float k_SparkleMagnitude = 2.0f;
inline constexpr int32_t k_PressSparkleTurns = 3;
inline constexpr int32_t k_StartSparkleTurns = 10;
inline constexpr float k_WalkMetresPerSecond = 4.0f;
/// The script sound bank's sample a bell rings
inline constexpr int32_t k_BellSample = 0x5C;

/// The wolf's walk to its square: it turns first, walks once it faces the way, and is there
enum class Walk : uint16_t
{
	Turning = 0,
	Walking = 1,
	There = 2,
};

// ---- The Theseus mazes (types 10 and 11) ----

/// The grid's steps of a half square, along x and z of the map
struct GridSteps
{
	int32_t cosine;
	int32_t sine;
};

/// The ground a click lands on, in half squares of the grid, put on a square's middle: the click's offset from the
/// place turned back by the grid's steps (each product, the difference or the sum and the quotient a float), truncated,
/// then halved towards zero, doubled and one added. None for a click off the grid's box: west or south of the place, or
/// east or north of its far corner, compared unsigned as the game does
[[nodiscard]] constexpr std::optional<GridSpot> ClickedSpot(int32_t placeX, int32_t placeZ, int32_t farX, int32_t farZ,
                                                            GridSteps steps, float scale, int32_t clickX,
                                                            int32_t clickZ) noexcept
{
	if (static_cast<uint32_t>(clickX) < static_cast<uint32_t>(placeX) ||
	    static_cast<uint32_t>(clickX) > static_cast<uint32_t>(farX) ||
	    static_cast<uint32_t>(clickZ) < static_cast<uint32_t>(placeZ) ||
	    static_cast<uint32_t>(clickZ) > static_cast<uint32_t>(farZ))
	{
		return std::nullopt;
	}
	const auto dx = static_cast<float>(clickX - placeX);
	const auto dz = static_cast<float>(clickZ - placeZ);
	const auto cosine = static_cast<float>(steps.cosine);
	const auto sine = static_cast<float>(steps.sine);
	const float divisor = (scale * scale) * 171793456.0f;
	const auto across = static_cast<int32_t>((cosine * dx - sine * dz) / divisor);
	const auto along = static_cast<int32_t>((sine * dx + cosine * dz) / divisor);
	return GridSpot {.across = (across / 2) * 2 + 1, .along = (along / 2) * 2 + 1};
}

/// What a click asks of the villager beyond a way: to wait, or nothing
inline constexpr int32_t k_StepNone = -2;
inline constexpr int32_t k_StepWait = -1;

/// The click's step from the villager's square, as a way (0 to 3) or k_StepWait or k_StepNone: his own square is a
/// wait; a square of his column is a step along y and one of his row a step along x, whatever the distance; a square
/// on a slant from him is nothing, and so is a step a wall stops
[[nodiscard]] constexpr int32_t ClickedStep(Square villager, GridSpot clicked, const Kind& kind) noexcept
{
	const int32_t dx = clicked.across - (villager.x * 2 + 1);
	const int32_t dz = clicked.along - (villager.y * 2 + 1);
	const int32_t adx = dx < 0 ? -dx : dx;
	const int32_t adz = dz < 0 ? -dz : dz;
	int32_t asked = k_StepNone;
	if (adx == 0 && adz == 0)
	{
		asked = k_StepWait;
	}
	else if (adx <= 1 && adz <= 1)
	{
		asked = k_StepWait;
	}
	else if (adx <= 1 && dz > 0)
	{
		asked = static_cast<int32_t>(Way::PlusY);
	}
	else if (adx <= 1 && dz < 0)
	{
		asked = static_cast<int32_t>(Way::MinusY);
	}
	else if (adz <= 1 && dx > 0)
	{
		asked = static_cast<int32_t>(Way::PlusX);
	}
	else if (adz <= 1 && dx < 0)
	{
		asked = static_cast<int32_t>(Way::MinusX);
	}
	if (asked == k_StepNone || asked == k_StepWait)
	{
		return asked;
	}
	return Blocked(villager, static_cast<Way>(asked), kind) ? k_StepNone : asked;
}

/// A game angle as the game works it: a float from -512 (a quarter turn short of +x) with the offset added, brought
/// into 0 to 2048, truncated
[[nodiscard]] constexpr uint16_t WrappedAngle(float offset) noexcept
{
	float angle = -512.0f + offset;
	if (!(angle < 2048.0f))
	{
		angle -= 2048.0f;
	}
	if (angle < 0.0f)
	{
		angle += 2048.0f;
	}
	return static_cast<uint16_t>(static_cast<int32_t>(angle));
}

/// The villager's way as the angle he faces: +y 512, +x 0, -y 1536, -x 1024
[[nodiscard]] constexpr uint16_t StepAngle(Way way) noexcept
{
	switch (way)
	{
	case Way::PlusY:
		return WrappedAngle(1024.0f);
	case Way::PlusX:
		return WrappedAngle(512.0f);
	case Way::MinusY:
		return WrappedAngle(0.0f);
	case Way::MinusX:
		return WrappedAngle(1536.0f);
	}
	return 0;
}

/// The wolf's chase: up to two steps towards the villager, each the first of +x, -x, +y, -y that brings it nearer along
/// that axis with no wall in the way, else it stops; and the code of the ways it took (+x 3, -x 18, +y 9, -y 1, each
/// counted once)
struct Chase
{
	Square wolf;
	int32_t ways;
};
[[nodiscard]] constexpr Chase ChaseOf(Square wolf, Square villager, const Kind& kind) noexcept
{
	Chase chase {.wolf = wolf, .ways = 0};
	auto& at = chase.wolf;
	for (int step = 0; step < 2; ++step)
	{
		if (at.x < villager.x && !Blocked(at, Way::PlusX, kind))
		{
			++at.x;
			chase.ways += chase.ways != 3 ? 3 : 0;
		}
		else if (villager.x < at.x && !Blocked(at, Way::MinusX, kind))
		{
			--at.x;
			chase.ways += chase.ways != 18 ? 18 : 0;
		}
		else if (at.y < villager.y && !Blocked(at, Way::PlusY, kind))
		{
			++at.y;
			chase.ways += chase.ways != 9 ? 9 : 0;
		}
		else if (villager.y < at.y && !Blocked(at, Way::MinusY, kind))
		{
			--at.y;
			chase.ways += chase.ways != 1 ? 1 : 0;
		}
		else
		{
			break;
		}
	}
	return chase;
}

/// The angle the wolf faces after its chase, by its ways' code: none when it did not move; one way or a pair its own or
/// the half way between them; any other code three quarters of a turn
[[nodiscard]] constexpr std::optional<uint16_t> ChaseAngle(int32_t ways) noexcept
{
	switch (ways)
	{
	case 0:
		return std::nullopt;
	case 3:
		return WrappedAngle(512.0f);
	case 4:
		return WrappedAngle(256.0f);
	case 9:
		return WrappedAngle(1024.0f);
	case 12:
		return WrappedAngle(768.0f);
	case 18:
		return WrappedAngle(1536.0f);
	case 19:
		return WrappedAngle(1792.0f);
	case 27:
		return WrappedAngle(1280.0f);
	default:
		return WrappedAngle(0.0f);
	}
}

/// The villager is out of the maze: on the column or the row past its last square
[[nodiscard]] constexpr bool Escaped(Square villager, const Kind& kind) noexcept
{
	return villager.x == kind.squares || villager.y == kind.squares;
}

/// The marker over a clicked square: a spot visual of no time limit, at a magnitude of 1
inline constexpr float k_MarkerMagnitude = 1.0f;

} // namespace openblack::ecs::lion_maze
