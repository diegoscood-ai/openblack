/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LipSync.h"

#include <cmath>

#include <algorithm>
#include <utility>

namespace openblack::audio::lip_sync
{

void Fft(std::span<float> data, int n, bool forward)
{
	const int isign = forward ? 1 : -1;
	// data is used 1-based
	float* d = data.data() - 1;
	const int size = n << 1;
	int j = 1;
	for (int i = 1; i < size; i += 2) // Bit reversal
	{
		if (j > i)
		{
			std::swap(d[j], d[i]);
			std::swap(d[j + 1], d[i + 1]);
		}
		int m = n; // size >> 1
		while (m >= 2 && j > m)
		{
			j -= m;
			m >>= 1;
		}
		j += m;
	}
	// The game runs at single precision: every add / sub / mul / div rounds to a float (R), the double constants are
	// exact and the sine keeps the full precision (wpi stays unrounded)
	const auto R = [](double v) { return static_cast<float>(v); };
	int mmax = 2;
	while (size > mmax) // Danielson-Lanczos
	{
		const int istep = mmax << 1;
		const float theta = R(6.2831853071795898 / static_cast<double>(mmax * isign)); // 2 pi / (mmax * isign)
		const double half = std::sin(static_cast<double>(R(theta * 0.5)));
		const float wpr = R(static_cast<double>(R(half * half)) * -2.0);
		const double wpi = std::sin(static_cast<double>(theta)); // Sine of the stored double
		float wr = 1.0f;
		float wi = 0.0f;
		for (int m = 1; m < mmax; m += 2)
		{
			for (int i = m; i <= size; i += istep)
			{
				const int k = i + mmax;
				const float tempr = d[k] * wr - d[k + 1] * wi;
				const float tempi = d[k] * wi + d[k + 1] * wr; // Stored as a float
				d[k] = d[i] - tempr;
				d[k + 1] = d[i + 1] - tempi;
				d[i] = tempr + d[i];
				d[i + 1] = tempi + d[i + 1];
			}
			const float wrOld = wr;
			// wr + (wr * wpr - wi * wpi); wi * (wpr + 1) + wpi * wr (the old wr)
			wr = (wrOld * wpr - R(static_cast<double>(wi) * wpi)) + wrOld;
			wi = wi * (wpr + 1.0f) + R(wpi * static_cast<double>(wrOld));
		}
		mmax = istep;
	}
}

void MagnitudeSpectrum(std::span<const int16_t> samples, std::span<float> out, int n)
{
	// The window's step 1 / (n * 32768) as a float; the window itself at single precision, so a float
	const float step = 1.0f / (static_cast<float>(n) * 32768.0f);
	float w = 0.0f;
	const int half = n / 2;
	int i = 0;
	for (; i < half; ++i) // Rising half
	{
		w += step;
		out[2 * i] = static_cast<float>(samples[i]) * w;
		out[2 * i + 1] = 0.0f;
	}
	for (; i < n; ++i) // Falling half
	{
		w -= step;
		out[2 * i] = static_cast<float>(samples[i]) * w;
		out[2 * i + 1] = 0.0f;
	}
	Fft(out.first(2 * static_cast<size_t>(n)), n, true);
	const auto count = static_cast<float>(n);
	for (i = 0; i < n; ++i) // In place, the magnitudes packed at the front
	{
		const float re = out[2 * i];
		const float im = out[2 * i + 1];
		out[i] = std::sqrt((re * re + im * im) / count);
	}
}

float BandLevel(std::span<const float> spectrum, int n, float rate, float low, float high)
{
	// Truncation of (low / rate) * n and (high / rate) * n, each kept at most n; the steps are float ones (n converts
	// exactly)
	int from = static_cast<int>(low / rate * static_cast<float>(n));
	int to = static_cast<int>(high / rate * static_cast<float>(n));
	from = std::min(from, n);
	to = std::min(to, n);
	if (from >= to)
	{
		return 0.0f;
	}
	float sum = 0.0f;
	for (int i = from; i < to; ++i)
	{
		sum += spectrum[i];
	}
	return static_cast<float>(static_cast<double>(sum) / (to - from)); // Rounded to a float
}

Levels UpdateKey(const Settings& settings, Key& key, float dt, float time, std::span<const int16_t> samples, int frames,
                 int sampleRate, std::span<float> spectrum, int window)
{
	Levels levels;
	const auto rate = static_cast<float>(sampleRate);
	// The centre sample, ((offsetMs * 0.001f) + dt + time) * rate in float steps
	const auto centre = static_cast<int>((settings.offsetMs * 0.001f + dt + time) * rate);
	int start = centre - window / 2;
	start = std::max(start, 0);
	start = std::min(start, frames - window);
	// (defensive, not in the game: a recording shorter than the window would be read before its samples)
	if (start < 0 || frames < window || static_cast<size_t>(start) + static_cast<size_t>(window) > samples.size())
	{
		return levels;
	}
	MagnitudeSpectrum(samples.subspan(static_cast<size_t>(start), static_cast<size_t>(window)), spectrum, window);
	float total = 0.0f;
	for (size_t k = 0; k < settings.bands.size(); ++k)
	{
		const auto& band = settings.bands[k];
		levels.bands[k] = BandLevel(spectrum, window, rate, band.low, band.high) * band.gain;
		total += levels.bands[k];
	}
	if (total < settings.threshold)
	{
		total = 0.0f;
	}
	levels.total = total;
	const float squared = total * total;
	if (squared != 0.0f)
	{
		for (auto& level : levels.bands)
		{
			level = level * level / squared;
		}
	}
	// The loudest band, the last one on a tie
	size_t loudest = 2;
	const auto& b = levels.bands;
	if (b[0] > b[1] && b[0] > b[2])
	{
		loudest = 0;
	}
	else if (b[1] > b[2] && b[1] > b[0])
	{
		loudest = 1;
	}
	float level = settings.level * levels.total;
	if (level > 1.0f)
	{
		level = 1.0f;
	}
	float sum = 0.0f;
	for (size_t k = 0; k < settings.bands.size(); ++k)
	{
		auto& weight = key.weights[static_cast<size_t>(settings.bands[k].weight)];
		// In float steps: the target and the sum stay unrounded in the game, the limit is stored
		const float ratio = b[k] / b[loudest];
		float v = ratio * ratio * level;
		const float limit = dt * settings.rate * 0.18f;
		const float previous = weight;
		if (previous - v > limit) // Falls at most by limit
		{
			v = previous - limit;
		}
		const float limitUp = limit + limit;
		if (v - previous > limitUp) // Rises at most by twice that
		{
			v = limitUp + previous;
		}
		weight = v;
		sum += weight;
	}
	if (sum > 1.0f)
	{
		const float scale = 1.0f / sum;
		for (const auto& band : settings.bands)
		{
			key.weights[static_cast<size_t>(band.weight)] *= scale;
		}
	}
	return levels;
}

} // namespace openblack::audio::lip_sync
