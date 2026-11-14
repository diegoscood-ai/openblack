/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CoastAlpha.h"

#include <cmath>

#include <algorithm>

using namespace openblack;

namespace
{
/// The alpha table (the same in the normal and SSE paths): 16 dwords already shifted to the ARGB4444 alpha (0x1000 per
/// step), kept here as the nibble
constexpr std::array<uint8_t, 16> k_AlphaTable = {0, 0, 0, 1, 2, 4, 6, 8, 9, 10, 10, 11, 12, 13, 14, 15};

coast::ConeWeights BuildConeWeights()
{
	coast::ConeWeights weights {};
	for (int i = 0; i < coast::k_TexelsPerCell; ++i)
	{
		for (int j = 0; j < coast::k_TexelsPerCell; ++j)
		{
			const auto distance = [](int a, int b) { return std::sqrt(static_cast<double>(a * a + b * b)); };
			const std::array<double, 4> d = {distance(i, j), distance(i, 16 - j), distance(16 - i, 16 - j),
			                                 distance(16 - i, j)};
			std::array<double, 4> w {};
			double sum = 0.0;
			for (size_t k = 0; k < w.size(); ++k)
			{
				w[k] = d[k] > 14.0 ? 0.0 : 14.0 - d[k];
				sum += w[k];
			}
			// 255 / sum, truncated; then +1 to the largest until they add up to 255, the *last* of equals
			// (the original's table, read back from an emulated run: e.g. texel (8, 8) = 63, 63, 63, 66)
			const double scale = 255.0 / sum;
			std::array<int, 4> q {};
			int total = 0;
			for (size_t k = 0; k < q.size(); ++k)
			{
				q[k] = static_cast<int>(scale * w[k]);
				total += q[k];
			}
			while (total < 255)
			{
				size_t largest = 0;
				for (size_t k = 1; k < q.size(); ++k)
				{
					if (q[k] >= q[largest])
					{
						largest = k;
					}
				}
				++q[largest];
				++total;
			}
			auto& out = weights.at(static_cast<size_t>(i * coast::k_TexelsPerCell + j));
			for (size_t k = 0; k < q.size(); ++k)
			{
				out.at(k) = static_cast<uint8_t>(q[k]);
			}
		}
	}
	return weights;
}
} // namespace

const coast::ConeWeights& coast::GetConeWeights()
{
	static const ConeWeights k_Weights = BuildConeWeights();
	return k_Weights;
}

uint8_t coast::CoastAlphaNibble(int32_t h, uint8_t noise)
{
	if (h < 0x100)
	{
		return 0; // the whole texel is 0 (black and transparent; SSE: the whole group below 0x100)
	}
	if (h >= 0x400)
	{
		return 15; // alpha 0xF000 (SSE: h > 0x400, the same nibble at 0x400 through the table)
	}
	const int32_t e = h + 4 * static_cast<int8_t>(noise);
	if (e < 0x200)
	{
		return k_AlphaTable.front();
	}
	if (e > 0x3B6)
	{
		return k_AlphaTable.back();
	}
	// C division truncates toward zero, like the original's idiv
	const int32_t index = std::min(15, 15 + (15 * e - 14250) / 438);
	return k_AlphaTable.at(static_cast<size_t>(std::max(0, index)));
}
