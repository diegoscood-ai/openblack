/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Audio/Game/EndOfTurnAudio.h"
#include "Audio/SoundMap.h"

using openblack::audio::sound_map::StratosphereVolume;

TEST(SoundMap, StratosphereVolumeRisesAndFalls)
{
	// the height above the land: none below the atmosphere, full at its top, none again in space
	EXPECT_EQ(StratosphereVolume(50.0f, 100.0f, 200.0f, 400.0f), 0.0f);
	EXPECT_EQ(StratosphereVolume(150.0f, 100.0f, 200.0f, 400.0f), 0.5f);
	EXPECT_EQ(StratosphereVolume(200.0f, 100.0f, 200.0f, 400.0f), 1.0f);
	EXPECT_EQ(StratosphereVolume(300.0f, 100.0f, 200.0f, 400.0f), 0.5f);
	EXPECT_EQ(StratosphereVolume(500.0f, 100.0f, 200.0f, 400.0f), 0.0f);
}

// ---- the audio's end of a turn ---------------------------------------------------------------------------------------

TEST(EndOfTurnAudio, SoundMapAndTagsRunEveryTurn)
{
	namespace end_of_turn = openblack::audio::end_of_turn;
	for (const bool paused : {false, true})
	{
		for (const uint32_t turn : {0u, 5u, 6u, 0xFFFFFFFFu})
		{
			const auto steps = end_of_turn::For(paused, turn);
			EXPECT_TRUE(steps.soundMap);
			EXPECT_TRUE(steps.soundTags);
		}
	}
}

TEST(EndOfTurnAudio, AudioTurnFromTheSixthUnpausedTurn)
{
	namespace end_of_turn = openblack::audio::end_of_turn;
	// turn > 5, compared unsigned
	EXPECT_EQ(end_of_turn::For(false, 0).atmos, end_of_turn::Atmos::Silence);
	EXPECT_EQ(end_of_turn::For(false, 5).atmos, end_of_turn::Atmos::Silence);
	EXPECT_EQ(end_of_turn::For(false, 6).atmos, end_of_turn::Atmos::Process);
	EXPECT_EQ(end_of_turn::For(false, 0xFFFFFFFFu).atmos, end_of_turn::Atmos::Process);
}

TEST(EndOfTurnAudio, APausedTurnSilencesTheAmbience)
{
	namespace end_of_turn = openblack::audio::end_of_turn;
	EXPECT_EQ(end_of_turn::For(true, 6).atmos, end_of_turn::Atmos::Silence);
	EXPECT_EQ(end_of_turn::For(true, 1000).atmos, end_of_turn::Atmos::Silence);
}
