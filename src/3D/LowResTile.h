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

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace openblack::lnd
{
struct LNDLowResolutionTexture;
} // namespace openblack::lnd

/// The land's low-resolution pages: each holds 4 x 4 tiles of 64 x 64 texels, one per block, that the game draws for
/// a block far away or still waiting for its own texture (docs/wiki/world/terrain.md#the-low-resolution-pages). No
/// render dependencies.
namespace openblack::low_res
{

/// Texels along a page side
constexpr uint32_t k_PageSize = 256;

/// A page as the game keeps it: ARGB 4444 words, row by row (y * width + x)
struct Page
{
	uint32_t width {0};
	uint32_t height {0};
	std::vector<uint16_t> texels;
};

/// One 16-byte DXT3 block as the game reads it back in 16 bits: its 16 texels, row by row. The alpha is the block's
/// own 4 bits. The two colours are widened from 5:6:5 to 8 bits a channel by repeating their top bits; when the first
/// colour is larger the other two are (2 c0 + c1 + 1) / 3 and (c0 + 2 c1 + 1) / 3, else the third is (c0 + c1) / 2
/// and the fourth black. Each channel then keeps its top 4 bits
[[nodiscard]] std::array<uint16_t, 16> DecodeDxt3Block(std::span<const uint8_t, 16> block) noexcept;

/// A whole DXT3 surface of width x height (multiples of 4) as ARGB 4444, row by row; empty when the bits are short
[[nodiscard]] std::vector<uint16_t> DecodeDxt3(std::span<const uint8_t> bits, uint32_t width, uint32_t height);

/// A land file's low-resolution page, decoded; nothing when it is not DXT3 or its texels are short
[[nodiscard]] std::optional<Page> DecodePage(const lnd::LNDLowResolutionTexture& record);

/// Where a block's tile sits on its page: its first texel's column and row (the land file's iuLrs and ivLrs, in
/// full-size texels: halved on a low-resolution page)
struct TilePlace
{
	int32_t column {0};
	int32_t row {0};
};

/// A neighbouring block whose tile has already been rewritten: the texels of its page (the same page as the block's
/// when they share one; its rows are as long as the block's page's) and its tile's place
struct NeighbourTile
{
	std::span<uint16_t> page;
	TilePlace place;
};

/// The four neighbours in the order their shared borders are joined: the blocks before and after along z (the tile's
/// columns), then along x (its rows). Nothing for a neighbour off the map, missing, or whose tile is not rewritten yet
struct TileNeighbours
{
	std::optional<NeighbourTile> lowerZ;
	std::optional<NeighbourTile> higherZ;
	std::optional<NeighbourTile> lowerX;
	std::optional<NeighbourTile> higherX;
};

/// Rewrites a block's tile on its page from the block's finished texture (ARGB 4444, row x holding the texels of
/// the block's x, `blockPitch` texels a row), as the game does at the last step of every block paint. The tile is a
/// quarter of a page row wide and high (64, or 32 with the low-resolution option): every fourth texel of the texture,
/// picked with a step of 64 / 62 (16 / 15 at low resolution), fills its inside, one texel in from each side; the
/// first column repeats the second, the last the one before it, then the first and last rows likewise. Last, the
/// border with each rewritten neighbour, two texels on each side, takes the mean of the two edge texels, nibble by
/// nibble, rounded down. The neighbours' pages are written too. Returns false, writing nothing, when the tile or the
/// texture do not fit their spans. The caller then counts the block's tile as rewritten and its own page as out of
/// date (a neighbour's page is not marked, though its border changed)
bool UpdateTile(std::span<uint16_t> page, uint32_t pagePitch, TilePlace place, std::span<const uint16_t> blockTexels,
                uint32_t blockPitch, bool lowResolution, const TileNeighbours& neighbours);

} // namespace openblack::low_res
