/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <string>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Audio/ScriptSoundEffect.h"
#include "Audio/Services/Confirmation.h"
#include "Audio/Services/ScriptAudioState.h"
#include "CHLApi.h"
#include "Creature/CreatureAudio.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
constexpr auto k_Any = static_cast<uint16_t>(SoundEffectUse::Any);
constexpr auto k_QuietInCutscenes = static_cast<uint16_t>(SoundEffectUse::QuietInCutscenes);
constexpr auto k_HeardInTemple = static_cast<uint16_t>(SoundEffectUse::HeardInTemple);
constexpr auto k_QuietInFights = static_cast<uint16_t>(SoundEffectUse::QuietInFights);
} // namespace

TEST(ScriptSoundEffect, AnythingIsHeardInTheOpen)
{
	const SoundEffectConditions open {};
	for (const auto use : {k_Any, k_QuietInCutscenes, k_HeardInTemple, k_QuietInFights})
	{
		EXPECT_TRUE(SoundEffectHeard(open, ScriptSoundBank::InGame, use));
	}
}

TEST(ScriptSoundEffect, AScriptsCutsceneQuietensOnlyItsSamples)
{
	const SoundEffectConditions cutscene {.scriptWideScreen = true};
	EXPECT_FALSE(SoundEffectHeard(cutscene, ScriptSoundBank::Guidance, k_QuietInCutscenes));
	EXPECT_TRUE(SoundEffectHeard(cutscene, ScriptSoundBank::ScriptSfx, k_Any));
	EXPECT_TRUE(SoundEffectHeard(cutscene, ScriptSoundBank::InGame, k_HeardInTemple));
}

TEST(ScriptSoundEffect, InsideTheTempleOnlyTheTemplesSoundsAreHeard)
{
	const SoundEffectConditions temple {.insideTemple = true};
	EXPECT_TRUE(SoundEffectHeard(temple, ScriptSoundBank::InGame, k_HeardInTemple));
	EXPECT_FALSE(SoundEffectHeard(temple, ScriptSoundBank::InGame, k_Any));
	EXPECT_FALSE(SoundEffectHeard(temple, ScriptSoundBank::HelpSprites, k_Any));
	EXPECT_FALSE(SoundEffectHeard(temple, ScriptSoundBank::InGame, k_QuietInFights));
}

TEST(ScriptSoundEffect, WithTheGameSoundOffOnlySpeechIsHeard)
{
	const SoundEffectConditions off {.gameSoundOn = false};
	EXPECT_TRUE(SoundEffectHeard(off, ScriptSoundBank::HelpSprites, k_Any));
	EXPECT_TRUE(SoundEffectHeard(off, ScriptSoundBank::Villagers, k_Any));
	EXPECT_FALSE(SoundEffectHeard(off, ScriptSoundBank::ScriptSfx, k_Any));
	EXPECT_FALSE(SoundEffectHeard(off, ScriptSoundBank::Guidance, k_Any));
	EXPECT_FALSE(SoundEffectHeard(off, ScriptSoundBank::InGame, k_HeardInTemple));
}

TEST(ScriptSoundEffect, FightControlsQuietenTheirSamples)
{
	const SoundEffectConditions fight {.creatureFightControl = true};
	EXPECT_FALSE(SoundEffectHeard(fight, ScriptSoundBank::InGame, k_QuietInFights));
	EXPECT_TRUE(SoundEffectHeard(fight, ScriptSoundBank::InGame, k_Any));
}

TEST(ScriptSoundEffect, OnlyTheWholeUserParameterCounts)
{
	// The user parameter is compared whole, not as bits: 3 or 6 is neither the cutscene's nor the fight's
	const SoundEffectConditions both {.scriptWideScreen = true, .creatureFightControl = true};
	EXPECT_TRUE(SoundEffectHeard(both, ScriptSoundBank::InGame, 3));
	EXPECT_TRUE(SoundEffectHeard(both, ScriptSoundBank::InGame, 6));
	EXPECT_FALSE(SoundEffectHeard({.insideTemple = true}, ScriptSoundBank::InGame, 6));
}

TEST(ScriptSoundEffect, NativesTakeWhatTheGameDoes)
{
	chlapi::CHLApi api;
	const auto& table = api.GetFunctionsTable();
	ASSERT_GT(table.size(), 357u);
	EXPECT_EQ(table[43].name, std::string("PLAY_SOUND_EFFECT"));
	EXPECT_EQ(table[43].stackIn, 6);
	EXPECT_EQ(table[43].stackOut, 0u);
	EXPECT_EQ(table[357].name, std::string("SET_GAME_SOUND"));
	EXPECT_EQ(table[357].stackIn, 1);
	EXPECT_EQ(table[357].stackOut, 0u);
	EXPECT_EQ(table[317].name, std::string("SET_CREATURE_SOUND"));
	EXPECT_EQ(table[317].stackIn, 1);
	EXPECT_EQ(table[317].stackOut, 0u);
}

namespace
{
/// Calls the native at `index` of the table after pushing `value`
void CallWithInt(chlapi::CHLApi& api, size_t index, int32_t value)
{
	if (!spdlog::get("scripting"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
	}
	const auto& table = api.GetFunctionsTable();
	ASSERT_LT(index, table.size());
	ASSERT_EQ(table[index].stackIn, 1);
	Locator::vm::value().Pushi(value);
	table[index].impl();
}
} // namespace

TEST(ScriptSoundEffect, TheCreatureSoundSettingKeepsTheScriptsValueAsItIs)
{
	openblack::test::RestoreService<Locator::vm> restoreVm;
	Locator::vm::emplace<lhvm::LHVM>();
	chlapi::CHLApi api;
	auto& state = GetScriptAudioState();
	const auto before = state.creatureSound.load();
	// Off: only the local player's creature is heard
	CallWithInt(api, 317, 0);
	EXPECT_EQ(state.creatureSound.load(), 0);
	EXPECT_FALSE(creature_audio::OtherVoicesEnabled(state.creatureSound.load()));
	// Any other value is kept as the script gave it, and lets every creature be heard
	CallWithInt(api, 317, 7);
	EXPECT_EQ(state.creatureSound.load(), 7);
	EXPECT_TRUE(creature_audio::OtherVoicesEnabled(state.creatureSound.load()));
	CallWithInt(api, 317, 1);
	EXPECT_EQ(state.creatureSound.load(), 1);
	state.creatureSound = before;
}

TEST(ScriptSoundEffect, TheTwoAngleSoundsWatchTheTurnAndTheTilt)
{
	openblack::test::RestoreService<Locator::vm> restoreVm;
	Locator::vm::emplace<lhvm::LHVM>();
	chlapi::CHLApi api;
	const auto& table = api.GetFunctionsTable();
	ASSERT_GT(table.size(), 348u);
	// Both are named START_ANGLE_SOUND
	EXPECT_EQ(table[285].name, std::string("START_ANGLE_SOUND"));
	EXPECT_EQ(table[348].name, std::string("START_ANGLE_SOUND"));
	const auto& watched = confirmation::Get();
	// The second: the tilt, from 0, within 5
	confirmation::FeedPitch(1.0f, 0.5f);
	CallWithInt(api, 348, 1);
	EXPECT_TRUE(watched.active);
	EXPECT_EQ(watched.range, 5.0f);
	EXPECT_EQ(confirmation::Pitch(), 0.0f);
	ASSERT_NE(watched.value, nullptr);
	confirmation::FeedPitch(1.0f, 0.5f);
	confirmation::FeedAngle(3.0f, 0.5f);
	EXPECT_EQ(*watched.value, confirmation::Pitch());
	CallWithInt(api, 348, 0);
	EXPECT_FALSE(watched.active);
	// The first: the turn, within 7
	CallWithInt(api, 285, 1);
	EXPECT_TRUE(watched.active);
	EXPECT_EQ(watched.range, 7.0f);
	EXPECT_EQ(*watched.value, confirmation::Angle());
	CallWithInt(api, 285, 0);
	EXPECT_FALSE(watched.active);
}
