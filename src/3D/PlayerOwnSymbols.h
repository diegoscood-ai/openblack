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
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <entt/core/fwd.hpp>

/// The player's own symbol pictures: Symbol1.bmp to Symbol16.bmp in the folder of the profile played, each taking the
/// place of the symbol of its number in the symbols the player chooses from (ChooseSymbol, 4 x 4 cells of 64 x 64) as
/// the game starts. A picture is a Windows bitmap of at least 4096 bytes; its top left 64 x 64 texels go into the cell,
/// as grey in the colour and the same grey in the alpha, the texels past a smaller bitmap 0. Windows matches each texel
/// to a grey ramp first: a texel whose red, green and blue agree in their top five bits takes that step's middle
/// grey, (top five bits << 3) | 4. The grey Windows gives any other colour depends on the texels around it, so a
/// picture with such a texel is not read.
namespace openblack::player_own_symbols
{

inline constexpr uint32_t k_Count = 16;
inline constexpr size_t k_SmallestFile = 4096;
inline constexpr uint16_t k_CellSize = 64;
inline constexpr uint16_t k_SheetSize = 256;
inline constexpr uint8_t k_CellsPerRow = 4;

/// A picture's 64 x 64 greys, row after row, top row first
using Picture = std::array<uint8_t, static_cast<size_t>(k_CellSize) * k_CellSize>;
using Pictures = std::array<std::optional<Picture>, k_Count>;

/// Why a file gives no picture
enum class Refusal : uint8_t
{
	TooSmall,      ///< fewer than 4096 bytes: the game does not open it
	NotABitmap,    ///< not a bitmap Windows loads
	NotDetermined, ///< a bitmap whose greys are not known: a texel of colour, or a header, row order or compressed
	               ///< run whose reading by Windows was not established (see the wiki's own symbols)
};

/// The file of a picture in the profile's folder: Symbol1.bmp for the first symbol
[[nodiscard]] std::string FileName(uint32_t index);

/// The grey a texel of colour takes, or none when it is not known
[[nodiscard]] constexpr std::optional<uint8_t> GreyOf(uint8_t red, uint8_t green, uint8_t blue) noexcept
{
	const auto step = static_cast<uint8_t>(red >> 3u);
	if (step != (green >> 3u) || step != (blue >> 3u))
	{
		return std::nullopt;
	}
	return static_cast<uint8_t>((step << 3u) | 4u);
}

/// The picture of a bitmap file's bytes
[[nodiscard]] std::expected<Picture, Refusal> PictureOf(std::span<const uint8_t> file);

/// Every picture of a profile's folder, by `read` (a file's bytes, none when it is not there). A file not read is logged,
/// but for one too small, which the game passes over without a word
[[nodiscard]] Pictures
ReadPictures(const std::function<std::optional<std::vector<uint8_t>>(const std::filesystem::path&)>& read);

/// The pictures kept after a read: each symbol the read has a picture for takes it, the others keep theirs. Whether
/// one changed
[[nodiscard]] bool Merge(Pictures& kept, const Pictures& read);

/// Puts a picture into its symbol's cell of a 256 x 256 sheet of `channels` bytes a texel (3 for ChooseSymbol.raw, 1
/// for ChooseSymbola.raw): its grey in every channel
void Paste(std::span<uint8_t> sheet, uint8_t channels, uint32_t index, const Picture& picture);

/// The id of a picture in the byte cache
[[nodiscard]] entt::id_type BlobId(uint32_t index);

/// The pictures of the profile played go into the byte cache, each in place of the one its symbol had; a symbol the
/// profile has no picture for keeps the one it had, from a profile played before. As the game starts, and again as the
/// menu's main page or the Players page shows and as another profile is played. Nothing without the profiles' or the
/// resource services, or a profile folder (a run of the checks keeps none). Whether a picture changed
bool ReadForProfile();

/// Whether a texture file is one the pictures go into: ChooseSymbol.raw (3) or ChooseSymbola.raw (1), by its stem in
/// lower case, the channels it has, or none for another
[[nodiscard]] std::optional<uint8_t> ChannelsOfSheet(std::string_view lowerCaseStem);

/// Pastes every picture the byte cache holds into a sheet's bytes. Nothing without the resource service
void PasteInto(std::span<uint8_t> sheet, uint8_t channels);

} // namespace openblack::player_own_symbols
