/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// audio::lip_sync (src/Audio/LipSync.h): the spectrum of a window of a recording and the mouth weights it gives, on
// synthetic tones. The last two tests are ours, moved from test_voices.cpp: the FFT against a direct DFT, and the
// numbers of the analysis, the bands and the key's first steps from before the move to these names.

#include <cmath>
#include <complex>
#include <cstdint>

#include <algorithm>
#include <array>
#include <numbers>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/LipSync.h"

using namespace openblack::audio::lip_sync;

namespace
{
constexpr int k_Rate = 22050;

/// A tone of `hz` at `amplitude` (of full scale), `frames` samples long
std::vector<int16_t> Tone(float hz, float amplitude, int frames)
{
	std::vector<int16_t> samples(static_cast<size_t>(frames));
	for (int i = 0; i < frames; ++i)
	{
		const double phase = 2.0 * std::numbers::pi * static_cast<double>(hz) * i / k_Rate;
		samples[static_cast<size_t>(i)] = static_cast<int16_t>(std::lround(std::sin(phase) * amplitude * 32767.0));
	}
	return samples;
}

/// Runs the lip sync `calls` times at 60 frames a second in the middle of the recording
Key Settle(const std::vector<int16_t>& samples, int calls, Key key = {})
{
	std::vector<float> spectrum(2 * k_Window);
	for (int i = 0; i < calls; ++i)
	{
		UpdateKey(Settings {}, key, 1.0f / 60.0f, 0.5f, samples, static_cast<int>(samples.size()), k_Rate, spectrum);
	}
	return key;
}

size_t Largest(const std::array<float, 3>& w)
{
	return static_cast<size_t>(std::distance(w.begin(), std::ranges::max_element(w)));
}
} // namespace

TEST(LipSync, FftOfAnImpulseIsFlat)
{
	constexpr int n = 16;
	std::vector<float> data(2 * n, 0.0f);
	data[0] = 1.0f;
	Fft(data, n, true);
	for (int i = 0; i < n; ++i)
	{
		EXPECT_FLOAT_EQ(data[static_cast<size_t>(2 * i)], 1.0f);
		EXPECT_NEAR(data[static_cast<size_t>(2 * i + 1)], 0.0f, 1e-6f);
	}
}

TEST(LipSync, FftFindsACosine)
{
	constexpr int n = 64;
	constexpr int bin = 5;
	std::vector<float> data(2 * n, 0.0f);
	for (int i = 0; i < n; ++i)
	{
		data[static_cast<size_t>(2 * i)] = static_cast<float>(std::cos(2.0 * std::numbers::pi * bin * i / n));
	}
	Fft(data, n, true);
	for (int i = 0; i < n; ++i)
	{
		const float magnitude = std::hypot(data[static_cast<size_t>(2 * i)], data[static_cast<size_t>(2 * i + 1)]);
		const float expected = (i == bin || i == n - bin) ? n / 2.0f : 0.0f;
		EXPECT_NEAR(magnitude, expected, 1e-3f) << "bin " << i;
	}
}

TEST(LipSync, SpectrumPeaksAtTheTonesBin)
{
	// 86.13 Hz a bin at 22050 Hz over 512 samples: bin 20 is about 1723 Hz
	const auto samples = Tone(20.0f * k_Rate / k_Window, 0.5f, k_Window);
	std::vector<float> out(2 * k_Window);
	MagnitudeSpectrum(samples, out, k_Window);
	const auto half = std::span(out).first(k_Window / 2);
	EXPECT_EQ(std::distance(half.begin(), std::ranges::max_element(half)), 20);
	// Silence has no spectrum at all
	const std::vector<int16_t> silence(k_Window, 0);
	MagnitudeSpectrum(silence, out, k_Window);
	EXPECT_TRUE(std::ranges::all_of(std::span(out).first(k_Window), [](float v) { return v == 0.0f; }));
}

TEST(LipSync, BandLevelIsTheMeanOfItsBins)
{
	const std::vector<float> spectrum {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
	// 8 bins over 800 Hz: 200..500 Hz are bins 2, 3 and 4
	EXPECT_FLOAT_EQ(BandLevel(spectrum, 8, 800.0f, 200.0f, 500.0f), 4.0f);
	// The top is cut at the last bin
	EXPECT_FLOAT_EQ(BandLevel(spectrum, 8, 800.0f, 600.0f, 10000.0f), 7.5f);
	// No bins between: 0
	EXPECT_EQ(BandLevel(spectrum, 8, 800.0f, 210.0f, 290.0f), 0.0f);
	EXPECT_EQ(BandLevel(spectrum, 8, 800.0f, 900.0f, 1000.0f), 0.0f);
}

TEST(LipSync, EachBandDrivesItsOwnMouthShape)
{
	const int frames = k_Rate;
	EXPECT_EQ(Largest(Settle(Tone(300.0f, 0.5f, frames), 60).weights), 0u);
	EXPECT_EQ(Largest(Settle(Tone(550.0f, 0.5f, frames), 60).weights), 1u);
	EXPECT_EQ(Largest(Settle(Tone(3000.0f, 0.5f, frames), 60).weights), 2u);
	// A loud tone settles with its shape fully shown and the weights adding up to at most 1
	const Key key = Settle(Tone(550.0f, 0.5f, frames), 60);
	EXPECT_NEAR(key.weights[1], 1.0f, 0.05f);
	EXPECT_LE(key.weights[0] + key.weights[1] + key.weights[2], 1.0f + 1e-6f);
}

TEST(LipSync, WeightsRiseAndFallAtLimitedRates)
{
	const auto loud = Tone(550.0f, 0.5f, k_Rate);
	const float dt = 1.0f / 60.0f;
	const float fall = dt * 40.0f * 0.18f;
	// Rising: at most twice the fall a call
	Key key = Settle(loud, 1);
	EXPECT_NEAR(key.weights[1], 2.0f * fall, 1e-6f);
	key = Settle(loud, 60);
	// Quiet sound (under the silence threshold) brings the weights down by the fall a call
	const auto quiet = Tone(550.0f, 0.0005f, k_Rate);
	const float before = key.weights[1];
	key = Settle(quiet, 1, key);
	EXPECT_NEAR(key.weights[1], before - fall, 1e-5f);
	key = Settle(quiet, 60, key);
	EXPECT_EQ(key.weights[1], 0.0f);
}

TEST(LipSync, ExactSilenceShowsNoMouthShape)
{
	// All three bands at exactly 0: like the game, the weights are not numbers for that call, so none shows (w > 0
	// is false), and the next call with sound recovers them
	const std::vector<int16_t> silence(static_cast<size_t>(k_Rate), 0);
	Key key = Settle(Tone(550.0f, 0.5f, k_Rate), 10);
	key = Settle(silence, 1, key);
	EXPECT_FALSE(key.weights[0] > 0.0f);
	EXPECT_FALSE(key.weights[1] > 0.0f);
	EXPECT_FALSE(key.weights[2] > 0.0f);
	key = Settle(Tone(550.0f, 0.5f, k_Rate), 1, key);
	EXPECT_TRUE(std::isfinite(key.weights[1]));
	EXPECT_GT(key.weights[1], 0.0f);
}

TEST(LipSync, TheWindowStaysInsideTheRecording)
{
	// Past the end the last window is used, before the start the first
	std::vector<int16_t> samples(static_cast<size_t>(k_Window) * 4, 0);
	const auto tone = Tone(550.0f, 0.5f, k_Window);
	std::ranges::copy(tone, samples.end() - k_Window);
	std::vector<float> spectrum(2 * k_Window);
	Key key;
	UpdateKey(Settings {}, key, 1.0f / 60.0f, 100.0f, samples, static_cast<int>(samples.size()), k_Rate, spectrum);
	EXPECT_GT(key.weights[1], 0.0f);
	// A recording shorter than the window leaves the key as it was
	Key untouched;
	untouched.weights = {0.25f, 0.5f, 0.125f};
	const std::vector<int16_t> shortRecording(100, 1000);
	UpdateKey(Settings {}, untouched, 0.0f, 0.0f, shortRecording, 100, k_Rate, spectrum);
	EXPECT_EQ(untouched.weights[1], 0.5f);
}

TEST(LipSync, FftMatchesTheDft)
{
	constexpr int k_N = 16;
	std::array<float, 2 * k_N> data {};
	std::array<std::complex<double>, k_N> input {};
	for (int i = 0; i < k_N; ++i)
	{
		input[i] = {std::sin(i * 0.7) + 0.25 * i, std::cos(i * 1.3)};
		data[2 * i] = static_cast<float>(input[i].real());
		data[2 * i + 1] = static_cast<float>(input[i].imag());
	}
	Fft(data, k_N, true);
	for (int k = 0; k < k_N; ++k)
	{
		// Forward: sum x[n] e^(+2 pi i k n / N)
		std::complex<double> sum {};
		for (int n = 0; n < k_N; ++n)
		{
			sum += std::complex<double>(static_cast<float>(input[n].real()), static_cast<float>(input[n].imag())) *
			       std::polar(1.0, 2.0 * 3.14159265358979 * k * n / k_N);
		}
		EXPECT_NEAR(data[2 * k], sum.real(), 1e-4) << k;
		EXPECT_NEAR(data[2 * k + 1], sum.imag(), 1e-4) << k;
	}
}

TEST(LipSync, SpectrumBandsAndKeySteps)
{
	// a 300 Hz sine at 22050 Hz: the spectrum's peak is near bin 300 / 22050 * 512 = 7 (and its mirror)
	std::vector<int16_t> pcm(k_Window);
	for (int i = 0; i < k_Window; ++i)
	{
		pcm[i] = static_cast<int16_t>(16000.0 * std::sin(2.0 * 3.14159265358979 * 300.0 * i / 22050.0));
	}
	std::vector<float> spectrum(2 * k_Window);
	MagnitudeSpectrum(pcm, spectrum, k_Window);
	const auto peak = std::max_element(spectrum.begin(), spectrum.begin() + k_Window / 2) - spectrum.begin();
	EXPECT_EQ(peak, 7);
	// BandLevel: the mean of bins [trunc(200 / 22050 * 512), trunc(400 / 22050 * 512)) = [4, 9)
	float mean = 0.0f;
	for (int i = 4; i < 9; ++i)
	{
		mean += spectrum[i];
	}
	EXPECT_NEAR(BandLevel(spectrum, k_Window, 22050.0f, 200.0f, 400.0f), mean / 5.0f, 1e-6);
	EXPECT_FLOAT_EQ(BandLevel(spectrum, k_Window, 22050.0f, 400.0f, 400.0f), 0.0f);

	// UpdateKey: the first band is the loudest, its weight (index 0) goes up by at most 2 * dt * 40 * 0.18 a call
	Key key;
	std::vector<int16_t> sentence(22050);
	for (size_t i = 0; i < sentence.size(); ++i)
	{
		sentence[i] = static_cast<int16_t>(16000.0 * std::sin(2.0 * 3.14159265358979 * 300.0 * i / 22050.0));
	}
	std::vector<float> work(2 * k_Window);
	const auto frames = static_cast<int>(sentence.size());
	UpdateKey(Settings {}, key, 0.01f, 0.5f, sentence, frames, k_Rate, work, k_Window);
	const float up = 2.0f * static_cast<float>(0.01 * 40.0 * static_cast<double>(0.18f));
	EXPECT_NEAR(key.weights[0], up, 1e-6);
	EXPECT_LT(key.weights[1], key.weights[0]);
	EXPECT_LT(key.weights[2], key.weights[0]);
	// a long dt: the weights reach their shape (band 0 / band 0 = 1, x the level capped at 1), normalised to <= 1
	for (int i = 0; i < 20; ++i)
	{
		UpdateKey(Settings {}, key, 1.0f, 0.5f, sentence, frames, k_Rate, work, k_Window);
	}
	EXPECT_LE(key.weights[0] + key.weights[1] + key.weights[2], 1.0f + 1e-6f);
	EXPECT_GT(key.weights[0], 0.5f);
}
