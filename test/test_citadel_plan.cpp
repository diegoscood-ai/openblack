/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The citadel as a plan (session Edificios, spec dev\documentacion\edificios\citadel_plan_spec.md): the
// PlannedTownCitadelHeart 0x467DD0 (a plan only), BUILD_BUILDING's ForceBuildingOfPlannedAtPos 0x73E560 converting it
// at once (CreatePlannedNoFixedCheck 0x467EF0: the heart at 0 %, in the map cells, found by GetNearestCitadel as
// CALL_NEAR(CITADEL) right after it in Land 1), the CitadelBuildingSite's six piles (CreatePilesOfWood 0x43D2A0 /
// GetResourcePosAndYAngle 0x43D470), AddResource 0x43D360 and CitadelHeart::Built 0x465000. No game data: a hand-made
// info.dat, no island, no meshes.

#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "ECS/Abodes.h"
#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/GUtilsAngle.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/Town/BuildingSites.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/Citadel.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace sites = openblack::ecs::building_sites;
namespace plans = openblack::ecs::plans;
namespace abodes = openblack::ecs::abodes;
namespace map_coords = openblack::ecs::map_coords;
using openblack::ecs::archetypes::CitadelArchetype;

namespace
{
constexpr uint32_t k_TownId = 1;
/// Land 1's temple (FollowUs L52474 BUILD_BUILDING((1915.05, 0, 2508.89), 1.0))
const glm::vec3 k_TemplePos {1915.05f, 0.0f, 2508.89f};

class CitadelPlanTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the conversion and HeartBuilt log at info level (SPDLOG_LOGGER_INFO needs the logger)
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		auto info = std::make_unique<InfoConstants>();
		// "Citadel Heart" (citadel_plan_spec §4.3): MaxVillagerNeededToBuild 100, WoodValue 5, DesireToBeBuilt 1.0,
		// DesireToBeRepaired 5.0, StartLife 1.0
		auto& heart = info->citadelHeart;
		heart.maxVillagerNeededToBuild = 100;
		heart.woodValue = 5;
		heart.desireToBeBuilt = 1.0f;
		heart.desireToBeRepaired = 5.0f;
		heart.startLife = 1.0f;
		auto& wood = info->pot.at(static_cast<size_t>(PotInfo::MagicWood));
		wood.potType = PotType::PileWood;
		wood.resourceType = ResourceType::Wood;
		wood.maxAmountInPot = 1000;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		auto& registry = Reg();
		_town = registry.Create();
		const auto townPos = k_TemplePos + glm::vec3(100.0f, 0.0f, 0.0f);
		registry.Assign<Transform>(_town, townPos, glm::mat3(1.0f), glm::vec3(1.0f));
		auto& town = registry.Assign<Town>(_town);
		town.id = k_TownId;
		town.owner = PlayerNames::PLAYER_ONE;
		registry.Context().towns[k_TownId] = _town;
	}

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	Town& TownData() { return Reg().Get<Town>(_town); }

	/// The one temple entity (components::CitadelHeart), or null
	static entt::entity TheHeart()
	{
		entt::entity found = entt::null;
		Reg().Each<const CitadelHeart>([&found](entt::entity e, const CitadelHeart&) { found = e; });
		return found;
	}

	/// CREATE_PLANNED_CITADEL then BUILD_BUILDING at the same point (desire 1.0 x 5)
	entt::entity PlanAndBuild()
	{
		CitadelArchetype::CreatePlan(_town, k_TemplePos, 0, 0.0f, 1.0f);
		sites::ForceBuildingOfPlannedAtPos(map_coords::FromMetres(glm::vec2(k_TemplePos.x, k_TemplePos.z)), 5.0f);
		return TheHeart();
	}

	entt::entity _town {entt::null};
};

// ---- the plan 0x467DD0 ----------------------------------------------------------------------------------------------

TEST_F(CitadelPlanTest, PlanIsOnlyAPlan)
{
	CitadelArchetype::CreatePlan(_town, k_TemplePos, 0, 0.5f, 1.25f);
	ASSERT_EQ(TownData().plannedAbodes.size(), 1u);
	const auto& plan = TownData().plannedAbodes.front();
	EXPECT_TRUE(plan.citadelHeart);
	EXPECT_FLOAT_EQ(plan.yAngleRadians, 0.5f);
	EXPECT_FLOAT_EQ(plan.scale, 1.25f);
	EXPECT_FALSE(plan.wasBuilt);
	// no temple, nothing drawn, nothing in the map cells, no citadel for the player
	EXPECT_EQ(Reg().Size<Temple>(), 0u);
	EXPECT_TRUE(TheHeart() == entt::null);
	EXPECT_TRUE(worship::citadel::Of(PlayerNames::PLAYER_ONE) == entt::null);
	// GetAbodeType 0x467E30 = 0x804, IsCivic 0x467E10 = 0, GetDesireToBeRepaired 0x648910 = 0 (+0x30 clear)
	EXPECT_EQ(plans::GetAbodeType(_town, 0), AbodeType::Citadel);
	EXPECT_FALSE(plans::IsCivic(_town, 0));
	EXPECT_EQ(plans::GetDesireToBeRepaired(_town, 0), 0.0f);
	// GetBestPlanned(mask 4) takes it: 0x804's default case, b = DesireToBeBuilt 1.0, no site of the type
	float best = 0.0f;
	const auto chosen = plans::GetBestPlanned(_town, best, 4);
	ASSERT_TRUE(chosen.has_value());
	EXPECT_EQ(*chosen, 0u);
	EXPECT_FLOAT_EQ(best, 1.0f);
}

TEST(CitadelFlattening, LandOneCellsAtSeventyMetres)
{
	// Land 1's heart cell (191, 250) is at 44; the axis cells at 7 cells (d = 70 exactly) keep their altitude: under
	// the 24-bit x87, (70 - 35) x 0.0285714f is exactly 1, so a x 1 + 0 x 44 = a (in double they came out one unit low)
	EXPECT_EQ(CitadelArchetype::FlattenedAltitude(-7, 0, 18, 44), 18);
	EXPECT_EQ(CitadelArchetype::FlattenedAltitude(0, -7, 22, 44), 22);
	EXPECT_EQ(CitadelArchetype::FlattenedAltitude(0, 7, 31, 44), 31);
	// within 35 m the centre's altitude, beyond 70 m the cell's own
	EXPECT_EQ(CitadelArchetype::FlattenedAltitude(3, 0, 10, 44), 44);
	EXPECT_EQ(CitadelArchetype::FlattenedAltitude(8, 0, 10, 44), 10);
}

TEST_F(CitadelPlanTest, PlanNeedsATown)
{
	CitadelArchetype::CreatePlan(entt::null, k_TemplePos, 0, 0.0f, 1.0f);
	EXPECT_TRUE(TownData().plannedAbodes.empty());
}

// ---- BUILD_BUILDING: the conversion 0x467EF0, synchronous -----------------------------------------------------------

TEST_F(CitadelPlanTest, BuildBuildingConvertsAtOnce)
{
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	// the plan is gone, the heart is the player's citadel (0x462B10 with it), under construction at 0 %
	EXPECT_TRUE(TownData().plannedAbodes.empty());
	EXPECT_EQ(worship::citadel::Of(PlayerNames::PLAYER_ONE), heart);
	EXPECT_EQ(Reg().Get<const Temple>(heart).owner, PlayerNames::PLAYER_ONE);
	const auto& part = Reg().Get<const CitadelPartBuild>(heart);
	EXPECT_EQ(part.buildFlags, CitadelPartBuild::k_UnderConstruction);
	EXPECT_EQ(part.percentBuilt, 0.0f);
	EXPECT_FALSE(abodes::IsBuilt(heart));
	EXPECT_EQ(abodes::GetBuiltPercentage(heart).value_or(-1.0f), 0.0f);
	// SetLife(StartLife) 0x469452; heart +0x94 = the plan's town
	EXPECT_EQ(ecs::life::LifeOf(heart), 1.0f);
	EXPECT_EQ(Reg().Get<const CitadelHeart>(heart).town, _town);
	// in the map cells (InsertMapObject 0x46776C) and found as the player's citadel near the point, as Land 1's
	// CALL_NEAR(CITADEL, 5000, pos, 5.0) at L52490 must
	EXPECT_TRUE(ecs::map_cells::IsObjectInMap(heart));
	EXPECT_EQ(ecs::map_cells::GetNearestCitadel(ecs::object::MapCoordsOf(heart), 5.0f), heart);
	// at 0 % nothing of the temple is drawn (openblack: NotDrawn); the entrance (Draw is a `ret`) is not in the cells
	EXPECT_TRUE(Reg().AllOf<NotDrawn>(heart));
	const auto entrance = Reg().Get<const CitadelHeart>(heart).entrance;
	ASSERT_TRUE(entrance != entt::null);
	EXPECT_EQ(Reg().Get<const CitadelEntrance>(entrance).heart, heart);
	EXPECT_FALSE(ecs::map_cells::IsObjectInMap(entrance));
}

TEST_F(CitadelPlanTest, ConversionMakesTheCitadelBuildingSite)
{
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	ASSERT_EQ(TownData().buildingSites.size(), 1u);
	const auto site = TownData().buildingSites.front();
	EXPECT_EQ(Reg().Get<const CitadelPartBuild>(heart).buildingSite, site);
	EXPECT_EQ(abodes::GetBuildingSite(heart), site);
	ASSERT_TRUE(Reg().AllOf<CitadelBuildingSite>(site));
	// ForceBuildingOfPlannedAtPos: +0x63C = desire x 5; not a repair site (+0x30 clear)
	EXPECT_FLOAT_EQ(Reg().Get<const BuildingSite>(site).desireBoost, 5.0f);
	EXPECT_FALSE(sites::IsRepairSite(site));
	// GetTown 0x43C0B0 -> the heart's MultiMapFixed::GetTown 0x4220A0 = 0
	EXPECT_TRUE(sites::GetTown(site) == entt::null);
	// the info's MaxVillagerNeededToBuild; the wood value 5 x GetScale (the plan's 1.0) / the neutral TribalPower[5]
	// (1)
	EXPECT_EQ(sites::GetMaxBuilders(site), 100);
	EXPECT_FLOAT_EQ(sites::GetWoodValue(site), 5.0f);
}

// ---- the six piles: CreatePilesOfWood 0x43D2A0 ----------------------------------------------------------------------

TEST_F(CitadelPlanTest, SixEmptyPilesTwentyTwoMetresOut)
{
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	const auto site = TownData().buildingSites.front();
	const auto& piles = Reg().Get<const CitadelBuildingSite>(site).piles;
	const auto root = ecs::object::MapCoordsOf(heart);
	for (size_t i = 0; i < piles.size(); ++i)
	{
		const auto pile = piles.at(i);
		ASSERT_TRUE(pile != entt::null) << i;
		ASSERT_TRUE(Reg().Valid(pile)) << i;
		EXPECT_EQ(Reg().Get<const Pot>(pile).amount, 0u) << i;
		EXPECT_TRUE(sites::IsLinkedToThisBuildingSite(site, pile)) << i;
		EXPECT_EQ(sites::SiteOfPile(pile), site) << i;
		// heart angle 0 + i x 2 pi / 7 - 1.1424, 22 m from the heart's MapCoords (GetPosFromAngle 0x74D580)
		const auto a = static_cast<float>(static_cast<double>(i) * static_cast<double>(0.8975979f) -
		                                  static_cast<double>(1.1424f));
		const auto expected = map_coords::ToMetres(root + gutils::GetPosFromAngle(a, 22.0f));
		const auto& at = Reg().Get<const Transform>(pile).position;
		EXPECT_NEAR(at.x, expected.x, 1e-3f) << i;
		EXPECT_NEAR(at.z, expected.y, 1e-3f) << i;
		EXPECT_NEAR(glm::length(glm::vec2(at.x - k_TemplePos.x, at.z - k_TemplePos.z)), 22.0f, 0.01f) << i;
	}
	EXPECT_EQ(sites::GetResource(site, ResourceType::Wood), 0u);
}

TEST_F(CitadelPlanTest, AddResourceGoesToTheNearestPile)
{
	PlanAndBuild();
	const auto site = TownData().buildingSites.front();
	const auto& piles = Reg().Get<const CitadelBuildingSite>(site).piles;
	// 0x43D360: pos NULL adds nothing (GScript's ADD_RESOURCE passes none)
	EXPECT_EQ(sites::AddResource(site, ResourceType::Wood, 10, nullptr), 0u);
	EXPECT_EQ(sites::GetResource(site, ResourceType::Wood), 0u);
	// at slot 3's position: that pile takes it
	const auto at = ecs::object::MapCoordsOf(piles.at(3));
	EXPECT_EQ(sites::GetPileWood(site, &at), piles.at(3));
	EXPECT_EQ(sites::AddResource(site, ResourceType::Wood, 10, &at), 10u);
	EXPECT_EQ(Reg().Get<const Pot>(piles.at(3)).amount, 10u);
	EXPECT_EQ(sites::GetResource(site, ResourceType::Wood), 10u);
	// the town's +0x710 is not moved: GetTown is 0 (the stats count only the sites of the town)
	EXPECT_EQ(sites::GetWoodForStats(site), 10u);
	// RemoveResource without an interface: the slots in order (0x43D410)
	EXPECT_EQ(sites::RemoveResource(site, ResourceType::Wood, 4), 4u);
	EXPECT_EQ(Reg().Get<const Pot>(piles.at(3)).amount, 6u);
}

// ---- CitadelHeart::Built 0x465000 -----------------------------------------------------------------------------------

TEST_F(CitadelPlanTest, SetPropertyBuiltPercentageAndBuilt)
{
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	const auto site = TownData().buildingSites.front();
	// Land 1: SET_PROPERTY(22, 0.375) (fn_52EDD0), then Citadel::Process copies it into the 3D object's percent
	EXPECT_TRUE(abodes::SetBuiltPercentage(heart, 0.375f));
	EXPECT_FLOAT_EQ(*abodes::GetBuiltPercentage(heart), 0.375f);
	EXPECT_FALSE(abodes::IsBuilt(heart));
	worship::citadel::Process(heart);
	EXPECT_FLOAT_EQ(Reg().Get<const CitadelHeart>(heart).drawPercent, 0.375f);
	// the builders finish it: BuildBy 0x52ED40 up to 1 -> Built
	abodes::BuildBy(heart, 0.7f);
	EXPECT_TRUE(abodes::IsBuilt(heart));
	const auto& part = Reg().Get<const CitadelPartBuild>(heart);
	EXPECT_EQ(part.buildFlags & CitadelPartBuild::k_Built, CitadelPartBuild::k_Built);
	EXPECT_EQ(part.buildFlags & CitadelPartBuild::k_UnderConstruction, 0u);
	EXPECT_EQ(part.percentBuilt, 1.0f);
	// MultiMapFixed::Built: the site deleted (out of the town, its empty piles gone); SetLife(1.0)
	EXPECT_TRUE(part.buildingSite == entt::null);
	EXPECT_TRUE(TownData().buildingSites.empty());
	EXPECT_FALSE(sites::IsAvailable(site));
	EXPECT_EQ(ecs::life::LifeOf(heart), 1.0f);
	// the next Citadel::Process: the whole temple again
	worship::citadel::Process(heart);
	EXPECT_EQ(Reg().Get<const CitadelHeart>(heart).drawPercent, 1.0f);
	EXPECT_FALSE(Reg().AllOf<NotDrawn>(heart));
	EXPECT_TRUE(ecs::map_cells::IsObjectInMap(heart));
}

// ---- CREATE_CITADEL 0x463240 ----------------------------------------------------------------------------------------

TEST_F(CitadelPlanTest, CreateCitadelIsBuilt)
{
	const auto heart = CitadelArchetype::Create(k_TemplePos, PlayerNames::PLAYER_ONE, glm::mat4(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(heart != entt::null);
	EXPECT_TRUE(abodes::IsBuilt(heart));
	EXPECT_EQ(Reg().Get<const CitadelPartBuild>(heart).buildFlags, CitadelPartBuild::k_Built);
	EXPECT_TRUE(abodes::GetBuildingSite(heart) == entt::null);
	EXPECT_FALSE(Reg().AllOf<NotDrawn>(heart));
	EXPECT_TRUE(worship::citadel::HasLivingHeart(heart));
}

// ---- SET_INTERFACE_CITADEL 414 / the entrance's tap 0x468F50 --------------------------------------------------------

TEST_F(CitadelPlanTest, BuiltPercentagePropertyOfTheHeart)
{
	// GET_PROPERTY 22 (0x70E1A9: a MultiMapFixed's GetPercentBuilt) as Land 1's CheckCitadel (L53118) reads it
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	EXPECT_EQ(abodes::GetBuiltPercentage(heart).value_or(-1.0f), 0.0f);
	abodes::BuildBy(heart, 0.25f);
	EXPECT_FLOAT_EQ(abodes::GetBuiltPercentage(heart).value_or(-1.0f), 0.25f);
	EXPECT_TRUE(abodes::SetBuiltPercentage(heart, 0.375f));
	EXPECT_FLOAT_EQ(abodes::GetBuiltPercentage(heart).value_or(-1.0f), 0.375f);
}

TEST_F(CitadelPlanTest, EntranceTapRegisteredOnce)
{
	PlanAndBuild();
	const auto handlers = ecs::hand_tap::Handlers().size();
	CitadelArchetype::Create(k_TemplePos + glm::vec3(300.0f, 0.0f, 0.0f), PlayerNames::PLAYER_TWO, glm::mat4(1.0f),
	                         glm::vec3(1.0f));
	EXPECT_EQ(ecs::hand_tap::Handlers().size(), handlers);
	const auto entrance = Reg().Get<const CitadelHeart>(TheHeart()).entrance;
	EXPECT_NE(ecs::hand_tap::Find(entrance), nullptr);
}

TEST_F(CitadelPlanTest, InterfaceCitadelGatesTheEntrance)
{
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	const auto entrance = Reg().Get<const CitadelHeart>(heart).entrance;
	worship::citadel::ResetInterfaceCitadel(); // GScript::Reset 0x6EB312: 1
	EXPECT_TRUE(worship::citadel::EntranceValidToTap(entrance));
	worship::citadel::SetInterfaceCitadel(0); // Land 1 L52501
	EXPECT_FALSE(worship::citadel::EntranceValidToTap(entrance));
	EXPECT_EQ(worship::citadel::EntranceTap(entrance, true), 1u);
	worship::citadel::ResetInterfaceCitadel();
}
} // namespace
