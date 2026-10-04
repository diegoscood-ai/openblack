/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// V11 (session Edificios): the building / town side of repairs and of the town emergency (spec
// dev\documentacion\edificios\repair_spec.md §2.2, §4.3, §4.4, §5, §7): Town::ProcessTownRepairs 0x747DE0 (the plans
// and the abodes sharing one best), the rock-damaged house's ordinary site (§5.4), Abode::MoveAbodeToPlannedAbodes
// 0x404520 / PlannedAbode::Create(Abode*) 0x405660 (+0x30), Town::SetInStateOfEmergency 0x7479A0 /
// ProcessTownEmergency 0x7477A0 (+0xF1C, the worship saved in +0xEC4) and Abode::ReduceLife's emergency for an unbuilt
// storage pit. No game data: a hand-made info.dat, abodes without meshes.

#include <memory>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Life.h"
#include "ECS/MapCoords.h"
#include "ECS/Registry.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownEmergency.h"
#include "ECS/Town/TownQueries.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/WorshipPercentage.h"

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
constexpr uint32_t k_TownId = 1;

class RepairsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto info = std::make_unique<InfoConstants>();
		auto& house = info->abode.at(static_cast<size_t>(k_House));
		house.abodeType = AbodeType::LivingQuarters;
		house.abodeNumber = AbodeNumber::A;
		house.tribeType = Tribe::CELTIC;
		house.maxVillagersInAbode = 4;
		house.desireToBeRepaired = 0.5f;
		house.maxVillagerNeededToBuild = 4;
		house.thresholdForStopBeingFunctional = 0.75f;
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
		info->town.thresholdToStartRepairing = 0.9f;
		info->town.gameTurnsAfterEmergencyVillagersReact = 1200;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
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

	/// A plan of the house's info; `rebuild` is +0x30
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

// ---- Town::ProcessTownRepairs 0x747DE0 ------------------------------------------------------------------------------

TEST_F(RepairsTest, RebuildPlanBeatsALessDamagedAbode)
{
	// plan +0x30: info +0x118 = 0.5; house at life 0.5 with someone in: ((1 - 0.5) x 0.5 + 0.5) x 0.5 = 0.375 < 0.5
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
	// life 0: (1 x 0.5 + 0.5) x 0.5 = 0.5 == the plan's 0.5: the plan stays (fcom; test ah, 0x41; jne)
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
	AddPlan(false); // +0x30 == 0: skipped
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
	// 0x747E6E: +0x58 |= 4 before AddBuildingSite, so the site copies it (0x43B85E): a repair site
	EXPECT_NE(Reg().Get<Abode>(pit).buildFlags & Abode::k_NotRepaired, 0u);
	EXPECT_TRUE(sites::IsRepairSite(site));
	// GetBestRepairBuildingSite 0x747EA0: GetDesireForVillagers (4 / 4) x 0.63 > 0
	EXPECT_EQ(sites::GetBestRepairBuildingSite(_town), site);
	// the next turn the abode has a site (0x747E39): nothing new
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

// ---- the rock-damaged house (repair_spec §2.2 correction, §5.4) -----------------------------------------------------

TEST_F(RepairsTest, RockSiteIsAnOrdinarySite)
{
	const auto house = MakeAbode(k_House, false, 1);
	const ecs::map_coords::MapCoords pos {0, 0, 0.0f};
	// Abode::ReduceLife 0x405D90: life 0.95 < 1 -> AddBuildingSite 0x405E7A; the site's +0x638 = bit 2 = 0
	EXPECT_FLOAT_EQ(abodes::ReduceLife(house, 0.05f, std::nullopt), 0.95f);
	const auto site = abodes::GetBuildingSite(house);
	ASSERT_TRUE(site != entt::null);
	EXPECT_FALSE(sites::IsRepairSite(site));
	EXPECT_NEAR(sites::GetRepairBase(site), 1.1f * 0.95f - 0.1f, 1e-6f); // 0x405EA7
	EXPECT_TRUE(sites::GetBestRepairBuildingSite(_town) == entt::null);
	// life > 0.9: GetDesireToBeRepaired 0 -> no builders needed (0x43BC2C); only a BUILDER disciple (includeFull,
	// 0x73CFA2) takes it
	EXPECT_FALSE(sites::NeedsBuilders(site));
	EXPECT_TRUE(sites::GetBestBuildingSite(_town, pos, false) == entt::null);
	EXPECT_EQ(sites::GetBestBuildingSite(_town, pos, true), site);
	// life 0.85 <= 0.9: the ordinary builders (GetBestBuildingSite 0x73CF60, no +0x638 test)
	abodes::ReduceLife(house, 0.1f, std::nullopt);
	EXPECT_TRUE(sites::NeedsBuilders(site));
	EXPECT_EQ(sites::GetBestBuildingSite(_town, pos, false), site);
	// still never the repair desire's site, and ProcessTownRepairs skips an abode with a site (0x747E39)
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
	EXPECT_EQ(abodes::GetDesireToBeRepaired(house), 0.0f); // 0x406993..0x4069A7: an empty house
	EXPECT_FALSE(sites::NeedsBuilders(site));
}

// ---- Abode::MoveAbodeToPlannedAbodes 0x404520 / PlannedAbode::Create(Abode*) 0x405660 ------------------------------

TEST_F(RepairsTest, DestroyedBuiltAbodeLeavesARebuildPlan)
{
	game_clock::SetTurn(77);
	const auto house = MakeAbode(k_House, false, 1);
	Reg().Assign<Transform>(house, glm::vec3(100.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_TRUE(abodes::MoveAbodeToPlannedAbodes(house));
	ASSERT_EQ(plans::PlansOf(_town), 1u);
	const auto& plan = TownData().plannedAbodes.front();
	EXPECT_TRUE(plan.wasBuilt); // 0x6488B4: +0x30 = +0x58 & 8
	EXPECT_FALSE(plan.townCentre);
	EXPECT_EQ(plan.info, k_House);
	EXPECT_EQ(plan.position, glm::vec3(100.0f, 0.0f, 50.0f));
	EXPECT_FLOAT_EQ(plan.yAngleRadians, 0.0f);
	EXPECT_FLOAT_EQ(plan.scale, 1.0f);
	EXPECT_EQ(plan.creationTurn, 77u);
	EXPECT_FLOAT_EQ(plans::GetDesireToBeRepaired(_town, 0), 0.5f); // 0x648910: +0x30 -> info +0x118
	// ProcessTownRepairs can now pick it (its conversion sets bit 2: V6's CreatePlannedNoFixedCheck 0x4057CC)
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

TEST_F(RepairsTest, NoTownNoPlan)
{
	const auto house = MakeAbode(k_House, false, 0, 99); // town 99 does not exist: GetTown 0 -> return 0
	Reg().Assign<Transform>(house, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_FALSE(abodes::MoveAbodeToPlannedAbodes(house));
	EXPECT_EQ(plans::PlansOf(_town), 0u);
}

// ---- the town emergency (repair_spec §7) ----------------------------------------------------------------------------

TEST_F(RepairsTest, SetInStateOfEmergencyStartsAndRefreshes)
{
	EXPECT_FALSE(ecs::town_queries::IsInStateOfEmergency(TownData()));
	game_clock::SetTurn(10);
	emergency::SetInStateOfEmergency(_town);
	EXPECT_EQ(TownData().emergencyStartTurn, 10u);
	EXPECT_TRUE(ecs::town_queries::IsInStateOfEmergency(TownData()));
	game_clock::SetTurn(30);
	emergency::SetInStateOfEmergency(_town); // 0x7479C9..0x7479D5: refreshed
	EXPECT_EQ(TownData().emergencyStartTurn, 30u);
	game_clock::SetTurn(1229);
	EXPECT_TRUE(ecs::town_queries::IsInStateOfEmergency(TownData())); // 1199 < 1200
	game_clock::SetTurn(1230);
	EXPECT_FALSE(ecs::town_queries::IsInStateOfEmergency(TownData())); // 1200: jae
}

TEST_F(RepairsTest, ProcessTownEmergencySavesAndRestoresTheWorship)
{
	Magic().worshipPercentage = 0.4f;
	game_clock::SetTurn(10);
	emergency::SetInStateOfEmergency(_town);
	// in the emergency: +0x5C0 != 0 -> +0xEC4 = it, SetWorshipPercentage(0)
	game_clock::SetTurn(20);
	emergency::ProcessTownEmergency(_town);
	EXPECT_FLOAT_EQ(TownData().savedWorshipPercentage, 0.4f);
	EXPECT_EQ(worship::percentage::GetWorshipPercentage(_town), 0.0f);
	EXPECT_EQ(TownData().emergencyStartTurn, 10u);
	// attacked this turn (+0xEB0 == turn): refreshed; the saved value is kept (+0x5C0 is 0 now)
	game_clock::SetTurn(50);
	TownData().aggressorTurn = 50;
	emergency::ProcessTownEmergency(_town);
	EXPECT_EQ(TownData().emergencyStartTurn, 50u);
	EXPECT_FLOAT_EQ(TownData().savedWorshipPercentage, 0.4f);
	// still in it at 1249 (1199 turns)
	game_clock::SetTurn(1249);
	emergency::ProcessTownEmergency(_town);
	EXPECT_EQ(TownData().emergencyStartTurn, 50u);
	// over: no fire -> the worship comes back, +0xEC4 = +0xF1C = 0
	game_clock::SetTurn(1250);
	emergency::ProcessTownEmergency(_town);
	EXPECT_FLOAT_EQ(worship::percentage::GetWorshipPercentage(_town), 0.4f);
	EXPECT_EQ(TownData().savedWorshipPercentage, 0.0f);
	EXPECT_EQ(TownData().emergencyStartTurn, 0u);
}

TEST_F(RepairsTest, RestoreOnlyWhenTheWorshipIsZero)
{
	// out of the emergency with a saved value but a current percentage != 0: not restored, both cleared
	Magic().worshipPercentage = 0.2f;
	TownData().savedWorshipPercentage = 0.6f;
	TownData().emergencyStartTurn = 0;
	game_clock::SetTurn(5);
	emergency::ProcessTownEmergency(_town);
	EXPECT_FLOAT_EQ(worship::percentage::GetWorshipPercentage(_town), 0.2f);
	EXPECT_EQ(TownData().savedWorshipPercentage, 0.0f);
}

TEST_F(RepairsTest, UnbuiltStoragePitAtZeroStartsTheEmergency)
{
	// repair_spec §5.4: a pit under construction whose percent reaches 0 drops its life 1 -> 0 (0x52F659), crossing
	// the 0.75 threshold: StopBeingFunctional and SetInStateOfEmergency (0x405E07..0x405E3E)
	game_clock::SetTurn(100);
	const auto pit = MakeAbode(k_Pit, true);
	abodes::SetPercentBuilt(pit, 0.1f);
	EXPECT_EQ(abodes::ReduceLife(pit, 0.2f, std::nullopt), 0.0f);
	EXPECT_EQ(abodes::GetPercentBuilt(pit), 0.0f);
	EXPECT_EQ(TownData().emergencyStartTurn, 100u);
	EXPECT_TRUE(ecs::town_queries::IsInStateOfEmergency(TownData()));
	// a house does not cause it (CausesTownEmergencyIfDamaged: Abode 0x4016F0 = 0)
	TownData().emergencyStartTurn = 0;
	const auto house = MakeAbode(k_House, false, 1);
	abodes::ReduceLife(house, 0.5f, std::nullopt);
	EXPECT_EQ(TownData().emergencyStartTurn, 0u);
}

TEST_F(RepairsTest, DamagedPitAboveTheThresholdNoEmergency)
{
	game_clock::SetTurn(7);
	const auto pit = MakeAbode(k_Pit, false);
	abodes::ReduceLife(pit, 0.125f, std::nullopt); // 0.875 > 0.75: still functional
	EXPECT_EQ(TownData().emergencyStartTurn, 0u);
	abodes::ReduceLife(pit, 0.125f, std::nullopt); // 0.75: 0.75 >= l -> stops, emergency (test ah, 1)
	EXPECT_EQ(TownData().emergencyStartTurn, 7u);
}

TEST_F(RepairsTest, TownCentreStopBeingFunctionalStopsTheWorship)
{
	// TownCentre::StopBeingFunctional 0x744A00: GetTown()->SetWorshipPercentage(0)
	Magic().worshipPercentage = 0.5f;
	const auto centre = MakeAbode(k_Centre, false);
	abodes::StopBeingFunctional(centre, std::nullopt);
	EXPECT_EQ(worship::percentage::GetWorshipPercentage(_town), 0.0f);
}

TEST_F(RepairsTest, FieldLosesNoLife)
{
	// a field carries an Abode too; its vt +0x5B8 is Field::ReduceLife 0x52A0A0 = GetLife: no change, no site
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
	// Town::UpdateAggressor 0x73C9B0: no caused player -> the neutral one (0x73C9C7); +0xEB0 = the turn (0x73CA98)
	game_clock::SetTurn(40);
	emergency::UpdateAggressor(_town, std::nullopt);
	EXPECT_EQ(TownData().aggressor, PlayerNames::NEUTRAL);
	EXPECT_EQ(TownData().aggressorTurn, 40u);
	// ProcessTownEmergency 0x7477AE: attacked this turn -> refreshed
	emergency::ProcessTownEmergency(_town);
	EXPECT_EQ(TownData().emergencyStartTurn, 40u);
	emergency::UpdateAggressor(_town, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(TownData().aggressor, PlayerNames::PLAYER_ONE);
}
} // namespace
