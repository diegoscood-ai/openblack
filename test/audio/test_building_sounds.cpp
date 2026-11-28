/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <chrono>
#include <limits>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Systems/AbodeSounds.h"

// The sounds buildings start by themselves: which sample, from which bank, when.
// Pure rules, without the game's data.

using namespace openblack;
using openblack::ecs::systems::AbodeSounds;

namespace
{
/// The sound tags: each thing's tag is kept with everything it was made with; everything else does nothing
class FakeSoundTags final: public ecs::systems::SoundTagSystemInterface
{
public:
	struct ThingTag
	{
		entt::entity thing;
		glm::vec3 offset;
		int sample;
		bool track;
		int mode;
		int loops;
		bool extra3DFlag;
		bool is3D;
		audio::SfxBank bank;
		int delay;
	};
	std::vector<ThingTag> tags;

	void ProcessTurn() override {}
	void SetActive(TagId, bool) override {}
	TagId CreatePointSound(const glm::vec3&, int, int, int, bool, bool, audio::SfxBank, int) override { return 0; }
	TagId CreatePointSound(const glm::vec3&, int, int, int, bool, bool, audio::AtmosType, int) override { return 0; }
	TagId Create(entt::entity thing, const glm::vec3& offset, int sample, bool track, int mode, int loops, bool extra3DFlag,
	             bool is3D, audio::SfxBank bank, int delay) override
	{
		tags.push_back({thing, offset, sample, track, mode, loops, extra3DFlag, is3D, bank, delay});
		return static_cast<TagId>(tags.size());
	}
	TagId CreateAtMarker(const glm::vec3&, int, int, int, bool, audio::SfxBank) override { return 0; }
	void Remove(entt::entity, int, audio::SfxBank, bool) override {}
	void Delete(TagId) override {}
	[[nodiscard]] bool Exists(TagId) const override { return false; }
	void Clear() override {}
	[[nodiscard]] std::optional<glm::vec3> TagSoundPoint(TagId) const override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> Point(TagId) const override { return std::nullopt; }
};
} // namespace

TEST(AbodeSounds, OnlyAnAvailableWindmillHums)
{
	EXPECT_TRUE(AbodeSounds::HasWindmillHum(AbodeType::Windmill, false));
	EXPECT_FALSE(AbodeSounds::HasWindmillHum(AbodeType::Windmill, true));
	EXPECT_FALSE(AbodeSounds::HasWindmillHum(AbodeType::LivingQuarters, false));
	EXPECT_FALSE(AbodeSounds::HasWindmillHum(AbodeType::Workshop, false));
	EXPECT_EQ(AbodeSounds::k_WindmillSample, 13);
	EXPECT_EQ(AbodeSounds::k_WindmillMode, 2);
	EXPECT_EQ(AbodeSounds::k_WindmillLoops, -1);
	EXPECT_FALSE(AbodeSounds::k_WindmillTrack);
	EXPECT_TRUE(AbodeSounds::k_WindmillIs3D);
	EXPECT_EQ(AbodeSounds::k_WindmillBank, audio::SfxBank::InGame);
}

TEST(AbodeSounds, TheWindmillsHumIsATagOnIt)
{
	// as the windmill's creation makes it: (windmill, 13 G_windmill, track 0, mode 2, loops -1, 0, 3D, InGame, delay 0)
	FakeSoundTags sounds;
	const auto windmill = static_cast<entt::entity>(7);
	EXPECT_EQ(AbodeSounds::StartWindmillHum(sounds, windmill), 1u);
	ASSERT_EQ(sounds.tags.size(), 1u);
	const auto& tag = sounds.tags.front();
	EXPECT_EQ(tag.thing, windmill);
	EXPECT_EQ(tag.offset, glm::vec3(0.0f));
	EXPECT_EQ(tag.sample, 13);
	EXPECT_FALSE(tag.track);
	EXPECT_EQ(tag.mode, 2);
	EXPECT_EQ(tag.loops, -1);
	EXPECT_FALSE(tag.extra3DFlag);
	EXPECT_TRUE(tag.is3D);
	EXPECT_EQ(tag.bank, audio::SfxBank::InGame);
	EXPECT_EQ(tag.delay, 0);
}

TEST(AbodeSounds, MostBuildingsSoundTheLifeAroundThem)
{
	for (const auto type :
	     {AbodeType::General, AbodeType::LivingQuarters, AbodeType::Civic, AbodeType::Windmill, AbodeType::StoragePit,
	      AbodeType::Creche, AbodeType::Workshop, AbodeType::Wonder, AbodeType::Graveyard, AbodeType::TownCentre})
	{
		EXPECT_TRUE(AbodeSounds::HasAmbience(type)) << static_cast<uint32_t>(type);
	}
	for (const auto type :
	     {AbodeType::Totem, AbodeType::FootballPitch, AbodeType::SpellDispenser, AbodeType::Field, AbodeType::Citadel})
	{
		EXPECT_FALSE(AbodeSounds::HasAmbience(type)) << static_cast<uint32_t>(type);
	}
}

TEST(AbodeSounds, AboutOnceInEightySecondsOfGameTime)
{
	using Milliseconds = std::chrono::duration<float, std::milli>;
	EXPECT_EQ(AbodeSounds::AmbienceChance(Milliseconds(33.0f)), 2424);
	EXPECT_EQ(AbodeSounds::AmbienceChance(Milliseconds(16.0f)), 5000);
	EXPECT_EQ(AbodeSounds::AmbienceChance(Milliseconds(7.0f)), 11428);
	EXPECT_EQ(AbodeSounds::AmbienceChance(Milliseconds(80000.0f)), 1);
	// While paused the roll is over 2^31, so it all but never comes up 1
	EXPECT_EQ(AbodeSounds::AmbienceChance(Milliseconds(0.0f)), std::numeric_limits<int32_t>::min());
}

TEST(AbodeSounds, OnlyWithinTwoHundredOfTheCamera)
{
	const glm::vec3 abode {100.0f, 20.0f, 200.0f};
	EXPECT_FLOAT_EQ(AbodeSounds::AmbienceDistance(abode, {103.0f, 24.0f, 212.0f}), 13.0f);
	EXPECT_FLOAT_EQ(AbodeSounds::AmbienceDistance(abode, {100.0f, 20.0f, 0.0f}), 200.0f);
	EXPECT_FALSE(AbodeSounds::IsWithinAmbienceReach(200.0f));
	EXPECT_TRUE(AbodeSounds::IsWithinAmbienceReach(199.99f));
	EXPECT_EQ(AbodeSounds::k_AmbienceBank, audio::SfxBank::Editor);
	EXPECT_TRUE(AbodeSounds::k_AmbienceTrack);
}

TEST(AbodeSounds, TheTimeOfDayPicksTheSound)
{
	using Keys = std::array<int32_t, 5>;
	// by day, below sky type 0.7
	EXPECT_EQ(AbodeSounds::AmbienceKeys(0.0f, 14.0f).ToArray(), (Keys {0, 0, 19, 0, 71}));
	EXPECT_EQ(AbodeSounds::AmbienceKeys(0.69f, 6.0f).ToArray(), (Keys {0, 0, 19, 0, 71}));
	// in the twilight, from 0.7 to below 1.3: dawn before noon, dusk from noon
	EXPECT_EQ(AbodeSounds::AmbienceKeys(0.7f, 6.0f).ToArray(), (Keys {0, 0, 19, 0, 73}));
	EXPECT_EQ(AbodeSounds::AmbienceKeys(1.0f, 11.99f).ToArray(), (Keys {0, 0, 19, 0, 73}));
	EXPECT_EQ(AbodeSounds::AmbienceKeys(1.0f, 12.0f).ToArray(), (Keys {0, 0, 19, 0, 72}));
	EXPECT_EQ(AbodeSounds::AmbienceKeys(1.29f, 18.0f).ToArray(), (Keys {0, 0, 19, 0, 72}));
	// at night, from 1.3
	EXPECT_EQ(AbodeSounds::AmbienceKeys(1.3f, 18.0f).ToArray(), (Keys {0, 0, 19, 0, 74}));
	EXPECT_EQ(AbodeSounds::AmbienceKeys(2.0f, 2.0f).ToArray(), (Keys {0, 0, 19, 0, 74}));
}

TEST(AbodeSounds, ADrawnBuildingPlaysOnlyWhenItsRollIsOneAndTheCameraIsNear)
{
	using Keys = std::array<int32_t, 5>;
	// the roll and the reach both decide; then the sound of the time of day, cockerels at dawn
	EXPECT_FALSE(AbodeSounds::Ambience(false, 50.0f, 1.0f, 6.0f).has_value());
	EXPECT_FALSE(AbodeSounds::Ambience(true, 200.0f, 1.0f, 6.0f).has_value());
	const auto dawn = AbodeSounds::Ambience(true, 199.0f, 1.0f, 6.0f);
	ASSERT_TRUE(dawn.has_value());
	EXPECT_EQ(dawn->ToArray(), (Keys {0, 0, 19, 0, 73}));
	// at night the key is the night's, which the bank leaves silent
	EXPECT_EQ(AbodeSounds::Ambience(true, 10.0f, 2.0f, 2.0f)->ToArray(), (Keys {0, 0, 19, 0, 74}));
}
