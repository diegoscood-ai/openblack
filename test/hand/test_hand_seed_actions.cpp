/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand with a spell seed in it, through the hand's own presses and releases (support/HandSeedHarness.h)

#define LOCATOR_IMPLEMENTATIONS

#include <optional>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/Device/Sound.h"
#include "ECS/Components/Unavailable.h"
#include "Input/GamePackets.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/MagicTables.h"
#include "support/HandSeedHarness.h"
#include "support/RecordingAudio.h"

using namespace openblack;
using openblack::test::hand_seed::k_GestureLoop;
using HandResult = openblack::ecs::systems::MagicSystemInterface::HandResult;

namespace
{
using HandSeedActions = openblack::test::hand_seed::HandSeedHarness;
} // namespace

// A gesture seed is armed by the press and waits for the release; a second press before it is no press, as the button
// never came up
TEST_F(HandSeedActions, ASecondPressWhileAGestureSeedIsArmedIsIgnored)
{
	const auto seed = HoldSeed(SpellSeedType::Storm, SpellCastType::SpellCastHandGesture);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(seed));
	EXPECT_EQ(Hand().PressSeedAction(), HandResult::Readied);
	EXPECT_EQ(Hand().PressSeedAction(), HandResult::None);
	EXPECT_EQ(Tags().made.size(), 1u); // armed once
	EXPECT_EQ(Hand().GetHeldObject(), std::optional(seed));
}

// While armed the hand loops the gesture sample at itself, 3D, in the game's bank; the release ends the loop, which
// plays out its pass
TEST_F(HandSeedActions, AnArmedGestureSeedHumsAtTheHandUntilTheReleaseLetsTheLoopPlayOut)
{
	HoldSeed(SpellSeedType::Storm, SpellCastType::SpellCastHandGesture);
	ASSERT_EQ(Hand().PressSeedAction(), HandResult::Readied);
	ASSERT_EQ(Tags().made.size(), 1u);
	const auto& hum = Tags().made.front();
	EXPECT_EQ(hum.sample, k_GestureLoop);
	EXPECT_FALSE(hum.track);
	EXPECT_EQ(hum.mode, 2);
	EXPECT_EQ(hum.loops, -1);
	EXPECT_TRUE(hum.is3D);
	EXPECT_EQ(hum.bank, audio::SfxBank::InGame);
	EXPECT_TRUE(Tags().removed.empty());
	static_cast<void>(Hand().ReleaseSeedAction());
	ASSERT_EQ(Tags().removed.size(), 1u);
	EXPECT_EQ(Tags().removed.front().sample, k_GestureLoop);
	EXPECT_EQ(Tags().removed.front().bank, audio::SfxBank::InGame);
	EXPECT_FALSE(Tags().removed.front().stop);
}

// The hum is the interface's: it sounds from the newest gesture sample's land point, not from the hand, and an empty
// buffer gives it no point
TEST_F(HandSeedActions, AnArmedSeedHumsFromTheNewestGestureSamplesLandPoint)
{
	HoldSeed(SpellSeedType::Storm, SpellCastType::SpellCastHandGesture);
	ASSERT_EQ(Hand().PressSeedAction(), HandResult::Readied);
	ASSERT_EQ(Tags().sources.size(), 1u);
	const auto& point = Tags().sources.front();
	ASSERT_TRUE(point);
	auto& buffer = magic::gestures::State().system;
	buffer.Clear();
	EXPECT_EQ(point(), std::nullopt);
	buffer.AddSample(glm::vec3(1.0f, 2.0f, 3.0f), {100, 100});
	buffer.AddSample(glm::vec3(4.0f, 5.0f, 6.0f), {140, 100});
	EXPECT_EQ(point(), std::optional(glm::vec3(4.0f, 5.0f, 6.0f)));
	buffer.Clear();
}

// A seed that leaves the hand while armed (gone, or dropped) ends the loop the same way at the next step
TEST_F(HandSeedActions, AnArmedSeedThatLeavesTheHandLetsTheLoopPlayOut)
{
	const auto seed = HoldSeed(SpellSeedType::Storm, SpellCastType::SpellCastHandGesture);
	ASSERT_EQ(Hand().PressSeedAction(), HandResult::Readied);
	Registry().Assign<ecs::components::Unavailable>(seed);
	EXPECT_EQ(Hand().ReleaseSeedAction(), HandResult::None);
	ASSERT_EQ(Tags().removed.size(), 1u);
	EXPECT_EQ(Tags().removed.front().sample, k_GestureLoop);
	EXPECT_FALSE(Tags().removed.front().stop);
}

// A miracle that can't be cast where the hand is fails: the cast failure sample plays at once, in two dimensions and
// untracked, the failure's spot visual is sent for the next turn's start, and the seed stays in the hand
TEST_F(HandSeedActions, AMiracleThatCantBeCastThereFailsWithItsSampleAndItsSpot)
{
	const test::RestoreService<Locator::audio> keepAudio;
	auto& sounds = static_cast<test::RecordingAudio&>(Locator::audio::emplace<test::RecordingAudio>());
	// a seed of the lightning bolt, which may be cast only on land, over the sea
	auto& seedRow = Info().spellSeed.at(static_cast<size_t>(SpellSeedType::Fire));
	seedRow.magicTypes.fill(MagicType::LightningBolt);
	auto& bolt = const_cast<GMagicInfo&>(magic::GetMagicInfo(Info(), MagicType::LightningBolt));
	bolt.magicType = MagicType::LightningBolt;
	bolt.castRuleType = CastRuleType::OnLand;
	static_cast<test::InertMagicSystem&>(Locator::magicSystem::value()).FakeWorld().wetBelowX = 1000.0f;
	const auto seed = HoldSeed(SpellSeedType::Fire, SpellCastType::SpellCastInHand);
	game_packets::Reset();
	EXPECT_EQ(Hand().PressSeedAction(), HandResult::CantCastThere);
	ASSERT_EQ(sounds.played.size(), 1u);
	const auto& played = sounds.played.front();
	EXPECT_EQ(played.sound, static_cast<entt::id_type>(audio::SoundId::G_SpellCastFailure));
	EXPECT_FALSE(played.is3D);
	EXPECT_FALSE(played.track);
	EXPECT_EQ(played.mode, 3);
	EXPECT_EQ(played.loops, 0);
	EXPECT_EQ(game_packets::Pending(), 1u);
	EXPECT_EQ(Hand().GetHeldObject(), std::optional(seed));
	game_packets::Reset();
}
