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
