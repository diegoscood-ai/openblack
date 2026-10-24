/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// the help speech service is built here, without a locator
#define LOCATOR_IMPLEMENTATIONS

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <PackFile.h>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Audio/Audio.h"
#include "Audio/Device/SampleOutput.h"
#include "Audio/Device/Sound.h"
#include "Audio/Device/WaveBuffers.h"
#include "Audio/Game/Banks.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/Voices.h"
#include "Common/HelpText.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/Implementations/HelpSpeechSystem.h"
#include "ECS/Systems/Implementations/HelpTextSystem.h"
#include "FileSystem/DefaultFileSystem.h"
#include "Help/AdvisorVoices.h"
#include "Help/HelpSystem.h"
#include "Help/ScriptSpirits.h"
#include "Help/Spirits.h"
#include "Locator.h"
#include "support/RestoreService.h"

// The voices on the channels. RUN_TEXT's voice (the advisors with k_OwnerAdvisor, played directly, the rest 2D with
// k_OwnerVoice through PlaySoundEffect), SAY_SOUND / SAY_SOUND_EFFECT_PLAYING, the click's cut (villagers only, with
// the ramped stop), TEXT_READ with a voice (+450 ms after the narration, +200 ms after an advisor) and the advisors'
// sentences on the engine (the lip-sync analysis is test_lip_sync.cpp, the voices' rules on fakes are
// test_advisor_voices.cpp). A fake output stands for the sound device. The installation tests (the lazy dialogue
// banks) need OPENBLACK_TEST_BW_ROOT; their *Synthetic twin uses a small bank written by the test.

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
	[[nodiscard]] int64_t PlayPositionMs(size_t channel) const override { return playing[channel] ? position[channel] : -1; }
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

/// The advisors' slot with only their control: what the help texts' voices give the spirits
class FakeAdvisors final: public ecs::systems::AdvisorSystemInterface
{
public:
	help::spirits::DudeData good;
	help::spirits::DudeData evil;
	help::spirits::AdvisorSpiritController control {good, evil, {}, {}};

	void Load() override {}
	[[nodiscard]] bool IsLoaded() const override { return true; }
	void Update(const Frame&) override {}
	void ProcessTurn() override {}
	[[nodiscard]] help::spirits::AdvisorSpiritController& GetController() override { return control; }
	[[nodiscard]] const help::spirits::AdvisorSpiritController& GetController() const override { return control; }
	void FillOverlay(graphics::OverlayFrame&) override {}
	[[nodiscard]] const help::HelpDudeFile* GetFile(int) const override { return nullptr; }
};

void PutWaveU32(std::vector<uint8_t>& out, uint32_t v)
{
	for (int i = 0; i < 4; ++i)
	{
		out.push_back(static_cast<uint8_t>(v >> (8 * i)));
	}
}

/// A chunk with its body, padded to an even length
void PutWaveChunk(std::vector<uint8_t>& out, const char* id, const std::vector<uint8_t>& body)
{
	out.insert(out.end(), id, id + 4);
	PutWaveU32(out, static_cast<uint32_t>(body.size()));
	out.insert(out.end(), body.begin(), body.end());
	if ((body.size() & 1) != 0)
	{
		out.push_back(0);
	}
}

/// A RIFF wave of 0.1 s of silent 16-bit mono PCM at 22050 Hz; with a label, one cue point at `cueAt` samples and its
/// label in an adtl list after the data, as the voices' waves carry their gesture tags
std::vector<uint8_t> LabelledWave(const std::string& label, uint32_t cueAt)
{
	std::vector<uint8_t> body {'W', 'A', 'V', 'E'};
	std::vector<uint8_t> format;
	PutWaveU32(format, 0x00010001); // PCM, one channel
	PutWaveU32(format, 22050);
	PutWaveU32(format, 44100);
	PutWaveU32(format, 0x00100002); // 2 bytes a frame, 16 bits
	PutWaveChunk(body, "fmt ", format);
	PutWaveChunk(body, "data", std::vector<uint8_t>(2 * 2205, 0));
	if (!label.empty())
	{
		std::vector<uint8_t> cue;
		PutWaveU32(cue, 1);
		PutWaveU32(cue, 1); // the point's id
		PutWaveU32(cue, cueAt);
		cue.insert(cue.end(), {'d', 'a', 't', 'a'});
		PutWaveU32(cue, 0);
		PutWaveU32(cue, 0);
		PutWaveU32(cue, cueAt);
		PutWaveChunk(body, "cue ", cue);
		std::vector<uint8_t> labl;
		PutWaveU32(labl, 1);
		labl.insert(labl.end(), label.begin(), label.end());
		labl.push_back(0);
		std::vector<uint8_t> list {'a', 'd', 't', 'l'};
		PutWaveChunk(list, "labl", labl);
		PutWaveChunk(body, "LIST", list);
	}
	std::vector<uint8_t> wave;
	PutWaveChunk(wave, "RIFF", body);
	return wave;
}

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
	/// The help texts slot that holds the advisors' voices, which RunTextVoice reaches through the locator
	test::RestoreService<Locator::helpTextSystem> restoreHelpTexts;
	help::AdvisorVoices* advisors {nullptr};

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
		sample_play::SetMainVolume(127);
		audio::ClearMap();
		output = FakeOutput {};
		for (int n = 1; n <= 3; ++n)
		{
			Add(helpSprites, n);
		}
		Add(villagers, 1);
		Add(villagers, 2);
		// villagers 1 is HELP_TEXT_MAN; HelpSprites 1..3 the spirits' and the other narrator's
		voices::SetTable(VoiceTable::Build(
		    {"HELP_TEXT_NONE", "HELP_TEXT_MAN", "HELP_TEXT_GOOD", "HELP_TEXT_EVIL", "HELP_TEXT_OTHER", "HELP_TEXT_SILENT"},
		    {"K:\\4frosty\\Spanish\\1622p\\HELP_TEXT_MAN.wav", "K:\\x\\HELP_TEXT_X.wav"},
		    {"K:\\x\\HELP_TEXT_GOOD.wav", "K:\\x\\HELP_TEXT_EVIL.wav", "K:\\x\\HELP_TEXT_OTHER.wav"}, {}));
		auto& texts = Locator::helpTextSystem::emplace<ecs::systems::HelpTextSystem>();
		texts.InitVoices(helpSprites);
		advisors = &texts.GetVoices();
		advisors->Reset();
	}
	void TearDown() override
	{
		advisors->Reset();
		audio::SetGameSound(true);
		audio::ClearMap();
		sample_play::SetMainVolume(127); // back to its default (the maximum)
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
	// a villager's text: PlaySoundEffect with k_OwnerVoice, 2D
	const auto channel = voices::RunTextVoice(5, voices::Table().Get(k_TextMan));
	ASSERT_NE(channel, k_NoChannel);
	const auto info = Channel(Owner::Key(k_OwnerVoice));
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->bank, villagers);
	EXPECT_EQ(info->sample, 1);
	EXPECT_FALSE(info->is3D);
	// a HelpSprites text of another narrator takes the same branch
	EXPECT_NE(voices::RunTextVoice(4, voices::Table().Get(k_TextOther)), k_NoChannel);
	EXPECT_EQ(advisors->GetSpeaker(), -1);
	// no voice: nothing
	EXPECT_EQ(voices::RunTextVoice(5, voices::Table().Get(k_TextSilent)), k_NoChannel);
}

TEST_F(VoicesTest, NarrationIsFilteredButAdvisorIsNot)
{
	// inside the citadel only user parameter 2 plays: the narration of villagers (0) is skipped...
	s_InsideCitadel = true;
	EXPECT_EQ(voices::RunTextVoice(5, voices::Table().Get(k_TextMan)), k_NoChannel);
	// ...but the advisor's sample is played directly
	voices::RunTextVoice(helptext::k_NarratorGoodSpirit, voices::Table().Get(k_TextGood));
	const auto info = Channel(Owner::Key(k_OwnerAdvisor));
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->bank, helpSprites);
	EXPECT_EQ(info->sample, 1);
	EXPECT_FALSE(info->is3D);
}

TEST_F(VoicesTest, SpiritTextsGoToTheirAdvisor)
{
	// narrator 2 -> dude 0, k_OwnerAdvisor; with the advisor's flight not ported, the delay is 0 and the sentence
	// starts at once (UpdateSaySentence from SaySentence)
	voices::RunTextVoice(helptext::k_NarratorGoodSpirit, voices::Table().Get(k_TextGood));
	EXPECT_EQ(advisors->GetSpeaker(), help::AdvisorVoices::k_GoodSpirit);
	EXPECT_EQ(advisors->GetSentence(), 1);
	EXPECT_TRUE(advisors->IsActive(help::AdvisorVoices::k_GoodSpirit));
	EXPECT_TRUE(advisors->IsTalking(help::AdvisorVoices::k_GoodSpirit));
	EXPECT_FALSE(advisors->IsTalking(help::AdvisorVoices::k_EvilSpirit)); // only the speaker talks
	EXPECT_TRUE(advisors->AnyTalking());
	// narrator 3: both advisors stopped (1 then 0), the evil one says HelpSprites 2
	voices::RunTextVoice(helptext::k_NarratorEvilSpirit, voices::Table().Get(k_TextEvil));
	EXPECT_EQ(advisors->GetSpeaker(), help::AdvisorVoices::k_EvilSpirit);
	EXPECT_EQ(advisors->GetSentence(), 2);
	EXPECT_FALSE(advisors->IsActive(help::AdvisorVoices::k_GoodSpirit));
	const auto info = Channel(Owner::Key(k_OwnerAdvisor));
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->sample, 2);
	EXPECT_EQ(output.ramped, 1); // the good one's sentence stopped with the ramp (StopSentence)
}

TEST_F(VoicesTest, AdvisorSentenceRules)
{
	// a sample outside 1..count is not said, but the dude is the speaker
	advisors->SaySentence(help::AdvisorVoices::k_GoodSpirit, 4, false, 0);
	EXPECT_EQ(advisors->GetSentence(), 0);
	EXPECT_EQ(advisors->GetSpeaker(), help::AdvisorVoices::k_GoodSpirit);
	advisors->SaySentence(help::AdvisorVoices::k_GoodSpirit, 0, false, 0);
	EXPECT_EQ(output.plays, 0);
	// onlyIfSilent while talking: nothing
	advisors->SaySentence(help::AdvisorVoices::k_GoodSpirit, 1, false, 0);
	advisors->SaySentence(help::AdvisorVoices::k_GoodSpirit, 2, true, 0);
	EXPECT_EQ(advisors->GetSentence(), 1);
	// without it: the sentence stops and the new one starts; as in the original, StopSentence runs after the dude
	// becomes the speaker and clears it, so the new sentence has no speaker (RUN_TEXT's path stops both advisors
	// first and never meets this)
	advisors->SaySentence(help::AdvisorVoices::k_GoodSpirit, 2, false, 0);
	EXPECT_EQ(advisors->GetSentence(), 2);
	EXPECT_EQ(advisors->GetSpeaker(), -1);
	EXPECT_FALSE(advisors->IsTalking(help::AdvisorVoices::k_GoodSpirit));
	// a delay: IsTalking already, nothing on the channels until the time comes
	advisors->Reset();
	const int plays = output.plays;
	advisors->SaySentence(help::AdvisorVoices::k_GoodSpirit, 3, false, 60);
	EXPECT_TRUE(advisors->IsTalking(help::AdvisorVoices::k_GoodSpirit));
	EXPECT_FLOAT_EQ(advisors->PercentageDone(help::AdvisorVoices::k_GoodSpirit), 0.0f);
	advisors->UpdateSaySentence(help::AdvisorVoices::k_GoodSpirit);
	EXPECT_EQ(output.plays, plays);
	std::this_thread::sleep_for(std::chrono::milliseconds(80));
	advisors->UpdateSaySentence(help::AdvisorVoices::k_GoodSpirit);
	EXPECT_EQ(output.plays, plays + 1);
	EXPECT_EQ(advisors->GetSentence(), 3);
}

TEST_F(VoicesTest, AdvisorTalksUntil200MsAfterItsSentence)
{
	advisors->Say(help::AdvisorVoices::k_GoodSpirit, 1, false);
	EXPECT_TRUE(advisors->TalkingOrJustStopped(help::AdvisorVoices::k_GoodSpirit));
	// the wave ends: IsTalking stops the sentence but TalkingOrJustStopped holds 200 ms
	End(Owner::Key(k_OwnerAdvisor));
	EXPECT_FALSE(advisors->IsTalking(help::AdvisorVoices::k_GoodSpirit));
	EXPECT_EQ(advisors->GetSentence(), 0);
	EXPECT_EQ(advisors->GetSpeaker(), -1);
	EXPECT_TRUE(advisors->AnyTalking());
	std::this_thread::sleep_for(std::chrono::milliseconds(230));
	EXPECT_FALSE(advisors->AnyTalking());
}

TEST_F(VoicesTest, ALinesCueLabelsGiveItsAdvisorItsTagsWhichItsStopFires)
{
	test::RestoreService<Locator::advisorSystem> restoreAdvisors;
	auto& fake = static_cast<FakeAdvisors&>(Locator::advisorSystem::emplace<FakeAdvisors>());
	auto& good = fake.control.Dude(help::spirits::k_GoodDude);
	auto& evil = fake.control.Dude(help::spirits::k_EvilDude);
	sounds.at(SampleId(helpSprites, 1)).buffer = {LabelledWave("well [TE sad]", 1102)};
	sounds.at(SampleId(helpSprites, 2)).buffer = {LabelledWave("", 0)};
	// The good advisor's line: the tag of its wave's label goes to it, none to the other
	advisors->Say(help::AdvisorVoices::k_GoodSpirit, 1, false);
	EXPECT_EQ(advisors->GetSentence(), 1);
	EXPECT_EQ(good.TagsLeft(), 1u);
	EXPECT_EQ(evil.TagsLeft(), 0u);
	EXPECT_EQ(good.EmotionTarget(), 0u);
	// The wave ends before the tag's time is seen: the line's stop fires it on the advisor that said it
	End(Owner::Key(k_OwnerAdvisor));
	EXPECT_FALSE(advisors->IsTalking(help::AdvisorVoices::k_GoodSpirit));
	EXPECT_EQ(good.TagsLeft(), 0u);
	EXPECT_EQ(good.EmotionTarget(), 3u); // Sad
	// A wave without cue points: the line has no tags
	advisors->Say(help::AdvisorVoices::k_EvilSpirit, 2, false);
	EXPECT_EQ(advisors->GetSentence(), 2);
	EXPECT_EQ(evil.TagsLeft(), 0u);
	EXPECT_EQ(evil.EmotionTarget(), 0u);
}

TEST_F(VoicesTest, PercentageDoneIsPositionOverLength)
{
	advisors->Say(help::AdvisorVoices::k_GoodSpirit, 1, false);
	const auto info = Channel(Owner::Key(k_OwnerAdvisor));
	ASSERT_TRUE(info.has_value());
	// the ms of the position over the wave's length (2 s here)
	output.position[(info->handle - 1) % 16] = 500;
	EXPECT_FLOAT_EQ(advisors->PercentageDone(help::AdvisorVoices::k_GoodSpirit), 0.25f);
	EXPECT_EQ(sample_play::PlayPosition(helpSprites, Owner::Key(k_OwnerAdvisor)), 500);
	// a dude that is not the speaker: 1
	EXPECT_FLOAT_EQ(advisors->PercentageDone(help::AdvisorVoices::k_EvilSpirit), 1.0f);
}

TEST_F(VoicesTest, InterruptionIsNeverSaidInVersion120)
{
	// Interrupt(dude, 1): Stop(dude) first, so PercentageDone sees no speaker (1 >= 0.9) and says nothing
	advisors->Say(help::AdvisorVoices::k_GoodSpirit, 1, false);
	const int plays = output.plays;
	advisors->Interrupt(help::AdvisorVoices::k_GoodSpirit, 1);
	EXPECT_FALSE(advisors->IsActive(help::AdvisorVoices::k_GoodSpirit));
	EXPECT_FALSE(advisors->IsTalking(help::AdvisorVoices::k_GoodSpirit));
	EXPECT_EQ(output.plays, plays);
	EXPECT_FALSE(Channel(Owner::Key(k_OwnerAdvisor)).has_value());
	// a dude that is not active: nothing at all
	advisors->Interrupt(help::AdvisorVoices::k_EvilSpirit, 1);
	EXPECT_EQ(output.plays, plays);
}

TEST_F(VoicesTest, SayOwnersPositionAndPlaying)
{
	// alt -> k_OwnerVoiceAlt, withPos -> 3D at the point, track 0
	ASSERT_NE(voices::Say(k_TextMan, true, true, glm::vec3(3.0f, 1.0f, 4.0f)), k_NoChannel);
	auto info = Channel(Owner::Key(k_OwnerVoiceAlt));
	ASSERT_TRUE(info.has_value());
	EXPECT_TRUE(info->is3D);
	EXPECT_FALSE(info->track);
	EXPECT_EQ(output.starts.at((info->handle - 1) % 16).position, glm::vec3(3.0f, 1.0f, 4.0f));
	EXPECT_TRUE(voices::IsSaying(true, k_TextMan));
	EXPECT_FALSE(voices::IsSaying(false, k_TextMan));
	// without position: 2D with k_OwnerVoice, even a spirit's text (no advisor)
	ASSERT_NE(voices::Say(k_TextGood, false, false, glm::vec3(9.0f)), k_NoChannel);
	info = Channel(Owner::Key(k_OwnerVoice));
	ASSERT_TRUE(info.has_value());
	EXPECT_FALSE(info->is3D);
	EXPECT_EQ(info->bank, helpSprites);
	EXPECT_EQ(advisors->GetSpeaker(), -1);
	EXPECT_TRUE(voices::IsSaying(false, k_TextGood));
	// no voice, or a text past the table (-> text 0): nothing
	EXPECT_EQ(voices::Say(k_TextSilent, false, false, glm::vec3(0.0f)), k_NoChannel);
	EXPECT_EQ(voices::Say(7000, false, false, glm::vec3(0.0f)), k_NoChannel);
	EXPECT_FALSE(voices::IsSaying(false, 7000));
}

TEST_F(VoicesTest, ClickCutsOnlyTheVillagersNarration)
{
	voices::RunTextVoice(5, voices::Table().Get(k_TextMan));   // villagers, k_OwnerVoice
	voices::RunTextVoice(4, voices::Table().Get(k_TextOther)); // HelpSprites, k_OwnerVoice
	voices::Say(k_TextMan, false, true, glm::vec3(0.0f));      // villagers, k_OwnerVoiceAlt
	voices::CutByClick();
	EXPECT_EQ(output.ramped, 1); // stopped with the 20 ms ramp
	EXPECT_FALSE(voices::IsSaying(false, k_TextMan));
	EXPECT_TRUE(voices::IsSaying(false, k_TextOther));
	EXPECT_TRUE(voices::IsSaying(true, k_TextMan));
}

TEST_F(VoicesTest, HelpSpeechSystemCountsTheSpokenTexts)
{
	// the man's line in villagers and the three in HelpSprites; none for HELP_TEXT_NONE and HELP_TEXT_SILENT
	ecs::systems::HelpSpeechSystem speech;
	EXPECT_EQ(speech.GetSpokenTextCount(), 4u);
	// none before the table is built
	voices::SetTable({});
	EXPECT_EQ(speech.GetSpokenTextCount(), 0u);
}

TEST_F(VoicesTest, HelpSpeechSystemSaysOnTheVoices)
{
	// the first voice is the narration's owner, the second the extra one; a position makes the line 3D there
	ecs::systems::HelpSpeechSystem speech;
	speech.Say(k_TextMan, SpeechVoice::Second, glm::vec3(3.0f, 1.0f, 4.0f));
	auto info = Channel(Owner::Key(k_OwnerVoiceAlt));
	ASSERT_TRUE(info.has_value());
	EXPECT_TRUE(info->is3D);
	EXPECT_EQ(output.starts.at((info->handle - 1) % 16).position, glm::vec3(3.0f, 1.0f, 4.0f));
	EXPECT_TRUE(speech.IsSaying(k_TextMan, SpeechVoice::Second));
	EXPECT_FALSE(speech.IsSaying(k_TextMan, SpeechVoice::First));
	speech.Say(k_TextOther, SpeechVoice::First, std::nullopt);
	info = Channel(Owner::Key(k_OwnerVoice));
	ASSERT_TRUE(info.has_value());
	EXPECT_FALSE(info->is3D);
	EXPECT_TRUE(speech.IsSaying(k_TextOther, SpeechVoice::First));
	// a stop reaches only its voice's lines from its bank
	speech.Stop(SpeechVoice::First, SpeechBank::Villagers);
	EXPECT_TRUE(speech.IsSaying(k_TextMan, SpeechVoice::Second));
	EXPECT_TRUE(speech.IsSaying(k_TextOther, SpeechVoice::First));
	speech.Stop(SpeechVoice::Second, SpeechBank::Villagers);
	EXPECT_FALSE(speech.IsSaying(k_TextMan, SpeechVoice::Second));
	speech.Stop(SpeechVoice::First, SpeechBank::HelpSprites);
	EXPECT_FALSE(speech.IsSaying(k_TextOther, SpeechVoice::First));
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
	queries.advisorsTalking = [this]() { return advisors->AnyTalking(); };
	queries.isPlaying = [](SfxBank bank, VoiceOwner owner, uint32_t sample) {
		return IsPlaying(Owner::Key(static_cast<uint32_t>(owner)), static_cast<int>(sample), bank);
	};
	help::HelpSystem::Hooks hooks;
	hooks.sayVoice = [entry](uint32_t id, help::VoiceRoute, TextVoice voice) {
		voices::RunTextVoice(entry(id).narrator, voice);
	};
	hooks.stopVoicesOnClick = []() { voices::CutByClick(); };
	hooks.interruptAdvisors = [this]() {
		advisors->Interrupt(help::AdvisorVoices::k_GoodSpirit, 1);
		advisors->Interrupt(help::AdvisorVoices::k_EvilSpirit, 1);
	};
	std::unique_ptr<help::DialogueText> dialogue;
	help::HelpSystem helpSystem({8, 5}, std::move(queries), std::move(hooks), dialogue);

	// a villager: not read while k_OwnerVoice plays; then read 450 ms after the last look
	helpSystem.RunText(false, k_TextMan, 0);
	EXPECT_FALSE(helpSystem.IsTextRead());
	now = 3000;
	EXPECT_FALSE(helpSystem.IsTextRead()); // read from 3450
	End(Owner::Key(k_OwnerVoice));
	now = 3449;
	EXPECT_FALSE(helpSystem.IsTextRead());
	now = 3450;
	EXPECT_TRUE(helpSystem.IsTextRead());

	// the good spirit: read when the advisor is quiet for 200 ms
	helpSystem.RunText(false, k_TextGood, 0);
	EXPECT_FALSE(helpSystem.IsTextRead());
	End(Owner::Key(k_OwnerAdvisor));
	EXPECT_FALSE(helpSystem.IsTextRead());
	std::this_thread::sleep_for(std::chrono::milliseconds(230));
	EXPECT_TRUE(helpSystem.IsTextRead());

	// the click (with the script's wide screen, shown for half a second) stops the advisors and cuts the villagers'
	// narration
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
	// SpiritWhoTalks and HelpSpiritOf
	EXPECT_EQ(help::script_spirits::SpiritWhoTalks(2), 1);
	EXPECT_EQ(help::script_spirits::SpiritWhoTalks(3), 2);
	EXPECT_EQ(help::script_spirits::SpiritWhoTalks(5), 0);
	const auto never = []() { return 0U; };
	EXPECT_EQ(help::script_spirits::HelpSpiritOf(1, 3, never), 1); // GOOD
	EXPECT_EQ(help::script_spirits::HelpSpiritOf(2, 3, never), 2); // EVIL
	EXPECT_EQ(help::script_spirits::HelpSpiritOf(3, 2, never), 2); // ALIGNMENT, evil player
	EXPECT_EQ(help::script_spirits::HelpSpiritOf(3, 3, never), 1);
	EXPECT_EQ(help::script_spirits::HelpSpiritOf(4, 3, never), 2); // ANTI_ALIGNMENT
	EXPECT_EQ(help::script_spirits::HelpSpiritOf(4, 2, never), 1);
	EXPECT_EQ(help::script_spirits::HelpSpiritOf(5, 0, []() { return 51U; }), 2); // RANDOM: > 50
	EXPECT_EQ(help::script_spirits::HelpSpiritOf(5, 0, []() { return 50U; }), 1);
	EXPECT_EQ(help::script_spirits::HelpSpiritOf(0, 0, never), 1);
	EXPECT_EQ(help::script_spirits::HelpSpiritOf(9, 0, never), 1);
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
		std::transform(name.begin(), name.end(), name.begin(),
		               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (name == "helpsprites.sad")
		{
			return entry.path();
		}
	}
	return std::nullopt;
}
} // namespace

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(LazyDialogueBank, HeadersOnlyThenTheWaveAtItsFirstUse)
{
	// the bank's waves are read through the file system: one over the machine's folders, restored when the test ends
	const test::RestoreService<Locator::filesystem> restoreFilesystem;
	Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
	const auto path = HelpSpritesPath();
	if (!path)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// a lazy bank: the headers only
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
	// HelpSprites 1 decodes from the file to about 5.2 s (measured) of 22050 Hz mono
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

namespace
{
/// The running test's suite and name and the process id: parallel test processes never share a temporary file
std::string UniqueSuffix()
{
	const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
#ifdef _WIN32
	const auto pid = _getpid();
#else
	const auto pid = getpid();
#endif
	return std::string(info->test_suite_name()) + "_" + info->name() + "_" + std::to_string(pid);
}

/// Removes the file the test wrote when it goes out of scope
struct TempFile
{
	std::filesystem::path path;
	explicit TempFile(std::filesystem::path p)
	    : path(std::move(p))
	{
	}
	TempFile(const TempFile&) = delete;
	TempFile& operator=(const TempFile&) = delete;
	~TempFile()
	{
		std::error_code ec;
		std::filesystem::remove(path, ec);
	}
};

void WriteBlock(std::ofstream& out, const char* name, const std::vector<uint8_t>& data)
{
	std::array<char, 32> blockName {};
	std::strncpy(blockName.data(), name, blockName.size() - 1);
	out.write(blockName.data(), blockName.size());
	const auto size = static_cast<uint32_t>(data.size());
	out.write(reinterpret_cast<const char*>(&size), sizeof(size));
	out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

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

/// A RIFF wave of format 0x50 holding `frames` silent mono MPEG-2 layer II frames at 22050 Hz and 64 kbps (the
/// frame header: no CRC, bitrate index 8, mono; zero bits after it allocate no subband)
std::vector<uint8_t> MpegRiff(int frames)
{
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
	std::vector<uint8_t> bytes = {'R', 'I', 'F', 'F'};
	PutU32(bytes, static_cast<uint32_t>(4 + 8 + 16 + 8 + data.size() + (data.size() & 1)));
	bytes.insert(bytes.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
	PutU32(bytes, 16);
	PutU16(bytes, 0x50); // format
	PutU16(bytes, 1);    // channels
	PutU32(bytes, 22050);
	PutU32(bytes, 8000); // bytes per second at 64 kbps
	PutU16(bytes, 1);
	PutU16(bytes, 0);
	bytes.insert(bytes.end(), {'d', 'a', 't', 'a'});
	PutU32(bytes, static_cast<uint32_t>(data.size()));
	bytes.insert(bytes.end(), data.begin(), data.end());
	if ((data.size() & 1) != 0)
	{
		bytes.push_back(0);
	}
	return bytes;
}

/// A sound bank of these waves: the wave data block, then the sample table (u16 count, u16 0, the records)
std::filesystem::path WriteSoundBank(const std::vector<std::vector<uint8_t>>& waves)
{
	const auto path = std::filesystem::path(TEST_BINARY_DIR) / ("lazy_bank_" + UniqueSuffix() + ".sad");
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write("LiOnHeAd", 8);
	std::vector<uint8_t> wave;
	std::vector<uint8_t> table;
	PutU16(table, static_cast<uint16_t>(waves.size()));
	PutU16(table, 0);
	for (size_t i = 0; i < waves.size(); ++i)
	{
		pack::AudioBankSampleHeader header;
		std::memset(&header, 0, sizeof(header));
		header.id = static_cast<int32_t>(i + 1);
		header.offset = static_cast<uint32_t>(wave.size());
		header.size = static_cast<uint32_t>(waves[i].size());
		header.sampleRate = 22050;
		wave.insert(wave.end(), waves[i].begin(), waves[i].end());
		const auto* raw = reinterpret_cast<const uint8_t*>(&header);
		table.insert(table.end(), raw, raw + sizeof(header));
	}
	WriteBlock(out, "LHAudioWaveData", wave);
	WriteBlock(out, "LHAudioBankSampleTable", table);
	return path;
}
} // namespace

TEST(LazyDialogueBank, HeadersOnlyThenTheWaveAtItsFirstUseSynthetic)
{
	// the bank's waves are read through the file system: one over the machine's folders, restored when the test ends
	const test::RestoreService<Locator::filesystem> restoreFilesystem;
	Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
	// Three samples: a RIFF MPEG wave of 10 frames and two short byte runs
	const std::vector<std::vector<uint8_t>> waves = {MpegRiff(10), {1, 2, 3}, {4, 5, 6, 7, 8}};
	const TempFile file {WriteSoundBank(waves)};
	// the headers only
	pack::PackFile lazy;
	{
		std::ifstream stream(file.path, std::ios::binary);
		ASSERT_EQ(lazy.ReadAudioHeaders(stream), pack::PackResult::Success);
	}
	EXPECT_EQ(lazy.GetAudioSampleHeaders().size(), waves.size());
	EXPECT_TRUE(lazy.GetAudioSamplesData().at(0).empty());
	EXPECT_FALSE(lazy.HasBlock("LHAudioWaveData"));
	ASSERT_NE(lazy.GetAudioWaveDataOffset(), 0u);
	pack::PackFile eager;
	ASSERT_EQ(eager.Open(file.path), pack::PackResult::Success);
	ASSERT_EQ(eager.GetAudioSampleHeaders().size(), lazy.GetAudioSampleHeaders().size());
	for (size_t i = 0; i < waves.size(); ++i)
	{
		const auto& header = lazy.GetAudioSampleHeader(static_cast<uint32_t>(i));
		Sound sound;
		sound.name = "Synthetic";
		sound.waveFile = file.path;
		sound.waveOffset = lazy.GetAudioWaveDataOffset() + header.offset;
		sound.waveSize = header.size;
		std::vector<uint8_t> bytes;
		ASSERT_TRUE(banks::ReadWave(sound, bytes)) << i;
		EXPECT_EQ(bytes, eager.GetAudioSampleData(static_cast<uint32_t>(i))) << i;
		EXPECT_EQ(bytes, waves[i]) << i;
	}
	// the first wave decodes from the file: 10 frames of 1152 samples, 22050 Hz mono
	const auto& first = lazy.GetAudioSampleHeader(0);
	Sound sound;
	sound.name = "Synthetic 1";
	sound.waveFile = file.path;
	sound.waveOffset = lazy.GetAudioWaveDataOffset() + first.offset;
	sound.waveSize = first.size;
	wave_buffers::Pcm pcm;
	ASSERT_TRUE(wave_buffers::Decode(sound, pcm));
	EXPECT_EQ(pcm.sampleRate, 22050);
	EXPECT_EQ(pcm.layout, ChannelLayout::Mono);
	EXPECT_EQ(pcm.Frames(), 10u * 1152u);
}
