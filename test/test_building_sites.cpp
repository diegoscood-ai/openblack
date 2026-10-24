/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The building side of plans and building sites:
// GetDesireToBeBuilt, BuildBy / Built / Abode's MakeFunctional,
// the repair branch, BuildingSite's GetNearestEdge index, GetPercentForDrawBuilding, the builders'
// count and the pruning. And a scaffold let go of by the hand: put down gently or not, and the puff where it cannot
// build. No game data: a hand-made info.dat, abodes without meshes (no ring, no partly built model).

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/AxisAlignedBoundingBox.h"
#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "Audio/AudioManagerNoOp.h"
#include "Audio/Game/BankTables.h"
#include "ECS/Abodes.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/LastDropped.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Workshop.h"
#include "ECS/LastDropped.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectResources.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Scaffolds.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/Graveyard.h"
#include "ECS/Town/ScaffoldPlans.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/Workshops.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/Resources.h"
#include "ScriptHeaders/ScriptPropertyRules.h"
#include "support/MapFakes.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace sites = openblack::ecs::building_sites;
namespace plans = openblack::ecs::plans;
namespace abodes = openblack::ecs::abodes;
namespace workshops = openblack::ecs::workshops;

namespace
{
constexpr auto k_House = static_cast<AbodeInfo>(0);
constexpr auto k_Pit = static_cast<AbodeInfo>(1);
constexpr auto k_Graveyard = static_cast<AbodeInfo>(2);
constexpr auto k_Football = static_cast<AbodeInfo>(3);
constexpr auto k_Workshop = static_cast<AbodeInfo>(4);
constexpr auto k_TownCentre = static_cast<AbodeInfo>(5);
constexpr uint32_t k_TownId = 1;

class BuildingSitesTest: public ::testing::Test
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
		house.desireToBeBuilt = 0.5f;
		house.desireToBeRepaired = 0.5f;
		house.scaffoldsRequired = 2;
		house.maxVillagerNeededToBuild = 4;
		house.thresholdForStopBeingFunctional = 0.75f;
		auto& pit = info->abode.at(static_cast<size_t>(k_Pit));
		pit.abodeType = AbodeType::StoragePit;
		pit.abodeNumber = AbodeNumber::StoragePit;
		pit.tribeType = Tribe::CELTIC;
		pit.desireToBeBuilt = 0.7f;
		auto& graveyard = info->abode.at(static_cast<size_t>(k_Graveyard));
		graveyard.abodeType = AbodeType::Graveyard;
		graveyard.abodeNumber = AbodeNumber::Graveyard;
		graveyard.tribeType = Tribe::CELTIC;
		graveyard.desireToBeBuilt = 0.6f;
		auto& football = info->abode.at(static_cast<size_t>(k_Football));
		football.abodeType = AbodeType::FootballPitch;
		football.abodeNumber = AbodeNumber::FootballPitch;
		football.tribeType = Tribe::CELTIC;
		football.desireToBeBuilt = 0.9f;
		auto& workshop = info->abode.at(static_cast<size_t>(k_Workshop));
		workshop.abodeType = AbodeType::Workshop;
		workshop.abodeNumber = AbodeNumber::Workshop;
		workshop.tribeType = Tribe::CELTIC;
		workshop.desireToBeBuilt = 0.8f;
		auto& centre = info->abode.at(static_cast<size_t>(k_TownCentre));
		centre.abodeType = AbodeType::TownCentre;
		centre.abodeNumber = AbodeNumber::TownCentre;
		centre.tribeType = Tribe::CELTIC;
		centre.desireToBeBuilt = 0.65f;
		info->pot.at(static_cast<size_t>(PotInfo::WoodPile_1)).resourceType = ResourceType::Wood;
		info->town.thresholdToStartRepairing = 0.9f;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		auto& registry = Reg();
		_town = registry.Create();
		auto& town = registry.Assign<Town>(_town);
		town.id = k_TownId;
		town.owner = PlayerNames::PLAYER_ONE;
		registry.Context().towns[k_TownId] = _town;
	}

	void TearDown() override
	{
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

	/// An abode of the town without a mesh; under construction as the plans make them (AbodeArchetype::Create(...,
	/// true))
	entt::entity MakeAbode(AbodeInfo info, bool underConstruction)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		auto& abode = registry.Assign<Abode>(e, Info(info).abodeNumber, k_TownId, 0u, 0u);
		abode.info = info;
		if (underConstruction)
		{
			abode.buildFlags = Abode::k_UnderConstruction;
			abode.percentBuilt = 0.0f;
			abode.addedToTownStats = false;
		}
		return e;
	}

	/// A player of the type (1 a human, 2 a computer player) on its own entity, as the player archetype makes it
	static entt::entity MakePlayer(PlayerNames name, int32_t type)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		registry.Assign<Player>(e, name);
		registry.Assign<PlayerMagic>(e).playerType = type;
		return e;
	}

	/// A scaffold of 4 where the test puts it, that offers a house (2 needed) and was put down gently
	entt::entity MakeGentleHouseScaffoldOfFour()
	{
		const auto scaffold = ecs::scaffolds::Create(map_coords::FromWorld(nullptr, glm::vec3(10.0f, 0.0f, 10.0f)), _town,
		                                             entt::null, 0.0f, 1.0f);
		ecs::scaffolds::SetValue(scaffold, 4);
		auto& c = Reg().Get<Scaffold>(scaffold);
		c.plan = ScaffoldPlan {.info = &Info(k_House)};
		c.flags |= Scaffold::k_DroppedGently;
		return scaffold;
	}

	/// The workshop and scaffold numbers of info.dat: a scaffold's wood 2500, 200 game turns to make one, 150 turns a
	/// placed scaffold can still be adjusted. The workshop record's own maximum is set to 5: the yard's three is not
	/// read from it
	static void UseWorkshopInfo()
	{
		auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
		info->scaffold.woodValue = 2500;
		info->scaffold.gameTurnsAfterPlacingCanStillPickUp = 150;
		auto& workshop = info->abode.at(static_cast<size_t>(k_Workshop));
		workshop.timeEachMobileObjectTakesToProduce = 200.0f;
		workshop.maxNumMobileObjectsToProduce = 5.0f;
		Locator::infoConstants::reset(info.release());
	}

	/// A finished workshop standing at `at`, with no mesh (so none of its special points) and no wood pile; of the
	/// town, or of none
	entt::entity MakeWorkshop(const glm::vec3& at, bool ofTheTown = true)
	{
		const auto workshop = MakeAbode(k_Workshop, false);
		if (!ofTheTown)
		{
			Reg().Get<Abode>(workshop).townId = Abode::k_NoTown;
		}
		Reg().Assign<Transform>(workshop, at, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<Workshop>(workshop);
		return workshop;
	}

	/// A scaffold of one the workshop made, waiting at its yard place `slot`
	entt::entity MakeYardScaffold(entt::entity workshop, int32_t slot)
	{
		const auto at = glm::vec3(5.0f + static_cast<float>(slot), 0.0f, 5.0f);
		const auto scaffold = ecs::scaffolds::Create(map_coords::FromWorld(nullptr, at), _town, workshop, 0.0f, 1.0f);
		ecs::scaffolds::SetValue(scaffold, 1);
		workshops::AddScaffold(workshop, scaffold);
		ecs::scaffolds::SetSlot(scaffold, slot);
		Reg().Get<Workshop>(workshop).slots.at(static_cast<size_t>(slot)) = Workshop::k_SlotOccupied;
		return scaffold;
	}

	/// A built house of the town of key `townId` standing at `at`, in the map cells
	entt::entity MakeBuildingAt(uint32_t townId, const glm::vec3& at)
	{
		const auto house = MakeAbode(k_House, false);
		Reg().Get<Abode>(house).townId = townId;
		Reg().Assign<Transform>(house, at, glm::mat3(1.0f), glm::vec3(1.0f));
		ecs::map_cells::InsertMapObject(house);
		return house;
	}

	/// Another town, of key `id` and of `owner`, standing at `at`
	static entt::entity MakeTownAt(uint32_t id, PlayerNames owner, const glm::vec3& at)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		auto& town = registry.Assign<Town>(e);
		town.id = id;
		town.owner = owner;
		registry.Assign<Transform>(e, at, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Context().towns[id] = e;
		return e;
	}

	/// Every scaffold, in the registry's order
	static std::vector<entt::entity> Scaffolds()
	{
		std::vector<entt::entity> all;
		Reg().Each<const Scaffold>([&all](entt::entity e, const Scaffold&) { all.push_back(e); });
		return all;
	}

	entt::entity _town {entt::null};
};

// ---- GetDesireToBeBuilt ---------------------------------------------------------------------------------------------

TEST_F(BuildingSitesTest, HouseNotWantedWithEnoughFreePlaces)
{
	// free (the free adult places - homeless) = two empty houses of 4 = 8 > u = (0 + 0) / 10 + 1 = 1 and n == 0 -> b = 0
	MakeAbode(k_House, false);
	MakeAbode(k_House, false);
	EXPECT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_House), 0), 0.0f);
}

TEST_F(BuildingSitesTest, HouseKeepsItsDesireWithFewFreePlaces)
{
	// no counted house: 0 <= free <= u: b unchanged; m = 0 houses -> b - b / max(1, 10) x 0
	EXPECT_FLOAT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_House), 0), 0.5f);
}

TEST_F(BuildingSitesTest, HouseDesireReadsTheFreePlacesAsTheyStand)
{
	// the town turn's stats say no free place; its one house of 4 has 1 adult now: free 3 > u = 1 -> b = 0
	TownData().stats.freeAdultPlaces = 0;
	const auto house = MakeAbode(k_House, false);
	Reg().Get<Abode>(house).adultCount = 1;
	EXPECT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_House), 0), 0.0f);
	// three more adults move in, with no town turn between: free 0 <= u -> b = 0.5, less its share for the one house
	// counted as it stands (m = 1): 0.5 - 0.5 / 10 = 0.45
	Reg().Get<Abode>(house).adultCount = 4;
	EXPECT_FLOAT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_House), 0), 0.45f);
	// a house still being built adds no place and is not counted
	MakeAbode(k_House, true);
	EXPECT_FLOAT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_House), 0), 0.45f);
}

TEST_F(BuildingSitesTest, HouseShortage)
{
	// free = 0 - 3 homeless; u = 1; v = min(0.5 + 3, 0.8) = 0.8; q = min(3, 10) = 3 <= M = 4 -> r = 3 / 4 x 0.2 + 0.2 =
	// 0.35; b = (0.35 + 0.6) x 0.8 = 0.76; m = 2 houses -> b - b / max(3, 10) x 2 = 0.608
	// two full houses: no free place, and 2 houses of number A
	Reg().Get<Abode>(MakeAbode(k_House, false)).adultCount = 4;
	Reg().Get<Abode>(MakeAbode(k_House, false)).adultCount = 4;
	TownData().homelessVillagers = {Reg().Create(), Reg().Create(), Reg().Create()};
	EXPECT_NEAR(plans::GetDesireToBeBuilt(_town, Info(k_House), 0), 0.608f, 1e-6f);
}

TEST_F(BuildingSitesTest, HouseDesireCountsTheHousesAsTheyStand)
{
	// the town turn's stats say 5 houses of number A; none is counted now: free = 0 - 3 homeless, b = 0.76 (as
	// HouseShortage) and m = 0 keeps it
	TownData().stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::A)) = 5;
	TownData().homelessVillagers = {Reg().Create(), Reg().Create(), Reg().Create()};
	EXPECT_NEAR(plans::GetDesireToBeBuilt(_town, Info(k_House), 0), 0.76f, 1e-6f);
	// two full houses are finished, with no town turn between: m = 2 -> 0.608
	Reg().Get<Abode>(MakeAbode(k_House, false)).adultCount = 4;
	Reg().Get<Abode>(MakeAbode(k_House, false)).adultCount = 4;
	EXPECT_NEAR(plans::GetDesireToBeBuilt(_town, Info(k_House), 0), 0.608f, 1e-6f);
	// a house still being built is not counted
	MakeAbode(k_House, true);
	EXPECT_NEAR(plans::GetDesireToBeBuilt(_town, Info(k_House), 0), 0.608f, 1e-6f);
}

TEST_F(BuildingSitesTest, WorkshopDesireCountsTheWorkshopsAsTheyStand)
{
	// none counted (the turn's stats say one): the whole desire; one counted: half of it
	TownData().stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::Workshop)) = 1;
	EXPECT_FLOAT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_Workshop), 0), 0.8f);
	MakeAbode(k_Workshop, false);
	EXPECT_FLOAT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_Workshop), 0), 0.4f);
}

TEST_F(BuildingSitesTest, TheDesiresReadTheBuildingsAndRoomAsTheyStand)
{
	// the town turn's copy says 3 houses, 1 civic, 12 places, none free, and 7 adults
	auto stats = TownData().stats;
	stats.abodesWithPlaces = 3;
	stats.civicBuildings = 1;
	stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::A)) = 3;
	stats.adultPlaces = 12;
	stats.freeAdultPlaces = 0;
	stats.adults = 7;
	// now: one house of 4 with 1 adult, a storage pit, and a house still being built
	Reg().Get<Abode>(MakeAbode(k_House, false)).adultCount = 1;
	MakeAbode(k_Pit, false);
	MakeAbode(k_House, true);
	const auto now = ecs::town_stats::AsTheyStand(stats, _town);
	EXPECT_EQ(now.abodesWithPlaces, 1u);
	EXPECT_EQ(now.civicBuildings, 1u);
	EXPECT_EQ(now.abodesByNumber.at(static_cast<size_t>(AbodeNumber::A)), 1);
	EXPECT_EQ(now.abodesByNumber.at(static_cast<size_t>(AbodeNumber::StoragePit)), 1);
	EXPECT_EQ(now.adultPlaces, 4u);
	EXPECT_EQ(now.childPlaces, 0u);
	EXPECT_EQ(now.freeAdultPlaces, 3);
	// the villagers' counts are left as they are (kept live)
	EXPECT_EQ(now.adults, 7u);
	// at a town turn both agree
	const auto turn = ecs::town_stats::Compute(_town);
	const auto again = ecs::town_stats::AsTheyStand(turn, _town);
	EXPECT_EQ(again.abodesWithPlaces, turn.abodesWithPlaces);
	EXPECT_EQ(again.civicBuildings, turn.civicBuildings);
	EXPECT_EQ(again.abodesByNumber, turn.abodesByNumber);
	EXPECT_EQ(again.adultPlaces, turn.adultPlaces);
	EXPECT_EQ(again.freeAdultPlaces, turn.freeAdultPlaces);
}

TEST_F(BuildingSitesTest, TheWoodDesireReadsTheWoodAtTheSitesAsItStands)
{
	const auto site = sites::AddBuildingSite(_town, MakeAbode(k_House, true));
	ASSERT_TRUE(site != entt::null);
	// the site's pile holds 6 wood; the town turn's copy says none
	const auto pile = Reg().Create();
	Reg().Assign<Pot>(pile, static_cast<uint16_t>(6), static_cast<uint16_t>(100), PotInfo::WoodPile_1);
	Reg().Get<BuildingSite>(site).woodPile = pile;
	auto stats = TownData().stats;
	stats.woodAtSites = 0.0f;
	EXPECT_FLOAT_EQ(ecs::town_stats::WoodAtSites(_town), 6.0f);
	EXPECT_FLOAT_EQ(ecs::town_stats::AsTheyStand(stats, _town).woodAtSites, 6.0f);
	// three more are brought, with no town turn between
	Reg().Get<Pot>(pile).amount = 9;
	EXPECT_FLOAT_EQ(ecs::town_stats::AsTheyStand(stats, _town).woodAtSites, 9.0f);
	// at a town turn both agree
	EXPECT_FLOAT_EQ(ecs::town_stats::Compute(_town).woodAtSites, 9.0f);
}

TEST_F(BuildingSitesTest, ScaffoldCorrection)
{
	// n = 3 >= ScaffoldsRequired 2: c = 1 x 0.3 x 0.5 = 0.15 -> 0.35
	EXPECT_NEAR(plans::GetDesireToBeBuilt(_town, Info(k_House), 3), 0.35f, 1e-6f);
	// n = 1 < 2: (uint32)(n - 2) wraps, c is huge and r - min(c, r) = 0 (literal)
	EXPECT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_House), 1), 0.0f);
}

TEST_F(BuildingSitesTest, StoragePitOnlyWithoutOne)
{
	EXPECT_FLOAT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_Pit), 0), 0.7f);
	TownData().storagePit = MakeAbode(k_Pit, false);
	EXPECT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_Pit), 0), 0.0f);
}

TEST_F(BuildingSitesTest, GraveyardSharedBetweenItsSites)
{
	// two graveyard sites: r = b / s
	sites::AddBuildingSite(_town, MakeAbode(k_Graveyard, true));
	sites::AddBuildingSite(_town, MakeAbode(k_Graveyard, true));
	EXPECT_FLOAT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_Graveyard), 0), 0.3f);
	TownData().graveyard = MakeAbode(k_Graveyard, false);
	EXPECT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_Graveyard), 0), 0.0f);
}

TEST_F(BuildingSitesTest, FootballOffIsZero)
{
	EXPECT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_Football), 0), 0.0f);
}

// ---- BuildBy / Built / MakeFunctional
// -------------------------------------------------------

TEST_F(BuildingSitesTest, BuildByUntilBuilt)
{
	const auto house = MakeAbode(k_House, true);
	const auto site = sites::AddBuildingSite(_town, house);
	ASSERT_TRUE(site != entt::null);
	EXPECT_EQ(abodes::GetBuildingSite(house), site);
	EXPECT_TRUE(abodes::IsDrawBuilding(house));
	EXPECT_FALSE(abodes::IsBuilt(house));
	EXPECT_FALSE(ecs::abode_queries::IsBuilt(house));
	EXPECT_EQ(TownData().buildPulse, 1u); // set by AddBuildingSite
	ASSERT_EQ(sites::SitesOf(_town).size(), 1u);

	sites::BuildBy(site, 0.4f);
	EXPECT_FLOAT_EQ(abodes::GetPercentBuilt(house), 0.4f);
	EXPECT_FALSE(abodes::IsBuilt(house));
	sites::BuildBy(site, -1.0f); // below 0 -> 0
	EXPECT_EQ(abodes::GetPercentBuilt(house), 0.0f);
	sites::BuildBy(site, 0.5f);
	sites::BuildBy(site, 0.6f); // 1.1 >= 1 -> Built
	const auto& abode = Reg().Get<Abode>(house);
	EXPECT_TRUE(abodes::IsBuilt(house));
	EXPECT_EQ(abode.percentBuilt, 1.0f);
	EXPECT_EQ(abode.buildFlags & Abode::k_UnderConstruction, 0u);
	EXPECT_NE(abode.buildFlags & Abode::k_Built, 0u);
	EXPECT_TRUE(abode.addedToTownStats); // MakeFunctional
	EXPECT_TRUE(abode.buildingSite == entt::null);
	EXPECT_FALSE(sites::IsAvailable(site)); // Built's ToBeDeleted(0), through ecs::ToBeDeleted
	EXPECT_TRUE(sites::SitesOf(_town).empty());
	EXPECT_FALSE(Reg().Valid(site)); // the deferral off: destroyed at once
}

TEST_F(BuildingSitesTest, SetPercentBuiltBuildsAtOne)
{
	const auto house = MakeAbode(k_House, true);
	abodes::SetPercentBuilt(house, -0.5f);
	EXPECT_EQ(abodes::GetPercentBuilt(house), 0.0f);
	EXPECT_EQ(abodes::GetBuiltPercentage(house), 0.0f);
	EXPECT_TRUE(abodes::SetBuiltPercentage(house, 1.0f)); // CHL 22 -> Built
	EXPECT_TRUE(abodes::IsBuilt(house));
}

TEST_F(BuildingSitesTest, StoragePitMadeFunctionalWhenBuilt)
{
	const auto pit = MakeAbode(k_Pit, true);
	sites::AddBuildingSite(_town, pit);
	EXPECT_TRUE(TownData().storagePit == entt::null);
	sites::BuildBy(abodes::GetBuildingSite(pit), 1.0f);
	EXPECT_EQ(TownData().storagePit, pit); // the storage pit's MakeFunctional -> SetStoragePit
}

TEST_F(BuildingSitesTest, RepairBranch)
{
	const auto house = MakeAbode(k_House, false);
	ecs::life::SetLife(house, 0.5f);
	Reg().Get<Abode>(house).buildFlags |= Abode::k_NotRepaired;
	EXPECT_FALSE(abodes::IsRepaired(house));
	abodes::BuildBy(house, 0.3f); // built, not repaired -> IncreaseLife
	EXPECT_FLOAT_EQ(ecs::life::LifeOf(house), 0.8f);
	abodes::BuildBy(house, 0.3f); // capped at 1 -> Repaired: the not-repaired flag cleared, MakeFunctional
	EXPECT_EQ(ecs::life::LifeOf(house), 1.0f);
	EXPECT_EQ(Reg().Get<Abode>(house).buildFlags & Abode::k_NotRepaired, 0u);
}

// ---- builders and desire
// ---------------------------------------------------------------------------------------------

TEST_F(BuildingSitesTest, BuildersNeeded)
{
	const auto site = sites::AddBuildingSite(_town, MakeAbode(k_House, true));
	EXPECT_EQ(sites::GetMaxBuilders(site), 4);
	EXPECT_EQ(sites::GetBuildersNeeded(site), 4);
	EXPECT_FLOAT_EQ(sites::GetDesireForVillagers(site), 1.0f);
	const auto a = Reg().Create();
	const auto b = Reg().Create();
	sites::AddBuilder(site, a);
	sites::AddBuilder(site, b);
	EXPECT_EQ(sites::GetBuildersNeeded(site), 2);
	EXPECT_FLOAT_EQ(sites::GetDesireForVillagers(site), 0.5f);
	EXPECT_EQ(Reg().Get<BuildingSite>(site).builders.front(), b); // the head first
	// RemoveBuilder: -1 once, also for a villager that is not there (literal)
	sites::RemoveBuilder(site, Reg().Create());
	EXPECT_EQ(sites::GetBuilderCount(site), 1);
	EXPECT_EQ(Reg().Get<BuildingSite>(site).builders.size(), 2u);
}

TEST_F(BuildingSitesTest, PruneBuiltAndRepaired)
{
	const auto house = MakeAbode(k_House, false); // built, life 1
	const auto site = sites::AddBuildingSite(_town, house);
	sites::PruneSites(_town); // built and repaired -> ToBeDeleted
	EXPECT_FALSE(sites::IsAvailable(site));
	EXPECT_TRUE(sites::SitesOf(_town).empty());
	EXPECT_TRUE(abodes::GetBuildingSite(house) == entt::null);
}

// ---- GetNearestEdge
// ------------------------------------------------------------------------------------------

TEST_F(BuildingSitesTest, NearestEdgeIndex)
{
	auto& registry = Reg();
	const auto site = registry.Create();
	auto& s = registry.Assign<BuildingSite>(site);
	for (size_t i = 0; i < s.ring.size(); ++i)
	{
		s.ring.at(i) = glm::vec3(static_cast<float>(i), 0.0f, 2.0f * static_cast<float>(i));
	}
	int32_t index = -1;
	(void)sites::GetNearestEdge(site, 0.0f, index);
	EXPECT_EQ(index, 0); // angle == 0 -> 0
	(void)sites::GetNearestEdge(site, glm::pi<float>(), index);
	EXPECT_EQ(index, 64); // ftol(pi x 0.159155 x 128) = 64
	(void)sites::GetNearestEdge(site, 0.1f, index);
	EXPECT_EQ(index, 2);
	(void)sites::GetNearestEdge(site, 3.0f * glm::pi<float>(), index);
	// above 2 pi: - 2 pi; in float 3 pi - 2 pi is 3.1415925, just under pi (3.1415927), so ftol gives 63
	EXPECT_EQ(index, 63);
	(void)sites::GetNearestEdge(site, 7.0f * glm::pi<float>(), index);
	EXPECT_EQ(index, 64); // above 6 pi: pi
	(void)sites::GetNearestEdge(site, -7.0f * glm::pi<float>(), index);
	EXPECT_EQ(index, 0); // below -6 pi: 0
	// 2 pi itself is kept (only above it is reduced): ftol(2 pi x 0.159155 x 128) = 128, & 0x7F = 0
	const auto pos = sites::GetNearestEdge(site, glm::two_pi<float>(), index);
	EXPECT_EQ(index, 0);
	EXPECT_EQ(pos, (map_coords::MapCoords {0, 0, 0.0f}));
	const auto pos126 = sites::GetNearestEdge(site, 6.2f, index); // ftol(126.3) = 126
	EXPECT_EQ(index, 126);
	EXPECT_EQ(pos126.x, map_coords::ToFixed(126.0f));
	EXPECT_EQ(pos126.z, map_coords::ToFixed(252.0f));
	EXPECT_FALSE(sites::GetBuildPos(site, 128).has_value());
	EXPECT_TRUE(sites::GetBuildPos(site, 127).has_value());
}

// ---- GetPercentForDrawBuilding --------------------------------------------------------------------------------------

TEST_F(BuildingSitesTest, PercentForDrawBuilding)
{
	const auto unbuilt = MakeAbode(k_House, true);
	abodes::SetPercentBuilt(unbuilt, 0.3f);
	EXPECT_FLOAT_EQ(abodes::GetPercentForDrawBuilding(unbuilt), 0.3f); // not built: min(0.3, 1)
	const auto built = MakeAbode(k_House, false);
	EXPECT_FLOAT_EQ(abodes::GetPercentForDrawBuilding(built), 0.98f); // no damage mesh: life x 0.98
	ecs::life::SetLife(built, 0.5f);
	EXPECT_FLOAT_EQ(abodes::GetPercentForDrawBuilding(built), 0.49f);
	sites::AddBuildingSite(_town, built); // a site but no damage mesh: still life x 0.98
	EXPECT_FLOAT_EQ(abodes::GetPercentForDrawBuilding(built), 0.49f);
	EXPECT_TRUE(abodes::IsDrawBuilding(built));
}

// ---- plans
// ------------------------------------------------------------------------------------------------------------

TEST_F(BuildingSitesTest, PlansListAndRepairDesire)
{
	PlannedAbode plan {k_House, glm::vec3(100.0f, 0.0f, 100.0f), 0.0f, 1.0f, false};
	EXPECT_EQ(plans::AddPlanned(_town, plan), 0u);
	plan.wasBuilt = true;
	EXPECT_EQ(plans::AddPlanned(_town, plan), 1u);
	EXPECT_EQ(plans::PlansOf(_town), 2u);
	EXPECT_EQ(plans::GetDesireToBeRepaired(_town, 0), 0.0f);       // not built: 0
	EXPECT_FLOAT_EQ(plans::GetDesireToBeRepaired(_town, 1), 0.5f); // built: the info's desireToBeRepaired
	EXPECT_FALSE(plans::IsCivic(_town, 0));
	float best = 0.0f;
	const auto chosen = plans::GetBestPlanned(_town, best, 2);
	ASSERT_TRUE(chosen.has_value());
	EXPECT_EQ(*chosen, 0u); // ties: the first (oldest) one
	EXPECT_FLOAT_EQ(best, 0.5f);
	EXPECT_FALSE(plans::GetBestPlanned(_town, best, 4).has_value()); // mask 4: no civic plan
	plans::RemovePlanned(_town, 0);
	EXPECT_EQ(plans::PlansOf(_town), 1u);
}

TEST_F(BuildingSitesTest, ATownAsksForTheMostWantedPlanOfTheKindItBuilds)
{
	// the town's plans, in the order they were made: a house (0.5), a graveyard (0.6), a storage pit (0.7) and a town
	// centre (0.65)
	const auto add = [this](AbodeInfo info, bool townCentre) {
		return plans::AddPlanned(_town, PlannedAbode {info, glm::vec3(100.0f, 0.0f, 100.0f), 0.0f, 1.0f, townCentre});
	};
	const auto house = add(k_House, false);
	const auto graveyard = add(k_Graveyard, false);
	const auto pit = add(k_Pit, false);
	const auto centre = add(k_TownCentre, true);
	EXPECT_TRUE(plans::IsCivic(_town, centre));
	// a civic building is asked for with the civic bit of the abode type (4), a home with the living quarters' (2):
	// each takes the most wanted plan of its kind, strictly above the ones before it
	float best = 0.0f;
	auto chosen = plans::GetBestPlanned(_town, best, 4);
	ASSERT_TRUE(chosen.has_value());
	EXPECT_EQ(*chosen, pit);
	EXPECT_FLOAT_EQ(best, 0.7f);
	chosen = plans::GetBestPlanned(_town, best, 2);
	ASSERT_TRUE(chosen.has_value());
	EXPECT_EQ(*chosen, house);
	EXPECT_FLOAT_EQ(best, 0.5f);
	// a town with a storage pit wants no other: the town centre comes next, wanted whole while the town has no centre,
	// no totem and no site of one
	TownData().storagePit = MakeAbode(k_Pit, false);
	chosen = plans::GetBestPlanned(_town, best, 4);
	ASSERT_TRUE(chosen.has_value());
	EXPECT_EQ(*chosen, centre);
	EXPECT_FLOAT_EQ(best, 0.65f);
	// once it has a centre it wants none: the graveyard
	TownData().centre = MakeAbode(k_TownCentre, false);
	chosen = plans::GetBestPlanned(_town, best, 4);
	ASSERT_TRUE(chosen.has_value());
	EXPECT_EQ(*chosen, graveyard);
	EXPECT_FLOAT_EQ(best, 0.6f);
	// and with a graveyard nothing civic is wanted: no plan, and a best of 0
	ecs::graveyard::SetGraveyard(_town, MakeAbode(k_Graveyard, false));
	EXPECT_FALSE(plans::GetBestPlanned(_town, best, 4).has_value());
	EXPECT_EQ(best, 0.0f);
}

TEST_F(BuildingSitesTest, WoodGivenToOrTakenFromAnAbodeBeingBuiltIsItsSites)
{
	// the site's pile row holds plenty
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->pot.at(static_cast<size_t>(PotInfo::WoodPile_1)).maxAmountInPot = 1000;
	Locator::infoConstants::reset(info.release());
	namespace objects = ecs::object_resources;
	const auto house = MakeAbode(k_House, true);
	const auto site = sites::AddBuildingSite(_town, house);
	ASSERT_TRUE(site != entt::null);
	const auto pile = Reg().Create();
	Reg().Assign<Transform>(pile, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<Pot>(pile, static_cast<uint16_t>(6), static_cast<uint16_t>(100), PotInfo::WoodPile_1);
	Reg().Get<BuildingSite>(site).woodPile = pile;
	// the abode then leaves its town, so that no town hears of what comes and goes (the branches do not ask for one)
	Reg().Get<Abode>(house).townId = Abode::k_NoTown;
	const auto& abode = Reg().Get<Abode>(house);

	// wood given goes to the site's pile, not to the abode
	EXPECT_EQ(objects::AddResource(house, ResourceType::Wood, 4), 4u);
	EXPECT_EQ(Reg().Get<Pot>(pile).amount, 10u);
	EXPECT_EQ(abode.woodAmount, 0u);
	// any type goes to the site too, which takes only wood: nothing is added anywhere
	EXPECT_EQ(objects::AddResource(house, ResourceType::Any, 4), 0u);
	EXPECT_EQ(Reg().Get<Pot>(pile).amount, 10u);
	EXPECT_EQ(abode.woodAmount, 0u);
	// food stays the abode's own
	EXPECT_EQ(objects::AddResource(house, ResourceType::Food, 7), 7u);
	EXPECT_EQ(abode.foodAmount, 7u);
	EXPECT_EQ(Reg().Get<Pot>(pile).amount, 10u);

	// wood taken comes from the site's pile; any type asks the site, which gives only wood
	EXPECT_EQ(objects::RemoveResource(house, ResourceType::Wood, 3), 3u);
	EXPECT_EQ(Reg().Get<Pot>(pile).amount, 7u);
	EXPECT_EQ(objects::RemoveResource(house, ResourceType::Any, 3), 0u);
	EXPECT_EQ(Reg().Get<Pot>(pile).amount, 7u);
	EXPECT_EQ(abode.foodAmount, 7u);
	// food comes from the abode
	EXPECT_EQ(objects::RemoveResource(house, ResourceType::Food, 5), 5u);
	EXPECT_EQ(abode.foodAmount, 2u);

	// without its site the abode keeps the wood itself
	Reg().Get<Abode>(house).buildingSite = entt::null;
	EXPECT_EQ(objects::AddResource(house, ResourceType::Wood, 4), 4u);
	EXPECT_EQ(abode.woodAmount, 4u);
	EXPECT_EQ(Reg().Get<Pot>(pile).amount, 7u);
	EXPECT_EQ(objects::RemoveResource(house, ResourceType::Wood, 1), 1u);
	EXPECT_EQ(abode.woodAmount, 3u);
	EXPECT_EQ(Reg().Get<Pot>(pile).amount, 7u);
}

// ---- the graveyard --------------------------------------------------------------------------------------------------

TEST_F(BuildingSitesTest, AWorkingGraveyardOfATownCountsItsDeadUpToFifty)
{
	const auto yard = MakeAbode(k_Graveyard, false);
	Reg().Assign<Graveyard>(yard);
	for (int i = 0; i < 60; ++i)
	{
		ecs::graveyard::AddDead(yard);
	}
	EXPECT_EQ(Reg().Get<Graveyard>(yard).dead, 50u);
	// one still being built, or of no town, counts nobody
	const auto building = MakeAbode(k_Graveyard, true);
	Reg().Assign<Graveyard>(building);
	ecs::graveyard::AddDead(building);
	EXPECT_EQ(Reg().Get<Graveyard>(building).dead, 0u);
	const auto alone = MakeAbode(k_Graveyard, false);
	Reg().Get<Abode>(alone).townId = 99; // no such town
	Reg().Assign<Graveyard>(alone);
	ecs::graveyard::AddDead(alone);
	EXPECT_EQ(Reg().Get<Graveyard>(alone).dead, 0u);
}

TEST_F(BuildingSitesTest, AGraveyardsGravesStageIsItsDeadTimesPointEighteen)
{
	// the stage after each burial: the dead x 0.18 truncated, at least 1 once someone is buried, in 3 bits on the model
	constexpr std::array<std::pair<uint32_t, uint8_t>, 14> k_Stages {{{1, 1},
	                                                                  {5, 1},
	                                                                  {6, 1},
	                                                                  {11, 1},
	                                                                  {12, 2},
	                                                                  {17, 3},
	                                                                  {22, 3},
	                                                                  {23, 4},
	                                                                  {28, 5},
	                                                                  {34, 6},
	                                                                  {39, 7},
	                                                                  {44, 7},
	                                                                  {45, 0},
	                                                                  {50, 1}}};
	const auto yard = MakeAbode(k_Graveyard, false);
	Reg().Assign<Graveyard>(yard);
	uint32_t buried = 0;
	for (const auto& [dead, stage] : k_Stages)
	{
		while (buried < dead)
		{
			ecs::graveyard::AddDead(yard);
			++buried;
		}
		EXPECT_EQ(Reg().Get<Graveyard>(yard).dead, dead);
		EXPECT_EQ(Reg().Get<Graveyard>(yard).gravesStage, stage) << dead << " dead";
	}
}
} // namespace

TEST(TownRoom, AnAbodeAddsItsFreeAdultPlacesAndItsPlaces)
{
	using openblack::ecs::town_stats::AddAbodeRoom;
	// 4 adult places and 2 child places, 1 adult in: 3 free
	auto room = AddAbodeRoom({}, 4, 2, 1);
	EXPECT_EQ(room.freeAdultPlaces, 3);
	EXPECT_EQ(room.adultPlaces, 4u);
	EXPECT_EQ(room.childPlaces, 2u);
	// an over-full home takes from the others' free places
	room = AddAbodeRoom(room, 2, 0, 5);
	EXPECT_EQ(room.freeAdultPlaces, 0);
	EXPECT_EQ(room.adultPlaces, 6u);
	// an abode with no places adds no places
	room = AddAbodeRoom(room, 0, 0, 0);
	EXPECT_EQ(room.freeAdultPlaces, 0);
	EXPECT_EQ(room.adultPlaces, 6u);
	EXPECT_EQ(room.childPlaces, 2u);
}

TEST_F(BuildingSitesTest, AScaffoldTakenIntoAHandLeavesItsSiteAndItsBuildingGoes)
{
	// a scaffold put down on a house's site and taken into a player's hand: held by that player, no placed turn, off
	// the site, and the site's unbuilt house goes with it
	const auto house = MakeAbode(k_House, true);
	const auto site = sites::AddBuildingSite(_town, house);
	ASSERT_TRUE(site != entt::null);
	const auto scaffold = Reg().Create();
	Reg().Assign<Transform>(scaffold, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& placed = Reg().Assign<Scaffold>(scaffold);
	placed.site = site;
	placed.town = _town;
	placed.placedTurn = 40;
	ecs::scaffolds::OnPickedUp(scaffold, PlayerNames::PLAYER_TWO);
	const auto& held = Reg().Get<const Scaffold>(scaffold);
	EXPECT_EQ(held.holder, std::optional(PlayerNames::PLAYER_TWO));
	EXPECT_EQ(held.placedTurn, 0u);
	EXPECT_EQ(held.site, entt::entity {entt::null});
	EXPECT_FALSE(ecs::IsAvailable(house));
}

TEST_F(BuildingSitesTest, AScriptsNewScaffoldCanBecomeAPhysicsObjectAndOneOnASiteCannot)
{
	// a scaffold as a script's create makes it, with no town, no workshop and no site, can become a physics object, so
	// the create puts it into the physics; once on a building site it cannot
	const auto scaffold = ecs::scaffolds::Create(map_coords::FromWorld(nullptr, glm::vec3(10.0f, 0.0f, 10.0f)), entt::null,
	                                             entt::null, 0.0f, 1.0f);
	EXPECT_TRUE(ecs::scaffolds::CanBecomePhysicsObject(scaffold));
	const auto site = sites::AddBuildingSite(_town, MakeAbode(k_House, true));
	ASSERT_TRUE(site != entt::null);
	Reg().Get<Scaffold>(scaffold).site = site;
	EXPECT_FALSE(ecs::scaffolds::CanBecomePhysicsObject(scaffold));
}

namespace
{
/// The sounds a tap plays, with the scaffold tap's counter at the value the test gives
class TapSounds final: public audio::AudioManagerNoOp
{
public:
	using AudioManagerNoOp::PlaySoundEffect;
	audio::Channel PlaySoundEffect(const audio::PlayOptions& options) override
	{
		played.push_back(options);
		return audio::k_NoChannel;
	}
	[[nodiscard]] int NextCounter(audio::Counter counter) override
	{
		counters.push_back(counter);
		return next;
	}

	int next {0};
	std::vector<audio::PlayOptions> played;
	std::vector<audio::Counter> counters;
};

/// Any mesh is a box 4 across and 1 high
std::optional<AxisAlignedBoundingBox> FourAcross(entt::id_type /*meshId*/)
{
	return AxisAlignedBoundingBox {{-2.0f, 0.0f, -2.0f}, {2.0f, 1.0f, 2.0f}};
}
} // namespace

TEST_F(BuildingSitesTest, OnlyAScaffoldOfMoreThanOneFreeForAnyBuildingCanBeTapped)
{
	// the original's test: a value above one, not flying, off any site or still adjustable on one, and free to offer
	// any building
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->scaffold.gameTurnsAfterPlacingCanStillPickUp = 50;
	Locator::infoConstants::reset(info.release());
	const test::RestoreService<Locator::dynamicsSystem> keepDynamics;
	Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>();
	const auto scaffold =
	    ecs::scaffolds::Create(map_coords::FromWorld(nullptr, glm::vec3(10.0f, 0.0f, 10.0f)), _town, entt::null, 0.0f, 1.0f);
	ecs::scaffolds::SetValue(scaffold, 1);
	EXPECT_FALSE(ecs::scaffolds::ValidToTap(scaffold));
	ecs::scaffolds::SetValue(scaffold, 2);
	EXPECT_TRUE(ecs::scaffolds::ValidToTap(scaffold));
	// one kept for a building of its own
	auto& c = Reg().Get<Scaffold>(scaffold);
	c.limit = 3;
	EXPECT_FALSE(ecs::scaffolds::ValidToTap(scaffold));
	c.limit = Scaffold::k_AnyAbode;
	// on a site: tappable while it can still be adjusted, 49 turns after it was put down, not 50
	const auto site = sites::AddBuildingSite(_town, MakeAbode(k_House, true));
	ASSERT_TRUE(site != entt::null);
	Reg().Get<Scaffold>(scaffold).site = site;
	Reg().Get<Scaffold>(scaffold).placedTurn = 100;
	game_clock::SetTurn(149);
	EXPECT_TRUE(ecs::scaffolds::ValidToTap(scaffold));
	game_clock::SetTurn(150);
	EXPECT_FALSE(ecs::scaffolds::ValidToTap(scaffold));
	// a scaffold not on a site does not ask
	Reg().Get<Scaffold>(scaffold).site = entt::null;
	EXPECT_TRUE(ecs::scaffolds::ValidToTap(scaffold));
	game_clock::SetTurn(0);
	// not a scaffold
	EXPECT_FALSE(ecs::scaffolds::ValidToTap(Reg().Create()));
}

TEST_F(BuildingSitesTest, TappingAScaffoldBreaksOneOffAndLeavesItsSite)
{
	// a scaffold of 3 on a house's site, tapped: it leaves the site, whose house goes, keeps 2, and a new scaffold of 1
	// of its town stands as far from where it was on the other side, one 2D radius each way
	const test::RestoreService<Locator::dynamicsSystem> keepDynamics;
	Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>();
	const test::RestoreService<Locator::resources> keepResources;
	Locator::resources::emplace<resources::Resources>();
	const test::RestoreService<Locator::meshBoxProvider> keepBoxes;
	test::FakeMeshBoxProvider boxes;
	boxes.meshBox = &FourAcross;
	Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>(boxes);
	const test::RestoreService<Locator::audio> keepAudio;
	auto& sounds = static_cast<TapSounds&>(Locator::audio::emplace<TapSounds>());
	sounds.next = 2;

	const auto house = MakeAbode(k_House, true);
	const auto site = sites::AddBuildingSite(_town, house);
	ASSERT_TRUE(site != entt::null);
	const glm::vec3 where(10.0f, 0.0f, 10.0f);
	const auto scaffold = ecs::scaffolds::Create(map_coords::FromWorld(nullptr, where), _town, entt::null, 0.0f, 1.0f);
	ecs::scaffolds::SetValue(scaffold, 3);
	Reg().Get<Scaffold>(scaffold).site = site;
	const glm::vec3 hand(12.0f, 3.0f, 14.0f);

	EXPECT_EQ(ecs::scaffolds::Tap(scaffold, hand), 1u);
	EXPECT_EQ(ecs::scaffolds::GetValue(scaffold), 2u);
	EXPECT_EQ(Reg().Get<const Scaffold>(scaffold).site, entt::entity {entt::null});
	EXPECT_FALSE(ecs::IsAvailable(house));
	std::vector<entt::entity> others;
	Reg().Each<const Scaffold>([&](entt::entity e, const Scaffold&) {
		if (e != scaffold)
		{
			others.push_back(e);
		}
	});
	ASSERT_EQ(others.size(), 1u);
	const auto broken = others.front();
	EXPECT_EQ(ecs::scaffolds::GetValue(broken), 1u);
	EXPECT_EQ(Reg().Get<const Scaffold>(broken).town, _town);
	// the radius of a scaffold of 3 (scale 1.1) over a box 4 across: 2.2 each way, about the place it stood
	const auto& left = Reg().Get<const Transform>(scaffold).position;
	const auto& right = Reg().Get<const Transform>(broken).position;
	EXPECT_NEAR((left.x + right.x) * 0.5f, where.x, 0.01f);
	EXPECT_NEAR((left.z + right.z) * 0.5f, where.z, 0.01f);
	EXPECT_NEAR(glm::distance(glm::vec2(left.x, left.z), glm::vec2(right.x, right.z)), 4.4f, 0.02f);
}

TEST_F(BuildingSitesTest, BreakingAScaffoldPlaysTheNextOfItsFourSoundsAtTheHand)
{
	// the in-game bank's sample 151 plus the scaffold tap's counter, in 3D at the hand's point, owned by the scaffold
	// and not following it
	const test::RestoreService<Locator::dynamicsSystem> keepDynamics;
	Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>();
	const test::RestoreService<Locator::resources> keepResources;
	Locator::resources::emplace<resources::Resources>();
	const test::RestoreService<Locator::audio> keepAudio;
	auto& sounds = static_cast<TapSounds&>(Locator::audio::emplace<TapSounds>());
	sounds.next = 2;
	const auto scaffold =
	    ecs::scaffolds::Create(map_coords::FromWorld(nullptr, glm::vec3(10.0f, 0.0f, 10.0f)), _town, entt::null, 0.0f, 1.0f);
	ecs::scaffolds::SetValue(scaffold, 2);
	const glm::vec3 hand(12.0f, 3.0f, 14.0f);

	EXPECT_EQ(ecs::scaffolds::Tap(scaffold, hand), 1u);
	ASSERT_EQ(sounds.counters.size(), 1u);
	EXPECT_EQ(sounds.counters.front(), audio::Counter::ScaffoldTap);
	ASSERT_EQ(sounds.played.size(), 1u);
	const auto& sound = sounds.played.front();
	EXPECT_EQ(sound.sample.bank, audio::Bank(audio::SfxBank::InGame));
	EXPECT_EQ(sound.sample.number, 153);
	EXPECT_TRUE(sound.is3D);
	EXPECT_FALSE(sound.track);
	EXPECT_EQ(sound.position, hand);
	EXPECT_TRUE(sound.owner == audio::Owner::Thing(scaffold));
}

TEST_F(BuildingSitesTest, AScaffoldHeldInAMovingHandTurnsItsPhantomAndFades)
{
	// every hand turn of a held scaffold whose hand moved: its running feedback counter steps on, its phantom turns by
	// the info's rotation per game turn, its fade drops by 100, and the still hand's hover flag is cleared; it stays
	// in the hand
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->scaffold.phantomBuildingRotationPerGameTurn = 0.25f;
	Locator::infoConstants::reset(info.release());
	const auto scaffold = Reg().Create();
	Reg().Assign<Transform>(scaffold, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& held = Reg().Assign<Scaffold>(scaffold);
	held.flags = Scaffold::k_FeedbackStep | Scaffold::k_HoverDone;
	held.phantomAngle = 1.0f;
	held.fade = 300;
	EXPECT_TRUE(ecs::scaffolds::ProcessInHand(scaffold, {.player = PlayerNames::PLAYER_ONE, .stillTurns = 0, .hand = {}}));
	const auto& after = Reg().Get<const Scaffold>(scaffold);
	EXPECT_EQ(after.flags & Scaffold::k_FeedbackMask, 2 * Scaffold::k_FeedbackStep);
	EXPECT_EQ(after.flags & Scaffold::k_HoverDone, 0);
	EXPECT_FLOAT_EQ(after.phantomAngle, 1.25f);
	EXPECT_EQ(after.fade, 200);
}

TEST_F(BuildingSitesTest, AScaffoldPutDownGentlyLeavesWhatItsBuildingDoesNotNeedInANewOne)
{
	// a scaffold of 4 that offers a house (2 needed), put down gently: on leaving the hand it keeps 2 and a new scaffold
	// of the other 2, for the hand to take, is made where it is
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	const auto scaffold = MakeGentleHouseScaffoldOfFour();
	const auto surplus = ecs::scaffolds::OnLeftHand(scaffold, PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(ecs::scaffolds::IsScaffold(surplus));
	EXPECT_EQ(ecs::scaffolds::GetValue(surplus), 2u);
	EXPECT_EQ(ecs::scaffolds::GetValue(scaffold), 2u);
	// thrown (not put down gently): no new scaffold
	Reg().Get<Scaffold>(scaffold).flags &= static_cast<uint16_t>(~Scaffold::k_DroppedGently);
	ecs::scaffolds::SetValue(scaffold, 4);
	EXPECT_EQ(ecs::scaffolds::OnLeftHand(scaffold, PlayerNames::PLAYER_ONE), entt::entity {entt::null});
	EXPECT_EQ(ecs::scaffolds::GetValue(scaffold), 4u);
}

TEST_F(BuildingSitesTest, AComputerPlayersHandPutsAScaffoldDownWhole)
{
	// the same scaffold let go of by a computer player's hand: no new scaffold, it keeps its 4
	MakePlayer(PlayerNames::PLAYER_TWO, 2);
	const auto scaffold = MakeGentleHouseScaffoldOfFour();
	EXPECT_EQ(ecs::scaffolds::OnLeftHand(scaffold, PlayerNames::PLAYER_TWO), entt::entity {entt::null});
	EXPECT_EQ(ecs::scaffolds::GetValue(scaffold), 4u);
}

TEST_F(BuildingSitesTest, AHandOfAPlayerNotYetAComputerPlayerLeavesTheSurplus)
{
	// a hand that moves for a player whose computer god was never switched on (type 0): the surplus is made as for the
	// local hand, for that hand to take
	MakePlayer(PlayerNames::PLAYER_THREE, 0);
	const auto scaffold = MakeGentleHouseScaffoldOfFour();
	const auto surplus = ecs::scaffolds::OnLeftHand(scaffold, PlayerNames::PLAYER_THREE);
	ASSERT_TRUE(ecs::scaffolds::IsScaffold(surplus));
	EXPECT_EQ(ecs::scaffolds::GetValue(surplus), 2u);
	EXPECT_EQ(ecs::scaffolds::GetValue(scaffold), 2u);
}

TEST_F(BuildingSitesTest, AHandsNextDropTakesTheMarkOffWhatItDroppedBefore)
{
	// each player's interface keeps one last thing let go of: the local hand drops a scaffold, then a pile, and the
	// scaffold is no longer the local hand's last
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	const auto scaffold = MakeGentleHouseScaffoldOfFour();
	const auto pile = Reg().Create();
	ecs::last_dropped::RecordDrop(Reg(), PlayerNames::PLAYER_ONE, scaffold);
	EXPECT_EQ(ecs::last_dropped::WhoLastDropped(Reg(), scaffold), PlayerNames::PLAYER_ONE);
	ecs::last_dropped::RecordDrop(Reg(), PlayerNames::PLAYER_ONE, pile);
	EXPECT_EQ(ecs::last_dropped::WhoLastDropped(Reg(), scaffold), std::nullopt);
	EXPECT_EQ(ecs::last_dropped::WhoLastDropped(Reg(), pile), PlayerNames::PLAYER_ONE);
	EXPECT_EQ(ecs::last_dropped::WhoLastDropped(Reg(), entt::null), std::nullopt);
}

TEST_F(BuildingSitesTest, ADropTakesTheMarkFromTheHandThatHadIt)
{
	// a computer player's hand drops the scaffold the local hand dropped: it is the computer player's now, and the
	// local hand keeps nothing; a player with no entity keeps no mark
	const auto local = MakePlayer(PlayerNames::PLAYER_ONE, 1);
	MakePlayer(PlayerNames::PLAYER_TWO, 2);
	const auto scaffold = MakeGentleHouseScaffoldOfFour();
	ecs::last_dropped::RecordDrop(Reg(), PlayerNames::PLAYER_ONE, scaffold);
	ecs::last_dropped::RecordDrop(Reg(), PlayerNames::PLAYER_TWO, scaffold);
	EXPECT_EQ(ecs::last_dropped::WhoLastDropped(Reg(), scaffold), PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(Reg().Get<const LastDropped>(local).object == entt::null);
	const auto pile = Reg().Create();
	ecs::last_dropped::MarkThrown(Reg(), PlayerNames::PLAYER_FOUR, pile);
	EXPECT_EQ(ecs::last_dropped::WhoLastDropped(Reg(), pile), std::nullopt);
}

TEST_F(BuildingSitesTest, AThrowMarksWithoutTakingTheMarkFromAnotherHand)
{
	// the local hand throws a scaffold that the second player's hand dropped last: both keep it, and the first player
	// counts. The local hand's own drop then takes its mark away and gives it back, so the second player still keeps
	// it; once the local hand drops something else, the scaffold is the second player's again
	MakePlayer(PlayerNames::PLAYER_ONE, 1);
	MakePlayer(PlayerNames::PLAYER_TWO, 2);
	const auto scaffold = MakeGentleHouseScaffoldOfFour();
	const auto pile = Reg().Create();
	ecs::last_dropped::RecordDrop(Reg(), PlayerNames::PLAYER_TWO, scaffold);
	ecs::last_dropped::MarkThrown(Reg(), PlayerNames::PLAYER_ONE, scaffold);
	EXPECT_EQ(ecs::last_dropped::WhoLastDropped(Reg(), scaffold), PlayerNames::PLAYER_ONE);
	ecs::last_dropped::RecordDrop(Reg(), PlayerNames::PLAYER_ONE, scaffold);
	EXPECT_EQ(ecs::last_dropped::WhoLastDropped(Reg(), scaffold), PlayerNames::PLAYER_ONE);
	ecs::last_dropped::RecordDrop(Reg(), PlayerNames::PLAYER_ONE, pile);
	EXPECT_EQ(ecs::last_dropped::WhoLastDropped(Reg(), scaffold), PlayerNames::PLAYER_TWO);
}

TEST_F(BuildingSitesTest, AScaffoldOnASiteImpressesByWhatItsTownWantsOfTheBuilding)
{
	// what a scaffold's reaction is multiplied by: 1 off any site; on a house's site, the town's raw desire for abodes
	// (raw + boost + boost A, in that order), not for civic buildings; once the site's building is gone, 0
	auto& desire = Reg().Get<Town>(_town).desire;
	desire.raw.at(5) = 0.3f;
	desire.boost.at(5) = 0.1f;
	desire.boostA.at(5) = 0.05f;
	desire.raw.at(6) = 0.9f;
	const auto scaffold = Reg().Create();
	Reg().Assign<Transform>(scaffold, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<Scaffold>(scaffold).town = _town;
	EXPECT_EQ(ecs::villager_reactions::ScaffoldSiteWant(scaffold), 1.0f);
	const auto house = MakeAbode(k_House, true);
	const auto site = sites::AddBuildingSite(_town, house);
	ASSERT_TRUE(site != entt::null);
	Reg().Get<Scaffold>(scaffold).site = site;
	EXPECT_EQ(ecs::villager_reactions::ScaffoldSiteWant(scaffold), 0.3f + 0.1f + 0.05f);
	ecs::ToBeDeleted(house);
	EXPECT_EQ(ecs::villager_reactions::ScaffoldSiteWant(scaffold), 0.0f);
}

// How many of a town's people a planted scaffold impresses: a tenth of them (the scaffold info's proportion), truncated
// toward zero after the float multiply, and at least one
TEST(Scaffolds, AScaffoldImpressesATenthOfATownAndAtLeastOne)
{
	using ecs::scaffolds::VillagersToImpress;
	EXPECT_EQ(VillagersToImpress(25, 0.1f), 2u);
	EXPECT_EQ(VillagersToImpress(30, 0.1f), 3u);
	EXPECT_EQ(VillagersToImpress(100, 0.1f), 10u);
	EXPECT_EQ(VillagersToImpress(19, 0.1f), 1u);
	EXPECT_EQ(VillagersToImpress(5, 0.1f), 1u);
	EXPECT_EQ(VillagersToImpress(0, 0.1f), 1u);
}

TEST_F(BuildingSitesTest, AHeldScaffoldMayJoinAScaffoldUnderTheHandOnlyWithinTheCombinedLimit)
{
	// a held scaffold over a scaffold on the ground: valid to apply while the two together are worth at most 8 less one
	// with football off (7), both are free to offer any building and the one on the ground is not built on; a thing that
	// is neither a scaffold nor a storage pit takes nothing. (The join itself builds at once, which a unit test's world
	// can't: it runs from the hand's apply press, HandApplyToObject.cpp)
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->scaffold.maxNumberForCombining = 8;
	Locator::infoConstants::reset(info.release());
	const auto at = [](float x) { return map_coords::FromWorld(nullptr, glm::vec3(x, 0.0f, 10.0f)); };
	const auto held = ecs::scaffolds::Create(at(10.0f), _town, entt::null, 0.0f, 1.0f);
	const auto ground = ecs::scaffolds::Create(at(20.0f), _town, entt::null, 0.0f, 1.0f);
	const auto rock = Reg().Create();
	EXPECT_FALSE(ecs::scaffolds::ValidToApplyThisToObject(held, rock));
	ecs::scaffolds::SetValue(held, 3);
	ecs::scaffolds::SetValue(ground, 4);
	EXPECT_TRUE(ecs::scaffolds::ValidToApplyThisToObject(held, ground));
	ecs::scaffolds::SetValue(ground, 5);
	EXPECT_FALSE(ecs::scaffolds::ValidToApplyThisToObject(held, ground));
	// one limited to a single kind of building may not join
	ecs::scaffolds::SetValue(ground, 2);
	Reg().Get<Scaffold>(ground).limit = 0;
	EXPECT_FALSE(ecs::scaffolds::ValidToApplyThisToObject(held, ground));
}

TEST(TownBuildings, AnAbodeCountsByItsPlacesItsTypeAndItsNumber)
{
	using openblack::ecs::town_stats::AddAbodeBuildings;
	// a house: one with places, of number A
	auto buildings = AddAbodeBuildings({}, 4, 2, false, AbodeNumber::A);
	EXPECT_EQ(buildings.abodesWithPlaces, 1u);
	EXPECT_EQ(buildings.civicBuildings, 0u);
	EXPECT_EQ(buildings.abodesByNumber.at(static_cast<size_t>(AbodeNumber::A)), 1);
	// a civic building with no places: one civic, of its own number
	buildings = AddAbodeBuildings(buildings, 0, 0, true, AbodeNumber::Workshop);
	EXPECT_EQ(buildings.abodesWithPlaces, 1u);
	EXPECT_EQ(buildings.civicBuildings, 1u);
	EXPECT_EQ(buildings.abodesByNumber.at(static_cast<size_t>(AbodeNumber::Workshop)), 1);
	// a number out of the table counts nowhere, and the counts are bytes
	buildings = AddAbodeBuildings(buildings, 0, 0, false, static_cast<AbodeNumber>(-1));
	EXPECT_EQ(buildings.abodesByNumber.at(static_cast<size_t>(AbodeNumber::A)), 1);
	for (int i = 0; i < 255; ++i)
	{
		buildings = AddAbodeBuildings(buildings, 1, 0, false, AbodeNumber::A);
	}
	EXPECT_EQ(buildings.abodesByNumber.at(static_cast<size_t>(AbodeNumber::A)), 0);
	EXPECT_EQ(buildings.abodesWithPlaces, 256u);
}

TEST_F(BuildingSitesTest, SevenScaffoldsOfferTheWonderOfTheScaffoldsTribeSizedByTheTownsWonderPower)
{
	// a Celtic town, its Celtic and the Japanese wonders' rows of seven scaffolds
	Reg().Assign<Tribe>(_town, Tribe::CELTIC);
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	for (const auto [row, tribe] :
	     {std::pair {AbodeInfo::CelticWonder, Tribe::CELTIC}, std::pair {AbodeInfo::JapaneseWonder, Tribe::JAPANESE}})
	{
		auto& wonder = info->abode.at(static_cast<size_t>(row));
		wonder.abodeType = AbodeType::Wonder;
		wonder.abodeNumber = AbodeNumber::Wonder;
		wonder.tribeType = tribe;
		wonder.scaffoldsRequired = 7;
		wonder.desireToBeBuilt = 0.5f;
	}
	Locator::infoConstants::reset(info.release());
	const auto pos = map_coords::FromWorld(nullptr, glm::vec3(10.0f, 0.0f, 10.0f));
	constexpr auto k_Wonder = static_cast<int32_t>(AbodeNumber::Wonder);
	// seven scaffolds made at the Japanese workshop offer the Japanese wonder, wanted fully
	ScaffoldPlan plan;
	EXPECT_FLOAT_EQ(ecs::scaffold_plans::GetNewPlannedBuilding(_town, plan, pos, 0.0f, 1.0f, 7, entt::null, Tribe::JAPANESE,
	                                                           k_Wonder, true),
	                1.0f);
	EXPECT_EQ(plan.info, &Info(AbodeInfo::JapaneseWonder));
	// sized by the town's wonder power there: nothing wanted and nothing worth anything about, so its least, 0.25, and
	// min(power, 5) x (1 - power / 10)
	EXPECT_FLOAT_EQ(plan.scale, 0.25f * (1.0f - 0.25f * 0.1f));
	// a scaffold of no tribe offers the town's own tribe's
	ScaffoldPlan own;
	EXPECT_FLOAT_EQ(
	    ecs::scaffold_plans::GetNewPlannedBuilding(_town, own, pos, 0.0f, 1.0f, 7, entt::null, Tribe::NONE, k_Wonder, true),
	    1.0f);
	EXPECT_EQ(own.info, &Info(AbodeInfo::CelticWonder));
	// six are not enough
	ScaffoldPlan none;
	EXPECT_EQ(ecs::scaffold_plans::GetNewPlannedBuilding(_town, none, pos, 0.0f, 1.0f, 6, entt::null, Tribe::JAPANESE, k_Wonder,
	                                                     true),
	          0.0f);
	EXPECT_EQ(none.info, nullptr);
}

TEST(AbodeMorph, ThePitchAndFiveTribesWondersDrapeOverTheLand)
{
	using openblack::ecs::archetypes::AbodeArchetype;
	// the football pitch, whatever its tribe
	EXPECT_TRUE(AbodeArchetype::MorphsWithTerrain(AbodeType::FootballPitch, Tribe::CELTIC));
	EXPECT_TRUE(AbodeArchetype::MorphsWithTerrain(AbodeType::FootballPitch, Tribe::GREEK));
	// the Celtic, Japanese, Indian, Norse and Tibetan wonders, and a wonder of no tribe; the African, Aztec, Egyptian and
	// Greek ones stand on it
	for (const auto tribe : {Tribe::NONE, Tribe::CELTIC, Tribe::JAPANESE, Tribe::INDIAN, Tribe::NORSE, Tribe::TIBETAN})
	{
		EXPECT_TRUE(AbodeArchetype::MorphsWithTerrain(AbodeType::Wonder, tribe));
	}
	for (const auto tribe : {Tribe::AFRICAN, Tribe::AZTEC, Tribe::EGYPTIAN, Tribe::GREEK})
	{
		EXPECT_FALSE(AbodeArchetype::MorphsWithTerrain(AbodeType::Wonder, tribe));
	}
}

TEST(ScaffoldAdjust, OnlyAnOldAnyBuildingScaffoldIsFixed)
{
	using openblack::ecs::scaffolds::CanStillBeAdjustedAfter;
	EXPECT_TRUE(CanStillBeAdjustedAfter(0, Scaffold::k_AnyAbode, 150));
	EXPECT_TRUE(CanStillBeAdjustedAfter(149, Scaffold::k_AnyAbode, 150));
	EXPECT_FALSE(CanStillBeAdjustedAfter(150, Scaffold::k_AnyAbode, 150));
	EXPECT_TRUE(CanStillBeAdjustedAfter(500, 3, 150)); // limited to one building: still adjustable
}

TEST(ScaffoldPhantom, AScriptTurnsThePhantomBuildingAndItStopsTurningByItself)
{
	using openblack::ecs::scaffolds::PhantomAngleByScript;
	using openblack::ecs::scaffolds::SetPhantomAngleByScript;
	using openblack::script::property_rules::AngleFromScript;
	using openblack::script::property_rules::AngleToScript;
	Scaffold scaffold;
	scaffold.phantomAngle = 1.0f;
	scaffold.flags = Scaffold::k_Destroy;
	// until a script turns it, its angle reads 0, whatever the phantom's own
	EXPECT_EQ(PhantomAngleByScript(scaffold), 0.0f);
	// 160 degrees: the original's 2.7925267 radians, and the phantom no longer turns; the other flags stay
	SetPhantomAngleByScript(scaffold, AngleFromScript(160.0f));
	EXPECT_EQ(std::bit_cast<uint32_t>(scaffold.phantomAngle), 0x4032B8C2u);
	EXPECT_EQ(scaffold.flags, Scaffold::k_Destroy | Scaffold::k_NoPhantomRotation);
	EXPECT_EQ(PhantomAngleByScript(scaffold), scaffold.phantomAngle);
	// read back in degrees with no wrapping, as the original's -7.5 radians give -429.71832
	SetPhantomAngleByScript(scaffold, -7.5f);
	EXPECT_EQ(std::bit_cast<uint32_t>(AngleToScript(PhantomAngleByScript(scaffold))), 0xC3D6DBF2u);
	EXPECT_EQ(std::bit_cast<uint32_t>(AngleFromScript(720.5f)), 0x41493399u);
}

namespace
{
/// The spot visuals started, by their kind and where
class SpotVisuals final: public test::InertParticleSystem
{
public:
	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> /*turns*/, entt::entity /*owner*/,
	                             float /*magnitude*/) override
	{
		started.emplace_back(type, position);
		return entt::null;
	}
	std::vector<std::pair<SpotVisualType, glm::vec3>> started;
};
} // namespace

TEST_F(BuildingSitesTest, AScaffoldLetGoOfGentlyIsMarkedAndOtherwisePuffs)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& visuals = static_cast<SpotVisuals&>(Locator::particleSystem::emplace<SpotVisuals>());
	// far from any town, so that nothing takes it into a workshop's slot
	const auto scaffold = ecs::scaffolds::Create(map_coords::FromWorld(nullptr, glm::vec3(300.0f, 0.0f, 300.0f)), entt::null,
	                                             entt::null, 0.0f, 1.0f);
	auto& c = Reg().Get<Scaffold>(scaffold);
	// let go of by the hand, it is put down gently when it landed and the hand did not throw it to be let fall
	ecs::PhysicsEntry po;
	po.entity = scaffold;
	po.flags = ecs::PhysicsEntry::k_FromHand | ecs::PhysicsEntry::k_Landed;
	ecs::scaffolds::OnInitialisePhysicsFromHand(scaffold, po, false);
	EXPECT_NE(c.flags & Scaffold::k_DroppedGently, 0);
	ecs::scaffolds::OnInitialisePhysicsFromHand(scaffold, po, true);
	EXPECT_EQ(c.flags & Scaffold::k_DroppedGently, 0);
	c.flags |= Scaffold::k_DroppedGently;
	po.flags = ecs::PhysicsEntry::k_FromHand;
	ecs::scaffolds::OnInitialisePhysicsFromHand(scaffold, po, false);
	EXPECT_EQ(c.flags & Scaffold::k_DroppedGently, 0);

	// thrown from the hand and come to rest where nothing takes it: the "no building here" puff (spot visual 4) where it
	// is; then no holder, placed this turn
	game_clock::SetTurn(77);
	c.holder = PlayerNames::PLAYER_ONE;
	ecs::map_cells::RemoveMapObject(scaffold);
	EXPECT_EQ(ecs::scaffolds::OnEndPhysics(scaffold, true), scaffold);
	ASSERT_EQ(visuals.started.size(), 1u);
	EXPECT_EQ(visuals.started.front().first, SpotVisualType::SpellFailCast);
	EXPECT_NEAR(visuals.started.front().second.x, 300.0f, 0.01f);
	EXPECT_NEAR(visuals.started.front().second.z, 300.0f, 0.01f);
	const auto& landed = Reg().Get<const Scaffold>(scaffold);
	EXPECT_FALSE(landed.holder.has_value());
	EXPECT_EQ(landed.placedTurn, 77u);
	EXPECT_EQ(landed.flags & Scaffold::k_DroppedGently, 0);

	// knocked over, not from the hand: no puff, and it is placed all the same
	game_clock::SetTurn(78);
	Reg().Get<Scaffold>(scaffold).holder = PlayerNames::PLAYER_ONE;
	ecs::map_cells::RemoveMapObject(scaffold);
	EXPECT_EQ(ecs::scaffolds::OnEndPhysics(scaffold, false), scaffold);
	EXPECT_EQ(visuals.started.size(), 1u);
	EXPECT_FALSE(Reg().Get<const Scaffold>(scaffold).holder.has_value());
	EXPECT_EQ(Reg().Get<const Scaffold>(scaffold).placedTurn, 78u);
}

// ---- the workshop (docs/wiki/world/building-sites.md#workshop-details) --------------------------------------------

TEST_F(BuildingSitesTest, AFinishedWorkshopTakesAScaffoldsWoodAtOnceAndMakesItInTwoHundredTurns)
{
	// with room and at least the scaffold's 2500 wood, the 2500 are taken at once and the countdown is set to 200 game
	// turns; on the 200th turn after that a scaffold of one appears at the first free yard place, the workshop's own,
	// and the ready horn (the in-game bank's sample 150) sounds at the workshop
	UseWorkshopInfo();
	const test::RestoreService<Locator::audio> keepAudio;
	auto& sounds = static_cast<TapSounds&>(Locator::audio::emplace<TapSounds>());
	const auto isHorn = [](const audio::PlayOptions& options) {
		return options.sample.bank == audio::Bank(audio::SfxBank::InGame) && options.sample.number == 150;
	};
	const auto workshop = MakeWorkshop(glm::vec3(30.0f, 0.0f, 40.0f), false);
	// short of the 2500: nothing starts
	Reg().Get<Abode>(workshop).woodAmount = 2499;
	workshops::Process(workshop);
	EXPECT_EQ(Reg().Get<const Workshop>(workshop).countdown, 0);
	EXPECT_EQ(Reg().Get<const Abode>(workshop).woodAmount, 2499u);
	// 2600: the 2500 go at once and 100 stay
	Reg().Get<Abode>(workshop).woodAmount = 2600;
	workshops::Process(workshop);
	EXPECT_EQ(Reg().Get<const Abode>(workshop).woodAmount, 100u);
	EXPECT_EQ(Reg().Get<const Workshop>(workshop).countdown, 200);
	for (int turn = 1; turn < 200; ++turn)
	{
		workshops::Process(workshop);
	}
	EXPECT_TRUE(Scaffolds().empty());
	EXPECT_EQ(std::ranges::count_if(sounds.played, isHorn), 0);
	// the 200th turn
	workshops::Process(workshop);
	const auto made = Scaffolds();
	ASSERT_EQ(made.size(), 1u);
	const auto first = made.front();
	EXPECT_EQ(ecs::scaffolds::GetValue(first), 1u);
	EXPECT_EQ(ecs::scaffolds::GetOwner(first), workshop);
	EXPECT_TRUE(workshops::OwnsScaffold(workshop, first));
	EXPECT_EQ(ecs::scaffolds::GetSlot(first), 0);
	EXPECT_EQ(workshops::GetSlotState(workshop, 0), Workshop::k_SlotOccupied);
	EXPECT_EQ(Reg().Get<const Workshop>(workshop).countdown, 0);
	ASSERT_EQ(std::ranges::count_if(sounds.played, isHorn), 1);
	const auto& horn = *std::ranges::find_if(sounds.played, isHorn);
	EXPECT_TRUE(horn.is3D);
	EXPECT_FALSE(horn.track);
	EXPECT_TRUE(horn.owner == audio::Owner::Thing(workshop));
	EXPECT_NEAR(horn.position.x, 30.0f, 0.01f);
	EXPECT_NEAR(horn.position.z, 40.0f, 0.01f);
	// the next one goes to the next free place
	Reg().Get<Abode>(workshop).woodAmount = 2500;
	for (int turn = 0; turn <= 200; ++turn)
	{
		workshops::Process(workshop);
	}
	const auto both = Scaffolds();
	ASSERT_EQ(both.size(), 2u);
	const auto second = both.front() == first ? both.back() : both.front();
	EXPECT_EQ(ecs::scaffolds::GetSlot(second), 1);
	EXPECT_EQ(workshops::GetSlotState(workshop, 1), Workshop::k_SlotOccupied);
	EXPECT_EQ(Reg().Get<const Abode>(workshop).woodAmount, 0u);
}

TEST_F(BuildingSitesTest, TheYardHoldsThreeLessTheOneBeingMadeAndTheScaffoldsItCounts)
{
	// room = 3 - (a scaffold being made) - the scaffolds it counts as its own, signed; the record's maximum (5 here) is
	// not read
	UseWorkshopInfo();
	const auto workshop = MakeWorkshop(glm::vec3(30.0f, 0.0f, 40.0f), false);
	EXPECT_EQ(workshops::GetSpaceInStore(workshop), 3);
	Reg().Get<Workshop>(workshop).countdown = 120;
	EXPECT_EQ(workshops::GetSpaceInStore(workshop), 2);
	Reg().Get<Workshop>(workshop).owned = 2;
	EXPECT_EQ(workshops::GetSpaceInStore(workshop), 0);
	Reg().Get<Workshop>(workshop).countdown = 0;
	EXPECT_EQ(workshops::GetSpaceInStore(workshop), 1);
	Reg().Get<Workshop>(workshop).owned = 3;
	EXPECT_EQ(workshops::GetSpaceInStore(workshop), 0);
	Reg().Get<Workshop>(workshop).countdown = 1;
	EXPECT_EQ(workshops::GetSpaceInStore(workshop), -1);
	// a full yard starts nothing, whatever its wood
	Reg().Get<Workshop>(workshop).countdown = 0;
	Reg().Get<Abode>(workshop).woodAmount = 5000;
	workshops::Process(workshop);
	EXPECT_EQ(Reg().Get<const Workshop>(workshop).countdown, 0);
	EXPECT_EQ(Reg().Get<const Abode>(workshop).woodAmount, 5000u);
}

TEST_F(BuildingSitesTest, TheWoodFlagRisesWithTheWoodStillMissingForTheNextScaffold)
{
	// 1 - min((wood + 0.0001) / (2500 + 0.0001), 1)
	UseWorkshopInfo();
	const auto workshop = MakeWorkshop(glm::vec3(30.0f, 0.0f, 40.0f));
	const auto withWood = [workshop](uint32_t wood) {
		Reg().Get<Abode>(workshop).woodAmount = wood;
		return workshops::GetVisualWoodDesire(workshop);
	};
	EXPECT_FLOAT_EQ(withWood(0), 1.0f);
	EXPECT_FLOAT_EQ(withWood(625), 0.75f);
	EXPECT_FLOAT_EQ(withWood(1250), 0.5f);
	EXPECT_FLOAT_EQ(withWood(2500), 0.0f);
	EXPECT_FLOAT_EQ(withWood(7000), 0.0f);
}

TEST_F(BuildingSitesTest, OverTheWoodFlagTheHandShowsTheWoodWantedForTheFreePlaces)
{
	// room > 0 ? 2500 x room - wood, an unsigned 32-bit number that wraps past zero : 0
	UseWorkshopInfo();
	const auto workshop = MakeWorkshop(glm::vec3(30.0f, 0.0f, 40.0f));
	EXPECT_EQ(workshops::WoodWanted(workshop), 7500u);
	Reg().Get<Abode>(workshop).woodAmount = 1000;
	EXPECT_EQ(workshops::WoodWanted(workshop), 6500u);
	// one place free and more wood than it needs: wrapped round
	Reg().Get<Workshop>(workshop).owned = 2;
	Reg().Get<Abode>(workshop).woodAmount = 3000;
	EXPECT_EQ(workshops::WoodWanted(workshop), 4294966796u);
	// no room: none
	Reg().Get<Workshop>(workshop).owned = 3;
	EXPECT_EQ(workshops::WoodWanted(workshop), 0u);
	Reg().Get<Workshop>(workshop).owned = 4;
	EXPECT_EQ(workshops::WoodWanted(workshop), 0u);
}

TEST_F(BuildingSitesTest, AFinishedWorkshopsWoodHasNoLimit)
{
	// the workshop's pile is a magic wood pile, whose kind is never clipped at its maximum, and a pot holds a 32-bit
	// amount: 70000 wood go to the pile and its mirror whole
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	auto& row = info->pot.at(static_cast<size_t>(PotInfo::MagicWood));
	row.resourceType = ResourceType::Wood;
	row.maxAmountInPot = 1000;
	row.nextPotForResource = PotInfo::_COUNT; // none: no cap
	Locator::infoConstants::reset(info.release());
	const auto workshop = MakeWorkshop(glm::vec3(30.0f, 0.0f, 40.0f), false);
	const auto pile = Reg().Create();
	Reg().Assign<Transform>(pile, glm::vec3(30.0f, 0.0f, 40.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<Pot>(pile, Pot {.amount = 0, .maxAmount = 1000, .type = PotInfo::MagicWood});
	Reg().Get<Workshop>(workshop).woodPile = pile;

	EXPECT_EQ(workshops::AddResource(workshop, ResourceType::Wood, 70000), 70000u);
	EXPECT_EQ(Reg().Get<const Pot>(pile).amount, 70000u);
	EXPECT_EQ(Reg().Get<const Abode>(workshop).woodAmount, 70000u);
}

TEST_F(BuildingSitesTest, ADestroyedWorkshopFreesItsScaffoldsWhereTheyStand)
{
	// the town's list loses the workshop; each of its scaffolds loses its owner and stays where it is; the workshop's
	// own count and places are left as they were
	UseWorkshopInfo();
	const auto workshop = MakeWorkshop(glm::vec3(30.0f, 0.0f, 40.0f));
	workshops::AddWorkshop(_town, workshop);
	const auto a = MakeYardScaffold(workshop, 0);
	const auto b = MakeYardScaffold(workshop, 1);
	const glm::vec3 whereA = Reg().Get<const Transform>(a).position;
	const glm::vec3 whereB = Reg().Get<const Transform>(b).position;
	EXPECT_FALSE(ecs::scaffolds::CanBecomePhysicsObject(a));
	workshops::DeleteDependants(workshop);
	EXPECT_TRUE(TownData().workshops.empty());
	for (const auto& [scaffold, where] : {std::pair {a, whereA}, std::pair {b, whereB}})
	{
		ASSERT_TRUE(Reg().Valid(scaffold));
		EXPECT_EQ(ecs::scaffolds::GetOwner(scaffold), entt::entity {entt::null});
		EXPECT_FALSE(workshops::OwnsScaffold(workshop, scaffold));
		EXPECT_EQ(Reg().Get<const Transform>(scaffold).position, where);
		EXPECT_TRUE(ecs::scaffolds::CanBecomePhysicsObject(scaffold));
	}
	EXPECT_EQ(Reg().Get<const Workshop>(workshop).owned, 2);
	EXPECT_EQ(workshops::GetSlotState(workshop, 0), Workshop::k_SlotOccupied);
	EXPECT_EQ(workshops::GetSlotState(workshop, 1), Workshop::k_SlotOccupied);
}

TEST_F(BuildingSitesTest, AScaffoldSittingInTheYardCannotBeKnockedAbout)
{
	// in a place of its workshop (owner and place byte 2) a scaffold cannot become a physics object; once its place is
	// only kept for it, or free, it can
	const auto workshop = MakeWorkshop(glm::vec3(30.0f, 0.0f, 40.0f));
	const auto scaffold = MakeYardScaffold(workshop, 2);
	EXPECT_FALSE(ecs::scaffolds::CanBecomePhysicsObject(scaffold));
	Reg().Get<Workshop>(workshop).slots.at(2) = Workshop::k_SlotTakenAway;
	EXPECT_TRUE(ecs::scaffolds::CanBecomePhysicsObject(scaffold));
	Reg().Get<Workshop>(workshop).slots.at(2) = Workshop::k_SlotFree;
	Reg().Get<Workshop>(workshop).slots.at(0) = Workshop::k_SlotOccupied;
	EXPECT_TRUE(ecs::scaffolds::CanBecomePhysicsObject(scaffold));
}

TEST_F(BuildingSitesTest, AScaffoldTakenFromTheYardKeepsItsPlaceReserved)
{
	// the hand's pick-up tells its workshop it moved: its place becomes kept for it (byte 1) and the workshop still
	// counts it as its own, so the room does not grow and the next scaffold takes another place
	UseWorkshopInfo();
	const auto workshop = MakeWorkshop(glm::vec3(30.0f, 0.0f, 40.0f));
	const auto scaffold = MakeYardScaffold(workshop, 0);
	EXPECT_EQ(workshops::GetSpaceInStore(workshop), 2);
	ecs::scaffolds::OnPickedUp(scaffold, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(workshops::GetSlotState(workshop, 0), Workshop::k_SlotTakenAway);
	EXPECT_EQ(ecs::scaffolds::GetOwner(scaffold), workshop);
	EXPECT_TRUE(workshops::OwnsScaffold(workshop, scaffold));
	EXPECT_EQ(workshops::GetSpaceInStore(workshop), 2);
	EXPECT_EQ(workshops::GetFirstFreeSlot(workshop), 1);
}

TEST_F(BuildingSitesTest, AScaffoldIsWorthTheScaffoldsWoodForEachItStandsFor)
{
	// its wood is its value x the scaffold's 2500 (docs/wiki/world/building-sites.md#scaffold-details)
	UseWorkshopInfo();
	const auto scaffold =
	    ecs::scaffolds::Create(map_coords::FromWorld(nullptr, glm::vec3(10.0f, 0.0f, 10.0f)), _town, entt::null, 0.0f, 1.0f);
	ecs::scaffolds::SetValue(scaffold, 1);
	EXPECT_EQ(ecs::scaffolds::GetDefaultResource(scaffold), 2500u);
	ecs::scaffolds::SetValue(scaffold, 3);
	EXPECT_EQ(ecs::scaffolds::GetDefaultResource(scaffold), 7500u);
	ecs::scaffolds::SetValue(scaffold, 7);
	EXPECT_EQ(ecs::scaffolds::GetDefaultResource(scaffold), 17500u);
}

TEST_F(BuildingSitesTest, AScaffoldOffersItsBuildingToTheTownOfTheNearestBuilding)
{
	// the scaffold is a mobile object, type 20 in info.dat (an empty row's type would be 0, an abode's), so it is not
	// the building it looks for
	auto rows = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	rows->mobileObject.at(static_cast<size_t>(MobileObjectInfo::OldScaffold)).type = ObjectType::MobileObject;
	Locator::infoConstants::reset(rows.release());
	// the nearest building within 50 m decides, whoever its town belongs to, even with a town nearer than it
	Reg().Assign<Transform>(_town, glm::vec3(505.0f, 0.0f, 500.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto other = MakeTownAt(2, PlayerNames::PLAYER_TWO, glm::vec3(560.0f, 0.0f, 500.0f));
	MakeBuildingAt(k_TownId, glm::vec3(520.0f, 0.0f, 500.0f));
	const auto theirs = MakeBuildingAt(2, glm::vec3(490.0f, 0.0f, 500.0f));
	const auto scaffold = ecs::scaffolds::Create(map_coords::FromWorld(nullptr, glm::vec3(500.0f, 0.0f, 500.0f)), entt::null,
	                                             entt::null, 0.0f, 1.0f);
	EXPECT_EQ(ecs::scaffolds::GetTownForBuilding(scaffold), other);
	ecs::map_cells::RemoveMapObject(theirs);
	EXPECT_EQ(ecs::scaffolds::GetTownForBuilding(scaffold), _town);
}

TEST_F(BuildingSitesTest, WithNoBuildingNearAScaffoldOffersItsBuildingToTheNearestTownWithinFiftyMetres)
{
	// no building within 50 m: the nearest town of any player below 50 m; none, no town
	Reg().Assign<Transform>(_town, glm::vec3(540.0f, 0.0f, 500.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto other = MakeTownAt(2, PlayerNames::PLAYER_TWO, glm::vec3(530.0f, 0.0f, 500.0f));
	MakeBuildingAt(k_TownId, glm::vec3(700.0f, 0.0f, 500.0f));
	const auto scaffold = ecs::scaffolds::Create(map_coords::FromWorld(nullptr, glm::vec3(500.0f, 0.0f, 500.0f)), entt::null,
	                                             entt::null, 0.0f, 1.0f);
	EXPECT_EQ(ecs::scaffolds::GetTownForBuilding(scaffold), other);
	Reg().Get<Transform>(other).position = glm::vec3(560.0f, 0.0f, 500.0f);
	EXPECT_EQ(ecs::scaffolds::GetTownForBuilding(scaffold), _town);
	Reg().Get<Transform>(_town).position = glm::vec3(551.0f, 0.0f, 500.0f);
	EXPECT_EQ(ecs::scaffolds::GetTownForBuilding(scaffold), entt::entity {entt::null});
	// a town's planned building below 50 m stands for its town, wherever the town itself is
	TownData().plannedAbodes.push_back({k_House, glm::vec3(520.0f, 0.0f, 500.0f), 0.0f, 1.0f, false});
	EXPECT_EQ(ecs::scaffolds::GetTownForBuilding(scaffold), _town);
}

TEST_F(BuildingSitesTest, WoodGoesToTheNearestFinishedWorkshopThatWantsIt)
{
	// each workshop of the town: the distance modifier out to 500 m x its wish to be supplied (room, or wood still
	// missing: 1; else 0); the strictly best of the finished ones (docs/wiki/beings/towns.md#workshops)
	UseWorkshopInfo();
	const auto farther = MakeWorkshop(glm::vec3(400.0f, 0.0f, 100.0f));
	const auto nearer = MakeWorkshop(glm::vec3(120.0f, 0.0f, 100.0f));
	workshops::AddWorkshop(_town, nearer);
	workshops::AddWorkshop(_town, farther);
	ASSERT_EQ(TownData().workshops.front(), farther);
	const auto from = map_coords::FromWorld(nullptr, glm::vec3(100.0f, 0.0f, 100.0f));
	EXPECT_EQ(workshops::GetBestWorkshop(_town, from, true, true), nearer);
	// full, with the wood for its next scaffold: it wishes for none
	Reg().Get<Workshop>(nearer).owned = 3;
	Reg().Get<Abode>(nearer).woodAmount = 2500;
	EXPECT_FLOAT_EQ(workshops::GetDesireToBeSupplied(nearer), 0.0f);
	EXPECT_EQ(workshops::GetBestWorkshop(_town, from, true, true), farther);
	// full but short of wood: it still wishes for it
	Reg().Get<Abode>(nearer).woodAmount = 1000;
	EXPECT_FLOAT_EQ(workshops::GetDesireToBeSupplied(nearer), 1.0f);
	EXPECT_EQ(workshops::GetBestWorkshop(_town, from, true, true), nearer);
	// one not finished is passed over
	Reg().Get<Abode>(nearer).buildFlags = Abode::k_UnderConstruction;
	Reg().Get<Abode>(nearer).percentBuilt = 0.0f;
	EXPECT_EQ(workshops::GetBestWorkshop(_town, from, true, true), farther);
	// none that wishes for wood: none
	Reg().Get<Workshop>(farther).owned = 3;
	Reg().Get<Abode>(farther).woodAmount = 2500;
	EXPECT_EQ(workshops::GetBestWorkshop(_town, from, true, true), entt::entity {entt::null});
}
