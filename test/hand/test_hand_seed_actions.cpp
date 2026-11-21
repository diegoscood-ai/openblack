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
#include "ECS/Components/Creature.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Unavailable.h"
#include "Input/GamePackets.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/MagicTables.h"
#include "creature/CreatureSystemFakes.h"
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

// The press and the apply it sends read one in-influence flag, measured at the hand's point in that pass. The hand's
// status still holds the last pass's flag (out of the influence, as a demo's press whose point was cleared leaves it):
// a press now in the influence casts, with no failure sample and no failure spot
TEST_F(HandSeedActions, APressInTheInfluenceCastsThoughTheLastPassWasOutOfIt)
{
	const test::RestoreService<Locator::audio> keepAudio;
	auto& sounds = static_cast<test::RecordingAudio&>(Locator::audio::emplace<test::RecordingAudio>());
	// a seed of the lightning bolt, which may be cast only on land, over land
	auto& seedRow = Info().spellSeed.at(static_cast<size_t>(SpellSeedType::Fire));
	seedRow.magicTypes.fill(MagicType::LightningBolt);
	auto& bolt = const_cast<GMagicInfo&>(magic::GetMagicInfo(Info(), MagicType::LightningBolt));
	bolt.magicType = MagicType::LightningBolt;
	bolt.castRuleType = CastRuleType::OnLand;
	auto& world = static_cast<test::InertMagicSystem&>(Locator::magicSystem::value()).FakeWorld();
	world.influence = true;
	const auto seed = HoldSeed(SpellSeedType::Fire, SpellCastType::SpellCastInHand);
	magic::gestures::HandStatus lastPass;
	lastPass.inInfluence = false;
	magic::gestures::SetHandStatus(std::move(lastPass));
	game_packets::Reset();
	const auto result = Hand().PressSeedAction();
	EXPECT_TRUE(result == HandResult::Cast || result == HandResult::CastHeld);
	EXPECT_TRUE(sounds.played.empty());
	EXPECT_GT(game_packets::Pending(), 0u);
	EXPECT_EQ(Hand().GetHeldObject(), std::optional(seed));
	game_packets::Reset();
}

// Out of the influence at the hand's point, the press does nothing at all, whatever the last pass told the status
TEST_F(HandSeedActions, APressOutOfTheInfluenceDoesNothingThoughTheLastPassWasInIt)
{
	const test::RestoreService<Locator::audio> keepAudio;
	auto& sounds = static_cast<test::RecordingAudio&>(Locator::audio::emplace<test::RecordingAudio>());
	static_cast<test::InertMagicSystem&>(Locator::magicSystem::value()).FakeWorld().influence = false;
	const auto seed = HoldSeed(SpellSeedType::Fire, SpellCastType::SpellCastInHand);
	game_packets::Reset();
	EXPECT_EQ(Hand().PressSeedAction(), HandResult::None);
	EXPECT_TRUE(sounds.played.empty());
	EXPECT_EQ(game_packets::Pending(), 0u);
	EXPECT_EQ(Hand().GetHeldObject(), std::optional(seed));
	game_packets::Reset();
}

namespace
{
/// A creature of the player's own, which a thing in the hand can be given to
entt::entity PlayersCreature()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto creature = registry.Create();
	registry.Assign<ecs::components::Transform>(creature, glm::vec3(12.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::Creature>(
	    creature, ecs::components::Creature {.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Tiger});
	return creature;
}

/// A one-shot miracle's orb of fire, put in the hand
entt::entity HoldOneShotOrb(ecs::systems::HandSystem& hand)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto orb = registry.Create();
	registry.Assign<ecs::components::Transform>(orb, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::OneOffSpellSeed>(orb).seedType = SpellSeedType::Fire;
	hand.PlaceObjectInMagicHand(orb);
	return orb;
}
} // namespace

// The hand gives a held one-shot orb to the player's creature as it gives anything else: the orb is no spell seed, the
// one thing the give refuses whatever the creature, so a creature that could pick it up off the map takes it. The give
// is the Give packet to that creature with the orb, and the hand keeps hold of it until the creature takes it; a
// creature that could not pick it up is given nothing
TEST_F(HandSeedActions, AOneShotOrbInTheHandIsGivenToThePlayersCreatureThatCanPickItUp)
{
	const test::RestoreService<Locator::creatureObjectActionSystem> keepHands;
	auto& hands = static_cast<test::creature_fakes::FakeObjectAction&>(
	    Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>());
	const auto creature = PlayersCreature();
	const auto orb = HoldOneShotOrb(Hand());
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(orb));
	game_packets::Reset();
	hands.canPickUpIgnoringMap = false;
	EXPECT_FALSE(Hand().GiveHeldTo(creature));
	EXPECT_EQ(game_packets::Pending(), 0u);
	hands.canPickUpIgnoringMap = true;
	EXPECT_TRUE(Hand().GiveHeldTo(creature));
	EXPECT_EQ(Hand().GetHeldObject(), std::optional(orb));
	ASSERT_TRUE(game_packets::IsPending(game_packets::Type::Give, PlayerNames::PLAYER_ONE));
	std::vector<game_packets::Packet> gives;
	game_packets::SetHandler(game_packets::Type::Give,
	                         [&gives](const game_packets::Packet& packet) { gives.push_back(packet); });
	game_packets::Flush();
	game_packets::DispatchQueuedPackets();
	ASSERT_EQ(gives.size(), 1u);
	EXPECT_EQ(gives.front().object, creature);
	EXPECT_EQ(gives.front().value, static_cast<int32_t>(entt::to_integral(orb)));
	game_packets::Reset();
	game_packets::ClearHandlers();
}

// A spell seed in the hand is never given, even to the player's creature that could pick it up
TEST_F(HandSeedActions, ASpellSeedInTheHandIsNeverGivenToThePlayersCreature)
{
	const test::RestoreService<Locator::creatureObjectActionSystem> keepHands;
	auto& hands = static_cast<test::creature_fakes::FakeObjectAction&>(
	    Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>());
	hands.canPickUpIgnoringMap = true;
	const auto creature = PlayersCreature();
	const auto seed = HoldSeed(SpellSeedType::Fire, SpellCastType::SpellCastHandGesture);
	game_packets::Reset();
	EXPECT_FALSE(Hand().GiveHeldTo(creature));
	EXPECT_EQ(game_packets::Pending(), 0u);
	EXPECT_EQ(Hand().GetHeldObject(), std::optional(seed));
}
