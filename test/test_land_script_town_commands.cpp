/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The land script's town commands: CREATE_TOWN (its owner, its tribe, the last created object),
// SET_TOWN_UNINHABITABLE (and a town that then takes no villager), SET_TOWN_CONGREGATION_POS, CREATE_PLANNED_ABODE
// (the plan on the town's list, the nearest town without one), the abode info found by "<tribe>_<abode>", and the
// homeless list's head. No game data: a hand-made abode info table, and no meshes, so no abode is built here (a built
// abode, a workshop's starting wood and a town centre's worship share need their meshes). Then the object commands, on
// a flat land: lanterns, mobile statics and objects, the place test of trees, animals, planned features, worship sites.
// Last, the miracles a town holds: CREATE_NEW_TOWN_SPELL, CREATE_TOWN_SPELL, CREATE_TOWN_CENTRE_SPELL_ICON,
// CREATE_PLANNED_SPELL_ICON and CREATE_SPELL_ICON, with no village centre.

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "Common/EventManager.h"
#include "Common/GameRandomTesting.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Resources/ResourceManager.h"
#include "support/LandFakes.h"
#include "support/ParticleFakes.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::lhscriptx::FeatureScriptCommands;
using openblack::lhscriptx::Script;
namespace tv = openblack::ecs::town_villagers;

namespace
{
// The rows of the hand-made abode info table
constexpr auto k_CelticHouse = static_cast<AbodeInfo>(2);
constexpr auto k_CelticCentre = static_cast<AbodeInfo>(3);
constexpr auto k_NorseHouse = static_cast<AbodeInfo>(4);
constexpr auto k_NorseHouseAgain = static_cast<AbodeInfo>(5);
constexpr auto k_TribelessHouse = static_cast<AbodeInfo>(6);

/// The abode info row's tribe, description and type
void Describe(InfoConstants& table, AbodeInfo row, Tribe tribe, std::string_view description, AbodeType type)
{
	auto& info = table.abode.at(static_cast<size_t>(row));
	info.tribeType = tribe;
	info.abodeType = type;
	info.debugString.fill('\0');
	std::ranges::copy(description, info.debugString.begin());
}

class LandScriptTownCommandsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the commands log their misses to the scripts' logger: a null sink, as the other script tests
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		test::EmplaceMapAndVillagerDefaults();
		// every other row is a Celtic one with no description, which no name in these tests matches
		auto info = std::make_unique<InfoConstants>();
		Describe(*info, k_CelticHouse, Tribe::CELTIC, "ABODE_A", AbodeType::LivingQuarters);
		Describe(*info, k_CelticCentre, Tribe::CELTIC, "TOWN_CENTRE", AbodeType::TownCentre);
		Describe(*info, k_NorseHouse, Tribe::NORSE, "ABODE_A", AbodeType::LivingQuarters);
		Describe(*info, k_NorseHouseAgain, Tribe::NORSE, "ABODE_A", AbodeType::LivingQuarters);
		Describe(*info, k_TribelessHouse, Tribe::NONE, "TRIBELESS", AbodeType::LivingQuarters);
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::events::emplace<EventManager>();
	}

	void TearDown() override
	{
		Locator::terrainSystem::reset();
		Locator::handSystem::reset();
		Locator::events::reset();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	static Town& TownOf(entt::entity town) { return Reg().Get<Town>(town); }

	static entt::entity MakeTown(int id, glm::vec3 at)
	{
		return ecs::archetypes::TownArchetype::Create(id, at, PlayerNames::PLAYER_ONE, Tribe::CELTIC);
	}

	/// A villager of no town and no abode
	static entt::entity MakeVillager()
	{
		const auto e = Reg().Create();
		auto& villager = Reg().Assign<Villager>(e);
		villager.town = entt::null;
		villager.abode = entt::null;
		return e;
	}

	static size_t AbodeCount()
	{
		size_t count = 0;
		Reg().Each<const Abode>([&count](entt::entity /*abode*/, const Abode& /*component*/) { ++count; });
		return count;
	}
};
} // namespace

// CREATE_TOWN "NALNL": the town of the script's number, owned by the named player, of the named tribe; it becomes the
// last created object (docs/wiki/beings/towns.md#town-lists-and-queries,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptTownCommandsTest, CreateTownGivesTheTownItsOwnerAndTribe)
{
	FeatureScriptCommands::CreateTown(4, glm::vec3(120.0f, 0.0f, 80.0f), "PLAYER_TWO", 0, "NORSE");
	const auto town = ecs::town_queries::FindTownWithID(4);
	ASSERT_NE(town, entt::entity {entt::null});
	EXPECT_EQ(TownOf(town).owner, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(Reg().Get<const Tribe>(town), Tribe::NORSE);
	EXPECT_EQ(Script::LastCreated(), town);

	FeatureScriptCommands::CreateTown(5, glm::vec3(300.0f, 0.0f, 80.0f), "NEUTRAL", 0, "AZTEC");
	const auto neutral = ecs::town_queries::FindTownWithID(5);
	ASSERT_NE(neutral, entt::entity {entt::null});
	EXPECT_EQ(TownOf(neutral).owner, PlayerNames::NEUTRAL);
	EXPECT_EQ(Reg().Get<const Tribe>(neutral), Tribe::AZTEC);
	EXPECT_EQ(Script::LastCreated(), neutral);
}

// CREATE_TOWN reads its player's name whatever its case, and a name no player has gives the town to the neutral player,
// as the original (docs/wiki/scripts/map-script-reader.md#towns)
TEST_F(LandScriptTownCommandsTest, CreateTownGivesAnUnknownPlayersTownToTheNeutralPlayer)
{
	FeatureScriptCommands::CreateTown(6, glm::vec3(120.0f, 0.0f, 80.0f), "player_three", 0, "NORSE");
	const auto town = ecs::town_queries::FindTownWithID(6);
	ASSERT_TRUE(town != entt::null);
	EXPECT_EQ(TownOf(town).owner, PlayerNames::PLAYER_THREE);

	FeatureScriptCommands::CreateTown(7, glm::vec3(300.0f, 0.0f, 80.0f), "NO_SUCH_PLAYER", 0, "AZTEC");
	const auto unknown = ecs::town_queries::FindTownWithID(7);
	ASSERT_TRUE(unknown != entt::null);
	EXPECT_EQ(TownOf(unknown).owner, PlayerNames::NEUTRAL);
	EXPECT_EQ(Script::LastCreated(), unknown);
}

// SET_TOWN_UNINHABITABLE marks the town; a number no town has does nothing. An uninhabitable town then takes no
// villager: the villager keeps no town and goes on no homeless list
// (docs/wiki/scripts/map-script-reader.md#towns, docs/wiki/beings/towns.md#villagers-joining-and-leaving-the-town)
TEST_F(LandScriptTownCommandsTest, AnUninhabitableTownTakesNoVillager)
{
	const auto town = MakeTown(2, glm::vec3(100.0f, 0.0f, 100.0f));
	const auto other = MakeTown(3, glm::vec3(400.0f, 0.0f, 100.0f));
	EXPECT_FALSE(TownOf(town).uninhabitable);
	FeatureScriptCommands::SetTownUninhabitable(9); // no such town
	EXPECT_FALSE(TownOf(town).uninhabitable);
	EXPECT_FALSE(TownOf(other).uninhabitable);
	FeatureScriptCommands::SetTownUninhabitable(2);
	EXPECT_TRUE(TownOf(town).uninhabitable);
	EXPECT_FALSE(TownOf(other).uninhabitable);

	const auto villager = MakeVillager();
	EXPECT_FALSE(tv::AddVillagerToTown(town, villager));
	EXPECT_EQ(Reg().Get<const Villager>(villager).town, entt::entity {entt::null});
	EXPECT_TRUE(tv::Homeless(town).empty());
}

// SET_TOWN_CONGREGATION_POS: the town's congregation position is the script position, x and z in map units (metres x
// 65536 / 10, truncated, as the original) with the height 0 of a two-field position; a number no town has does nothing
// (docs/wiki/scripts/map-script-reader.md#towns, docs/wiki/scripts/map-script-reader.md#the-map-script-reader-lhscriptx)
TEST_F(LandScriptTownCommandsTest, SetTownCongregationPosSetsTheTownsCongregationPosition)
{
	const auto town = MakeTown(1, glm::vec3(100.0f, 0.0f, 100.0f));
	TownOf(town).congregationPosY = 3.0f;
	FeatureScriptCommands::SetTownCongregationPos(1, glm::vec3(25.0f, 7.0f, 10.0f));
	EXPECT_EQ(TownOf(town).congregationPos, glm::ivec2(163840, 65536));
	EXPECT_EQ(TownOf(town).congregationPosY, 0.0f); // not the terrain height openblack's parser gives the vector

	FeatureScriptCommands::SetTownCongregationPos(8, glm::vec3(50.0f, 0.0f, 50.0f)); // no such town
	EXPECT_EQ(TownOf(town).congregationPos, glm::ivec2(163840, 65536));
}

// A villager made homeless goes to the head of the town's homeless list, so the list is newest first
// (docs/wiki/beings/villager-life.md#town-homeless-and-vagrants)
TEST_F(LandScriptTownCommandsTest, AddToHomelessListPutsTheVillagerAtTheHead)
{
	const auto town = MakeTown(1, glm::vec3(100.0f, 0.0f, 100.0f));
	const auto first = MakeVillager();
	const auto second = MakeVillager();
	const auto third = MakeVillager();
	tv::AddToHomelessList(town, first);
	tv::AddToHomelessList(town, second);
	tv::AddToHomelessList(town, third);
	EXPECT_EQ(tv::Homeless(town), (std::vector<entt::entity> {third, second, first}));
	EXPECT_TRUE(tv::IsVillagerInHomelessList(town, first));
	// out of the list, the order of the others is kept
	EXPECT_TRUE(tv::RemoveFromHomelessList(town, second));
	EXPECT_EQ(tv::Homeless(town), (std::vector<entt::entity> {third, first}));
}

// CREATE_PLANNED_ABODE: a plan of the named abode on the town's planned list, at the script position, turned by N4 x
// 0.001 and scaled by N5 x 0.001 as the original, the food and wood not used; a town centre's type makes a planned
// town centre (docs/wiki/scripts/map-script-reader.md#towns)
TEST_F(LandScriptTownCommandsTest, CreatePlannedAbodePutsThePlanOnTheTownsList)
{
	const auto town = MakeTown(3, glm::vec3(100.0f, 0.0f, 100.0f));
	const glm::vec3 at(110.0f, 0.0f, 95.0f);
	FeatureScriptCommands::CreatePlannedAbode(3, at, "CELTIC_ABODE_A", 1500, 1200, 300, 400);
	const glm::vec3 centreAt(90.0f, 0.0f, 105.0f);
	FeatureScriptCommands::CreatePlannedAbode(3, centreAt, "celtic_town_centre", 250, 1000, 0, 0);

	const auto& plans = TownOf(town).plannedAbodes;
	ASSERT_EQ(plans.size(), 2u);
	EXPECT_EQ(plans.at(0).info, k_CelticHouse);
	EXPECT_EQ(plans.at(0).position, at);
	EXPECT_EQ(plans.at(0).yAngleRadians, 1500 * 0.001f);
	EXPECT_EQ(plans.at(0).scale, 1200 * 0.001f);
	EXPECT_FALSE(plans.at(0).townCentre);
	EXPECT_FALSE(plans.at(0).wasBuilt);
	EXPECT_EQ(plans.at(1).info, k_CelticCentre);
	EXPECT_EQ(plans.at(1).position, centreAt);
	EXPECT_EQ(plans.at(1).yAngleRadians, 250 * 0.001f);
	EXPECT_EQ(plans.at(1).scale, 1000 * 0.001f);
	EXPECT_TRUE(plans.at(1).townCentre);
	// a plan is not drawn: it makes no abode
	EXPECT_EQ(AbodeCount(), 0u);
}

// CREATE_PLANNED_ABODE with a number no town has: the nearest town, the newest first and then only a strictly nearer
// one, so an exact tie goes to the newer town; with no town at all, nothing
// (docs/wiki/scripts/map-script-reader.md#towns, docs/wiki/beings/towns.md#town-lists-and-queries)
TEST_F(LandScriptTownCommandsTest, CreatePlannedAbodeWithoutItsTownTakesTheNearestTown)
{
	FeatureScriptCommands::CreatePlannedAbode(7, glm::vec3(200.0f, 0.0f, 0.0f), "CELTIC_ABODE_A", 0, 1000, 0, 0);
	EXPECT_EQ(AbodeCount(), 0u); // no town: no plan anywhere

	const auto older = MakeTown(1, glm::vec3(100.0f, 0.0f, 0.0f));
	const auto newer = MakeTown(2, glm::vec3(300.0f, 0.0f, 0.0f));
	const auto far = MakeTown(3, glm::vec3(900.0f, 0.0f, 0.0f));
	// 100 m from both: the newer town
	FeatureScriptCommands::CreatePlannedAbode(7, glm::vec3(200.0f, 0.0f, 0.0f), "CELTIC_ABODE_A", 0, 1000, 0, 0);
	EXPECT_TRUE(TownOf(older).plannedAbodes.empty());
	EXPECT_EQ(TownOf(newer).plannedAbodes.size(), 1u);
	EXPECT_TRUE(TownOf(far).plannedAbodes.empty());
	// nearer the older one
	FeatureScriptCommands::CreatePlannedAbode(7, glm::vec3(120.0f, 0.0f, 0.0f), "CELTIC_ABODE_A", 0, 1000, 0, 0);
	EXPECT_EQ(TownOf(older).plannedAbodes.size(), 1u);
	EXPECT_EQ(TownOf(newer).plannedAbodes.size(), 1u);
	// the town's own number wins over a nearer town
	FeatureScriptCommands::CreatePlannedAbode(3, glm::vec3(120.0f, 0.0f, 0.0f), "CELTIC_ABODE_A", 0, 1000, 0, 0);
	EXPECT_EQ(TownOf(older).plannedAbodes.size(), 1u);
	EXPECT_EQ(TownOf(far).plannedAbodes.size(), 1u);
}

// openblack: a plan is not an entity, so CREATE_PLANNED_ABODE leaves no last created object; a name no abode info has
// is skipped, as for CREATE_ABODE and CREATE_TOWN_CENTRE, where the original goes on with no info. A town centre with
// no town anywhere makes nothing and is the last created object, none
TEST_F(LandScriptTownCommandsTest, PlansAndUnknownNamesLeaveNoAbode)
{
	Script::SetLastCreated(Reg().Create());
	FeatureScriptCommands::CreateTownCentre(1, glm::vec3(100.0f, 0.0f, 100.0f), "CELTIC_TOWN_CENTRE", 0, 1000, 500);
	EXPECT_EQ(Script::LastCreated(), entt::entity {entt::null});
	EXPECT_EQ(AbodeCount(), 0u);

	const auto town = MakeTown(1, glm::vec3(100.0f, 0.0f, 100.0f));
	Script::SetLastCreated(town);
	FeatureScriptCommands::CreatePlannedAbode(1, glm::vec3(110.0f, 0.0f, 100.0f), "CELTIC_ABODE_A", 0, 1000, 0, 0);
	EXPECT_EQ(Script::LastCreated(), entt::entity {entt::null});

	Script::SetLastCreated(town);
	FeatureScriptCommands::CreatePlannedAbode(1, glm::vec3(110.0f, 0.0f, 100.0f), "CELTIC_NO_SUCH_ABODE", 0, 1000, 0, 0);
	FeatureScriptCommands::CreateAbode(1, glm::vec3(110.0f, 0.0f, 100.0f), "CELTIC_NO_SUCH_ABODE", 0, 1000, 0, 2500);
	FeatureScriptCommands::CreateTownCentre(1, glm::vec3(110.0f, 0.0f, 100.0f), "CELTIC_NO_SUCH_ABODE", 0, 1000, 500);
	EXPECT_EQ(Script::LastCreated(), town);
	EXPECT_EQ(TownOf(town).plannedAbodes.size(), 1u);
	EXPECT_EQ(TownOf(town).centre, entt::entity {entt::null});
	EXPECT_EQ(AbodeCount(), 0u);
}

// The abode info by name: "<tribe>_<description>", the tribe's name and the description both without case, the '_'
// required, the first row that matches; an info of no tribe is never named; none otherwise
// (docs/wiki/scripts/map-script-objects.md#pots-features-and-flowers)
TEST_F(LandScriptTownCommandsTest, AbodeInfoIsFoundByTribeAndDescriptionWithoutCase)
{
	EXPECT_EQ(GAbodeInfo::Find("CELTIC_ABODE_A"), k_CelticHouse);
	EXPECT_EQ(GAbodeInfo::Find("celtic_abode_a"), k_CelticHouse);
	EXPECT_EQ(GAbodeInfo::Find("Celtic_Town_Centre"), k_CelticCentre);
	EXPECT_EQ(GAbodeInfo::Find("NORSE_ABODE_A"), k_NorseHouse); // the first of the two Norse rows
	EXPECT_EQ(GAbodeInfo::Find("nOrSe_aBoDe_A"), k_NorseHouse);
	EXPECT_EQ(GAbodeInfo::Find("AZTEC_ABODE_A"), AbodeInfo::None);    // no Aztec row
	EXPECT_EQ(GAbodeInfo::Find("NORSEABODE_A"), AbodeInfo::None);     // no '_'
	EXPECT_EQ(GAbodeInfo::Find("NORSE-ABODE_A"), AbodeInfo::None);    // another character
	EXPECT_EQ(GAbodeInfo::Find("NORSE_ABODE_AB"), AbodeInfo::None);   // the whole description
	EXPECT_EQ(GAbodeInfo::Find("NORSE_ABODE"), AbodeInfo::None);      // not a prefix of it
	EXPECT_EQ(GAbodeInfo::Find("NORSE"), AbodeInfo::None);            // the tribe alone
	EXPECT_EQ(GAbodeInfo::Find("CELTIC_TRIBELESS"), AbodeInfo::None); // the row of no tribe
	EXPECT_EQ(GAbodeInfo::Find("NONE_TRIBELESS"), AbodeInfo::None);
}

// The abode info by tribe and number: the first row of that number whose tribe is the tribe or none; none when no row
// matches, as the original (docs/wiki/scripts/map-script-objects.md#fields-and-big-forests)
TEST_F(LandScriptTownCommandsTest, AbodeInfoByTribeAndNumberIsTheFirstRowThatMatches)
{
	// the fixture's table, with the two Norse rows and the row of no tribe made field rows
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->abode.at(static_cast<size_t>(k_NorseHouse)).abodeNumber = AbodeNumber::Field;
	info->abode.at(static_cast<size_t>(k_NorseHouseAgain)).abodeNumber = AbodeNumber::Field;
	info->abode.at(static_cast<size_t>(k_TribelessHouse)).abodeNumber = AbodeNumber::Field;
	Locator::infoConstants::reset(info.release());
	EXPECT_EQ(GAbodeInfo::Find(Tribe::NORSE, AbodeNumber::Field), k_NorseHouse);
	EXPECT_EQ(GAbodeInfo::Find(Tribe::JAPANESE, AbodeNumber::Field), k_TribelessHouse);
	EXPECT_EQ(GAbodeInfo::Find(Tribe::NORSE, AbodeNumber::D), AbodeInfo::None);
}

// ---- The land script's object commands ------------------------------------------------------------------------------
// CREATE_STREET_LANTERN, CREATE_MOBILESTATIC, CREATE_MOBILE_STATIC, CREATE_MOBILEOBJECT, CREATE_ANIMAL, the place test
// of CREATE_TREE and CREATE_NEW_TREE, the planned features and CREATE_WORSHIP_SITE without a temple, on a flat land.
// No meshes, so what is sized from its mesh (a tree, a feature, a big forest, a field, a dispenser, a worship site) is
// not made here

namespace
{
/// The spot visuals started, as a bonfire starts its fire
class SpotVisualRecorder final: public test::InertParticleSystem
{
public:
	struct Spot
	{
		SpotVisualType type;
		std::optional<int> turns;
		entt::entity owner;
	};
	std::vector<Spot> spots;

	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 /*position*/, std::optional<int> turns, entt::entity owner,
	                             float /*magnitude*/) override
	{
		spots.push_back({.type = type, .turns = turns, .owner = owner});
		return entt::null;
	}
};

class LandScriptObjectCommandsTest: public LandScriptTownCommandsTest
{
protected:
	void SetUp() override
	{
		LandScriptTownCommandsTest::SetUp();
		// every row of a table has that table's object type, as in info.dat
		auto info = std::make_unique<InfoConstants>();
		for (auto& row : info->mobileStatic)
		{
			row.type = ObjectType::MobileStatic;
		}
		for (auto& row : info->mobileObject)
		{
			row.type = ObjectType::MobileObject;
		}
		for (auto& row : info->tree)
		{
			row.type = ObjectType::ForestTree;
		}
		for (auto& row : info->animal)
		{
			row.type = ObjectType::Animal;
		}
		info->animal.at(static_cast<size_t>(AnimalInfo::Sheep)).animalInfo = AnimalInfo::Sheep;
		Locator::infoConstants::reset(info.release());
		// a flat dry land at height 0
		Locator::terrainSystem::emplace<test::HeightFieldIsland>([](glm::vec2) { return 0.0f; });
		Locator::particleSystem::emplace<SpotVisualRecorder>();
	}

	void TearDown() override
	{
		Script::SetLastCreated(entt::null);
		LandScriptTownCommandsTest::TearDown();
		Locator::particleSystem::reset();
	}

	static SpotVisualRecorder& Spots() { return static_cast<SpotVisualRecorder&>(Locator::particleSystem::value()); }

	template <typename Component>
	static size_t Count()
	{
		size_t count = 0;
		Reg().Each<const Component>([&count](entt::entity /*entity*/, const Component& /*component*/) { ++count; });
		return count;
	}

	/// Something an earlier line made, as the last created object
	static entt::entity EarlierObject()
	{
		const auto earlier = Reg().Create();
		Script::SetLastCreated(earlier);
		return earlier;
	}

	/// A tree standing at `at` in the map's cells: a fixed object for the place test to meet
	static entt::entity TreeAt(glm::vec3 at)
	{
		const auto tree = Reg().Create();
		ecs::object_index::Assign(tree);
		Reg().Assign<Transform>(tree, at, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<Tree>(tree);
		ecs::map_cells::InsertMapObject(tree);
		return tree;
	}
};
} // namespace

// CREATE_STREET_LANTERN makes a lantern of the mobile static info N1: the street lantern's info gives a town light, with
// the town light's mesh and light, and any other info a country lantern, with the campfire's mesh and light, unturned at
// scale 1, as the original. The lantern is the last created object
// (docs/wiki/scripts/map-script-objects.md#street-lanterns,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptObjectCommandsTest, AStreetLanternIsATownLightOnlyForTheStreetLanternInfo)
{
	FeatureScriptCommands::CreateStreetLantern(glm::vec3(105.0f, 0.0f, 105.0f),
	                                           static_cast<int32_t>(MobileStaticInfo::StreetLantern));
	const auto town = Script::LastCreated();
	ASSERT_NE(town, entt::entity {entt::null});
	EXPECT_FALSE(Reg().Get<const StreetLantern>(town).country);
	EXPECT_EQ(Reg().Get<const Mesh>(town).id, resources::HashIdentifier(MeshId::ObjectTownLight));
	EXPECT_EQ(Reg().Get<const LanternLight>(town).type, 0);
	EXPECT_EQ(Reg().Get<const Transform>(town).rotation, glm::mat3(1.0f));
	EXPECT_EQ(Reg().Get<const Transform>(town).scale, glm::vec3(1.0f));

	// Land 1's field lanterns, info 59
	FeatureScriptCommands::CreateStreetLantern(glm::vec3(205.0f, 0.0f, 105.0f),
	                                           static_cast<int32_t>(MobileStaticInfo::CountryLantern));
	const auto country = Script::LastCreated();
	ASSERT_NE(country, entt::entity {entt::null});
	EXPECT_NE(country, town);
	EXPECT_TRUE(Reg().Get<const StreetLantern>(country).country);
	EXPECT_EQ(Reg().Get<const Mesh>(country).id, resources::HashIdentifier(MeshId::BuildingCampfire));
	EXPECT_EQ(Reg().Get<const LanternLight>(country).type, 1);
}

// CREATE_STREET_LANTERN makes nothing less than 0.5 m from a thing of the mobile static type in the place's map cell (a
// lantern, a fence), and the last created object is then none; 0.7 m away it is made, as the original
// (docs/wiki/scripts/map-script-objects.md#street-lanterns,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptObjectCommandsTest, NoStreetLanternIsMadeWithinHalfAMetreOfAMobileStatic)
{
	constexpr auto k_Street = static_cast<int32_t>(MobileStaticInfo::StreetLantern);
	FeatureScriptCommands::CreateStreetLantern(glm::vec3(105.0f, 0.0f, 105.0f), k_Street);
	ASSERT_NE(Script::LastCreated(), entt::entity {entt::null});
	FeatureScriptCommands::CreateStreetLantern(glm::vec3(105.3f, 0.0f, 105.0f), k_Street);
	EXPECT_EQ(Script::LastCreated(), entt::entity {entt::null});
	EXPECT_EQ(Count<StreetLantern>(), 1u);
	FeatureScriptCommands::CreateStreetLantern(glm::vec3(105.7f, 0.0f, 105.0f), k_Street);
	EXPECT_NE(Script::LastCreated(), entt::entity {entt::null});
	EXPECT_EQ(Count<StreetLantern>(), 2u);

	// a fence, made with a mobile static info, keeps a lantern away the same way
	FeatureScriptCommands::CreateMobileStatic(glm::vec3(305.0f, 0.0f, 105.0f), MobileStaticInfo::CeltFenceTall, 0.0f, 1.0f);
	ASSERT_NE(Script::LastCreated(), entt::entity {entt::null});
	FeatureScriptCommands::CreateStreetLantern(glm::vec3(305.0f, 0.0f, 105.2f),
	                                           static_cast<int32_t>(MobileStaticInfo::CountryLantern));
	EXPECT_EQ(Script::LastCreated(), entt::entity {entt::null});
	EXPECT_EQ(Count<StreetLantern>(), 2u);
}

// CREATE_MOBILESTATIC "ANFF": a mobile static of info N1 at the script position, with the Y angle F2 and the scale F3.
// The singing stone's base makes nothing, and the bonfire's info a bonfire with its fire for ever. What it made, none
// too, is the last created object (docs/wiki/scripts/map-script-objects.md#bonfires-mobile-statics-and-dead-trees,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptObjectCommandsTest, CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo)
{
	const glm::vec3 at(105.0f, 0.0f, 105.0f);
	FeatureScriptCommands::CreateMobileStatic(at, MobileStaticInfo::CeltFenceTall, 0.5f, 1.25f);
	const auto fence = Script::LastCreated();
	ASSERT_NE(fence, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<const MobileStatic>(fence).type, MobileStaticInfo::CeltFenceTall);
	EXPECT_EQ(Reg().Get<const Transform>(fence).position, at);
	EXPECT_EQ(Reg().Get<const Transform>(fence).scale, glm::vec3(1.25f));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(Reg(), fence), glm::vec3(0.0f, 0.5f, 0.0f));

	EarlierObject();
	FeatureScriptCommands::CreateMobileStatic(glm::vec3(205.0f, 0.0f, 105.0f), MobileStaticInfo::SingingStoneBase, 0.5f, 1.0f);
	EXPECT_EQ(Script::LastCreated(), entt::entity {entt::null});
	EXPECT_EQ(Count<MobileStatic>(), 1u);

	FeatureScriptCommands::CreateMobileStatic(glm::vec3(305.0f, 0.0f, 105.0f), MobileStaticInfo::Bonfire, 0.25f, 0.75f);
	const auto bonfire = Script::LastCreated();
	ASSERT_NE(bonfire, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<const MobileStatic>(bonfire).type, MobileStaticInfo::Bonfire);
	EXPECT_EQ(Reg().Get<const Transform>(bonfire).scale, glm::vec3(0.75f));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(Reg(), bonfire).y, 0.25f);
	ASSERT_EQ(Spots().spots.size(), 1u);
	EXPECT_EQ(Spots().spots.at(0).type, SpotVisualType::Bonfire);
	EXPECT_EQ(Spots().spots.at(0).turns, std::optional<int>(-1));
	EXPECT_EQ(Spots().spots.at(0).owner, bonfire);
}

// CREATE_MOBILE_STATIC "ANFFFFF": the position raised by F2, the X, Y and Z angles F3, F4 and F5 and the scale F6. The
// street lantern's info makes nothing (lanterns come only from CREATE_STREET_LANTERN), and the last created object is
// then none (docs/wiki/scripts/map-script-objects.md#bonfires-mobile-statics-and-dead-trees,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptObjectCommandsTest, CreateMobileStaticWithAnglesIsRaisedAndTurnedButMakesNoLantern)
{
	FeatureScriptCommands::CreateMobileUStatic(glm::vec3(105.0f, 0.0f, 105.0f), MobileStaticInfo::CeltFenceShort, 2.5f, 0.25f,
	                                           0.5f, 0.75f, 1.5f);
	const auto fence = Script::LastCreated();
	ASSERT_NE(fence, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<const MobileStatic>(fence).type, MobileStaticInfo::CeltFenceShort);
	EXPECT_EQ(Reg().Get<const Transform>(fence).position, glm::vec3(105.0f, 2.5f, 105.0f));
	EXPECT_EQ(Reg().Get<const Transform>(fence).scale, glm::vec3(1.5f));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(Reg(), fence), glm::vec3(0.25f, 0.5f, 0.75f));

	EarlierObject();
	FeatureScriptCommands::CreateMobileUStatic(glm::vec3(205.0f, 0.0f, 105.0f), MobileStaticInfo::StreetLantern, 0.0f, 0.0f,
	                                           0.0f, 0.0f, 1.0f);
	EXPECT_EQ(Script::LastCreated(), entt::entity {entt::null});
	EXPECT_EQ(Count<StreetLantern>(), 0u);
	EXPECT_EQ(Count<MobileStatic>(), 1u);
}

// CREATE_MOBILEOBJECT makes a mobile object of info N1, the last created object. On top of a fixed object of the place's
// map cell (a tree here) it makes nothing and writes no last created object, and the script goes on
// (docs/wiki/scripts/map-script-objects.md#isoktocreateatpos,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptObjectCommandsTest, CreateMobileObjectIsNotMadeOnTopOfAFixedObject)
{
	FeatureScriptCommands::CreateMobileObject(glm::vec3(105.0f, 0.0f, 105.0f), MobileObjectInfo::EgyptBarrel, 1500, 800);
	const auto barrel = Script::LastCreated();
	ASSERT_NE(barrel, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<const MobileObject>(barrel).type, MobileObjectInfo::EgyptBarrel);
	// the angle and the scale in thousandths, as the original's command reads them
	EXPECT_EQ(Reg().Get<const Transform>(barrel).scale, glm::vec3(800 * 0.001f));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(Reg(), barrel), glm::vec3(0.0f, 1500 * 0.001f, 0.0f));

	TreeAt(glm::vec3(205.0f, 0.0f, 105.0f));
	const auto earlier = EarlierObject();
	FeatureScriptCommands::CreateMobileObject(glm::vec3(205.0f, 0.0f, 105.0f), MobileObjectInfo::EgyptCart, 0, 1000);
	EXPECT_EQ(Script::LastCreated(), earlier);
	EXPECT_EQ(Count<MobileObject>(), 1u);
}

// CREATE_TREE and CREATE_NEW_TREE on top of a fixed object of the place's map cell (a tree here) make nothing and write
// no last created object (docs/wiki/scripts/map-script-objects.md#isoktocreateatpos,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptObjectCommandsTest, NoTreeIsMadeOnTopOfAFixedObject)
{
	const glm::vec3 at(105.0f, 0.0f, 105.0f);
	TreeAt(at);
	const auto earlier = EarlierObject();
	FeatureScriptCommands::CreateNewTree(0, at, TreeInfo::Birch, 1, 0.0f, 1.0f, 1.0f);
	EXPECT_EQ(Script::LastCreated(), earlier);
	FeatureScriptCommands::CreateTree(0, glm::vec3(105.4f, 0.0f, 105.0f), TreeInfo::Beech, 0, 1000);
	EXPECT_EQ(Script::LastCreated(), earlier);
	EXPECT_EQ(Count<Tree>(), 1u);
}

// CREATE_NEW_ANIMAL draws an age only for age 0: any other age is the animal's as it is, a negative one taken as the
// unsigned number it is written as, as the original
// (docs/wiki/scripts/map-script-reader.md#animals-and-flocks-create_flock-create_new_animal)
TEST_F(LandScriptObjectCommandsTest, CreateNewAnimalKeepsAnyAgeButNoughtAsItIsEvenANegativeOne)
{
	FeatureScriptCommands::CreateNewAnimal(glm::vec3(100.0f, 0.0f, 100.0f), static_cast<int32_t>(AnimalInfo::Sheep), 42, 0, 7);
	const auto seven = Script::LastCreated();
	ASSERT_TRUE(seven != entt::null);
	EXPECT_EQ(Reg().Get<const Animal>(seven).age, 7u);

	FeatureScriptCommands::CreateNewAnimal(glm::vec3(300.0f, 0.0f, 200.0f), static_cast<int32_t>(AnimalInfo::Sheep), 42, 0, -1);
	const auto negative = Script::LastCreated();
	ASSERT_TRUE(negative != entt::null);
	EXPECT_EQ(Reg().Get<const Animal>(negative).age, 0xFFFFFFFFu);
}

// CREATE_ANIMAL is CREATE_NEW_ANIMAL with age 0, which draws the age: GameRand(20) + 5 for an animal joining the flock of
// its id, GameRand(40) + 5 for one that gets a flock of its own, as the original
// (docs/wiki/scripts/map-script-reader.md#animals-and-flocks-create_flock-create_new_animal)
TEST_F(LandScriptObjectCommandsTest, CreateAnimalDrawsTheAgeByWhetherItJoinsAFlock)
{
	FeatureScriptCommands::CreateFlock(7, glm::vec3(100.0f, 0.0f, 100.0f), glm::vec3(100.0f, 0.0f, 100.0f), 0, 30, 0);
	const auto herd = Script::LastCreated();
	ASSERT_NE(herd, entt::entity {entt::null});

	const game_random::testing::ScopedState state;
	std::vector<uint32_t> draws;
	// every draw answers the highest number of its range
	game_random::testing::SetGameRand(
	    [&draws](uint32_t range) {
		    draws.push_back(range);
		    return range - 1;
	    },
	    [](float /*range*/) { return 0.0f; });

	FeatureScriptCommands::CreateAnimal(glm::vec3(100.0f, 0.0f, 100.0f), static_cast<int32_t>(AnimalInfo::Sheep), 7, 0);
	const auto sheep = Script::LastCreated();
	ASSERT_NE(sheep, entt::entity {entt::null});
	ASSERT_FALSE(draws.empty());
	EXPECT_EQ(draws.front(), 20u);
	EXPECT_EQ(Reg().Get<const Animal>(sheep).age, 24u);
	EXPECT_EQ(Reg().Get<const Animal>(sheep).flock, herd);

	draws.clear();
	FeatureScriptCommands::CreateAnimal(glm::vec3(300.0f, 0.0f, 200.0f), static_cast<int32_t>(AnimalInfo::Sheep), 42, 0);
	const auto alone = Script::LastCreated();
	ASSERT_NE(alone, entt::entity {entt::null});
	ASSERT_FALSE(draws.empty());
	EXPECT_EQ(draws.front(), 40u);
	EXPECT_EQ(Reg().Get<const Animal>(alone).age, 44u);
	EXPECT_NE(Reg().Get<const Animal>(alone).flock, herd);
	EXPECT_NE(Reg().Get<const Animal>(alone).flock, entt::entity {entt::null});
}

// CREATE_WORSHIP_SITE needs the named player's temple: without one it makes nothing and writes no last created object,
// even with a town of the tribe (docs/wiki/scripts/script-commands.md#worship,
// docs/wiki/magic/temple-and-citadel.md#the-citadel-and-its-six-slots,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptObjectCommandsTest, AWorshipSiteNeedsThePlayersTemple)
{
	MakeTown(1, glm::vec3(100.0f, 0.0f, 100.0f));
	const auto earlier = EarlierObject();
	FeatureScriptCommands::CreateWorshipSite(glm::vec3(105.0f, 0.0f, 105.0f), 0, "PLAYER_ONE", "CELTIC", 0, 0);
	EXPECT_EQ(Script::LastCreated(), earlier);
	EXPECT_EQ(Count<WorshipSite>(), 0u);
}

// CREATE_MIST "AFNFF" makes a patch of mist at the place raised F1 above the land there, of size F3 and of the ARGB
// colour N2, whose alpha is its top byte; a last value of 1 leaves the mist's plain look (its k at 3), any other turns
// the edge shrink on with that k. Each mist is the last created object. Land 1's line, then the same with a k of its
// own, as the original (docs/wiki/world/haze-and-mist.md#map-mist-create_mist,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptObjectCommandsTest, CreateMistIsRaisedAboveTheLandSizedAndColouredAsItsLine)
{
	Locator::terrainSystem::emplace<test::HeightFieldIsland>([](glm::vec2 at) { return 20.0f + (at.x * 0.01f); });
	const glm::vec3 at(2356.91f, 0.0f, 2782.03f);
	const float land = 20.0f + (at.x * 0.01f);
	FeatureScriptCommands::CreateMist(at, 0.0f, 1398505633, 2.373684f, 1.0f);
	const auto plain = Script::LastCreated();
	ASSERT_TRUE(plain != entt::null);
	const auto& mist = Reg().Get<const Mist>(plain);
	EXPECT_EQ(mist.size, 2.373684f);
	EXPECT_EQ(mist.colour, 1398505633u);
	EXPECT_EQ(mist.colour >> 24u, 83u);
	EXPECT_FALSE(mist.edgeShrink);
	EXPECT_EQ(mist.k, 3.0f);
	EXPECT_EQ(Reg().Get<const Transform>(plain).position, glm::vec3(at.x, land, at.z));

	FeatureScriptCommands::CreateMist(at, 4.5f, 1398505633, 2.373684f, 0.5f);
	const auto shrinking = Script::LastCreated();
	ASSERT_TRUE(shrinking != entt::null);
	EXPECT_TRUE(shrinking != plain);
	EXPECT_TRUE(Reg().Get<const Mist>(shrinking).edgeShrink);
	EXPECT_EQ(Reg().Get<const Mist>(shrinking).k, 0.5f);
	EXPECT_EQ(Reg().Get<const Transform>(shrinking).position, glm::vec3(at.x, 4.5f + land, at.z));
	EXPECT_EQ(Count<Mist>(), 2u);
}

// ---- Miracles held by a town ----------------------------------------------------------------------------------------

namespace
{
/// The seeds of the hand-made tables, where the game's file has them: the fire seed casts the fireball and its two
/// power-ups, the heal seed the heal and its power-up
constexpr size_t k_FireSeed = 2;
constexpr size_t k_HealSeed = 7;

/// Each magic record's own magic type set to where the record stands, as the game's file has them
void NumberMagicRecords(InfoConstants& info)
{
	uint32_t next = 0;
	const auto number = [&next](auto& records) {
		for (auto& record : records)
		{
			record.magicType = static_cast<MagicType>(next++);
		}
	};
	number(info.magicGeneral);
	number(info.magicHeal);
	number(info.magicTeleport);
	number(info.magicForest);
	number(info.magicFood);
	number(info.magicStormAndTornado);
	number(info.magicShield);
	number(info.magicWood);
	number(info.magicWater);
	number(info.magicFlockFlying);
	number(info.magicFlockGround);
	number(info.magicCreatureSpell);
}

/// A table row's name, as info.dat holds it
template <typename Row>
void Named(Row& row, std::string_view name)
{
	row.debugString.fill('\0');
	std::ranges::copy(name, row.debugString.begin());
}

/// The install the real-data tests read, from OPENBLACK_GAME_PATH; none when it is not set
std::optional<std::filesystem::path> RealGamePath()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		return std::nullopt;
	}
	return std::filesystem::path(game);
}

/// info.dat's table as the game reads it (after the file's header), or nothing when the file is missing or of another
/// size
std::unique_ptr<InfoConstants> ReadRealInfoDat(const std::filesystem::path& game)
{
	std::ifstream file(game / "Scripts" / "info.dat", std::ios::binary);
	if (!file.is_open())
	{
		return nullptr;
	}
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	if (data.size() != 0x2C + sizeof(InfoConstants))
	{
		return nullptr;
	}
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	return info;
}

/// The magic types the town holds, in order
std::vector<MagicType> HeldBy(entt::entity town)
{
	std::vector<MagicType> held;
	const auto& magic = Locator::entitiesRegistry::value().Get<const TownMagic>(town);
	for (size_t i = 0; i < magic.held.size(); ++i)
	{
		if (magic.held.at(i))
		{
			held.push_back(static_cast<MagicType>(i));
		}
	}
	return held;
}

/// How many holders give the first player the magic type
int32_t Holders(MagicType type)
{
	return magic::players::ReadMagicOf(PlayerNames::PLAYER_ONE).remainder.at(static_cast<size_t>(type));
}

/// No magic type given to the first player by any holder
bool NoHolders()
{
	const auto& remainder = magic::players::ReadMagicOf(PlayerNames::PLAYER_ONE).remainder;
	return std::ranges::all_of(remainder, [](int32_t count) { return count == 0; });
}

/// The town commands' tables with the miracles named: the magic records numbered, the effect rows named NONE, FIRE,
/// FIRE_PU1, FIRE_PU2, HEAL and HEAL_PU1, and the fire and heal seeds; the first player's town 5, with no village
/// centre, and something an earlier line made as the last created object
class LandScriptTownSpellCommandsTest: public LandScriptTownCommandsTest
{
protected:
	void SetUp() override
	{
		LandScriptTownCommandsTest::SetUp();
		auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
		NumberMagicRecords(*info);
		Named(info->magicEffect.at(static_cast<size_t>(MagicType::None)), "NONE");
		Named(info->magicEffect.at(static_cast<size_t>(MagicType::Fireball)), "FIRE");
		Named(info->magicEffect.at(static_cast<size_t>(MagicType::FireballPowerUpOne)), "FIRE_PU1");
		Named(info->magicEffect.at(static_cast<size_t>(MagicType::FireballPowerUpTwo)), "FIRE_PU2");
		Named(info->magicEffect.at(static_cast<size_t>(MagicType::Heal)), "HEAL");
		Named(info->magicEffect.at(static_cast<size_t>(MagicType::HealPowerUpOne)), "HEAL_PU1");
		auto& fire = info->spellSeed.at(k_FireSeed);
		Named(fire, "FIRE");
		fire.magicTypes = {MagicType::Fireball, MagicType::FireballPowerUpOne, MagicType::FireballPowerUpTwo, MagicType::None};
		auto& heal = info->spellSeed.at(k_HealSeed);
		Named(heal, "HEAL");
		heal.magicTypes = {MagicType::Heal, MagicType::HealPowerUpOne, MagicType::None, MagicType::None};
		Locator::infoConstants::reset(info.release());
		town = MakeTown(5, glm::vec3(100.0f, 0.0f, 100.0f));
		earlier = Reg().Create();
		Script::SetLastCreated(earlier);
	}

	void TearDown() override
	{
		Script::SetLastCreated(entt::null);
		LandScriptTownCommandsTest::TearDown();
	}

	entt::entity town {entt::null};
	entt::entity earlier {entt::null};
};
} // namespace

// CREATE_NEW_TOWN_SPELL(town, miracle) gives the numbered town the miracle named (without case) and the base level of
// the first seed that casts it, each once, and the town's player has each from one more holder; it makes nothing and
// leaves the last created object, as the original (docs/wiki/magic/worship.md#script,
// docs/wiki/magic/worship.md#a-towns-magic-and-the-creatures-theft,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptTownSpellCommandsTest, CreateNewTownSpellGivesTheTownTheMiracleAndItsBaseLevel)
{
	FeatureScriptCommands::CreateNewTownSpell(5, "fire_pu1");
	EXPECT_EQ(HeldBy(town), (std::vector<MagicType> {MagicType::Fireball, MagicType::FireballPowerUpOne}));
	EXPECT_EQ(Holders(MagicType::Fireball), 1);
	EXPECT_EQ(Holders(MagicType::FireballPowerUpOne), 1);
	EXPECT_EQ(Holders(MagicType::FireballPowerUpTwo), 0);
	EXPECT_EQ(Script::LastCreated(), earlier);

	// The other power-up: the base is held already, so only the power-up has one more holder
	FeatureScriptCommands::CreateNewTownSpell(5, "FIRE_PU2");
	EXPECT_EQ(HeldBy(town),
	          (std::vector<MagicType> {MagicType::Fireball, MagicType::FireballPowerUpOne, MagicType::FireballPowerUpTwo}));
	EXPECT_EQ(Holders(MagicType::Fireball), 1);
	EXPECT_EQ(Holders(MagicType::FireballPowerUpTwo), 1);
}

// A base level named is the miracle and its own base: held once. Named again, a miracle the town holds changes
// nothing, as the original (docs/wiki/magic/worship.md#script,
// docs/wiki/magic/worship.md#a-towns-magic-and-the-creatures-theft)
TEST_F(LandScriptTownSpellCommandsTest, CreateNewTownSpellOfABaseLevelGivesItOnce)
{
	FeatureScriptCommands::CreateNewTownSpell(5, "HEAL");
	EXPECT_EQ(HeldBy(town), (std::vector<MagicType> {MagicType::Heal}));
	EXPECT_EQ(Holders(MagicType::Heal), 1);
	FeatureScriptCommands::CreateNewTownSpell(5, "HEAL");
	EXPECT_EQ(HeldBy(town), (std::vector<MagicType> {MagicType::Heal}));
	EXPECT_EQ(Holders(MagicType::Heal), 1);
}

// No town of the number, the name of magic type 0 or a name no miracle has: nothing is held and nobody is given
// anything, as the original (docs/wiki/magic/worship.md#script)
TEST_F(LandScriptTownSpellCommandsTest, CreateNewTownSpellNeedsTheTownAndAMiracle)
{
	FeatureScriptCommands::CreateNewTownSpell(9, "FIRE");
	FeatureScriptCommands::CreateNewTownSpell(5, "NONE");
	FeatureScriptCommands::CreateNewTownSpell(5, "NOT_A_MIRACLE");
	EXPECT_TRUE(HeldBy(town).empty());
	EXPECT_TRUE(NoHolders());
	EXPECT_EQ(Script::LastCreated(), earlier);
}

// CREATE_TOWN_SPELL(town, seed) gives the numbered town the base miracle of the seed named (without case) and its
// player one more holder, the seed's power-ups not; CREATE_TOWN_CENTRE_SPELL_ICON is the same command. A number no
// town has gives nothing; neither makes anything or changes the last created object, as the original
// (docs/wiki/magic/worship.md#script, docs/wiki/scripts/map-script-reader.md#towns-and-players,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptTownSpellCommandsTest, CreateTownSpellAndItsTownCentreFormGiveTheSeedsMiracle)
{
	FeatureScriptCommands::CreateTownSpell(5, "heal");
	EXPECT_EQ(HeldBy(town), (std::vector<MagicType> {MagicType::Heal}));
	EXPECT_EQ(Holders(MagicType::Heal), 1);
	EXPECT_EQ(Holders(MagicType::HealPowerUpOne), 0);

	FeatureScriptCommands::CreateTownCentreSpellIcon(5, "FIRE");
	EXPECT_EQ(HeldBy(town), (std::vector<MagicType> {MagicType::Fireball, MagicType::Heal}));
	EXPECT_EQ(Holders(MagicType::Fireball), 1);

	FeatureScriptCommands::CreateTownSpell(9, "FIRE");
	FeatureScriptCommands::CreateTownCentreSpellIcon(9, "HEAL");
	EXPECT_EQ(Holders(MagicType::Fireball), 1);
	EXPECT_EQ(Holders(MagicType::Heal), 1);
	EXPECT_EQ(Script::LastCreated(), earlier);
}

// CREATE_PLANNED_SPELL_ICON(town, place, seed, ...) only gives the numbered town the seed's base miracle, as the
// original (docs/wiki/magic/worship.md#script)
TEST_F(LandScriptTownSpellCommandsTest, CreatePlannedSpellIconGivesTheTownOnlyTheSeedsMiracle)
{
	FeatureScriptCommands::CreatePlannedSpellIcon(5, glm::vec3(110.0f, 0.0f, 100.0f), "FIRE", 0, 0, 0);
	EXPECT_EQ(HeldBy(town), (std::vector<MagicType> {MagicType::Fireball}));
	EXPECT_EQ(Holders(MagicType::Fireball), 1);
	FeatureScriptCommands::CreatePlannedSpellIcon(9, glm::vec3(110.0f, 0.0f, 100.0f), "HEAL", 0, 0, 0);
	EXPECT_EQ(Holders(MagicType::Heal), 0);
	EXPECT_EQ(Script::LastCreated(), earlier);
}

// CREATE_SPELL_ICON does nothing: no town holds anything more and the last created object stays, as the original
// (docs/wiki/magic/worship.md#script, docs/wiki/scripts/map-script-reader.md#towns-and-players)
TEST_F(LandScriptTownSpellCommandsTest, CreateSpellIconDoesNothing)
{
	FeatureScriptCommands::CreateSpellIcon(glm::vec3(100.0f, 0.0f, 100.0f), "FIRE", 5, 0, 0);
	EXPECT_TRUE(HeldBy(town).empty());
	EXPECT_TRUE(NoHolders());
	EXPECT_EQ(Script::LastCreated(), earlier);
}

/// The same commands over the game's own info.dat: its fire seed casts the fireball and its two power-ups, its heal
/// seed the heal and its power-up, its wood seed the wood
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST_F(LandScriptTownSpellCommandsTest, realInfoDatTownSpells)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	auto info = ReadRealInfoDat(*game);
	ASSERT_NE(info, nullptr);
	Locator::infoConstants::reset(info.release());
	// the second fire power-up and the fire seed's base
	FeatureScriptCommands::CreateNewTownSpell(5, "FIRE_PU2");
	EXPECT_EQ(HeldBy(town), (std::vector<MagicType> {MagicType::Fireball, MagicType::FireballPowerUpTwo}));
	// the heal's power-up and its base
	FeatureScriptCommands::CreateNewTownSpell(5, "HEAL_PU1");
	EXPECT_EQ(HeldBy(town), (std::vector<MagicType> {MagicType::Fireball, MagicType::FireballPowerUpTwo, MagicType::Heal,
	                                                 MagicType::HealPowerUpOne}));
	// the wood seed's miracle
	FeatureScriptCommands::CreateTownSpell(5, "WOOD");
	EXPECT_EQ(Holders(MagicType::Wood), 1);
	EXPECT_EQ(Holders(MagicType::Fireball), 1);
	EXPECT_EQ(Holders(MagicType::FireballPowerUpOne), 0);
}
