/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LowResTile.h"

#include <LNDFile.h>

namespace openblack::low_res
{

namespace
{
constexpr size_t k_BlockBytes = 16;
// Far beyond any page, so that a broken file cannot ask for an absurd surface
constexpr uint32_t k_MaxSize = 4096;

struct Rgb
{
	uint32_t r;
	uint32_t g;
	uint32_t b;
};

// 5:6:5 to 8 bits a channel, the top bits repeated into the low ones
constexpr Rgb Widen(uint32_t colour) noexcept
{
	const uint32_t r = (colour >> 11) & 0x1F;
	const uint32_t g = (colour >> 5) & 0x3F;
	const uint32_t b = colour & 0x1F;
	return {.r = (r << 3) | (r >> 2), .g = (g << 2) | (g >> 4), .b = (b << 3) | (b >> 2)};
}

constexpr uint16_t ToNibbles(const Rgb& colour) noexcept
{
	return static_cast<uint16_t>(((colour.r >> 4) << 8) | ((colour.g >> 4) << 4) | (colour.b >> 4));
}

// The tile's step through the block texture, 16.16: 64 / 62 and 16 / 15 as the game's single floats round them to
// the nearest whole, and the texels each fills across the tile's inside
constexpr uint32_t k_Step = 67650;
constexpr uint32_t k_LowResolutionStep = 69905;
constexpr uint32_t k_Inside = 62;
constexpr uint32_t k_LowResolutionInside = 30;

// The mean of two ARGB 4444 texels, nibble by nibble, rounded down
constexpr uint16_t NibbleMean(uint32_t a, uint32_t b) noexcept
{
	uint32_t mean = 0;
	for (const uint32_t mask : {0xFu, 0xF0u, 0xF00u, 0xF000u})
	{
		mean |= (((a & mask) + (b & mask)) >> 1) & mask;
	}
	return static_cast<uint16_t>(mean);
}

// The texel index of a tile's first texel: its place, halved by the divisor
constexpr int64_t TileOrigin(TilePlace place, uint32_t pitch, int32_t divisor) noexcept
{
	return static_cast<int64_t>(place.row / divisor) * pitch + place.column / divisor;
}

// Whether a tile of `size` texels a side from `origin` lies inside a span of `count` texels
constexpr bool TileFits(int64_t origin, uint32_t pitch, uint32_t size, size_t count) noexcept
{
	return origin >= 0 && size >= 2 &&
	       origin + static_cast<int64_t>(size - 1) * pitch + (size - 1) < static_cast<int64_t>(count);
}
} // namespace

std::array<uint16_t, 16> DecodeDxt3Block(std::span<const uint8_t, 16> block) noexcept
{
	const uint32_t colour0 = block[8] | (static_cast<uint32_t>(block[9]) << 8);
	const uint32_t colour1 = block[10] | (static_cast<uint32_t>(block[11]) << 8);
	const Rgb c0 = Widen(colour0);
	const Rgb c1 = Widen(colour1);

	std::array<uint16_t, 4> palette {ToNibbles(c0), ToNibbles(c1), 0, 0};
	if (colour0 > colour1)
	{
		palette[2] =
		    ToNibbles({.r = (2 * c0.r + c1.r + 1) / 3, .g = (2 * c0.g + c1.g + 1) / 3, .b = (2 * c0.b + c1.b + 1) / 3});
		palette[3] =
		    ToNibbles({.r = (c0.r + 2 * c1.r + 1) / 3, .g = (c0.g + 2 * c1.g + 1) / 3, .b = (c0.b + 2 * c1.b + 1) / 3});
	}
	else
	{
		palette[2] = ToNibbles({.r = (c0.r + c1.r) / 2, .g = (c0.g + c1.g) / 2, .b = (c0.b + c1.b) / 2});
	}

	std::array<uint16_t, 16> texels {};
	for (size_t i = 0; i < texels.size(); ++i)
	{
		const uint32_t alpha = (block[i / 2] >> ((i % 2) * 4)) & 0xF;
		const uint32_t code = (block[12 + i / 4] >> ((i % 4) * 2)) & 3;
		texels[i] = static_cast<uint16_t>((alpha << 12) | palette[code]);
	}
	return texels;
}

std::vector<uint16_t> DecodeDxt3(std::span<const uint8_t> bits, uint32_t width, uint32_t height)
{
	const size_t blocksAcross = width / 4;
	const size_t blocksDown = height / 4;
	if (width % 4 != 0 || height % 4 != 0 || width > k_MaxSize || height > k_MaxSize ||
	    bits.size() < blocksAcross * blocksDown * k_BlockBytes)
	{
		return {};
	}
	std::vector<uint16_t> texels(static_cast<size_t>(width) * height);
	for (size_t by = 0; by < blocksDown; ++by)
	{
		for (size_t bx = 0; bx < blocksAcross; ++bx)
		{
			const auto block = bits.subspan((by * blocksAcross + bx) * k_BlockBytes).first<k_BlockBytes>();
			const auto decoded = DecodeDxt3Block(block);
			for (size_t i = 0; i < decoded.size(); ++i)
			{
				texels[(by * 4 + i / 4) * width + bx * 4 + i % 4] = decoded[i];
			}
		}
	}
	return texels;
}

std::optional<Page> DecodePage(const lnd::LNDLowResolutionTexture& record)
{
	if (record.FourCC() != lnd::LNDLowResolutionTexture::k_FourCCDxt3)
	{
		return std::nullopt;
	}
	Page page {.width = record.Width(), .height = record.Height(), .texels = {}};
	page.texels = DecodeDxt3(record.CompressedTexels(), page.width, page.height);
	if (page.texels.empty())
	{
		return std::nullopt;
	}
	return page;
}

bool UpdateTile(std::span<uint16_t> page, uint32_t pagePitch, TilePlace place, std::span<const uint16_t> blockTexels,
                uint32_t blockPitch, bool lowResolution, const TileNeighbours& neighbours)
{
	const uint32_t step = lowResolution ? k_LowResolutionStep : k_Step;
	const uint32_t inside = lowResolution ? k_LowResolutionInside : k_Inside;
	// the tile is a quarter of the page's row whatever the option; how many texels its inside takes is the option's
	const uint32_t size = pagePitch / 4;
	const int64_t origin = TileOrigin(place, pagePitch, lowResolution ? 2 : 1);
	const uint32_t lastPick = 4 * (((inside - 1) * step) >> 16);
	if (size < inside + 2 || !TileFits(origin, pagePitch, size, page.size()) ||
	    static_cast<size_t>(lastPick) * blockPitch + lastPick >= blockTexels.size())
	{
		return false;
	}
	const auto at = [&page, origin, pagePitch](uint32_t row, uint32_t column) -> uint16_t& {
		return page[static_cast<size_t>(origin + static_cast<int64_t>(row) * pagePitch + column)];
	};

	// the inside, from every fourth row and column of the texture, its first column twice and its last repeated
	for (uint32_t r = 0; r < inside; ++r)
	{
		const size_t sourceRow = static_cast<size_t>(((r * step) >> 16) * 4) * blockPitch;
		at(r + 1, 0) = blockTexels[sourceRow];
		for (uint32_t k = 0; k < inside; ++k)
		{
			at(r + 1, k + 1) = blockTexels[sourceRow + 4 * ((k * step) >> 16)];
		}
		at(r + 1, inside + 1) = at(r + 1, inside);
	}
	// the first and the last rows
	for (uint32_t c = 0; c < size; ++c)
	{
		at(0, c) = at(1, c);
		at(size - 1, c) = at(size - 2, c);
	}

	// the borders shared with the rewritten neighbours, whose places are halved when a tile is 32 texels a side
	const int32_t neighbourDivisor = size == 32 ? 2 : 1;
	const uint32_t last = size - 1;
	const auto join = [&](const std::optional<NeighbourTile>& neighbour, bool alongZ, bool lower) {
		if (!neighbour)
		{
			return;
		}
		const auto other = neighbour->page;
		const int64_t otherOrigin = TileOrigin(neighbour->place, pagePitch, neighbourDivisor);
		if (!TileFits(otherOrigin, pagePitch, size, other.size()))
		{
			return;
		}
		const auto otherAt = [&other, otherOrigin, pagePitch](uint32_t row, uint32_t column) -> uint16_t& {
			return other[static_cast<size_t>(otherOrigin + static_cast<int64_t>(row) * pagePitch + column)];
		};
		// the tile's first or last line against the neighbour's last or first, and the line next to each
		const uint32_t own = lower ? 0 : last;
		const uint32_t theirs = lower ? last : 0;
		const uint32_t ownNext = lower ? own + 1 : own - 1;
		const uint32_t theirsNext = lower ? theirs - 1 : theirs + 1;
		for (uint32_t k = 0; k < size; ++k)
		{
			if (alongZ)
			{
				const uint16_t mean = NibbleMean(at(k, own), otherAt(k, theirs));
				otherAt(k, theirsNext) = mean;
				at(k, ownNext) = mean;
				otherAt(k, theirs) = mean;
				at(k, own) = mean;
			}
			else
			{
				const uint16_t mean = NibbleMean(at(own, k), otherAt(theirs, k));
				otherAt(theirsNext, k) = mean;
				at(ownNext, k) = mean;
				otherAt(theirs, k) = mean;
				at(own, k) = mean;
			}
		}
	};
	join(neighbours.lowerZ, true, true);
	join(neighbours.higherZ, true, false);
	join(neighbours.lowerX, false, true);
	join(neighbours.higherX, false, false);
	return true;
}

} // namespace openblack::low_res
