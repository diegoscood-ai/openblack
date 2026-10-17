/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Who forgets a thing that breaks buildings when it leaves the physics
// (docs/wiki/physics/physics-impacts.md#buildings-that-break): only the buildings listed in the physics, as the last thing that
// struck them and as their pass-through partner, and only for a thing that breaks buildings. Every other body keeps its
// partner. And what nobody forgets: a body's hitter is a place in the list, kept from turn to turn, looked up only when the
// turn's force counts (ECS/Physics/Hitter).

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/BuildingDamageWorld.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Physics/DynamicsState.h"
#include "ECS/Physics/Hitter.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/BuildingDamageSystem.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "Locator.h"
#include "Physics/DamageMesh.h"
#include "Physics/TurnRules.h"
#include "support/DynamicsFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The physics of a test with its own entities, and its buildings writing down what they are asked to forget
class ForgetHitterTest: public ::testing::Test
{
protected:
	ForgetHitterTest()
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_dynamics = std::make_unique<ecs::systems::DynamicsSystem>(
		    std::make_unique<test::FakeDynamicsWorld>(Entities(), nullptr, &_buildings));
	}

	static ecs::Registry& Entities() { return Locator::entitiesRegistry::value(); }

	/// A new entity listed in the physics with its partner
	entt::entity Listed(entt::entity thrower = entt::null)
	{
		const auto entity = Entities().Create();
		auto entry = std::make_unique<ecs::PhysicsEntry>();
		entry->entity = entity;
		entry->thrower = thrower;
		_dynamics->GetState().objects.push_back(std::move(entry));
		return entity;
	}

	/// The thing taken out: marked for deletion, so that leaving the physics puts nothing back in the map's cells
	entt::entity Leaving(entt::entity thing)
	{
		Entities().Assign<Unavailable>(thing);
		return thing;
	}

	/// A thing that breaks buildings: a kind the physics counts as a rock
	entt::entity Rock()
	{
		const auto rock = Listed();
		Entities().Assign<MobileStatic>(rock, MobileStaticInfo::WeepingStone);
		return rock;
	}

	/// The partner of an entity's entry, as listed (whatever its object's state)
	entt::entity PartnerOf(entt::entity entity)
	{
		for (const auto& entry : _dynamics->GetState().objects)
		{
			if (entry->entity == entity)
			{
				return entry->thrower;
			}
		}
		throw std::logic_error("not listed");
	}

	const test::RestoreService<Locator::entitiesRegistry> _restore;
	test::RecordingBuildingDamage _buildings;
	std::unique_ptr<ecs::systems::DynamicsSystem> _dynamics;
};

/// What the broken buildings need of the game, for a test of their own entities: no models and no physics
class EntitiesOnlyBuildingWorld final: public ecs::building_world::World
{
public:
	explicit EntitiesOnlyBuildingWorld(ecs::Registry& registry)
	    : _registry(registry)
	{
	}
	[[nodiscard]] ecs::Registry& Entities() override { return _registry; }
	[[nodiscard]] const ecs::Registry& Entities() const override { return _registry; }
	[[nodiscard]] resources::ResourcesInterface& Resources() override
	{
		throw std::logic_error("the test's buildings have no resource caches");
	}
	[[nodiscard]] ecs::systems::DynamicsSystemInterface& Dynamics() override
	{
		throw std::logic_error("the test's buildings have no physics");
	}

private:
	ecs::Registry& _registry;
};

/// A list of n bodies for the hitter's tests: nothing but their places
ecs::physics::hitter::Entries ListOf(size_t n)
{
	ecs::physics::hitter::Entries list;
	for (size_t i = 0; i < n; ++i)
	{
		list.push_back(std::make_unique<ecs::PhysicsEntry>());
	}
	return list;
}
} // namespace

TEST_F(ForgetHitterTest, ARockRemovedIsForgottenByTheListedBuildingsOnly)
{
	const auto rock = Rock();
	const auto hut = Listed(rock);
	Entities().Assign<Abode>(hut);
	const auto pit = Listed(rock);
	Entities().Assign<StoragePit>(pit);
	const auto piece = Listed(rock);
	Entities().Assign<Fragment>(piece);
	const auto other = Listed(rock);

	_dynamics->RemoveObject(Leaving(rock));

	// the hut and the store collide with it again, and are asked to forget it as their last hitter, in list order
	EXPECT_EQ(PartnerOf(hut), entt::entity {entt::null});
	EXPECT_EQ(PartnerOf(pit), entt::entity {entt::null});
	const std::vector<std::pair<entt::entity, entt::entity>> expected {{rock, hut}, {rock, pit}};
	EXPECT_EQ(_buildings.forgotten, expected);
	// what is not a building keeps it as its partner
	EXPECT_EQ(PartnerOf(piece), rock);
	EXPECT_EQ(PartnerOf(other), rock);
}

TEST_F(ForgetHitterTest, AThingThatBreaksNoBuildingIsNotForgotten)
{
	// a building's piece, and a thing of no breaking kind
	const auto piece = Listed();
	Entities().Assign<Fragment>(piece);
	const auto thing = Listed();
	const auto hutOfPiece = Listed(piece);
	Entities().Assign<Abode>(hutOfPiece);
	const auto hutOfThing = Listed(thing);
	Entities().Assign<Abode>(hutOfThing);

	_dynamics->RemoveObject(Leaving(piece));
	_dynamics->RemoveObject(Leaving(thing));

	EXPECT_EQ(PartnerOf(hutOfPiece), piece);
	EXPECT_EQ(PartnerOf(hutOfThing), thing);
	EXPECT_TRUE(_buildings.forgotten.empty());
}

TEST_F(ForgetHitterTest, ABuildingMarkedForDeletionForgetsItToo)
{
	const auto rock = Rock();
	const auto hut = Listed(rock);
	Entities().Assign<Abode>(hut);
	Entities().Assign<Unavailable>(hut);

	_dynamics->RemoveObject(Leaving(rock));

	EXPECT_EQ(PartnerOf(hut), entt::entity {entt::null});
	ASSERT_EQ(_buildings.forgotten.size(), 1u);
	EXPECT_EQ(_buildings.forgotten.at(0).second, hut);
}

TEST_F(ForgetHitterTest, ABuildingWithAnotherPartnerKeepsIt)
{
	const auto rock = Rock();
	const auto otherRock = Rock();
	const auto hut = Listed(otherRock);
	Entities().Assign<Abode>(hut);

	_dynamics->RemoveObject(Leaving(rock));

	// asked to forget the rock as its last hitter, it keeps the other one as its partner
	EXPECT_EQ(PartnerOf(hut), otherRock);
	const std::vector<std::pair<entt::entity, entt::entity>> expected {{rock, hut}};
	EXPECT_EQ(_buildings.forgotten, expected);
}

TEST(BuildingForgetsHitter, OnlyTheHitterGivenOrAny)
{
	const test::RestoreService<Locator::entitiesRegistry> restore;
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& registry = Locator::entitiesRegistry::value();
	ecs::systems::BuildingDamageSystem buildings(std::make_unique<EntitiesOnlyBuildingWorld>(registry));
	const auto hut = registry.Create();
	const auto rock = registry.Create();
	const auto otherRock = registry.Create();
	auto& damage = registry.Assign<BuildingDamage>(hut);
	damage.mesh = std::make_shared<const openblack::physics::damage::Mesh>();
	damage.lastHitter = rock;

	// another rock: the hut keeps its last hitter
	buildings.ForgetHitter(otherRock, hut);
	EXPECT_EQ(registry.Get<BuildingDamage>(hut).lastHitter, rock);
	// its own last hitter: forgotten
	buildings.ForgetHitter(rock, hut);
	EXPECT_EQ(registry.Get<BuildingDamage>(hut).lastHitter, entt::entity {entt::null});
	// no hitter given (its body set up again): forgotten whatever it was
	registry.Get<BuildingDamage>(hut).lastHitter = otherRock;
	buildings.ForgetHitter(entt::null, hut);
	EXPECT_EQ(registry.Get<BuildingDamage>(hut).lastHitter, entt::entity {entt::null});
}

TEST(PhysicsHitter, TheHitterSurvivesTheTurnStart)
{
	auto list = ListOf(2);
	auto& a = *list.at(0);
	a.hitBySlot = 1;
	a.body.lastHit = &list.at(1)->body;
	a.forceSum = glm::vec3(1.0f, 2.0f, 3.0f);

	ecs::physics::hitter::ClearTurnContacts(a);

	// the force and the body last touched go; the hitter stays
	EXPECT_EQ(a.hitBySlot, std::optional<uint32_t> {1});
	EXPECT_EQ(a.body.lastHit, nullptr);
	EXPECT_EQ(a.forceSum, glm::vec3(0.0f));
	EXPECT_EQ(ecs::physics::hitter::HitterEntry(list, a), list.at(1).get());
}

TEST_F(ForgetHitterTest, ARemovedHitterIsNotForgottenItsSlotIsNamed)
{
	const auto a = Listed();
	const auto b = Listed();
	const auto c = Listed();
	auto& objects = _dynamics->GetState().objects;
	objects.at(0)->hitBySlot = 1; // B hit A

	_dynamics->RemoveObject(Leaving(b));

	// C moved into B's place, and A's hitter names that place: C now
	ASSERT_EQ(objects.size(), 2u);
	EXPECT_EQ(objects.at(0)->entity, a);
	EXPECT_EQ(objects.at(0)->hitBySlot, std::optional<uint32_t> {1});
	const auto* hitter = ecs::physics::hitter::HitterEntry(objects, *objects.at(0));
	ASSERT_NE(hitter, nullptr);
	EXPECT_EQ(hitter->entity, c);
}

TEST(PhysicsHitter, AGrowthInTheLoopKeepsTheOldHitterAndNoCredit)
{
	// sixteen bodies: the fifth last touched the seventh, which has a player's credit; its hitter is the third, from an
	// earlier turn
	const auto make = []() {
		auto list = ListOf(16);
		list.at(7)->byPlayer = true;
		list.at(5)->body.lastHit = &list.at(7)->body;
		list.at(5)->hitBySlot = 3;
		ecs::physics::hitter::CaptureLastHits(list);
		return list;
	};

	// an earlier body's reaction adds a seventeenth, and the list grows: the body touched is not found
	auto grown = make();
	EXPECT_EQ(grown.at(5)->lastHitSlot, std::optional<uint32_t> {7});
	grown.push_back(std::make_unique<ecs::PhysicsEntry>());
	ecs::physics::hitter::FindHitter(grown, 5, true);
	EXPECT_EQ(grown.at(5)->hitBySlot, std::optional<uint32_t> {3});
	EXPECT_FALSE(grown.at(5)->byPlayer);

	// with no growth it is the seventh, and takes its credit
	auto same = make();
	ecs::physics::hitter::FindHitter(same, 5, false);
	EXPECT_EQ(same.at(5)->hitBySlot, std::optional<uint32_t> {7});
	EXPECT_TRUE(same.at(5)->byPlayer);

	// a body that touched nothing has no hitter, grown or not
	auto none = make();
	none.at(4)->hitBySlot = 2;
	ecs::physics::hitter::FindHitter(none, 4, true);
	EXPECT_FALSE(none.at(4)->hitBySlot.has_value());
}

TEST(PhysicsHitter, AnAddObjectInTheLoopMovesTheNamedSlot)
{
	// five bodies: the second last touched the third (resting), the fourth last touched the fifth (the last)
	auto list = ListOf(5);
	auto* last = list.at(4).get();
	list.at(1)->body.lastHit = &list.at(2)->body;
	list.at(3)->body.lastHit = &list.at(4)->body;
	ecs::physics::hitter::CaptureLastHits(list);

	// the first body's reaction puts the third into flight: its resting body goes (the last moves into its place) and
	// its moving body is added at the end
	openblack::physics::turn::RemoveByMovingLast(list, 2);
	list.push_back(std::make_unique<ecs::PhysicsEntry>());
	auto* added = list.back().get();

	ecs::physics::hitter::FindHitter(list, 1, false);
	ecs::physics::hitter::FindHitter(list, 3, false);
	// the second's hitter is the body that moved into the third place, the fourth's the body added in the last place
	EXPECT_EQ(ecs::physics::hitter::HitterEntry(list, *list.at(1)), last);
	EXPECT_EQ(ecs::physics::hitter::HitterEntry(list, *list.at(3)), added);
}

TEST(PhysicsHitter, BelowTheThresholdTheImpactIsLeft)
{
	auto list = ListOf(2);
	auto& a = *list.at(0);
	a.forceSum = glm::vec3(0.0f, 140.0f, 0.0f);
	ASSERT_TRUE(ecs::physics::hitter::TakeImpact(a));
	EXPECT_FLOAT_EQ(a.impact, 7.0f);
	a.hitBySlot = 1;

	// a turn whose force does not count: the impact stays, and the hitter is not looked up (turn::EndOfTurn)
	a.forceSum = glm::vec3(0.0f);
	EXPECT_FALSE(ecs::physics::hitter::TakeImpact(a));
	EXPECT_FLOAT_EQ(a.impact, 7.0f);
	EXPECT_EQ(a.hitBySlot, std::optional<uint32_t> {1});
}
