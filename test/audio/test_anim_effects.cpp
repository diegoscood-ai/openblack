/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <PackFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Audio/Audio.h"
#include "Audio/AudioManagerNoOp.h"
#include "Audio/ClipSounds.h"
#include "Audio/Device/SampleOutput.h"
#include "Audio/Device/Sound.h"
#include "Audio/Engine/AnimEffectBank.h"
#include "Audio/Engine/AnimEffects.h"
#include "Audio/Game/Banks.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/AnimationSounds.h"
#include "ECS/Systems/TreeRustle.h"
#include "FileSystem/DefaultFileSystem.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/Resources.h"
#include "Resources/ResourcesInterface.h"
#include "support/RestoreService.h"

// The anim effects in the audio core: the tables of a bank read once as it is registered, the effect number drawn with
// the audio library's own random, and PlayAnimationEffect (the caller's distance against 800 and the sample's max
// distance, the owner's point, stop / release of the row's samples). Real editor.sad and
// VillagersBanter.sad (OPENBLACK_TEST_BW_ROOT; without it the tests skip). A fake output stands for QMixer.
// The AnimEffectsSyntheticTest tests run the same logic on small banks built in memory and need no game data, and the
// AnimationSoundsTest tests play real clips' sounds (AnimationSounds::Fire) from the game's own SmallSounds.SAS, which
// the AnimationSoundsSyntheticTest tests replace with a clip table and two banks built in memory.

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// The tables of a bank from its path, as the miracles' AnimEffectBank read them (only these tests do): a bank the game has
/// registered already has its tables in the core, which are copied, not read again; its last three parts name it
/// ("Sfx/Game/spells.sad"). A bank not registered is read from its file (nothing when it is missing or has none)
void LoadFromPath(AnimEffectTable& table, const std::filesystem::path& path)
{
	const auto tail = path.parent_path().parent_path().filename() / path.parent_path().filename() / path.filename();
	if (const auto* registered = anim_effects::Tables(FindBank(tail.generic_string())); registered != nullptr)
	{
		table.rows = registered->rows;
		table.waves = registered->waves;
		table.samples = registered->samples;
		return;
	}
	auto& fileSystem = Locator::filesystem::value();
	pack::PackFile file;
	if (file.ReadFile(*fileSystem.GetData(path)) != pack::PackResult::Success)
	{
		return;
	}
	table.Load(file);
}

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

/// The audio core over the fake output, shared by the installation fixture and the synthetic one
class AnimEffectsBase: public ::testing::Test
{
protected:
	FakeOutput output;
	std::map<entt::id_type, Sound> sounds;
	static inline std::map<uint32_t, glm::vec3> s_Things;
	static inline int s_Rand {16383};
	BankId editor {k_NoBank};
	BankId banter {k_NoBank};

	void StartAudio()
	{
		s_Things.clear();
		s_Rand = 16383;
		GameQueries queries;
		queries.thingPosition = [](ThingId thing) -> std::optional<glm::vec3> {
			const auto found = s_Things.find(thing);
			return found != s_Things.end() ? std::optional<glm::vec3>(found->second) : std::nullopt;
		};
		queries.camera = []() -> std::optional<CameraState> { return CameraState {}; };
		audio::Init(std::move(queries));
	}
	void StartPlayback()
	{
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
		sample_play::SetMainVolume(127);
		audio::ClearMap();
		output = FakeOutput {};
	}
	void StopAudio()
	{
		audio::ClearMap();
		sample_play::SetMainVolume(127); // back to its default (QMixer's maximum)
		sample_play::SetBackend({});
		audio::Shutdown();
	}

	/// Game's bank loop: RegisterBank, the anim effect tables, the samples' records
	BankId Load(const std::filesystem::path& root, const std::string& relative, const std::string& group)
	{
		pack::PackFile pack;
		EXPECT_EQ(pack.Open(root / relative), pack::PackResult::Success) << relative;
		return Register(pack, relative, group);
	}
	/// The same loop over a pack already read
	BankId Register(const pack::PackFile& pack, const std::string& relative, const std::string& group)
	{
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
	/// The same loop over a bank built in memory
	BankId RegisterBuffer(const std::vector<uint8_t>& bytes, const std::string& relative, const std::string& group)
	{
		pack::PackFile pack;
		EXPECT_EQ(pack.Open(bytes), pack::PackResult::Success) << relative;
		return Register(pack, relative, group);
	}
	int SampleOn(size_t channel) const
	{
		const auto infos = sample_play::Channels();
		return infos[channel].sample;
	}
};

/// The real editor.sad and VillagersBanter.sad
class AnimEffectsTest: public AnimEffectsBase
{
protected:
	void SetUp() override
	{
		const auto root = GameRoot();
		if (!root)
		{
			GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
		}
		StartAudio();
		editor = Load(*root, "Audio/Sfx/Game/editor.sad", "editor.sad");
		banter = Load(*root, "Audio/Dialogue/VillagersBanter.sad", "VillagersBanter.sad");
		StartPlayback();
	}
	void TearDown() override
	{
		if (!GameRoot())
		{
			return;
		}
		StopAudio();
	}
};

// ---- small sound banks built in memory ------------------------------------------------------------------------------

struct SyntheticSample
{
	int32_t id;
	float maxDistance;
};

void AppendBlock(std::vector<uint8_t>& out, const char* name, const std::vector<uint8_t>& data)
{
	std::array<char, 32> blockName {};
	std::strncpy(blockName.data(), name, blockName.size() - 1);
	out.insert(out.end(), blockName.begin(), blockName.end());
	const auto size = static_cast<uint32_t>(data.size());
	const auto* sizeBytes = reinterpret_cast<const uint8_t*>(&size);
	out.insert(out.end(), sizeBytes, sizeBytes + sizeof(size));
	out.insert(out.end(), data.begin(), data.end());
}

template <typename T>
void AppendValue(std::vector<uint8_t>& out, const T& value)
{
	const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
	out.insert(out.end(), bytes, bytes + sizeof(T));
}

/// A .sad in memory: the bank info, one 4-byte wave per sample, the sample table and the two anim effect tables
std::vector<uint8_t> MakeSoundBank(const std::vector<SyntheticSample>& samples, const std::vector<std::array<int32_t, 6>>& rows,
                                   const std::vector<int32_t>& waves)
{
	std::vector<uint8_t> file {'L', 'i', 'O', 'n', 'H', 'e', 'A', 'd'};
	AppendBlock(file, "LHFileSegmentBankInfo", std::vector<uint8_t>(12, 0));
	AppendBlock(file, "LHAudioWaveData", std::vector<uint8_t>(samples.size() * 4, 0));
	std::vector<uint8_t> table;
	AppendValue(table, static_cast<uint16_t>(samples.size()));
	AppendValue(table, static_cast<uint16_t>(0));
	for (size_t i = 0; i < samples.size(); ++i)
	{
		pack::AudioBankSampleHeader header;
		std::memset(&header, 0, sizeof(header)); // the padding too: the loader reads one word across it
		std::strncpy(header.name.data(), "synthetic.wav", header.name.size() - 1);
		header.id = samples[i].id;
		header.size = 4;
		header.offset = static_cast<uint32_t>(i * 4);
		header.sampleRate = 22050;
		header.maxDist = samples[i].maxDistance;
		AppendValue(table, header);
	}
	AppendBlock(file, "LHAudioBankSampleTable", table);
	std::vector<uint8_t> anim;
	AppendValue(anim, static_cast<int32_t>(rows.size()));
	AppendValue(anim, static_cast<int32_t>(6));
	for (const auto& row : rows)
	{
		AppendValue(anim, row);
	}
	AppendBlock(file, "LHAudioAnimArrayTable", anim);
	std::vector<uint8_t> lists;
	for (const auto value : waves)
	{
		AppendValue(lists, value);
	}
	AppendBlock(file, "LHAudioWaveNumTable", lists);
	return file;
}

constexpr int32_t k_Wild = AnimEffectTable::k_Wildcard;
constexpr float k_StepMaxDistance = 20.0f;

/// The effects bank: a footstep row with a list of ten samples (100..109), a saw row with one sample (300), and a row
/// with a wildcard column (400)
std::vector<uint8_t> MakeEffectsBank()
{
	std::vector<SyntheticSample> samples;
	for (int32_t id = 100; id < 110; ++id)
	{
		samples.push_back({id, k_StepMaxDistance});
	}
	samples.push_back({300, 50.0f});
	samples.push_back({400, k_StepMaxDistance});
	const std::vector<std::array<int32_t, 6>> rows = {
	    {1, 2, 1, 2, 4, 0},
	    {1, 2, 1, 2, 31, 11},
	    {k_Wild, 2, 1, 2, 50, 13},
	};
	const std::vector<int32_t> waves = {10, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 1, 300, 1, 400};
	return MakeSoundBank(samples, rows, waves);
}

/// The banter bank: one row with the samples 1..3
std::vector<uint8_t> MakeBanterBank()
{
	return MakeSoundBank({{1, 50.0f}, {2, 50.0f}, {3, 50.0f}}, {{1, 2, 1, 2, 0x92, 0}}, {3, 1, 2, 3});
}

/// The same paths and groups as the installation fixture, with the banks above
class AnimEffectsSyntheticTest: public AnimEffectsBase
{
protected:
	void SetUp() override
	{
		StartAudio();
		editor = RegisterBuffer(MakeEffectsBank(), "Audio/Sfx/Game/editor.sad", "editor.sad");
		banter = RegisterBuffer(MakeBanterBank(), "Audio/Dialogue/VillagersBanter.sad", "VillagersBanter.sad");
		StartPlayback();
	}
	void TearDown() override { StopAudio(); }
};
} // namespace

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
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

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimEffectsTest, TreeKeysAlignmentIsAWildcardInTheEditorBank)
{
	const auto* tables = anim_effects::Tables(editor);
	ASSERT_NE(tables, nullptr);
	// every row of editor.sad takes any value in column 1, so the trees' 0 there finds the rows any other value would
	for (const auto& row : tables->rows)
	{
		EXPECT_EQ(row[1], AnimEffectTable::k_Wildcard);
	}
	using openblack::ecs::systems::TreeRustle;
	auto bend = TreeRustle::BendKeys(1.0f).ToArray();
	auto idle = TreeRustle::IdleKeys().ToArray();
	const auto bendList = tables->FindList(bend);
	const auto idleList = tables->FindList(idle);
	EXPECT_FALSE(bendList.empty());
	EXPECT_FALSE(idleList.empty());
	bend[1] = 2;
	idle[1] = 2;
	EXPECT_EQ(tables->FindList(bend), bendList);
	EXPECT_EQ(tables->FindList(idle), idleList);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimEffectsTest, NumberUsesTheAudioRandom)
{
	// a man's footstep on gravel: {1, 2, 1, 2, 4} -> H_Footstep_Gravel 259..268
	const AnimKey key = {1, 2, 1, 2, 4};
	const auto list = anim_effects::Tables(editor)->FindList(key);
	ASSERT_EQ(list.size(), 10u);
	EXPECT_EQ(list.front(), 259);
	// Random(10) = draw * 10 / 32767, the draw = rand() / 2 plus 0x3FFF on every other call (the flag starts set):
	// the first draw is in the upper half
	s_Rand = 16383;
	EXPECT_EQ(anim_effects::Number(key, editor), list[7]); // (8191 + 16383) * 10 / 32767
	s_Rand = 0;
	EXPECT_EQ(anim_effects::Number(key, editor), list[0]); // 0 * 10 / 32767
	s_Rand = 32767;
	EXPECT_EQ(anim_effects::Number(key, editor), list[9]); // (16383 + 16383) * 10 / 32767
	s_Rand = 32767;
	EXPECT_EQ(anim_effects::Number(key, editor), list[4]);          // 16383 * 10 / 32767
	EXPECT_EQ(anim_effects::Number({1, 2, 1, 2, 9999}, editor), 0); // no row
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimEffectsTest, PlayGatesOnTheCallersDistance)
{
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(3.0f, 0.0f, 4.0f);
	const AnimKey key = {1, 2, 1, 2, 4};
	const float maxDistance = sounds.at(SampleId(editor, 263)).maxDistance; // the footsteps' 20
	ASSERT_GT(maxDistance, 0.0f);
	// beyond the sample's max distance: nothing
	EXPECT_EQ(
	    PlayAnimationEffect(Owner::Thing(k_Villager), maxDistance + 1.0f, key, AnimAction::Play, editor, true, 0.0f, 0.0f),
	    k_NoChannel);
	EXPECT_EQ(output.plays, 0);
	// within: one 3D channel at the owner's point, tracked
	const auto channel = PlayAnimationEffect(Owner::Thing(k_Villager), 5.0f, key, AnimAction::Play, editor, true, 0.0f, 0.0f);
	ASSERT_NE(channel, k_NoChannel);
	EXPECT_EQ(output.plays, 1);
	EXPECT_TRUE(output.starts[0].is3D);
	EXPECT_EQ(output.starts[0].position, glm::vec3(3.0f, 0.0f, 4.0f));
	EXPECT_EQ(sample_play::Channels()[0].owner, Owner::Thing(k_Villager));
	EXPECT_TRUE(sample_play::Channels()[0].track);
	// a gone owner has no position: nothing plays
	EXPECT_EQ(PlayAnimationEffect(Owner::Thing(static_cast<entt::entity>(99)), 5.0f, key, AnimAction::Play, editor, false, 0.0f,
	                              0.0f),
	          k_NoChannel);
	// no owner: at the camera
	output = FakeOutput {};
	audio::ClearMap();
	EXPECT_NE(PlayAnimationEffect(Owner::None(), 5.0f, key, AnimAction::Play, editor, true, 0.0f, 0.0f), k_NoChannel);
	EXPECT_EQ(output.starts[0].position, glm::vec3(0.0f));
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimEffectsTest, GlobalMaxDistanceGatesEveryAction)
{
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(1.0f);
	// the saw's back stroke {2, 2, 1, *, 31} (M_P_Saw_Wood 760:31) plays, then a stop farther than 800 does nothing
	const AnimKey saw = {1, 2, 1, 2, 31};
	ASSERT_FALSE(anim_effects::Tables(editor)->FindList(saw).empty());
	const auto channel = PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Play, editor, true, 0.0f, 0.0f);
	ASSERT_NE(channel, k_NoChannel);
	PlayAnimationEffect(Owner::Thing(k_Villager), 801.0f, saw, AnimAction::Stop, editor, true, 0.0f, 0.0f);
	EXPECT_TRUE(output.playing[0]); // the stop is gated too
	// another owner's stop leaves it alone; the owner's own stop stops it (the row's samples are stopped)
	PlayAnimationEffect(Owner::Thing(k_Abode), 1.0f, saw, AnimAction::Stop, editor, true, 0.0f, 0.0f);
	EXPECT_TRUE(output.playing[0]);
	PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Stop, editor, true, 0.0f, 0.0f);
	EXPECT_FALSE(output.playing[0]);
	// any other action releases the loop
	ASSERT_NE(PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Play, editor, true, 0.0f, 0.0f),
	          k_NoChannel);
	PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Release, editor, true, 0.0f, 0.0f);
	EXPECT_TRUE(output.released[0]);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimEffectsTest, BanterAtTheAbodeWithTheVillagersDistance)
{
	// 0x92 (M_P_Yawn 700:146): VillagersBanter.sad, the owner the abode, the distance the villager's
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(2.0f, 0.0f, 0.0f);
	s_Things[static_cast<uint32_t>(k_Abode)] = glm::vec3(30.0f, 0.0f, 0.0f);
	const AnimKey key = {1, 2, 1, 2, 0x92};
	ASSERT_FALSE(anim_effects::Tables(banter)->FindList(key).empty());
	const auto channel = PlayAnimationEffect(Owner::Thing(k_Abode), 2.0f, key, AnimAction::Play, banter, true, 0.0f, 0.0f);
	ASSERT_NE(channel, k_NoChannel);
	EXPECT_EQ(output.starts[0].position, glm::vec3(30.0f, 0.0f, 0.0f));
	const int sample = SampleOn(0);
	EXPECT_GE(sample, 1);
	EXPECT_LE(sample, 23); // HELP_TEXT_VILLAGER_BANTER_*
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimEffectsTest, MinAndMaxOverrideOnlyWhenPositive)
{
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(1.0f);
	const AnimKey key = {1, 2, 1, 2, 4};
	PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, key, AnimAction::Play, editor, false, 0.0f, 0.0f);
	const auto withSad = output.starts[0];
	audio::ClearMap();
	output = FakeOutput {};
	// min / max with their caller bits 0x80 / 0x100 when > 0
	PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, key, AnimAction::Play, editor, false, 2.5f, 12.0f);
	EXPECT_FLOAT_EQ(output.starts[0].minDistance, 2.5f);
	EXPECT_FLOAT_EQ(output.starts[0].maxDistance, 12.0f);
	EXPECT_FLOAT_EQ(withSad.scale, output.starts[0].scale);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimEffectsTest, MiraclesBankCopiesTheCoresTables)
{
	// SpellSounds' AnimEffectBank::Load(path) (LoadFromPath here) of a registered bank takes the tables the core read at
	// registration instead of reading the file again; the same rows, lists and samples as a read of the file
	const auto root = GameRoot();
	const auto spells = Load(*root, "Audio/Sfx/Game/spells.sad", "spells.sad");
	const auto* core = anim_effects::Tables(spells);
	ASSERT_NE(core, nullptr);
	AnimEffectBank copied {"spells.sad", {}, {}, {}};
	LoadFromPath(copied, *root / "Audio" / "Sfx" / "Game" / "spells.sad");
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

// ---- the same logic on the banks built in memory (no game data) ------------------------------------------------------

TEST_F(AnimEffectsSyntheticTest, TablesReadOnceByBankSynthetic)
{
	const auto* tables = anim_effects::Tables(editor);
	ASSERT_NE(tables, nullptr);
	ASSERT_EQ(tables->rows.size(), 3u); // the three rows of MakeEffectsBank
	EXPECT_EQ(tables->rows[0], (std::array<int32_t, 6> {1, 2, 1, 2, 4, 0}));
	EXPECT_EQ(tables->waves.size(), 15u);
	ASSERT_NE(anim_effects::Tables(banter), nullptr);
	EXPECT_EQ(anim_effects::Tables(banter)->rows.size(), 1u);
	EXPECT_EQ(tables->name, "editor.sad");
	// the miracles' alias is the same type
	const AnimEffectBank* alias = tables;
	EXPECT_EQ(alias->FindList({1, 2, 1, 2, 4}), tables->FindList({1, 2, 1, 2, 4}));
	// a second registration of the same bank reads nothing more
	const auto* before = anim_effects::Tables(editor);
	pack::PackFile empty;
	anim_effects::RegisterTables(editor, empty);
	EXPECT_EQ(anim_effects::Tables(editor), before);
	// a bank whose pack has no anim effect tables gets none
	const auto plain = RegisterBank("Audio/Sfx/Game/plain.sad", "plain.sad");
	anim_effects::RegisterTables(plain, empty);
	EXPECT_EQ(anim_effects::Tables(plain), nullptr);
}

TEST_F(AnimEffectsSyntheticTest, TreeKeysAlignmentIsAWildcardInTheEditorBankSynthetic)
{
	// a bank of the trees' two rows, each taking any value in column 1: the bend's list 500, 501 and the rustle's 502
	const std::vector<std::array<int32_t, 6>> rows = {
	    {1, k_Wild, 0, 10, 75, 0},
	    {0, k_Wild, 20, 0, 70, 3},
	};
	const auto trees = RegisterBuffer(MakeSoundBank({{500, 50.0f}, {501, 50.0f}, {502, 50.0f}}, rows, {2, 500, 501, 1, 502}),
	                                  "Audio/Sfx/Game/trees.sad", "trees.sad");
	const auto* tables = anim_effects::Tables(trees);
	ASSERT_NE(tables, nullptr);
	for (const auto& row : tables->rows)
	{
		EXPECT_EQ(row[1], AnimEffectTable::k_Wildcard);
	}
	// the trees' 0 in column 1 finds the rows any other value would
	using openblack::ecs::systems::TreeRustle;
	auto bend = TreeRustle::BendKeys(1.0f).ToArray();
	auto idle = TreeRustle::IdleKeys().ToArray();
	const auto bendList = tables->FindList(bend);
	const auto idleList = tables->FindList(idle);
	EXPECT_EQ(bendList, (std::vector<int32_t> {500, 501}));
	EXPECT_EQ(idleList, (std::vector<int32_t> {502}));
	bend[1] = 2;
	idle[1] = 2;
	EXPECT_EQ(tables->FindList(bend), bendList);
	EXPECT_EQ(tables->FindList(idle), idleList);
}

TEST_F(AnimEffectsSyntheticTest, NumberUsesTheAudioRandomSynthetic)
{
	const AnimKey key = {1, 2, 1, 2, 4};
	const auto list = anim_effects::Tables(editor)->FindList(key);
	ASSERT_EQ(list.size(), 10u);
	EXPECT_EQ(list.front(), 100);
	EXPECT_EQ(list.back(), 109);
	// Random(10) = draw * 10 / 32767, the draw = rand / 2 plus 16383 on every other call, the first call included
	s_Rand = 16383;
	EXPECT_EQ(anim_effects::Number(key, editor), list[7]); // (8191 + 16383) * 10 / 32767
	s_Rand = 0;
	EXPECT_EQ(anim_effects::Number(key, editor), list[0]); // 0 * 10 / 32767
	s_Rand = 32767;
	EXPECT_EQ(anim_effects::Number(key, editor), list[9]); // (16383 + 16383) * 10 / 32767
	s_Rand = 32767;
	EXPECT_EQ(anim_effects::Number(key, editor), list[4]);          // 16383 * 10 / 32767
	EXPECT_EQ(anim_effects::Number({1, 2, 1, 2, 9999}, editor), 0); // no row
	// a list of one sample gives that sample
	EXPECT_EQ(anim_effects::Number({1, 2, 1, 2, 31}, editor), 300);
	// a wildcard column matches any value of the key
	EXPECT_EQ(anim_effects::Number({77, 2, 1, 2, 50}, editor), 400);
}

TEST_F(AnimEffectsSyntheticTest, PlayGatesOnTheCallersDistanceSynthetic)
{
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(3.0f, 0.0f, 4.0f);
	const AnimKey key = {1, 2, 1, 2, 4};
	// every sample of the footstep list has the same max distance, so the draw does not matter
	const float maxDistance = sounds.at(SampleId(editor, 103)).maxDistance;
	ASSERT_FLOAT_EQ(maxDistance, k_StepMaxDistance);
	// beyond the sample's max distance: nothing
	EXPECT_EQ(
	    PlayAnimationEffect(Owner::Thing(k_Villager), maxDistance + 1.0f, key, AnimAction::Play, editor, true, 0.0f, 0.0f),
	    k_NoChannel);
	EXPECT_EQ(output.plays, 0);
	// within: one 3D channel at the owner's point, tracked
	const auto channel = PlayAnimationEffect(Owner::Thing(k_Villager), 5.0f, key, AnimAction::Play, editor, true, 0.0f, 0.0f);
	ASSERT_NE(channel, k_NoChannel);
	EXPECT_EQ(output.plays, 1);
	EXPECT_TRUE(output.starts[0].is3D);
	EXPECT_EQ(output.starts[0].position, glm::vec3(3.0f, 0.0f, 4.0f));
	EXPECT_EQ(sample_play::Channels()[0].owner, Owner::Thing(k_Villager));
	EXPECT_TRUE(sample_play::Channels()[0].track);
	// an owner that is gone has no position: nothing plays
	EXPECT_EQ(PlayAnimationEffect(Owner::Thing(static_cast<entt::entity>(99)), 5.0f, key, AnimAction::Play, editor, false, 0.0f,
	                              0.0f),
	          k_NoChannel);
	// no owner: at the camera
	output = FakeOutput {};
	audio::ClearMap();
	EXPECT_NE(PlayAnimationEffect(Owner::None(), 5.0f, key, AnimAction::Play, editor, true, 0.0f, 0.0f), k_NoChannel);
	EXPECT_EQ(output.starts[0].position, glm::vec3(0.0f));
}

TEST_F(AnimEffectsSyntheticTest, GlobalMaxDistanceGatesEveryActionSynthetic)
{
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(1.0f);
	// the saw row has one sample (300, max distance 50)
	const AnimKey saw = {1, 2, 1, 2, 31};
	ASSERT_EQ(anim_effects::Tables(editor)->FindList(saw), (std::vector<int32_t> {300}));
	const auto channel = PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Play, editor, true, 0.0f, 0.0f);
	ASSERT_NE(channel, k_NoChannel);
	// a stop farther than the global maximum does nothing
	PlayAnimationEffect(Owner::Thing(k_Villager), anim_effects::k_MaxDistance + 1.0f, saw, AnimAction::Stop, editor, true, 0.0f,
	                    0.0f);
	EXPECT_TRUE(output.playing[0]);
	// another owner's stop leaves it alone; the owner's own stop stops it
	PlayAnimationEffect(Owner::Thing(k_Abode), 1.0f, saw, AnimAction::Stop, editor, true, 0.0f, 0.0f);
	EXPECT_TRUE(output.playing[0]);
	PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Stop, editor, true, 0.0f, 0.0f);
	EXPECT_FALSE(output.playing[0]);
	// any other action releases the loop
	ASSERT_NE(PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Play, editor, true, 0.0f, 0.0f),
	          k_NoChannel);
	PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, saw, AnimAction::Release, editor, true, 0.0f, 0.0f);
	EXPECT_TRUE(output.released[0]);
}

TEST_F(AnimEffectsSyntheticTest, BanterAtTheAbodeWithTheVillagersDistanceSynthetic)
{
	// the banter bank, the owner the abode, the distance the villager's
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(2.0f, 0.0f, 0.0f);
	s_Things[static_cast<uint32_t>(k_Abode)] = glm::vec3(30.0f, 0.0f, 0.0f);
	const AnimKey key = {1, 2, 1, 2, 0x92};
	ASSERT_EQ(anim_effects::Tables(banter)->FindList(key), (std::vector<int32_t> {1, 2, 3}));
	const auto channel = PlayAnimationEffect(Owner::Thing(k_Abode), 2.0f, key, AnimAction::Play, banter, true, 0.0f, 0.0f);
	ASSERT_NE(channel, k_NoChannel);
	EXPECT_EQ(output.starts[0].position, glm::vec3(30.0f, 0.0f, 0.0f));
	const int sample = SampleOn(0);
	EXPECT_GE(sample, 1);
	EXPECT_LE(sample, 3); // the row's list
}

TEST_F(AnimEffectsSyntheticTest, MinAndMaxOverrideOnlyWhenPositiveSynthetic)
{
	s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(1.0f);
	const AnimKey key = {1, 2, 1, 2, 4};
	PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, key, AnimAction::Play, editor, false, 0.0f, 0.0f);
	const auto withSad = output.starts[0];
	audio::ClearMap();
	output = FakeOutput {};
	// a positive min / max of the caller replaces the sample's
	PlayAnimationEffect(Owner::Thing(k_Villager), 1.0f, key, AnimAction::Play, editor, false, 2.5f, 12.0f);
	EXPECT_FLOAT_EQ(output.starts[0].minDistance, 2.5f);
	EXPECT_FLOAT_EQ(output.starts[0].maxDistance, 12.0f);
	EXPECT_FLOAT_EQ(withSad.scale, output.starts[0].scale);
}

TEST_F(AnimEffectsSyntheticTest, MiraclesBankCopiesTheCoresTablesSynthetic)
{
	// a registered bank's tables are copied from the core, not read from its file
	const auto bytes = MakeEffectsBank();
	const auto spells = RegisterBuffer(bytes, "Audio/Sfx/Game/spells.sad", "spells.sad");
	const auto* core = anim_effects::Tables(spells);
	ASSERT_NE(core, nullptr);
	AnimEffectBank copied {"spells.sad", {}, {}, {}};
	LoadFromPath(copied, std::filesystem::path("Audio") / "Sfx" / "Game" / "spells.sad");
	pack::PackFile pack;
	ASSERT_EQ(pack.Open(bytes), pack::PackResult::Success);
	AnimEffectBank read {"spells.sad", {}, {}, {}};
	read.Load(pack);
	ASSERT_EQ(read.rows.size(), 3u);
	EXPECT_EQ(copied.rows, read.rows);
	EXPECT_EQ(copied.waves, read.waves);
	EXPECT_EQ(copied.samples.size(), read.samples.size());
	EXPECT_EQ(read.samples.size(), 12u); // 100..109, 300 and 400
	// every row's own key finds the same list in both (the wildcard column kept)
	for (const auto& row : read.rows)
	{
		const std::array<int32_t, 5> key = {row[0], row[1], row[2], row[3], row[4]};
		EXPECT_EQ(core->FindList(key), read.FindList(key));
	}
	EXPECT_EQ(copied.SoundId(40), SampleId(spells, 40));
}

namespace
{
/// A recording audio service: the arguments of every animation effect asked for
class AnimEffectRequests final: public AudioManagerNoOp
{
public:
	struct Request
	{
		Owner owner;
		float distance;
		AnimKey key;
		AnimAction action;
		BankId bank;
		bool track;
		float minDistance;
		float maxDistance;
	};
	std::vector<Request> requests;

	Channel PlayAnimationEffect(Owner owner, float distance, const AnimKey& key, AnimAction action, BankId bank, bool track,
	                            float minDistance, float maxDistance) override
	{
		requests.push_back({owner, distance, key, action, bank, track, minDistance, maxDistance});
		return k_NoChannel;
	}
};
} // namespace

// A tree's bend and its rustle are played with the tree as their owner, from the editor bank at the camera's distance
// to the tree: the bend stays where it started, the rustle follows the tree
TEST_F(AnimEffectsSyntheticTest, ATreesSoundsAreOwnedByTheTree)
{
	using openblack::ecs::systems::TreeRustle;
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& heard = static_cast<AnimEffectRequests&>(Locator::audio::emplace<AnimEffectRequests>());
	constexpr auto k_Tree = static_cast<entt::entity>(11);
	// 5 from the camera, which stands at the origin
	const glm::vec3 tree(3.0f, 0.0f, 4.0f);
	AnimationSounds::PlayFromTable(k_Tree, tree, TreeRustle::BendKeys(1.0f).ToArray());
	AnimationSounds::PlayFromTable(k_Tree, tree, TreeRustle::IdleKeys().ToArray());
	ASSERT_EQ(heard.requests.size(), 2u);
	for (const auto& request : heard.requests)
	{
		EXPECT_EQ(request.owner, Owner::Thing(k_Tree));
		EXPECT_FLOAT_EQ(request.distance, 5.0f);
		EXPECT_EQ(request.action, AnimAction::Play);
		EXPECT_EQ(request.bank, Bank(SfxBank::Editor));
		EXPECT_EQ(request.minDistance, 0.0f);
		EXPECT_EQ(request.maxDistance, 0.0f);
	}
	EXPECT_EQ(heard.requests[0].key, (AnimKey {1, 0, 0, 10, 75}));
	EXPECT_FALSE(heard.requests[0].track);
	EXPECT_EQ(heard.requests[1].key, (AnimKey {0, 0, 20, 0, 70}));
	EXPECT_TRUE(heard.requests[1].track);
}

// The samples' bytes move out of the pack into their sounds unchanged (synthetic bank, no game data)
TEST(SoundBankBytes, MovedIntoTheSoundUnchangedSynthetic)
{
	const std::vector<uint8_t> waves = {1, 2, 3, 4, 5, 6, 7, 8, 9};
	std::vector<uint8_t> file {'L', 'i', 'O', 'n', 'H', 'e', 'A', 'd'};
	AppendBlock(file, "LHFileSegmentBankInfo", std::vector<uint8_t>(12, 0));
	AppendBlock(file, "LHAudioWaveData", waves);
	std::vector<uint8_t> table;
	AppendValue(table, static_cast<uint16_t>(2));
	AppendValue(table, static_cast<uint16_t>(0));
	for (const auto& [offset, size] : {std::pair<uint32_t, uint32_t> {0, 4}, std::pair<uint32_t, uint32_t> {4, 5}})
	{
		pack::AudioBankSampleHeader header;
		std::memset(&header, 0, sizeof(header));
		std::strncpy(header.name.data(), "synthetic.wav", header.name.size() - 1);
		header.offset = offset;
		header.size = size;
		header.sampleRate = 22050;
		AppendValue(table, header);
	}
	AppendBlock(file, "LHAudioBankSampleTable", table);
	pack::PackFile pack;
	ASSERT_EQ(pack.Open(file), pack::PackResult::Success);
	const auto copy = pack.GetAudioSamplesData();
	ASSERT_EQ(copy.size(), 2u);
	EXPECT_EQ(copy[0], (std::vector<uint8_t> {1, 2, 3, 4}));
	EXPECT_EQ(copy[1], (std::vector<uint8_t> {5, 6, 7, 8, 9}));
	auto taken = pack.TakeAudioSamplesData();
	EXPECT_EQ(taken, copy);
	EXPECT_TRUE(pack.GetAudioSamplesData().empty());
	for (size_t i = 0; i < taken.size(); ++i)
	{
		std::vector<std::vector<uint8_t>> buffer;
		buffer.emplace_back(std::move(taken[i]));
		const auto sound = resources::SoundLoader {}(resources::SoundLoader::FromBufferTag {},
		                                             pack.GetAudioSampleHeader(static_cast<uint32_t>(i)), std::move(buffer));
		ASSERT_EQ(sound->buffer.size(), 1u);
		EXPECT_EQ(sound->buffer[0], copy[i]) << i;
	}
}

TEST(TreeRustle, BendPicksTheSizeOfTheCollision)
{
	using openblack::ecs::systems::TreeRustle;
	EXPECT_EQ(TreeRustle::BendKeys(0.29f).size, SoundSize::Small);
	EXPECT_EQ(TreeRustle::BendKeys(0.3f).size, SoundSize::Medium);
	EXPECT_EQ(TreeRustle::BendKeys(0.66f).size, SoundSize::Medium);
	EXPECT_EQ(TreeRustle::BendKeys(0.67f).size, SoundSize::Large);
	EXPECT_EQ(TreeRustle::BendKeys(1.0f).ToArray(), (std::array<int32_t, 5> {1, 0, 0, 10, 75}));
	EXPECT_EQ(TreeRustle::IdleKeys().ToArray(), (std::array<int32_t, 5> {0, 0, 20, 0, 70}));
}

TEST(TreeRustle, TallTreesRustleAroundTheCamera)
{
	using openblack::ecs::systems::TreeRustle;
	const glm::vec3 tree {100.0f, 20.0f, 200.0f};
	EXPECT_TRUE(TreeRustle::IsAmongTree(tree, 12.0f, {110.0f, 37.0f, 190.0f}));
	// Too far along x, z or above
	EXPECT_FALSE(TreeRustle::IsAmongTree(tree, 12.0f, {110.5f, 25.0f, 200.0f}));
	EXPECT_FALSE(TreeRustle::IsAmongTree(tree, 12.0f, {100.0f, 25.0f, 189.5f}));
	EXPECT_FALSE(TreeRustle::IsAmongTree(tree, 12.0f, {100.0f, 38.0f, 200.0f}));
	EXPECT_FALSE(TreeRustle::IsAmongTree(tree, 12.0f, {100.0f, 2.0f, 200.0f}));
	// Too short
	EXPECT_FALSE(TreeRustle::IsAmongTree(tree, 10.0f, {100.0f, 25.0f, 200.0f}));
}

TEST(TreeRustle, NearTheCameraIsAcrossTheGroundWhateverTheHeight)
{
	using openblack::ecs::systems::TreeRustle;
	const glm::vec3 tree {100.0f, 20.0f, 200.0f};
	EXPECT_TRUE(TreeRustle::IsNearCamera(tree, {110.0f, 500.0f, 190.0f}));
	EXPECT_FALSE(TreeRustle::IsNearCamera(tree, {110.5f, 20.0f, 200.0f}));
	EXPECT_FALSE(TreeRustle::IsNearCamera(tree, {100.0f, 20.0f, 189.5f}));
	// Every tree the camera is among is near it
	EXPECT_TRUE(TreeRustle::IsNearCamera(tree, {110.0f, 37.0f, 190.0f}));
}

TEST(TreeRustle, AboutOnceASecondOfGameTime)
{
	using openblack::ecs::systems::TreeRustle;
	using Milliseconds = std::chrono::duration<float, std::milli>;
	EXPECT_EQ(TreeRustle::IdleChance(Milliseconds(33.0f)), 30);
	EXPECT_EQ(TreeRustle::IdleChance(Milliseconds(16.0f)), 62);
	EXPECT_EQ(TreeRustle::IdleChance(Milliseconds(1000.0f)), 1);
	// While paused the roll is over 2^31, so it all but never comes up 1
	EXPECT_EQ(TreeRustle::IdleChance(Milliseconds(0.0f)), std::numeric_limits<int32_t>::min());
}

TEST(ClipEventVoice, VoicesOfTheVillagersGroup)
{
	using Thing = AnimationSounds::EventThing;
	// group 1: a man 1, a woman 2, a child 3, anything that is not a villager 3
	EXPECT_EQ(AnimationSounds::EventVoice(1, Thing {.villager = true}), 1);
	EXPECT_EQ(AnimationSounds::EventVoice(1, Thing {.villager = true, .female = true}), 2);
	EXPECT_EQ(AnimationSounds::EventVoice(1, Thing {.villager = true, .child = true, .female = true}), 3);
	EXPECT_EQ(AnimationSounds::EventVoice(1, Thing {}), 3);
	// any other group: 2, whatever the thing
	EXPECT_EQ(AnimationSounds::EventVoice(0, Thing {.villager = true}), 2);
	EXPECT_EQ(AnimationSounds::EventVoice(18, Thing {.alive = false}), 2);
}

TEST(ClipEventVoice, OnlyALivingThingSpeaks)
{
	using Thing = AnimationSounds::EventThing;
	// the life test is made on any thing in group 1: a dead villager and a dead animal both drop the clip's events
	EXPECT_EQ(AnimationSounds::EventVoice(1, Thing {.alive = false, .villager = true}), std::nullopt);
	EXPECT_EQ(AnimationSounds::EventVoice(1, Thing {.alive = false}), std::nullopt);
}

// ---- a clip's sounds (AnimationSounds::Fire) from the real SmallSounds.SAS, editor.sad and VillagersBanter.sad -------

namespace
{
// The clips by their index in Data/AllAnims.anm, the order the game numbers them in, with their sounds in
// Data/SmallSounds.SAS (in the file's order)
constexpr int32_t k_CowWalkClip = 45;      // M_A_Cow_Walk (the cows' group): a cow bell at 1332, 937, 485 and 0 ms
constexpr int32_t k_OnFireDieClip = 317;   // M_P_On_Fire_Die: a scream at 219 ms
constexpr int32_t k_OnFireRunClip = 319;   // M_P_On_Fire_Run: a scream at 0 ms, footsteps at 598 and 204 ms
constexpr int32_t k_ThrownClip = 399;      // M_P_Thrown: a scream at 10 ms
constexpr int32_t k_DespairWalkClip = 420; // M_P_Walk_Despair_Away_1: a cry at 43 ms, footsteps at 190 and 822 ms
constexpr int32_t k_YawnClip = 437;        // M_P_Yawn: the home's banter at 700 ms
constexpr auto k_Animal = static_cast<entt::entity>(9);
/// The surface under every thing: its footsteps are the gravel ones
constexpr int32_t k_Gravel = 2;

/// A man 5 from the camera, his home 30 from it, the ground gravel everywhere. The file system and the resources from
/// before come back when it goes
class AnimationSoundsBase: public AnimEffectsBase
{
protected:
	static inline std::map<entt::entity, AnimatedThing> s_Animated;

	struct Heard
	{
		int sample;
		Owner owner;
		glm::vec3 position;
	};

	/// The audio started with no bank, then a file system (on the game's folder when there is one) and the resources
	void StartClipAudio(const std::optional<std::filesystem::path>& root)
	{
		if (spdlog::get("audio") == nullptr)
		{
			_logger = spdlog::create<spdlog::sinks::null_sink_mt>("audio");
		}
		// No file system while the audio starts, so it registers no bank of its own: the two banks are registered once,
		// after it, and their samples are the fake output's sounds
		Locator::filesystem::reset();
		Locator::resources::reset();
		s_Things.clear();
		s_Animated.clear();
		s_Rand = 16383;
		GameQueries queries;
		queries.thingPosition = [](ThingId thing) -> std::optional<glm::vec3> {
			const auto found = s_Things.find(thing);
			return found != s_Things.end() ? std::optional<glm::vec3>(found->second) : std::nullopt;
		};
		queries.camera = []() -> std::optional<CameraState> { return CameraState {}; };
		queries.surfaceType = [](glm::vec3 /*point*/) { return k_Gravel; };
		queries.animatedThing = [](entt::entity thing) -> std::optional<AnimatedThing> {
			const auto found = s_Animated.find(thing);
			return found != s_Animated.end() ? std::optional<AnimatedThing>(found->second) : std::nullopt;
		};
		queries.animationClipName = [](int32_t index) -> std::optional<std::string> {
			static const std::map<int32_t, std::string> k_Names = {
			    {k_CowWalkClip, "M_A_Cow_Walk"},
			    {k_OnFireDieClip, "M_P_On_Fire_Die"},
			    {k_OnFireRunClip, "M_P_On_Fire_Run"},
			    {k_ThrownClip, "M_P_Thrown"},
			    {k_DespairWalkClip, "M_P_Walk_Despair_Away_1"},
			    {k_YawnClip, "M_P_Yawn"},
			};
			const auto found = k_Names.find(index);
			return found != k_Names.end() ? std::optional<std::string>(found->second) : std::nullopt;
		};
		audio::Init(std::move(queries));
		// The clips' sounds are read from the game's own file at the first Fire, unless the resources have them already
		Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
		if (root)
		{
			Locator::filesystem::value().SetGamePath(*root);
		}
		Locator::resources::emplace<resources::Resources>();
	}
	/// The man and his home
	static void PlaceTheMan()
	{
		s_Things[static_cast<uint32_t>(k_Villager)] = glm::vec3(3.0f, 0.0f, 4.0f);
		s_Things[static_cast<uint32_t>(k_Abode)] = glm::vec3(30.0f, 0.0f, 0.0f);
		AnimatedThing man;
		man.position = glm::vec3(3.0f, 0.0f, 4.0f);
		man.villager = AnimatedThing::Villager {.child = false, .female = false, .abode = k_Abode};
		s_Animated[k_Villager] = man;
	}
	void StopClipAudio()
	{
		StopAudio();
		if (_logger)
		{
			spdlog::drop("audio");
		}
	}

	static AnimatedThing::Villager& Person() { return *s_Animated.at(k_Villager).villager; }

	/// An animal, 5 from the camera
	static void AddAnimal()
	{
		s_Things[static_cast<uint32_t>(k_Animal)] = glm::vec3(0.0f, 0.0f, 5.0f);
		AnimatedThing animal;
		animal.position = glm::vec3(0.0f, 0.0f, 5.0f);
		s_Animated[k_Animal] = animal;
	}

	/// The samples playing, in channel order
	[[nodiscard]] std::vector<Heard> Playing() const
	{
		std::vector<Heard> heard;
		const auto infos = sample_play::Channels();
		for (size_t i = 0; i < output.playing.size(); ++i)
		{
			if (output.playing.at(i))
			{
				heard.push_back({infos.at(i).sample, infos.at(i).owner, output.starts.at(i).position});
			}
		}
		return heard;
	}
	/// The playing sample of that number, nullopt for none
	[[nodiscard]] std::optional<Heard> Find(int sample) const
	{
		const auto heard = Playing();
		const auto found = std::ranges::find(heard, sample, &Heard::sample);
		return found != heard.end() ? std::optional<Heard>(*found) : std::nullopt;
	}

private:
	std::shared_ptr<spdlog::logger> _logger;
	// made before SetUp, so they keep the services that were there before
	const test::RestoreService<Locator::filesystem> _restoreFilesystem;
	const test::RestoreService<Locator::resources> _restoreResources;
};

/// The game's own clips' sounds, read through the resources from its Data folder, and its own banks
class AnimationSoundsTest: public AnimationSoundsBase
{
protected:
	void SetUp() override
	{
		const auto root = GameRoot();
		if (!root)
		{
			GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
		}
		StartClipAudio(root);
		editor = Load(*root, "Audio/Sfx/Game/editor.sad", "editor.sad");
		banter = Load(*root, "Audio/Dialogue/VillagersBanter.sad", "VillagersBanter.sad");
		StartPlayback();
		PlaceTheMan();
	}
	void TearDown() override
	{
		if (!GameRoot())
		{
			return;
		}
		StopClipAudio();
	}
};

// ---- the clips' sounds and the two banks built in memory -------------------------------------------------------------

/// A run of consecutive sample numbers, one row's list
struct SampleRun
{
	int32_t first;
	int32_t count;
};

/// The sounds the synthetic clips place (the footstep's and the house banter's numbers are the ones the code tests for)
constexpr int32_t k_ScreamSound = 20;
constexpr int32_t k_CrySound = 21;
constexpr int32_t k_BellSound = 22;
constexpr int32_t k_FootstepSound = 4;
constexpr int32_t k_BanterSound = 0x92;
/// The people's sound group is 1, the villagers' voices group; the synthetic cows have another
constexpr int32_t k_PeopleGroup = 1;
constexpr int32_t k_CowGroup = 5;

/// The lists of the synthetic banks
constexpr SampleRun k_ManScreams {.first = 10, .count = 8};
constexpr SampleRun k_WomanScreams {.first = 30, .count = 6};
constexpr SampleRun k_ChildScreams {.first = 40, .count = 6};
constexpr SampleRun k_Footsteps {.first = 50, .count = 10};
constexpr SampleRun k_Cries {.first = 60, .count = 5};
constexpr SampleRun k_CowBells {.first = 70, .count = 4};
constexpr SampleRun k_Banter {.first = 1, .count = 6};
/// The banter's samples are heard up to 8 from the camera, the others up to 50
constexpr float k_BanterMaxDistance = 8.0f;
constexpr float k_EffectMaxDistance = 50.0f;

/// A row of a synthetic bank: its key and the run of samples of its list
struct ClipRow
{
	std::array<int32_t, 5> key;
	SampleRun run;
};

/// A bank of those rows, every sample heard up to the same distance
std::vector<uint8_t> MakeClipBank(const std::vector<ClipRow>& rows, float maxDistance)
{
	std::vector<SyntheticSample> samples;
	std::vector<std::array<int32_t, 6>> table;
	std::vector<int32_t> waves;
	for (const auto& [key, run] : rows)
	{
		table.push_back({key[0], key[1], key[2], key[3], key[4], static_cast<int32_t>(waves.size())});
		waves.push_back(run.count);
		for (int32_t id = run.first; id < run.first + run.count; ++id)
		{
			waves.push_back(id);
			samples.push_back({id, maxDistance});
		}
	}
	return MakeSoundBank(samples, table, waves);
}

/// The effects bank: the screams by voice, the man's footsteps on gravel, his cries and the cow bells
std::vector<uint8_t> MakeClipEffectsBank()
{
	return MakeClipBank(
	    {
	        {{1, 2, k_PeopleGroup, k_Wild, k_ScreamSound}, k_ManScreams},
	        {{2, 2, k_PeopleGroup, k_Wild, k_ScreamSound}, k_WomanScreams},
	        {{3, 2, k_PeopleGroup, k_Wild, k_ScreamSound}, k_ChildScreams},
	        {{1, 2, k_PeopleGroup, k_Gravel, k_FootstepSound}, k_Footsteps},
	        {{1, 2, k_PeopleGroup, k_Wild, k_CrySound}, k_Cries},
	        {{2, 2, k_CowGroup, k_Wild, k_BellSound}, k_CowBells},
	    },
	    k_EffectMaxDistance);
}

/// The banter bank: the man's banter at his house
std::vector<uint8_t> MakeClipBanterBank()
{
	return MakeClipBank({{{1, 2, k_PeopleGroup, k_Wild, k_BanterSound}, k_Banter}}, k_BanterMaxDistance);
}

/// The clips' sounds in the file's text form, under the names the fixture gives the clips, with the same events at the
/// same times as the installation's clips: "name group", then "time sound action more" until more is 0
std::vector<uint8_t> MakeClipSounds()
{
	const auto text =
	    fmt::format("1\n"
	                "M_A_Cow_Walk {0}\n1332 {2} 0 1\n937 {2} 0 1\n485 {2} 0 1\n0 {2} 0 0\n"
	                "M_P_On_Fire_Die {1}\n219 {3} 0 0\n"
	                "M_P_On_Fire_Run {1}\n0 {3} 0 1\n598 {4} 0 1\n204 {4} 0 0\n"
	                "M_P_Thrown {1}\n10 {3} 0 0\n"
	                "M_P_Walk_Despair_Away_1 {1}\n43 {5} 0 1\n190 {4} 0 1\n822 {4} 0 0\n"
	                "M_P_Yawn {1}\n700 {6} 0 0\n"
	                "END\n",
	                k_CowGroup, k_PeopleGroup, k_BellSound, k_ScreamSound, k_FootstepSound, k_CrySound, k_BanterSound);
	return std::vector<uint8_t>(text.begin(), text.end());
}

/// The same things, clips and bank paths as the installation fixture, with the clips' sounds and the banks above
class AnimationSoundsSyntheticTest: public AnimationSoundsBase
{
protected:
	void SetUp() override
	{
		StartClipAudio(std::nullopt);
		// the clips' sounds already in the resources, so no file is read for them
		const auto clipSounds = MakeClipSounds();
		Locator::resources::value().GetClipSounds().Load(clip_sounds::k_TableId.value(),
		                                                 resources::ClipSoundsLoader::FromBufferTag {},
		                                                 std::span<const uint8_t>(clipSounds));
		editor = RegisterBuffer(MakeClipEffectsBank(), "Audio/Sfx/Game/editor.sad", "editor.sad");
		banter = RegisterBuffer(MakeClipBanterBank(), "Audio/Dialogue/VillagersBanter.sad", "VillagersBanter.sad");
		StartPlayback();
		PlaceTheMan();
	}
	void TearDown() override { StopClipAudio(); }
};
} // namespace

// The expected samples are the game data's own numbers: the row of editor.sad or VillagersBanter.sad the key {voice, 2,
// clip group, surface, sound} picks, and the sample of its list the audio random draws. With rand() at 16383 the draws
// are (8191 + 16383) * count / 32767 on the first, third... call and 8191 * count / 32767 on the second, fourth...

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimationSoundsTest, AMansClipPlaysItsSoundsInHisVoiceAndHisHomesBanterFromHisHome)
{
	AnimationSounds::Fire(k_Villager, k_OnFireRunClip, 0, 1000);
	AnimationSounds::Fire(k_Villager, k_YawnClip, 0, 1000);
	EXPECT_EQ(output.plays, 4);
	// the man's screams 221..228, the first draw: 5
	const auto scream = Find(226);
	ASSERT_TRUE(scream.has_value());
	EXPECT_EQ(scream->owner, Owner::Thing(k_Villager));
	EXPECT_EQ(scream->position, glm::vec3(3.0f, 0.0f, 4.0f));
	// the footsteps on gravel 259..268, the second draw (2) and the third (7)
	const auto step = Find(261);
	ASSERT_TRUE(step.has_value());
	EXPECT_EQ(step->owner, Owner::Thing(k_Villager));
	const auto nextStep = Find(266);
	ASSERT_TRUE(nextStep.has_value());
	EXPECT_EQ(nextStep->owner, Owner::Thing(k_Villager));
	// the banter 1..23, the fourth draw (5), at his home: it is 30 from the camera, beyond the banter's 8, and is heard
	// because the distance is the man's
	const auto chatter = Find(6);
	ASSERT_TRUE(chatter.has_value());
	EXPECT_EQ(chatter->owner, Owner::Thing(k_Abode));
	EXPECT_EQ(chatter->position, glm::vec3(30.0f, 0.0f, 0.0f));
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimationSoundsTest, OnlyTheSoundsTheClipPassesPlay)
{
	// from the time the clip was at, up to but not including the time it is at now: the cry at 43, not the step at 190.
	// The cries 369..373, the first draw: 3
	AnimationSounds::Fire(k_Villager, k_DespairWalkClip, 43, 190);
	ASSERT_EQ(output.plays, 1);
	EXPECT_TRUE(Find(372).has_value());
	AnimationSounds::Fire(k_Villager, k_DespairWalkClip, 190, 190);
	AnimationSounds::Fire(k_Villager, k_DespairWalkClip, 0, 43);
	EXPECT_EQ(output.plays, 1);
	// the footsteps on gravel 259..268, the second draw (2) and the third (7)
	AnimationSounds::Fire(k_Villager, k_DespairWalkClip, 190, 191);
	EXPECT_EQ(output.plays, 2);
	EXPECT_TRUE(Find(261).has_value());
	AnimationSounds::Fire(k_Villager, k_DespairWalkClip, 191, 822);
	EXPECT_EQ(output.plays, 2);
	AnimationSounds::Fire(k_Villager, k_DespairWalkClip, 822, 823);
	EXPECT_EQ(output.plays, 3);
	EXPECT_TRUE(Find(266).has_value());
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimationSoundsTest, AWomanIsHeardInHerVoiceAndAChildInItsOwn)
{
	// the woman's screams 536..541, the first draw: 4
	Person().female = true;
	AnimationSounds::Fire(k_Villager, k_OnFireDieClip, 0, 1000);
	EXPECT_TRUE(Find(540).has_value());
	// the child's screams 620..625, the second draw: 1
	Person().female = false;
	Person().child = true;
	AnimationSounds::Fire(k_Villager, k_OnFireDieClip, 0, 1000);
	EXPECT_TRUE(Find(621).has_value());
	EXPECT_EQ(output.plays, 2);
	// none of the man's 221..228
	for (const auto& heard : Playing())
	{
		EXPECT_TRUE(heard.sample < 221 || heard.sample > 228) << heard.sample;
	}
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimationSoundsTest, ADeadVillagersClipIsQuiet)
{
	s_Animated.at(k_Villager).alive = false;
	AnimationSounds::Fire(k_Villager, k_OnFireRunClip, 0, 1000);
	AnimationSounds::Fire(k_Villager, k_YawnClip, 0, 1000);
	EXPECT_EQ(output.plays, 0);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimationSoundsTest, ADeadAnimalIsQuietOnlyInThePeoplesClips)
{
	AddAnimal();
	s_Animated.at(k_Animal).alive = false;
	// the people's clip (the villagers' voices group) makes the life test on any thing: no scream
	AnimationSounds::Fire(k_Animal, k_OnFireDieClip, 0, 1000);
	EXPECT_EQ(output.plays, 0);
	// its own clip has no life test: the first cow bell still rings
	AnimationSounds::Fire(k_Animal, k_CowWalkClip, 0, 1);
	EXPECT_EQ(output.plays, 1);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimationSoundsTest, AnAnimalHasNoHomeToBanterFrom)
{
	AddAnimal();
	// a thing that is not a villager has the third voice in a people's clip: the child's screams 620..625, the first
	// draw: 4
	AnimationSounds::Fire(k_Animal, k_OnFireDieClip, 0, 1000);
	const auto scream = Find(624);
	ASSERT_TRUE(scream.has_value());
	EXPECT_EQ(scream->owner, Owner::Thing(k_Animal));
	EXPECT_EQ(scream->position, glm::vec3(0.0f, 0.0f, 5.0f));
	// no banter, and no draw for it
	AnimationSounds::Fire(k_Animal, k_YawnClip, 0, 1000);
	EXPECT_EQ(output.plays, 1);
	// its own clip: the cow bells 303..306, the second draw: 0
	AnimationSounds::Fire(k_Animal, k_CowWalkClip, 0, 1);
	EXPECT_EQ(output.plays, 2);
	const auto bell = Find(303);
	ASSERT_TRUE(bell.has_value());
	EXPECT_EQ(bell->owner, Owner::Thing(k_Animal));
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(AnimationSoundsTest, AThrownVillagerCriesOnlyEarlyInItsFlight)
{
	s_Animated.at(k_Villager).turnsSinceStateChange = 15;
	AnimationSounds::Fire(k_Villager, k_ThrownClip, 0, 100);
	EXPECT_EQ(output.plays, 0);
	// the man's screams 221..228, the first draw: 5
	s_Animated.at(k_Villager).turnsSinceStateChange = 14;
	AnimationSounds::Fire(k_Villager, k_ThrownClip, 0, 100);
	EXPECT_EQ(output.plays, 1);
	EXPECT_TRUE(Find(226).has_value());
	// only a villager cries
	AddAnimal();
	AnimationSounds::Fire(k_Animal, k_ThrownClip, 0, 100);
	EXPECT_EQ(output.plays, 1);
}

// ---- the same clips' sounds from the clips and banks built in memory (no game data) ---------------------------------

// The draws are those of the installation's tests, over the synthetic lists: with rand() at 16383 they are
// (8191 + 16383) * count / 32767 on the first, third... call and 8191 * count / 32767 on the second, fourth...

TEST_F(AnimationSoundsSyntheticTest, AMansClipPlaysItsSoundsInHisVoiceAndHisHomesBanterFromHisHomeSynthetic)
{
	AnimationSounds::Fire(k_Villager, k_OnFireRunClip, 0, 1000);
	AnimationSounds::Fire(k_Villager, k_YawnClip, 0, 1000);
	EXPECT_EQ(output.plays, 4);
	// the man's screams, the first draw of 8: 5
	const auto scream = Find(k_ManScreams.first + 5);
	ASSERT_TRUE(scream.has_value());
	EXPECT_EQ(scream->owner, Owner::Thing(k_Villager));
	EXPECT_EQ(scream->position, glm::vec3(3.0f, 0.0f, 4.0f));
	// the footsteps on gravel, the second draw of 10 (2) and the third (7)
	const auto step = Find(k_Footsteps.first + 2);
	ASSERT_TRUE(step.has_value());
	EXPECT_EQ(step->owner, Owner::Thing(k_Villager));
	const auto nextStep = Find(k_Footsteps.first + 7);
	ASSERT_TRUE(nextStep.has_value());
	EXPECT_EQ(nextStep->owner, Owner::Thing(k_Villager));
	// the banter, the fourth draw of 6 (1), at his home: it is 30 from the camera, beyond the banter's 8, and is heard
	// because the distance is the man's
	const auto chatter = Find(k_Banter.first + 1);
	ASSERT_TRUE(chatter.has_value());
	EXPECT_EQ(chatter->owner, Owner::Thing(k_Abode));
	EXPECT_EQ(chatter->position, glm::vec3(30.0f, 0.0f, 0.0f));
}

TEST_F(AnimationSoundsSyntheticTest, OnlyTheSoundsTheClipPassesPlaySynthetic)
{
	// from the time the clip was at, up to but not including the time it is at now: the cry at 43, not the step at 190.
	// The cries, the first draw of 5: 3
	AnimationSounds::Fire(k_Villager, k_DespairWalkClip, 43, 190);
	ASSERT_EQ(output.plays, 1);
	EXPECT_TRUE(Find(k_Cries.first + 3).has_value());
	AnimationSounds::Fire(k_Villager, k_DespairWalkClip, 190, 190);
	AnimationSounds::Fire(k_Villager, k_DespairWalkClip, 0, 43);
	EXPECT_EQ(output.plays, 1);
	// the footsteps on gravel, the second draw of 10 (2) and the third (7)
	AnimationSounds::Fire(k_Villager, k_DespairWalkClip, 190, 191);
	EXPECT_EQ(output.plays, 2);
	EXPECT_TRUE(Find(k_Footsteps.first + 2).has_value());
	AnimationSounds::Fire(k_Villager, k_DespairWalkClip, 191, 822);
	EXPECT_EQ(output.plays, 2);
	AnimationSounds::Fire(k_Villager, k_DespairWalkClip, 822, 823);
	EXPECT_EQ(output.plays, 3);
	EXPECT_TRUE(Find(k_Footsteps.first + 7).has_value());
}

TEST_F(AnimationSoundsSyntheticTest, AWomanIsHeardInHerVoiceAndAChildInItsOwnSynthetic)
{
	// the woman's screams, the first draw of 6: 4
	Person().female = true;
	AnimationSounds::Fire(k_Villager, k_OnFireDieClip, 0, 1000);
	EXPECT_TRUE(Find(k_WomanScreams.first + 4).has_value());
	// the child's screams, the second draw of 6: 1
	Person().female = false;
	Person().child = true;
	AnimationSounds::Fire(k_Villager, k_OnFireDieClip, 0, 1000);
	EXPECT_TRUE(Find(k_ChildScreams.first + 1).has_value());
	EXPECT_EQ(output.plays, 2);
	// none of the man's
	for (const auto& heard : Playing())
	{
		EXPECT_TRUE(heard.sample < k_ManScreams.first || heard.sample >= k_ManScreams.first + k_ManScreams.count)
		    << heard.sample;
	}
}

TEST_F(AnimationSoundsSyntheticTest, ADeadVillagersClipIsQuietSynthetic)
{
	s_Animated.at(k_Villager).alive = false;
	AnimationSounds::Fire(k_Villager, k_OnFireRunClip, 0, 1000);
	AnimationSounds::Fire(k_Villager, k_YawnClip, 0, 1000);
	EXPECT_EQ(output.plays, 0);
}

TEST_F(AnimationSoundsSyntheticTest, ADeadAnimalIsQuietOnlyInThePeoplesClipsSynthetic)
{
	AddAnimal();
	s_Animated.at(k_Animal).alive = false;
	// the people's clip (the villagers' voices group) makes the life test on any thing: no scream
	AnimationSounds::Fire(k_Animal, k_OnFireDieClip, 0, 1000);
	EXPECT_EQ(output.plays, 0);
	// its own clip has no life test: the first cow bell still rings
	AnimationSounds::Fire(k_Animal, k_CowWalkClip, 0, 1);
	EXPECT_EQ(output.plays, 1);
}

TEST_F(AnimationSoundsSyntheticTest, AnAnimalHasNoHomeToBanterFromSynthetic)
{
	AddAnimal();
	// a thing that is not a villager has the third voice in a people's clip: the child's screams, the first draw of 6: 4
	AnimationSounds::Fire(k_Animal, k_OnFireDieClip, 0, 1000);
	const auto scream = Find(k_ChildScreams.first + 4);
	ASSERT_TRUE(scream.has_value());
	EXPECT_EQ(scream->owner, Owner::Thing(k_Animal));
	EXPECT_EQ(scream->position, glm::vec3(0.0f, 0.0f, 5.0f));
	// no banter, and no draw for it
	AnimationSounds::Fire(k_Animal, k_YawnClip, 0, 1000);
	EXPECT_EQ(output.plays, 1);
	// its own clip: the cow bells, the second draw of 4: 0
	AnimationSounds::Fire(k_Animal, k_CowWalkClip, 0, 1);
	EXPECT_EQ(output.plays, 2);
	const auto bell = Find(k_CowBells.first);
	ASSERT_TRUE(bell.has_value());
	EXPECT_EQ(bell->owner, Owner::Thing(k_Animal));
}

TEST_F(AnimationSoundsSyntheticTest, AThrownVillagerCriesOnlyEarlyInItsFlightSynthetic)
{
	s_Animated.at(k_Villager).turnsSinceStateChange = 15;
	AnimationSounds::Fire(k_Villager, k_ThrownClip, 0, 100);
	EXPECT_EQ(output.plays, 0);
	// the man's screams, the first draw of 8: 5
	s_Animated.at(k_Villager).turnsSinceStateChange = 14;
	AnimationSounds::Fire(k_Villager, k_ThrownClip, 0, 100);
	EXPECT_EQ(output.plays, 1);
	EXPECT_TRUE(Find(k_ManScreams.first + 5).has_value());
	// only a villager cries
	AddAnimal();
	AnimationSounds::Fire(k_Animal, k_ThrownClip, 0, 100);
	EXPECT_EQ(output.plays, 1);
}
