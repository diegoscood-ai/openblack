/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand directing the fight of the player's creature (ECS/HandFight.h) and the packets its presses send
// (ECS/CreatureFightPackets.h), with a fake fight: what a press on a body or on the ground makes, which button replaces
// the queue, the arena's edge, the held time of a press, and each packet reaching the sending player's creature at the
// turn after the one that sent it

#define LOCATOR_IMPLEMENTATIONS

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/CreatureFightPackets.h"
#include "ECS/HandFight.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "Input/GamePackets.h"
#include "creature/CreatureSystemWorld.h"

using namespace openblack;
namespace hand_fight = openblack::ecs::hand_fight;
namespace packets = openblack::ecs::creature_fight_packets;
using openblack::ecs::systems::CreatureFightSystemInterface;

namespace
{
/// A fight that only keeps what the packets ask of it; the creature in `duelling` is in a duel
class RecordingFight final: public CreatureFightSystemInterface
{
public:
	struct Queued
	{
		entt::entity creature;
		creature_fight::Move move;
		bool replace;
	};
	struct Ground
	{
		entt::entity creature;
		glm::vec3 point;
		bool replace;
	};
	std::vector<Queued> queued;
	std::vector<Ground> ground;
	std::vector<std::pair<entt::entity, float>> charges;
	std::optional<entt::entity> duelling;

	void ProcessTurn() override {}
	void Update(float, float) override {}
	void AnimationAt(entt::entity, float, ecs::components::CreatureAnimationInputs&) const override {}
	StartResult StartFight(entt::entity, entt::entity) override { return StartResult::NoOpponent; }
	void AbortFight(entt::entity) override {}
	[[nodiscard]] bool IsFighting(entt::entity creature) const override { return duelling == creature; }
	[[nodiscard]] bool IsDuelling(entt::entity creature) const override { return duelling == creature; }
	[[nodiscard]] std::optional<entt::entity> OpponentOf(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] std::vector<ArenaView> GetArenas() const override { return {}; }
	[[nodiscard]] std::optional<ArenaView> ArenaOf(entt::entity) const override { return std::nullopt; }
	bool QueueMove(entt::entity creature, const creature_fight::Move& move, bool replace) override
	{
		queued.push_back({creature, move, replace});
		return true;
	}
	void ReleaseCharge(entt::entity creature, float heldMs) override { charges.emplace_back(creature, heldMs); }
	void GroundPress(entt::entity creature, const glm::vec3& point, bool replace) override
	{
		ground.push_back({creature, point, replace});
	}
	void SetAutoFighting(entt::entity, bool) override {}
	[[nodiscard]] bool IsAutoFighting(entt::entity) const override { return false; }
	void KnockOut(entt::entity) override {}
	void KillPermanently(entt::entity) override {}
	void Resurrect(entt::entity) override {}
	[[nodiscard]] bool IsKnockedOut(entt::entity) const override { return false; }
	[[nodiscard]] std::optional<creature_fight_hud::Values> GetPanel() const override { return std::nullopt; }
	void SetAngerStartsFights(bool) override {}
	[[nodiscard]] bool GetAngerStartsFights() const override { return false; }
	void SetCameraWatches(bool) override {}
	[[nodiscard]] bool GetCameraWatches() const override { return false; }
	[[nodiscard]] bool IsCameraOnFight() const override { return false; }
};

class HandFightPacketsTest: public ::testing::Test
{
protected:
	void SetUp() override { game_packets::Reset(); }
	void TearDown() override
	{
		game_packets::Reset();
		game_packets::ClearHandlers();
	}

	test::creature_world::World _world;
	RecordingFight _fight;
};
} // namespace

// The grab button's move goes in place of the queue, the action button's after it; each has its packets and the state
// its press on a body holds
TEST(HandFight, EachButtonHasItsPacketsAndItsHeldState)
{
	EXPECT_TRUE(hand_fight::ReplacesQueue(hand_fight::Button::Grab));
	EXPECT_FALSE(hand_fight::ReplacesQueue(hand_fight::Button::Action));
	EXPECT_EQ(hand_fight::BodyPacket(hand_fight::Button::Grab), game_packets::Type::FightMoveReplace);
	EXPECT_EQ(hand_fight::BodyPacket(hand_fight::Button::Action), game_packets::Type::FightMoveAdd);
	EXPECT_EQ(hand_fight::GroundPacket(hand_fight::Button::Grab), game_packets::Type::FightSelectPoint);
	EXPECT_EQ(hand_fight::GroundPacket(hand_fight::Button::Action), game_packets::Type::FightApplyPoint);
	EXPECT_EQ(static_cast<int>(game_packets::Type::FightSelectPoint), 0x0F);
	EXPECT_EQ(static_cast<int>(game_packets::Type::FightApplyPoint), 0x10);
	EXPECT_EQ(static_cast<int>(game_packets::Type::FightMoveAdd), 0x22);
	EXPECT_EQ(static_cast<int>(game_packets::Type::FightMoveReplace), 0x23);
	EXPECT_EQ(static_cast<int>(game_packets::Type::FightCharge), 0x24);
	EXPECT_EQ(hand_fight::HeldState(hand_fight::Button::Grab), 0x17);
	EXPECT_EQ(hand_fight::HeldState(hand_fight::Button::Action), 0x16);
	EXPECT_EQ(hand_fight::k_FightState, 0x10);
	EXPECT_EQ(hand_fight::k_MostWaiting, 12u);
	// the special move's gesture sends the scripts' number of the special move
	EXPECT_EQ(creature_fight::ScriptMove(static_cast<uint32_t>(hand_fight::k_SpecialMove))->kind,
	          creature_fight::Move::Kind::Special);
}

// On the player's own creature a press blocks; on its opponent it strikes high above two thirds of its height, low
// below a third, in the middle between, each bound in the middle; on any other creature it does nothing
TEST(HandFight, APressOnABodyBlocksOrStrikesAtTheBandItLandsOn)
{
	EXPECT_EQ(hand_fight::BodyMove(hand_fight::Body::Own, 0.9f), 3u);
	EXPECT_EQ(hand_fight::BodyMove(hand_fight::Body::Opponent, 0.9f), 0u);
	EXPECT_EQ(hand_fight::BodyMove(hand_fight::Body::Opponent, 0.66f), 1u);
	EXPECT_EQ(hand_fight::BodyMove(hand_fight::Body::Opponent, 0.5f), 1u);
	EXPECT_EQ(hand_fight::BodyMove(hand_fight::Body::Opponent, 0.33f), 1u);
	EXPECT_EQ(hand_fight::BodyMove(hand_fight::Body::Opponent, 0.32f), 2u);
	EXPECT_EQ(hand_fight::BodyMove(hand_fight::Body::Opponent, -0.1f), 2u);
	EXPECT_FALSE(hand_fight::BodyMove(hand_fight::Body::Other, 0.5f).has_value());
	// the moves as the scripts number them
	EXPECT_EQ(creature_fight::ScriptMove(3)->kind, creature_fight::Move::Kind::Block);
	EXPECT_EQ(creature_fight::ScriptMove(0)->kind, creature_fight::Move::Kind::High);
	EXPECT_EQ(creature_fight::ScriptMove(2)->kind, creature_fight::Move::Kind::Low);
}

// A point on the ground counts only strictly within the arena's radius of its middle
TEST(HandFight, TheGroundCountsOnlyInsideTheArena)
{
	EXPECT_TRUE(hand_fight::InsideArena({0.0f, 0.0f}, 5.0f, {3.0f, 3.99f}));
	EXPECT_FALSE(hand_fight::InsideArena({0.0f, 0.0f}, 5.0f, {3.0f, 4.0f}));
	EXPECT_FALSE(hand_fight::InsideArena({0.0f, 0.0f}, 5.0f, {-3.0f, -4.0f}));
	EXPECT_TRUE(hand_fight::InsideArena({2472.0f, 2788.0f}, 20.0f, {2472.5f, 2800.0f}));
	EXPECT_FALSE(hand_fight::InsideArena({2472.0f, 2788.0f}, 20.0f, {2492.0f, 2788.0f}));
	EXPECT_FALSE(hand_fight::InsideArena({0.0f, 0.0f}, 0.0f, {0.0f, 0.0f}));
}

// A press is held for the visual clock's milliseconds since it, at most a hundred for each turn since it and one more
TEST(HandFight, APressIsHeldAtMostAHundredMillisecondsForEachTurnAndOneMore)
{
	EXPECT_EQ(hand_fight::HeldMs(40, 0), 40);
	EXPECT_EQ(hand_fight::HeldMs(100, 0), 100);
	EXPECT_EQ(hand_fight::HeldMs(250, 0), 100);
	EXPECT_EQ(hand_fight::HeldMs(250, 1), 200);
	EXPECT_EQ(hand_fight::HeldMs(250, 2), 250);
	EXPECT_EQ(hand_fight::HeldMs(5000, 20), 2100);
	// the visual clock going back gives less than nothing, which the charge takes as nothing
	EXPECT_EQ(hand_fight::HeldMs(-30, 0), -30);
}

// A press on the ground reaches the player's creature at its altitude above the land, while it fights; otherwise it
// steps nowhere
TEST_F(HandFightPacketsTest, AGroundPressStepsThePlayersCreatureWhileItFights)
{
	const auto creature = test::creature_world::World::MakeCreature();
	game_packets::Packet packet {.type = game_packets::Type::FightSelectPoint};
	packet.coords = map_coords::FromMetres({110.0f, 95.0f});
	packet.coords.altitude = 0.0f;
	packets::ApplyGroundPress(&_fight, nullptr, creature, packet, true);
	EXPECT_TRUE(_fight.ground.empty());
	_fight.duelling = creature;
	packets::ApplyGroundPress(&_fight, nullptr, creature, packet, true);
	packets::ApplyGroundPress(&_fight, nullptr, creature, packet, false);
	ASSERT_EQ(_fight.ground.size(), 2u);
	EXPECT_EQ(_fight.ground[0].creature, creature);
	EXPECT_FLOAT_EQ(_fight.ground[0].point.x, map_coords::ToMetres(packet.coords.x));
	EXPECT_FLOAT_EQ(_fight.ground[0].point.z, map_coords::ToMetres(packet.coords.z));
	EXPECT_TRUE(_fight.ground[0].replace);
	EXPECT_FALSE(_fight.ground[1].replace);
	// no creature of the player's, or no fights
	packets::ApplyGroundPress(&_fight, nullptr, std::nullopt, packet, true);
	packets::ApplyGroundPress(nullptr, nullptr, creature, packet, true);
	EXPECT_EQ(_fight.ground.size(), 2u);
}

// A press on a body puts its move in the queue of the player's creature while the body pressed on is still there
TEST_F(HandFightPacketsTest, ABodyPressQueuesItsMoveForThePlayersCreature)
{
	const auto creature = test::creature_world::World::MakeCreature();
	const auto opponent = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_TWO);
	packets::ApplyBodyPress(&_fight, creature, {.type = game_packets::Type::FightMoveAdd, .object = opponent, .value = 2},
	                        false);
	packets::ApplyBodyPress(&_fight, creature, {.type = game_packets::Type::FightMoveReplace, .object = creature, .value = 3},
	                        true);
	ASSERT_EQ(_fight.queued.size(), 2u);
	EXPECT_EQ(_fight.queued[0].creature, creature);
	EXPECT_EQ(_fight.queued[0].move.kind, creature_fight::Move::Kind::Low);
	EXPECT_FALSE(_fight.queued[0].replace);
	EXPECT_EQ(_fight.queued[1].move.kind, creature_fight::Move::Kind::Block);
	EXPECT_TRUE(_fight.queued[1].replace);
	// the body gone, or no creature of the player's
	packets::ApplyBodyPress(&_fight, creature, {.type = game_packets::Type::FightMoveAdd, .object = entt::null, .value = 1},
	                        false);
	packets::ApplyBodyPress(&_fight, std::nullopt, {.type = game_packets::Type::FightMoveAdd, .object = opponent, .value = 1},
	                        false);
	EXPECT_EQ(_fight.queued.size(), 2u);
}

// A fight miracle's gesture queues the miracle after the queued moves of the creature the packet names, while it is there
TEST_F(HandFightPacketsTest, AFightMiraclesPacketQueuesTheMiracleForItsCreature)
{
	EXPECT_EQ(static_cast<int>(game_packets::Type::FightSpell), 0x5A);
	const auto creature = test::creature_world::World::MakeCreature();
	packets::ApplyFightSpell(
	    &_fight,
	    {.type = game_packets::Type::FightSpell, .object = creature, .value = static_cast<int32_t>(MagicType::LightningBolt)});
	ASSERT_EQ(_fight.queued.size(), 1u);
	EXPECT_EQ(_fight.queued[0].creature, creature);
	EXPECT_EQ(_fight.queued[0].move, (creature_fight::Move {.kind = creature_fight::Move::Kind::Spell,
	                                                        .value = static_cast<uint32_t>(MagicType::LightningBolt)}));
	EXPECT_FALSE(_fight.queued[0].replace);
	// the creature gone
	packets::ApplyFightSpell(&_fight, {.type = game_packets::Type::FightSpell, .object = entt::null, .value = 4});
	packets::ApplyFightSpell(nullptr, {.type = game_packets::Type::FightSpell, .object = creature, .value = 4});
	EXPECT_EQ(_fight.queued.size(), 1u);
}

// The let go of a press charges the blow of the player's creature by how long it was held
TEST_F(HandFightPacketsTest, TheLetGoChargesThePlayersCreaturesBlow)
{
	const auto creature = test::creature_world::World::MakeCreature();
	packets::ApplyCharge(&_fight, creature, {.type = game_packets::Type::FightCharge, .value = 340});
	ASSERT_EQ(_fight.charges.size(), 1u);
	EXPECT_EQ(_fight.charges[0].first, creature);
	EXPECT_FLOAT_EQ(_fight.charges[0].second, 340.0f);
	packets::ApplyCharge(&_fight, std::nullopt, {.type = game_packets::Type::FightCharge, .value = 340});
	EXPECT_EQ(_fight.charges.size(), 1u);
}

// The press's packet waits for the turn after the one that sent it
TEST_F(HandFightPacketsTest, APressWaitsForTheNextTurn)
{
	static RecordingFight* fight = nullptr;
	static std::optional<entt::entity> player;
	fight = &_fight;
	player = test::creature_world::World::MakeCreature();
	game_packets::SetHandler(game_packets::Type::FightMoveReplace,
	                         [](const game_packets::Packet& packet) { packets::ApplyBodyPress(fight, player, packet, true); });
	game_packets::Push({.type = game_packets::Type::FightMoveReplace, .object = *player, .value = 3});
	game_packets::DispatchQueuedPackets();
	EXPECT_TRUE(_fight.queued.empty());
	game_packets::Flush();
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(_fight.queued.size(), 1u);
	fight = nullptr;
	player.reset();
}
