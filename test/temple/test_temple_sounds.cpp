/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <bit>

#include <gtest/gtest.h>

#include "3D/TempleChallengePick.h"
#include "3D/TempleSounds.h"
#include "Audio/Device/Sound.h"

using namespace openblack;

TEST(TempleSounds, TheHeartsSparksInTurnAtTheHeartOwnedByIt)
{
	const auto heart = audio::Owner::Thing(static_cast<entt::entity>(9));
	const glm::vec3 place {1000.0f, 12.5f, 2000.0f};
	// The original's spark: the InGame bank's sample 206 + the counter, 3D, not following its owner, at the heart's
	// place (its land point plus its height above the land), owned by the heart, with the default mode, loops and volume
	const auto first = temple_sounds::HeartSpark(0, place, heart);
	EXPECT_EQ(first.sound, entt::hashed_string("InGame.sad/206").value());
	EXPECT_TRUE(first.is3D);
	EXPECT_FALSE(first.track);
	EXPECT_EQ(first.owner, heart);
	EXPECT_EQ(first.position, place);
	EXPECT_EQ(first.mode, 3);
	EXPECT_EQ(first.loops, 0);
	EXPECT_EQ(first.volume, 127);
	EXPECT_EQ(first.pitch, 100);
	EXPECT_EQ(temple_sounds::HeartSpark(4, place, heart).sound, entt::hashed_string("InGame.sad/210").value());
}

TEST(TempleChallengePick, TheCameraTurnsFirstThenTheChallengeIsChosen)
{
	using temple_challenge_pick::Click;
	// The original chooses at a blend of exactly 1 or an unordered one, and turns the camera otherwise
	EXPECT_EQ(temple_challenge_pick::ClickOn(1.0f), Click::Choose);
	EXPECT_EQ(temple_challenge_pick::ClickOn(std::bit_cast<float>(0x7FC00000U)), Click::Choose);
	EXPECT_EQ(temple_challenge_pick::ClickOn(0.5f), Click::TurnTo);
	EXPECT_EQ(temple_challenge_pick::ClickOn(0.0f), Click::TurnTo);
	EXPECT_EQ(temple_challenge_pick::ClickOn(std::bit_cast<float>(0x3F7FFFFFU)), Click::TurnTo);
}

TEST(TempleChallengePick, AChallengeWonAlreadyIsNotChosenAndMakesNoSound)
{
	// The original: success 1 or unordered, nothing; 0.5, 0 and just under 1 are chosen with a woosh
	EXPECT_FALSE(temple_challenge_pick::ChosenWoosh(1.0f, 0x12345677U).has_value());
	EXPECT_FALSE(temple_challenge_pick::ChosenWoosh(std::bit_cast<float>(0x7FC00000U), 0x12345677U).has_value());
	for (const float success : {0.5f, 0.0f, std::bit_cast<float>(0x3F7FFFFFU)})
	{
		const auto woosh = temple_challenge_pick::ChosenWoosh(success, 0x12345677U);
		ASSERT_TRUE(woosh.has_value());
		// 46 + (the clock & 3): the fourth woosh, 2D, no owner, mode 3, once
		EXPECT_EQ(woosh->sound, static_cast<entt::id_type>(audio::SoundId::G_Woosh_04));
		EXPECT_FALSE(woosh->is3D);
		EXPECT_EQ(woosh->owner, audio::Owner::None());
		EXPECT_EQ(woosh->mode, 3);
		EXPECT_EQ(woosh->loops, 0);
	}
	EXPECT_EQ(temple_challenge_pick::ChosenWoosh(0.5f, 0x12345678U)->sound,
	          static_cast<entt::id_type>(audio::SoundId::G_Woosh_01));
}
