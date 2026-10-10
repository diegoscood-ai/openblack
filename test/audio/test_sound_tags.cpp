/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <map>
#include <optional>
#include <string>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/Lightning.h"
#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "Audio/Device/SampleOutput.h"
#include "Audio/Device/Sound.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/SoundMap.h"
#include "Audio/Services/SoundTags.h"
#include "ECS/Systems/Implementations/SoundTagSystem.h"
#include "ECS/Systems/Implementations/WeatherSystem.h"
#include "ECS/Weather/WeatherState.h"
#include "GameClock.h"
#include "Locator.h"
#include "support/RestoreService.h"

// Sound tags on the 16 channels. A tag of a thing replays through PlaySoundEffect
// every turn (mode 2: only when silent); a thing that goes releases a playing loop and the tag waits for its end; a
// point tag plays once and goes when it stops; a delayed one waits for the sound at 347 a second (CheckDelay); the
// street lantern's tag sounds at the lantern's top within the sample's max distance. A fake output stands for QMixer.

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
	std::array<std::string, 16> names {};
	int plays {0};

	bool Play(size_t channel, Sound& sound, const Start& start) override
	{
		playing[channel] = true;
		released[channel] = false;
		starts[channel] = start;
		names[channel] = sound.name;
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
	/// the index of the channel that plays, -1 for none (one at a time in these tests)
	[[nodiscard]] int Single() const
	{
		int found = -1;
		for (size_t i = 0; i < playing.size(); ++i)
		{
			if (playing[i])
			{
				found = found == -1 ? static_cast<int>(i) : -2;
			}
		}
		return found;
	}
};

constexpr auto k_Thing = static_cast<entt::entity>(42);

class SoundTagTest: public ::testing::Test
{
protected:
	FakeOutput output;
	std::map<entt::id_type, Sound> sounds;
	/// the things' positions (GameQueries::thingPosition); a missing one is gone
	static inline std::map<uint32_t, glm::vec3> s_Things;
	static inline glm::vec3 s_Camera {0.0f};
	BankId inGame {k_NoBank};
	/// The system under test, injected through its slot (the audio's ClearMap and the functions of audio::tags reach it
	/// there); the tests call it on its own reference
	const test::RestoreService<Locator::soundTagSystem> restoreTags;
	ecs::systems::SoundTagSystemInterface* soundTags {nullptr};

	/// The things' positions and the camera, from the statics above
	static GameQueries MakeQueries()
	{
		GameQueries queries;
		queries.thingPosition = [](ThingId thing) -> std::optional<glm::vec3> {
			const auto found = s_Things.find(thing);
			return found != s_Things.end() ? std::optional<glm::vec3>(found->second) : std::nullopt;
		};
		queries.camera = []() -> std::optional<CameraState> {
			CameraState camera;
			camera.position = s_Camera;
			return camera;
		};
		return queries;
	}

	void SetUp() override
	{
		s_Things.clear();
		s_Camera = glm::vec3(0.0f);
		audio::Init(MakeQueries());
		inGame = RegisterBank("audio/sfx/game/InGame.sad", "InGame.sad");
		InstallBackend();
		soundTags = &Locator::soundTagSystem::emplace<ecs::systems::SoundTagSystem>();
		audio::ClearMap();
		output = FakeOutput {};
	}
	/// The fake output and sounds as the channels' backend (audio::Init puts in the game's)
	void InstallBackend()
	{
		sample_play::Backend backend;
		backend.output = &output;
		backend.sound = [this](entt::id_type id) -> Sound* {
			const auto found = sounds.find(id);
			return found != sounds.end() ? &found->second : nullptr;
		};
		backend.rand = []() { return 16383; };
		backend.camera = []() -> std::optional<glm::vec3> { return s_Camera; };
		backend.ownerPosition = [](const Owner& owner) { return OwnerSoundPosition(owner); };
		sample_play::SetBackend(std::move(backend));
		sample_play::SetMainVolume(127);
	}
	void TearDown() override
	{
		audio::ClearMap();
		sample_play::SetMainVolume(127); // back to its default (QMixer's maximum)
		sample_play::SetBackend({});
		audio::Shutdown();
	}

	/// An InGame.sad sample with the defaults of the options (no .sad override) and a max distance
	void Add(int number, float maxDistance)
	{
		Sound sound;
		sound.name = fmt::format("InGame {}", number);
		sound.id = number;
		sound.bank = inGame;
		sound.priority = 100;
		sound.sampleRate = 22050;
		sound.pitch = 100;
		sound.pitchDeviation = 0;
		sound.maxDistance = maxDistance;
		sounds.emplace(SampleId(inGame, number), std::move(sound));
	}
	/// Sample 7 of the rain's ambient bank, registered as the atmos mixer does
	void AddRain7()
	{
		const auto rain = RegisterBank("audio/sfx/atmos/rain.sad", "rain.sad");
		Sound sound;
		sound.name = "rain 7";
		sound.id = 7;
		sound.bank = rain;
		sound.priority = 100;
		sound.sampleRate = 22050;
		sound.pitch = 100;
		sound.maxDistance = 500.0f;
		sounds.emplace(SampleId(rain, 7), std::move(sound));
	}
	/// The channel that plays the sound of that name, -1 for none
	[[nodiscard]] int ChannelOf(const std::string& name) const
	{
		for (size_t i = 0; i < output.playing.size(); ++i)
		{
			if (output.playing[i] && output.names[i] == name)
			{
				return static_cast<int>(i);
			}
		}
		return -1;
	}
};
} // namespace

TEST_F(SoundTagTest, ThingTagMode2ReplaysOnlyWhenSilent)
{
	Add(13, 100.0f);
	s_Things[static_cast<uint32_t>(k_Thing)] = glm::vec3(10.0f, 0.0f, 0.0f);
	// the windmill's creation: (windmill, 13 G_windmill, 0, 2, -1, 0, 1, InGame, 0)
	const auto tag = soundTags->Create(k_Thing, glm::vec3(0.0f), 13, false, 2, -1, false, true, SfxBank::InGame, 0);
	EXPECT_EQ(output.plays, 0); // a tag of a thing plays from the next ProcessTurn
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 1);
	const int channel = output.Single();
	ASSERT_GE(channel, 0);
	EXPECT_TRUE(output.starts[static_cast<size_t>(channel)].is3D);
	EXPECT_EQ(output.starts[static_cast<size_t>(channel)].position, glm::vec3(10.0f, 0.0f, 0.0f));
	EXPECT_EQ(output.starts[static_cast<size_t>(channel)].loops, -1);
	soundTags->ProcessTurn();
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 1); // mode 2: nothing while it plays
	output.playing[static_cast<size_t>(channel)] = false;
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 2); // silent again: it replays
	EXPECT_TRUE(soundTags->Exists(tag));
}

TEST_F(SoundTagTest, ThingTagMode3RestartsEveryTurn)
{
	Add(10, 100.0f);
	s_Things[static_cast<uint32_t>(k_Thing)] = glm::vec3(1.0f);
	soundTags->Create(k_Thing, glm::vec3(0.0f), 10, false, 3, 0, false, true, SfxBank::InGame, 0);
	soundTags->ProcessTurn();
	soundTags->ProcessTurn();
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 3);
	EXPECT_EQ(output.Single(), 0); // the same channel, restarted (mode 3: same bank, owner and sample)
}

TEST_F(SoundTagTest, OutOfRangeAndInactive)
{
	Add(13, 50.0f);
	s_Things[static_cast<uint32_t>(k_Thing)] = glm::vec3(60.0f, 0.0f, 0.0f);
	const auto tag = soundTags->Create(k_Thing, glm::vec3(0.0f), 13, false, 2, -1, false, true, SfxBank::InGame, 0);
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 0); // farther than the sample's max distance from the camera
	s_Camera = glm::vec3(20.0f, 0.0f, 0.0f);
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 1);
	// an active tag turned off stops at once and does not replay
	soundTags->SetActive(tag, false);
	EXPECT_EQ(output.Single(), -1);
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 1);
	soundTags->SetActive(tag, true);
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 2);
}

TEST_F(SoundTagTest, GoneThingReleasesItsLoop)
{
	Add(74, 100.0f);
	s_Things[static_cast<uint32_t>(k_Thing)] = glm::vec3(5.0f, 0.0f, 0.0f);
	// the workshop's update: (workshop, 74 G_Workshop, 0, 2, -1, 0, 1, InGame, 0)
	const auto tag = soundTags->Create(k_Thing, glm::vec3(0.0f), 74, false, 2, -1, false, true, SfxBank::InGame, 0);
	soundTags->ProcessTurn();
	const int channel = output.Single();
	ASSERT_GE(channel, 0);
	s_Things.clear();
	// not functional -> ToBeDeleted -> a dead object's tag: looping and playing -> released
	soundTags->ProcessTurn();
	EXPECT_TRUE(output.released[static_cast<size_t>(channel)]);
	EXPECT_TRUE(output.playing[static_cast<size_t>(channel)]);
	EXPECT_TRUE(soundTags->Exists(tag));
	soundTags->ProcessTurn();
	EXPECT_TRUE(soundTags->Exists(tag)); // still playing
	EXPECT_EQ(output.plays, 1);          // and not replayed
	output.playing[static_cast<size_t>(channel)] = false;
	soundTags->ProcessTurn();
	EXPECT_FALSE(soundTags->Exists(tag));
}

TEST_F(SoundTagTest, GoneThingOneShotGoesAtOnce)
{
	Add(11, 100.0f);
	s_Things[static_cast<uint32_t>(k_Thing)] = glm::vec3(5.0f, 0.0f, 0.0f);
	// the totem statue's worship percentage: (totem, 11 G_TotumMove, 0, 2, 0, 0, 1, InGame, 0)
	const auto tag = soundTags->Create(k_Thing, glm::vec3(0.0f), 11, false, 2, 0, false, true, SfxBank::InGame, 0);
	soundTags->ProcessTurn();
	const int channel = output.Single();
	ASSERT_GE(channel, 0);
	s_Things.clear();
	soundTags->ProcessTurn();
	EXPECT_FALSE(soundTags->Exists(tag));
	EXPECT_FALSE(output.released[static_cast<size_t>(channel)]);
	EXPECT_TRUE(output.playing[static_cast<size_t>(channel)]); // its sample plays on
}

TEST_F(SoundTagTest, PointTagPlaysOnceAndGoes)
{
	Add(31, 100.0f);
	// a falling tree's physics turn: a point tag (coords, 31 G_TreeFall_01, 0, 3, 0, 0, 1, InGame, 0)
	const auto tag = soundTags->CreatePointSound(glm::vec3(3.0f, 0.0f, 4.0f), 31, 3, 0, false, true, SfxBank::InGame, 0);
	EXPECT_EQ(output.plays, 1); // at once
	const int channel = output.Single();
	ASSERT_GE(channel, 0);
	EXPECT_EQ(output.starts[static_cast<size_t>(channel)].position, glm::vec3(3.0f, 0.0f, 4.0f));
	soundTags->ProcessTurn();
	EXPECT_TRUE(soundTags->Exists(tag));
	EXPECT_EQ(output.plays, 1); // a point tag never replays
	output.playing[static_cast<size_t>(channel)] = false;
	soundTags->ProcessTurn();
	EXPECT_FALSE(soundTags->Exists(tag));
}

TEST_F(SoundTagTest, MapCoordsTagIsTheMapPoint)
{
	Add(10, 100.0f);
	// a tag at map coords: x and z x 10 / 65536, y = the land's altitude (0 without a land)
	// + the altitude above the land
	const map_coords::MapCoords coords {3 * 0x10000 + 0x8000, 0x10000 + 0x4000, 2.0f};
	tags::CreateAtMapCoords(coords, 10, false, 3, 0, false, true, SfxBank::InGame, 0);
	const int channel = output.Single();
	ASSERT_GE(channel, 0);
	EXPECT_EQ(output.starts[static_cast<size_t>(channel)].position, glm::vec3(35.0f, 2.0f, 12.5f));
	// the same map point already in metres (magic::ToMap's): not quantised again
	output = FakeOutput {};
	tags::CreateAtMapCoords(map_coords::ToMetres(coords.x), map_coords::ToMetres(coords.z), 2.0f, 10, false, 3, 0, false, true,
	                        SfxBank::InGame, 0);
	const int second = output.Single();
	ASSERT_GE(second, 0);
	EXPECT_EQ(output.starts[static_cast<size_t>(second)].position, glm::vec3(35.0f, 2.0f, 12.5f));
}

TEST_F(SoundTagTest, DelayedPointTagWaitsForTheSound)
{
	Add(20, 500.0f);
	// 100 away: 347 * turns * 0.1 >= 100 from the 3rd turn (34.7, 69.4, 104.1)
	const auto tag = soundTags->CreatePointSound(glm::vec3(100.0f, 0.0f, 0.0f), 20, 2, 0, false, true, SfxBank::InGame, 1);
	EXPECT_EQ(output.plays, 0);
	soundTags->ProcessTurn();
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 0);
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 1);
	EXPECT_TRUE(soundTags->Exists(tag));
	// a 2D tag keeps no delay: at once
	soundTags->CreatePointSound(glm::vec3(100.0f, 0.0f, 0.0f), 20, 1, 0, false, false, SfxBank::InGame, 1);
	EXPECT_EQ(output.plays, 2);
}

TEST_F(SoundTagTest, AmbientPointSoundWaitsAndPlaysFromItsBank)
{
	// sample 7 of the rain's ambient bank, and a sample 7 of InGame.sad that must not be the one played
	AddRain7();
	Add(7, 500.0f);
	// the thunder: (point, sample, mode 2, no loops, 3D, the rain's bank, delay 1), 100 away: heard from the 3rd turn
	const auto tag = soundTags->CreatePointSound(glm::vec3(100.0f, 0.0f, 0.0f), 7, 2, 0, false, true, AtmosType::Rain, 1);
	EXPECT_EQ(output.plays, 0);
	soundTags->ProcessTurn();
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 0);
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 1);
	const int channel = output.Single();
	ASSERT_GE(channel, 0);
	EXPECT_EQ(output.names[static_cast<size_t>(channel)], "rain 7");
	EXPECT_EQ(output.starts[static_cast<size_t>(channel)].position, glm::vec3(100.0f, 0.0f, 0.0f));
	// a point's tag: it goes once its sample stops
	soundTags->ProcessTurn();
	EXPECT_TRUE(soundTags->Exists(tag));
	output.playing.fill(false);
	soundTags->ProcessTurn();
	EXPECT_FALSE(soundTags->Exists(tag));
	// without a delay it plays at once
	soundTags->CreatePointSound(glm::vec3(0.0f, 0.0f, 10.0f), 7, 2, 0, false, true, AtmosType::Rain, 0);
	EXPECT_EQ(output.plays, 2);
	const int second = output.Single();
	ASSERT_GE(second, 0);
	EXPECT_EQ(output.names[static_cast<size_t>(second)], "rain 7");
}

TEST_F(SoundTagTest, ThunderIsAClapOfTheRainBankPickedFromTheTicks)
{
	// clap 7 is the one of the ticks 5, 16, 27...: 2 + ticks % 11, no random number
	EXPECT_EQ(lightning::ThunderClap(0), 2);
	EXPECT_EQ(lightning::ThunderClap(10), 12);
	EXPECT_EQ(lightning::ThunderClap(16), 7);
	// the tick count is unsigned: past 2^31 it still gives a clap in 2..12
	EXPECT_EQ(lightning::ThunderClap(0xFFFFFFFFu), 5);
	AddRain7();
	Add(7, 500.0f);
	// a 3D tag of the rain's bank, 100 away: it waits for the sound to reach the camera, heard from the 3rd turn
	const auto tag = tags::Thunder(*soundTags, glm::vec3(100.0f, 0.0f, 0.0f), 16);
	EXPECT_EQ(output.plays, 0);
	soundTags->ProcessTurn();
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 0);
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 1);
	const int channel = output.Single();
	ASSERT_GE(channel, 0);
	EXPECT_EQ(output.names[static_cast<size_t>(channel)], "rain 7");
	EXPECT_EQ(output.starts[static_cast<size_t>(channel)].position, glm::vec3(100.0f, 0.0f, 0.0f));
	EXPECT_EQ(output.starts[static_cast<size_t>(channel)].loops, 0);
	EXPECT_TRUE(soundTags->Exists(tag));
}

namespace
{
uint32_t g_ThunderTicks = 0;
uint32_t ThunderTicks()
{
	return g_ThunderTicks;
}
} // namespace

TEST_F(SoundTagTest, TheAudiosStartSetsTheStormsThunder)
{
	AddRain7();
	// a weather of this test's own, before the audio starts again
	const test::RestoreService<Locator::weatherSystem> restoreWeather;
	auto& weather = Locator::weatherSystem::emplace<ecs::systems::WeatherSystem>();
	EXPECT_FALSE(weather.GetState().sheetCallback);
	audio::Init(MakeQueries());
	InstallBackend();
	ASSERT_TRUE(weather.GetState().sheetCallback);
	EXPECT_FALSE(weather.GetState().forkCallback);
	// a bright flash at the camera: its clap, picked from the clock's ticks, is heard at the next turn
	g_ThunderTicks = 27;
	game_clock::SetTickSource(&ThunderTicks);
	weather.GetState().sheetCallback(weather::storms::Storm {}, glm::vec3(0.0f), 300.0f);
	game_clock::SetTickSource(nullptr);
	EXPECT_EQ(output.plays, 0);
	soundTags->ProcessTurn();
	ASSERT_EQ(output.plays, 1);
	EXPECT_GE(ChannelOf("rain 7"), 0);
	// the audio's end takes the thunder away
	audio::Shutdown();
	EXPECT_FALSE(weather.GetState().sheetCallback);
	audio::Init(MakeQueries());
	InstallBackend();
}

TEST_F(SoundTagTest, AmbientPointSoundStopsInItsBank)
{
	AddRain7();
	Add(7, 500.0f);
	// the same sample number in InGame.sad and in the rain's bank, each a point's tag that plays at once
	soundTags->CreatePointSound(glm::vec3(0.0f, 0.0f, 20.0f), 7, 2, 0, false, true, SfxBank::InGame, 0);
	const auto ambient = soundTags->CreatePointSound(glm::vec3(0.0f, 0.0f, 10.0f), 7, 2, 0, false, true, AtmosType::Rain, 0);
	EXPECT_EQ(output.plays, 2);
	const int inGameChannel = ChannelOf("InGame 7");
	const int rainChannel = ChannelOf("rain 7");
	ASSERT_GE(inGameChannel, 0);
	ASSERT_GE(rainChannel, 0);
	// switched off: its sample stops in the rain's bank, and the InGame one plays on
	soundTags->SetActive(ambient, false);
	EXPECT_FALSE(output.playing[static_cast<size_t>(rainChannel)]);
	EXPECT_TRUE(output.playing[static_cast<size_t>(inGameChannel)]);
	// a new map: every tag's sample stopped in its own bank, and every tag forgotten
	const auto again = soundTags->CreatePointSound(glm::vec3(0.0f, 0.0f, 10.0f), 7, 2, 0, false, true, AtmosType::Rain, 0);
	ASSERT_GE(ChannelOf("rain 7"), 0);
	soundTags->Clear();
	EXPECT_EQ(ChannelOf("rain 7"), -1);
	EXPECT_EQ(ChannelOf("InGame 7"), -1);
	EXPECT_FALSE(soundTags->Exists(ambient));
	EXPECT_FALSE(soundTags->Exists(again));
}

TEST_F(SoundTagTest, RemoveStopsOrReleases)
{
	Add(166, 100.0f);
	s_Things[static_cast<uint32_t>(k_Thing)] = glm::vec3(1.0f);
	// the creature's creed power: (creature, 166 G_Creed_01, 0, 2, -1, ...), then Remove
	auto tag = soundTags->Create(k_Thing, glm::vec3(0.0f), 166, false, 2, -1, false, true, SfxBank::InGame, 0);
	soundTags->ProcessTurn();
	int channel = output.Single();
	ASSERT_GE(channel, 0);
	soundTags->Remove(k_Thing, 166, SfxBank::Editor, false); // another bank type: not that tag
	EXPECT_TRUE(soundTags->Exists(tag));
	soundTags->Remove(k_Thing, 166, SfxBank::InGame, false);
	EXPECT_TRUE(output.released[static_cast<size_t>(channel)]); // ToBeDeleted, the loop released
	EXPECT_TRUE(soundTags->Exists(tag));                        // as a dead object's tag until it ends
	output.playing.fill(false);
	soundTags->ProcessTurn();
	EXPECT_FALSE(soundTags->Exists(tag));

	tag = soundTags->Create(k_Thing, glm::vec3(0.0f), 166, false, 2, -1, false, true, SfxBank::InGame, 0);
	soundTags->ProcessTurn();
	channel = output.Single();
	ASSERT_GE(channel, 0);
	soundTags->Remove(k_Thing, 166, SfxBank::InGame, true); // with stop: stopped, then gone
	EXPECT_FALSE(output.playing[static_cast<size_t>(channel)]);
	EXPECT_FALSE(soundTags->Exists(tag));
}

TEST_F(SoundTagTest, StreetLanternAtItsTop)
{
	// InGame 147 G_Lantern_01: max distance 5
	Add(147, 5.0f);
	s_Things[static_cast<uint32_t>(k_Thing)] = glm::vec3(100.0f, 10.0f, 100.0f);
	// the street lantern's creation: a tag at (0, h, 0) of sample 0x93 (0, 2, -1, 0, 1, InGame, 0), then SetActive, off
	// by day
	const auto tag =
	    soundTags->Create(k_Thing, glm::vec3(0.0f, 3.0f, 0.0f), 147, false, 2, -1, false, true, SfxBank::InGame, 0);
	soundTags->SetActive(tag, false);
	s_Camera = glm::vec3(100.0f, 17.0f, 100.0f); // 4 above the top
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 0); // daylight: inactive
	soundTags->SetActive(tag, true);
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 1);
	const int channel = output.Single();
	ASSERT_GE(channel, 0);
	EXPECT_EQ(output.starts[static_cast<size_t>(channel)].position, glm::vec3(100.0f, 13.0f, 100.0f));
	EXPECT_EQ(output.starts[static_cast<size_t>(channel)].loops, -1);
	// the camera going away does not stop it (an untracked channel), and mode 2 leaves it alone
	s_Camera = glm::vec3(0.0f);
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 1);
	EXPECT_TRUE(output.playing[static_cast<size_t>(channel)]);
	// nor does it start again while the camera stays farther than 5 from the top
	output.playing.fill(false);
	s_Camera = glm::vec3(100.0f, 19.0f, 100.0f);
	soundTags->ProcessTurn();
	EXPECT_EQ(output.plays, 1);
}

TEST_F(SoundTagTest, RandomSample)
{
	// LocalRand(0) is 0
	EXPECT_EQ(tags::RandomSample(180, 0), 180);
}
