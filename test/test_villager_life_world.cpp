/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Villagers' lives in a small world of the game's own systems and data, with villagers, towns and huts made as the game
// makes them: a dying villager drops its wood as a log and loses its food; a hungry grown villager walks slower; a
// villager whose home is destroyed becomes homeless in its town; a missionary moves into the nearest town's hut, or,
// with none, goes back to deciding what to do. Every expected value is the original's
// (docs/wiki/project/progress/villager/death.md, daily_routine.md and special_villagers.md).

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerMissionary.h"
#include "ECS/VillagerAge.h"
#include "ECS/VillagerNeeds.h"
#include "ECS/VillagerSpeed.h"
#include "Enums.h"
#include "support/VillagerWorld.h"

using namespace openblack;
using ecs::components::Villager;
using ecs::villager::Index;

namespace
{
const glm::vec2 k_At {1005.0f, 1205.0f};

class VillagerLifeWorld: public test::world::VillagerWorld
{
protected:
	void SetUp() override
	{
		VillagerWorld::SetUp();
		if (IsSkipped() || HasFatalFailure() || GameData())
		{
			return;
		}
		for (const auto type : {VillagerInfo::CelticForesterMale, VillagerInfo::CelticFarmerMale})
		{
			auto& row = Info().villager.at(static_cast<size_t>(type));
			row.life = 1.0f;
			row.lifeWhenCrawlsWounded = 0.15f;
			row.grownUpAge = 13;
			row.oldAge = 60;
			row.minWoodToShowGraphic = 50;
			row.minFoodToShowGraphic = 100;
		}
		// the log a villager drops is a dead pine
		auto& pine = Info().tree.at(static_cast<size_t>(TreeInfo::Pine));
		pine.normal = MeshId::ObjectRockChalk;
		pine.woodValue = 100;
		pine.weight = 50.0f;
		pine.carriedType = CarriedTreeType::Evergreen;
		// the hut
		auto& hut = Info().abode.at(static_cast<size_t>(AbodeInfo::CelticHut));
		hut.meshId = MeshId::BuildingCeltic1;
		hut.abodeNumber = static_cast<AbodeNumber>(1);
		hut.maxVillagersInAbode = 4;
	}

	/// A town of the player, as a land script makes one
	[[nodiscard]] static entt::entity Town(int id, glm::vec2 at, PlayerNames player)
	{
		return ecs::archetypes::TownArchetype::Create(id, glm::vec3(at.x, k_Ground, at.y), player, Tribe::CELTIC);
	}

	/// A hut of the town standing on the land
	[[nodiscard]] entt::entity Hut(uint32_t town, glm::vec2 at) const
	{
		LoadMesh(Info().abode.at(static_cast<size_t>(AbodeInfo::CelticHut)).meshId, glm::vec3(-4.0f, 0.0f, -4.0f),
		         glm::vec3(4.0f, 5.0f, 4.0f));
		return ecs::archetypes::AbodeArchetype::Create(town, glm::vec3(at.x, k_Ground, at.y), AbodeInfo::CelticHut, 0.0f, 1.0f,
		                                               0, 0, false, false);
	}

	[[nodiscard]] static std::vector<entt::entity> Logs()
	{
		std::vector<entt::entity> found;
		Registry().Each<const ecs::components::DeadTree>(
		    [&found](entt::entity log, const ecs::components::DeadTree& /*dead*/) { found.push_back(log); });
		return found;
	}
};
} // namespace

// A dying villager drops the wood it carries as a log of what it held, and loses its food
TEST_P(VillagerLifeWorld, ADyingVillagerDropsItsWoodAsALogAndLosesItsFood)
{
	LoadMesh(MeshId::ObjectBranchFruitInHand, glm::vec3(-1.5f, -0.25f, -0.25f), glm::vec3(1.5f, 0.25f, 0.25f));
	const auto villager = Villager(VillagerInfo::CelticForesterMale, k_At);
	auto& held = Registry().Get<ecs::components::Villager>(villager).resourceHeld;
	held.at(0) = 200;
	held.at(1) = 200;
	Registry().Get<ecs::components::SkeletalAnimation>(villager).carriedObject = static_cast<int32_t>(CarriedObject::Tree_2);
	ASSERT_TRUE(Logs().empty());
	ecs::villager::VillagerDead(villager, DeathReason::OldAge, std::nullopt, 1.0f, 1);
	const auto logs = Logs();
	ASSERT_EQ(logs.size(), 1u);
	EXPECT_EQ(ecs::TreeCarriedType(logs.front()), CarriedTreeType::Fruit);
	EXPECT_EQ(held.at(0), 0);
	EXPECT_EQ(held.at(1), 0);
}

// A grown villager walks slower the hungrier it is
TEST_P(VillagerLifeWorld, AHungryGrownVillagerWalksSlower)
{
	const auto villager = Villager(VillagerInfo::CelticFarmerMale, k_At, 30);
	auto& v = Registry().Get<ecs::components::Villager>(villager);
	const auto speedWith = [&v, villager](float food) {
		v.food = food;
		ecs::SetVillagerSpeed(villager, 1000, true);
		return ecs::WholeSpeed(Registry().Get<const ecs::components::WallHug>(villager));
	};
	const auto fed = speedWith(1.0f);
	const auto hungry = speedWith(0.1f);
	EXPECT_LT(hungry, fed);
	// by the hunger alone: the same villager, its desire for food the only difference
	const auto* info = ecs::VillagerInfoOf(villager);
	ASSERT_NE(info, nullptr);
	const auto factor = [&](float food) {
		return ecs::villager_speed::PersonalFactor({
		    .creationIndex = static_cast<uint32_t>(ecs::object_index::Of(villager)),
		    .age = ecs::villager_age::AgeNow(v),
		    .grownUpAge = info->grownUpAge,
		    .oldAge = info->oldAge,
		    .desireForFood = ecs::villager_needs::DesireForFood(food),
		    .life = v.life,
		    .female = v.sex == Villager::Sex::FEMALE,
		});
	};
	EXPECT_EQ(hungry, ecs::villager_speed::FinalSpeed(1000, factor(0.1f)));
	EXPECT_EQ(fed, ecs::villager_speed::FinalSpeed(1000, factor(1.0f)));
}

// A villager whose home is destroyed becomes homeless: it has no abode and is on its town's homeless list
TEST_P(VillagerLifeWorld, AVillagerWhoseHomeIsDestroyedBecomesHomeless)
{
	const auto town = Town(1, k_At, PlayerNames::PLAYER_ONE);
	const auto hut = Hut(1, k_At + glm::vec2(10.0f, 0.0f));
	ASSERT_NE(hut, entt::entity {entt::null});
	const auto villager = Villager(VillagerInfo::CelticFarmerMale, k_At + glm::vec2(0.0f, 10.0f));
	ecs::abode_villagers::AddVillagerToAbode(hut, villager);
	ASSERT_EQ(Registry().Get<const ecs::components::Villager>(villager).abode, hut);
	ecs::ToBeDeleted(hut);
	ecs::ProcessDeadList(true);
	EXPECT_EQ(Registry().Get<const ecs::components::Villager>(villager).abode, entt::entity {entt::null});
	const auto& homeless = Registry().Get<const ecs::components::Town>(town).homelessVillagers;
	EXPECT_NE(std::ranges::find(homeless, villager), homeless.end());
}

// A missionary with no abode anywhere goes back to deciding what to do
TEST_P(VillagerLifeWorld, AMissionaryWithNoAbodeAnywhereGoesBackToDeciding)
{
	const auto missionary = Villager(VillagerInfo::CelticFarmerMale, k_At);
	ecs::villager::SetState(missionary, Index::Top, VillagerStates::MissionaryDisciple);
	static_cast<void>(ecs::villager::MissionaryDisciple(Registry().Get<ecs::components::LivingAction>(missionary)));
	EXPECT_EQ(ecs::villager::GetState(missionary, Index::Top), VillagerStates::DecideWhatToDo);
}

// A missionary is moved into the nearest town's hut and goes home to change there, and the town is preached to
TEST_P(VillagerLifeWorld, AMissionaryMovesIntoTheNearestTownsHutAndChangesThere)
{
	const auto own = Town(1, k_At, PlayerNames::PLAYER_ONE);
	const auto other = Town(2, k_At + glm::vec2(200.0f, 0.0f), PlayerNames::PLAYER_TWO);
	const auto home = Hut(2, k_At + glm::vec2(210.0f, 0.0f));
	const auto missionary = Villager(VillagerInfo::CelticFarmerMale, k_At + glm::vec2(180.0f, 0.0f));
	ecs::villager::SetTown(missionary, own);
	ecs::villager::SetState(missionary, Index::Top, VillagerStates::MissionaryDisciple);
	static_cast<void>(ecs::villager::MissionaryDisciple(Registry().Get<ecs::components::LivingAction>(missionary)));
	EXPECT_EQ(Registry().Get<const ecs::components::Villager>(missionary).abode, home);
	EXPECT_EQ(ecs::villager::GetState(missionary, Index::Top), VillagerStates::GoHomeAndChange);
	EXPECT_FALSE(Registry().Get<const ecs::components::Town>(other).missionaries.empty());
}

INSTANTIATE_TEST_SUITE_P(Data, VillagerLifeWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
