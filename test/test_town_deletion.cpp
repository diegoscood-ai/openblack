/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A town's deletion, as the original's: a town that its last building leaves, with no plan and nobody living in it, is
// deleted; it leaves its player's and the game's lists, its homeless go off as vagrants, its animals have no town and
// its need flags and rewards go with it. No game data: a hand-made info.dat, abodes without meshes.
// docs/wiki/world/buildings.md (a town's deletion)

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <memory>
#include <string_view>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownDeletion.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace abodes = openblack::ecs::abodes;
namespace town_deletion = openblack::ecs::town_deletion;

namespace
{
constexpr auto k_House = static_cast<AbodeInfo>(0);
constexpr uint32_t k_TownId = 1;

class TownDeletionTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		auto& house = info->abode.at(static_cast<size_t>(k_House));
		house.abodeType = AbodeType::LivingQuarters;
		house.abodeNumber = AbodeNumber::A;
		house.tribeType = Tribe::CELTIC;
		house.thresholdForStopBeingFunctional = 0.75f;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		auto& registry = Reg();
		// entity 0 is "no thing" for a script id: the town must not be it
		registry.Create();
		_town = registry.Create();
		auto& town = registry.Assign<Town>(_town);
		town.id = k_TownId;
		town.owner = PlayerNames::PLAYER_ONE;
		registry.Context().towns[k_TownId] = _town;
		game_clock::SetTurn(0);
		ecs::effects::reactions::Clear();
		ecs::town_villagers::ClearVagrants();
	}

	void TearDown() override
	{
		ecs::SetDeferredDeletion(false);
		ecs::town_villagers::ClearVagrants();
		ecs::effects::reactions::Clear();
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A built house of the town, without a mesh nor a Transform (its deletion then leaves no plan)
	entt::entity MakeHouse()
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		registry.Assign<Abode>(e, AbodeNumber::A, k_TownId, 0u, 0u).info = k_House;
		return e;
	}

	[[nodiscard]] bool TownListed() const
	{
		const auto listed = ecs::map_cells::TownsOf(PlayerNames::PLAYER_ONE);
		const auto newest = ecs::town_queries::TownsNewestFirst();
		return std::ranges::find(listed, _town) != listed.end() || std::ranges::find(newest, _town) != newest.end();
	}

	entt::entity _town {entt::null};
};
} // namespace

TEST(TownDeletion, ATownGoesOnlyWithNoBuildingNoPlanAndNobodyLeft)
{
	EXPECT_TRUE(town_deletion::EmptiedForGood(0, 0, 0));
	EXPECT_FALSE(town_deletion::EmptiedForGood(1, 0, 0));
	EXPECT_FALSE(town_deletion::EmptiedForGood(0, 1, 0));
	EXPECT_FALSE(town_deletion::EmptiedForGood(0, 0, 1));
}

TEST_F(TownDeletionTest, ItsLastBuildingGoingDeletesTheTown)
{
	const auto first = MakeHouse();
	const auto last = MakeHouse();
	ASSERT_TRUE(TownListed());
	// another building is left: the town stays
	abodes::DestroyedByEffect(first);
	ASSERT_TRUE(Reg().Valid(_town));
	EXPECT_TRUE(Reg().Context().towns.contains(k_TownId));
	EXPECT_TRUE(TownListed());
	// the last one: the town goes, out of the game's list (the deferral is off, so at once)
	abodes::DestroyedByEffect(last);
	EXPECT_FALSE(Reg().Valid(_town));
	EXPECT_FALSE(Reg().Context().towns.contains(k_TownId));
}

TEST_F(TownDeletionTest, ATownWithSomebodyOrAPlanLeftStays)
{
	Reg().Get<Town>(_town).stats.adults = 1;
	abodes::DestroyedByEffect(MakeHouse());
	ASSERT_TRUE(Reg().Valid(_town));
	EXPECT_TRUE(Reg().Context().towns.contains(k_TownId));
	auto& town = Reg().Get<Town>(_town);
	town.stats.adults = 0;
	town.stats.children = 1;
	abodes::DestroyedByEffect(MakeHouse());
	ASSERT_TRUE(Reg().Valid(_town));
	Reg().Get<Town>(_town).stats.children = 0;
	Reg().Get<Town>(_town).plannedAbodes.emplace_back();
	abodes::DestroyedByEffect(MakeHouse());
	EXPECT_TRUE(Reg().Valid(_town));
	EXPECT_TRUE(Reg().Context().towns.contains(k_TownId));
}

TEST_F(TownDeletionTest, ADeletedTownLeavesItsListsAndLetsGoOfWhatItHeld)
{
	auto& registry = Reg();
	ecs::SetDeferredDeletion(true);
	// a homeless villager of the town
	const auto villager = registry.Create();
	registry.Assign<Villager>(villager).town = _town;
	ecs::town_villagers::AddToHomelessList(_town, villager);
	// an animal of the town
	const auto animal = registry.Create();
	registry.Assign<Animal>(animal).town = _town;
	registry.Get<Town>(_town).animals.push_back(animal);
	// a need flag and a reward of the town, and a reward of no town
	const auto flag = registry.Create();
	registry.Assign<TownDesireFlag>(flag).town = _town;
	TownDesireFlags flags;
	flags.flags.fill(entt::null);
	flags.flags.at(2) = flag;
	registry.Assign<TownDesireFlags>(_town, flags);
	const auto reward = registry.Create();
	registry.Assign<Reward>(reward).town = _town;
	const auto otherReward = registry.Create();
	registry.Assign<Reward>(otherReward);

	// a script's deletion of the town goes through the same steps
	ecs::ToBeDeleted(_town);

	// marked, out of its player's list and the game's
	ASSERT_TRUE(registry.Valid(_town));
	EXPECT_TRUE(registry.AllOf<Unavailable>(_town));
	EXPECT_FALSE(registry.Context().towns.contains(k_TownId));
	EXPECT_FALSE(TownListed());
	// its homeless villager is a vagrant with no town
	EXPECT_TRUE(ecs::town_villagers::Homeless(_town).empty());
	EXPECT_TRUE(ecs::town_villagers::IsVagrant(villager));
	EXPECT_TRUE(registry.Get<Villager>(villager).town == entt::null);
	// its animal has no town, and its list is empty
	EXPECT_TRUE(registry.Get<Animal>(animal).town == entt::null);
	EXPECT_TRUE(registry.Get<Town>(_town).animals.empty());
	// its need flag and its reward go; another reward stays
	EXPECT_FALSE(ecs::IsAvailable(flag));
	EXPECT_TRUE(registry.Get<TownDesireFlags>(_town).flags.at(2) == entt::null);
	EXPECT_FALSE(ecs::IsAvailable(reward));
	EXPECT_TRUE(ecs::IsAvailable(otherReward));
	// a second deletion does nothing more
	ecs::ToBeDeleted(_town);
	EXPECT_TRUE(ecs::IsAvailable(otherReward));
}

// THING_VALID of a town, as the original's: a town is valid while one of its buildings stands, and a thing while the
// scripts can reach it (docs/wiki/scripts/script-commands.md)
TEST_F(TownDeletionTest, AScriptSeesATownAsValidWhileOneOfItsBuildingsStands)
{
	if (!spdlog::get("scripting"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
	}
	const openblack::test::RestoreService<Locator::vm> restoreVm;
	Locator::vm::emplace<lhvm::LHVM>();
	chlapi::CHLApi api;
	const auto& table = api.GetFunctionsTable();
	const auto native =
	    std::ranges::find_if(table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "THING_VALID"; });
	ASSERT_NE(native, table.end());
	const auto valid = [&native](entt::entity thing) {
		auto& vm = Locator::vm::value();
		vm.Pusho(static_cast<uint32_t>(thing));
		native->impl();
		lhvm::DataType type {};
		const auto answer = vm.Pop(type);
		EXPECT_EQ(type, lhvm::DataType::Boolean);
		return answer.intVal != 0;
	};
	// a town with no building is wholly destroyed; with one standing it is valid
	EXPECT_FALSE(valid(_town));
	const auto house = MakeHouse();
	EXPECT_TRUE(valid(_town));
	// a thing being deleted is not valid any more, nor is nothing
	ecs::SetDeferredDeletion(true);
	ecs::ToBeDeleted(house);
	EXPECT_FALSE(valid(house));
	EXPECT_FALSE(valid(entt::entity {0}));
}
