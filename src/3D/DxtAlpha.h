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

#include <span>
#include <vector>

/// The alpha of the mesh pack's compressed (DXT) textures, as the game reads it back once they are converted to ARGB
/// 4444: the hand's pick of a tree tests it (docs/bw1-notes/hand-and-interface.md, "A tree under the cursor")
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

} // namespace openblack::dxt_alpha
