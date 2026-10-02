/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BlockTexture.h"

#include <algorithm>

#include <LNDFile.h>

#include "3D/CoastAlpha.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Graphics/Argb4444.h"

using namespace openblack;

namespace
{
/// Bit 1 of the cell's flags byte: an open sea cell, not drawn, its texels cleared (0x8739F8)
constexpr uint8_t k_NotDrawnFlag = 0x02;
constexpr size_t k_MaterialTexels = static_cast<size_t>(lnd::LNDMaterial::k_Width) * lnd::LNDMaterial::k_Height;

uint16_t MaterialTexel(const block_texture::Materials& materials, uint32_t material, size_t texelIndex)
{
	if (material >= materials.count || texelIndex >= k_MaterialTexels)
	{
		return 0;
	}
	return materials.texels[material * k_MaterialTexels + texelIndex];
}
} // namespace

uint16_t block_texture::CountryTexel(const lnd::LNDCountry& country, int32_t h, uint8_t noise, uint8_t bump,
                                     const Materials& materials, size_t texelIndex)
{
	if (h < 0x100)
	{
		return 0; // 0x87339C: the whole texel is 0 (black and transparent)
	}
	// 0x8733BF: the entry of the table (country + 4 + 12 mat: indices[0], indices[1], coefficient)
	const auto mat = static_cast<size_t>(std::min((h >> 8) + static_cast<int32_t>(noise), 255));
	const auto& entry = country.materials.at(mat);
	const uint32_t b = bump;
	const uint32_t c0 = MaterialTexel(materials, entry.indices[0], texelIndex);
	uint32_t red;
	uint32_t green;
	uint32_t blue;
	if (entry.indices[0] == entry.indices[1])
	{
		// 0x873403: one material, (c x bump) with the 5-bit channel into the top 4 bits
		red = ((c0 & 0x7C00u) * b) >> 10;
		green = ((c0 & 0x3E0u) * b) >> 9;
		blue = ((c0 & 0x1Fu) * b) >> 8;
	}
	else
	{
		// 0x873438: indices[0] x coefficient + indices[1] x (256 - coefficient), 32-bit products like the imul
		const uint32_t c1 = MaterialTexel(materials, entry.indices[1], texelIndex);
		const uint32_t k0 = entry.coefficient;
		const uint32_t k1 = 0x100u - k0;
		red = (((c0 & 0x7C00u) * k0 + (c1 & 0x7C00u) * k1) * b) >> 18;
		green = (((c0 & 0x3E0u) * k0 + (c1 & 0x3E0u) * k1) * b) >> 17;
		blue = (((c0 & 0x1Fu) * k0 + (c1 & 0x1Fu) * k1) * b) >> 16;
	}
	// 0x8734AF..0x8734EA: clamped at 15 in each channel's nibble
	red = red > 0xF00u ? 0xF00u : (red & 0xF00u);
	green = green > 0xF0u ? 0xF0u : (green & 0xF0u);
	blue = blue > 0xFu ? 0xFu : (blue & 0xFu);
	const uint32_t alpha = static_cast<uint32_t>(coast::CoastAlphaNibble(h, noise)) << 12;
	return static_cast<uint16_t>(alpha | red | green | blue);
}

uint16_t block_texture::BlendCorners(const std::array<uint16_t, 4>& texels, const std::array<uint8_t, 4>& weights)
{
	// fn_00871850 0x8718EC..0x871A36: per channel mask, sum(texel & mask x w) / 255 (mul 0x80808081, shr 7), & mask
	uint32_t out = 0;
	for (const uint32_t mask : {0xF00u, 0xF0u, 0xFu, 0xF000u})
	{
		uint32_t sum = 0;
		for (size_t k = 0; k < texels.size(); ++k)
		{
			sum += (texels.at(k) & mask) * weights.at(k);
		}
		out |= (sum / 255u) & mask;
	}
	return static_cast<uint16_t>(out);
}

std::vector<uint8_t> block_texture::BuildIslandBlockTexture(const LandIslandInterface& island, glm::u16vec2 extentIndexMin,
                                                            glm::u16vec2 indexSize, uint16_t texelsPerBlock,
                                                            std::span<const uint8_t> noise, std::span<const uint8_t> bump,
                                                            const Materials& materials)
{
	const size_t width = static_cast<size_t>(indexSize.x) * texelsPerBlock;
	const size_t height = static_cast<size_t>(indexSize.y) * texelsPerBlock;
	std::vector<uint8_t> texels(width * height * 4, 0);
	constexpr size_t k_MapSize = static_cast<size_t>(coast::k_TexelsPerBlock) * coast::k_TexelsPerBlock;
	const auto& countries = island.GetCountries();
	const auto& weights = coast::GetConeWeights();
	for (const auto& block : island.GetBlocks())
	{
		const auto* cells = block.GetCells();
		const auto position = block.GetBlockPosition() - glm::ivec2(extentIndexMin);
		if (cells == nullptr || position.x < 0 || position.y < 0 || position.x >= indexSize.x || position.y >= indexSize.y)
		{
			continue;
		}
		for (int tx = 0; tx < texelsPerBlock; ++tx)
		{
			// the original's texel under this one (the same one with 256 texels per block)
			const int ox = tx * coast::k_TexelsPerBlock / texelsPerBlock;
			const int cx = ox / coast::k_TexelsPerCell;
			const int i = ox % coast::k_TexelsPerCell;
			for (int tz = 0; tz < texelsPerBlock; ++tz)
			{
				const int oz = tz * coast::k_TexelsPerBlock / texelsPerBlock;
				const int cz = oz / coast::k_TexelsPerCell;
				const int j = oz % coast::k_TexelsPerCell;
				// the block's own 17 x 17 cells: +1 along z, +17 along x; corners (x, z), (x, z + 1), (x + 1, z + 1),
				// (x + 1, z) like the weights
				const auto* base = &cells[cx * 17 + cz];
				const std::array<const lnd::LNDCell*, 4> corners = {&base[0], &base[1], &base[18], &base[17]};
				uint16_t texel = 0;
				if ((base[0].flags & k_NotDrawnFlag) == 0 && !countries.empty())
				{
					const auto& w = weights.at(static_cast<size_t>(i * coast::k_TexelsPerCell + j));
					int32_t h = 0;
					for (size_t k = 0; k < corners.size(); ++k)
					{
						h += static_cast<int32_t>(w.at(k)) * island.GetCellAltitude(*corners.at(k));
					}
					const size_t index = static_cast<size_t>(ox) * coast::k_TexelsPerBlock + static_cast<size_t>(oz);
					const uint8_t n = noise.size() >= k_MapSize ? noise[index] : uint8_t {0};
					const uint8_t b = bump.size() >= k_MapSize ? bump[index] : uint8_t {0x80};
					const auto countryOf = [&countries](const lnd::LNDCell& cell) -> const lnd::LNDCountry& {
						return countries.at(std::min<size_t>(cell.properties.country, countries.size() - 1));
					};
					// 0x8737EA..0x873821: one build when the four corners share the country, else one per corner
					// country blended by fn_00871850 (the same result for equal ones, the weights add up to 255)
					const auto own = corners[0]->properties.country;
					const bool single = std::all_of(corners.begin(), corners.end(),
					                                [own](const lnd::LNDCell* c) { return c->properties.country == own; });
					if (single)
					{
						texel = CountryTexel(countryOf(*corners[0]), h, n, b, materials, index);
					}
					else
					{
						std::array<uint16_t, 4> built {};
						for (size_t k = 0; k < corners.size(); ++k)
						{
							built.at(k) = CountryTexel(countryOf(*corners.at(k)), h, n, b, materials, index);
						}
						texel = BlendCorners(built, w);
					}
				}
				const size_t row = static_cast<size_t>(position.y) * texelsPerBlock + static_cast<size_t>(tz);
				const size_t column = static_cast<size_t>(position.x) * texelsPerBlock + static_cast<size_t>(tx);
				std::ranges::copy(graphics::argb4444::Unpack(texel), &texels[(row * width + column) * 4]);
			}
		}
	}
	return texels;
}
