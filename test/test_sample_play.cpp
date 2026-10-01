/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>

#include <algorithm>
#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include <PackFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Audio/Audio.h"
#include "Audio/GameQueries.h"
#include "Audio/QMixerLaws.h"
#include "Audio/SampleOutput.h"
#include "Audio/Sound.h"
#include "Audio/WaveBuffers.h"
#include "Resources/Loaders.h"

// Milestones B0 / B1 of dev\tmp_dis\audio\PLAN.md: LHaudio's 16 channels (LHSamplePlay 0x100113B0, allocation
// 0x10011020, priority steal 0x100112CA), GAudio's options variant 0x429E30, the master volume 0x100150E0, the finite
// loops, GAudio::Reset 0x426CA0 leaving no OpenAL source, and the decoding of the RIFF MPEG layer II waves. A fake
// output stands for QMixer. The installation tests need OPENBLACK_TEST_BW_ROOT; without it they skip.

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// QMixer as the tests see it: a channel plays until stopped or Finish()ed
class FakeOutput final: public SampleOutput
{
public:
	std::array<bool, 16> playing {};
	std::array<bool, 16> made {};
	std::array<bool, 16> released {};
	std::array<float, 16> gain {};
	std::array<Start, 16> starts {};
	int plays {0};

	bool Play(size_t channel, Sound&, const Start& start) override
	{
		playing[channel] = true;
		made[channel] = true;
		released[channel] = false;
		gain[channel] = start.gain;
		starts[channel] = start;
		++plays;
		return true;
	}
	void Stop(size_t channel) override { playing[channel] = false; }
	[[nodiscard]] bool Playing(size_t channel) const override { return playing[channel]; }
	void SetGain(size_t channel, float g) override { gain[channel] = g; }
	void SetPitch(size_t, float) override {}
	void SetPosition(size_t, glm::vec3) override {}
	void ReleaseLoop(size_t channel) override { released[channel] = true; }
	void SetListener(glm::vec3) override {}
	void Update() override {}
	[[nodiscard]] size_t Sources() const override
	{
		size_t n = 0;
		for (const bool m : made)
		{
			n += m ? 1 : 0;
		}
		return n;
	}
	void DeleteAll() override
	{
		made.fill(false);
		playing.fill(false);
	}
	void Finish(size_t channel) { playing[channel] = false; }
};

std::optional<std::filesystem::path> GameRoot()
{
	const char* root = std::getenv("OPENBLACK_TEST_BW_ROOT");
	if (root == nullptr || *root == '\0' || !std::filesystem::is_directory(root))
	{
		return std::nullopt;
	}
	return std::filesystem::path(root);
}

class SamplePlayTest: public ::testing::Test
{
protected:
	FakeOutput output;
	std::map<entt::id_type, Sound> sounds;

	void SetUp() override
	{
		audio::Init(GameQueries {});
		sample_play::Backend backend;
		backend.output = &output;
		backend.sound = [this](entt::id_type id) -> Sound* {
			const auto found = sounds.find(id);
			return found != sounds.end() ? &found->second : nullptr;
		};
		backend.rand = []() { return 16383; };
		backend.camera = []() -> std::optional<glm::vec3> { return glm::vec3(0.0f); };
		sample_play::SetBackend(std::move(backend));
		sample_play::SetMasterVolume(127);
		audio::ClearMap();
		output = FakeOutput {};
	}
	void TearDown() override
	{
		audio::ClearMap();
		sample_play::SetBackend({});
		audio::Shutdown();
	}

	/// A plain sample of the default options (no .sad overrides): mode 3, its priority
	entt::id_type Add(uint16_t bank, int number, int priority, int group = 0)
	{
		const auto id = entt::hashed_string(fmt::format("test{}.sad/{}", bank, number).c_str()).value();
		Sound sound;
		sound.name = fmt::format("test {} {}", bank, number);
		sound.id = number;
		sound.bank = bank;
		sound.priority = priority;
		sound.sampleRate = 22050;
		sound.pitch = 100;
		sound.pitchDeviation = 0;
		sound.cloneGroup = group;
		sounds.emplace(id, std::move(sound));
		return id;
	}

	static Channel Start(entt::id_type id, int mode, Owner owner = {})
	{
		sample_play::Options options;
		options.sound = id;
		options.mode = mode;
		options.owner = owner;
		return sample_play::Start(options);
	}

	/// The real records of a bank of the installation (the wave bytes too when `withData`)
	bool LoadBank(const std::string& relative, uint16_t bank, bool withData)
	{
		const auto root = GameRoot();
		if (!root)
		{
			return false;
		}
		pack::PackFile pack;
		if (pack.Open(*root / relative) != pack::PackResult::Success)
		{
			return false;
		}
		const auto& headers = pack.GetAudioSampleHeaders();
		const auto& data = pack.GetAudioSamplesData();
		for (size_t i = 0; i < headers.size(); ++i)
		{
			std::vector<std::vector<uint8_t>> buffer;
			if (withData)
			{
				buffer.push_back(data[i]);
			}
			const auto sound = resources::SoundLoader {}(resources::SoundLoader::FromBufferTag {}, headers[i], buffer);
			sound->bank = bank;
			const auto id = entt::hashed_string(fmt::format("bank{}/{}", bank, headers[i].id).c_str()).value();
			sounds.emplace(id, *sound);
		}
		return true;
	}
	static entt::id_type Real(uint16_t bank, int number)
	{
		return entt::hashed_string(fmt::format("bank{}/{}", bank, number).c_str()).value();
	}
};
} // namespace

TEST_F(SamplePlayTest, SeventeenthDoesNotPlay)
{
	std::array<Channel, 16> handles {};
	for (int i = 0; i < 16; ++i)
	{
		handles[static_cast<size_t>(i)] = Start(Add(1, i + 1, 100), 1);
		EXPECT_NE(handles[static_cast<size_t>(i)], k_NoChannel) << i;
	}
	// 0x100112CA: no free channel and none of a lower priority
	EXPECT_EQ(Start(Add(1, 17, 100), 1), k_NoChannel);
	EXPECT_EQ(output.plays, 16);
	for (const auto handle : handles)
	{
		EXPECT_TRUE(sample_play::IsPlaying(handle));
	}
}

TEST_F(SamplePlayTest, PriorityStealsTheLowest)
{
	std::array<Channel, 16> handles {};
	for (int i = 0; i < 16; ++i)
	{
		handles[static_cast<size_t>(i)] = Start(Add(1, i + 1, i == 7 ? 50 : 100), 1);
	}
	const auto stealer = Start(Add(1, 17, 100), 1);
	ASSERT_NE(stealer, k_NoChannel);
	// the priority-50 one went, the others play on
	for (size_t i = 0; i < handles.size(); ++i)
	{
		EXPECT_EQ(sample_play::IsPlaying(handles[i]), i != 7) << i;
	}
	EXPECT_TRUE(sample_play::IsPlaying(stealer));
	// a finished channel is free again (+0x8C = 0)
	output.Finish(3);
	EXPECT_NE(Start(Add(1, 18, 1), 1), k_NoChannel);
}

TEST_F(SamplePlayTest, ModesTwoAndThree)
{
	const auto a = Add(1, 1, 100);
	const auto first = Start(a, 3, Owner::Key(5));
	const auto again = Start(a, 3, Owner::Key(5));
	// mode 3: the same channel restarted, the old start is gone
	EXPECT_NE(first, again);
	EXPECT_FALSE(sample_play::IsPlaying(first));
	EXPECT_TRUE(sample_play::IsPlaying(again));
	EXPECT_EQ((first - 1) % 16, (again - 1) % 16);
	// another owner: another channel
	const auto other = Start(a, 3, Owner::Key(6));
	EXPECT_NE((other - 1) % 16, (again - 1) % 16);
	// mode 2: the playing one is left alone
	const auto b = Add(1, 2, 100);
	const auto two = Start(b, 2);
	const int plays = output.plays;
	EXPECT_EQ(Start(b, 2), two);
	EXPECT_EQ(output.plays, plays);
}

TEST_F(SamplePlayTest, MasterVolume)
{
	const auto handle = Start(Add(1, 1, 100), 1);
	const auto index = (handle - 1) % 16;
	EXPECT_NEAR(output.gain[index], 32766.0f / 32767.0f, 1e-6f);
	// LHSampleSetMasterVolume 64 re-applies floor(127 * 64 / 127) * 258 / 32767 = 0.504
	sample_play::SetMasterVolume(64);
	EXPECT_NEAR(output.gain[index], 0.504f, 1e-3f);
	EXPECT_NEAR(output.gain[index], 16512.0f / 32767.0f, 1e-6f);
	// more than 127 is ignored
	sample_play::SetMasterVolume(200);
	EXPECT_EQ(sample_play::MasterVolume(), 64);
	EXPECT_NEAR(qmixer::Gain(127, 64), 16512.0f / 32767.0f, 1e-6f);
	sample_play::SetMasterVolume(127);
}

TEST_F(SamplePlayTest, ClearMapLeavesNoSource)
{
	for (int i = 0; i < 5; ++i)
	{
		Start(Add(1, i + 1, 100), 1);
	}
	EXPECT_EQ(output.Sources(), 5u);
	audio::ClearMap();
	EXPECT_EQ(output.Sources(), 0u);
}

TEST_F(SamplePlayTest, SwitchStopsAndStopNeedsActive)
{
	const auto a = Add(1, 1, 100);
	const auto handle = Start(a, 1);
	// LHWaveSwitch(0): StopAll and inactive; IsPlaying answers nothing while inactive (0x10013F69)
	sample_play::Switch(false);
	EXPECT_FALSE(sample_play::IsPlaying(handle));
	EXPECT_FALSE(sample_play::IsPlaying(a, Owner {}));
	sample_play::Switch(true);
	// an atmos channel survives StopAll (0x10012C13)
	sample_play::Options atmos;
	atmos.sound = Add(2, 1, 1);
	atmos.atmos = true;
	atmos.owner = Owner::AtmosMixer();
	const auto loop = sample_play::Start(atmos);
	sample_play::StopAll();
	EXPECT_TRUE(sample_play::IsPlaying(loop));
}

TEST_F(SamplePlayTest, SetVolumeNeedsActive)
{
	// LHSamplePlay does not test +0x14, so a channel can start while switched off; LHSampleSetVolume (0x10013412) leaves
	// it alone then, unless it is an atmos channel (LHSampleSetPitch 0x10013572 too)
	sample_play::Switch(false);
	const auto plain = Start(Add(1, 1, 100), 1);
	sample_play::Options options;
	options.sound = Add(2, 1, 1);
	options.atmos = true;
	options.owner = Owner::AtmosMixer();
	options.mode = 1;
	const auto atmos = sample_play::Start(options);
	ASSERT_NE(plain, k_NoChannel);
	ASSERT_NE(atmos, k_NoChannel);
	const auto plainIndex = (plain - 1) % 16;
	const auto atmosIndex = (atmos - 1) % 16;
	const float before = output.gain[plainIndex];
	sample_play::SetVolume(plain, 10);
	EXPECT_EQ(output.gain[plainIndex], before);
	sample_play::SetVolume(atmos, 10);
	EXPECT_NEAR(output.gain[atmosIndex], qmixer::Gain(10, 127), 1e-6f);
	sample_play::Switch(true);
	sample_play::SetVolume(plain, 10);
	EXPECT_NEAR(output.gain[plainIndex], qmixer::Gain(10, 127), 1e-6f);
}

TEST(LoopCounter, FinitePasses)
{
	// QSWaveMixPlayEx iLoops = 2: two wraps, then the looping stops (3 passes, inferred)
	LoopCounter counter;
	counter.Start(2);
	EXPECT_FALSE(counter.Feed(1000));
	EXPECT_FALSE(counter.Feed(5000));
	EXPECT_FALSE(counter.Feed(100)); // wrap 1
	EXPECT_FALSE(counter.Feed(4000));
	EXPECT_TRUE(counter.Feed(50)); // wrap 2: stop looping now
	EXPECT_FALSE(counter.Feed(10));
	counter.Start(-1);
	EXPECT_FALSE(counter.Feed(0));
	EXPECT_FALSE(counter.Feed(10));
}

TEST(GameSfxCounter, KnockRoofCycles)
{
	// Abode::InterfaceTap 0x4068F4: 110 + c, c = 0..8 then 0 again
	for (int i = 0; i < 9; ++i)
	{
		EXPECT_EQ(NextCounter(Counter::KnockRoof), i);
	}
	EXPECT_EQ(NextCounter(Counter::KnockRoof), 0);
	// the mulch adds first (0x63AA39): 1, 2, 3, 0
	EXPECT_EQ(NextCounter(Counter::TreeMulch), 1);
	EXPECT_EQ(NextCounter(Counter::TreeMulch), 2);
	EXPECT_EQ(NextCounter(Counter::TreeMulch), 3);
	EXPECT_EQ(NextCounter(Counter::TreeMulch), 0);
}

TEST_F(SamplePlayTest, HelpSpritesRestartGuidanceWaits)
{
	if (!LoadBank("Audio/Dialogue/HelpSprites.sad", 6, false) || !LoadBank("Audio/Dialogue/Guidance.sad", 10, false))
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// HelpSprites: clone group 1, the default mode 3 (no flag 0x400 in 1877 of them): the second of the same owner
	// restarts the first's channel (0x10011146)
	const auto owner = Owner::Key(k_OwnerVoice);
	const auto first = Start(Real(6, 1), 3, owner);
	const auto second = Start(Real(6, 2), 3, owner);
	ASSERT_NE(first, k_NoChannel);
	ASSERT_NE(second, k_NoChannel);
	EXPECT_EQ((first - 1) % 16, (second - 1) % 16);
	EXPECT_FALSE(sample_play::IsPlaying(first));
	// Guidance: mode 2 from the .sad (flags 0x780 / 0x7A0), clone group 1: the second does not sound
	const auto guide = Start(Real(10, 1), 3, owner);
	const int plays = output.plays;
	EXPECT_EQ(Start(Real(10, 2), 3, owner), guide);
	EXPECT_EQ(output.plays, plays);
	// priority 9999 against Guidance 20: sixteen HelpSprites of other owners take every channel but never lose theirs
	for (uint32_t i = 0; i < 16; ++i)
	{
		Start(Real(6, static_cast<int>(10 + i)), 3, Owner::Key(100 + i));
	}
	EXPECT_FALSE(sample_play::IsPlaying(guide));
}

TEST_F(SamplePlayTest, MpegInRiffDecodes)
{
	if (!LoadBank("Audio/Dialogue/HelpSprites.sad", 6, true))
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// HelpSprites 1 (HELP_TEXT_ALIGNMENT_CHANGE_01): RIFF wFormatTag 0x50, 41780 bytes at 64 kbps = about 5.2 s
	const auto& sound = sounds.at(Real(6, 1));
	EXPECT_EQ(sound.waveFormat, 0x50);
	wave_buffers::Pcm pcm;
	ASSERT_TRUE(wave_buffers::Decode(sound, pcm));
	EXPECT_EQ(pcm.sampleRate, 22050);
	const double seconds = static_cast<double>(pcm.Frames()) / pcm.sampleRate;
	EXPECT_NEAR(seconds, 5.2, 0.15);
	// every 50th HelpSprites wave decodes
	for (int n = 1; n <= 1923; n += 50)
	{
		wave_buffers::Pcm part;
		EXPECT_TRUE(wave_buffers::Decode(sounds.at(Real(6, n)), part)) << n;
	}
}

TEST_F(SamplePlayTest, InGameWavesDecodeAndLoop)
{
	if (!LoadBank("Audio/SFX/Game/InGame.sad", 1, true))
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// every sample but the empty 165 (engine.md §2.2); 210 records
	int decoded = 0;
	for (int n = 1; n <= 210; ++n)
	{
		const auto found = sounds.find(Real(1, n));
		if (found == sounds.end())
		{
			continue;
		}
		wave_buffers::Pcm pcm;
		if (wave_buffers::Decode(found->second, pcm))
		{
			++decoded;
		}
	}
	EXPECT_EQ(decoded, 209);
	// G_VillageBell (30): 5 loops (flag 0x40) of 0..27400 in 57855 frames (engine.md §1.7)
	const auto& bell = sounds.at(Real(1, 30));
	EXPECT_EQ(bell.loops, 5);
	EXPECT_EQ(bell.loopStart, 0);
	EXPECT_EQ(bell.loopEnd, 27400);
	wave_buffers::Pcm pcm;
	ASSERT_TRUE(wave_buffers::Decode(bell, pcm));
	EXPECT_EQ(pcm.Frames(), 57855u);
}

TEST_F(SamplePlayTest, OwnerChannelAndTheFadeOfAPSysSound)
{
	// LHSampleIsPlaying(bank, owner, LH_SampleInfo**) 0x10014010, which PSysSound fn_006D11A0 calls straight (0x6D120A):
	// the first channel of the bank and owner, whatever its sample
	const auto first = Start(Add(3, 5, 100), 1, Owner::Object(7));
	const auto second = Start(Add(3, 6, 100), 1, Owner::Object(8));
	ASSERT_NE(first, k_NoChannel);
	ASSERT_NE(second, k_NoChannel);
	EXPECT_EQ(sample_play::OwnerChannel(3, Owner::Object(7)), first);
	EXPECT_EQ(sample_play::OwnerChannel(3, Owner::Object(8)), second);
	// another bank or another owner: none (0x10014038..0x10014040)
	EXPECT_EQ(sample_play::OwnerChannel(4, Owner::Object(7)), k_NoChannel);
	EXPECT_EQ(sample_play::OwnerChannel(3, Owner::Object(9)), k_NoChannel);
	// 0x6D121C..0x6D1239: the FadeStep taken off that channel's volume (+0x38), never below 0
	EXPECT_EQ(sample_play::Volume(first), 127);
	sample_play::SetVolume(first, std::max(sample_play::Volume(first) - 30, 0));
	EXPECT_EQ(sample_play::Volume(first), 97);
	EXPECT_NEAR(output.gain[(first - 1) % 16], qmixer::Gain(97, 127), 1e-6f);
	sample_play::SetVolume(first, std::max(sample_play::Volume(first) - 200, 0));
	EXPECT_EQ(sample_play::Volume(first), 0);
	// the wave ends: +0x8C != 1, so the PSysSound is deleted (0x6D12A5, vtable +4 0x6D12B5)
	output.Finish((first - 1) % 16);
	EXPECT_EQ(sample_play::OwnerChannel(3, Owner::Object(7)), k_NoChannel);
	EXPECT_EQ(sample_play::OwnerChannel(3, Owner::Object(8)), second);
}

TEST_F(SamplePlayTest, OwnerChannelNeedsActive)
{
	// 0x10014018: k_NoChannel while switched off (+0x14), even though LHSamplePlay did start the channel
	sample_play::Switch(false);
	const auto channel = Start(Add(3, 5, 100), 1, Owner::Object(7));
	ASSERT_NE(channel, k_NoChannel);
	EXPECT_TRUE(output.playing[(channel - 1) % 16]);
	EXPECT_EQ(sample_play::OwnerChannel(3, Owner::Object(7)), k_NoChannel);
	sample_play::Switch(true);
}

TEST_F(SamplePlayTest, TapSoundKeepsTheOptionsPitch)
{
	// PlayTapSound fn_00726490: G_ClickOnSpell_01 (InGame 42) with the pitch of the icon's placement in the options
	// (+0x48) and no caller mask (+0x1C). The .sad's flags of sample 42 are 0x402, with no pitch bit 0x1, so the
	// options' pitch reaches the channel (0x1001278B..0x1001283B): placement 5 gives 175.
	if (!LoadBank("Audio/SFX/Game/InGame.sad", 1, false))
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	const auto& click = sounds.at(Real(1, 42));
	EXPECT_EQ(click.overrides, 0x402u);
	EXPECT_EQ(click.overrides & 0x1u, 0u);
	sample_play::Options options;
	options.sound = Real(1, 42);
	options.owner = Owner::None();
	options.is3D = false;
	options.pitch = 175;
	const auto channel = sample_play::Start(options);
	ASSERT_NE(channel, k_NoChannel);
	EXPECT_FLOAT_EQ(output.starts[(channel - 1) % 16].pitch, qmixer::FrequencyRatio(click.sampleRate, 175));
	// with the .sad's pitch bit the options' pitch would be dropped for the .sad's own
	auto raised = click;
	raised.overrides |= 0x1u;
	raised.pitch = 100;
	sounds.insert_or_assign(Real(1, 42), raised);
	sample_play::StopAll();
	const auto other = sample_play::Start(options);
	ASSERT_NE(other, k_NoChannel);
	EXPECT_FLOAT_EQ(output.starts[(other - 1) % 16].pitch, qmixer::FrequencyRatio(click.sampleRate, 100));
}
