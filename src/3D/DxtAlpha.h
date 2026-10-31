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
#include <span>
#include <vector>

/// The alpha of the mesh pack's compressed (DXT) textures, as the game reads it back once they are converted to ARGB
/// 4444: the hand's pick of a tree tests it (docs/wiki/hand/picking.md#a-tree-under-the-cursor)
namespace openblack::dxt_alpha
{

enum class Format : uint8_t
{
	Dxt1, ///< 8-byte blocks: the fourth colour of a three-colour block is transparent
	Dxt3, ///< 16-byte blocks: four bits of alpha for every texel
	Dxt5, ///< 16-byte blocks: two alphas and the steps between them, three bits a texel
};

/// The 4-bit alpha of texel `index` (row x 4 + column) of one block. A DXT5 step is the integer mean of its two
/// alphas, and its 8 bits go to 4 by dropping the low ones
[[nodiscard]] inline uint16_t BlockAlpha(Format format, std::span<const uint8_t> block, uint32_t index)
{
	switch (format)
	{
	case Format::Dxt1:
	{
		const uint32_t colour0 = block[0] | (static_cast<uint32_t>(block[1]) << 8);
		const uint32_t colour1 = block[2] | (static_cast<uint32_t>(block[3]) << 8);
		const uint32_t code = (block[4 + index / 4] >> ((index % 4) * 2)) & 3;
		return colour0 <= colour1 && code == 3 ? 0 : 0xF;
	}
	case Format::Dxt3:
	{
		const uint32_t pair = block[index / 2];
		return static_cast<uint16_t>((index % 2 == 0 ? pair : pair >> 4) & 0xF);
	}
	case Format::Dxt5:
	{
		const uint32_t alpha0 = block[0];
		const uint32_t alpha1 = block[1];
		uint64_t codes = 0;
		for (size_t i = 0; i < 6; ++i)
		{
			codes |= static_cast<uint64_t>(block[2 + i]) << (8 * i);
		}
		const auto code = static_cast<uint32_t>((codes >> (index * 3)) & 7);
		uint32_t alpha = 0;
		if (code < 2)
		{
			alpha = code == 0 ? alpha0 : alpha1;
		}
		else if (alpha0 > alpha1)
		{
			alpha = ((8 - code) * alpha0 + (code - 1) * alpha1) / 7;
		}
		else if (code < 6)
		{
			alpha = ((6 - code) * alpha0 + (code - 1) * alpha1) / 5;
		}
		else
		{
			alpha = code == 6 ? 0 : 255;
		}
		return static_cast<uint16_t>(alpha >> 4);
	}
	}
	return 0xF;
}

/// The texture's top level as ARGB 4444 words with only their alpha: the 4-bit alpha in the top four bits, the colour
/// bits 0 (what graphics::shadow_math::MakeAlphaMap reads). Empty when `data` is too short for `width` x `height`
[[nodiscard]] inline std::vector<uint16_t> AlphaWords(Format format, int width, int height, std::span<const uint8_t> data)
{
	const size_t blockBytes = format == Format::Dxt1 ? 8 : 16;
	if (width <= 0 || height <= 0)
	{
		return {};
	}
	const auto blocksWide = static_cast<size_t>((width + 3) / 4);
	const auto blocksHigh = static_cast<size_t>((height + 3) / 4);
	if (data.size() < blocksWide * blocksHigh * blockBytes)
	{
		return {};
	}
	std::vector<uint16_t> words(static_cast<size_t>(width) * static_cast<size_t>(height));
	for (size_t y = 0; y < static_cast<size_t>(height); ++y)
	{
		for (size_t x = 0; x < static_cast<size_t>(width); ++x)
		{
			const auto block = data.subspan(((y / 4) * blocksWide + x / 4) * blockBytes, blockBytes);
			const auto alpha = BlockAlpha(format, block, static_cast<uint32_t>((y % 4) * 4 + x % 4));
			words[y * static_cast<size_t>(width) + x] = static_cast<uint16_t>(alpha << 12);
		}
	}
	return words;
}

/// Whether a DXT1 texture's top level has a transparent texel: a three-colour block (colour 0 not above colour 1) that
/// uses its fourth code. False when `data` is too short for `width` x `height`
[[nodiscard]] inline bool HasTransparentTexels(int width, int height, std::span<const uint8_t> data)
{
	if (width <= 0 || height <= 0)
	{
		return false;
	}
	const auto blocks = static_cast<size_t>((width + 3) / 4) * static_cast<size_t>((height + 3) / 4);
	if (data.size() < blocks * 8)
	{
		return false;
	}
	for (size_t b = 0; b < blocks; ++b)
	{
		const auto block = data.subspan(b * 8, 8);
		for (uint32_t i = 0; i < 16; ++i)
		{
			if (BlockAlpha(Format::Dxt1, block, i) == 0)
			{
				return true;
			}
		}
	}
	return false;
}

/// A DXT1 texture's top level as RGBA8, row after row, four bytes a texel. Each 5:6:5 colour is widened by repeating
/// its top bits; a four-colour block's two steps are a third and two thirds of the way between its colours, a
/// three-colour block's step is halfway and its fourth code transparent black, each step rounded to the nearest. (The
/// graphics cards' own decoders may round a step one apart; the original's is not read.) Empty when `data` is too
/// short
[[nodiscard]] inline std::vector<uint8_t> DecodeDxt1(int width, int height, std::span<const uint8_t> data)
{
	if (width <= 0 || height <= 0)
	{
		return {};
	}
	const auto blocksWide = static_cast<size_t>((width + 3) / 4);
	const auto blocksHigh = static_cast<size_t>((height + 3) / 4);
	if (data.size() < blocksWide * blocksHigh * 8)
	{
		return {};
	}
	const auto widen = [](uint32_t colour) {
		const uint32_t r = (colour >> 11) & 0x1F;
		const uint32_t g = (colour >> 5) & 0x3F;
		const uint32_t b = colour & 0x1F;
		return std::array<uint32_t, 3> {(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)};
	};
	std::vector<uint8_t> texels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
	for (size_t by = 0; by < blocksHigh; ++by)
	{
		for (size_t bx = 0; bx < blocksWide; ++bx)
		{
			const auto block = data.subspan((by * blocksWide + bx) * 8, 8);
			const uint32_t colour0 = block[0] | (static_cast<uint32_t>(block[1]) << 8);
			const uint32_t colour1 = block[2] | (static_cast<uint32_t>(block[3]) << 8);
			const auto c0 = widen(colour0);
			const auto c1 = widen(colour1);
			std::array<std::array<uint32_t, 4>, 4> palette {};
			for (size_t k = 0; k < 3; ++k)
			{
				palette[0][k] = c0[k];
				palette[1][k] = c1[k];
				if (colour0 > colour1)
				{
					palette[2][k] = (2 * c0[k] + c1[k] + 1) / 3;
					palette[3][k] = (c0[k] + 2 * c1[k] + 1) / 3;
				}
				else
				{
					palette[2][k] = (c0[k] + c1[k] + 1) / 2;
					palette[3][k] = 0;
				}
			}
			palette[0][3] = 255;
			palette[1][3] = 255;
			palette[2][3] = 255;
			palette[3][3] = colour0 > colour1 ? 255 : 0;
			for (uint32_t i = 0; i < 16; ++i)
			{
				const size_t x = bx * 4 + i % 4;
				const size_t y = by * 4 + i / 4;
				if (x >= static_cast<size_t>(width) || y >= static_cast<size_t>(height))
				{
					continue;
				}
				const uint32_t code = (block[4 + i / 4] >> ((i % 4) * 2)) & 3;
				auto* texel = &texels[(y * static_cast<size_t>(width) + x) * 4];
				for (size_t k = 0; k < 4; ++k)
				{
					texel[k] = static_cast<uint8_t>(palette[code][k]);
				}
			}
		}
	}
	return texels;
}

} // namespace openblack::dxt_alpha
