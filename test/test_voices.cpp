/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <chrono>
#include <complex>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include <PackFile.h>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Audio/Advisor.h"
#include "Audio/Audio.h"
#include "Audio/Banks.h"
#include "Audio/GameQueries.h"
#include "Audio/SampleOutput.h"
#include "Audio/Sound.h"
#include "Audio/Voices.h"
#include "Audio/WaveBuffers.h"
#include "Common/HelpText.h"
#include "Help/HelpSystem.h"

// Milestone B7 of dev\tmp_dis\audio\PLAN.md (dev\tmp_dis\audio\voices.md §2.3-2.6, script.md §2.3): the voices on the
// channels. RUN_TEXT's voice fn_005C5F90 (the advisors with 0x270C through LHSamplePlay directly, the rest 2D with
// 0x270F through GAudio::PlaySoundEffect), SAY_SOUND 0x70F8E0 / SAY_SOUND_EFFECT_PLAYING 0x710280, the click's cut
// 0x5C6AAD (villagers only, with LHSampleStop's ramp), TEXT_READ with a voice (+450 ms after the narration, +200 ms
// after an advisor, 0x5C6340), HelpDude (0x5BB340..0x5BB840, 0x5C36D0, 0x5C3780) and the lip-sync analysis
// (AudioAnalyse 0x428C60 / four1 0x428D50, CalcKey 0x428850). A fake output stands for QMixer. The installation tests
// (the lazy dialogue banks) need OPENBLACK_TEST_BW_ROOT.

using namespace openblack;
using namespace openblack::audio;

namespace
{
class FakeOutput final: public SampleOutput
{
public:
	std::array<bool, 16> playing {};
	std::array<Start, 16> starts {};
	std::array<int64_t, 16> position {};
	int plays {0};
	int ramped {0};

	bool Play(size_t channel, Sound&, const Start& start) override
	{
		playing[channel] = true;
		starts[channel] = start;
		position[channel] = 0;
		++plays;
		return true;
	}
	void Stop(size_t channel) override { playing[channel] = false; }
	void StopRamped(size_t channel) override
	{
		++ramped;
		playing[channel] = false;
	}
	[[nodiscard]] int64_t PlayPositionMs(size_t channel) const override
	{
		return playing[channel] ? position[channel] : -1;
	}
	[[nodiscard]] bool Playing(size_t channel) const override { return playing[channel]; }
	void SetGain(size_t, float) override {}
	void SetPitch(size_t, float) override {}
	void SetPosition(size_t, glm::vec3) override {}
	void ReleaseLoop(size_t) override {}
	void SetListener(glm::vec3) override {}
	void Update() override {}
	[[nodiscard]] size_t Sources() const override { return 0; }
	void DeleteAll() override { playing.fill(false); }
};

// The texts of the fixture: 1 a villager's (villagers 1), 2 / 3 the spirits' (HelpSprites 1 / 2), 4 a HelpSprites text
// of another narrator (HelpSprites 3), 5 no voice
constexpr uint32_t k_TextMan = 1;
constexpr uint32_t k_TextGood = 2;
constexpr uint32_t k_TextEvil = 3;
constexpr uint32_t k_TextOther = 4;
constexpr uint32_t k_TextSilent = 5;

class VoicesTest: public ::testing::Test
{
protected:
	FakeOutput output;
	std::map<entt::id_type, Sound> sounds;
	BankId helpSprites {k_NoBank};
	BankId villagers {k_NoBank};
	static inline bool s_InsideCitadel = false;

	void SetUp() override
	{
		s_InsideCitadel = false;
		GameQueries queries;
		queries.camera = []() -> std::optional<CameraState> { return CameraState {}; };
		queries.insideCitadel = []() { return s_InsideCitadel; };
		audio::Init(std::move(queries));
		helpSprites = RegisterBank("audio/dialogue/HelpSprites.sad", "HelpSprites.sad");
		villagers = RegisterBank("audio/dialogue/villagers.sad", "villagers.sad");
		SetBankSampleCount(helpSprites, 3);
		SetBankSampleCount(villagers, 2);
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
		sample_play::SetMasterVolume(127);
		audio::ClearMap();
		output = FakeOutput {};
		for (int n = 1; n <= 3; ++n)
		{
			Add(helpSprites, n);
		}
		Add(villagers, 1);
		Add(villagers, 2);
		// villagers 1 is HELP_TEXT_MAN; HelpSprites 1..3 the spirits' and the other narrator's
		voices::SetTable(VoiceTable::Build({"HELP_TEXT_NONE", "HELP_TEXT_MAN", "HELP_TEXT_GOOD", "HELP_TEXT_EVIL",
		                                    "HELP_TEXT_OTHER", "HELP_TEXT_SILENT"},
		                                   {"K:\\4frosty\\Spanish\\1622p\\HELP_TEXT_MAN.wav", "K:\\x\\HELP_TEXT_X.wav"},
		                                   {"K:\\x\\HELP_TEXT_GOOD.wav", "K:\\x\\HELP_TEXT_EVIL.wav", "K:\\x\\HELP_TEXT_OTHER.wav"},
		                                   {}));
		advisor::Init(helpSprites);
		advisor::Reset();
	}
	void TearDown() override
	{
		advisor::Reset();
		audio::SetGameSound(true);
		audio::ClearMap();
		sample_play::SetBackend({});
		voices::SetTable({});
		audio::Shutdown();
	}

	/// A dialogue sample as the .sad gives it: priority 9999, mode 3 by default, user parameter 0, 22050 Hz
	void Add(BankId bank, int number)
	{
		Sound sound;
		sound.name = fmt::format("{} {}", BankGroup(bank), number);
		sound.id = number;
		sound.bank = bank;
		sound.priority = 9999;
		sound.sampleRate = 22050;
		sound.pitch = 100;
		sound.pitchDeviation = 0;
		sound.duration = 2.0f;
		sounds.emplace(SampleId(bank, number), std::move(sound));
	}

	[[nodiscard]] static std::optional<sample_play::ChannelInfo> Channel(Owner owner)
	{
		for (const auto& info : sample_play::Channels())
		{
			if (info.playing && info.owner == owner)
			{
				return info;
			}
		}
		return std::nullopt;
	}

	/// Ends whatever plays on the channels of an owner (the wave reached its end)
	void End(Owner owner)
	{
		for (const auto& info : sample_play::Channels())
		{
			if (info.playing && info.owner == owner)
			{
				output.playing[(info.handle - 1) % 16] = false;
			}
		}
	}
};
} // namespace

TEST_F(VoicesTest, NarrationIs2DWithTheNarrationOwner)
{
	// fn_005C5F90 0x5C60A4..0x5C60DB: a villager's text, GAudio::PlaySoundEffect with 0x270F, 2D
	const auto channel = voices::RunTextVoice(5, voices::Table().Get(k_TextMan));
	ASSERT_NE(channel, k_NoChannel);
	const auto info = Channel(Owner::Key(k_OwnerVoice));
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->bank, villagers);
	EXPECT_EQ(info->sample, 1);
	EXPECT_FALSE(info->is3D);
	// a HelpSprites text of another narrator takes the same branch (0x5C6098)
	EXPECT_NE(voices::RunTextVoice(4, voices::Table().Get(k_TextOther)), k_NoChannel);
	EXPECT_EQ(advisor::Speaker(), -1);
	// no voice: nothing
	EXPECT_EQ(voices::RunTextVoice(5, voices::Table().Get(k_TextSilent)), k_NoChannel);
}

TEST_F(VoicesTest, NarrationGoesThroughGAudiosFiltersAndTheAdvisorDoesNot)
{
	// inside the citadel only user parameter 2 plays (0x429F6D): the narration of villagers (0) is skipped...
	s_InsideCitadel = true;
	EXPECT_EQ(voices::RunTextVoice(5, voices::Table().Get(k_TextMan)), k_NoChannel);
	// ...but the advisor plays through LHSamplePlay directly (HelpDude::PlaySample 0x5BB544)
	voices::RunTextVoice(helptext::k_NarratorGoodSpirit, voices::Table().Get(k_TextGood));
	const auto info = Channel(Owner::Key(k_OwnerAdvisor));
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->bank, helpSprites);
	EXPECT_EQ(info->sample, 1);
	EXPECT_FALSE(info->is3D);
}

TEST_F(VoicesTest, SpiritTextsGoToTheirAdvisor)
{
	// narrator 2 -> dude 0, owner 0x270C; with the advisor's flight not ported, the delay of 0x5C36D0 is 0 and the
	// sentence starts at once (UpdateSaySentence from SaySentence 0x5BB408)
	voices::RunTextVoice(helptext::k_NarratorGoodSpirit, voices::Table().Get(k_TextGood));
	EXPECT_EQ(advisor::Speaker(), advisor::k_GoodSpirit);
	EXPECT_EQ(advisor::Sentence(), 1);
	EXPECT_TRUE(advisor::Active(advisor::k_GoodSpirit));
	EXPECT_TRUE(advisor::IsTalking(advisor::k_GoodSpirit));
	EXPECT_FALSE(advisor::IsTalking(advisor::k_EvilSpirit)); // only the speaker talks
	EXPECT_TRUE(advisor::AnyTalking());
	// narrator 3: both advisors stopped (fn_005C3750 1 then 0), the evil one says HelpSprites 2
	voices::RunTextVoice(helptext::k_NarratorEvilSpirit, voices::Table().Get(k_TextEvil));
	EXPECT_EQ(advisor::Speaker(), advisor::k_EvilSpirit);
	EXPECT_EQ(advisor::Sentence(), 2);
	EXPECT_FALSE(advisor::Active(advisor::k_GoodSpirit));
	const auto info = Channel(Owner::Key(k_OwnerAdvisor));
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->sample, 2);
	EXPECT_EQ(output.ramped, 1); // the good one's sentence stopped by LHSampleStop (StopSentence 0x5BB875)
}

TEST_F(VoicesTest, AdvisorSentenceRules)
{
	// a sample outside 1..count is not said (0x5BB389..0x5BB39A), but the dude is the speaker (0x5BB343)
	advisor::SaySentence(advisor::k_GoodSpirit, 4, false, 0);
	EXPECT_EQ(advisor::Sentence(), 0);
	EXPECT_EQ(advisor::Speaker(), advisor::k_GoodSpirit);
	advisor::SaySentence(advisor::k_GoodSpirit, 0, false, 0);
	EXPECT_EQ(output.plays, 0);
	// onlyIfSilent while talking: nothing (0x5BB368)
	advisor::SaySentence(advisor::k_GoodSpirit, 1, false, 0);
	advisor::SaySentence(advisor::k_GoodSpirit, 2, true, 0);
	EXPECT_EQ(advisor::Sentence(), 1);
	// without it: the sentence stops and the new one starts; as in W120, the StopSentence of 0x5BB377 runs after
	// g_speaker = this (0x5BB343) and clears it (0x5BB89F), so the new sentence has no speaker (RUN_TEXT's path stops
	// both advisors first, fn_005C3750, and never meets this)
	advisor::SaySentence(advisor::k_GoodSpirit, 2, false, 0);
	EXPECT_EQ(advisor::Sentence(), 2);
	EXPECT_EQ(advisor::Speaker(), -1);
	EXPECT_FALSE(advisor::IsTalking(advisor::k_GoodSpirit));
	// a delay: IsTalking already (+0x35D4 set, 0x5BB774), nothing on the channels until the time comes
	advisor::Reset();
	const int plays = output.plays;
	advisor::SaySentence(advisor::k_GoodSpirit, 3, false, 60);
	EXPECT_TRUE(advisor::IsTalking(advisor::k_GoodSpirit));
	EXPECT_FLOAT_EQ(advisor::PercentageDone(advisor::k_GoodSpirit), 0.0f); // 0x5BB7D5
	advisor::Update(0.016f);
	EXPECT_EQ(output.plays, plays);
	std::this_thread::sleep_for(std::chrono::milliseconds(80));
	advisor::Update(0.016f);
	EXPECT_EQ(output.plays, plays + 1);
	EXPECT_EQ(advisor::Sentence(), 3);
}

TEST_F(VoicesTest, AdvisorTalksUntil200MsAfterItsSentence)
{
	advisor::Say(advisor::k_GoodSpirit, 1, false);
	EXPECT_TRUE(advisor::TalkingOrJustStopped(advisor::k_GoodSpirit));
	// the wave ends: IsTalking stops the sentence (0x5BB7B6) but fn_005BB730 holds 200 ms (0x5BB74F)
	End(Owner::Key(k_OwnerAdvisor));
	EXPECT_FALSE(advisor::IsTalking(advisor::k_GoodSpirit));
	EXPECT_EQ(advisor::Sentence(), 0);
	EXPECT_EQ(advisor::Speaker(), -1);
	EXPECT_TRUE(advisor::AnyTalking());
	std::this_thread::sleep_for(std::chrono::milliseconds(230));
	EXPECT_FALSE(advisor::AnyTalking());
}

TEST_F(VoicesTest, PercentageDoneIsPositionOverLength)
{
	advisor::Say(advisor::k_GoodSpirit, 1, false);
	const auto info = Channel(Owner::Key(k_OwnerAdvisor));
	ASSERT_TRUE(info.has_value());
	// LHSampleGetPercentageDone 0x10015180: the ms of the position over the wave's length (2 s here)
	output.position[(info->handle - 1) % 16] = 500;
	EXPECT_FLOAT_EQ(advisor::PercentageDone(advisor::k_GoodSpirit), 0.25f);
	EXPECT_EQ(sample_play::PlayPosition(helpSprites, Owner::Key(k_OwnerAdvisor)), 500);
	// a dude that is not the speaker: 1 (0x5BB7CB)
	EXPECT_FLOAT_EQ(advisor::PercentageDone(advisor::k_EvilSpirit), 1.0f);
}

TEST_F(VoicesTest, InterruptionIsNeverSaidInW120)
{
	// fn_005C3780(dude, 1): Stop(dude) first, so PercentageDone sees no speaker (1 >= 0.9) and says nothing
	advisor::Say(advisor::k_GoodSpirit, 1, false);
	const int plays = output.plays;
	advisor::Interrupt(advisor::k_GoodSpirit, 1);
	EXPECT_FALSE(advisor::Active(advisor::k_GoodSpirit));
	EXPECT_FALSE(advisor::IsTalking(advisor::k_GoodSpirit));
	EXPECT_EQ(output.plays, plays);
	EXPECT_FALSE(Channel(Owner::Key(k_OwnerAdvisor)).has_value());
	// a dude that is not active: nothing at all
	advisor::Interrupt(advisor::k_EvilSpirit, 1);
	EXPECT_EQ(output.plays, plays);
}

TEST_F(VoicesTest, SayOwnersPositionAndPlaying)
{
	// SaySoundEffect 0x70F8E0: alt -> 0x270D, withPos -> 3D at the point, track 0
	ASSERT_NE(voices::Say(k_TextMan, true, true, glm::vec3(3.0f, 1.0f, 4.0f)), k_NoChannel);
	auto info = Channel(Owner::Key(k_OwnerVoiceAlt));
	ASSERT_TRUE(info.has_value());
	EXPECT_TRUE(info->is3D);
	EXPECT_FALSE(info->track);
	EXPECT_EQ(output.starts.at((info->handle - 1) % 16).position, glm::vec3(3.0f, 1.0f, 4.0f));
	EXPECT_TRUE(voices::IsSaying(true, k_TextMan));
	EXPECT_FALSE(voices::IsSaying(false, k_TextMan));
	// without position: 2D with 0x270F, even a spirit's text (no advisor)
	ASSERT_NE(voices::Say(k_TextGood, false, false, glm::vec3(9.0f)), k_NoChannel);
	info = Channel(Owner::Key(k_OwnerVoice));
	ASSERT_TRUE(info.has_value());
	EXPECT_FALSE(info->is3D);
	EXPECT_EQ(info->bank, helpSprites);
	EXPECT_EQ(advisor::Speaker(), -1);
	EXPECT_TRUE(voices::IsSaying(false, k_TextGood));
	// no voice, or a text past the table (-> text 0): nothing
	EXPECT_EQ(voices::Say(k_TextSilent, false, false, glm::vec3(0.0f)), k_NoChannel);
	EXPECT_EQ(voices::Say(7000, false, false, glm::vec3(0.0f)), k_NoChannel);
	EXPECT_FALSE(voices::IsSaying(false, 7000));
}

TEST_F(VoicesTest, ClickCutsOnlyTheVillagersNarration)
{
	voices::RunTextVoice(5, voices::Table().Get(k_TextMan));     // villagers, 0x270F
	voices::RunTextVoice(4, voices::Table().Get(k_TextOther));   // HelpSprites, 0x270F
	voices::Say(k_TextMan, false, true, glm::vec3(0.0f));        // villagers, 0x270D
	voices::CutByClick();
	EXPECT_EQ(output.ramped, 1); // LHSampleStop(villagers, 0x270F, 0) with the 20 ms ramp
	EXPECT_FALSE(voices::IsSaying(false, k_TextMan));
	EXPECT_TRUE(voices::IsSaying(false, k_TextOther));
	EXPECT_TRUE(voices::IsSaying(true, k_TextMan));
}

TEST_F(VoicesTest, TextReadWaitsForTheVoice)
{
	// HelpSystem wired as Game.cpp does, with clocks the test moves
	int32_t now = 1000;
	uint32_t turn = 0;
	const auto entry = [](uint32_t id) {
		static const std::array<helptext::Entry, 6> k_Texts = {{{0, 0, "HELP_TEXT_NONE", u"x"},
		                                                         {1, 5, "HELP_TEXT_MAN", u"Un hombre habla"},
		                                                         {1, 2, "HELP_TEXT_GOOD", u"Hola"},
		                                                         {1, 3, "HELP_TEXT_EVIL", u"Hola"},
		                                                         {1, 4, "HELP_TEXT_OTHER", u"Hola"},
		                                                         {1, 5, "HELP_TEXT_SILENT", u"Hola"}}};
		return id < k_Texts.size() ? k_Texts[id] : k_Texts[0];
	};
	help::HelpSystem::Queries queries;
	queries.textEntry = entry;
	queries.textVoice = [](uint32_t id) { return voices::Table().Get(id); };
	queries.nowMs = [&now]() { return now; };
	queries.turn = [&turn]() { return turn; };
	queries.voiceBankLoaded = [](SfxBank bank) { return voices::BankRegistered(bank); };
	queries.advisorsTalking = []() { return advisor::AnyTalking(); };
	queries.isPlaying = [](SfxBank bank, VoiceOwner owner, uint32_t sample) {
		return IsPlaying(Owner::Key(static_cast<uint32_t>(owner)), static_cast<int>(sample), bank);
	};
	help::HelpSystem::Hooks hooks;
	hooks.sayVoice = [entry](uint32_t id, help::VoiceRoute, TextVoice voice) {
		voices::RunTextVoice(entry(id).narrator, voice);
	};
	hooks.stopVoicesOnClick = []() { voices::CutByClick(); };
	hooks.spiritStop = [](int32_t spirit, int32_t arg) {
		advisor::Interrupt(spirit == 1 ? advisor::k_GoodSpirit : advisor::k_EvilSpirit, arg);
	};
	help::HelpSystem helpSystem({8, 5}, std::move(queries), std::move(hooks));

	// a villager: not read while 0x270F plays; then read 450 ms after the last look (0x5C640A)
	helpSystem.RunText(false, k_TextMan, 0);
	EXPECT_FALSE(helpSystem.IsTextRead());
	now = 3000;
	EXPECT_FALSE(helpSystem.IsTextRead()); // +0x45E4 = 3450
	End(Owner::Key(k_OwnerVoice));
	now = 3449;
	EXPECT_FALSE(helpSystem.IsTextRead());
	now = 3450;
	EXPECT_TRUE(helpSystem.IsTextRead());

	// the good spirit: read when the advisor is quiet for 200 ms (fn_005BB730)
	helpSystem.RunText(false, k_TextGood, 0);
	EXPECT_FALSE(helpSystem.IsTextRead());
	End(Owner::Key(k_OwnerAdvisor));
	EXPECT_FALSE(helpSystem.IsTextRead());
	std::this_thread::sleep_for(std::chrono::milliseconds(230));
	EXPECT_TRUE(helpSystem.IsTextRead());

	// the click (with the script's wide screen, shown for half a second: fn_005C68C0) stops the advisors
	// (fn_005C6720) and cuts the villagers' narration (0x5C6AAD)
	helpSystem.SetWideScreen(1, 7);
	helpSystem.RunText(false, k_TextMan, 0);
	EXPECT_TRUE(voices::IsSaying(false, k_TextMan));
	const int ramped = output.ramped;
	turn += 5;
	EXPECT_EQ(helpSystem.ProcessInterface(true), help::k_ClickTaken);
	EXPECT_FALSE(voices::IsSaying(false, k_TextMan));
	EXPECT_EQ(output.ramped, ramped + 1);
	helpSystem.SetWideScreen(0, 0);
}

TEST_F(VoicesTest, SpiritSpeaks)
{
	// GetSpiritWhoTalks 0x5C6E20 and ConvertScriptSpiritToHelpSpirit 0x710350
	EXPECT_EQ(help::SpiritWhoTalks(2), 1);
	EXPECT_EQ(help::SpiritWhoTalks(3), 2);
	EXPECT_EQ(help::SpiritWhoTalks(5), 0);
	const auto never = []() { return 0; };
	EXPECT_EQ(help::ConvertScriptSpiritToHelpSpirit(1, 3, never), 1); // GOOD
	EXPECT_EQ(help::ConvertScriptSpiritToHelpSpirit(2, 3, never), 2); // EVIL
	EXPECT_EQ(help::ConvertScriptSpiritToHelpSpirit(3, 2, never), 2); // ALIGNMENT, evil player
	EXPECT_EQ(help::ConvertScriptSpiritToHelpSpirit(3, 3, never), 1);
	EXPECT_EQ(help::ConvertScriptSpiritToHelpSpirit(4, 3, never), 2); // ANTI_ALIGNMENT
	EXPECT_EQ(help::ConvertScriptSpiritToHelpSpirit(4, 2, never), 1);
	EXPECT_EQ(help::ConvertScriptSpiritToHelpSpirit(5, 0, []() { return 51; }), 2); // RANDOM: > 50
	EXPECT_EQ(help::ConvertScriptSpiritToHelpSpirit(5, 0, []() { return 50; }), 1);
	EXPECT_EQ(help::ConvertScriptSpiritToHelpSpirit(0, 0, never), 1);
	EXPECT_EQ(help::ConvertScriptSpiritToHelpSpirit(9, 0, never), 1);
}

TEST(LipSync, Four1MatchesTheDft)
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
	advisor::Four1(data.data(), k_N, 1);
	for (int k = 0; k < k_N; ++k)
	{
		// Numerical Recipes' isign = 1: sum x[n] e^(+2 pi i k n / N)
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

TEST(LipSync, AnalyseAndBands)
{
	// a 300 Hz sine at 22050 Hz: Analyse's peak is near bin 300 / 22050 * 512 = 7 (and its mirror)
	constexpr int k_N = advisor::k_LipSyncWindow;
	std::vector<int16_t> pcm(k_N);
	for (int i = 0; i < k_N; ++i)
	{
		pcm[i] = static_cast<int16_t>(16000.0 * std::sin(2.0 * 3.14159265358979 * 300.0 * i / 22050.0));
	}
	std::vector<float> spectrum(2 * k_N);
	advisor::Analyse(pcm.data(), spectrum.data(), k_N);
	const auto peak = std::max_element(spectrum.begin(), spectrum.begin() + k_N / 2) - spectrum.begin();
	EXPECT_EQ(peak, 7);
	// fn_00428A80: the mean of bins [ftol(200 / 22050 * 512), ftol(400 / 22050 * 512)) = [4, 9)
	float mean = 0.0f;
	for (int i = 4; i < 9; ++i)
	{
		mean += spectrum[i];
	}
	EXPECT_NEAR(advisor::BandLevel(spectrum.data(), k_N, 22050.0f, 200.0f, 400.0f), mean / 5.0f, 1e-6);
	EXPECT_FLOAT_EQ(advisor::BandLevel(spectrum.data(), k_N, 22050.0f, 400.0f, 400.0f), 0.0f);

	// CalcKey: the first band is the loudest, its weight (index 0) goes up by at most 2 * dt * 40 * 0.18 a call
	advisor::AutoVoiceParams params;
	advisor::VoiceKey key;
	std::vector<int16_t> sentence(22050);
	for (size_t i = 0; i < sentence.size(); ++i)
	{
		sentence[i] = static_cast<int16_t>(16000.0 * std::sin(2.0 * 3.14159265358979 * 300.0 * i / 22050.0));
	}
	std::vector<float> work(2 * k_N);
	advisor::CalcKey(params, key, 0.01f, 0.5f, sentence.data(), static_cast<int>(sentence.size()), 22050.0f, work.data(),
	                 k_N);
	EXPECT_FLOAT_EQ(key.time, 0.5f);
	const float up = 2.0f * static_cast<float>(0.01 * 40.0 * static_cast<double>(0.18f));
	EXPECT_NEAR(key.weights[0], up, 1e-6);
	EXPECT_LT(key.weights[1], key.weights[0]);
	EXPECT_LT(key.weights[2], key.weights[0]);
	// a long dt: the weights reach their shape (band 0 / band 0 = 1, x the level capped at 1), normalised to <= 1
	for (int i = 0; i < 20; ++i)
	{
		advisor::CalcKey(params, key, 1.0f, 0.5f, sentence.data(), static_cast<int>(sentence.size()), 22050.0f,
		                 work.data(), k_N);
	}
	EXPECT_LE(key.weights[0] + key.weights[1] + key.weights[2], 1.0f + 1e-6f);
	EXPECT_GT(key.weights[0], 0.5f);
}

namespace
{
std::optional<std::filesystem::path> HelpSpritesPath()
{
	const char* root = std::getenv("OPENBLACK_TEST_BW_ROOT");
	if (root == nullptr || *root == '\0')
	{
		return std::nullopt;
	}
	for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(root) / "Audio" / "Dialogue"))
	{
		std::string name = entry.path().filename().string();
		std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (name == "helpsprites.sad")
		{
			return entry.path();
		}
	}
	return std::nullopt;
}
} // namespace

TEST(LazyDialogueBank, HeadersOnlyThenTheWaveAtItsFirstUse)
{
	const auto path = HelpSpritesPath();
	if (!path)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// LHBankRegister(path, 0) 0x10002240: the headers only
	pack::PackFile lazy;
	{
		std::ifstream stream(*path, std::ios::binary);
		ASSERT_EQ(lazy.ReadAudioHeaders(stream), pack::PackResult::Success);
	}
	EXPECT_EQ(lazy.GetAudioSampleHeaders().size(), 1923u);
	EXPECT_TRUE(lazy.GetAudioSamplesData().at(0).empty());
	EXPECT_FALSE(lazy.HasBlock("LHAudioWaveData"));
	ASSERT_NE(lazy.GetAudioWaveDataOffset(), 0u);
	pack::PackFile eager;
	ASSERT_EQ(eager.Open(*path), pack::PackResult::Success);
	ASSERT_EQ(eager.GetAudioSampleHeaders().size(), lazy.GetAudioSampleHeaders().size());
	for (const size_t i : {size_t {0}, size_t {646}, size_t {1922}})
	{
		const auto& header = lazy.GetAudioSampleHeader(static_cast<uint32_t>(i));
		Sound sound;
		sound.name = "HelpSprites";
		sound.waveFile = *path;
		sound.waveOffset = lazy.GetAudioWaveDataOffset() + header.offset;
		sound.waveSize = header.size;
		std::vector<uint8_t> bytes;
		ASSERT_TRUE(banks::ReadWave(sound, bytes)) << i;
		EXPECT_EQ(bytes, eager.GetAudioSampleData(static_cast<uint32_t>(i))) << i;
	}
	// HelpSprites 1 decodes from the file to about 5.2 s (milestone B0's measure) of 22050 Hz mono
	const auto& first = lazy.GetAudioSampleHeader(0);
	Sound sound;
	sound.name = "HelpSprites 1";
	sound.waveFile = *path;
	sound.waveOffset = lazy.GetAudioWaveDataOffset() + first.offset;
	sound.waveSize = first.size;
	wave_buffers::Pcm pcm;
	ASSERT_TRUE(wave_buffers::Decode(sound, pcm));
	EXPECT_EQ(pcm.sampleRate, 22050);
	EXPECT_EQ(pcm.layout, ChannelLayout::Mono);
	EXPECT_NEAR(static_cast<double>(pcm.Frames()) / pcm.sampleRate, 5.2, 0.15);
}
