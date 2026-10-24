/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The building / town side of repairs and of the town emergency: ProcessTownRepairs (the plans and the abodes
// sharing one best), the rock-damaged house's ordinary site, MoveAbodeToPlannedAbodes and the rebuild plan it leaves
// (wasBuilt), SetInStateOfEmergency / ProcessTownEmergency (emergencyTurn, the worship saved in
// savedWorshipPercentage) and ReduceLife's emergency for an unbuilt storage pit. No game data: a hand-made info.dat,
// abodes without meshes.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Footpaths.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownEmergency.h"
#include "ECS/Town/TownQueries.h"
#include "Enums.h"
#include "Game/GameStats.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/Rules/Explosion.h"
#include "Worship/WorshipPercentage.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace sites = openblack::ecs::building_sites;
namespace plans = openblack::ecs::plans;
namespace abodes = openblack::ecs::abodes;
namespace emergency = openblack::ecs::town_emergency;

namespace
{
constexpr auto k_House = static_cast<AbodeInfo>(0);
constexpr auto k_Pit = static_cast<AbodeInfo>(1);
constexpr auto k_Centre = static_cast<AbodeInfo>(2);
constexpr auto k_Totem = static_cast<AbodeInfo>(3);
constexpr uint32_t k_TownId = 1;

class RepairsTest: public ::testing::Test
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
		house.maxVillagersInAbode = 4;
		house.desireToBeRepaired = 0.5f;
		house.maxVillagerNeededToBuild = 4;
		house.thresholdForStopBeingFunctional = 0.75f;
		house.aggressorValue = 2.0f;
		auto& pit = info->abode.at(static_cast<size_t>(k_Pit));
		pit.abodeType = AbodeType::StoragePit;
		pit.abodeNumber = AbodeNumber::StoragePit;
		pit.tribeType = Tribe::CELTIC;
		pit.desireToBeRepaired = 0.9f;
		pit.maxVillagerNeededToBuild = 4;
		pit.thresholdForStopBeingFunctional = 0.75f;
		auto& centre = info->abode.at(static_cast<size_t>(k_Centre));
		centre.abodeType = AbodeType::TownCentre;
		centre.abodeNumber = AbodeNumber::TownCentre;
		centre.tribeType = Tribe::CELTIC;
		centre.thresholdForStopBeingFunctional = 0.75f;
		auto& totem = info->abode.at(static_cast<size_t>(k_Totem));
		totem.abodeType = AbodeType::Totem;
		totem.abodeNumber = AbodeNumber::Totem;
		totem.tribeType = Tribe::CELTIC;
		info->town.thresholdToStartRepairing = 0.9f;
		info->town.gameTurnsAfterEmergencyVillagersReact = 1200;
		info->town.firstTimeDamageDoneAddition = 0.5f;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		auto& registry = Reg();
		_town = registry.Create();
		auto& town = registry.Assign<Town>(_town);
		town.id = k_TownId;
		town.owner = PlayerNames::PLAYER_ONE;
		registry.Context().towns[k_TownId] = _town;
		game_clock::SetTurn(0);
	}

	void TearDown() override
	{
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	Town& TownData() { return Reg().Get<Town>(_town); }
	static const GAbodeInfo& Info(AbodeInfo info)
	{
		return Locator::infoConstants::value().abode.at(static_cast<size_t>(info));
	}

	/// An abode of a town without a mesh; under construction as the plans make them. `people` inhabitants (bare
	/// entities: only their count is read here)
	entt::entity MakeAbode(AbodeInfo info, bool underConstruction, uint32_t people = 0, uint32_t townId = k_TownId)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		auto& abode = registry.Assign<Abode>(e, Info(info).abodeNumber, townId, 0u, 0u);
		abode.info = info;
		if (underConstruction)
		{
			abode.buildFlags = Abode::k_UnderConstruction;
			abode.percentBuilt = 0.0f;
			abode.addedToTownStats = false;
		}
		for (uint32_t i = 0; i < people; ++i)
		{
			Reg().Get<Abode>(e).inhabitants.push_back(registry.Create());
		}
		return e;
	}

	/// A plan of the house's info; `rebuild` is wasBuilt
	plans::PlanIndex AddPlan(bool rebuild)
	{
		PlannedAbode plan {k_House, glm::vec3(100.0f, 0.0f, 100.0f), 0.0f, 1.0f, false};
		plan.wasBuilt = rebuild;
		return plans::AddPlanned(_town, plan);
	}

	/// The town's worship part, with a worship site that is not a live entity (SetWorshipPercentage keeps the value
	/// only with a site; GetWorshipersNeeded reads it only when valid)
	TownMagic& Magic()
	{
		auto& registry = Reg();
		if (registry.TryGet<TownMagic>(_town) == nullptr)
		{
			const auto gone = registry.Create();
			registry.Destroy(gone);
			registry.Assign<TownMagic>(_town).worshipSite = gone;
		}
		return registry.Get<TownMagic>(_town);
	}

	entt::entity _town {entt::null};
};

// ---- ProcessTownRepairs ---------------------------------------------------------------------------------------------

TEST_F(RepairsTest, RebuildPlanBeatsALessDamagedAbode)
{
	// plan wasBuilt: desireToBeRepaired = 0.5; house at life 0.5 with someone in:
	// ((1 - 0.5) x 0.5 + 0.5) x 0.5 = 0.375 < 0.5
	AddPlan(true);
	const auto house = MakeAbode(k_House, false, 1);
	ecs::life::SetLife(house, 0.5f);
	const auto choice = sites::ChooseTownRepair(_town);
	ASSERT_TRUE(choice.plan.has_value());
	EXPECT_EQ(*choice.plan, 0u);
	EXPECT_TRUE(choice.abode == entt::null);
}

TEST_F(RepairsTest, AbodeMustBeatTheBestPlanStrictly)
{
	// life 0: (1 x 0.5 + 0.5) x 0.5 = 0.5 == the plan's 0.5: the plan stays (the abode must be strictly above)
	AddPlan(true);
	const auto house = MakeAbode(k_House, false, 1);
	ecs::life::SetLife(house, 0.0f);
	auto choice = sites::ChooseTownRepair(_town);
	ASSERT_TRUE(choice.plan.has_value());
	EXPECT_TRUE(choice.abode == entt::null);
	// a storage pit at 0.6: (0.4 x 0.5 + 0.5) x 0.9 = 0.63 > 0.5 -> the abode
	const auto pit = MakeAbode(k_Pit, false);
	ecs::life::SetLife(pit, 0.6f);
	choice = sites::ChooseTownRepair(_town);
	EXPECT_EQ(choice.abode, pit);
}

TEST_F(RepairsTest, PlansWithoutRebuildFlagAndTiesKeepTheFirst)
{
	AddPlan(false); // wasBuilt false: skipped
	AddPlan(true);
	AddPlan(true); // the same 0.5: the first one with it stays
	const auto choice = sites::ChooseTownRepair(_town);
	ASSERT_TRUE(choice.plan.has_value());
	EXPECT_EQ(*choice.plan, 1u);
	EXPECT_TRUE(choice.abode == entt::null);
}

TEST_F(RepairsTest, ProcessTownRepairsMakesARepairSiteOnce)
{
	const auto pit = MakeAbode(k_Pit, false);
	ecs::life::SetLife(pit, 0.6f);
	sites::ProcessTownRepairs(_town);
	const auto site = abodes::GetBuildingSite(pit);
	ASSERT_TRUE(site != entt::null);
	// the not-repaired build flag is set before AddBuildingSite, so the site copies it: a repair site
	EXPECT_NE(Reg().Get<Abode>(pit).buildFlags & Abode::k_NotRepaired, 0u);
	EXPECT_TRUE(sites::IsRepairSite(site));
	// GetBestRepairBuildingSite: GetDesireForVillagers (4 / 4) x 0.63 > 0
	EXPECT_EQ(sites::GetBestRepairBuildingSite(_town), site);
	// the next turn the abode has a site: nothing new
	sites::ProcessTownRepairs(_town);
	EXPECT_EQ(sites::SitesOf(_town).size(), 1u);
	EXPECT_EQ(abodes::GetBuildingSite(pit), site);
}

TEST_F(RepairsTest, NothingDamagedNothingChosen)
{
	MakeAbode(k_House, false, 1); // life 1 > 0.9: 0
	MakeAbode(k_House, false, 0); // empty
	const auto choice = sites::ChooseTownRepair(_town);
	EXPECT_FALSE(choice.plan.has_value());
	EXPECT_TRUE(choice.abode == entt::null);
	sites::ProcessTownRepairs(_town);
	EXPECT_TRUE(sites::SitesOf(_town).empty());
}

// ---- the rock-damaged house -----------------------------------------------------------------------------------------

TEST_F(RepairsTest, RockSiteIsAnOrdinarySite)
{
	const auto house = MakeAbode(k_House, false, 1);
	const map_coords::MapCoords pos {0, 0, 0.0f};
	// ReduceLife: life 0.95 < 1 -> AddBuildingSite; the site's repair flag = bit 2 = 0
	EXPECT_FLOAT_EQ(abodes::ReduceLife(house, 0.05f, std::nullopt), 0.95f);
	const auto site = abodes::GetBuildingSite(house);
	ASSERT_TRUE(site != entt::null);
	EXPECT_FALSE(sites::IsRepairSite(site));
	EXPECT_NEAR(sites::GetRepairBase(site), 1.1f * 0.95f - 0.1f, 1e-6f);
	EXPECT_TRUE(sites::GetBestRepairBuildingSite(_town) == entt::null);
	// life > 0.9: GetDesireToBeRepaired 0 -> no builders needed; only a BUILDER disciple (includeFull) takes it
	EXPECT_FALSE(sites::NeedsBuilders(site));
	EXPECT_TRUE(sites::GetBestBuildingSite(_town, pos, false) == entt::null);
	EXPECT_EQ(sites::GetBestBuildingSite(_town, pos, true), site);
	// life 0.85 <= 0.9: the ordinary builders (GetBestBuildingSite, no repair flag test)
	abodes::ReduceLife(house, 0.1f, std::nullopt);
	EXPECT_TRUE(sites::NeedsBuilders(site));
	EXPECT_EQ(sites::GetBestBuildingSite(_town, pos, false), site);
	// still never the repair desire's site, and ProcessTownRepairs skips an abode with a site
	EXPECT_TRUE(sites::GetBestRepairBuildingSite(_town) == entt::null);
	EXPECT_TRUE(sites::ChooseTownRepair(_town).abode == entt::null);
	EXPECT_EQ(sites::SitesOf(_town).size(), 1u);
}

TEST_F(RepairsTest, EmptyDamagedHouseNeedsNoBuilders)
{
	const auto house = MakeAbode(k_House, false, 0);
	abodes::ReduceLife(house, 0.5f, std::nullopt);
	const auto site = abodes::GetBuildingSite(house);
	ASSERT_TRUE(site != entt::null);
	EXPECT_EQ(abodes::GetDesireToBeRepaired(house), 0.0f); // an empty house
	EXPECT_FALSE(sites::NeedsBuilders(site));
}

TEST_F(RepairsTest, ABuildingTheBlastsWaveReachesBecomesASiteDrawnAtNothingOnlyWithATown)
{
	// the wave takes all its life: with a town it gets a building site and, with no broken model, is drawn at
	// min(built, 0.98 x life) = 0 until repaired
	const auto house = MakeAbode(k_House, false);
	particles::explosion::DestroyedByBeam(house);
	EXPECT_FLOAT_EQ(ecs::life::LifeOf(house), 0.0f);
	const auto site = abodes::GetBuildingSite(house);
	ASSERT_TRUE(site != entt::null);
	EXPECT_TRUE(abodes::IsDrawBuilding(house));
	EXPECT_FLOAT_EQ(abodes::GetPercentForDrawBuilding(house), 0.0f);
	EXPECT_NEAR(sites::GetRepairBase(site), -0.1f, 1e-6f);
	EXPECT_EQ(sites::SitesOf(_town).size(), 1u);
	// without a town: no site, so it is still drawn whole
	const auto alone = MakeAbode(k_House, false, 0, 99); // town 99 does not exist
	particles::explosion::DestroyedByBeam(alone);
	EXPECT_FLOAT_EQ(ecs::life::LifeOf(alone), 0.0f);
	EXPECT_TRUE(abodes::GetBuildingSite(alone) == entt::null);
	EXPECT_FALSE(abodes::IsDrawBuilding(alone));
}

// ---- MoveAbodeToPlannedAbodes and the plan made from an abode -------------------------------------------------------

TEST_F(RepairsTest, DestroyedBuiltAbodeLeavesARebuildPlan)
{
	game_clock::SetTurn(77);
	const auto house = MakeAbode(k_House, false, 1);
	Reg().Assign<Transform>(house, glm::vec3(100.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_TRUE(abodes::MoveAbodeToPlannedAbodes(house));
	ASSERT_EQ(plans::PlansOf(_town), 1u);
	const auto& plan = TownData().plannedAbodes.front();
	EXPECT_TRUE(plan.wasBuilt); // the abode's build flag 8
	EXPECT_FALSE(plan.townCentre);
	EXPECT_EQ(plan.info, k_House);
	EXPECT_EQ(plan.position, glm::vec3(100.0f, 0.0f, 50.0f));
	EXPECT_FLOAT_EQ(plan.yAngleRadians, 0.0f);
	EXPECT_FLOAT_EQ(plan.scale, 1.0f);
	EXPECT_EQ(plan.creationTurn, 77u);
	EXPECT_FLOAT_EQ(plans::GetDesireToBeRepaired(_town, 0), 0.5f); // wasBuilt -> the info's desireToBeRepaired
	// ProcessTownRepairs can now pick it (its conversion sets bit 2: CreatePlannedNoFixedCheck)
	const auto choice = sites::ChooseTownRepair(_town);
	ASSERT_TRUE(choice.plan.has_value());
	EXPECT_EQ(*choice.plan, 0u);
}

TEST_F(RepairsTest, DestroyedUnbuiltAbodeLeavesAnOrdinaryPlan)
{
	const auto house = MakeAbode(k_House, true);
	Reg().Assign<Transform>(house, glm::vec3(10.0f, 0.0f, 20.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_TRUE(abodes::MoveAbodeToPlannedAbodes(house));
	ASSERT_EQ(plans::PlansOf(_town), 1u);
	EXPECT_FALSE(TownData().plannedAbodes.front().wasBuilt);
	EXPECT_EQ(plans::GetDesireToBeRepaired(_town, 0), 0.0f);
	EXPECT_FALSE(sites::ChooseTownRepair(_town).plan.has_value());
}

TEST_F(RepairsTest, AnAbodeMadeAPlanHandsItsFootpathLinkToThePlan)
{
	// the rebuild plan takes the abode's link and the abode is left with none, so its deletion keeps the link
	const auto house = MakeAbode(k_House, false, 1);
	Reg().Assign<Transform>(house, glm::vec3(100.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto link = Reg().Create();
	Reg().Assign<FootpathLink>(link, glm::vec3(100.0f, 0.0f, 50.0f), std::vector<Footpath::Id> {});
	ecs::footpaths::SetFootpathLink(house, link);
	EXPECT_TRUE(abodes::MoveAbodeToPlannedAbodes(house));
	ASSERT_EQ(plans::PlansOf(_town), 1u);
	EXPECT_EQ(TownData().plannedAbodes.front().footpathLink, link);
	EXPECT_EQ(ecs::footpaths::GetFootpathLink(house), entt::entity {entt::null});
	EXPECT_TRUE(Reg().Valid(link));
}

TEST_F(RepairsTest, AnAbodeWithoutALinkLeavesAPlanWithout)
{
	const auto house = MakeAbode(k_House, false, 1);
	Reg().Assign<Transform>(house, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_TRUE(abodes::MoveAbodeToPlannedAbodes(house));
	ASSERT_EQ(plans::PlansOf(_town), 1u);
	EXPECT_EQ(TownData().plannedAbodes.front().footpathLink, entt::entity {entt::null});
}

TEST_F(RepairsTest, NoTownNoPlan)
{
	const auto house = MakeAbode(k_House, false, 0, 99); // town 99 does not exist: GetTown 0 -> return 0
	Reg().Assign<Transform>(house, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_FALSE(abodes::MoveAbodeToPlannedAbodes(house));
	EXPECT_EQ(plans::PlansOf(_town), 0u);
}

// ---- the town emergency ---------------------------------------------------------------------------------------------

TEST_F(RepairsTest, SetInStateOfEmergencyStartsAndRefreshes)
{
	EXPECT_FALSE(ecs::town_queries::IsInStateOfEmergency(TownData()));
	game_clock::SetTurn(10);
	emergency::SetInStateOfEmergency(_town);
	EXPECT_EQ(TownData().emergencyTurn, 10u);
	EXPECT_TRUE(ecs::town_queries::IsInStateOfEmergency(TownData()));
	game_clock::SetTurn(30);
	emergency::SetInStateOfEmergency(_town); // refreshed
	EXPECT_EQ(TownData().emergencyTurn, 30u);
	game_clock::SetTurn(1229);
	EXPECT_TRUE(ecs::town_queries::IsInStateOfEmergency(TownData())); // 1199 < 1200
	game_clock::SetTurn(1230);
	EXPECT_FALSE(ecs::town_queries::IsInStateOfEmergency(TownData())); // 1200: over
}

TEST_F(RepairsTest, ProcessTownEmergencySavesAndRestoresTheWorship)
{
	Magic().worshipPercentage = 0.4f;
	game_clock::SetTurn(10);
	emergency::SetInStateOfEmergency(_town);
	// in the emergency: a worship percentage != 0 -> saved in savedWorshipPercentage, SetWorshipPercentage(0)
	game_clock::SetTurn(20);
	emergency::ProcessTownEmergency(_town);
	EXPECT_FLOAT_EQ(TownData().savedWorshipPercentage, 0.4f);
	EXPECT_EQ(worship::percentage::GetWorshipPercentage(_town), 0.0f);
	EXPECT_EQ(TownData().emergencyTurn, 10u);
	// attacked this turn (the last attack's turn == turn): refreshed; the saved value is kept (the percentage is 0 now)
	game_clock::SetTurn(50);
	TownData().aggression.lastTurn = 50;
	emergency::ProcessTownEmergency(_town);
	EXPECT_EQ(TownData().emergencyTurn, 50u);
	EXPECT_FLOAT_EQ(TownData().savedWorshipPercentage, 0.4f);
	// still in it at 1249 (1199 turns)
	game_clock::SetTurn(1249);
	emergency::ProcessTownEmergency(_town);
	EXPECT_EQ(TownData().emergencyTurn, 50u);
	// over: no fire -> the worship comes back, savedWorshipPercentage = emergencyTurn = 0
	game_clock::SetTurn(1250);
	emergency::ProcessTownEmergency(_town);
	EXPECT_FLOAT_EQ(worship::percentage::GetWorshipPercentage(_town), 0.4f);
	EXPECT_EQ(TownData().savedWorshipPercentage, 0.0f);
	EXPECT_EQ(TownData().emergencyTurn, 0u);
}

TEST_F(RepairsTest, RestoreOnlyWhenTheWorshipIsZero)
{
	// out of the emergency with a saved value but a current percentage != 0: not restored, both cleared
	Magic().worshipPercentage = 0.2f;
	TownData().savedWorshipPercentage = 0.6f;
	TownData().emergencyTurn = 0;
	game_clock::SetTurn(5);
	emergency::ProcessTownEmergency(_town);
	EXPECT_FLOAT_EQ(worship::percentage::GetWorshipPercentage(_town), 0.2f);
	EXPECT_EQ(TownData().savedWorshipPercentage, 0.0f);
}

TEST_F(RepairsTest, UnbuiltStoragePitAtZeroStartsTheEmergency)
{
	// a pit under construction whose percent reaches 0 drops its life 1 -> 0, crossing
	// the 0.75 threshold: StopBeingFunctional and SetInStateOfEmergency
	game_clock::SetTurn(100);
	const auto pit = MakeAbode(k_Pit, true);
	abodes::SetPercentBuilt(pit, 0.1f);
	EXPECT_EQ(abodes::ReduceLife(pit, 0.2f, std::nullopt), 0.0f);
	EXPECT_EQ(abodes::GetPercentBuilt(pit), 0.0f);
	EXPECT_EQ(TownData().emergencyTurn, 100u);
	EXPECT_TRUE(ecs::town_queries::IsInStateOfEmergency(TownData()));
	// a house does not cause it (CausesTownEmergencyIfDamaged is false for an abode)
	TownData().emergencyTurn = 0;
	const auto house = MakeAbode(k_House, false, 1);
	abodes::ReduceLife(house, 0.5f, std::nullopt);
	EXPECT_EQ(TownData().emergencyTurn, 0u);
}

TEST_F(RepairsTest, DamagedPitAboveTheThresholdNoEmergency)
{
	game_clock::SetTurn(7);
	const auto pit = MakeAbode(k_Pit, false);
	abodes::ReduceLife(pit, 0.125f, std::nullopt); // 0.875 > 0.75: still functional
	EXPECT_EQ(TownData().emergencyTurn, 0u);
	abodes::ReduceLife(pit, 0.125f, std::nullopt); // 0.75: 0.75 >= l -> stops, emergency
	EXPECT_EQ(TownData().emergencyTurn, 7u);
}

TEST_F(RepairsTest, TownCentreStopBeingFunctionalStopsTheWorship)
{
	// a town centre that stops being functional sets its town's worship percentage to 0
	Magic().worshipPercentage = 0.5f;
	const auto centre = MakeAbode(k_Centre, false);
	abodes::StopBeingFunctional(centre, std::nullopt);
	EXPECT_EQ(worship::percentage::GetWorshipPercentage(_town), 0.0f);
}

TEST_F(RepairsTest, ASettledBuildingAPlayerStopsCountsInThatPlayersStatistics)
{
	// a player stopping a building 200 turns old or more counts one stopped building; a younger one, or one stopped by
	// no player however old, counts nothing
	game_stats::ClearAll();
	constexpr auto k_Two = static_cast<size_t>(PlayerNames::PLAYER_TWO);
	const auto young = MakeAbode(k_House, false);
	Reg().Get<Abode>(young).age = 199;
	abodes::StopBeingFunctional(young, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(game_stats::Of(k_Two).buildingStopped, 0u);
	const auto settled = MakeAbode(k_House, false);
	Reg().Get<Abode>(settled).age = 200;
	abodes::StopBeingFunctional(settled, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(game_stats::Of(k_Two).buildingStopped, 1u);
	const auto old = MakeAbode(k_House, false);
	Reg().Get<Abode>(old).age = 255;
	abodes::StopBeingFunctional(old, std::nullopt);
	EXPECT_EQ(game_stats::Of(k_Two).buildingStopped, 1u);
	for (size_t player = 0; player < static_cast<size_t>(PlayerNames::PLAYER_TWO); ++player)
	{
		EXPECT_EQ(game_stats::Of(player).buildingStopped, 0u) << "player " << player;
	}
}

TEST_F(RepairsTest, AHealRestoresABuildingsLifeByItsHealDefence)
{
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	auto& house = info->abode.at(static_cast<size_t>(k_House));
	house.defenceMultiplierHeal = 1.5f;
	// the burn turned into damage divides by the combustion temperature, never 0 in info.dat
	house.combustionTemperature = 1000.0f;
	Locator::infoConstants::reset(info.release());
	const auto building = MakeAbode(k_House, false);
	ecs::life::SetLife(building, 0.5f);
	ecs::effects::EffectValues heal;
	heal.numbers.at(static_cast<size_t>(ecs::effects::EffectValues::Number::Heal)) = 0.2f;
	// the healing is the heal x the building's heal defence, 0.3: its life goes to 0.8, and the effect answers
	// (1 - the life it had) / the healing
	auto first = heal;
	EXPECT_FLOAT_EQ(ecs::effects::ApplyEffect(building, first), 0.5f / (0.2f * 1.5f));
	EXPECT_FLOAT_EQ(ecs::life::LifeOf(building), 0.8f);
	// never above whole, and no building site
	auto second = heal;
	ecs::effects::ApplyEffect(building, second);
	EXPECT_FLOAT_EQ(ecs::life::LifeOf(building), 1.0f);
	EXPECT_TRUE(abodes::GetBuildingSite(building) == entt::null);
	EXPECT_TRUE(sites::SitesOf(_town).empty());
}

TEST_F(RepairsTest, FieldLosesNoLife)
{
	// a field carries an Abode too; its ReduceLife only returns the life: no change, no site
	const auto field = MakeAbode(k_House, false, 1);
	Reg().Assign<Field>(field);
	EXPECT_EQ(abodes::ReduceLife(field, 0.5f, std::nullopt), 1.0f);
	EXPECT_EQ(ecs::life::LifeOf(field), 1.0f);
	EXPECT_TRUE(abodes::GetBuildingSite(field) == entt::null);
	EXPECT_TRUE(sites::SitesOf(_town).empty());
}

TEST_F(RepairsTest, UpdateAggressorRecordsAndRefreshesTheEmergency)
{
	game_clock::SetTurn(10);
	emergency::SetInStateOfEmergency(_town);
	// UpdateAggressor: no caused player -> the neutral one; the last attack's turn = the turn
	game_clock::SetTurn(40);
	emergency::UpdateAggressor(_town, std::nullopt, 0.0f);
	EXPECT_EQ(TownData().aggression.lastAggressor, PlayerNames::NEUTRAL);
	EXPECT_EQ(TownData().aggression.lastTurn, 40u);
	// ProcessTownEmergency: attacked this turn -> refreshed
	emergency::ProcessTownEmergency(_town);
	EXPECT_EQ(TownData().emergencyTurn, 40u);
	emergency::UpdateAggressor(_town, PlayerNames::PLAYER_ONE, 0.0f);
	EXPECT_EQ(TownData().aggression.lastAggressor, PlayerNames::PLAYER_ONE);
}

TEST_F(RepairsTest, UpdateAggressorFeedsTheTownsRecordWithTheTownInfosAddition)
{
	game_clock::SetTurn(12);
	// Another player's harm: the first time with the addition, by the protection multiplier, which wears down
	emergency::UpdateAggressor(_town, PlayerNames::PLAYER_TWO, 1.0f);
	const auto& record = TownData().aggression;
	EXPECT_FLOAT_EQ(record.aggression.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)), 1.5f);
	EXPECT_FLOAT_EQ(record.protectionMultiplier, 0.9f);
	EXPECT_FLOAT_EQ(record.mercyMultiplier, 1.0f);
	// The town's own player (PLAYER_ONE): by the mercy multiplier
	emergency::UpdateAggressor(_town, PlayerNames::PLAYER_ONE, 1.0f);
	EXPECT_FLOAT_EQ(record.aggression.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)), 1.5f);
	EXPECT_FLOAT_EQ(record.mercyMultiplier, 0.9f);
	EXPECT_EQ(record.lastTurns.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)), 12u);
	// what the attack did (its burn, crush and hit amounts) is kept as the aggressor's last attack
	emergency::UpdateAggressor(_town, PlayerNames::PLAYER_TWO, 0.0f, entt::null, {2.0f, 0.0f, 0.5f});
	EXPECT_EQ(record.lastEffects.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)).burn, 2.0f);
	EXPECT_EQ(record.lastEffects.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)).hit, 0.5f);
	EXPECT_EQ(record.lastEffects.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)).burn, 0.0f);
}

TEST_F(RepairsTest, AnAttackedObjectsTownIsAttackedByItsAggressorValue)
{
	// the house's aggressor value is 2 and the first-time addition 0.5: 0.25 x 2 + 0.5
	const auto house = MakeAbode(k_House, false);
	emergency::AttackTown(house, 0.25f, PlayerNames::PLAYER_TWO, entt::null, {0.0f, 0.75f});
	EXPECT_FLOAT_EQ(TownData().aggression.aggression.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)), 1.0f);
	EXPECT_EQ(TownData().aggression.lastEffects.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)).crush, 0.75f);
	// A thing of no town: nothing
	const auto loose = Reg().Create();
	emergency::AttackTown(loose, 1.0f, PlayerNames::PLAYER_THREE);
	EXPECT_EQ(TownData().aggression.aggression.at(static_cast<size_t>(PlayerNames::PLAYER_THREE)), 0.0f);
}

TEST_F(RepairsTest, AHitterAnswersItsOwnPlayer)
{
	auto& registry = Reg();
	// A villager: its town's owner, none without a town
	const auto villager = registry.Create();
	registry.Assign<Villager>(villager).town = _town;
	EXPECT_EQ(emergency::HitterPlayer(villager), std::optional<PlayerNames> {PlayerNames::PLAYER_ONE});
	const auto homeless = registry.Create();
	// no town: the default villager's town is entity 0, which is this test's town
	registry.Assign<Villager>(homeless).town = entt::null;
	EXPECT_FALSE(emergency::HitterPlayer(homeless).has_value());
	// A creature: its owner
	const auto creature = registry.Create();
	registry.Assign<Creature>(creature, Creature {.owner = PlayerNames::PLAYER_THREE});
	EXPECT_EQ(emergency::HitterPlayer(creature), std::optional<PlayerNames> {PlayerNames::PLAYER_THREE});
	// A standing tree: none; anything else (a rock, a mobile object): the neutral player
	const auto tree = registry.Create();
	registry.Assign<Tree>(tree);
	EXPECT_FALSE(emergency::HitterPlayer(tree).has_value());
	const auto rock = registry.Create();
	EXPECT_EQ(emergency::HitterPlayer(rock), std::optional<PlayerNames> {PlayerNames::NEUTRAL});
	EXPECT_FALSE(emergency::HitterPlayer(entt::null).has_value());
}

TEST_F(RepairsTest, AnAttackersKindIsWhatItsAttackSampleSees)
{
	using audio::guidance::Attacker;
	auto& registry = Reg();
	// No thing: none; a creature; a rock, the bonfire, a fragment, a dead tree and a felled one as rocks; anything else
	// (a fence among them) a plain thing
	EXPECT_FALSE(emergency::AttackerOf(entt::null).has_value());
	const auto creature = registry.Create();
	registry.Assign<Creature>(creature);
	EXPECT_EQ(emergency::AttackerOf(creature), std::optional {Attacker::Creature});
	const auto fragment = registry.Create();
	registry.Assign<Fragment>(fragment);
	EXPECT_EQ(emergency::AttackerOf(fragment), std::optional {Attacker::Rock});
	const auto dead = registry.Create();
	registry.Assign<DeadTree>(dead);
	EXPECT_EQ(emergency::AttackerOf(dead), std::optional {Attacker::Rock});
	const auto felled = registry.Create();
	registry.Assign<FelledTree>(felled);
	EXPECT_EQ(emergency::AttackerOf(felled), std::optional {Attacker::Rock});
	const auto tree = registry.Create();
	registry.Assign<Tree>(tree);
	EXPECT_EQ(emergency::AttackerOf(tree), std::optional {Attacker::Thing});
	// the rock's row is of the rock kind; the fence's is not
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Rock)).mobileType = MobileStaticInfo::Rock;
	Locator::infoConstants::reset(info.release());
	const auto mobileStatic = [&registry](MobileStaticInfo row) {
		const auto thing = registry.Create();
		registry.Assign<MobileStatic>(thing, row);
		return thing;
	};
	EXPECT_EQ(emergency::AttackerOf(mobileStatic(MobileStaticInfo::Rock)), std::optional {Attacker::Rock});
	EXPECT_EQ(emergency::AttackerOf(mobileStatic(MobileStaticInfo::Bonfire)), std::optional {Attacker::Rock});
	EXPECT_EQ(emergency::AttackerOf(mobileStatic(MobileStaticInfo::CeltFenceShort)), std::optional {Attacker::Thing});
}

TEST(TownAttackSound, TheAttackIsReadAsTheSoundSeesIt)
{
	std::array<float, 8> aggression {};
	aggression.at(1) = 2.5f;
	const std::array<float, 7> effects {300.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f};
	// The local player's town attacked by player two: its people, position and want of protection, the effects and
	// the aggression held so far are passed on as they are
	const auto attack =
	    emergency::TownAttackOf(12, glm::vec3(1.0f, 2.0f, 3.0f), 4.0f, aggression, effects, audio::guidance::Attacker::Rock,
	                            PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(attack.population, 12u);
	EXPECT_EQ(attack.position, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_FLOAT_EQ(attack.severity, 4.0f);
	EXPECT_EQ(attack.effects, effects);
	EXPECT_EQ(attack.attacker, std::optional {audio::guidance::Attacker::Rock});
	EXPECT_TRUE(attack.townIsMine);
	EXPECT_TRUE(attack.townOfLocalPlayer);
	EXPECT_EQ(attack.causedPlayer, std::optional<uint32_t> {1u});
	EXPECT_FALSE(attack.causedIsLocalPlayer);
	EXPECT_FLOAT_EQ(attack.aggression.at(1), 2.5f);
	// Another player's town, by the local player; and an attack that names no player keeps none
	const auto mine = emergency::TownAttackOf(0, glm::vec3(0.0f), 0.0f, aggression, effects, std::nullopt,
	                                          PlayerNames::PLAYER_ONE, PlayerNames::NEUTRAL, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(mine.townIsMine);
	EXPECT_FALSE(mine.townOfLocalPlayer);
	EXPECT_TRUE(mine.causedIsLocalPlayer);
	EXPECT_FALSE(mine.attacker.has_value());
	const auto nobody = emergency::TownAttackOf(0, glm::vec3(0.0f), 0.0f, aggression, effects, std::nullopt, std::nullopt,
	                                            PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(nobody.causedPlayer.has_value());
	EXPECT_FALSE(nobody.causedIsLocalPlayer);
}

TEST_F(RepairsTest, AnEffectThatDestroysABuildingGoesOnWithTheBuildingGone)
{
	const test::RestoreService<Locator::playerSystem> restorePlayers;
	auto& players = Locator::playerSystem::emplace<ecs::systems::PlayerSystem>();
	// the house takes a crush whole and its harm moves the alignment as a building's
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	auto& house = info->abode.at(static_cast<size_t>(k_House));
	house.alignmentType = AlignmentType::Building;
	house.defenceMultiplierCrush = 1.0f;
	// the burn turned into damage divides by the combustion temperature, never 0 in info.dat: no burn, no damage
	house.combustionTemperature = 1000.0f;
	constexpr auto k_Crush = static_cast<size_t>(ecs::effects::EffectValues::Number::Crush);
	info->alignment.at(k_Crush).building = -0.5f;
	Locator::infoConstants::reset(info.release());
	const auto hitter = Reg().Create();
	const auto blow = [hitter](float crush) {
		ecs::effects::EffectValues values;
		values.numbers.at(static_cast<size_t>(ecs::effects::EffectValues::Number::Crush)) = crush;
		values.player = PlayerNames::PLAYER_TWO;
		values.appliedBy = hitter;
		return values;
	};
	const auto harmFrom = [&players](PlayerNames player) {
		return players.Rivalry(player).harmFrom.at(static_cast<size_t>(PlayerNames::PLAYER_TWO));
	};
	const auto aggressionOfTwo = [this]() {
		return TownData().aggression.aggression.at(static_cast<size_t>(PlayerNames::PLAYER_TWO));
	};

	// a blow that leaves it standing attacks its town (0.25 x 2 + 0.5) and its town's player keeps the harm
	const auto standing = MakeAbode(k_House, false);
	auto first = blow(0.25f);
	ecs::effects::ApplyEffect(standing, first);
	ASSERT_TRUE(Reg().Valid(standing));
	EXPECT_FLOAT_EQ(aggressionOfTwo(), 1.0f);
	EXPECT_EQ(harmFrom(PlayerNames::PLAYER_ONE), 0.25f);
	const float pending = players.Alignment(PlayerNames::PLAYER_TWO).pending;

	// a blow that takes the last of its life destroys it inside the effect, so the rest sees it gone: out of its town,
	// it attacks no town and its harm is the neutral player's; the alignment moves by the life it lost, 1:
	// 2 x -0.5 x (1 + 0), against an alignment of 0
	const auto doomed = MakeAbode(k_House, false);
	auto last = blow(2.0f);
	ecs::effects::ApplyEffect(doomed, last);
	EXPECT_FALSE(Reg().Valid(doomed));
	EXPECT_FLOAT_EQ(aggressionOfTwo(), 1.0f);
	EXPECT_EQ(harmFrom(PlayerNames::PLAYER_ONE), 0.25f);
	EXPECT_EQ(harmFrom(PlayerNames::NEUTRAL), 2.0f);
	EXPECT_FLOAT_EQ(players.Alignment(PlayerNames::PLAYER_TWO).pending, pending - 1.0f);
}

TEST_F(RepairsTest, AMiracleWearsAFishFarmDownAndItsLastLifeDeletesItStillAttackingItsTown)
{
	const test::RestoreService<Locator::playerSystem> restorePlayers;
	auto& players = Locator::playerSystem::emplace<ecs::systems::PlayerSystem>();
	// the game's one fish farm row (info.dat): a burn does it no harm, a crush or a hit counts whole, its aggressor
	// value is 3.75, and its combustion temperature and heat capacity are 9999 and 50000
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->fishFarm.defenceMultiplierBurn = 0.0f;
	info->fishFarm.defenceMultiplierCrush = 1.0f;
	info->fishFarm.defenceMultiplierHit = 1.0f;
	info->fishFarm.heatCapacity = 50000.0f;
	info->fishFarm.combustionTemperature = 9999.0f;
	info->fishFarm.aggressorValue = 3.75f;
	Locator::infoConstants::reset(info.release());
	auto& registry = Reg();
	const auto farm = registry.Create();
	registry.Assign<Transform>(farm, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<FishFarm>(farm).town = _town;
	// a fire reads the farm's own row: it is heated as that row says, and however hot, its burn takes no life
	EXPECT_EQ(ecs::fire::traits::CombustionTemperature(farm), 9999.0f);
	EXPECT_EQ(ecs::fire::traits::HeatCapacity(farm), 50000.0f);
	EXPECT_EQ(ecs::fire::traits::DefenceMultiplierBurn(farm), 0.0f);
	EXPECT_EQ(ecs::effects::ConvertTemperatureToDamage(farm, 19998.0f), 0.0f);
	// the fish puzzle's shoals have no place on the map and no row
	const auto shoal = registry.Create();
	registry.Assign<FishFarm>(shoal);
	EXPECT_EQ(ecs::fire::traits::InfoOf(shoal), nullptr);

	const auto hitter = registry.Create();
	const auto blow = [hitter](float hit) {
		ecs::effects::EffectValues values;
		values.numbers.at(static_cast<size_t>(ecs::effects::EffectValues::Number::Hit)) = hit;
		values.player = PlayerNames::PLAYER_TWO;
		values.appliedBy = hitter;
		return values;
	};
	const auto aggressionOfTwo = [this]() {
		return TownData().aggression.aggression.at(static_cast<size_t>(PlayerNames::PLAYER_TWO));
	};
	const auto harmFromTwo = [&players]() {
		return players.Rivalry(PlayerNames::PLAYER_ONE).harmFrom.at(static_cast<size_t>(PlayerNames::PLAYER_TWO));
	};
	// a hit of 0.25 leaves it at 0.75 and attacks its town, 0.25 x 3.75 plus the first time's 0.5; its town's player
	// keeps the harm
	auto first = blow(0.25f);
	ecs::effects::ApplyEffect(farm, first);
	ASSERT_TRUE(registry.Valid(farm));
	EXPECT_FLOAT_EQ(ecs::life::LifeOf(farm), 0.75f);
	EXPECT_FLOAT_EQ(aggressionOfTwo(), 1.4375f);
	EXPECT_FLOAT_EQ(harmFromTwo(), 0.25f);
	// a hit of 2 takes the rest: the farm is deleted, and its town is still attacked, 2 x 3.75 by the multiplier worn
	// down by a tenth, and its town's player still keeps the harm
	auto last = blow(2.0f);
	ecs::effects::ApplyEffect(farm, last);
	EXPECT_FALSE(registry.Valid(farm));
	EXPECT_FLOAT_EQ(aggressionOfTwo(), 1.4375f + 6.75f);
	EXPECT_FLOAT_EQ(harmFromTwo(), 2.25f);
}

TEST(TotemLife, ATotemsLifeIsItsTownCentresWhileItHasOne)
{
	const auto totem = static_cast<entt::entity>(5);
	const auto centre = static_cast<entt::entity>(7);
	EXPECT_EQ(ecs::life::LifeTarget(totem, true, centre), centre);
	EXPECT_EQ(ecs::life::LifeTarget(totem, true, entt::null), totem);
	EXPECT_EQ(ecs::life::LifeTarget(totem, false, centre), totem);
}

TEST_F(RepairsTest, ATotemsDamageAndRepairGoToItsTownCentre)
{
	auto& registry = Reg();
	const auto centre = MakeAbode(k_Centre, false);
	const auto icon = registry.Create();
	const auto totem = registry.Create();
	registry.Assign<VillageTotem>(totem, centre, icon, 0.0f, 0.0f);
	ecs::life::ReduceLife(totem, 0.25f);
	EXPECT_FLOAT_EQ(ecs::life::LifeOf(centre), 0.75f);
	EXPECT_FLOAT_EQ(ecs::life::LifeOf(totem), 1.0f);
	ecs::life::IncreaseLife(totem, 0.1f);
	EXPECT_FLOAT_EQ(ecs::life::LifeOf(centre), 0.85f);
	EXPECT_FLOAT_EQ(ecs::life::LifeOf(totem), 1.0f);
	// a totem with no town centre takes its own
	const auto alone = registry.Create();
	registry.Assign<VillageTotem>(alone, entt::entity {entt::null}, entt::entity {entt::null}, 0.0f, 0.0f);
	ecs::life::ReduceLife(alone, 0.25f);
	EXPECT_FLOAT_EQ(ecs::life::LifeOf(alone), 0.75f);
}

TEST_F(RepairsTest, NoEffectReachesATotemOrItsIcon)
{
	auto& registry = Reg();
	const auto centre = MakeAbode(k_Centre, false);
	const auto icon = registry.Create();
	const auto totem = registry.Create();
	registry.Assign<VillageTotem>(totem, centre, icon, 0.0f, 0.0f);
	const ecs::effects::EffectValues values {};
	EXPECT_FALSE(ecs::effects::IsEffectReceiver(totem, values));
	EXPECT_FALSE(ecs::effects::IsEffectReceiver(icon, values));
	EXPECT_TRUE(ecs::effects::IsEffectReceiver(centre, values));
}

TEST_F(RepairsTest, NoEffectReachesATownDesireFlag)
{
	// the flag's class takes no effect: the map's effects, a miracle's and an explosion's pass over it
	auto& registry = Reg();
	const auto flag = registry.Create();
	registry.Assign<TownDesireFlag>(flag);
	const ecs::effects::EffectValues values {};
	EXPECT_FALSE(ecs::effects::IsEffectReceiver(flag, values));
}

TEST_F(RepairsTest, NoEffectReachesATeleportStoneASeedATotemOrAnEmptyPot)
{
	// each class's own answer in the original: a teleport stone, a spell seed, a one-off seed and a totem building take
	// none; a pot or a pile takes them only while it holds something; a house and a villager do
	auto& registry = Reg();
	const auto stone = registry.Create();
	registry.Assign<TeleportStone>(stone);
	const auto seed = registry.Create();
	registry.Assign<SpellSeed>(seed);
	const auto oneOff = registry.Create();
	registry.Assign<OneOffSpellSeed>(oneOff);
	const auto full = registry.Create();
	registry.Assign<Pot>(full, uint16_t {800}, uint16_t {1000}, PotInfo::FoodPot);
	const auto empty = registry.Create();
	registry.Assign<Pot>(empty, uint16_t {0}, uint16_t {1000}, PotInfo::FoodPot);
	const auto totem = MakeAbode(k_Totem, false);
	const auto house = MakeAbode(k_House, false);
	const auto villager = registry.Create();
	registry.Assign<Villager>(villager);
	const ecs::effects::EffectValues values {};
	EXPECT_FALSE(ecs::effects::IsEffectReceiver(stone, values));
	EXPECT_FALSE(ecs::effects::IsEffectReceiver(seed, values));
	EXPECT_FALSE(ecs::effects::IsEffectReceiver(oneOff, values));
	EXPECT_TRUE(ecs::effects::IsEffectReceiver(full, values));
	EXPECT_FALSE(ecs::effects::IsEffectReceiver(empty, values));
	EXPECT_FALSE(ecs::effects::IsEffectReceiver(totem, values));
	EXPECT_TRUE(ecs::effects::IsEffectReceiver(house, values));
	EXPECT_TRUE(ecs::effects::IsEffectReceiver(villager, values));
}
} // namespace
