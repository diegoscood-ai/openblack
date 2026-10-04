/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// DeleteObjectAndTakeResource of the storage pit (0x733750) and the worship site (0x77E7B0): the return values, the
// storage pit's REACTION 22 (0x7337B4..0x7337BA) and the object deleted (0x63A940). No game data: no entities map, so
// CreateReaction does not spread.

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/TakeResource.h"
#include "GameClock.h"
#include "Locator.h"
#include "Worship/WorshipSite.h"

using namespace openblack;
namespace reactions = openblack::ecs::effects::reactions;

namespace
{
class TakeResourceTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		reactions::Clear();
		game_clock::SetTurn(1234);
	}
	void TearDown() override
	{
		reactions::Clear();
		Locator::entitiesRegistry::reset();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
};
} // namespace

TEST_F(TakeResourceTest, StoragePitReactsWithThePlayerOfTheInterface)
{
	const auto store = Reg().Create();
	const auto object = Reg().Create();
	const ecs::pot_resource::Dropper hand {true, PlayerNames::PLAYER_TWO, true};
	// 0x7337C5: 1
	EXPECT_TRUE(ecs::take_resource::StoragePit(store, object, hand));
	// CreateReaction(this, 0x16, is->GetPlayer(), 1)
	const auto id = reactions::GetReactionOfTypeInitiatedBy(store, Reaction::ReactToHandPuttingStuffInStoragePit);
	ASSERT_NE(id, 0u);
	const auto* reaction = reactions::Find(id);
	ASSERT_NE(reaction, nullptr);
	EXPECT_EQ(static_cast<int>(reaction->type), 0x16);
	EXPECT_EQ(reaction->player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(reaction->turnCreated, 1234u); // the last argument 1: stamped
	EXPECT_EQ(reactions::All().size(), 1u);
	// DoDeleteObjectAndTakeResource 0x63A940: the object goes (ToBeDeleted 0x63AAB1)
	EXPECT_FALSE(Reg().Valid(object));
}

TEST_F(TakeResourceTest, StoragePitWithoutInterfaceIsNeutral)
{
	const auto store = Reg().Create();
	const auto object = Reg().Create();
	// is NULL -> GPlayer NULL (0x73375C)
	EXPECT_TRUE(ecs::take_resource::StoragePit(store, object, {}));
	const auto id = reactions::GetReactionOfTypeInitiatedBy(store, Reaction::ReactToHandPuttingStuffInStoragePit);
	ASSERT_NE(id, 0u);
	EXPECT_EQ(reactions::Find(id)->player, PlayerNames::NEUTRAL);
}

TEST_F(TakeResourceTest, WorshipSiteTakesWithoutReaction)
{
	const auto site = Reg().Create();
	const auto object = Reg().Create();
	const ecs::pot_resource::Dropper hand {true, PlayerNames::PLAYER_ONE, true};
	// 0x77E7FF: 1, and no CreateReaction
	EXPECT_TRUE(worship::site::DeleteObjectAndTakeResource(site, object, hand));
	EXPECT_TRUE(reactions::All().empty());
	EXPECT_FALSE(Reg().Valid(object));
}
