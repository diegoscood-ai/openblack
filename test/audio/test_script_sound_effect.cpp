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

#include <gtest/gtest.h>

#include "Audio/ScriptSoundEffect.h"
#include "CHLApi.h"

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
