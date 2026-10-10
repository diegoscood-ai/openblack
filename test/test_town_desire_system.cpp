/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The town desire system over the towns' own desires: a synthetic town in a registry of the test's own, the system
// made here, not reached through the locator.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/TownDesireSystem.h"
#include "ECS/Town/TownDesire.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
constexpr size_t D(TownDesireInfo d)
{
	return static_cast<size_t>(static_cast<int>(d));
}

class TownDesireSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		auto info = std::make_unique<InfoConstants>();
		for (auto& desire : info->townDesire)
		{
			desire.desireTriggersVillagerAction = 0.01f;
		}
		Locator::infoConstants::reset(info.release());
		auto& registry = Locator::entitiesRegistry::value();
		_town = registry.Create();
		registry.Assign<Town>(_town, 3u);
		auto& town = registry.Get<Town>(_town);
		town.stats.adults = 8;
		town.stats.children = 2;
		_desire = &town.desire;
	}

	ecs::systems::TownDesireSystem _system;
	entt::entity _town {entt::null};
	TownDesire* _desire {nullptr};

private:
	// put back in reverse order: the tables, then the registry
	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	const test::RestoreService<Locator::infoConstants> _restoreInfo;
};
} // namespace

TEST_F(TownDesireSystemTest, DesiresWithTheirBoosts)
{
	const auto food = D(TownDesireInfo::ForFood);
	_desire->desire.at(food) = 0.5f;
	_desire->raw.at(food) = 1.25f;
	_desire->boost.at(food) = 0.25f;
	_desire->boostA.at(food) = 0.125f;
	EXPECT_FLOAT_EQ(_system.GetDesire(_town, TownDesireInfo::ForFood), 0.875f);
	EXPECT_FLOAT_EQ(_system.GetRawDesire(_town, TownDesireInfo::ForFood), 1.625f);
	// no town, or no such desire: none
	EXPECT_FLOAT_EQ(_system.GetDesire(entt::null, TownDesireInfo::ForFood), 0.0f);
	EXPECT_FLOAT_EQ(_system.GetRawDesire(_town, TownDesireInfo::None), 0.0f);
	const auto other = Locator::entitiesRegistry::value().Create();
	EXPECT_FLOAT_EQ(_system.GetDesire(other, TownDesireInfo::ForFood), 0.0f);
}

TEST_F(TownDesireSystemTest, MostWantedIsTheFirstOfTheOrder)
{
	_desire->desire.at(D(TownDesireInfo::ForSleep)) = 0.75f;
	_desire->desire.at(D(TownDesireInfo::ForAbodes)) = 0.25f;
	ecs::town_desire::SortDesires(*_desire);
	EXPECT_EQ(_system.GetMostWanted(_town), TownDesireInfo::ForSleep);
	EXPECT_EQ(_system.GetMostWanted(entt::null), TownDesireInfo::None);
}

TEST_F(TownDesireSystemTest, BoostResortsOnlyWhenAsked)
{
	_desire->desire.at(D(TownDesireInfo::ForSleep)) = 0.75f;
	ecs::town_desire::SortDesires(*_desire);
	_system.SetBoost(_town, TownDesireInfo::ForAbodes, 1.0f, false);
	EXPECT_FLOAT_EQ(_desire->boost.at(D(TownDesireInfo::ForAbodes)), 1.0f);
	EXPECT_EQ(_system.GetMostWanted(_town), TownDesireInfo::ForSleep);
	_system.SetBoost(_town, TownDesireInfo::ForAbodes, 1.0f, true);
	EXPECT_EQ(_system.GetMostWanted(_town), TownDesireInfo::ForAbodes);
	// no town, or no such desire: nothing written
	_system.SetBoost(entt::null, TownDesireInfo::ForFood, 0.5f, true);
	_system.SetBoost(_town, TownDesireInfo::None, 0.5f, true);
	EXPECT_FLOAT_EQ(_desire->boost.at(D(TownDesireInfo::ForFood)), 0.0f);
}

TEST_F(TownDesireSystemTest, VillagerOfferedMostWantedFirst)
{
	// Sleep, then Food, then nothing the villagers can serve above the trigger
	_desire->desire.at(D(TownDesireInfo::ForSleep)) = 1.0f;
	_desire->desire.at(D(TownDesireInfo::ForFood)) = 0.5f;
	ecs::town_desire::SortDesires(*_desire);
	std::vector<TownDesireInfo> asked;
	uint32_t answer = 0;
	const auto satisfy = [&](TownDesireInfo d) -> uint32_t {
		asked.push_back(d);
		return answer;
	};
	EXPECT_EQ(_system.OfferVillager(_town, false, 0.3f, satisfy), 0u);
	EXPECT_EQ(asked, (std::vector<TownDesireInfo> {TownDesireInfo::ForSleep, TownDesireInfo::ForFood}));
	// the first one that takes the villager ends the offer
	asked.clear();
	answer = 1;
	EXPECT_EQ(_system.OfferVillager(_town, false, 0.3f, satisfy), 1u);
	EXPECT_EQ(asked, (std::vector<TownDesireInfo> {TownDesireInfo::ForSleep}));
	// a child is not offered food
	asked.clear();
	answer = 0;
	EXPECT_EQ(_system.OfferVillager(_town, true, 0.3f, satisfy), 0u);
	EXPECT_EQ(asked, (std::vector<TownDesireInfo> {TownDesireInfo::ForSleep}));
	// no town: nothing offered
	asked.clear();
	EXPECT_EQ(_system.OfferVillager(entt::null, false, 0.3f, satisfy), 0u);
	EXPECT_TRUE(asked.empty());
}
