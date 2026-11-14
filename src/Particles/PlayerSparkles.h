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

#include <glm/vec3.hpp>

#include "Physics/TurnRules.h"

/// What the other gods show of themselves on the land: no hand, but a cloud of sparkles in the god's colour where its
/// interface reports its camera, which for a computer god is where its hand is, with the god's symbol turning faintly in
/// the middle. Each game turn every other god that shows them adds one sparkle to a pool of its own; the pool is drawn
/// as one piece at that point, the symbol after the sparkles while the pool has any.
namespace openblack::player_sparkles
{

/// The number of tints that a sparkle takes at random. Four of them change the colour, the other three keep it.
inline constexpr int32_t k_Tints = 7;
/// The full size of a sparkle, as a mote's size is given
inline constexpr float k_Size = 10.0f;
/// The alpha of a sparkle
inline constexpr uint32_t k_Alpha = 0x80u;
/// The most sparkles a god's pool holds
inline constexpr size_t k_PoolSize = 1024;

/// The symbol: a sprite of the players' symbol sheet (3D/PlayerSymbolSheet.h), four cells to a row, the god's cell by its
/// player number, half as wide as this, drawn black at this alpha (0xAARRGGBB) over what is behind it
inline constexpr float k_SymbolHalfSize = 3.0f;
inline constexpr uint8_t k_SymbolCellsPerRow = 4;
inline constexpr uint32_t k_SymbolArgb = 0x30000000u;
/// The symbol turns this many radians for each millisecond of game time, each time a pool's symbol is drawn: one turn
/// shared by every god, so it goes round faster the more gods show theirs
inline constexpr float k_SymbolTurnPerMs = 0.0005f;

/// Who plays a player, by the original's player types
enum class Played : uint8_t
{
	Nobody = 0,
	Person = 1,
	Computer = 2,
	Neutral = 3,
};

/// Whether another god shows sparkles: a computer god only while its mind plays, a person only while the network game
/// has them. Nobody's player and the neutral one show none.
[[nodiscard]] bool Shows(Played played, bool mindOn, bool personInGame);

/// The colour of a sparkle, 0xAARRGGBB: each channel of the god's colour at half strength, then changed by the tint
/// (0 to 6), with the sparkle's alpha. Tint 0 goes half way to (255, 144, 144), 1 to (255, 255, 144), 2 to
/// (144, 255, 144), and 3 makes it a quarter darker.
[[nodiscard]] uint32_t Colour(uint32_t godColour, uint32_t tint);

/// The kind of a sparkle from a random number of 0 or 1: a mote with its frames reversed for 0, else a mote
[[nodiscard]] physics::turn::PuffKind KindOf(uint32_t roll);

/// The symbol's turn after a draw that took this many milliseconds of game time: the turn goes on by the frame's
/// share, then is brought back once into -pi..pi
[[nodiscard]] float TurnSymbol(float angle, uint32_t milliseconds);

/// The symbol's cell of the sheet: the low six bits of the player number
[[nodiscard]] uint8_t SymbolCell(uint32_t playerNumber);

/// Where a god's interface reports its camera once its citadel is made, before its hand reports anything: the camera
/// looks at the citadel's place on its map grid (`focus`, at the citadel's height there) from 20 above it and 20 further
/// along z
[[nodiscard]] glm::vec3 CameraOverCitadel(glm::vec3 focus);

} // namespace openblack::player_sparkles
