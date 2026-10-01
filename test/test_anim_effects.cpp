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

#include "Audio/AnimEffectBank.h"
#include "Audio/Audio.h"
#include "Audio/GameQueries.h"
#include "Audio/SampleOutput.h"
#include "Audio/Sound.h"
#include "Resources/Loaders.h"

// Milestone B2 of dev\tmp_dis\audio\PLAN.md: the anim effects in the audio core. The tables of a bank read once as it
// is registered (LHBankRegister 0x10002778), LHSampleGetAnimEffectNumber 0x10014670 (LH_AudioSystem::Rand 0x10015710),
// GAudio::SamplePlayAnimEffect 0x42A4B0 with LHSamplePlayAnimEffect 0x10014A20 (the caller's distance against 800 and
// the sample's max distance, the owner's point) and 0x100146F0 (stop / release the row's samples). Real editor.sad and
// VillagersBanter.sad (OPENBLACK_TEST_BW_ROOT; without it the tests skip). A fake output stands for QMixer.

using namespace openblack;
using namespace openblack::audio;

namespace
{
class FakeOutput final: public SampleOutput
{
public:
	std::array<bool, 16> playing {};
	std::array<bool, 16> released {};
	std::array<Start, 16> starts {};
	int plays {0};

	bool Play(size_t channel, Sound&, const Start& start) override
	{
		playing[channel] = true;
		released[channel] = false;
		starts[channel] = start;
		++plays;
		return true;
	}
	void Stop(size_t channel) override { playing[channel] = false; }
	[[nodiscard]] bool Playing(size_t channel) const override { return playing[channel]; }
	void SetGain(size_t, float) override {}
	void SetPitch(size_t, float) override {}
	void SetPosition(size_t, glm::vec3) override {}
	void ReleaseLoop(size_t channel) override { released[channel] = true; }
	void SetListener(glm::vec3) override {}
	void Update() override {}
	[[nodiscard]] size_t Sources() const override { return 0; }
	void DeleteAll() override { playing.fill(false); }
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

constexpr auto k_Villager = static_cast<entt::entity>(7);
constexpr auto k_Abode = static_cast<entt::entity>(8);

class AnimEffectsTest: public ::testing::Test
{
protected:
	FakeOutput output;
	std::map<entt::id_type, Sound> sounds;
	static inline std::map<uint32_t, glm::vec3> s_Things;
	static inline int s_Rand {16383};
	BankId editor {k_NoBank};
	BankId banter {k_NoBank};

	void SetUp() override
	{
		const auto root = GameRoot();
		if (!root)
		{
			GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
		}
		s_Things.clear();
		s_Rand = 16383;
		GameQueries queries;
		queries.thingPosition = [](ThingId thing) -> std::optional<glm::vec3> {
			const auto found = s_Things.find(thing);
			return found != s_Things.end() ? std::optional<glm::vec3>(found->second) : std::nullopt;
		};
		queries.camera = []() -> std::optional<CameraState> { return CameraState {}; };
		audio::Init(std::move(queries));
		editor = Load(*root, "Audio/Sfx/Game/editor.sad", "editor.sad");
		banter = Load(*root, "Audio/Dialogue/VillagersBanter.sad", "VillagersBanter.sad");
		sample_play::Backend backend;
		backend.output = &output;
		backend.sound = [this](entt::id_type id) -> Sound* {
			const auto found = sounds.find(id);
			return found != sounds.end() ? &found->second : nullptr;
		};
		backend.rand = []() { return s_Rand; };
		backend.camera = []() -> std::optional<glm::vec3> { return glm::vec3(0.0f); };
		backend.ownerPosition = [](const Owner& owner) { return OwnerSoundPosition(owner); };
		sample_play::SetBackend(std::move(backend));
		sample_play::SetMasterVolume(127);
		audio::ClearMap();
		output = FakeOutput {};
	}
	void TearDown() override
	{
		if (!GameRoot())
		{
			return;
		}
		audio::ClearMap();
		sample_play::SetBackend({});
		audio::Shutdown();
	}

	/// Game's bank loop: RegisterBank, the anim effect tables, the samples' records
	BankId Load(const std::filesystem::path& root, const std::string& relative, const std::string& group)
	{
		pack::PackFile pack;
		EXPECT_EQ(pack.Open(root / relative), pack::PackResult::Success) << relative;
		const auto bank = RegisterBank(relative, group);
		anim_effects::RegisterTables(bank, pack);
		const auto& headers = pack.GetAudioSampleHeaders();
		for (const auto& header : headers)
		{
			const auto sound = resources::SoundLoader {}(resources::SoundLoader::FromBufferTag {}, header, {});
			sound->bank = bank;
			sounds.emplace(SampleId(bank, header.id), *sound);
		}
		return bank;
	}
	int SampleOn(size_t channel) const
	{
		const auto infos = sample_play::Channels();
		return infos[channel].sample;
	}
};
} // namespace

TEST_F(AnimEffectsTest, TablesReadOnceByBank)
{
	const auto* tables = anim_effects::Tables(editor);
	ASSERT_NE(tables, nullptr);
	EXPECT_EQ(tables->rows.size(), 201u); // the count the old AnimationSounds logged ("201 + 3 effect rows")
	ASSERT_NE(anim_effects::Tables(banter), nullptr);
	EXPECT_EQ(anim_effects::Tables(banter)->rows.size(), 3u);
	EXPECT_EQ(tables->name, "editor.sad");
	// the miracles' alias is the same type
	const AnimEffectBank* alias = tables;
	EXPECT_EQ(alias->FindList({1, 2, 1, 2, 4}), tables->FindList({1, 2, 1, 2, 4}));
	// a second registration of the same bank reads nothing more
	const auto* before = anim_effects::Tables(editor);
	pack::PackFile pack;
	anim_effects::RegisterTables(editor, pack);
	EXPECT_EQ(anim_effects::Tables(editor), before);
}

TEST_F(AnimEffectsTest, NumberUsesTheDllRand)
{
	// a man's footstep on gravel: {1, 2, 1, 2, 4} -> H_Footstep_Gravel 259..268 (sounds_props.md B)
	const AnimKey key = {1, 2, 1, 2, 4};
	const auto list = anim_effects::Tables(editor)->FindList(key);
	ASSERT_EQ(list.size(), 10u);
	EXPECT_EQ(list.front(), 259);
	// LH_AudioSystem::Rand(10) 0x10015710 = rand * 10 / 32767
	s_Rand = 16383;
	EXPECT_EQ(anim_effects::Number(key, editor), list[4]);
	s_Rand = 0;
	EXPECT_EQ(anim_effects::Number(key, editor), list[0]);
	s_Rand = 32766;
	EXPECT_EQ(anim_effects::Number(key, editor), list[9]);
	EXPECT_EQ(anim_effects::Number({1, 2, 1, 2, 9999}, editor), 0); // no row
}

TEST_F(AnimEffectsTest, PlayGatesOnTheCallersDistance)
{
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(3.0f, 0.0f, 4.0f);
	const AnimKey key = {1, 2, 1, 2, 4};
	const float maxDistance = sounds.at(SampleId(editor, 263)).maxDistance; // the footsteps' 20
	ASSERT_GT(maxDistance, 0.0f);
	// beyond the sample's max distance: nothing (0x10014ABD)
	EXPECT_EQ(SamplePlayAnimEffect(Owner::Thing(k_Villager), maxDistance + 1.0f, key, AnimAction::Play, editor, true, 0.0f,
	                               0.0f),
	          k_NoChannel);
	EXPECT_EQ(output.plays, 0);
	// within: one 3D channel at the owner's point (fn_00427200), tracked
	const auto channel =
	    SamplePlayAnimEffect(Owner::Thing(k_Villager), 5.0f, key, AnimAction::Play, editor, true, 0.0f, 0.0f);
	ASSERT_NE(channel, k_NoChannel);
	EXPECT_EQ(output.plays, 1);
	EXPECT_TRUE(output.starts[0].is3D);
	EXPECT_EQ(output.starts[0].position, glm::vec3(3.0f, 0.0f, 4.0f));
	EXPECT_EQ(sample_play::Channels()[0].owner, Owner::Thing(k_Villager));
	EXPECT_TRUE(sample_play::Channels()[0].track);
	// a gone owner: fn_00427200 gives 0, nothing plays (0x10014B9D)
	EXPECT_EQ(SamplePlayAnimEffect(Owner::Thing(static_cast<entt::entity>(99)), 5.0f, key, AnimAction::Play, editor, false,
	                               0.0f, 0.0f),
	          k_NoChannel);
	// no owner: at the camera (0x4272F9)
	output = FakeOutput {};
	audio::ClearMap();
	EXPECT_NE(SamplePlayAnimEffect(Owner::None(), 5.0f, key, AnimAction::Play, editor, true, 0.0f, 0.0f), k_NoChannel);
	EXPECT_EQ(output.starts[0].position, glm::vec3(0.0f));
}

TEST_F(AnimEffectsTest, GlobalMaxDistanceGatesEveryAction)
{
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(1.0f);
	// the saw's back stroke {2, 2, 1, *, 31} (M_P_Saw_Wood 760:31) plays, then a stop farther than 800 does nothing
	const AnimKey saw = {1, 2, 1, 2, 31};
	ASSERT_FALSE(anim_effects::Tables(editor)->FindList(saw).empty());
	const auto channel = SamplePlayAnimEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Play, editor, true, 0.0f, 0.0f);
	ASSERT_NE(channel, k_NoChannel);
	SamplePlayAnimEffect(Owner::Thing(k_Villager), 801.0f, saw, AnimAction::Stop, editor, true, 0.0f, 0.0f);
	EXPECT_TRUE(output.playing[0]); // 0x1001475A: the stop is gated too
	// another owner's stop leaves it alone; the owner's own stop stops it (0x1001491C, LHSampleStop of the row's samples)
	SamplePlayAnimEffect(Owner::Thing(k_Abode), 1.0f, saw, AnimAction::Stop, editor, true, 0.0f, 0.0f);
	EXPECT_TRUE(output.playing[0]);
	SamplePlayAnimEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Stop, editor, true, 0.0f, 0.0f);
	EXPECT_FALSE(output.playing[0]);
	// any other action releases the loop (0x10014990)
	ASSERT_NE(SamplePlayAnimEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Play, editor, true, 0.0f, 0.0f),
	          k_NoChannel);
	SamplePlayAnimEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Release, editor, true, 0.0f, 0.0f);
	EXPECT_TRUE(output.released[0]);
}

TEST_F(AnimEffectsTest, BanterAtTheAbodeWithTheVillagersDistance)
{
	// 0x92 (M_P_Yawn 700:146): VillagersBanter.sad, the owner the abode, the distance the villager's (fn_00516510)
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(2.0f, 0.0f, 0.0f);
	s_Things[static_cast<uint32_t>(k_Abode)] = glm::vec3(30.0f, 0.0f, 0.0f);
	const AnimKey key = {1, 2, 1, 2, 0x92};
	ASSERT_FALSE(anim_effects::Tables(banter)->FindList(key).empty());
	const auto channel = SamplePlayAnimEffect(Owner::Thing(k_Abode), 2.0f, key, AnimAction::Play, banter, true, 0.0f, 0.0f);
	ASSERT_NE(channel, k_NoChannel);
	EXPECT_EQ(output.starts[0].position, glm::vec3(30.0f, 0.0f, 0.0f));
	const int sample = SampleOn(0);
	EXPECT_GE(sample, 1);
	EXPECT_LE(sample, 23); // HELP_TEXT_VILLAGER_BANTER_* (sounds_props.md B)
}

TEST_F(AnimEffectsTest, MinAndMaxOverrideOnlyWhenPositive)
{
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(1.0f);
	const AnimKey key = {1, 2, 1, 2, 4};
	SamplePlayAnimEffect(Owner::Thing(k_Villager), 1.0f, key, AnimAction::Play, editor, false, 0.0f, 0.0f);
	const auto withSad = output.starts[0];
	audio::ClearMap();
	output = FakeOutput {};
	// 0x10014ACC..0x10014B4F: min / max with their caller bits 0x80 / 0x100 when > 0
	SamplePlayAnimEffect(Owner::Thing(k_Villager), 1.0f, key, AnimAction::Play, editor, false, 2.5f, 12.0f);
	EXPECT_FLOAT_EQ(output.starts[0].minDistance, 2.5f);
	EXPECT_FLOAT_EQ(output.starts[0].maxDistance, 12.0f);
	EXPECT_FLOAT_EQ(withSad.scale, output.starts[0].scale);
}

TEST_F(AnimEffectsTest, MiraclesBankCopiesTheCoresTables)
{
	// SpellSounds' AnimEffectBank::Load(path) of a registered bank takes the tables the core read at LHBankRegister
	// (0x10002778) instead of reading the file again; the same rows, lists and samples as a read of the file
	const auto root = GameRoot();
	const auto spells = Load(*root, "Audio/Sfx/Game/spells.sad", "spells.sad");
	const auto* core = anim_effects::Tables(spells);
	ASSERT_NE(core, nullptr);
	AnimEffectBank copied {"spells.sad", {}, {}, {}};
	copied.Load(*root / "Audio" / "Sfx" / "Game" / "spells.sad");
	pack::PackFile pack;
	ASSERT_EQ(pack.Open(*root / "Audio/Sfx/Game/spells.sad"), pack::PackResult::Success);
	AnimEffectBank read {"spells.sad", {}, {}, {}};
	read.Load(pack);
	ASSERT_FALSE(read.rows.empty());
	EXPECT_EQ(copied.rows, read.rows);
	EXPECT_EQ(copied.waves, read.waves);
	EXPECT_EQ(copied.samples.size(), read.samples.size());
	// every row's own key finds the same list in both (the attribute columns with the wildcard kept)
	for (const auto& row : read.rows)
	{
		const std::array<int32_t, 5> key = {row[0], row[1], row[2], row[3], row[4]};
		EXPECT_EQ(core->FindList(key), read.FindList(key));
	}
	EXPECT_EQ(copied.SoundId(40), SampleId(spells, 40));
}
