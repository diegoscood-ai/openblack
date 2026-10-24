/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scaffold's own rules, against the original's (docs/wiki/world/building-sites.md): the building and the tribe a
// scaffold offers and the wonder's power, the town it is for, what its end of physics does when no building is on
// offer, the plans its new site removes, the scaffold a finished building takes with it, what a destroying scaffold
// removes, the join, the piece broken off into the yard, a scaffold given to a storage pit, a scaffold snapped in the
// yard, and the land file's CREATE_SCAFFOLD. No game data: a hand-made info.dat, no island and no meshes (every mesh is
// a box 4 across).

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AxisAlignedBoundingBox.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Audio/AudioManagerNoOp.h"
#include "Audio/Game/BankTables.h"
#include "Common/EventManager.h"
#include "Creature/CreatureDeeds.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Events/CreatureMimicEvents.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectDelivery.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Scaffolds.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/ScaffoldPlans.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Resources/Resources.h"
#include "support/MapFakes.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace scaffolds = openblack::ecs::scaffolds;
namespace scaffold_plans = openblack::ecs::scaffold_plans;
namespace sites = openblack::ecs::building_sites;
namespace plans = openblack::ecs::plans;
namespace map_cells = openblack::ecs::map_cells;

namespace
{
constexpr auto k_House = static_cast<AbodeInfo>(0);
constexpr auto k_CelticWonder = static_cast<AbodeInfo>(5);
constexpr auto k_NorseWonder = static_cast<AbodeInfo>(6);
constexpr auto k_NorseHouse = static_cast<AbodeInfo>(7);
constexpr uint32_t k_TownId = 1;
constexpr uint32_t k_OtherTownId = 2;
/// The scaffold info's wood value in info.dat
constexpr uint32_t k_WoodValue = 2500;
/// Far from every town of the tests that have no position (they stand at the origin)
const glm::vec3 k_Away {3000.0f, 0.0f, 3000.0f};

/// Any mesh is a box 4 across and 1 high: a 2D radius of 2 at scale 1
std::optional<AxisAlignedBoundingBox> FourAcross(entt::id_type /*meshId*/)
{
	return AxisAlignedBoundingBox {{-2.0f, 0.0f, -2.0f}, {2.0f, 1.0f, 2.0f}};
}

/// The spot visuals started, with where
class RecordedSpots final: public test::InertParticleSystem
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

/// The sounds played, with each counter at the value the test gives
class RecordedSounds final: public audio::AudioManagerNoOp
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

class ScaffoldsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		test::FakeMeshBoxProvider boxes;
		boxes.meshBox = &FourAcross;
		Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>(boxes);
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
		auto& norseHouse = info->abode.at(static_cast<size_t>(k_NorseHouse));
		norseHouse = house;
		norseHouse.tribeType = Tribe::NORSE;
		auto& celticWonder = info->abode.at(static_cast<size_t>(k_CelticWonder));
		celticWonder.abodeType = AbodeType::Wonder;
		celticWonder.abodeNumber = AbodeNumber::Wonder;
		celticWonder.tribeType = Tribe::CELTIC;
		celticWonder.desireToBeBuilt = 0.5f;
		celticWonder.scaffoldsRequired = 7;
		auto& norseWonder = info->abode.at(static_cast<size_t>(k_NorseWonder));
		norseWonder = celticWonder;
		norseWonder.tribeType = Tribe::NORSE;
		info->pot.at(static_cast<size_t>(PotInfo::WoodPile_1)).resourceType = ResourceType::Wood;
		info->town.thresholdToStartRepairing = 0.9f;
		info->scaffold.woodValue = k_WoodValue;
		info->scaffold.maxNumberForCombining = 8;
		info->scaffold.gameTurnsAfterPlacingCanStillPickUp = 150;
		// the rock row's impressiveness, for the wonder's power
		info->mobileStatic.at(0).impressiveValue = 2.5f;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::resources::emplace<resources::Resources>();
		Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>();
		_spots = &static_cast<RecordedSpots&>(Locator::particleSystem::emplace<RecordedSpots>());
		Locator::events::emplace<EventManager>();
		_town = MakeTown(k_TownId, PlayerNames::PLAYER_ONE, Tribe::CELTIC);
		game_clock::SetTurn(0);
	}

	void TearDown() override
	{
		game_clock::SetTurn(0);
		lhscriptx::Script::SetLastCreated(entt::null);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	static const GAbodeInfo& Info(AbodeInfo info)
	{
		return Locator::infoConstants::value().abode.at(static_cast<size_t>(info));
	}
	static Scaffold& Of(entt::entity scaffold) { return Reg().Get<Scaffold>(scaffold); }

	/// A town of the player and tribe, found by its number, with no position (at the origin)
	static entt::entity MakeTown(uint32_t id, PlayerNames owner, Tribe tribe)
	{
		auto& registry = Reg();
		const auto town = registry.Create();
		auto& t = registry.Assign<Town>(town);
		t.id = id;
		t.owner = owner;
		t.ownerListStamp = id;
		registry.Assign<Tribe>(town, tribe);
		registry.Context().towns[id] = town;
		return town;
	}

	/// A scaffold worth `value` at the point, of the town (or none), owned by no workshop
	static entt::entity MakeScaffold(const glm::vec3& at, entt::entity town, uint32_t value)
	{
		const auto scaffold = scaffolds::Create(map_coords::FromWorld(nullptr, at), town, entt::null, 0.0f, 1.0f);
		scaffolds::SetValue(scaffold, value);
		return scaffold;
	}

	/// A house of the town under construction, with no mesh of its own
	static entt::entity MakeHouse(uint32_t townId)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		auto& abode = registry.Assign<Abode>(e, AbodeNumber::A, townId, 0u, 0u);
		abode.info = k_House;
		abode.buildFlags = Abode::k_UnderConstruction;
		abode.percentBuilt = 0.0f;
		abode.addedToTownStats = false;
		return e;
	}

	/// A thing at the point, put in the map cells
	static entt::entity Place(entt::entity e, const glm::vec3& at)
	{
		ecs::object_index::Assign(e);
		Reg().Assign<Transform>(e, at, glm::mat3(1.0f), glm::vec3(1.0f));
		map_cells::InsertMapObject(e);
		return e;
	}

	/// A rock (a static object that cannot be thrown about) at the point
	static entt::entity RockAt(const glm::vec3& at)
	{
		const auto e = Reg().Create();
		Reg().Assign<MobileStatic>(e);
		return Place(e, at);
	}

	const test::RestoreService<Locator::resources> _keepResources;
	const test::RestoreService<Locator::dynamicsSystem> _keepDynamics;
	const test::RestoreService<Locator::particleSystem> _keepParticles;
	const test::RestoreService<Locator::events> _keepEvents;
	RecordedSpots* _spots {nullptr};
	entt::entity _town {entt::null};
};
} // namespace

// ---- the building on offer and the wonder's power --------------------------------------------------------------------

TEST_F(ScaffoldsTest, TheWonderTakesTheScaffoldsTribeAndOtherBuildingsTheTownsTribe)
{
	// the receiving town is Celtic. Seven scaffolds offer the wonder (desire 1 from seven on, no penalty at its seven):
	// the tribe given (the scaffold's town's), Norse, not the town's; with none given, the town's
	const auto pos = map_coords::FromWorld(nullptr, k_Away);
	const float angle = 0.75f;
	ScaffoldPlan out;
	EXPECT_FLOAT_EQ(scaffold_plans::GetNewPlannedBuilding(_town, out, pos, angle, 1.0f, 7, entt::null, Tribe::NORSE,
	                                                      static_cast<int32_t>(AbodeNumber::Wonder), true),
	                1.0f);
	EXPECT_EQ(out.info, &Info(k_NorseWonder));
	EXPECT_FLOAT_EQ(out.yAngle, 0.75f);
	out = {};
	scaffold_plans::GetNewPlannedBuilding(_town, out, pos, angle, 1.0f, 7, entt::null, Tribe::NONE,
	                                      static_cast<int32_t>(AbodeNumber::Wonder), true);
	EXPECT_EQ(out.info, &Info(k_CelticWonder));
	// a house is the town's tribe's whatever tribe is given, at the scale given: the town's desire for it, 0.5
	out = {};
	EXPECT_FLOAT_EQ(scaffold_plans::GetNewPlannedBuilding(_town, out, pos, angle, 1.0f, 2, entt::null, Tribe::NORSE,
	                                                      static_cast<int32_t>(AbodeNumber::A), true),
	                0.5f);
	EXPECT_EQ(out.info, &Info(k_House));
	EXPECT_FLOAT_EQ(out.scale, 1.0f);
}

TEST_F(ScaffoldsTest, AScaffoldFromAnotherTribesTownOffersAWonderOfItsOwnTribe)
{
	// a scaffold of seven made by a Norse town, held over a Celtic town (with the script's destroy flag, so that the
	// fit is not asked): the building chosen is the Norse wonder
	const auto norse = MakeTown(k_OtherTownId, PlayerNames::PLAYER_TWO, Tribe::NORSE);
	const auto scaffold = MakeScaffold(k_Away, norse, 7);
	Of(scaffold).flags |= Scaffold::k_Destroy;
	EXPECT_TRUE(scaffolds::ChoosePlan(scaffold, _town));
	ASSERT_TRUE(Of(scaffold).candidate.has_value());
	EXPECT_EQ(Of(scaffold).candidate->info, &Info(k_NorseWonder));
}

TEST_F(ScaffoldsTest, TheWondersPowerIsTheTownsWonderDesireAndAtLeastAQuarter)
{
	// nothing around the spot: the town's raw wonder desire (raw + boost + boost A), and 0.25 below it
	const auto pos = map_coords::FromWorld(nullptr, k_Away);
	auto& desire = Reg().Get<Town>(_town).desire;
	const auto wonder = static_cast<size_t>(TownDesireInfo::ToBuildWonder);
	desire.raw.at(wonder) = 0.1f;
	EXPECT_FLOAT_EQ(scaffold_plans::GetWonderPower(_town, pos), 0.25f);
	desire.raw.at(wonder) = 1.5f;
	desire.boost.at(wonder) = 0.25f;
	desire.boostA.at(wonder) = 0.25f;
	EXPECT_FLOAT_EQ(scaffold_plans::GetWonderPower(_town, pos), 2.0f);
}

TEST_F(ScaffoldsTest, EachThingInTheSpotsOwnCellAddsItsWorthOncePerCellOfTheSearch)
{
	// a rock artefact worth 1.5, of impressiveness 2.5, at the spot: its artefact worth plus a tenth of its
	// impressiveness (1.75), added once for every cell of the search, which reads the spot's own cell each time. With
	// no wonder desire the search is 15 m: (15 x 0.2)^2 = 9 cells, 9 x 1.75
	const auto rock = RockAt(k_Away);
	Reg().Assign<TownArtefact>(rock).value = 1.5f;
	const auto pos = map_coords::FromWorld(nullptr, k_Away);
	EXPECT_FLOAT_EQ(scaffold_plans::GetWonderPower(_town, pos), 15.75f);
	// a desire of 2 searches 15 x 2 = 30 m: 36 cells, 2 + 36 x 1.75
	auto& desire = Reg().Get<Town>(_town).desire;
	desire.raw.at(static_cast<size_t>(TownDesireInfo::ToBuildWonder)) = 2.0f;
	EXPECT_FLOAT_EQ(scaffold_plans::GetWonderPower(_town, pos), 65.0f);
}

TEST_F(ScaffoldsTest, TheWonderIsSizedByItsPowerUpToFive)
{
	// the scale is p x (1 - 0.1 p), with p the power and at most 5: 0.25 gives 0.24375; any power from 5 gives 2.5
	const auto pos = map_coords::FromWorld(nullptr, k_Away);
	EXPECT_FLOAT_EQ(scaffold_plans::WonderScale(_town, pos), 0.24375f);
	Reg().Get<Town>(_town).desire.raw.at(static_cast<size_t>(TownDesireInfo::ToBuildWonder)) = 9.0f;
	EXPECT_FLOAT_EQ(scaffold_plans::WonderScale(_town, pos), 2.5f);
	// the wonder offered takes it as its scale
	ScaffoldPlan out;
	const float angle = 0.0f;
	scaffold_plans::GetNewPlannedBuilding(_town, out, pos, angle, 1.0f, 7, entt::null, Tribe::NONE,
	                                      static_cast<int32_t>(AbodeNumber::Wonder), true);
	EXPECT_FLOAT_EQ(out.scale, 2.5f);
}

// ---- the town it is for -----------------------------------------------------------------------------------------------

TEST_F(ScaffoldsTest, AScaffoldTakesTheTownOfTheNearestBuildingWhoeverOwnsIt)
{
	// the scaffold is a mobile object, type 20 in info.dat (an empty row's type would be 0, an abode's), so it is not
	// the building it looks for
	auto rows = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	rows->mobileObject.at(static_cast<size_t>(MobileObjectInfo::OldScaffold)).type = ObjectType::MobileObject;
	Locator::infoConstants::reset(rows.release());
	// with no building near, the nearest town of any player within 50 m: the second player's at 20 m before the first
	// player's at 30 m; then a plan of the first player's town at 10 m makes it the nearer
	const glm::vec3 at {1000.0f, 0.0f, 1000.0f};
	const auto scaffold = MakeScaffold(at, entt::null, 1);
	const auto other = MakeTown(k_OtherTownId, PlayerNames::PLAYER_TWO, Tribe::CELTIC);
	Reg().Assign<Transform>(_town, at + glm::vec3(30.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<Transform>(other, at + glm::vec3(20.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_EQ(scaffolds::GetTownForBuilding(scaffold), other);
	plans::AddPlanned(_town, PlannedAbode {k_House, at + glm::vec3(10.0f, 0.0f, 0.0f), 0.0f, 1.0f, false});
	EXPECT_EQ(scaffolds::GetTownForBuilding(scaffold), _town);
	// a building of the second player's town within 50 m comes first, nearer town or plan or not
	Place(MakeHouse(k_OtherTownId), at + glm::vec3(40.0f, 0.0f, 0.0f));
	EXPECT_EQ(scaffolds::GetTownForBuilding(scaffold), other);
	// the nearest building's town, even none
	Place(MakeHouse(99), at + glm::vec3(5.0f, 0.0f, 0.0f));
	EXPECT_EQ(scaffolds::GetTownForBuilding(scaffold), entt::entity {entt::null});
}

TEST_F(ScaffoldsTest, NoTownWithin50MetresTakesTheScaffold)
{
	const glm::vec3 at {1000.0f, 0.0f, 1000.0f};
	const auto scaffold = MakeScaffold(at, entt::null, 1);
	Reg().Assign<Transform>(_town, at + glm::vec3(51.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_EQ(scaffolds::GetTownForBuilding(scaffold), entt::entity {entt::null});
}

// ---- putting it down --------------------------------------------------------------------------------------------------

TEST_F(ScaffoldsTest, AScaffoldPutDownWithNoBuildingOnOfferLiesThereWithAPuff)
{
	// a scaffold of no town far from every town, put down gently from the hand: no building is on offer, so it makes
	// no site and the "no building here" puff (spot visual 4) comes where it lies. It is placed now and held by no one
	game_clock::SetTurn(321);
	const auto scaffold = MakeScaffold(k_Away, entt::null, 2);
	Of(scaffold).holder = PlayerNames::PLAYER_ONE;
	Of(scaffold).flags |= Scaffold::k_DroppedGently;
	EXPECT_EQ(scaffolds::OnEndPhysics(scaffold, true), scaffold);
	ASSERT_EQ(_spots->started.size(), 1u);
	EXPECT_EQ(_spots->started.front().first, static_cast<SpotVisualType>(4));
	EXPECT_NEAR(_spots->started.front().second.x, k_Away.x, 0.01f);
	EXPECT_NEAR(_spots->started.front().second.z, k_Away.z, 0.01f);
	const auto& lying = Of(scaffold);
	EXPECT_EQ(lying.site, entt::entity {entt::null});
	EXPECT_EQ(lying.holder, std::nullopt);
	EXPECT_EQ(lying.placedTurn, 321u);
	EXPECT_EQ(lying.flags & Scaffold::k_DroppedGently, 0);
	// thrown from the hand, the same puff; coming to rest after being knocked about, none
	EXPECT_EQ(scaffolds::OnEndPhysics(scaffold, true), scaffold);
	EXPECT_EQ(_spots->started.size(), 2u);
	EXPECT_EQ(scaffolds::OnEndPhysics(scaffold, false), scaffold);
	EXPECT_EQ(_spots->started.size(), 2u);
}

TEST_F(ScaffoldsTest, AScaffoldsEndOfPhysicsOffersItsPlayersCreatureTheDeedOfBuildingAHouse)
{
	// a scaffold with a player ends its physics: that player's creature may copy it as building a house (deed 6),
	// thrown or not; one with no player tells nothing
	std::vector<ecs::events::PlayerDeedForMimic> deeds;
	Locator::events::value().AddHandler<ecs::events::PlayerDeedForMimic>(
	    [&deeds](const ecs::events::PlayerDeedForMimic& event) { deeds.push_back(event); });
	const auto scaffold = MakeScaffold(k_Away, entt::null, 1);
	Of(scaffold).holder = PlayerNames::PLAYER_TWO;
	ecs::PhysicsEntry po;
	po.entity = scaffold;
	scaffolds::OnEndPhysics(scaffold, po);
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.front().player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(deeds.front().deed, creature_watching::Deed::BuildHouse);
	EXPECT_EQ(static_cast<int>(deeds.front().deed), 6);
	EXPECT_EQ(deeds.front().object, scaffold);
	EXPECT_FALSE(deeds.front().magic.has_value());
	// from the hand: the same deed
	Of(scaffold).holder = PlayerNames::PLAYER_TWO;
	po.flags = ecs::PhysicsEntry::k_FromHand;
	scaffolds::OnEndPhysics(scaffold, po);
	EXPECT_EQ(deeds.size(), 2u);
	// its end cleared the player: nothing more
	scaffolds::OnEndPhysics(scaffold, po);
	EXPECT_EQ(deeds.size(), 2u);
}

TEST_F(ScaffoldsTest, ANewSiteRemovesThePlansOfEveryTownItsGroundReaches)
{
	// the scaffold's site is a house 2 m across each way from its middle. A plan goes when its distance less the
	// site's radius is below 100 m and does not exceed the plan's own radius (2 m x its scale), in every player's towns
	const glm::vec3 at {100.0f, 0.0f, 100.0f};
	const auto house = MakeHouse(k_TownId);
	const auto site = sites::AddBuildingSite(_town, house);
	ASSERT_TRUE(site != entt::null);
	Reg().Assign<Transform>(house, at, glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<Mesh>(house, entt::id_type {1}, static_cast<int8_t>(0), static_cast<int8_t>(0));
	const auto scaffold = MakeScaffold(at, _town, 2);
	Of(scaffold).site = site;
	const auto other = MakeTown(k_OtherTownId, PlayerNames::PLAYER_TWO, Tribe::CELTIC);
	const auto plan = [](float x, float scale) {
		return PlannedAbode {k_House, glm::vec3(x, 0.0f, 100.0f), 0.0f, scale, false};
	};
	plans::AddPlanned(_town, plan(103.0f, 1.0f));   // 3 - 2 = 1 <= 2: goes
	plans::AddPlanned(_town, plan(105.5f, 1.0f));   // 3.5 - 2 = 1.5 > 0: stays
	plans::AddPlanned(_town, plan(105.5f, 2.0f));   // 1.5 <= 4: goes
	plans::AddPlanned(_town, plan(180.0f, 50.0f));  // 78 < 100 and 78 <= 100: goes
	plans::AddPlanned(_town, plan(300.0f, 150.0f)); // 198 is not below 100: stays, however big
	plans::AddPlanned(other, plan(103.0f, 1.0f));   // another player's town's: goes
	scaffolds::DeletePlannedBuildingsUnderMe(scaffold);
	const auto& left = Reg().Get<const Town>(_town).plannedAbodes;
	ASSERT_EQ(left.size(), 2u);
	EXPECT_FLOAT_EQ(left.at(0).position.x, 105.5f);
	EXPECT_FLOAT_EQ(left.at(0).scale, 1.0f);
	EXPECT_FLOAT_EQ(left.at(1).position.x, 300.0f);
	EXPECT_TRUE(Reg().Get<const Town>(other).plannedAbodes.empty());
}

TEST_F(ScaffoldsTest, AFinishedBuildingTakesItsScaffoldWithIt)
{
	// a scaffold on a house's site: when the house is finished its site goes, and every scaffold of the site with it
	const auto house = MakeHouse(k_TownId);
	const auto site = sites::AddBuildingSite(_town, house);
	ASSERT_TRUE(site != entt::null);
	const auto scaffold = MakeScaffold(k_Away, _town, 2);
	sites::AddScaffold(site, scaffold);
	Of(scaffold).site = site;
	sites::BuildBy(site, 0.5f);
	EXPECT_TRUE(ecs::IsAvailable(scaffold));
	sites::BuildBy(site, 0.5f);
	EXPECT_FALSE(sites::IsAvailable(site));
	EXPECT_FALSE(ecs::IsAvailable(scaffold));
	EXPECT_TRUE(ecs::IsAvailable(house));
}

TEST_F(ScaffoldsTest, AScaffoldThatDestroysWhatIsUnderItRemovesWhatCannotMoveWhenLetGo)
{
	// a scaffold of one (radius 2 x 0.7 = 1.4) a script marked to destroy what is under it, leaving the hand (out of
	// the map cells, as a held one is): a rock 1 m away goes, one 3 m away stays, and a loose scaffold 1 m away, which
	// can move, is pushed rather than removed
	const glm::vec3 at {55.0f, 0.0f, 55.0f};
	const auto scaffold = MakeScaffold(at, entt::null, 1);
	map_cells::RemoveMapObject(scaffold);
	const auto nearRock = RockAt(at + glm::vec3(1.0f, 0.0f, 0.0f));
	const auto farRock = RockAt(at + glm::vec3(3.0f, 0.0f, 0.0f));
	const auto loose = MakeScaffold(at + glm::vec3(0.0f, 0.0f, 1.0f), entt::null, 1);
	// without the mark nothing goes
	scaffolds::OnBeforeInitialisePhysicsFromHand(scaffold);
	EXPECT_TRUE(ecs::IsAvailable(nearRock));
	Of(scaffold).flags |= Scaffold::k_Destroy;
	scaffolds::OnBeforeInitialisePhysicsFromHand(scaffold);
	EXPECT_FALSE(ecs::IsAvailable(nearRock));
	EXPECT_TRUE(ecs::IsAvailable(farRock));
	EXPECT_TRUE(ecs::IsAvailable(loose));
	EXPECT_TRUE(ecs::IsAvailable(scaffold));
}

// ---- joining and breaking ---------------------------------------------------------------------------------------------

TEST_F(ScaffoldsTest, JoiningLeavesTheGroundScaffoldWorthBothWithTheHeldOnesBuilding)
{
	// the held scaffold (1, offering a house, its phantom turned to 0.5 and at 1.25) joins the one on the ground (2,
	// offering a wonder): the ground one is worth 3 and offers the house at the phantom's angle and scale, and the held
	// one is gone. Far from every town, so the joined one finds no town to build in
	const auto held = MakeScaffold(k_Away, entt::null, 1);
	const auto ground = MakeScaffold(k_Away + glm::vec3(10.0f, 0.0f, 0.0f), entt::null, 2);
	Of(held).plan = ScaffoldPlan {.info = &Info(k_House)};
	Of(held).phantomAngle = 0.5f;
	Of(held).phantomScale = 1.25f;
	Of(held).holder = PlayerNames::PLAYER_ONE;
	Of(ground).plan = ScaffoldPlan {.info = &Info(k_CelticWonder)};
	scaffolds::Combine(held, ground);
	EXPECT_FALSE(ecs::IsAvailable(held));
	EXPECT_EQ(scaffolds::GetValue(ground), 3u);
	const auto& joined = Of(ground);
	ASSERT_TRUE(joined.plan.has_value());
	EXPECT_EQ(joined.plan->info, &Info(k_House));
	EXPECT_FLOAT_EQ(joined.plan->yAngle, 0.5f);
	EXPECT_FLOAT_EQ(joined.plan->scale, 1.25f);
	EXPECT_FLOAT_EQ(joined.phantomAngle, 0.5f);
	EXPECT_FLOAT_EQ(joined.phantomScale, 1.25f);
	EXPECT_EQ(joined.site, entt::entity {entt::null});
}

TEST_F(ScaffoldsTest, JoiningShowsThePuffAndPlaysTheNextOfItsFourSoundsAtTheHand)
{
	// let go onto another scaffold: the join (1), the combine puff (spot visual 27) at the scaffold on the ground, and
	// with a hand the in-game sample 201 plus the join's counter, in 3D at the hand's point, not following it
	const test::RestoreService<Locator::audio> keepAudio;
	auto& sounds = static_cast<RecordedSounds&>(Locator::audio::emplace<RecordedSounds>());
	sounds.next = 1;
	const auto held = MakeScaffold(k_Away, entt::null, 1);
	const glm::vec3 there = k_Away + glm::vec3(10.0f, 0.0f, 0.0f);
	const auto ground = MakeScaffold(there, entt::null, 2);
	const glm::vec3 hand {3008.0f, 4.0f, 3002.0f};
	const ecs::pot_resource::Dropper byHand {.hasInterface = true, .player = PlayerNames::PLAYER_ONE, .isMyInterface = true};
	EXPECT_EQ(scaffolds::ApplyThisToObject(held, ground, byHand, hand), 1);
	ASSERT_EQ(_spots->started.size(), 1u);
	EXPECT_EQ(_spots->started.front().first, static_cast<SpotVisualType>(27));
	EXPECT_NEAR(_spots->started.front().second.x, there.x, 0.01f);
	EXPECT_NEAR(_spots->started.front().second.z, there.z, 0.01f);
	ASSERT_EQ(sounds.counters.size(), 1u);
	EXPECT_EQ(sounds.counters.front(), audio::Counter::ScaffoldCombine);
	ASSERT_EQ(sounds.played.size(), 1u);
	const auto& sound = sounds.played.front();
	EXPECT_EQ(sound.sample.bank, audio::Bank(audio::SfxBank::InGame));
	EXPECT_EQ(sound.sample.number, 202);
	EXPECT_TRUE(sound.is3D);
	EXPECT_FALSE(sound.track);
	EXPECT_EQ(sound.position, hand);
	// its owner is the held scaffold, which the join has already deleted, not the one on the ground
	EXPECT_EQ(sound.owner, audio::Owner::Thing(held));
	EXPECT_FALSE(ecs::IsAvailable(held));
	// without a hand: the join and the puff, no sound
	const auto another = MakeScaffold(k_Away + glm::vec3(0.0f, 0.0f, 20.0f), entt::null, 1);
	EXPECT_EQ(scaffolds::ApplyThisToObject(another, ground, {}, hand), 1);
	EXPECT_EQ(_spots->started.size(), 2u);
	EXPECT_EQ(sounds.played.size(), 1u);
	EXPECT_EQ(scaffolds::GetValue(ground), 4u);
}

TEST_F(ScaffoldsTest, ThePieceBrokenOffTakesTheWorkshopsFirstFreePlaceWhenTheYardHasRoom)
{
	// a scaffold of 3 in its workshop's first place breaks one off: the piece is the workshop's, in its next free
	// place, which is marked as moved away (its place kept); the workshop owns one more
	const auto workshop = Reg().Create();
	auto& yard = Reg().Assign<Workshop>(workshop);
	const auto scaffold = scaffolds::Create(map_coords::FromWorld(nullptr, k_Away), _town, workshop, 0.0f, 1.0f);
	scaffolds::SetValue(scaffold, 3);
	scaffolds::SetSlot(scaffold, 0);
	yard.slots.at(0) = Workshop::k_SlotOccupied;
	yard.scaffolds.push_back(scaffold);
	yard.owned = 1;
	scaffolds::Split(scaffold);
	const auto& after = Reg().Get<const Workshop>(workshop);
	ASSERT_EQ(after.scaffolds.size(), 2u);
	const auto piece = after.scaffolds.front();
	EXPECT_NE(piece, scaffold);
	EXPECT_EQ(scaffolds::GetValue(piece), 1u);
	EXPECT_EQ(scaffolds::GetValue(scaffold), 2u);
	EXPECT_EQ(scaffolds::GetOwner(piece), workshop);
	EXPECT_EQ(scaffolds::GetSlot(piece), 1);
	EXPECT_EQ(after.slots.at(1), Workshop::k_SlotTakenAway);
	EXPECT_EQ(after.slots.at(2), Workshop::k_SlotFree);
	EXPECT_EQ(after.owned, 2);
}

TEST_F(ScaffoldsTest, ThePieceBrokenOffAFullYardBelongsToNoWorkshop)
{
	// a workshop that owns three has no room: the piece is nobody's
	const auto workshop = Reg().Create();
	auto& yard = Reg().Assign<Workshop>(workshop);
	const auto scaffold = scaffolds::Create(map_coords::FromWorld(nullptr, k_Away), _town, workshop, 0.0f, 1.0f);
	scaffolds::SetValue(scaffold, 2);
	yard.slots = {Workshop::k_SlotOccupied, Workshop::k_SlotTakenAway, Workshop::k_SlotTakenAway};
	yard.scaffolds.push_back(scaffold);
	yard.owned = 3;
	scaffolds::Split(scaffold);
	entt::entity piece = entt::null;
	Reg().Each<const Scaffold>([&piece, scaffold](entt::entity e, const Scaffold&) {
		if (e != scaffold)
		{
			piece = e;
		}
	});
	ASSERT_TRUE(piece != entt::null);
	EXPECT_EQ(scaffolds::GetOwner(piece), entt::entity {entt::null});
	EXPECT_EQ(scaffolds::GetSlot(piece), 0);
	EXPECT_EQ(Reg().Get<const Workshop>(workshop).owned, 3);
}

TEST_F(ScaffoldsTest, AScaffoldLetGoOntoAStoragePitIsTakenInAsItsWood)
{
	// a scaffold of 3 over a storage pit: it may be applied, it gives the pit its wood, 3 x 2500, and goes (3)
	const test::RestoreService<Locator::audio> keepAudio;
	Locator::audio::emplace<RecordedSounds>();
	const auto pit = Reg().Create();
	Reg().Assign<StoragePit>(pit);
	const auto scaffold = MakeScaffold(k_Away, entt::null, 3);
	EXPECT_TRUE(scaffolds::ValidToApplyThisToObject(scaffold, pit));
	const auto [type, amount] = ecs::object_delivery::ResourceOf(scaffold);
	EXPECT_EQ(type, ResourceType::Wood);
	EXPECT_EQ(amount, 3 * k_WoodValue);
	EXPECT_EQ(scaffolds::ApplyThisToObject(scaffold, pit, {}, k_Away), 3);
	EXPECT_FALSE(ecs::IsAvailable(scaffold));
}

// ---- in the world -----------------------------------------------------------------------------------------------------

TEST_F(ScaffoldsTest, AScaffoldSnappedInTheYardCannotBecomeAPhysicsObject)
{
	// owned by a workshop, in the second place: not while the place holds it (2); once moved away (1), or with its
	// workshop gone, it can
	const auto workshop = Reg().Create();
	Reg().Assign<Workshop>(workshop).slots.at(1) = Workshop::k_SlotOccupied;
	const auto scaffold = scaffolds::Create(map_coords::FromWorld(nullptr, k_Away), _town, workshop, 0.0f, 1.0f);
	scaffolds::SetSlot(scaffold, 1);
	EXPECT_FALSE(scaffolds::CanBecomePhysicsObject(scaffold));
	Reg().Get<Workshop>(workshop).slots.at(1) = Workshop::k_SlotTakenAway;
	EXPECT_TRUE(scaffolds::CanBecomePhysicsObject(scaffold));
	Reg().Get<Workshop>(workshop).slots.at(1) = Workshop::k_SlotOccupied;
	Reg().Destroy(workshop);
	EXPECT_TRUE(scaffolds::CanBecomePhysicsObject(scaffold));
}

// ---- land files -------------------------------------------------------------------------------------------------------

TEST_F(ScaffoldsTest, ALandFilesScaffoldIsMadeOfTheNumberedTownAtItsPlaceAngleAndSize)
{
	// CREATE_SCAFFOLD(town 1, the place, row 0, angle 1571, size 1300): a scaffold of town 1 there with no workshop, its
	// Y angle 1571 x 0.001 and its scale 1300 x 0.001 (worth 4), the last thing made
	const glm::vec3 at {2000.0f, 0.0f, 2000.0f};
	lhscriptx::FeatureScriptCommands::CreateScaffold(static_cast<int32_t>(k_TownId), at, 0, 1571, 1300);
	const auto scaffold = lhscriptx::Script::LastCreated();
	ASSERT_TRUE(scaffolds::IsScaffold(scaffold));
	EXPECT_EQ(scaffolds::GetTown(scaffold), _town);
	EXPECT_EQ(scaffolds::GetOwner(scaffold), entt::entity {entt::null});
	const auto& transform = Reg().Get<const Transform>(scaffold);
	EXPECT_NEAR(transform.position.x, at.x, 0.01f);
	EXPECT_NEAR(transform.position.z, at.z, 0.01f);
	EXPECT_EQ(transform.rotation, affine::AngleY(1571.0f * 0.001f));
	EXPECT_EQ(transform.scale, glm::vec3(1300.0f * 0.001f));
	EXPECT_EQ(scaffolds::GetValue(scaffold), 4u);
	// a number no town has: a scaffold of no town
	lhscriptx::FeatureScriptCommands::CreateScaffold(42, at, 0, 0, 1000);
	EXPECT_EQ(scaffolds::GetTown(lhscriptx::Script::LastCreated()), entt::entity {entt::null});
}
