/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "ECS/HandSeedUnlock.h"
#include "Input/GamePackets.h"

using namespace openblack;

// The single-player packet path: nothing before the flush, the order kept, the flush by itself at 6 packets
TEST(GamePackets, AppliedAfterTheFlushInOrder)
{
	game_packets::Reset();
	static std::vector<int32_t> seen;
	seen.clear();
	game_packets::SetHandler(game_packets::Type::Tap, [](const game_packets::Packet& p) { seen.push_back(p.value); });
	game_packets::Push({game_packets::Type::Tap, entt::null, {}, 1});
	game_packets::Push({game_packets::Type::Tap, entt::null, {}, 2});
	game_packets::DispatchQueuedPackets();
	EXPECT_TRUE(seen.empty()); // not flushed yet
	game_packets::Flush();
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(seen, (std::vector<int32_t> {1, 2}));
	EXPECT_EQ(game_packets::Pending(), 0U);
}

TEST(GamePackets, FlushesByItselfAtSix)
{
	game_packets::Reset();
	static int count = 0;
	count = 0;
	game_packets::SetHandler(game_packets::Type::Tap, [](const game_packets::Packet&) { ++count; });
	for (int i = 0; i < 6; ++i)
	{
		game_packets::Push({game_packets::Type::Tap});
	}
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(count, 6);
}

// A packet a handler sends waits for the next flush: it is not applied in the turn that sent it
TEST(GamePackets, HandlerPushWaitsForTheNextFlush)
{
	game_packets::Reset();
	static std::vector<int32_t> seen;
	seen.clear();
	game_packets::SetHandler(game_packets::Type::Tap, [](const game_packets::Packet& p) {
		seen.push_back(p.value);
		if (p.value == 1)
		{
			game_packets::Push({game_packets::Type::Tap, entt::null, {}, 2});
		}
	});
	game_packets::Push({game_packets::Type::Tap, entt::null, {}, 1});
	game_packets::Flush();
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(seen, (std::vector<int32_t> {1}));
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(seen, (std::vector<int32_t> {1}));
	game_packets::Flush();
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(seen, (std::vector<int32_t> {1, 2}));
	game_packets::ClearHandlers();
}

// An object that is gone (here: no registry at all) reaches the handler as NULL, not dropped
TEST(GamePackets, GoneObjectReachesTheHandlerAsNull)
{
	game_packets::Reset();
	static int calls = 0;
	static bool gotNull = false;
	calls = 0;
	gotNull = false;
	game_packets::SetHandler(game_packets::Type::PlaceInHand, [](const game_packets::Packet& p) {
		++calls;
		gotNull = p.object == entt::null;
	});
	game_packets::Push({game_packets::Type::PlaceInHand, static_cast<entt::entity>(7)});
	game_packets::Flush();
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(calls, 1);
	EXPECT_TRUE(gotNull);
	game_packets::ClearHandlers();
}

// The creature hand's two packets carry the type bytes of the game's own
TEST(GamePackets, TheCreatureHandsTypes)
{
	EXPECT_EQ(static_cast<int>(game_packets::Type::CreatureFeedback), 0x59);
	EXPECT_EQ(static_cast<int>(game_packets::Type::CreatureLeashClick), 0x5F);
}

// The leash picked at the temple's posts: the game's type byte, the leash and the sending player through the list
TEST(GamePackets, TheLeashPickedAtTheTempleCarriesItsLeashAndPlayer)
{
	EXPECT_EQ(static_cast<int>(game_packets::Type::LeashType), 0x65);
	game_packets::Packet got {};
	game_packets::SetHandler(game_packets::Type::LeashType, [&got](const game_packets::Packet& p) { got = p; });
	game_packets::Push({.type = game_packets::Type::LeashType,
	                    .value = static_cast<int32_t>(LeashType::Good),
	                    .player = PlayerNames::PLAYER_TWO});
	game_packets::Flush();
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(got.type, game_packets::Type::LeashType);
	EXPECT_EQ(got.value, static_cast<int32_t>(LeashType::Good));
	EXPECT_EQ(got.player, PlayerNames::PLAYER_TWO);
	// a packet that names no player is the local one's
	EXPECT_EQ(game_packets::Packet {}.player, PlayerNames::PLAYER_ONE);
	game_packets::ClearHandlers();
}

// A locked apply's unlock is a packet of the game's own type, with the turns the apply was held
TEST(GamePackets, TheUnlockCarriesTheTurnsHeld)
{
	EXPECT_EQ(static_cast<int>(game_packets::Type::ApplyUnlock), 0x1A);
	const auto packet = ecs::hand_seed::UnlockPacket(entt::null, 290, 311);
	EXPECT_EQ(packet.type, game_packets::Type::ApplyUnlock);
	EXPECT_TRUE(packet.object == entt::null);
	EXPECT_EQ(packet.value, 21);
}

// A click's apply and its unlock, sent in the same frame, are handled in that order: the apply casts before the
// unlock closes the spell
TEST(GamePackets, AnUnlockSentWithItsApplyComesAfterIt)
{
	game_packets::Reset();
	static std::vector<game_packets::Type> seen;
	seen.clear();
	const auto record = [](const game_packets::Packet& p) { seen.push_back(p.type); };
	game_packets::SetHandler(game_packets::Type::ApplyToMapCoord, record);
	game_packets::SetHandler(game_packets::Type::ApplyUnlock, record);
	game_packets::Push({game_packets::Type::ApplyToMapCoord});
	game_packets::Push(ecs::hand_seed::UnlockPacket(entt::null, 290, 290));
	game_packets::Flush();
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(seen, (std::vector<game_packets::Type> {game_packets::Type::ApplyToMapCoord, game_packets::Type::ApplyUnlock}));
	game_packets::ClearHandlers();
}
