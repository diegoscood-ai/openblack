/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TextureUpscale.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace openblack::graphics
{
namespace
{
constexpr int k_Taps = 6; // Lanczos-3 at a 2x magnification: 6 source texels per output texel

float Lanczos3(float x)
{
	constexpr float k_Pi = 3.14159265358979f;
	if (x == 0.0f)
	{
		return 1.0f;
	}
	if (std::abs(x) >= 3.0f)
	{
		return 0.0f;
	}
	const float px = k_Pi * x;
	return 3.0f * std::sin(px) * std::sin(px / 3.0f) / (px * px);
}

/// Output texel i of a 2x upscale samples the source at (i + 0.5) / 2 - 0.5; the two phases (even / odd outputs) have
/// fixed normalised weights over the source texels floor(pos) - 2 .. floor(pos) + 3
struct Phase
{
	std::array<float, k_Taps> weights {};
	int firstOffset {0};
};

std::array<Phase, 2> MakePhases()
{
	std::array<Phase, 2> phases;
	for (int parity = 0; parity < 2; ++parity)
	{
		const float position = (static_cast<float>(parity) + 0.5f) / 2.0f - 0.5f; // relative to source texel i / 2
		const int base = static_cast<int>(std::floor(position));
		float sum = 0.0f;
		for (int tap = 0; tap < k_Taps; ++tap)
		{
			const int offset = base - 2 + tap;
			phases[parity].weights[tap] = Lanczos3(position - static_cast<float>(offset));
			sum += phases[parity].weights[tap];
		}
		for (auto& weight : phases[parity].weights)
		{
			weight /= sum;
		}
		phases[parity].firstOffset = base - 2;
	}
	return phases;
}

/// One pass along rows (horizontal = true) or columns, from float RGBA to float RGBA
void Pass(const std::vector<float>& src, int srcW, int srcH, std::vector<float>& dst, bool horizontal)
{
	static const auto k_Phases = MakePhases();
	const int dstW = horizontal ? srcW * 2 : srcW;
	const int dstH = horizontal ? srcH : srcH * 2;
	dst.assign(static_cast<size_t>(dstW) * dstH * 4, 0.0f);
	for (int y = 0; y < dstH; ++y)
	{
		for (int x = 0; x < dstW; ++x)
		{
			const int i = horizontal ? x : y;
			const auto& phase = k_Phases[i & 1];
			const int centre = i >> 1;
			std::array<float, 4> accumulated {};
			for (int tap = 0; tap < k_Taps; ++tap)
			{
				const int s = std::clamp(centre + phase.firstOffset + tap, 0, (horizontal ? srcW : srcH) - 1);
				const int sx = horizontal ? s : x;
				const int sy = horizontal ? y : s;
				const float* texel = &src[(static_cast<size_t>(sy) * srcW + sx) * 4];
				for (int c = 0; c < 4; ++c)
				{
					accumulated[c] += texel[c] * phase.weights[tap];
				}
			}
			std::copy(accumulated.begin(), accumulated.end(), &dst[(static_cast<size_t>(y) * dstW + x) * 4]);
		}
	}
}
} // namespace

std::vector<uint8_t> UpscaleRgba8Lanczos2x(const uint8_t* src, uint16_t width, uint16_t height, uint16_t layers)
{
	const size_t srcLayer = static_cast<size_t>(width) * height * 4;
	const size_t dstLayer = srcLayer * 4;
	std::vector<uint8_t> result(dstLayer * layers);
	std::vector<float> source(srcLayer);
	std::vector<float> wide;
	std::vector<float> full;
	for (uint16_t layer = 0; layer < layers; ++layer)
	{
		std::transform(src + srcLayer * layer, src + srcLayer * (layer + 1), source.begin(),
		               [](uint8_t v) { return static_cast<float>(v); });
		Pass(source, width, height, wide, true);
		Pass(wide, width * 2, height, full, false);
		std::transform(full.begin(), full.end(), result.begin() + static_cast<std::ptrdiff_t>(dstLayer * layer),
		               [](float v) { return static_cast<uint8_t>(std::clamp(v + 0.5f, 0.0f, 255.0f)); });
	}
	return result;
}

} // namespace openblack::graphics
