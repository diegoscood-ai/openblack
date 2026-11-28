/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/vec2.hpp>

/// The players' symbol sheet the other gods' symbols are drawn from: 256 x 256 texels, four cells of 64 to a row, a
/// player's cell by its number. It starts empty (no colour, no alpha) the first time the game needs it and lasts as long
/// as the game runs. At the start of each game, once its first land is loaded (a campaign's first land or a
/// skirmish's land, not a land change), every player that the computer plays then gets its god's image in its cell: one
/// of three greyscale images of 64 x 64 bytes, Lethys's, Khazar's or Nemesis's, each byte's top four bits giving the
/// cell's colour and alpha alike. A god switched on later keeps the cell it had.
namespace openblack::player_symbol_sheet
{

/// The sheet's name, which the particle creators drawn from it carry as their texture, and its id in the texture cache
inline constexpr std::string_view k_TextureName = "player_symbols/sheet";
inline constexpr entt::id_type k_TextureId = entt::hashed_string(k_TextureName.data(), k_TextureName.size()).value();
inline constexpr uint16_t k_SheetSize = 256;
inline constexpr uint16_t k_CellSize = 64;
inline constexpr uint8_t k_CellsPerRow = 4;
/// The bytes of a god's image, one a texel
inline constexpr size_t k_ImageBytes = static_cast<size_t>(k_CellSize) * k_CellSize;

/// The sheet's and its cells' sizes in texels. While the game keeps its pictures at half size (the detail levels 0 and
/// 1) the sheet is made at half size, 128 x 128 with cells of 32, the same four cells to a row
struct Layout
{
	uint16_t sheet {k_SheetSize};
	uint16_t cell {k_CellSize};
};
[[nodiscard]] constexpr Layout LayoutFor(bool halfSize)
{
	return halfSize ? Layout {.sheet = k_SheetSize / 2, .cell = k_CellSize / 2} : Layout {};
}

/// Which god's image a computer player's cell takes: the image after the player's number less one, but the second
/// player takes Khazar's on the third land and Nemesis's on the fifth; brought within the three images
[[nodiscard]] uint32_t ComputerImageIndex(uint32_t playerNumber, int32_t landNumber);

/// The file of an image in the game's Data/Textures folder: Lethis.cps, Kazarr.cps, nemesis.cps; at half size their
/// images of 32 x 32, LethisL.cps, KazarrL.cps, nemesisL.cps
[[nodiscard]] std::string_view ComputerImageFile(uint32_t index, bool halfSize = false);

/// The corner of a player's cell, in texels: across by the number's two low bits, down by the rest
[[nodiscard]] glm::u16vec2 CellOrigin(uint32_t playerNumber, uint16_t cellSize = k_CellSize);

/// A god's image as the cell's RGBA8 texels, row after row: each byte's top four bits, widened to eight (times 17), in
/// all four channels. None when the image is shorter than a cell
[[nodiscard]] std::optional<std::vector<uint8_t>> CellTexels(std::span<const uint8_t> image, uint16_t cellSize = k_CellSize);

/// A game starts, its first land loaded: the sheet is made empty the first time, then each player 0 to 7 whose type is
/// the computer's gets its god's image, by the land number the land's script set. The sheet's size is the one it was made
/// at, half size when the game kept its pictures at half size then (the detail level's). Nothing without the resource
/// caches
void MakeComputerSymbols();

} // namespace openblack::player_symbol_sheet
