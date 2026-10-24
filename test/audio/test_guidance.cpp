/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Audio/Audio.h"
#include "Audio/Device/SampleOutput.h"
#include "Audio/Device/Sound.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/Guidance.h"
#include "Audio/Services/Voices.h"
#include "Common/GUtilsDistance.h"
#include "ECS/AudioQueries.h"
#include "ECS/Systems/Implementations/DialogueControlSystem.h"
#include "ECS/Systems/Implementations/HeartbeatSystem.h"
#include "ECS/Town/TownEmergency.h"
#include "Help/HelpSystem.h"
#include "Help/ScriptControl.h"

// The guidance remarks of the help spirits.
// The random numbers come from SeededRandom on a seed; the expected values of the seeded tests come from an
// independent emulation of the original. A fake
// output stands for the audio device.

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::audio::guidance;

namespace
{
class FakeOutput final: public SampleOutput
{
public:
	std::array<bool, 16> playing {};
	bool Play(size_t channel, Sound&, const Start&) override
	{
		playing[channel] = true;
		return true;
	}
	void Stop(size_t channel) override { playing[channel] = false; }
	void StopRamped(size_t channel) override { playing[channel] = false; }
	[[nodiscard]] int64_t PlayPositionMs(size_t channel) const override { return playing[channel] ? 0 : -1; }
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

/// The texts the fixture gives a Guidance sample (sample n = position + 1)
constexpr std::array<uint32_t, 22> k_GuidanceTexts {4940, 4941, 4942, 4943, 4944, 4945, 4946, 4947, 4948, 4949, 4950,
                                                    4951, 4952, 4953, 4954, 4955, 4956, 4957, 4967, 4968, 4969, 5720};

int GuidanceSampleOf(uint32_t text)
{
	for (size_t i = 0; i < k_GuidanceTexts.size(); ++i)
	{
		if (k_GuidanceTexts.at(i) == text)
		{
			return static_cast<int>(i) + 1;
		}
	}
	return 0;
}

struct Said
{
	uint32_t first;
	uint32_t last;
	std::string script;
};

class GuidanceTest: public ::testing::Test
{
protected:
	FakeOutput output;
	std::map<entt::id_type, Sound> sounds;
	BankId guidanceBank {k_NoBank};
	BankId inGame {k_NoBank};
	static inline uint32_t s_Turn = 100000;
	static inline int s_Land = 2;
	static inline int s_HelpLevel = 3;
	static inline uint32_t s_Seed = 12345;
	static inline std::optional<std::array<float, 3>> s_Needs;
	static inline std::vector<DesireTown> s_Towns;
	static inline HeartBeatInput s_Heart;
	static inline std::vector<Said> s_Said;
	static inline std::vector<int> s_Categories;

	void SetUp() override
	{
		s_Turn = 100000;
		s_Land = 2;
		s_HelpLevel = 3;
		s_Seed = 12345;
		s_Needs.reset();
		s_Towns.clear();
		s_Heart = {};
		s_Said.clear();
		s_Categories.clear();
		GameQueries queries;
		queries.turn = []() { return s_Turn; };
		queries.landNumber = []() { return s_Land; };
		queries.helpLevel = []() { return s_HelpLevel; };
		queries.camera = []() -> std::optional<CameraState> { return CameraState {}; };
		queries.nearestTownAt = [](glm::vec3, float maxDistance) -> std::optional<ThingId> {
			EXPECT_EQ(maxDistance, 100.0f);
			return s_Needs ? std::optional<ThingId>(7) : std::nullopt;
		};
		queries.townResourceNeeds = [](ThingId town) {
			EXPECT_EQ(town, 7u);
			return s_Needs;
		};
		queries.desireTowns = []() { return s_Towns; };
		queries.heartBeat = []() { return s_Heart; };
		queries.helpRunMessage = [](uint32_t first, uint32_t last, std::string_view script) {
			s_Said.push_back({first, last, std::string(script)});
			return true;
		};
		queries.helpTriggerCategory = [](int category) { s_Categories.push_back(category); };
		audio::Init(std::move(queries));
		guidanceBank = RegisterBank("audio/dialogue/Guidance.sad", "Guidance.sad");
		inGame = RegisterBank("audio/sfx/game/ingame.sad", "ingame.sad");
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
		for (int n = 1; n <= static_cast<int>(k_GuidanceTexts.size()); ++n)
		{
			Add(guidanceBank, n);
		}
		Add(inGame, 45);
		// the voice table: text i is named HELP_TEXT_<i>; the Guidance waves are the texts of k_GuidanceTexts
		std::vector<std::string> names;
		for (uint32_t i = 0; i < 6974; ++i)
		{
			names.push_back(fmt::format("HELP_TEXT_{}", i));
		}
		VoiceTable::SampleNames guidance;
		for (const auto text : k_GuidanceTexts)
		{
			guidance.push_back(fmt::format("K:\\x\\HELP_TEXT_{}.wav", text));
		}
		voices::SetTable(VoiceTable::Build(names, {}, {}, guidance));
		std::array<SpriteList, k_SpriteLists> lists {};
		for (size_t k = 0; k < lists.size(); ++k)
		{
			// list k: 1000 k + 1 .. 1000 k + 3
			lists.at(k) = {static_cast<uint32_t>(1000 * k + 1), static_cast<uint32_t>(1000 * k + 2),
			               static_cast<uint32_t>(1000 * k + 3)};
		}
		SetSpriteLists(lists);
		SetRandom([](uint32_t n) { return SeededRandom(n, s_Seed); });
		ResetForTests();
	}
	void TearDown() override
	{
		SetRandom({});
		audio::ClearMap();
		sample_play::SetMainVolume(127); // back to its default (the maximum)
		sample_play::SetBackend({});
		voices::SetTable({});
		audio::Shutdown();
	}

	void Add(BankId bank, int number)
	{
		Sound sound;
		sound.name = fmt::format("{} {}", BankGroup(bank), number);
		sound.id = number;
		sound.bank = bank;
		sound.priority = 20;
		sound.sampleRate = 22050;
		sound.pitch = 100;
		sound.pitchDeviation = 0;
		sound.duration = 2.0f;
		sounds.emplace(SampleId(bank, number), std::move(sound));
	}

	[[nodiscard]] static std::optional<sample_play::ChannelInfo> Playing(BankId bank)
	{
		for (const auto& info : sample_play::Channels())
		{
			if (info.playing && info.bank == bank)
			{
				return info;
			}
		}
		return std::nullopt;
	}
};
} // namespace

TEST(GuidanceTable, TypeTableRows)
{
	// spot checks of the type table: {base, level, always}
	EXPECT_EQ(k_Types.at(0).base, 50u);
	EXPECT_FALSE(k_Types.at(0).always);
	EXPECT_EQ(k_Types.at(4).helpLevel, 0);
	EXPECT_EQ(k_Types.at(8).base, 100u);
	EXPECT_EQ(k_Types.at(16).helpLevel, 4);
	EXPECT_EQ(k_Types.at(28).base, 10000u);
	EXPECT_TRUE(k_Types.at(30).always);
	EXPECT_EQ(k_Types.at(30).helpLevel, 3);
}

TEST(GuidanceRandom, SequenceFromASeed)
{
	// seed * 9377 + 9439, rotated right by 13, % n
	uint32_t seed = 12345;
	const std::array<uint32_t, 5> expected {507, 504, 162, 496, 651};
	for (const auto value : expected)
	{
		EXPECT_EQ(SeededRandom(1000, seed), value);
	}
}

TEST_F(GuidanceTest, IntervalWithASeed)
{
	// base + LocalRand(trunc(5 base (1 - r^3))), r = LocalFloatRand(1)
	s_Seed = 12345;
	EXPECT_EQ(Interval(Type::TownBeingAttacked), 5912u); // base 1000
	EXPECT_EQ(Interval(Type::TownBeingAttacked), 1249u);
	s_Seed = 12345;
	EXPECT_EQ(Interval(Type::TownDesire), 141u); // base 50
	EXPECT_EQ(Interval(Type::TownDesire), 282u);
	s_Seed = 12345;
	EXPECT_EQ(Interval(Type::VeryGood), 45076u); // base 10000
	// base 0: LocalRand(0) is 0 without a draw
	const auto before = s_Seed;
	s_Seed = before;
	EXPECT_EQ(Interval(Type::Disciple), 0u);
	// every interval lies in [base, 6 base)
	for (int i = 0; i < 200; ++i)
	{
		const auto v = Interval(Type::LowOnFood);
		EXPECT_GE(v, 2500u);
		EXPECT_LT(v, 15000u);
	}
}

TEST_F(GuidanceTest, PlayNowGates)
{
	// enough turns since the last play (lastPlayed 0, turn 100000 > any interval)
	EXPECT_TRUE(PlayNow(Type::TownDesire));
	// land 1 of a single-player campaign mutes the types that are not `always`
	s_Land = 1;
	EXPECT_FALSE(PlayNow(Type::TownDesire));
	EXPECT_TRUE(PlayNow(Type::TownAttack)); // always
	s_Land = 2;
	// the help level (0 while the help is off) below the type's
	s_HelpLevel = 2;
	EXPECT_FALSE(PlayNow(Type::CreatureAttackingThem)); // needs 3
	EXPECT_TRUE(PlayNow(Type::TownBeingAttacked));      // needs 2
	s_HelpLevel = 0;
	EXPECT_TRUE(PlayNow(Type::Disciple)); // needs 0
	EXPECT_FALSE(PlayNow(Type::HeartBeat));
	s_HelpLevel = 3;
	// TimeSince must be above the interval (unsigned, strict): just played -> 0 > base fails
	PlaySample(false, 4946, 0, static_cast<int>(Type::ResourceDrop), 127, 100, 90, std::nullopt, 0.0f, true);
	EXPECT_EQ(TimeSinceLastPlayed(Type::ResourceDrop), 0u);
	EXPECT_FALSE(PlayNow(Type::ResourceDrop));
	s_Turn += 25; // 25 > base 25 + LocalRand(...) never
	EXPECT_FALSE(PlayNow(Type::ResourceDrop));
	s_Turn += 1000; // past 6 x 25
	EXPECT_TRUE(PlayNow(Type::ResourceDrop));
	// CanPlaySpiritRemark: the gate type 8 first
	EXPECT_TRUE(CanPlaySpiritRemark(Type::LosingBelief));
}

TEST_F(GuidanceTest, InitDrawsTwiceForAPositiveFirstDraw)
{
	// last = turn - LocalRand(base) when that is > 0, with a second draw; else 0
	std::vector<uint32_t> draws;
	SetRandom([&draws](uint32_t n) {
		draws.push_back(n);
		return n / 2;
	});
	s_Turn = 2000;
	Init();
	const auto& state = GetState();
	// type 0, base 50: 2000 - 25 > 0, drawn again: 2000 - 25
	EXPECT_EQ(state.lastPlayed.at(0), 1975u);
	// type 28, base 10000: 2000 - 5000 <= 0 -> 0, one draw
	EXPECT_EQ(state.lastPlayed.at(28), 0u);
	// type 4, base 0: LocalRand(0) = 0 without asking -> 2000 > 0 -> 2000
	EXPECT_EQ(state.lastPlayed.at(4), 2000u);
	// the draws: two per type with turn - draw > 0 and base > 0, one for the others with base > 0
	EXPECT_EQ(draws.at(0), 50u);
	EXPECT_EQ(draws.at(1), 50u);
	EXPECT_EQ(state.heartBeatPitch, 30.0f);
	EXPECT_EQ(state.options.mode, 2);
	EXPECT_FALSE(state.options.is3D);
	EXPECT_FALSE(state.options.track);
}

TEST_F(GuidanceTest, PlaySampleOptions)
{
	// 3D: max = maxDistance, min = maxDistance x 0.333, mask 0x180, track 0; the sample from the voice table
	const auto channel = PlaySample(true, 4946, 1, static_cast<int>(Type::ResourceDrop), 127, 100, 90,
	                                glm::vec3(10.0f, 0.0f, 10.0f), 200.0f, true);
	EXPECT_NE(channel, k_NoChannel);
	const auto& state = GetState();
	EXPECT_EQ(state.options.maxDistance, 200.0f);
	EXPECT_FLOAT_EQ(state.options.minDistance, 200.0f * 0.333f);
	EXPECT_EQ(state.options.callerMask, 0x180u);
	EXPECT_FALSE(state.options.track);
	EXPECT_EQ(state.options.owner, Owner::Key(1));
	const auto info = Playing(guidanceBank);
	ASSERT_TRUE(info);
	EXPECT_EQ(info->sample, GuidanceSampleOf(4946));
	EXPECT_TRUE(info->is3D);
	EXPECT_EQ(TimeSinceLastPlayed(Type::ResourceDrop), 0u);
	// 2D: is3D 0, the rest as the 3D one left it; owner 0 is no owner
	PlaySample(false, 5720, 0, static_cast<int>(Type::DeathInVillage), 127, 100, 90, std::nullopt, 0.0f, true);
	EXPECT_FALSE(state.options.is3D);
	EXPECT_EQ(state.options.callerMask, 0x180u);
	EXPECT_EQ(state.options.owner, Owner::None());
	// 3D without a point: nothing, and lastPlayed stays
	s_Turn += 10;
	EXPECT_EQ(PlaySample(true, 4947, 0, static_cast<int>(Type::Belief), 127, 100, 90, std::nullopt, 200.0f, true), k_NoChannel);
	EXPECT_EQ(state.lastPlayed.at(static_cast<size_t>(Type::Belief)), 0u);
}

TEST_F(GuidanceTest, ResourceDropSamples)
{
	// ResourceDropSample: >= 0.5 pleased, < 0.25 displeased (food) or pleased again (wood, rain)
	for (int i = 0; i < 20; ++i)
	{
		const auto food = ResourceDropSample(0.5f, RainType::Food);
		EXPECT_GE(food, 4946u);
		EXPECT_LE(food, 4948u);
		const auto hungry = ResourceDropSample(0.1f, RainType::Food);
		EXPECT_GE(hungry, 4955u);
		EXPECT_LE(hungry, 4957u);
		const auto wood = ResourceDropSample(0.0f, RainType::Wood);
		EXPECT_GE(wood, 4949u);
		EXPECT_LE(wood, 4951u);
		const auto rain = ResourceDropSample(0.9f, RainType::Rain);
		EXPECT_GE(rain, 4952u);
		EXPECT_LE(rain, 4954u);
	}
	EXPECT_EQ(ResourceDropSample(0.3f, RainType::Food), 0u);
	EXPECT_EQ(ResourceDropSample(0.25f, RainType::Wood), 0u);
	EXPECT_EQ(ResourceDropSample(1.0f, RainType::None), 0u);
}

TEST_F(GuidanceTest, FoodDroppedOnATownThatWantsItIsPleased)
{
	// PlayResourceDropRemark: no town within 100 -> silence
	PlayResourceDropRemark(glm::vec3(5.0f, 0.0f, 5.0f), RainType::Food);
	EXPECT_FALSE(Playing(guidanceBank));
	// a town whose food values add up to 0.7: PLEASED_FOOD, 3D at the drop, max 200
	s_Needs = std::array<float, 3> {0.7f, 0.0f, 0.0f};
	PlayResourceDropRemark(glm::vec3(5.0f, 0.0f, 5.0f), RainType::Food);
	const auto info = Playing(guidanceBank);
	ASSERT_TRUE(info);
	EXPECT_GE(info->sample, GuidanceSampleOf(4946));
	EXPECT_LE(info->sample, GuidanceSampleOf(4948));
	EXPECT_TRUE(info->is3D);
	EXPECT_EQ(GetState().options.maxDistance, 200.0f);
	// on land 1 of the campaign (ResourceDrop is not `always`): silence
	ClearMap();
	ResetForTests();
	s_Land = 1;
	PlayResourceDropRemark(glm::vec3(5.0f, 0.0f, 5.0f), RainType::Food);
	EXPECT_FALSE(Playing(guidanceBank));
}

TEST_F(GuidanceTest, ATownsPeopleAreAwedByAPlayerTheyBelieveInLessThanTheStrongest)
{
	// player 1 believed in at 0.2 by a town whose strongest belief, player 0's, is 1: (0.2 + 0.0001) / (1 + 0.0001)
	// less a third of it at most stays in 0.05..0.4, the third awe of the alignment's (GOOD_AWE_03 4942), 3D at the point
	std::array<float, 8> beliefs {};
	beliefs.at(0) = 1.0f;
	beliefs.at(1) = 0.2f;
	const glm::vec3 point(5.0f, 0.0f, 5.0f);
	PlayBeliefRemark(beliefs, 1, point, 0.0f, 1);
	auto info = Playing(guidanceBank);
	ASSERT_TRUE(info);
	EXPECT_EQ(info->sample, GuidanceSampleOf(4942));
	EXPECT_TRUE(info->is3D);
	EXPECT_EQ(GetState().options.maxDistance, 200.0f);
	// the evil alignment: EVIL_AWE_03 4945
	ClearMap();
	ResetForTests();
	PlayBeliefRemark(beliefs, 1, point, 0.0f, 2);
	info = Playing(guidanceBank);
	ASSERT_TRUE(info);
	EXPECT_EQ(info->sample, GuidanceSampleOf(4945));
	// the strongest believed-in player is not awed about
	ClearMap();
	ResetForTests();
	PlayBeliefRemark(beliefs, 0, point, 0.0f, 1);
	EXPECT_FALSE(Playing(guidanceBank));
	// nearly as strong: the first or second awe (0.66..1)
	ClearMap();
	ResetForTests();
	beliefs.at(1) = 0.99f;
	PlayBeliefRemark(beliefs, 1, point, 0.0f, 1);
	info = Playing(guidanceBank);
	ASSERT_TRUE(info);
	EXPECT_TRUE(info->sample == GuidanceSampleOf(4940) || info->sample == GuidanceSampleOf(4941)) << info->sample;
	// at the edge of hearing (200 from the hand) the visibility 1 - d^2 is 0: nothing
	ClearMap();
	ResetForTests();
	PlayBeliefRemark(beliefs, 1, point, 200.0f, 1);
	EXPECT_FALSE(Playing(guidanceBank));
}

TEST_F(GuidanceTest, DesireSampleAndScore)
{
	// DesireSample with no random part (LocalFloatRand(0) for value 0 is 0; here the draws are 0)
	SetRandom([](uint32_t) { return 0u; });
	EXPECT_EQ(DesireSample(0, 0.9f), 4967u);
	EXPECT_EQ(DesireSample(0, 0.7f), 4968u);
	EXPECT_EQ(DesireSample(0, 0.5f), 4969u);
	EXPECT_EQ(DesireSample(0, 0.4f), 0u);
	EXPECT_EQ(DesireSample(5, 0.9f), 4973u);
	EXPECT_EQ(DesireSample(2, 0.9f), 0u);
	// DesireScore: 2 t0 s v^3 (1 - d^2) (1 - t1^3); t0 = 1 (long since the last desire), a new thing t1 = 0, d = 100 / 200
	const float score = DesireScore(77, 100.0f, 0.5f, 4967);
	EXPECT_FLOAT_EQ(score, static_cast<float>(2.0 * 0.125 * 0.75));
	// the same thing 150 turns later: t1 = 0.5
	s_Turn += 150;
	EXPECT_FLOAT_EQ(DesireScore(77, 100.0f, 0.5f, 4967), static_cast<float>(2.0 * 0.125 * 0.75 * (1.0 - 0.125)));
	// past 600 turns the thing is new again
	s_Turn += 600;
	EXPECT_EQ(TimeSinceThingSeen(77), 0u);
}

TEST_F(GuidanceTest, TownDesireEveryTenTurns)
{
	SetRandom([](uint32_t) { return 0u; });
	DesireTown town;
	town.id = 5;
	town.position = glm::vec3(20.0f, 0.0f, 20.0f);
	town.storagePit = glm::vec3(10.0f, 0.0f, 0.0f);
	town.population = 4;
	town.desires.at(0) = {0.9f, 0, 1.0f}; // DESIRE_FOOD, raw 1
	s_Towns = {town};
	s_Turn = 100001; // not a multiple of 10
	UpdateTownDesireRemarks();
	EXPECT_FALSE(Playing(guidanceBank));
	s_Turn = 100000;
	UpdateTownDesireRemarks();
	const auto info = Playing(guidanceBank);
	ASSERT_TRUE(info);
	EXPECT_EQ(info->sample, GuidanceSampleOf(4967));
	EXPECT_EQ(GetState().lastDesireSample, 4967u);
	// max distance 200 x (value - 0): the value is 2 x 1 x 1 x (1 - (d / 200)^2) x 1, d the pit's distance to the
	// camera by gutils::GetDistanceInMetres (the 1 / sqrt table: 10 m comes out a little short)
	const float d = gutils::GetDistanceInMetres(glm::vec3(10.0f, 0.0f, 0.0f), glm::vec3(0.0f));
	EXPECT_NEAR(d, 10.0f, 0.01f);
	const double near = static_cast<double>(d / 200.0f);
	EXPECT_FLOAT_EQ(GetState().options.maxDistance, 200.0f * static_cast<float>(2.0 * (1.0 - near * near)));
	// a town without people or storage pit says nothing
	ClearMap();
	ResetForTests();
	s_Towns.at(0).population = 0;
	UpdateTownDesireRemarks();
	EXPECT_FALSE(Playing(guidanceBank));
}

TEST_F(GuidanceTest, AlignmentRemarks)
{
	SetRandom([](uint32_t) { return 0u; });
	// below 2 x the max change a turn: nothing
	UpdateAlignmentRemarks(0.001f, 0.9f, 0.01f);
	EXPECT_TRUE(s_Said.empty());
	// a good player getting better (same sign), |a| > 0.75: type 29 and list 20 (sic)
	UpdateAlignmentRemarks(0.1f, 0.9f, 0.01f);
	ASSERT_EQ(s_Said.size(), 1u);
	EXPECT_EQ(s_Said.at(0).first, 20001u);
	EXPECT_EQ(s_Said.at(0).last, 20001u);
	EXPECT_EQ(s_Said.at(0).script, "MultiHelpJustTalkWithText");
	EXPECT_EQ(s_Categories.at(0), 8);
	EXPECT_EQ(TimeSinceLastPlayed(Type::VeryEvil), 0u);
	// a good player turning evil, |a| > 0.4: type 25, list 16
	ResetForTests();
	s_Said.clear();
	UpdateAlignmentRemarks(-0.1f, 0.5f, 0.01f);
	ASSERT_EQ(s_Said.size(), 1u);
	EXPECT_EQ(s_Said.at(0).first, 16001u);
	EXPECT_EQ(TimeSinceLastPlayed(Type::GoodBeingEvil), 0u);
	// an evil player turning good: type 26, list 17
	ResetForTests();
	s_Said.clear();
	UpdateAlignmentRemarks(0.1f, -0.5f, 0.01f);
	ASSERT_EQ(s_Said.size(), 1u);
	EXPECT_EQ(s_Said.at(0).first, 17001u);
	// the accumulator decays by 0.95 a call
	EXPECT_FLOAT_EQ(GetState().alignmentChange, 0.1f);
	UpdateAlignmentRemarks(0.0f, -0.5f, 0.01f);
	EXPECT_FLOAT_EQ(GetState().alignmentChange, 0.095f);
}

TEST(GuidanceDisciple, TheLocalPlayersAlignmentIsGoodAboveAndEvilBelowFiftyFiveHundredths)
{
	// Good only strictly above 0.55, evil only strictly below -0.55, neutral between and at both edges
	EXPECT_EQ(AlignmentClass(1.0f), 1);
	EXPECT_EQ(AlignmentClass(std::nextafter(0.55f, 1.0f)), 1);
	EXPECT_EQ(AlignmentClass(0.55f), 0);
	EXPECT_EQ(AlignmentClass(0.0f), 0);
	EXPECT_EQ(AlignmentClass(-0.55f), 0);
	EXPECT_EQ(AlignmentClass(std::nextafter(-0.55f, -1.0f)), 2);
	EXPECT_EQ(AlignmentClass(-1.0f), 2);
}

TEST(GuidanceDisciple, EachJobGivenHasItsOwnTextAndTheBreederSaysWhoLivesHere)
{
	// The original's table of the ten jobs' texts, by disciple type; 0 and 6 say nothing
	constexpr std::array<uint32_t, 10> k_Original {0, 4863, 4865, 4868, 4867, 4869, 0, 4870, 4864, 4866};
	for (uint32_t disciple = 0; disciple < k_Original.size(); ++disciple)
	{
		for (int alignment = 0; alignment <= 2; ++alignment)
		{
			EXPECT_EQ(DiscipleText(disciple, alignment), k_Original.at(disciple)) << disciple << " " << alignment;
		}
	}
	// The breeder's remark depends on the local player's alignment and a draw: above a half the first of the pair,
	// else the second; neutral speaks as good
	SetRandom([](uint32_t) { return 0xFFFEu; });
	EXPECT_EQ(DiscipleText(10, 0), 0x12FBu); // GOOD_LIVE_HERE_01
	EXPECT_EQ(DiscipleText(10, 1), 0x12FBu);
	EXPECT_EQ(DiscipleText(10, 2), 0x12FDu); // EVIL_LIVE_HERE_01
	EXPECT_EQ(DiscipleText(10, -1), 0u);
	SetRandom([](uint32_t) { return 0u; });
	EXPECT_EQ(DiscipleText(10, 0), 0x12FCu); // GOOD_LIVE_HERE_02
	EXPECT_EQ(DiscipleText(10, 1), 0x12FCu);
	EXPECT_EQ(DiscipleText(10, 2), 0x12FEu); // EVIL_LIVE_HERE_02
	SetRandom({});
}

TEST_F(GuidanceTest, MakingADiscipleIsRemarkedOnIn2DAtVolumeEightyFiveAtMostOnceATurn)
{
	// The remark is always allowed and needs no help level, so it plays on land 1 and with the help off
	s_Land = 1;
	s_HelpLevel = 0;
	PlayDiscipleRemark(1, 0.0f);
	const auto& state = GetState();
	EXPECT_FALSE(state.options.is3D);
	EXPECT_EQ(state.options.volume, 85);
	EXPECT_EQ(state.options.pitch, 100);
	EXPECT_EQ(state.options.pan, 90);
	EXPECT_EQ(TimeSinceLastPlayed(Type::Disciple), 0u);
	// Its interval is nought turns: in the same turn it waits, a turn later it plays again
	PlaySample(false, 5720, 0, static_cast<int>(Type::DeathInVillage), 127, 100, 90, std::nullopt, 0.0f, true);
	PlayDiscipleRemark(1, 0.0f);
	EXPECT_EQ(state.options.volume, 127);
	s_Turn += 1;
	EXPECT_EQ(TimeSinceLastPlayed(Type::Disciple), 1u);
	PlayDiscipleRemark(1, 0.0f);
	EXPECT_EQ(state.options.volume, 85);
	EXPECT_EQ(TimeSinceLastPlayed(Type::Disciple), 0u);
}

TEST_F(GuidanceTest, OneOffOnceAndTheNoTextScript)
{
	// OneOff(5): probability 0.1; a draw of 0 passes, then never again this land
	SetRandom([](uint32_t) { return 0u; });
	OneOff(5);
	ASSERT_EQ(s_Said.size(), 1u);
	EXPECT_EQ(s_Said.at(0).first, 3326u);
	EXPECT_EQ(TimeSinceLastPlayed(Type::OneOff), 0u);
	OneOff(5);
	EXPECT_EQ(s_Said.size(), 1u);
	// a draw above the probability: not said, the flag stays clear
	SetRandom([](uint32_t n) { return n - 1; });
	OneOff(1);
	EXPECT_EQ(s_Said.size(), 1u);
	EXPECT_FALSE(GetState().oneOffs.at(1));
	// type 31 uses the other help script
	SpiritSay(42, Type::JustTalkNoText);
	EXPECT_EQ(s_Said.back().script, "MultiHelpJustTalkWithNoText");
}

TEST_F(GuidanceTest, SpiritSayRestartsTheRemarksGate)
{
	// a remark of any type stamps its own type and the remarks' common gate, so the next remark waits again
	s_Turn = 100000;
	SpiritSay(42, Type::AttackingTown);
	EXPECT_EQ(TimeSinceLastPlayed(Type::AttackingTown), 0u);
	EXPECT_EQ(TimeSinceLastPlayed(Type::HelpSprites), 0u);
	s_Turn = 100050;
	EXPECT_EQ(TimeSinceLastPlayed(Type::HelpSprites), 50u);
	// a type past the table stamps the gate alone
	SpiritSay(43, Type::_Count);
	EXPECT_EQ(TimeSinceLastPlayed(Type::HelpSprites), 0u);
	EXPECT_EQ(TimeSinceLastPlayed(Type::AttackingTown), 50u);
}

TEST_F(GuidanceTest, EndWaitsLetsTheNextRemarkThrough)
{
	// just played: the gate's wait is not over; after EndWaits every type is half of all the turns ago
	SpiritSay(42, Type::AttackingTown);
	EXPECT_FALSE(PlayNow(Type::HelpSprites));
	EndWaits();
	for (size_t type = 0; type < k_TypeCount; ++type)
	{
		EXPECT_EQ(TimeSinceLastPlayed(static_cast<Type>(type)), 0x80000000u);
	}
	EXPECT_TRUE(PlayNow(Type::HelpSprites));
}

TEST_F(GuidanceTest, TheCreatureFightingRemarksWaitUntilTheyAreAllowedAgain)
{
	// stopped, the remark about the creature attacking says nothing, and allowed again it is made
	EndWaits();
	EXPECT_TRUE(GetState().enabled);
	SetEnabled(false);
	EXPECT_FALSE(GetState().enabled);
	RemarkCreatureAttacking();
	EXPECT_TRUE(s_Said.empty());
	SetEnabled(true);
	EXPECT_TRUE(GetState().enabled);
	RemarkCreatureAttacking();
	EXPECT_EQ(s_Said.size(), 1u);
}

TEST_F(GuidanceTest, AttackingATownAndWreckingABuilding)
{
	// another player's town with people: one of list 4's texts; the local player's own town or one without people:
	// nothing
	EndWaits();
	RemarkAttackingTown(true, 10);
	RemarkAttackingTown(false, 0);
	EXPECT_TRUE(s_Said.empty());
	RemarkAttackingTown(false, 10);
	ASSERT_EQ(s_Said.size(), 1u);
	EXPECT_GE(s_Said.at(0).first, 4001u);
	EXPECT_LE(s_Said.at(0).first, 4003u);
	// a wrecked building off screen: nothing; on screen: one of list 21's
	EndWaits();
	RemarkBuildingDestroyed(false);
	EXPECT_EQ(s_Said.size(), 1u);
	RemarkBuildingDestroyed(true);
	ASSERT_EQ(s_Said.size(), 2u);
	EXPECT_GE(s_Said.at(1).first, 21001u);
	EXPECT_LE(s_Said.at(1).first, 21003u);
}

TEST(GuidanceTownAttack, TheLocalPlayersAttackAndItsCreaturesAreRemarkedOn)
{
	using ecs::town_emergency::AttackingCreature;
	using ecs::town_emergency::AttackRemark;
	using ecs::town_emergency::AttackRemarkFor;
	constexpr auto k_Local = PlayerNames::PLAYER_ONE;
	constexpr auto k_Other = PlayerNames::PLAYER_TWO;
	EXPECT_EQ(AttackRemarkFor(PlayerNames::PLAYER_ONE, std::nullopt, k_Other, k_Local), AttackRemark::AttackingTown);
	EXPECT_EQ(AttackRemarkFor(PlayerNames::PLAYER_TWO, std::nullopt, k_Other, k_Local), AttackRemark::None);
	EXPECT_EQ(AttackRemarkFor(PlayerNames::NEUTRAL, std::nullopt, k_Other, k_Local), AttackRemark::None);
	// harm applied by the local player's creature: its own remark, whoever the caused player is
	const AttackingCreature mine {.owner = k_Local, .ownersCreature = true};
	EXPECT_EQ(AttackRemarkFor(PlayerNames::NEUTRAL, mine, k_Other, k_Local), AttackRemark::CreatureAttackingThem);
	// but not on the town of its own player, and never for another player's creature
	EXPECT_EQ(AttackRemarkFor(PlayerNames::PLAYER_ONE, mine, k_Local, k_Local), AttackRemark::None);
	EXPECT_EQ(AttackRemarkFor(PlayerNames::PLAYER_ONE, AttackingCreature {.owner = k_Other}, k_Other, k_Local),
	          AttackRemark::None);
	// a second creature of the local player is not the one it leads: its attack on the player's own town is remarked
	EXPECT_EQ(AttackRemarkFor(PlayerNames::PLAYER_ONE, AttackingCreature {.owner = k_Local}, k_Local, k_Local),
	          AttackRemark::CreatureAttackingThem);
}

TEST_F(GuidanceTest, HeartBeatPitchAndTheCitadelHeart)
{
	// HeartBeat: pitch = (30 + 70 v - pitch) x 0.1 + pitch from 30
	HeartBeat(1.0f);
	EXPECT_FLOAT_EQ(GetState().heartBeatPitch, 37.0f);
	EXPECT_FALSE(Playing(inGame)); // no citadel heart
	// the phase: + pitch x 0.025 x 100 ms x 0.001
	EXPECT_FLOAT_EQ(GetState().heartBeatPhase, 37.0f * 0.025f * 0.1f);
	// with a heart: InGame 45, 3D, looping, pitch trunc(the beat's pitch); then the options back to Guidance with loops 0
	s_Heart.citadelHeart = glm::vec3(1.0f, 0.0f, 1.0f);
	HeartBeat(1.0f);
	const auto info = Playing(inGame);
	ASSERT_TRUE(info);
	EXPECT_EQ(info->sample, 45);
	EXPECT_TRUE(info->is3D);
	EXPECT_EQ(GetState().options.loops, 0);
	EXPECT_EQ(GetState().sample.bank, guidanceBank);
	EXPECT_EQ(GetState().options.maxDistance, 500.0f);
	// UpdateHeartBeat every 10 turns: all inputs 0 -> value 0
	UpdateHeartBeat();
	EXPECT_EQ(GetState().heartBeatValue, 0.0f);
	// an enemy creature 200 from my town: 1 - (200 - 100) / 400
	s_Heart.enemyCreatureDistances = {200};
	UpdateHeartBeat();
	EXPECT_FLOAT_EQ(GetState().heartBeatValue, 0.75f);
}

TEST_F(GuidanceTest, TheHeartbeatServiceReadsTheLocalPlayersBeat)
{
	// the heartbeat the temples' columns beat with is the guidance's, the local player's (the first player without the
	// player service); every other player has none
	HeartBeat(1.0f);
	HeartBeat(1.0f);
	const ecs::systems::HeartbeatSystem heartbeats;
	EXPECT_FLOAT_EQ(heartbeats.Pulse(PlayerNames::PLAYER_ONE, 0.25f), HeartBeatPulse(0.25f));
	EXPECT_FLOAT_EQ(heartbeats.Pulse(PlayerNames::PLAYER_ONE, 0.0f), GetState().heartBeatPulsePrevious);
	EXPECT_FLOAT_EQ(heartbeats.Pulse(PlayerNames::PLAYER_ONE, 1.0f), GetState().heartBeatPulse);
	EXPECT_EQ(heartbeats.Pulse(PlayerNames::PLAYER_TWO, 0.5f), 0.0f);
	const auto state = heartbeats.StateOf(PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(state.has_value());
	EXPECT_EQ(state->rate, GetState().heartBeatPitch);
	EXPECT_EQ(state->phase, GetState().heartBeatPhase);
	EXPECT_EQ(state->pulse, GetState().heartBeatPulse);
	EXPECT_EQ(state->previousPulse, GetState().heartBeatPulsePrevious);
	EXPECT_EQ(state->stress, GetState().heartBeatValue);
	EXPECT_FALSE(heartbeats.StateOf(PlayerNames::PLAYER_TWO).has_value());
}

TEST(HeartBeatEnemies, OtherPlayersCreaturesNearTheLocalPlayersTowns)
{
	using openblack::ecs::audio_queries::CreatureNearTown;
	using openblack::ecs::audio_queries::EnemyCreatureDistances;
	const std::array creatures {
	    // the local player's own creature near its town: not an enemy
	    CreatureNearTown {.player = PlayerNames::PLAYER_ONE, .townOwner = PlayerNames::PLAYER_ONE, .distance = 10},
	    // another player's creature near the local player's town
	    CreatureNearTown {.player = PlayerNames::PLAYER_TWO, .townOwner = PlayerNames::PLAYER_ONE, .distance = 250},
	    // another player's creature near its own town
	    CreatureNearTown {.player = PlayerNames::PLAYER_THREE, .townOwner = PlayerNames::PLAYER_THREE, .distance = 20},
	    // a creature with no town at all
	    CreatureNearTown {.player = PlayerNames::PLAYER_FOUR, .townOwner = std::nullopt, .distance = 0},
	    // the neutral player's creature counts as another player's
	    CreatureNearTown {.player = PlayerNames::NEUTRAL, .townOwner = PlayerNames::PLAYER_ONE, .distance = 90},
	};
	EXPECT_EQ(EnemyCreatureDistances(creatures, PlayerNames::PLAYER_ONE), (std::vector<uint32_t> {250, 90}));
}

TEST_F(GuidanceTest, HeartBeatRunsEveryTurnTheValueEveryTen)
{
	// HeartBeat(value) also runs off the tenth turns; the value is only read on them
	s_Heart.enemyCreatureDistances = {200};
	s_Turn = 100000;
	UpdateHeartBeat();
	EXPECT_FLOAT_EQ(GetState().heartBeatValue, 0.75f);
	const float pitch10 = GetState().heartBeatPitch;
	EXPECT_FLOAT_EQ(pitch10, (30.0f + 70.0f * 0.75f - 30.0f) * 0.1f + 30.0f);
	const float phase10 = GetState().heartBeatPhase;
	// turn 100001: the input changes but is not read; the beat moves on with the kept 0.75
	s_Heart.enemyCreatureDistances = {};
	s_Turn = 100001;
	UpdateHeartBeat();
	EXPECT_FLOAT_EQ(GetState().heartBeatValue, 0.75f);
	EXPECT_FLOAT_EQ(GetState().heartBeatPitch, (30.0f + 70.0f * 0.75f - pitch10) * 0.1f + pitch10);
	EXPECT_GT(GetState().heartBeatPhase, phase10);
}

TEST_F(GuidanceTest, TheMapsClearsHeartBeatFindsNoTownAndNoCitadel)
{
	// the land's towns have left their players and the citadel is gone: only the belief share is as it was
	s_Heart = {.protectionDesire = 0.9f,
	           .believers = 0.5f,
	           .beliefShare = 2.0f,
	           .enemyCreatureDistances = {150},
	           .citadelHeart = glm::vec3(1.0f, 0.0f, 1.0f)};
	const auto atClear = HeartBeatInputAtClearMap(s_Heart);
	EXPECT_EQ(atClear.protectionDesire, 0.0f);
	EXPECT_EQ(atClear.believers, 0.0f);
	EXPECT_EQ(atClear.beliefShare, 2.0f);
	EXPECT_TRUE(atClear.enemyCreatureDistances.empty());
	EXPECT_FALSE(atClear.citadelHeart.has_value());
	// on a tenth turn the value is worked out from that, and the beat moves on with no sample
	s_Turn = 100000;
	UpdateHeartBeatAtClearMap();
	constexpr float k_Small = 0.001f;
	const float value =
	    std::clamp((0.0f + k_Small) / (2.0f + k_Small) - 1.0f + ((0.0f + k_Small) / (0.0f + k_Small) - 1.0f), 0.0f, 1.0f);
	EXPECT_FLOAT_EQ(GetState().heartBeatValue, value);
	EXPECT_FLOAT_EQ(GetState().beliefShare, 0.2f);
	EXPECT_EQ(GetState().believers, 0.0f);
	EXPECT_FLOAT_EQ(GetState().heartBeatPitch, (30.0f + 70.0f * value - 30.0f) * 0.1f + 30.0f);
	EXPECT_FALSE(Playing(inGame));
	// off a tenth turn the beat still moves, still with no citadel heart to play at
	const float phase = GetState().heartBeatPhase;
	s_Turn = 100001;
	UpdateHeartBeatAtClearMap();
	EXPECT_GT(GetState().heartBeatPhase, phase);
	EXPECT_FALSE(Playing(inGame));
	// the turn's own beat, with the heart there, plays it
	UpdateHeartBeat();
	EXPECT_TRUE(Playing(inGame));
}

TEST_F(GuidanceTest, ALandChangeKeepsTheGuidanceAndOnlyANewGameStartsIt)
{
	std::vector<uint32_t> draws;
	SetRandom([&draws](uint32_t n) {
		draws.push_back(n);
		return n / 2;
	});
	// a new game starts it: each type's last play drawn back from the turn
	s_Turn = 2000;
	StartLand(true);
	EXPECT_FALSE(draws.empty());
	EXPECT_EQ(GetState().lastPlayed.at(0), 1975u);
	// the last land: two things remembered, a remark made, the guidance disabled by a script
	EXPECT_EQ(TimeSinceThingSeen(77), 0u);
	EXPECT_EQ(TimeSinceThingSeen(78), 0u);
	SpiritSay(42, Type::AttackingTown);
	SetEnabled(false);
	const auto lastPlayed = GetState().lastPlayed;
	const float pitch = GetState().heartBeatPitch;
	// a land change: the turn starts again from 0, nothing is drawn and everything is kept
	draws.clear();
	s_Turn = 0;
	StartLand(false);
	EXPECT_TRUE(draws.empty());
	EXPECT_EQ(GetState().lastPlayed, lastPlayed);
	EXPECT_EQ(GetState().heartBeatPitch, pitch);
	EXPECT_FALSE(GetState().enabled);
	// the remembered things stay in the list, as from an older land
	EXPECT_EQ(RememberedThings().count, 2u);
	EXPECT_EQ(RememberedThings().olderLand, 2u);
	// a thing of the new land with a kept entry's id matches nothing: it is remembered anew
	EXPECT_EQ(TimeSinceThingSeen(77), 0u);
	EXPECT_EQ(RememberedThings().count, 3u);
	EXPECT_EQ(RememberedThings().olderLand, 2u);
	s_Turn = 100;
	EXPECT_EQ(TimeSinceThingSeen(77), 100u);
	EXPECT_EQ(RememberedThings().count, 3u);
	// the next new game starts it again
	StartLand(true);
	EXPECT_FALSE(draws.empty());
	EXPECT_EQ(RememberedThings().count, 0u);
	EXPECT_TRUE(GetState().enabled);
}

TEST(GuidanceMoon, PhaseFromTheRealClock)
{
	// days = time / 86400 - 10962 (2000-01-06, a new moon): phase 2 pi
	EXPECT_FLOAT_EQ(MoonPhase(int64_t {10962} * 86400 + 3600), 6.2831855f);
	// the original's arithmetic with the FPU at 24 bits, emulated:
	// each step rounds to a float, the two double constants do not (the old all-double model gave 3.3044245
	// for day 10976 and 5.0752940 for day 19000)
	EXPECT_EQ(MoonPhase(int64_t {10976} * 86400), 3.30442476272583f);
	EXPECT_EQ(MoonPhase(int64_t {10963} * 86400 + 3600), 6.0704169273376465f);
	EXPECT_EQ(MoonPhase(int64_t {12345} * 86400), 1.0506809949874878f);
	EXPECT_EQ(MoonPhase(int64_t {19000} * 86400), 5.075367450714111f);
	EXPECT_EQ(MoonPhase(int64_t {20000} * 86400), 5.934971809387207f);
	EXPECT_EQ(MoonPhase(int64_t {20363} * 86400), 4.098221302032471f);
}

TEST(GuidanceMoon, WaitIsTheCubeOfThePhaseFromFull)
{
	// (phase - pi)^3 * 12000, truncated: a moon before the full gives a wait below 1
	EXPECT_EQ(MoonWait(0.5f), 1500);
	EXPECT_EQ(MoonWait(2.0f), 96000);
	EXPECT_EQ(MoonWait(-1.0f), -12000);
	EXPECT_EQ(MoonWait(-0.25f), -187);
	EXPECT_EQ(MoonWait(0.1f), 12);
}

TEST(GuidanceHelpScript, RunMessagePushesBothTextsAndStartsTheScript)
{
	// RunMessage on a fake VM
	std::unique_ptr<help::DialogueText> helpDialogue;
	help::HelpSystem help({8, 5}, {}, {}, helpDialogue);
	ecs::systems::DialogueControlSystem dialogue;
	std::vector<float> pushed;
	std::string started;
	uint32_t mask = 0;
	std::vector<uint32_t> stopped;
	uint32_t ownerType = 1;
	help::script_control::Vm vm;
	vm.pushFloat = [&pushed](float v) { pushed.push_back(v); };
	vm.startScript = [&started, &mask](std::string_view name, uint32_t m) {
		started = std::string(name);
		mask = m;
	};
	vm.stopTasksOfType = [&stopped](uint32_t m) { stopped.push_back(m); };
	vm.taskType = [&ownerType](uint32_t) { return ownerType; };
	EXPECT_TRUE(help::script_control::RunMessage(help, dialogue, 4946, 4946, "MultiHelpJustTalkWithText", vm, 77));
	EXPECT_EQ(pushed, (std::vector<float> {4946.0f, 4946.0f}));
	EXPECT_EQ(started, "MultiHelpJustTalkWithText");
	EXPECT_EQ(mask, 0x7Fu);
	EXPECT_EQ(stopped, (std::vector<uint32_t> {0x4A}));
	EXPECT_EQ(help.GetMessageTurn(), 77u);
	// a Script task (type 1) holding the dialogue keeps it: nothing
	dialogue.Request(9, 0);
	pushed.clear();
	EXPECT_FALSE(help::script_control::RunMessage(help, dialogue, 1, 1, "x", vm, 78));
	EXPECT_TRUE(pushed.empty());
	// a Help task (type 2) holding it is stopped
	ownerType = 2;
	EXPECT_TRUE(help::script_control::RunMessage(help, dialogue, 1, 1, "x", vm, 79));
	// first > last: nothing
	EXPECT_FALSE(help::script_control::RunMessage(help, dialogue, 2, 1, "x", vm, 80));
	// the help switch and level: on 1 and level 3 by default; HELP_SYSTEM_ON wants both
	EXPECT_TRUE(help.IsHelpSystemOn());
	EXPECT_EQ(help.GetGuidanceLevel(), 3);
	help.SetHelpOn(0);
	EXPECT_FALSE(help.IsHelpSystemOn());
	EXPECT_EQ(help.GetGuidanceLevel(), 0);
	help.Reset();
	EXPECT_EQ(help.GetHelpOn(), 1u);
}
