/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <PackFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Audio/Audio.h"
#include "Audio/AudioRandom.h"
#include "Audio/Device/SampleOutput.h"
#include "Audio/Device/Sound.h"
#include "Audio/Device/WaveBuffers.h"
#include "Audio/Engine/QMixerLaws.h"
#include "Audio/GameQueries.h"
#include "Resources/Loaders.h"

// The sample player's 16 channels (allocation, priority steal), the options variant of the game's sounds, the
// main volume, the finite loops, the map clear leaving no OpenAL source, and the decoding of the RIFF MPEG layer II
// waves. A fake output stands for the original's mixer. The installation tests need OPENBLACK_TEST_BW_ROOT; without
// it they skip. Their *Synthetic twins load sample records built by the test through the same loader.

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// The mixer as the tests see it: a channel plays until stopped or Finish()ed
class FakeOutput final: public SampleOutput
{
public:
	std::array<bool, 16> playing {};
	std::array<bool, 16> made {};
	std::array<bool, 16> released {};
	std::array<float, 16> gain {};
	std::array<Start, 16> starts {};
	std::array<glm::vec3, 16> positions {};
	std::array<int, 16> ramped {}; ///< the ramped stops of each channel
	std::array<int, 16> cuts {};   ///< the fades of each channel cut
	int plays {0};
	/// the frequency changes of each channel after its start
	std::array<int, 16> pitches {};
	/// the listener's point as last given
	glm::vec3 listener {0.0f};

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
	void StopRamped(size_t channel) override
	{
		playing[channel] = false;
		++ramped[channel];
	}
	void CutFade(size_t channel) override { ++cuts[channel]; }
	[[nodiscard]] bool Playing(size_t channel) const override { return playing[channel]; }
	void SetGain(size_t channel, float g) override { gain[channel] = g; }
	void SetPitch(size_t channel, float) override { ++pitches[channel]; }
	void SetPosition(size_t channel, glm::vec3 at) override { positions[channel] = at; }
	void ReleaseLoop(size_t channel) override { released[channel] = true; }
	void SetListener(glm::vec3 at) override { listener = at; }
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
		sample_play::SetMainVolume(127);
		audio::ClearMap();
		output = FakeOutput {};
	}
	void TearDown() override
	{
		audio::ClearMap();
		sample_play::SetMainVolume(127); // back to its default (the mixer's maximum)
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

	/// A sample record of a .sad with only the fields a test sets (the rest zero)
	static pack::AudioBankSampleHeader Header(int id, uint16_t priority = 100, int16_t group = 0)
	{
		pack::AudioBankSampleHeader header {};
		header.id = id;
		header.priority = priority;
		header.group = group;
		header.sampleRate = 22050;
		return header;
	}

	/// A synthetic record loaded as LoadBank loads the real ones, keyed by Real(bank, header.id)
	void AddRecord(uint16_t bank, const pack::AudioBankSampleHeader& header, std::vector<uint8_t> wave = {})
	{
		std::vector<std::vector<uint8_t>> buffer;
		buffer.push_back(std::move(wave));
		const auto sound = resources::SoundLoader {}(resources::SoundLoader::FromBufferTag {}, header, buffer);
		sound->bank = bank;
		sounds.insert_or_assign(Real(bank, header.id), *sound);
	}
};

void PutU16(std::vector<uint8_t>& bytes, uint16_t value)
{
	bytes.push_back(static_cast<uint8_t>(value & 0xFF));
	bytes.push_back(static_cast<uint8_t>(value >> 8));
}

void PutU32(std::vector<uint8_t>& bytes, uint32_t value)
{
	PutU16(bytes, static_cast<uint16_t>(value & 0xFFFF));
	PutU16(bytes, static_cast<uint16_t>(value >> 16));
}

/// A RIFF wave: a 16 byte "fmt " chunk and the "data" chunk
std::vector<uint8_t> Riff(uint16_t formatTag, uint16_t channels, uint16_t bits, const std::vector<uint8_t>& data)
{
	constexpr uint32_t k_Rate = 22050;
	const auto blockAlign = static_cast<uint16_t>(channels * bits / 8);
	std::vector<uint8_t> bytes = {'R', 'I', 'F', 'F'};
	PutU32(bytes, static_cast<uint32_t>(4 + 8 + 16 + 8 + data.size() + (data.size() & 1)));
	bytes.insert(bytes.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
	PutU32(bytes, 16);
	PutU16(bytes, formatTag);
	PutU16(bytes, channels);
	PutU32(bytes, k_Rate);
	PutU32(bytes, k_Rate * blockAlign);
	PutU16(bytes, blockAlign);
	PutU16(bytes, bits);
	bytes.insert(bytes.end(), {'d', 'a', 't', 'a'});
	PutU32(bytes, static_cast<uint32_t>(data.size()));
	bytes.insert(bytes.end(), data.begin(), data.end());
	if ((data.size() & 1) != 0)
	{
		bytes.push_back(0);
	}
	return bytes;
}

/// A mono 16-bit PCM wave of `frames` samples (a ramp)
std::vector<uint8_t> PcmRiff(size_t frames)
{
	std::vector<uint8_t> data;
	for (size_t i = 0; i < frames; ++i)
	{
		PutU16(data, static_cast<uint16_t>(i * 64));
	}
	return Riff(1, 1, 16, data);
}

/// A RIFF wave of format 0x50 holding `frames` silent mono MPEG-2 layer II frames at 22050 Hz and 64 kbps
std::vector<uint8_t> MpegRiff(int frames)
{
	// 144 x 64000 / 22050 bytes; the header: no CRC, bitrate index 8, mono; zero bits after it allocate no subband
	std::vector<uint8_t> frame(144 * 64000 / 22050, 0);
	frame[0] = 0xFF;
	frame[1] = 0xF5;
	frame[2] = 0x80;
	frame[3] = 0xC0;
	std::vector<uint8_t> data;
	for (int i = 0; i < frames; ++i)
	{
		data.insert(data.end(), frame.begin(), frame.end());
	}
	return Riff(0x50, 1, 0, data);
}
} // namespace

TEST_F(SamplePlayTest, SeventeenthDoesNotPlay)
{
	std::array<Channel, 16> handles {};
	for (int i = 0; i < 16; ++i)
	{
		handles[static_cast<size_t>(i)] = Start(Add(1, i + 1, 100), 1);
		EXPECT_NE(handles[static_cast<size_t>(i)], k_NoChannel) << i;
	}
	// no free channel and none of a lower priority
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
	// a finished channel is free again
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

TEST_F(SamplePlayTest, ATwoDSampleIsHeardAtItsPan)
{
	// ahead (90, every 2D sample but the spooky voices'): at the listener, as before; 0 on the listener's left and 179
	// on the right, on the unit circle: the output gets AL's (right, up, -ahead) as (z, y, x)
	const auto at = [this](int pan) {
		sample_play::Options options;
		options.sound = Add(1, 1, 100);
		options.mode = 1;
		options.pan = pan;
		const auto handle = sample_play::Start(options);
		return output.starts[(handle - 1) % 16].position;
	};
	EXPECT_EQ(at(90), glm::vec3(0.0f));
	const auto left = at(0);
	EXPECT_NEAR(left.z, -1.0f, 1e-6f);
	EXPECT_NEAR(left.x, 0.0f, 1e-6f);
	const auto right = at(179);
	EXPECT_GT(right.z, 0.999f);
	EXPECT_NEAR(right.x, -std::cos(89.0f * 3.14159265f / 180.0f), 1e-5f);
	EXPECT_EQ(right.y, 0.0f);
}

TEST_F(SamplePlayTest, ASadPanWithItsFlagTakesThePlaceOfTheOptions)
{
	// flag 0x2: the .sad's pan (the whole u32 the library reads) instead of the options'; without it the options'
	const auto id = Add(1, 1, 100);
	sounds.at(id).overrides = 0x2;
	sounds.at(id).pan = 0;
	sample_play::Options options;
	options.sound = id;
	options.mode = 1;
	options.pan = 90;
	auto handle = sample_play::Start(options);
	EXPECT_NEAR(output.starts[(handle - 1) % 16].position.z, -1.0f, 1e-6f); // on the left
	sounds.at(id).pan = 0x10000u + 90u; // a high half that is not 0: past 180, so heard on the right
	sample_play::StopAll();
	handle = sample_play::Start(options);
	EXPECT_NEAR(output.starts[(handle - 1) % 16].position.z, 1.0f, 1e-6f);
	sounds.at(id).overrides = 0;
	sample_play::StopAll();
	handle = sample_play::Start(options);
	EXPECT_EQ(output.starts[(handle - 1) % 16].position, glm::vec3(0.0f));
}

TEST_F(SamplePlayTest, MainVolume)
{
	const auto handle = Start(Add(1, 1, 100), 1);
	const auto index = (handle - 1) % 16;
	EXPECT_NEAR(output.gain[index], 32766.0f / 32767.0f, 1e-6f);
	// a main volume of 64 re-applies floor(127 * 64 / 127) * 258 / 32767 = 0.504
	sample_play::SetMainVolume(64);
	EXPECT_NEAR(output.gain[index], 0.504f, 1e-3f);
	EXPECT_NEAR(output.gain[index], 16512.0f / 32767.0f, 1e-6f);
	// more than 127 is ignored
	sample_play::SetMainVolume(200);
	EXPECT_EQ(sample_play::MainVolume(), 64);
	EXPECT_NEAR(qmixer::Gain(127, 64), 16512.0f / 32767.0f, 1e-6f);
	sample_play::SetMainVolume(127);
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
	// Switch(false): StopAll and inactive; IsPlaying answers nothing while inactive
	sample_play::Switch(false);
	EXPECT_FALSE(sample_play::IsPlaying(handle));
	EXPECT_FALSE(sample_play::IsPlaying(a, Owner {}));
	sample_play::Switch(true);
	// an atmos channel survives StopAll
	sample_play::Options atmos;
	atmos.sound = Add(2, 1, 1);
	atmos.atmos = true;
	atmos.owner = Owner::AtmosMixer();
	const auto loop = sample_play::Start(atmos);
	sample_play::StopAll();
	EXPECT_TRUE(sample_play::IsPlaying(loop));
}

TEST_F(SamplePlayTest, RampedStopFreesTheChannelAtOnce)
{
	const auto a = Add(1, 1, 100);
	const auto first = Start(a, 1);
	ASSERT_NE(first, k_NoChannel);
	const auto index = static_cast<size_t>((first - 1) % 16);
	// the stop fades out in the background: the logic sees the channel stopped as soon as the call returns
	sample_play::Stop(a, Owner {});
	EXPECT_EQ(output.ramped[index], 1);
	EXPECT_FALSE(sample_play::IsPlaying(first));
	EXPECT_FALSE(sample_play::IsPlaying(a, Owner {}));
	// and the next start may take that channel at once
	const auto again = Start(Add(1, 2, 100), 1);
	ASSERT_NE(again, k_NoChannel);
	EXPECT_EQ(static_cast<size_t>((again - 1) % 16), index);
	EXPECT_TRUE(sample_play::IsPlaying(again));
	EXPECT_EQ(output.cuts[index], 0);
}

TEST_F(SamplePlayTest, StopAllCutsTheFadesButAtmos)
{
	sample_play::Options atmos;
	atmos.sound = Add(2, 1, 1);
	atmos.atmos = true;
	atmos.owner = Owner::AtmosMixer();
	const auto loop = sample_play::Start(atmos);
	ASSERT_NE(loop, k_NoChannel);
	const auto a = Add(1, 1, 100);
	const auto faded = Start(a, 1);
	ASSERT_NE(faded, k_NoChannel);
	sample_play::Stop(a, Owner {});
	// StopAll during the fade: every channel's fade is cut at once but the atmos channel's, which StopAll leaves alone
	sample_play::StopAll();
	const auto atmosIndex = static_cast<size_t>((loop - 1) % 16);
	for (size_t i = 0; i < 16; ++i)
	{
		EXPECT_EQ(output.cuts[i], i == atmosIndex ? 0 : 1) << i;
	}
	EXPECT_TRUE(sample_play::IsPlaying(loop));
}

TEST_F(SamplePlayTest, SetVolumeNeedsActive)
{
	// Start does not test the switch, so a channel can start while switched off; SetVolume leaves it alone then,
	// unless it is an atmos channel (the pitch too)
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

TEST_F(SamplePlayTest, SadFixedVolumeIsKept)
{
	// SetVolume leaves a sample whose .sad fixes the volume (override 0x20) at its own volume
	const auto id = Add(1, 1, 100);
	sounds.at(id).overrides = 0x20;
	sounds.at(id).volume127 = 100;
	const auto channel = Start(id, 1);
	ASSERT_NE(channel, k_NoChannel);
	const auto index = (channel - 1) % 16;
	const float before = output.gain[index];
	sample_play::SetVolume(channel, 10);
	EXPECT_EQ(output.gain[index], before);
}

TEST(SampleOverrides, AppliedAreTheSadsFieldsTheCallerLeftClear)
{
	// the audio library's rule: each of the eleven fields is taken, and remembered on the channel, only where the
	// .sad's flag is set and the caller's mask bit is clear
	EXPECT_EQ(sample_play::AppliedOverrides(0x20u, 0u), 0x20u);
	EXPECT_EQ(sample_play::AppliedOverrides(0x20u, 0x20u), 0u);
	EXPECT_EQ(sample_play::AppliedOverrides(0u, 0x7FFu), 0u);
	// the ambience loops (caller mask 0x20) of country.sad's garden loop, flags 0x221: pitch and scale stay
	EXPECT_EQ(sample_play::AppliedOverrides(0x221u, 0x20u), 0x201u);
	// the guidance's mask 0x180 keeps a .sad volume
	EXPECT_EQ(sample_play::AppliedOverrides(0x3A0u, 0x180u), 0x220u);
	// only the eleven fields 0x1..0x400 exist
	EXPECT_EQ(sample_play::AppliedOverrides(0xFFFFFFFFu, 0u), 0x7FFu);
	EXPECT_EQ(sample_play::AppliedOverrides(0xFFFFF800u, 0u), 0u);
}

TEST_F(SamplePlayTest, CallerVolumeCanBeChangedDespiteTheSadsVolume)
{
	// a .sad volume (flag 0x20) the caller overrode with its own (mask 0x20) is not kept by the channel, so the
	// caller's later volume changes are heard, as the ambience's fades are
	const auto id = Add(1, 1, 100);
	sounds.at(id).overrides = 0x20;
	sounds.at(id).volume127 = 50;
	sample_play::Options options;
	options.sound = id;
	options.mode = 1;
	options.volume = 0;
	options.callerMask = 0x20;
	const auto channel = sample_play::Start(options);
	ASSERT_NE(channel, k_NoChannel);
	const auto index = (channel - 1) % 16;
	EXPECT_EQ(sample_play::Volume(channel), 0);
	EXPECT_NEAR(output.gain[index], qmixer::Gain(0, 127), 1e-6f);
	sample_play::SetVolume(channel, 40);
	EXPECT_EQ(sample_play::Volume(channel), 40);
	EXPECT_NEAR(output.gain[index], qmixer::Gain(40, 127), 1e-6f);
}

TEST_F(SamplePlayTest, CallerPitchCanBeChangedDespiteTheSadsPitch)
{
	// the same for the pitch: a .sad pitch (flag 0x1) the caller overrode (mask 0x1) can be changed; one the start
	// took from the .sad cannot
	const auto pitchOf = [](Channel channel) {
		for (const auto& info : sample_play::Channels())
		{
			if (info.handle == channel)
			{
				return info.pitch;
			}
		}
		return 0;
	};
	const auto overridden = Add(1, 1, 100);
	const auto fixed = Add(1, 2, 100);
	for (const auto id : {overridden, fixed})
	{
		sounds.at(id).overrides = 0x1;
		sounds.at(id).pitch = 120;
	}
	sample_play::Options options;
	options.sound = overridden;
	options.mode = 1;
	options.pitch = 90;
	options.callerMask = 0x1;
	const auto first = sample_play::Start(options);
	options.sound = fixed;
	options.callerMask = 0;
	const auto second = sample_play::Start(options);
	ASSERT_NE(first, k_NoChannel);
	ASSERT_NE(second, k_NoChannel);
	EXPECT_EQ(pitchOf(first), 90);
	EXPECT_EQ(pitchOf(second), 120);
	sample_play::SetPitch(overridden, Owner::None(), 150);
	sample_play::SetPitch(fixed, Owner::None(), 150);
	EXPECT_EQ(pitchOf(first), 150);
	EXPECT_EQ(pitchOf(second), 120);
}

namespace
{
/// The 13 ambience banks of Audio\SFX\Atmos the mixer registers (ocean .. wind)
using AtmosBankFiles = std::array<const char*, 13>;
constexpr AtmosBankFiles k_AtmosBankFiles = {"ocean", "lake",   "shore", "jungle", "arctic", "desert", "country",
                                             "swamp", "stream", "high",  "night",  "rain",   "wind"};

struct AtmosFadeCount
{
	int loops {0};
	int loopsWithSadVolume {0};
	int loose {0};
	int looseWithSadVolume {0};
	int heard {0}; ///< the starts whose volume change reached the channel
};

/// Each ambience record (frequency >= 0, id > 0) of the banks loaded in `sounds` starts as the mixer starts it (the
/// loops at volume 0 in mode 2, the loose ones in 3D with their volume, both with the caller mask 0x20), then gets a
/// new volume; the count of those the channel took
AtmosFadeCount FadeEveryAtmosRecord(const std::map<entt::id_type, Sound>& sounds)
{
	AtmosFadeCount count;
	for (const auto& [id, sound] : sounds)
	{
		if (sound.atmosFrequency < 0 || sound.id <= 0)
		{
			continue;
		}
		const bool loop = sound.atmosFrequency == 0;
		const bool sadVolume = (sound.overrides & 0x20u) != 0;
		(loop ? count.loops : count.loose)++;
		(loop ? count.loopsWithSadVolume : count.looseWithSadVolume) += sadVolume ? 1 : 0;
		sample_play::Options options;
		options.sound = id;
		options.atmos = true;
		options.owner = Owner::AtmosMixer();
		options.callerMask = 0x20;
		options.is3D = !loop;
		options.relative = !loop;
		options.volume = loop ? 0 : sound.volume127;
		options.loops = loop ? -1 : 0;
		options.mode = loop ? 2 : 3;
		const auto channel = sample_play::Start(options);
		if (channel == k_NoChannel)
		{
			continue;
		}
		const int target = loop ? sound.volume127 : sound.volume127 - 5;
		sample_play::SetVolume(channel, target);
		count.heard += sample_play::Volume(channel) == target ? 1 : 0;
		sample_play::Stop(channel);
	}
	return count;
}
} // namespace

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(SamplePlayTest, AmbienceFadesReachTheChannels)
{
	for (size_t i = 0; i < k_AtmosBankFiles.size(); ++i)
	{
		if (!LoadBank(fmt::format("Audio/SFX/Atmos/{}.sad", k_AtmosBankFiles[i]), static_cast<uint16_t>(20 + i), false))
		{
			GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
		}
	}
	// 15 loops, 14 of them with a .sad volume, and 400 loose samples, 378 with one: every one takes the mixer's
	// volume changes, since the mixer gives its own volume
	const auto count = FadeEveryAtmosRecord(sounds);
	EXPECT_EQ(count.loops, 15);
	EXPECT_EQ(count.loopsWithSadVolume, 14);
	EXPECT_EQ(count.loose, 400);
	EXPECT_EQ(count.looseWithSadVolume, 378);
	EXPECT_EQ(count.heard, 415);
}

TEST_F(SamplePlayTest, AmbienceFadesReachTheChannelsSynthetic)
{
	// a loop with a .sad volume, a loop without one, and a loose sample with a .sad volume, pitch and scale
	const auto record = [](int id, uint16_t flags, uint8_t volume, uint16_t frequency) {
		pack::AudioBankSampleHeader header;
		std::memset(&header, 0, sizeof(header));
		header.id = id;
		header.priority = 100;
		header.sampleRate = 22050;
		header.unknown10 = flags;
		header.volume = volume;
		header.pitch = 100;
		header.atmos = frequency;
		return header;
	};
	AddRecord(20, record(1, 0x20, 30, 0));
	AddRecord(20, record(2, 0x01, 0, 0));
	AddRecord(20, record(3, 0x221, 75, 40));
	const auto count = FadeEveryAtmosRecord(sounds);
	EXPECT_EQ(count.loops, 2);
	EXPECT_EQ(count.loopsWithSadVolume, 1);
	EXPECT_EQ(count.loose, 1);
	EXPECT_EQ(count.looseWithSadVolume, 1);
	EXPECT_EQ(count.heard, 3);
	// without the caller's mask the .sad's volume is the channel's, and it stays
	const auto channel = Start(Real(20, 1), 1);
	ASSERT_NE(channel, k_NoChannel);
	EXPECT_EQ(sample_play::Volume(channel), 30);
	sample_play::SetVolume(channel, 10);
	EXPECT_EQ(sample_play::Volume(channel), 30);
}

TEST(LibraryRandomState, CrtSequenceAndAlternation)
{
	// No Backend::rand: the audio library's own CRT rand from the CRT's seed 1 (ResetRand through SetBackend):
	// seed = seed * 214013 + 2531011, (seed >> 16) & 0x7FFF, the MSVC sequence 41, 18467, 6334, 26500
	sample_play::SetBackend({});
	EXPECT_EQ(sample_play::Rand(), 41);
	EXPECT_EQ(sample_play::Rand(), 18467);
	EXPECT_EQ(sample_play::Rand(), 6334);
	EXPECT_EQ(sample_play::Rand(), 26500);
	// AlternatingRandom (seeded from the time the first time): rand() >> 1, + 0x3FFF while a flag (1 at load) is set,
	// then the flag flips
	for (int i = 0; i < 8; ++i)
	{
		const int upper = sample_play::AlternatingRandom();
		EXPECT_GE(upper, 0x3FFF);
		EXPECT_LE(upper, 0x7FFE);
		const int lower = sample_play::AlternatingRandom();
		EXPECT_GE(lower, 0);
		EXPECT_LE(lower, 0x3FFF);
	}
	// Random(n) never reaches n
	for (int i = 0; i < 64; ++i)
	{
		const int pick = sample_play::Random(10);
		EXPECT_GE(pick, 0);
		EXPECT_LT(pick, 10);
	}
	EXPECT_EQ(sample_play::Random(0), 0);
	sample_play::ResetRand();
	EXPECT_EQ(sample_play::Rand(), 41);
}

TEST(LoopCounter, FinitePasses)
{
	// a loop count of 2: two wraps, then the looping stops (3 passes, inferred)
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
	// the abode tap: 110 + c, c = 0..8 then 0 again
	for (int i = 0; i < 9; ++i)
	{
		EXPECT_EQ(NextCounter(Counter::KnockRoof), i);
	}
	EXPECT_EQ(NextCounter(Counter::KnockRoof), 0);
	// the mulch adds first: 1, 2, 3, 0
	EXPECT_EQ(NextCounter(Counter::TreeMulch), 1);
	EXPECT_EQ(NextCounter(Counter::TreeMulch), 2);
	EXPECT_EQ(NextCounter(Counter::TreeMulch), 3);
	EXPECT_EQ(NextCounter(Counter::TreeMulch), 0);
}

TEST(GameSfxCounter, ScaffoldTapCyclesThroughFour)
{
	// the scaffold tap: 151 + c with the count before it steps on, c = 0..3 then 0 again
	for (int i = 0; i < 4; ++i)
	{
		EXPECT_EQ(NextCounter(Counter::ScaffoldTap), i);
	}
	EXPECT_EQ(NextCounter(Counter::ScaffoldTap), 0);
}

TEST(GameSfxCounter, HandInWaterCyclesThroughTen)
{
	// the hand gripping the sea: 99 + c, c = 0..9 then 0 again
	for (int i = 0; i < 10; ++i)
	{
		EXPECT_EQ(NextCounter(Counter::HandInWater), i);
	}
	EXPECT_EQ(NextCounter(Counter::HandInWater), 0);
}

TEST(GameSfxCounter, RockTapCyclesThroughFour)
{
	// a rock split by a tap: 130 + c, c = 0..3 then 0 again
	for (int i = 0; i < 4; ++i)
	{
		EXPECT_EQ(NextCounter(Counter::RockTap), i);
	}
	EXPECT_EQ(NextCounter(Counter::RockTap), 0);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(SamplePlayTest, GuidanceRestartWaits)
{
	if (!LoadBank("Audio/Dialogue/HelpSprites.sad", 6, false) || !LoadBank("Audio/Dialogue/Guidance.sad", 10, false))
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// HelpSprites: clone group 1, the default mode 3 (no flag 0x400 in 1877 of them): the second of the same owner
	// restarts the first's channel
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

TEST_F(SamplePlayTest, GuidanceRestartWaitsSynthetic)
{
	// HelpSprites-like records: clone group 1, priority 9999, no play mode in the record (so the caller's mode 3).
	// Guidance-like records: clone group 1, priority 20, play mode 2 from the record (flag 0x400).
	constexpr uint16_t k_HelpSprites = 6;
	constexpr uint16_t k_Guidance = 10;
	for (const int n : {1, 2})
	{
		AddRecord(k_HelpSprites, Header(n, 9999, 1));
	}
	for (int n = 10; n < 26; ++n)
	{
		AddRecord(k_HelpSprites, Header(n, 9999, 1));
	}
	for (const int n : {1, 2})
	{
		auto header = Header(n, 20, 1);
		header.unknown10 = 0x400;
		header.loopType = pack::AudioBankLoop::Once;
		AddRecord(k_Guidance, header);
	}
	ASSERT_EQ(sounds.at(Real(k_Guidance, 1)).playMode, 2);
	ASSERT_EQ(sounds.at(Real(k_HelpSprites, 1)).playMode, 3);

	// mode 3 and the same clone group: the second of the same owner restarts the first's channel
	const auto owner = Owner::Key(k_OwnerVoice);
	const auto first = Start(Real(k_HelpSprites, 1), 3, owner);
	const auto second = Start(Real(k_HelpSprites, 2), 3, owner);
	ASSERT_NE(first, k_NoChannel);
	ASSERT_NE(second, k_NoChannel);
	EXPECT_EQ((first - 1) % 16, (second - 1) % 16);
	EXPECT_FALSE(sample_play::IsPlaying(first));
	// mode 2 from the record and the same clone group: the second does not sound
	const auto guide = Start(Real(k_Guidance, 1), 3, owner);
	const int plays = output.plays;
	EXPECT_EQ(Start(Real(k_Guidance, 2), 3, owner), guide);
	EXPECT_EQ(output.plays, plays);
	// sixteen of priority 9999 for other owners fill the 14 free channels, then steal the priority 20 one
	for (uint32_t i = 0; i < 16; ++i)
	{
		Start(Real(k_HelpSprites, static_cast<int>(10 + i)), 3, Owner::Key(100 + i));
	}
	EXPECT_FALSE(sample_play::IsPlaying(guide));
}

TEST_F(SamplePlayTest, NoDecodedWaveToTakeWithoutItsBuffer)
{
	// TakeDecoded gives only the PCM of the last buffer made, for that sound: a sound with no buffer has none, so the
	// advisor decodes the wave itself, as before
	Sound sound;
	sound.buffer.push_back({1, 2, 3});
	wave_buffers::Pcm pcm;
	EXPECT_FALSE(wave_buffers::TakeDecoded(sound, pcm));
	EXPECT_TRUE(pcm.samples.empty());
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
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

TEST_F(SamplePlayTest, MpegInRiffDecodesSynthetic)
{
	// A record whose wave format is 0x50 and whose wave is a RIFF holding 10 MPEG layer II frames of 1152 samples
	auto header = Header(1);
	header.unknown6a = 0x50;
	AddRecord(6, header, MpegRiff(10));
	const auto& sound = sounds.at(Real(6, 1));
	EXPECT_EQ(sound.waveFormat, 0x50);
	wave_buffers::Pcm pcm;
	ASSERT_TRUE(wave_buffers::Decode(sound, pcm));
	EXPECT_EQ(pcm.sampleRate, 22050);
	EXPECT_EQ(pcm.layout, ChannelLayout::Mono);
	EXPECT_EQ(pcm.Frames(), 10u * 1152u);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(SamplePlayTest, InGameWavesDecodeAndLoop)
{
	if (!LoadBank("Audio/SFX/Game/InGame.sad", 1, true))
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// every sample but the empty 165; 210 records
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
	// G_VillageBell (30): 5 loops (flag 0x40) of 0..27400 in 57855 frames
	const auto& bell = sounds.at(Real(1, 30));
	EXPECT_EQ(bell.loops, 5);
	EXPECT_EQ(bell.loopStart, 0);
	EXPECT_EQ(bell.loopEnd, 27400);
	wave_buffers::Pcm pcm;
	ASSERT_TRUE(wave_buffers::Decode(bell, pcm));
	EXPECT_EQ(pcm.Frames(), 57855u);
}

TEST_F(SamplePlayTest, InGameWavesDecodeAndLoopSynthetic)
{
	// 1: a PCM wave; 2: an empty sample; 3: a looping PCM wave (flag 0x40, 5 loops of 0..60);
	// 4: a loop count without flag 0x40, which the loader ignores
	AddRecord(1, Header(1), PcmRiff(100));
	AddRecord(1, Header(2));
	auto bellHeader = Header(3);
	bellHeader.unknown10 = 0x40;
	bellHeader.loop = 5;
	bellHeader.lStart = 0;
	bellHeader.lEnd = 60;
	AddRecord(1, bellHeader, PcmRiff(100));
	auto noFlag = Header(4);
	noFlag.loop = 3;
	AddRecord(1, noFlag, PcmRiff(10));
	int decoded = 0;
	for (int n = 1; n <= 4; ++n)
	{
		wave_buffers::Pcm pcm;
		if (wave_buffers::Decode(sounds.at(Real(1, n)), pcm))
		{
			++decoded;
		}
	}
	EXPECT_EQ(decoded, 3);
	const auto& bell = sounds.at(Real(1, 3));
	EXPECT_EQ(bell.loops, 5);
	EXPECT_EQ(bell.loopStart, 0);
	EXPECT_EQ(bell.loopEnd, 60);
	wave_buffers::Pcm pcm;
	ASSERT_TRUE(wave_buffers::Decode(bell, pcm));
	EXPECT_EQ(pcm.Frames(), 100u);
	EXPECT_EQ(pcm.sampleRate, 22050);
	EXPECT_EQ(sounds.at(Real(1, 4)).loops, 0);
}

TEST_F(SamplePlayTest, OwnerChannelAndTheFadeOfAParticleSound)
{
	// the owner lookup a particle sound uses to fade its own sound: the first channel of the bank and owner,
	// whatever its sample
	const auto first = Start(Add(3, 5, 100), 1, Owner::Object(7));
	const auto second = Start(Add(3, 6, 100), 1, Owner::Object(8));
	ASSERT_NE(first, k_NoChannel);
	ASSERT_NE(second, k_NoChannel);
	EXPECT_EQ(sample_play::OwnerChannel(3, Owner::Object(7)), first);
	EXPECT_EQ(sample_play::OwnerChannel(3, Owner::Object(8)), second);
	// another bank or another owner: none
	EXPECT_EQ(sample_play::OwnerChannel(4, Owner::Object(7)), k_NoChannel);
	EXPECT_EQ(sample_play::OwnerChannel(3, Owner::Object(9)), k_NoChannel);
	// the fade step taken off that channel's volume, never below 0
	EXPECT_EQ(sample_play::Volume(first), 127);
	sample_play::SetVolume(first, std::max(sample_play::Volume(first) - 30, 0));
	EXPECT_EQ(sample_play::Volume(first), 97);
	EXPECT_NEAR(output.gain[(first - 1) % 16], qmixer::Gain(97, 127), 1e-6f);
	sample_play::SetVolume(first, std::max(sample_play::Volume(first) - 200, 0));
	EXPECT_EQ(sample_play::Volume(first), 0);
	// the wave ends, so the particle sound is deleted
	output.Finish((first - 1) % 16);
	EXPECT_EQ(sample_play::OwnerChannel(3, Owner::Object(7)), k_NoChannel);
	EXPECT_EQ(sample_play::OwnerChannel(3, Owner::Object(8)), second);
}

TEST_F(SamplePlayTest, OwnerChannelNeedsActive)
{
	// k_NoChannel while switched off, even though Start did start the channel
	sample_play::Switch(false);
	const auto channel = Start(Add(3, 5, 100), 1, Owner::Object(7));
	ASSERT_NE(channel, k_NoChannel);
	EXPECT_TRUE(output.playing[(channel - 1) % 16]);
	EXPECT_EQ(sample_play::OwnerChannel(3, Owner::Object(7)), k_NoChannel);
	sample_play::Switch(true);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(SamplePlayTest, TapSoundKeepsTheOptionsPitch)
{
	// the tap sound: G_ClickOnSpell_01 (InGame 42) with the pitch of the icon's placement in the options and no
	// caller mask. The .sad's flags of sample 42 are 0x402, with no pitch bit 0x1, so the options' pitch reaches the
	// channel: placement 5 gives 175.
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

TEST_F(SamplePlayTest, TapSoundKeepsTheOptionsPitchSynthetic)
{
	// A record with the play mode flag and another bit but no pitch bit, and a pitch of its own (150) that must not
	// be used
	auto header = Header(42);
	header.unknown10 = 0x402;
	header.loopType = pack::AudioBankLoop::Restart;
	header.pitch = 150;
	AddRecord(1, header);
	const auto& click = sounds.at(Real(1, 42));
	EXPECT_EQ(click.overrides, 0x402u);
	EXPECT_EQ(click.overrides & 0x1u, 0u);
	EXPECT_EQ(click.pitch, 100);
	sample_play::Options options;
	options.sound = Real(1, 42);
	options.owner = Owner::None();
	options.is3D = false;
	options.pitch = 175;
	const auto channel = sample_play::Start(options);
	ASSERT_NE(channel, k_NoChannel);
	EXPECT_FLOAT_EQ(output.starts[(channel - 1) % 16].pitch, qmixer::FrequencyRatio(click.sampleRate, 175));
	// with the pitch bit the record's pitch replaces the options'
	auto raised = click;
	raised.overrides |= 0x1u;
	raised.pitch = 120;
	sounds.insert_or_assign(Real(1, 42), raised);
	sample_play::StopAll();
	const auto other = sample_play::Start(options);
	ASSERT_NE(other, k_NoChannel);
	EXPECT_FLOAT_EQ(output.starts[(other - 1) % 16].pitch, qmixer::FrequencyRatio(click.sampleRate, 120));
}

TEST(GameSfx, GuardSoundPoint)
{
	// only a coordinate whose magnitude is above 5000 becomes 0
	EXPECT_EQ(GuardSoundPoint(glm::vec3(5000.0f, -5000.0f, 4999.5f)), glm::vec3(5000.0f, -5000.0f, 4999.5f));
	EXPECT_EQ(GuardSoundPoint(glm::vec3(5000.001f, -6000.0f, 12.0f)), glm::vec3(0.0f, 0.0f, 12.0f));
	EXPECT_EQ(GuardSoundPoint(glm::vec3(1e30f, 3.0f, -1e30f)), glm::vec3(0.0f, 3.0f, 0.0f));
	const auto nan = GuardSoundPoint(glm::vec3(std::numeric_limits<float>::quiet_NaN(), 1.0f, 2.0f));
	EXPECT_TRUE(std::isnan(nan.x)); // NaN compares unordered: kept
}

TEST_F(SamplePlayTest, TrackedPointGuardedAt5000)
{
	// the 3D channel update: the point handed to the mixer has each coordinate beyond 5000 cleared, the distance it
	// returns is the unguarded one
	glm::vec3 at(6000.0f, 10.0f, -7000.0f);
	RegisterObject(5, [&at]() { return std::optional<glm::vec3>(at); });
	sample_play::Backend backend;
	backend.output = &output;
	backend.sound = [this](entt::id_type id) -> Sound* {
		const auto found = sounds.find(id);
		return found != sounds.end() ? &found->second : nullptr;
	};
	backend.rand = []() { return 16383; };
	backend.camera = []() -> std::optional<glm::vec3> { return glm::vec3(0.0f); };
	backend.ownerPosition = [](const Owner& owner) { return OwnerSoundPosition(owner); };
	sample_play::SetBackend(std::move(backend));

	sample_play::Options options;
	options.sound = Add(1, 5, 100);
	options.mode = 1;
	options.owner = Owner::Object(5);
	options.is3D = true;
	options.track = true;
	options.position = at;
	const auto channel = sample_play::Start(options);
	ASSERT_NE(channel, k_NoChannel);
	const auto index = static_cast<size_t>((channel - 1) % 16);
	sample_play::UpdateChannels();
	EXPECT_TRUE(output.playing[index]); // 9219.5 < 9999
	EXPECT_EQ(output.positions[index], glm::vec3(0.0f, 10.0f, 0.0f));
	// the anim effects' start asks the same function: guarded too
	EXPECT_EQ(OwnerSoundPoint(Owner::Object(5)), std::optional<glm::vec3>(glm::vec3(0.0f, 10.0f, 0.0f)));
	// 6000, 8000: the guarded point is the camera's, but the distance 10000 is past the max 9999: it stops
	at = glm::vec3(6000.0f, 0.0f, 8000.0f);
	sample_play::UpdateChannels();
	EXPECT_FALSE(output.playing[index]);
	UnregisterObject(5);
}

TEST_F(SamplePlayTest, AnUntrackedSoundOfAGoneThingPlaysOnWhereItStartedOnAChannelOfItsOwn)
{
	// a scaffold's join sound belongs to the held scaffold, which the join has already deleted: in 3D and not
	// following its owner, it never asks the owner for a point, so it plays to its end where it started; and the gone
	// owner keys a channel of its own, leaving the one on the ground's channel of the same sample alone
	int asked = 0;
	sample_play::Backend backend;
	backend.output = &output;
	backend.sound = [this](entt::id_type id) -> Sound* {
		const auto found = sounds.find(id);
		return found != sounds.end() ? &found->second : nullptr;
	};
	backend.rand = []() { return 16383; };
	backend.camera = []() -> std::optional<glm::vec3> { return glm::vec3(0.0f); };
	backend.ownerPosition = [&asked](const Owner&) -> std::optional<glm::vec3> {
		++asked;
		return std::nullopt;
	};
	sample_play::SetBackend(std::move(backend));

	const glm::vec3 hand(30.0f, 4.0f, 20.0f);
	sample_play::Options options;
	options.sound = Add(1, 202, 100);
	options.mode = 3;
	options.is3D = true;
	options.track = false;
	options.position = hand;
	options.owner = Owner::Thing(entt::entity {8});
	const auto ground = sample_play::Start(options);
	options.owner = Owner::Thing(entt::entity {7});
	const auto joined = sample_play::Start(options);
	ASSERT_NE(ground, k_NoChannel);
	ASSERT_NE(joined, k_NoChannel);
	const auto index = static_cast<size_t>((joined - 1) % 16);
	EXPECT_NE(static_cast<size_t>((ground - 1) % 16), index);
	EXPECT_TRUE(sample_play::IsPlaying(ground));
	sample_play::UpdateChannels();
	EXPECT_TRUE(output.playing[index]);
	EXPECT_EQ(output.starts[index].position, hand);
	EXPECT_EQ(asked, 0);
}

TEST_F(SamplePlayTest, MovingTheCameraPastASoundChangesNoPitch)
{
	// the listener has no motion: as the camera sweeps past a 3D sound, every turn's update moves the listener and the
	// sound's point, and never the sound's frequency, so there is no Doppler
	const auto camera = std::make_shared<glm::vec3>(0.0f);
	const glm::vec3 at(10.0f, 0.0f, 0.0f);
	sample_play::Backend backend;
	backend.output = &output;
	backend.sound = [this](entt::id_type id) -> Sound* {
		const auto found = sounds.find(id);
		return found != sounds.end() ? &found->second : nullptr;
	};
	backend.rand = []() { return 16383; };
	backend.camera = [camera]() -> std::optional<glm::vec3> { return *camera; };
	backend.ownerPosition = [at](const Owner&) -> std::optional<glm::vec3> { return at; };
	sample_play::SetBackend(std::move(backend));

	sample_play::Options options;
	options.sound = Add(1, 5, 100);
	options.mode = 1;
	options.owner = Owner::Thing(entt::entity {7});
	options.is3D = true;
	options.track = true;
	options.position = at;
	const auto channel = sample_play::Start(options);
	ASSERT_NE(channel, k_NoChannel);
	const auto index = static_cast<size_t>((channel - 1) % 16);
	const int pitchesAtStart = output.pitches[index];
	for (int turn = 1; turn <= 10; ++turn)
	{
		*camera = glm::vec3(50.0f * static_cast<float>(turn) - 250.0f, 5.0f, 20.0f);
		sample_play::UpdateChannels();
		EXPECT_TRUE(output.playing[index]) << turn;
		EXPECT_EQ(output.listener, *camera) << turn;
		EXPECT_EQ(output.positions[index], at) << turn;
	}
	EXPECT_EQ(output.pitches[index], pitchesAtStart);
}

TEST(AudioRandom, IsTheLibrarysCRuntimeRandFromSeedOne)
{
	// MSVC's rand() from srand(1): 41, 18467, 6334, 26500
	audio::AudioRandom random;
	EXPECT_EQ(random.Next(), 41u);
	EXPECT_EQ(random.Next(), 18467u);
	EXPECT_EQ(random.Next(), 6334u);
	EXPECT_EQ(random.Next(), 26500u);
	// seeded again, the same sequence again
	random.Seed(1);
	EXPECT_EQ(random.Next(), 41u);
	EXPECT_LE(random.Next(), audio::AudioRandom::k_RandMax);
}
