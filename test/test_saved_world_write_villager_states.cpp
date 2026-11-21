/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The values a villager's state writes, made from what openblack holds of a villager made in play, in the original's
// order and sizes (docs/wiki/engine/saved-games.md#openblack, the villagers)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstring>

#include <array>
#include <optional>
#include <vector>

#include <SaveParts.h>
#include <SavedRecords.h>
#include <VillagerRecords.h>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/MagicTreeWatch.h"
#include "ECS/Components/ScriptAnimation.h"
#include "ECS/Components/ScriptStateArguments.h"
#include "ECS/Components/ShepherdWork.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerHiding.h"
#include "ECS/Components/VillagerInspection.h"
#include "ECS/Components/VillagerShieldState.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorld/WriteVillagers.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "Enums.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;
using gamesave::StateValue;

namespace
{
/// An object a state names: a class of its own, so that the context numbers it
struct Named
{
};

ClassWriters NamingWriters()
{
	ClassWriters writers;
	writers.Register(
	    gamesave::FootpathRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) { return registry.AllOf<Named>(entity); },
	    [](WriteContext& /*context*/, entt::entity /*entity*/) -> std::optional<gamesave::SavedRecord> {
		    return std::nullopt;
	    });
	return writers;
}

entt::entity MakeNamed(ecs::Registry& registry)
{
	const auto entity = registry.Create();
	registry.Assign<Named>(entity);
	return entity;
}

/// The value as it reads back, of its size
template <typename T>
T As(const StateValue& value)
{
	EXPECT_EQ(value.size, sizeof(T));
	T held {};
	std::memcpy(&held, value.bytes.data(), sizeof(T));
	return held;
}

/// Each value's size, as the reader reads the state's values
std::vector<uint8_t> SizesOf(const std::vector<StateValue>& values)
{
	std::vector<uint8_t> sizes;
	sizes.reserve(values.size());
	for (const auto& value : values)
	{
		sizes.push_back(value.size);
	}
	return sizes;
}

constexpr uint8_t State(VillagerStates state)
{
	return static_cast<uint8_t>(state);
}

class VillagerStateValues: public ::testing::Test
{
protected:
	VillagerStateValues()
	    : _registry(MakeServices())
	    , _writers(NamingWriters())
	    , _source {.registry = _registry}
	    , _context(_writers, _source)
	{
		_villager = _registry.Create();
		_registry.Assign<components::Villager>(_villager);
	}

	components::Villager& Villager() { return _registry.Get<components::Villager>(_villager); }

	/// The state's values, which the reader's sizes for it must match
	std::vector<StateValue> ValuesOf(VillagerStates state, uint8_t discipleType = 0, const gamesave::LivingFields& living = {})
	{
		auto values = HeldVillagerStateValues(_context, _villager, State(state), living);
		EXPECT_TRUE(values.has_value()) << static_cast<int>(state);
		if (!values.has_value())
		{
			return {};
		}
		EXPECT_EQ(SizesOf(*values), gamesave::StateValueSizes(gamesave::VillagerStateSaveOf(State(state)), discipleType));
		EXPECT_TRUE(_context.Failure().empty()) << _context.Failure();
		return *values;
	}

	/// The game's registry and its map cells, which an object's place reads, for this test only
	static ecs::Registry& MakeServices()
	{
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		return Locator::entitiesRegistry::emplace<ecs::Registry>();
	}

	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	const test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	ecs::Registry& _registry;
	ClassWriters _writers;
	WorldSource _source;
	WriteContext _context;
	entt::entity _villager {entt::null};
};
} // namespace

// A builder writes its building site and its place at the site; one fetching wood or stone for it the same
TEST_F(VillagerStateValues, ABuilderWritesItsSiteAndItsPlaceThere)
{
	const auto site = MakeNamed(_registry);
	Villager().buildingSite = site;
	Villager().buildPosIndex = 37;
	for (const auto state : {VillagerStates::Building, VillagerStates::ArrivesAtBuildingSite,
	                         VillagerStates::ArrivesAtStoragePitForBuildingMaterials})
	{
		const auto values = ValuesOf(state);
		ASSERT_EQ(values.size(), 2u);
		EXPECT_EQ(values[0].number, _context.Reference(site));
		EXPECT_EQ(As<int32_t>(values[1]), 37);
	}
}

// A farmer writes its work point in its field, its height too, then the field
TEST_F(VillagerStateValues, AFarmerWritesItsWorkPointThenItsField)
{
	const auto field = MakeNamed(_registry);
	Villager().targetThing = field;
	Villager().workPos = map_coords::MapCoords {.x = 0x01234567, .z = -5, .altitude = 1.5f};
	const auto values = ValuesOf(VillagerStates::FarmerPlantsCrop);
	ASSERT_EQ(values.size(), 2u);
	const auto point = As<gamesave::MapCoords>(values[0]);
	EXPECT_EQ(point.x, 0x01234567);
	EXPECT_EQ(point.z, -5);
	EXPECT_EQ(point.altitude, 1.5f);
	EXPECT_EQ(values[1].number, _context.Reference(field));
}

// A breeder disciple writes its partner, none while it wanders, then the spot it wanders about, x and z
TEST_F(VillagerStateValues, ABreederWritesItsPartnerThenItsSpot)
{
	Villager().wanderArea = map_coords::MapCoords {.x = 700000, .z = 800000, .altitude = 3.0f};
	const auto values = ValuesOf(VillagerStates::BreederDisciple);
	ASSERT_EQ(values.size(), 2u);
	EXPECT_EQ(values[0].number, 0u);
	EXPECT_EQ((As<std::array<int32_t, 2>>(values[1])), (std::array<int32_t, 2> {700000, 800000}));
}

// A trader writes the resource it trades and its trade town; a trader with nothing to do the same, another disciple
// with nothing to do nothing
TEST_F(VillagerStateValues, ATraderWritesItsResourceAndItsTradeTown)
{
	constexpr uint8_t k_Trader = gamesave::k_TraderDisciple;
	const auto town = MakeNamed(_registry);
	Villager().discipleType = k_Trader;
	Villager().tradeResource = ResourceType::Wood;
	Villager().tradeTown = town;
	for (const auto state : {VillagerStates::ArrivesAtStoragePitForTraderPickUp, VillagerStates::DiscipleNothingToDo})
	{
		const auto values = ValuesOf(state, k_Trader);
		ASSERT_EQ(values.size(), 2u);
		EXPECT_EQ(As<int32_t>(values[0]), 1);
		EXPECT_EQ(values[1].number, _context.Reference(town));
	}
	EXPECT_TRUE(gamesave::StateValueSizes(gamesave::VillagerStateSave::DiscipleNothingToDo, 1).empty());
}

// A villager in the hand writes the disciple it had as it was picked up, as a whole word
TEST_F(VillagerStateValues, AVillagerInTheHandWritesItsDiscipleAtThePickUp)
{
	Villager().discipleOnPickUp = 6;
	const auto values = ValuesOf(VillagerStates::InHand);
	ASSERT_EQ(values.size(), 1u);
	EXPECT_EQ(As<uint32_t>(values[0]), 6u);
}

// A villager on a footpath writes the footpath and its node again, as its living part names them
TEST_F(VillagerStateValues, AVillagerOnAFootpathWritesItsFootpathAndNode)
{
	gamesave::LivingFields living;
	living.footpath.number = 12;
	living.footpathNode.number = 13;
	for (const auto state : {VillagerStates::MoveOnPath, VillagerStates::MoveAlongPath})
	{
		const auto values = ValuesOf(state, 0, living);
		ASSERT_EQ(values.size(), 2u);
		EXPECT_EQ(values[0].number, 12u);
		EXPECT_EQ(values[1].number, 13u);
	}
}

// A state whose values openblack does not hold gives none: pushing an object out of a clearing, which openblack never
// enters
TEST_F(VillagerStateValues, AStateNotHeldGivesNone)
{
	EXPECT_FALSE(HeldVillagerStateValues(_context, _villager, State(VillagerStates::ArriveAtPushObject), {}).has_value());
}

// A football player writes whether it chooses its substate again, the turns in it and its place among its position's
// players as one byte each, then the pitch
TEST_F(VillagerStateValues, AFootballPlayerWritesItsSubstateBytesThenThePitch)
{
	const auto pitch = MakeNamed(_registry);
	Villager().footballPitch = pitch;
	Villager().footballChooseAgain = true;
	Villager().footballSubstateTurns = 17;
	Villager().footballNumber = 2;
	Villager().footballSubstate = 5; // the action's own byte, not a state value
	const auto values = ValuesOf(VillagerStates::FootballAttacker);
	ASSERT_EQ(values.size(), 4u);
	EXPECT_EQ(As<uint8_t>(values[0]), 1);
	EXPECT_EQ(As<uint8_t>(values[1]), 17);
	EXPECT_EQ(As<uint8_t>(values[2]), 2);
	EXPECT_EQ(values[3].number, _context.Reference(pitch));
}

// A shepherd writes the turns it waits and the state it goes on to; before its first wait 0 and 0, as it is made
TEST_F(VillagerStateValues, AShepherdWritesItsWaitAndTheStateAfter)
{
	auto values = ValuesOf(VillagerStates::ShepherdDecideWhatToDoWithFlock);
	ASSERT_EQ(values.size(), 2u);
	EXPECT_EQ(As<int16_t>(values[0]), 0);
	EXPECT_EQ(As<uint16_t>(values[1]), 0);
	_registry.Assign<components::ShepherdWork>(
	    _villager, components::ShepherdWork {.waitTurns = -3, .pending = VillagerStates::ShepherdMoveFlockBack});
	values = ValuesOf(VillagerStates::ShepherdWaitForFlock);
	ASSERT_EQ(values.size(), 2u);
	EXPECT_EQ(As<int16_t>(values[0]), -3);
	EXPECT_EQ(As<uint16_t>(values[1]), State(VillagerStates::ShepherdMoveFlockBack));
}

// A villager in a script's states writes the script's place in whole map x and z, its number, then its pair of whole
// numbers; 0 for each no script gave yet
TEST_F(VillagerStateValues, AVillagerInAScriptWritesItsPlaceNumberAndPair)
{
	auto values = ValuesOf(VillagerStates::InScript);
	ASSERT_EQ(values.size(), 4u);
	EXPECT_EQ((As<std::array<int32_t, 2>>(values[0])), (std::array<int32_t, 2> {0, 0}));
	EXPECT_EQ(As<float>(values[1]), 0.0f);
	EXPECT_EQ(As<uint32_t>(values[2]), 0u);
	EXPECT_EQ(As<uint32_t>(values[3]), 0u);

	_registry.Assign<components::ScriptStateArguments>(
	    _villager, components::ScriptStateArguments {.place = glm::vec2(1234.5f, 2001.25f), .number = 12.5f});
	_registry.Assign<components::ScriptAnimation>(_villager,
	                                              components::ScriptAnimation {.clip = static_cast<AnimId>(5), .playsLeft = 9});
	for (const auto state : {VillagerStates::ScriptWanderAroundPosition, VillagerStates::ScriptPlayAnim})
	{
		values = ValuesOf(state);
		ASSERT_EQ(values.size(), 4u);
		EXPECT_EQ((As<std::array<int32_t, 2>>(values[0])),
		          (std::array<int32_t, 2> {map_coords::ToFixed(1234.5f), map_coords::ToFixed(2001.25f)}));
		EXPECT_EQ(As<float>(values[1]), 12.5f);
		EXPECT_EQ(As<uint32_t>(values[2]), 5u);
		EXPECT_EQ(As<uint32_t>(values[3]), 9u);
	}
}

// A villager by a fire writes the place it goes round the fire to in whole map x and z, then the fire; none before a
// fire, 0 and no fire, as it is made
TEST_F(VillagerStateValues, AVillagerByAFireWritesItsPlaceThenTheFire)
{
	auto values = ValuesOf(VillagerStates::MoveAroundFire);
	ASSERT_EQ(values.size(), 2u);
	EXPECT_EQ((As<std::array<int32_t, 2>>(values[0])), (std::array<int32_t, 2> {0, 0}));
	EXPECT_EQ(values[1].number, 0u);
	_registry.Assign<components::VillagerFireState>(
	    _villager, components::VillagerFireState {.fire = 42, .savedDestination = glm::vec2(1500.25f, 312.5f)});
	values = ValuesOf(VillagerStates::OnFire);
	ASSERT_EQ(values.size(), 2u);
	EXPECT_EQ((As<std::array<int32_t, 2>>(values[0])),
	          (std::array<int32_t, 2> {map_coords::ToFixed(1500.25f), map_coords::ToFixed(312.5f)}));
	EXPECT_EQ(values[1].number,
	          _context.Reference(SourceKey {.type = gamesave::FireEffectRecord::k_Type, .id = uint64_t {42}}));
}

// A villager hiding writes its turns there, then the place of the building it hides in, as that building stands
TEST_F(VillagerStateValues, AHidingVillagerWritesItsTurnsThenItsBuildingsPlace)
{
	const auto building = MakeNamed(_registry);
	_registry.Assign<components::Transform>(building, glm::vec3(20.0f, 0.0f, 30.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_registry.Assign<components::VillagerHiding>(_villager, components::VillagerHiding {.building = building, .turns = 7});
	for (const auto state : {VillagerStates::GoAndHideInNearbyBuilding, VillagerStates::LookToSeeIfItIsSafe})
	{
		const auto values = ValuesOf(state);
		ASSERT_EQ(values.size(), 2u);
		EXPECT_EQ(As<uint32_t>(values[0]), 7u);
		const auto place = PositionPartOf(_context, building, nullptr).position;
		EXPECT_EQ((As<std::array<int32_t, 2>>(values[1])), (std::array<int32_t, 2> {place.x, place.z}));
		EXPECT_EQ(place.x, map_coords::ToFixed(20.0f));
	}
}

// A villager amazed by a shield writes the point it looks at, x and z
TEST_F(VillagerStateValues, AVillagerAmazedByAShieldWritesThePointItLooksAt)
{
	_registry.Assign<components::VillagerShieldState>(_villager, components::VillagerShieldState {.lookAt = {65536, -2}});
	const auto values = ValuesOf(VillagerStates::AmazedByMagicShieldReaction);
	ASSERT_EQ(values.size(), 1u);
	EXPECT_EQ((As<std::array<int32_t, 2>>(values[0])), (std::array<int32_t, 2> {65536, -2}));
}

// A villager inspecting a creature writes the distance it looks from and the turns before it walks closer; a
// bewildered one the next tree's number and how many it has looked at
TEST_F(VillagerStateValues, AnInspectingOrBewilderedVillagerWritesItsTwoWords)
{
	_registry.Assign<components::VillagerInspection>(
	    _villager, components::VillagerInspection {.distance = 20.0f, .turnsUntilApproach = 11});
	auto values = ValuesOf(VillagerStates::InspectCreatureReaction);
	ASSERT_EQ(values.size(), 2u);
	EXPECT_EQ(As<float>(values[0]), 20.0f);
	EXPECT_EQ(As<uint32_t>(values[1]), 11u);
	_registry.Assign<components::MagicTreeWatch>(_villager, components::MagicTreeWatch {.next = 3, .looked = 2});
	values = ValuesOf(VillagerStates::PerformBewilderedByMagicTreeReaction);
	ASSERT_EQ(values.size(), 2u);
	EXPECT_EQ(As<uint32_t>(values[0]), 3u);
	EXPECT_EQ(As<uint32_t>(values[1]), 2u);
}
