/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TextureMipmaps.h"

#include <algorithm>

#include <bimg/bimg.h>
#include <bx/allocator.h>

namespace openblack::graphics
{
namespace
{
// Alpha test reference used to measure cut-out coverage: the usual threshold of TexturedChroma materials (0x96)
constexpr uint8_t k_CoverageReference = 0x96;
constexpr float k_BinaryAlphaFraction = 0.85f; // tree skins have ~5 % of in-between alpha texels

float Coverage(const uint8_t* rgba, uint32_t pixels, float scale)
{
	uint32_t covered = 0;
	for (uint32_t i = 0; i < pixels; ++i)
	{
		if (std::min(255.0f, rgba[i * 4 + 3] * scale) >= k_CoverageReference)
		{
			++covered;
		}
	}
	return static_cast<float>(covered) / static_cast<float>(pixels);
}

/// True when almost every texel is fully transparent or fully opaque and both kinds exist (a cut-out texture).
bool HasCutoutAlpha(const uint8_t* rgba, uint32_t pixels)
{
	uint32_t low = 0;
	uint32_t high = 0;
	for (uint32_t i = 0; i < pixels; ++i)
	{
		const auto a = rgba[i * 4 + 3];
		low += a < 32 ? 1 : 0;
		high += a > 223 ? 1 : 0;
	}
	return low > 0 && high > 0 && static_cast<float>(low + high) >= k_BinaryAlphaFraction * static_cast<float>(pixels);
}

void Downsample(const uint8_t* src, uint32_t srcW, uint32_t srcH, uint8_t* dst, uint32_t dstW, uint32_t dstH)
{
	for (uint32_t y = 0; y < dstH; ++y)
	{
		for (uint32_t x = 0; x < dstW; ++x)
		{
			float rgb[3] = {0.0f, 0.0f, 0.0f};
			float rgbWeighted[3] = {0.0f, 0.0f, 0.0f};
			float alpha = 0.0f;
			for (uint32_t dy = 0; dy < 2; ++dy)
			{
				for (uint32_t dx = 0; dx < 2; ++dx)
				{
					const auto sx = std::min(x * 2 + dx, srcW - 1);
					const auto sy = std::min(y * 2 + dy, srcH - 1);
					const uint8_t* p = &src[(sy * srcW + sx) * 4];
					const float a = p[3];
					for (int c = 0; c < 3; ++c)
					{
						rgb[c] += p[c];
						rgbWeighted[c] += p[c] * a;
					}
					alpha += a;
				}
			}
			uint8_t* out = &dst[(y * dstW + x) * 4];
			for (int c = 0; c < 3; ++c)
			{
				const float value = alpha > 0.0f ? rgbWeighted[c] / alpha : rgb[c] / 4.0f;
				out[c] = static_cast<uint8_t>(std::clamp(value + 0.5f, 0.0f, 255.0f));
			}
			out[3] = static_cast<uint8_t>(alpha / 4.0f + 0.5f);
		}
	}
}

/// Scales the level's alpha so that as many texels pass the alpha test as in the base level.
void PreserveCoverage(uint8_t* rgba, uint32_t pixels, float targetCoverage)
{
	float lo = 0.25f;
	float hi = 4.0f;
	for (int i = 0; i < 12; ++i)
	{
		const float mid = (lo + hi) * 0.5f;
		(Coverage(rgba, pixels, mid) < targetCoverage ? lo : hi) = mid;
	}
	const float scale = (lo + hi) * 0.5f;
	for (uint32_t i = 0; i < pixels; ++i)
	{
		rgba[i * 4 + 3] = static_cast<uint8_t>(std::min(255.0f, rgba[i * 4 + 3] * scale + 0.5f));
	}
}
} // namespace

std::vector<uint8_t> BuildRgba8MipChain(const void* src, uint16_t width, uint16_t height, uint16_t layers, int bimgFormat)
{
	const auto format = static_cast<bimg::TextureFormat::Enum>(bimgFormat);
	const uint32_t srcLayerSize = bimg::imageGetSize(nullptr, width, height, 1, false, false, 1, format);

	uint32_t chainSize = 0;
	for (uint32_t w = width, h = height;; w = std::max(1u, w >> 1), h = std::max(1u, h >> 1))
	{
		chainSize += w * h * 4;
		if (w == 1 && h == 1)
		{
			break;
		}
	}

	std::vector<uint8_t> result(chainSize * layers);
	bx::DefaultAllocator allocator;
	for (uint16_t layer = 0; layer < layers; ++layer)
	{
		uint8_t* level = &result[chainSize * layer];
		bimg::imageDecodeToRgba8(&allocator, level, static_cast<const uint8_t*>(src) + srcLayerSize * layer, width, height,
		                         width * 4, format);

		const uint32_t basePixels = static_cast<uint32_t>(width) * height;
		const bool cutout = HasCutoutAlpha(level, basePixels);
		const float baseCoverage = cutout ? Coverage(level, basePixels, 1.0f) : 0.0f;

		uint32_t w = width;
		uint32_t h = height;
		while (w > 1 || h > 1)
		{
			const uint32_t nextW = std::max(1u, w >> 1);
			const uint32_t nextH = std::max(1u, h >> 1);
			uint8_t* next = level + w * h * 4;
			Downsample(level, w, h, next, nextW, nextH);
			if (cutout)
			{
				PreserveCoverage(next, nextW * nextH, baseCoverage);
			}
			level = next;
			w = nextW;
			h = nextH;
		}
	}
	return result;
}

} // namespace openblack::graphics
