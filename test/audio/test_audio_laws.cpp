/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <array>
#include <limits>
#include <mutex>
#include <vector>

#include <entt/entity/registry.hpp>
#include <gtest/gtest.h>

#include "Audio/Engine/SamplePlay.h"
#include "Audio/Services/AtmosBanks.h"
#include "ECS/AudioQueries.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "Locator.h"

using namespace openblack::audio;

// The expected values come from an emulation of the original audio DLLs: the mixer's volume and distance laws and
// the conversion of a relative position to the mixer's axes.

TEST(AudioLaws, QMixerVolume)
{
	// floor(127 * v / 127) * 258 / 32767
	EXPECT_NEAR(sample_play::QMixerGain(127), 32766.0f / 32767.0f, 1e-6f);
	EXPECT_NEAR(sample_play::QMixerGain(60), 15480.0f / 32767.0f, 1e-6f);
	EXPECT_NEAR(sample_play::QMixerGain(20), 5160.0f / 32767.0f, 1e-6f);
	EXPECT_NEAR(sample_play::QMixerGain(1), 258.0f / 32767.0f, 1e-6f);
	EXPECT_EQ(sample_play::QMixerGain(0), 0.0f);
	EXPECT_NEAR(sample_play::QMixerGain(200), 32766.0f / 32767.0f, 1e-6f);
}

TEST(AudioLaws, QMixerDistance)
{
	EXPECT_NEAR(sample_play::DistanceGain(50, 160, 4, 0), 1.0f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(50, 160, 4, 50), 1.0f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(50, 160, 4, 60), 0.5555556f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(50, 160, 4, 100), 0.2f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(50, 160, 4, 160), 0.1020408f, 1e-6f);
	EXPECT_EQ(sample_play::DistanceGain(50, 160, 4, 161), 0.0f);
	EXPECT_NEAR(sample_play::DistanceGain(1, 9999, 2, 2.828427f), 0.2147372f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(1, 9999, 1, 5), 0.2f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(1, 9999, 0, 10), 1.0f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(60, 180, 0.3f, 200), 0.0f, 1e-6f);
}

TEST(AudioLaws, RelativeAxes)
{
	// LHaudio's relative (x, y, z) -> QMixer's (right, up, ahead)
	struct Case
	{
		glm::vec3 lh;
		glm::vec3 heard;
	};
	const Case cases[] = {
	    {{2, 2, 0}, {1.9992f, 0.0f, 2.0008f}},
	    {{-2, 1, 0}, {-1.9998f, 0.0f, 1.0005f}},
	    {{0, 4, 0}, {0, 0, 4}},
	    {{0, -4, 0}, {0, 0, -4}},
	    {{4, 0, 0}, {4, 0, 0}},
	    {{-4, 0, 0}, {-4, 0, 0}},
	    {{-5, -5, 0}, {-4.9980f, 0.0f, -5.0020f}},
	    {{1, -2, 0}, {0.9989f, 0.0f, -2.0006f}},
	    {{2, -1.5f, 0}, {2.2358f, 0.0f, -1.1186f}},
	    {{0, 0, 3}, {0, 3, 0}},
	    {{1, 1, 1}, {0.9994f, 1.0004f, 1.0002f}},
	};
	for (const auto& c : cases)
	{
		const auto p = sample_play::PolarRelative(c.lh);
		EXPECT_NEAR(p.x, c.heard.x, 2e-4f) << c.lh.x << "," << c.lh.y << "," << c.lh.z;
		EXPECT_NEAR(p.y, c.heard.y, 2e-4f) << c.lh.x << "," << c.lh.y << "," << c.lh.z;
		EXPECT_NEAR(p.z, c.heard.z, 2e-4f) << c.lh.x << "," << c.lh.y << "," << c.lh.z;
	}
}

TEST(AudioLaws, RelativeAxesExact)
{
	// both halves emulated with the original float code: the audio library's with the FPU at 24 bits as on the game
	// thread, the mixer's at 53 bits as on its timer thread, all digits. The doubles 180 * 0.31847133757961782,
	// the float angles and range, pi * 0.0055555557f and the float az / flat / up of the mixer.
	struct Case
	{
		glm::vec3 lh;
		glm::vec3 heard;
	};
	const Case cases[] = {
	    {{2, 2, 0}, {1.99920309f, 0.0f, 2.00079656f}},
	    {{-2, 1, 0}, {-1.99976468f, 0.0f, 1.00047064f}},
	    {{-5, -5, 0}, {-4.99800825f, 0.0f, -5.0019908f}},
	    {{1, -2, 0}, {0.998876214f, 0.0f, -2.00056148f}},
	    {{2, -1.5f, 0}, {2.2358048f, 0.0f, -1.11855996f}},
	    {{1, 1, 1}, {0.999380827f, 1.00044143f, 1.00017738f}},
	    {{123.25f, -48.5f, 7.75f}, {123.410568f, 7.75392675f, -48.0893211f}},
	    {{-300.5f, 210.25f, -15.5f}, {-300.269836f, -15.5078573f, 210.578003f}},
	    // |y| truncated to 0: the azimuth is 90
	    {{0.75f, -0.25f, 2.5f}, {0.788965642f, 2.50050664f, -3.4486785e-08f}},
	};
	for (const auto& c : cases)
	{
		const auto p = sample_play::PolarRelative(c.lh);
		const auto tolerance = [](float v) { return std::abs(v) * 2e-7f + 1e-6f; };
		EXPECT_NEAR(p.x, c.heard.x, tolerance(c.heard.x)) << c.lh.x << "," << c.lh.y << "," << c.lh.z;
		EXPECT_NEAR(p.y, c.heard.y, tolerance(c.heard.y)) << c.lh.x << "," << c.lh.y << "," << c.lh.z;
		EXPECT_NEAR(p.z, c.heard.z, tolerance(c.heard.z)) << c.lh.x << "," << c.lh.y << "," << c.lh.z;
	}
}

TEST(AudioLaws, AlignmentCurve)
{
	// x clamped to 0..1 (a NaN is 0), 2 - 2 (1 - x) - 1
	using openblack::ecs::audio_queries::AudioAlignmentValue;
	EXPECT_EQ(AudioAlignmentValue(0.0f), -1.0f);
	EXPECT_EQ(AudioAlignmentValue(0.5f), 0.0f);
	EXPECT_EQ(AudioAlignmentValue(1.0f), 1.0f);
	EXPECT_EQ(AudioAlignmentValue(0.25f), -0.5f);
	EXPECT_EQ(AudioAlignmentValue(0.9f), 2.0f - ((1.0f - 0.9f) + (1.0f - 0.9f)) - 1.0f);
	EXPECT_EQ(AudioAlignmentValue(-3.0f), -1.0f);
	EXPECT_EQ(AudioAlignmentValue(2.0f), 1.0f);
	EXPECT_EQ(AudioAlignmentValue(std::numeric_limits<float>::quiet_NaN()), -1.0f);
}

TEST(AudioLaws, AtmosGroupByAlignment)
{
	// ProcessAtmosBanks: the float alignment against the double -0.59999999999999998; at or below it -> 2
	EXPECT_EQ(atmos_banks::GroupFor(-1.0f), 2u);
	EXPECT_EQ(atmos_banks::GroupFor(-0.6f), 2u); // the float -0.6 is -0.60000002384, below the double
	EXPECT_EQ(atmos_banks::GroupFor(std::nextafter(-0.6f, 0.0f)), 1u);
	EXPECT_EQ(atmos_banks::GroupFor(0.0f), 1u);
	EXPECT_EQ(atmos_banks::GroupFor(1.0f), 1u);
	EXPECT_EQ(atmos_banks::GroupFor(std::numeric_limits<float>::quiet_NaN()), 2u);
}

namespace
{
/// The audio modules' state, kept by the test
class FakeAudioState final: public openblack::ecs::systems::AudioStateInterface
{
protected:
	[[nodiscard]] entt::registry& Store() noexcept override { return _store; }
	[[nodiscard]] std::mutex& Mutex() noexcept override { return _mutex; }

private:
	entt::registry _store;
	std::mutex _mutex;
};
} // namespace

TEST(AudioLaws, AtmosViewWithoutAudioState)
{
	openblack::Locator::audioState::reset();
	const auto view = atmos_banks::GetView();
	EXPECT_FALSE(view.initialised);
	EXPECT_EQ(view.counter, 0u);
	EXPECT_TRUE(view.loops.empty());
	EXPECT_TRUE(view.queue.empty());
	EXPECT_TRUE(view.playing.empty());
}

TEST(AudioLaws, AtmosViewOfUnregisteredBanks)
{
	// before the first game turn of the audio: no bank registered and every volume 0
	openblack::Locator::audioState::reset();
	openblack::Locator::audioState::emplace<FakeAudioState>();
	const auto view = atmos_banks::GetView();
	EXPECT_FALSE(view.initialised);
	EXPECT_EQ(view.counter, 0u);
	for (const auto& bank : view.banks)
	{
		EXPECT_FALSE(bank.registered);
		EXPECT_EQ(bank.group, 0u);
		EXPECT_EQ(bank.volume, 0);
		EXPECT_EQ(bank.target, 0.0f);
		EXPECT_EQ(bank.current, 0.0f);
	}
	EXPECT_TRUE(view.loops.empty());
	EXPECT_TRUE(view.queue.empty());
	EXPECT_TRUE(view.playing.empty());
	openblack::Locator::audioState::reset();
}

namespace
{
/// A state shaped as the ambient banks' own, with a value of its own in every field
struct FakeAtmosState
{
	struct Bank
	{
		bool registered;
		uint32_t group;
		int32_t volume;
	};
	struct Loop
	{
		size_t bank;
		entt::id_type sample;
		uint32_t group;
		int32_t volume;
		int32_t fade;
		bool playing;
		uint32_t channel; ///< not shown
	};
	struct Loose
	{
		size_t bank;
		entt::id_type sample;
		uint32_t group;
		int32_t volume; ///< not shown
		int32_t frequency;
		uint32_t next;
	};
	struct LooseChannel
	{
		uint32_t emitter; ///< not shown
		size_t bank;
		uint32_t group;
		int32_t volume;
	};

	bool initialised {true};
	std::array<Bank, k_AtmosTypeCount> banks {};
	std::array<float, k_AtmosTypeCount> target {};
	std::array<float, k_AtmosTypeCount> current {};
	std::vector<Loop> loops;
	std::vector<Loose> queue;
	std::vector<LooseChannel> channels;
	uint32_t counter {4321};
};
} // namespace

TEST(AudioLaws, AtmosViewCopiesEveryField)
{
	FakeAtmosState state;
	for (size_t i = 0; i < k_AtmosTypeCount; ++i)
	{
		const auto n = static_cast<int32_t>(i);
		state.banks.at(i) = {.registered = i % 2 == 1, .group = static_cast<uint32_t>(1 + (i % 2)), .volume = 10 + n};
		state.target.at(i) = 0.25f + 0.01f * static_cast<float>(n);
		state.current.at(i) = 0.5f + 0.01f * static_cast<float>(n);
	}
	state.loops = {
	    {.bank = 3, .sample = 31, .group = 2, .volume = 90, .fade = 45, .playing = true, .channel = 7},
	    {.bank = 5, .sample = 52, .group = 0, .volume = 127, .fade = 0, .playing = false, .channel = 0},
	};
	state.queue = {
	    {.bank = 4, .sample = 41, .group = 1, .volume = 60, .frequency = 8, .next = 4400},
	    {.bank = 11, .sample = 112, .group = 2, .volume = 70, .frequency = 3, .next = 4500},
	};
	state.channels = {{.emitter = 9, .bank = 6, .group = 1, .volume = 33}};

	const auto view = atmos_banks::MakeView(state);
	EXPECT_TRUE(view.initialised);
	EXPECT_EQ(view.counter, 4321u);
	for (size_t i = 0; i < k_AtmosTypeCount; ++i)
	{
		const auto& bank = view.banks.at(i);
		EXPECT_EQ(bank.registered, i % 2 == 1) << i;
		EXPECT_EQ(bank.group, 1 + (i % 2)) << i;
		EXPECT_EQ(bank.volume, 10 + static_cast<int32_t>(i)) << i;
		EXPECT_EQ(bank.target, state.target.at(i)) << i;
		EXPECT_EQ(bank.current, state.current.at(i)) << i;
	}
	ASSERT_EQ(view.loops.size(), 2u);
	EXPECT_EQ(view.loops[0].bank, 3u);
	EXPECT_EQ(view.loops[0].sample, 31u);
	EXPECT_EQ(view.loops[0].group, 2u);
	EXPECT_EQ(view.loops[0].volume, 90);
	EXPECT_EQ(view.loops[0].fade, 45);
	EXPECT_TRUE(view.loops[0].playing);
	EXPECT_EQ(view.loops[1].bank, 5u);
	EXPECT_EQ(view.loops[1].sample, 52u);
	EXPECT_EQ(view.loops[1].group, 0u);
	EXPECT_EQ(view.loops[1].volume, 127);
	EXPECT_EQ(view.loops[1].fade, 0);
	EXPECT_FALSE(view.loops[1].playing);
	// the queue in its own order
	ASSERT_EQ(view.queue.size(), 2u);
	EXPECT_EQ(view.queue[0].bank, 4u);
	EXPECT_EQ(view.queue[0].sample, 41u);
	EXPECT_EQ(view.queue[0].group, 1u);
	EXPECT_EQ(view.queue[0].frequency, 8);
	EXPECT_EQ(view.queue[0].next, 4400u);
	EXPECT_EQ(view.queue[1].bank, 11u);
	EXPECT_EQ(view.queue[1].sample, 112u);
	EXPECT_EQ(view.queue[1].group, 2u);
	EXPECT_EQ(view.queue[1].frequency, 3);
	EXPECT_EQ(view.queue[1].next, 4500u);
	ASSERT_EQ(view.playing.size(), 1u);
	EXPECT_EQ(view.playing[0].bank, 6u);
	EXPECT_EQ(view.playing[0].group, 1u);
	EXPECT_EQ(view.playing[0].volume, 33);
}
