/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The objects a particle system carries: the take and the release (the object's carried flag, and the end of physics of
// the object's own class), the tornado service's carry, the vortex's fling and PushObject on a carried object. And the
// InPhysics tag, which the physics' list and the carry keep: on an object with a moving body or a carried one; and on a
// thing a script made immovable that a hand lets go of, which hangs where it was let go, with no body, until a hand's
// pick of it puts it back in the map. And the start of physics a script's create gives a new mobile object: standing
// still, settled on the land. And what the physics asks of the toys and the buildings (their constants, sounds and
// weights), the end of physics off the map, and the raise of a body let go of over what is under it.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectFlags.h"
#include "ECS/Physics/DynamicsState.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Physics/ParticleCarriedObjects.h"
#include "ECS/Physics/ScriptCreate.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PhysicsGameHooks.h"
#include "ECS/PhysicsGround.h"
#include "ECS/PhysicsHooksWorld.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/Systems/Implementations/HandSystemDetail.h"
#include "ECS/Systems/Implementations/TornadoSystem.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Trees.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Physics/Body.h"
#include "Physics/LivingRules.h"
#include "Resources/ResourceManager.h"
#include "creature/CreatureSystemFakes.h"
#include "support/DynamicsFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace particle_carried_objects = openblack::ecs::physics::particle_carried_objects;
namespace from_hand = openblack::ecs::physics::from_hand;
namespace script_create = openblack::ecs::physics::script_create;

class ParticleCarriedObjectsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		map_cells::Clear();
		object_index::OnLoadMap();
	}
	void TearDown() override
	{
		map_cells::Clear();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A static mobile object (MobileStatic) in the map cells
	static entt::entity Rock(const glm::vec3& p)
	{
		const auto e = Reg().Create();
		object_index::Assign(e);
		Reg().Assign<Transform>(e, p, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<MobileStatic>(e);
		map_cells::InsertMapObject(e);
		return e;
	}
};

TEST_F(ParticleCarriedObjectsTest, FireSeesTheMapFlag)
{
	// IsObjectInMap for the fire: the map flag, so an object taken out of the map (held, flying, carried)
	// is not in the map, and a fireball never is
	const auto rock = Rock(glm::vec3(105.0f, 0.0f, 205.0f));
	EXPECT_TRUE(fire::traits::IsObjectInMap(rock));
	map_cells::RemoveMapObject(rock);
	EXPECT_FALSE(fire::traits::IsObjectInMap(rock));
	const auto ball = Reg().Create();
	Reg().Assign<Transform>(ball, glm::vec3(10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<MagicFireBall>(ball);
	EXPECT_FALSE(fire::traits::IsObjectInMap(ball));
}

TEST_F(ParticleCarriedObjectsTest, TakeAndReleaseRoundTrip)
{
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg()));
	const auto rock = Rock(glm::vec3(105.0f, 0.0f, 205.0f));
	ASSERT_TRUE(map_cells::IsObjectInMap(rock));
	EXPECT_TRUE(dynamics.CanWakeKnockedProxy(rock));

	// Take: out of the map cells, the carried flag set; a second take is refused (already in physics)
	ASSERT_TRUE(particle_carried_objects::Take(rock));
	EXPECT_TRUE(particle_carried_objects::IsCarried(rock));
	EXPECT_TRUE(Reg().AllOf<CarriedByTornado>(rock));
	EXPECT_TRUE(Reg().AllOf<InPhysics>(rock));
	EXPECT_FALSE(map_cells::IsObjectInMap(rock));
	EXPECT_FALSE(particle_carried_objects::Take(rock));
	// PushObject on a carried object: no body, no force
	EXPECT_EQ(dynamics.PushObject(rock), 0.0f);
	EXPECT_EQ(dynamics.Find(rock), nullptr);
	// a knocked resting proxy of a carried object is not woken
	EXPECT_FALSE(dynamics.CanWakeKnockedProxy(rock));

	// Release: the atom's angles (a MobileStatic keeps the tilt), Pos = ftol(x 6553.6) with altitude 0, back in the
	// map, the carried flag cleared
	const auto rows = affine::RotationYXZ(0.4f, 0.1f, -0.2f);
	particle_carried_objects::Release(rock, rows, glm::vec3(305.3f, 40.0f, 405.7f));
	EXPECT_FALSE(particle_carried_objects::IsCarried(rock));
	EXPECT_FALSE(Reg().AllOf<InPhysics>(rock));
	EXPECT_TRUE(map_cells::IsObjectInMap(rock));
	EXPECT_TRUE(dynamics.CanWakeKnockedProxy(rock));
	const auto& transform = Reg().Get<const Transform>(rock);
	EXPECT_EQ(transform.position.x, map_coords::ToMetres(map_coords::ToFixed(305.3f)));
	EXPECT_EQ(transform.position.z, map_coords::ToMetres(map_coords::ToFixed(405.7f)));
	EXPECT_EQ(transform.position.y, 0.0f); // no island: the ground is 0 and the altitude 0
	for (int c = 0; c < 3; ++c)
	{
		for (int r = 0; r < 3; ++r)
		{
			EXPECT_NEAR(transform.rotation[c][r], rows[c][r], 1e-6f);
		}
	}
}

TEST_F(ParticleCarriedObjectsTest, TornadoCarriesWhatItCanTake)
{
	ecs::systems::TornadoSystem tornado;
	// nothing there: not carried
	EXPECT_FALSE(tornado.Carry(entt::null));

	// a rock on the map: taken (out of the map cells, marked carried) and carried
	const auto rock = Rock(glm::vec3(105.0f, 0.0f, 205.0f));
	EXPECT_TRUE(tornado.Carry(rock));
	EXPECT_TRUE(particle_carried_objects::IsCarried(rock));
	EXPECT_FALSE(map_cells::IsObjectInMap(rock));

	// carried already: another particle carries it too, with no second take (still carried, still off the map)
	EXPECT_TRUE(tornado.Carry(rock));
	EXPECT_TRUE(particle_carried_objects::IsCarried(rock));
	EXPECT_FALSE(map_cells::IsObjectInMap(rock));

	// in flight (a physics object whose body is not resting): not carried and not taken (not marked, still in the map)
	const auto flying = Rock(glm::vec3(115.0f, 0.0f, 215.0f));
	auto& objects = Locator::dynamicsSystem::value().GetState().objects;
	auto body = std::make_unique<ecs::PhysicsEntry>();
	body->entity = flying;
	body->body.resting = false;
	objects.push_back(std::move(body));
	ASSERT_TRUE(Locator::dynamicsSystem::value().IsFlying(flying));
	EXPECT_FALSE(tornado.Carry(flying));
	EXPECT_FALSE(particle_carried_objects::IsCarried(flying));
	EXPECT_FALSE(Reg().AllOf<CarriedByTornado>(flying));
	EXPECT_TRUE(map_cells::IsObjectInMap(flying));
	objects.pop_back();
}

namespace
{
/// The class hooks of a test object whose body needs no model: the creature class's own body, an empty one
class EmptyBodyHooks final: public ecs::systems::PhysicsClassHooks
{
public:
	bool SetUpBody([[maybe_unused]] ecs::systems::DynamicsSystemInterface& dynamics, [[maybe_unused]] entt::entity object,
	               [[maybe_unused]] openblack::physics::Body& body) override
	{
		return true;
	}
};
} // namespace

TEST_F(ParticleCarriedObjectsTest, InPhysicsFollowsTheMovingBody)
{
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg()));
	dynamics.SetClassHooks(std::make_unique<EmptyBodyHooks>());
	// a mobile object of the creature class, so that its body is the hooks' and needs no model; marked for deletion, so
	// that taking it out of the physics puts nothing back in the map's cells
	const auto thing = Reg().Create();
	Reg().Assign<Transform>(thing, glm::vec3(105.0f, 0.0f, 205.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<MobileObject>(thing);
	Reg().Assign<Creature>(thing);
	Reg().Assign<Unavailable>(thing);
	EXPECT_FALSE(Reg().AllOf<InPhysics>(thing));

	// a moving body: in the physics
	ASSERT_NE(dynamics.AddObject(thing, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f)), nullptr);
	EXPECT_TRUE(dynamics.IsFlying(thing));
	EXPECT_TRUE(Reg().AllOf<InPhysics>(thing));
	// a second one is refused, and it stays in
	EXPECT_EQ(dynamics.AddObject(thing, glm::vec3(0.0f), glm::vec3(0.0f)), nullptr);
	EXPECT_TRUE(Reg().AllOf<InPhysics>(thing));

	// its entry goes: out of the physics
	dynamics.RemoveObject(thing);
	EXPECT_EQ(dynamics.Find(thing), nullptr);
	EXPECT_FALSE(Reg().AllOf<InPhysics>(thing));

	// a resting body that only waits to be hit is not in the physics, and its going changes nothing
	auto resting = std::make_unique<ecs::PhysicsEntry>();
	resting->entity = thing;
	resting->body.resting = true;
	dynamics.GetState().objects.push_back(std::move(resting));
	ASSERT_NE(dynamics.Find(thing), nullptr);
	dynamics.RemoveObject(thing);
	EXPECT_FALSE(Reg().AllOf<InPhysics>(thing));
}

TEST_F(ParticleCarriedObjectsTest, AStartFromTheHandKeepsItsThrowerAndItsPlayer)
{
	// what a creature lets go of has the creature as its thrower and its player as the throw's; a hand's has no thrower
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg()));
	dynamics.SetClassHooks(std::make_unique<EmptyBodyHooks>());
	// mobile objects of the creature class, so that their bodies are the hooks' and need no model; marked for deletion,
	// so that nothing is put back in the map's cells
	const auto thing = [](glm::vec3 position) {
		const auto e = Reg().Create();
		Reg().Assign<Transform>(e, position, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<MobileObject>(e);
		Reg().Assign<Creature>(e);
		Reg().Assign<Unavailable>(e);
		return e;
	};
	const auto creature = Reg().Create();
	const auto letGo = thing(glm::vec3(105.0f, 0.0f, 205.0f));
	const auto* byCreature =
	    dynamics.InitialisePhysics(letGo, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f), PlayerNames::PLAYER_TWO, creature);
	ASSERT_NE(byCreature, nullptr);
	EXPECT_EQ(byCreature->thrower, creature);
	EXPECT_EQ(byCreature->throwerPlayer, std::optional(PlayerNames::PLAYER_TWO));
	EXPECT_NE(byCreature->flags & ecs::PhysicsEntry::k_FromHand, 0u);

	const auto dropped = thing(glm::vec3(115.0f, 0.0f, 215.0f));
	const auto* byHand =
	    dynamics.InitialisePhysics(dropped, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f), PlayerNames::PLAYER_ONE, entt::null);
	ASSERT_NE(byHand, nullptr);
	EXPECT_EQ(byHand->thrower, entt::entity {entt::null});
	EXPECT_EQ(byHand->throwerPlayer, std::optional(PlayerNames::PLAYER_ONE));
	dynamics.RemoveObject(letGo);
	dynamics.RemoveObject(dropped);
}

namespace
{
/// The class hooks of a test object whose body needs no model: a small triangle about the object's origin, as the
/// object's Transform places it
class TriangleBodyHooks final: public ecs::systems::PhysicsClassHooks
{
public:
	bool SetUpBody([[maybe_unused]] ecs::systems::DynamicsSystemInterface& dynamics, entt::entity object,
	               openblack::physics::Body& body) override
	{
		constexpr std::array<glm::vec3, 3> k_Local = {glm::vec3(1.3f, 0.2f, -0.7f), glm::vec3(-0.9f, 0.6f, 0.4f),
		                                              glm::vec3(0.1f, -1.1f, 0.8f)};
		constexpr std::array<std::array<uint32_t, 3>, 1> k_Triangles = {{{0, 1, 2}}};
		constexpr openblack::physics::PhysicsData k_Data = {0.8f, 300.0f, 30.0f, 0.5f, 0.3f, 0.1f};
		const auto& transform = Reg().Get<const Transform>(object);
		body.Initialise(1.0f, 1.0f);
		body.SetUpConstants(6.75f, k_Data, true);
		body.BuildShape(k_Local, k_Triangles, glm::vec3(0.0f), 1.5f, 1.0f, transform.rotation, transform.position);
		return true;
	}

private:
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
};
} // namespace

TEST_F(ParticleCarriedObjectsTest, AScriptsNewObjectGoesIntoThePhysicsStillAndSettlesOnTheLand)
{
	// a script's create of a mobile object (a one-shot miracle among them): a body standing still, from no thrower and
	// not from a hand, lowered onto the land along its slope; the object stays where it was made until the physics moves
	// it, and a second start is refused while the body moves
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg()));
	dynamics.SetClassHooks(std::make_unique<TriangleBodyHooks>());
	// a mobile object of the creature class, so that its body is the hooks'; marked for deletion, so that taking it out
	// of the physics puts nothing back in the map's cells
	const auto thing = Reg().Create();
	Reg().Assign<Transform>(thing, glm::vec3(105.0f, 40.0f, 205.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<MobileObject>(thing);
	Reg().Assign<Creature>(thing);
	Reg().Assign<Unavailable>(thing);
	const ecs::PhysicsGround flatLand(nullptr); // no island: the land is flat at height 0

	auto* po = script_create::PutIntoPhysics(dynamics, thing, flatLand);
	ASSERT_NE(po, nullptr);
	EXPECT_EQ(dynamics.Find(thing), po);
	EXPECT_TRUE(dynamics.IsFlying(thing));
	EXPECT_EQ(po->body.velocity, glm::vec3(0.0f));
	EXPECT_EQ(po->thrower, entt::entity {entt::null});
	EXPECT_NE(po->flags & ecs::PhysicsEntry::k_Awake, 0u);
	EXPECT_EQ(po->flags & ecs::PhysicsEntry::k_FromHand, 0u);
	EXPECT_EQ(po->flags & ecs::PhysicsEntry::k_NoObjectCollision, 0u);
	// turned to the flat land and lowered from 40 until its lowest point touches it
	EXPECT_EQ(po->body.Axes()[1], glm::vec3(0.0f, 1.0f, 0.0f));
	const auto lowest = std::ranges::min(po->body.Points(), {}, [](const auto& q) { return q.world.y; }).world.y;
	EXPECT_NEAR(lowest, 0.0f, 1e-4f);
	// the turn starts from the settled pose
	EXPECT_EQ(po->turnStartCentre, po->body.Centre());
	// the object itself has not moved yet
	EXPECT_EQ(Reg().Get<const Transform>(thing).position, glm::vec3(105.0f, 40.0f, 205.0f));

	// already moving in the physics: no second body
	EXPECT_EQ(script_create::PutIntoPhysics(dynamics, thing, flatLand), nullptr);
	dynamics.RemoveObject(thing);
}

TEST_F(ParticleCarriedObjectsTest, AScriptsNewRockMadeAsAMobileStaticGoesIntoThePhysicsWhereItIs)
{
	// a rock a script's create makes as a mobile static: a body standing still where the script put it, 40 above the
	// land, neither turned nor lowered, so that it falls from there; a second start is refused while the body moves
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg()));
	dynamics.SetClassHooks(std::make_unique<TriangleBodyHooks>());
	// of the creature class, so that its body is the hooks'; marked for deletion, so that taking it out of the physics
	// puts nothing back in the map's cells
	const auto thing = Reg().Create();
	Reg().Assign<Transform>(thing, glm::vec3(105.0f, 40.0f, 205.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<MobileObject>(thing);
	Reg().Assign<Creature>(thing);
	Reg().Assign<Unavailable>(thing);

	auto* po = script_create::PutIntoPhysicsWhereItIs(dynamics, thing);
	ASSERT_NE(po, nullptr);
	EXPECT_EQ(dynamics.Find(thing), po);
	EXPECT_TRUE(dynamics.IsFlying(thing));
	EXPECT_EQ(po->body.velocity, glm::vec3(0.0f));
	EXPECT_EQ(po->thrower, entt::entity {entt::null});
	EXPECT_EQ(po->flags & ecs::PhysicsEntry::k_FromHand, 0u);
	EXPECT_EQ(po->flags & ecs::PhysicsEntry::k_NoObjectCollision, 0u);
	// still as it was built: its lowest point (the triangle's, 1.1 below the origin) is far above the land
	const auto lowest = std::ranges::min(po->body.Points(), {}, [](const auto& q) { return q.world.y; }).world.y;
	EXPECT_GT(lowest, 30.0f);
	EXPECT_EQ(po->turnStartCentre, po->body.Centre());
	EXPECT_EQ(Reg().Get<const Transform>(thing).position, glm::vec3(105.0f, 40.0f, 205.0f));

	EXPECT_EQ(script_create::PutIntoPhysicsWhereItIs(dynamics, thing), nullptr);
	dynamics.RemoveObject(thing);

	// an object a script made immovable gets no body
	const auto fixed = Reg().Create();
	Reg().Assign<Transform>(fixed, glm::vec3(115.0f, 40.0f, 215.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<MobileObject>(fixed);
	Reg().Assign<Creature>(fixed);
	Reg().Assign<Unavailable>(fixed);
	object_flags::SetMoveable(fixed, false);
	EXPECT_EQ(script_create::PutIntoPhysicsWhereItIs(dynamics, fixed), nullptr);
}

TEST_F(ParticleCarriedObjectsTest, AScriptsNewObjectThatCannotMoveStaysOutOfThePhysics)
{
	// an object a script made immovable gets no body, and neither does one that can never be a physics object
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg()));
	dynamics.SetClassHooks(std::make_unique<TriangleBodyHooks>());
	const ecs::PhysicsGround flatLand(nullptr);
	const auto thing = Reg().Create();
	Reg().Assign<Transform>(thing, glm::vec3(105.0f, 40.0f, 205.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<MobileObject>(thing);
	Reg().Assign<Creature>(thing);
	Reg().Assign<Unavailable>(thing);
	object_flags::SetMoveable(thing, false);
	EXPECT_EQ(script_create::PutIntoPhysics(dynamics, thing, flatLand), nullptr);
	EXPECT_EQ(dynamics.Find(thing), nullptr);

	const auto marker = Reg().Create(); // no class of object at all
	Reg().Assign<Transform>(marker, glm::vec3(115.0f, 40.0f, 215.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_EQ(script_create::PutIntoPhysics(dynamics, marker, flatLand), nullptr);
	EXPECT_EQ(dynamics.Find(marker), nullptr);
}

TEST_F(ParticleCarriedObjectsTest, ALumpOfPooTakesItsOwnRowOfPhysicsConstants)
{
	// a lump of poo is a class of its own with row 12 of the physics constants; another mobile object takes row 1, a
	// mushroom its own
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg()));
	const auto mobileObject = [](MobileObjectInfo row) {
		const auto e = Reg().Create();
		Reg().Assign<Transform>(e, glm::vec3(105.0f, 0.0f, 205.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<MobileObject>(e, row);
		return e;
	};
	EXPECT_EQ(dynamics.ConstantsType(mobileObject(MobileObjectInfo::LumpOfPoo)), 12);
	EXPECT_EQ(dynamics.ConstantsType(mobileObject(MobileObjectInfo::EgyptBarrel)), 1);
	EXPECT_EQ(dynamics.ConstantsType(mobileObject(MobileObjectInfo::Ark)), 1);
	EXPECT_EQ(dynamics.ConstantsType(mobileObject(MobileObjectInfo::MagicMushroom)), 22);
}

TEST_F(ParticleCarriedObjectsTest, AFootballIsAPhysicsObjectWithItsOwnInfoAndRow)
{
	// a football can become a physics object, as any mobile object; its body takes the football's info and row 2 of the
	// physics constants
	auto info = std::make_unique<InfoConstants>();
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg(), info.get()));
	const auto ball = Reg().Create();
	Reg().Assign<Transform>(ball, glm::vec3(105.0f, 0.0f, 205.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<Mobile>(ball);
	Reg().Assign<Ball>(ball);
	EXPECT_TRUE(dynamics.CanBecomeAPhysicsObject(ball));
	EXPECT_EQ(dynamics.ConstantsType(ball), 2);
	EXPECT_EQ(dynamics.ObjectInfo(ball), &info->ball);
}

TEST_F(ParticleCarriedObjectsTest, ADeadTreeIsARockOfTheDeadTreeRowThatWeighsAsItsTree)
{
	// a dead tree's info is the static row "Dead Tree": its collision sound, its fire and its values. Only its weight is
	// its tree's row, times the cube of its scale
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	const test::RestoreService<Locator::dynamicsSystem> restoreDynamics;
	auto info = std::make_unique<InfoConstants>();
	auto& tree = info->tree.at(static_cast<size_t>(TreeInfo::Beech));
	tree.collideSound = static_cast<SoundCollisionType>(5);
	tree.weight = 7.0f;
	tree.combustionTemperature = 110.0f;
	tree.heatCapacity = 500.0f;
	tree.defenceMultiplierBurn = 0.5f;
	tree.sacrificeValue = 4.0f;
	auto& dead = info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::DeadTree));
	dead.collideSound = static_cast<SoundCollisionType>(9);
	dead.weight = 3.0f;
	dead.combustionTemperature = 100.0f;
	dead.heatCapacity = 1000.0f;
	dead.defenceMultiplierBurn = 0.01f;
	dead.sacrificeValue = 2.0f;
	// the fire reads the table through the physics' info, which is this one
	Locator::infoConstants::emplace(*info);
	auto& dynamics = Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>(
	    std::make_unique<test::FakeDynamicsWorld>(Reg(), info.get()));
	const auto log = Reg().Create();
	Reg().Assign<Transform>(log, glm::vec3(105.0f, 0.0f, 205.0f), glm::mat3(1.0f), glm::vec3(2.0f));
	Reg().Assign<Mobile>(log);
	Reg().Assign<DeadTree>(log, DeadTree {.type = TreeInfo::Beech});

	EXPECT_EQ(dynamics.ObjectInfo(log), &dead);
	EXPECT_EQ(dynamics.ObjectInfo(log)->collideSound, static_cast<SoundCollisionType>(9));
	EXPECT_EQ(fire::traits::InfoOf(log), &dead);
	EXPECT_FLOAT_EQ(fire::traits::CombustionTemperature(log), 100.0f);
	EXPECT_FLOAT_EQ(fire::traits::HeatCapacity(log), 1000.0f);
	EXPECT_FLOAT_EQ(fire::traits::DefenceMultiplierBurn(log), 0.01f);
	EXPECT_FLOAT_EQ(fire::traits::InfoOf(log)->sacrificeValue, 2.0f);
	EXPECT_FLOAT_EQ(dynamics.WeightOf(log), 2.0f * 2.0f * 2.0f * 7.0f);

	// a standing tree of that kind keeps its own row for all of them
	const auto standing = Reg().Create();
	Reg().Assign<Transform>(standing, glm::vec3(105.0f, 0.0f, 205.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<Tree>(standing, Tree {.type = TreeInfo::Beech});
	EXPECT_EQ(dynamics.ObjectInfo(standing), &tree);
	EXPECT_FLOAT_EQ(fire::traits::CombustionTemperature(standing), 110.0f);
	EXPECT_FLOAT_EQ(dynamics.WeightOf(standing), 7.0f);
}

TEST_F(ParticleCarriedObjectsTest, AScriptsNewMobileObjectStartsItsPhysicsByItsClass)
{
	using enum MobileObjectInfo;
	// a lump of poo, the ark, a cauldron, a barrel: their physics starts, a lump of poo whatever its row's kind says
	EXPECT_TRUE(script_create::MobileObjectStartsPhysics(LumpOfPoo, LumpOfPoo));
	EXPECT_TRUE(script_create::MobileObjectStartsPhysics(LumpOfPoo, Crop));
	EXPECT_TRUE(script_create::MobileObjectStartsPhysics(Ark, Ark));
	EXPECT_TRUE(script_create::MobileObjectStartsPhysics(Cauldron, Cauldron));
	EXPECT_TRUE(script_create::MobileObjectStartsPhysics(EgyptBarrel, EgyptBarrel));
	// a crop and a creed by their row's kind (the eyes are a creed too), whatever the row
	EXPECT_FALSE(script_create::MobileObjectStartsPhysics(Crop, Crop));
	EXPECT_FALSE(script_create::MobileObjectStartsPhysics(Creed, Creed));
	EXPECT_FALSE(script_create::MobileObjectStartsPhysics(Eyes, Creed));
	EXPECT_TRUE(script_create::MobileObjectStartsPhysics(Eyes, Eyes));
	// the mushrooms by their row
	EXPECT_FALSE(script_create::MobileObjectStartsPhysics(Champi, Champi));
	EXPECT_FALSE(script_create::MobileObjectStartsPhysics(MagicMushroom, MagicMushroom));
	EXPECT_FALSE(script_create::MobileObjectStartsPhysics(Toadstool, Toadstool));
}

TEST_F(ParticleCarriedObjectsTest, AScriptsNewMobileStaticIsARockByItsRowsKind)
{
	// the mobile static table's kind 2 makes a thing of the rock class, whatever the row, but the bonfire's row always
	// makes a bonfire; any other kind a plain mobile static
	constexpr auto k_RockKind = static_cast<MobileStaticInfo>(2);
	EXPECT_TRUE(script_create::MakesARock(MobileStaticInfo::Rock, k_RockKind));
	EXPECT_TRUE(script_create::MakesARock(MobileStaticInfo::RockChalk, k_RockKind));
	EXPECT_TRUE(script_create::MakesARock(MobileStaticInfo::Meteor, k_RockKind));
	EXPECT_FALSE(script_create::MakesARock(MobileStaticInfo::Bonfire, k_RockKind));
	EXPECT_FALSE(script_create::MakesARock(MobileStaticInfo::ToyBall, MobileStaticInfo::ToyBall));
	EXPECT_FALSE(script_create::MakesARock(MobileStaticInfo::CeltFenceShort, MobileStaticInfo::CeltFenceShort));
}

TEST_F(ParticleCarriedObjectsTest, ACarriedObjectIsInPhysicsUntilItIsLetGo)
{
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg()));
	const auto rock = Rock(glm::vec3(105.0f, 0.0f, 205.0f));
	EXPECT_FALSE(Reg().AllOf<InPhysics>(rock));

	// taken: in the physics with no body, and carried
	ASSERT_TRUE(particle_carried_objects::Take(rock));
	EXPECT_TRUE(Reg().AllOf<InPhysics>(rock));
	EXPECT_TRUE(Reg().AllOf<CarriedByTornado>(rock));
	EXPECT_EQ(dynamics.Find(rock), nullptr);

	// a resting body of the carried object that goes leaves it in the physics: the carry keeps it there
	auto resting = std::make_unique<ecs::PhysicsEntry>();
	resting->entity = rock;
	resting->body.resting = true;
	dynamics.GetState().objects.push_back(std::move(resting));
	dynamics.RemoveObject(rock);
	EXPECT_TRUE(Reg().AllOf<InPhysics>(rock));

	// let go: neither mark is left
	particle_carried_objects::Release(rock, glm::mat3(1.0f), glm::vec3(305.0f, 40.0f, 405.0f));
	EXPECT_FALSE(Reg().AllOf<InPhysics>(rock));
	EXPECT_FALSE(Reg().AllOf<CarriedByTornado>(rock));
	EXPECT_TRUE(map_cells::IsObjectInMap(rock));
}

TEST_F(ParticleCarriedObjectsTest, ATreeLetGoStaysInThePhysicsOutOfTheMap)
{
	// the tree's end of physics with no body does nothing while it is carried: it is not put back in the map, and it
	// stays in the physics with no body, as the original
	const auto tree = Reg().Create();
	object_index::Assign(tree);
	Reg().Assign<Transform>(tree, glm::vec3(105.0f, 0.0f, 205.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<Tree>(tree);
	map_cells::InsertMapObject(tree);
	ASSERT_TRUE(particle_carried_objects::Take(tree));
	EXPECT_TRUE(Reg().AllOf<InPhysics>(tree));
	EXPECT_TRUE(Reg().AllOf<CarriedByTornado>(tree));

	particle_carried_objects::Release(tree, glm::mat3(1.0f), glm::vec3(305.0f, 40.0f, 405.0f));
	EXPECT_TRUE(Reg().AllOf<InPhysics>(tree));
	EXPECT_FALSE(Reg().AllOf<CarriedByTornado>(tree));
	EXPECT_FALSE(map_cells::IsObjectInMap(tree));
}

namespace
{
/// The game's classes in a world of the test's entities, whose hand is a recording fake
class HandOnlyWorld final: public ecs::physics_hooks::World
{
public:
	HandOnlyWorld(ecs::Registry& registry, test::creature_fakes::FakeHand& hand)
	    : _registry(registry)
	    , _hand(hand)
	{
	}
	HandOnlyWorld(ecs::Registry& registry, test::creature_fakes::FakeHand& hand, test::RecordingBuildingDamage& buildings)
	    : _registry(registry)
	    , _hand(hand)
	    , _buildings(&buildings)
	{
	}
	[[nodiscard]] ecs::Registry& Entities() override { return _registry; }
	[[nodiscard]] ecs::systems::HandSystemInterface& HandPart() override { return _hand; }
	[[nodiscard]] ecs::systems::BuildingDamageSystemInterface& BuildingDamage() override
	{
		if (_buildings == nullptr)
		{
			throw std::logic_error("the test's world has no broken buildings");
		}
		return *_buildings;
	}
	[[nodiscard]] PlayerNames LocalPlayer() const override { return PlayerNames::PLAYER_ONE; }
	[[nodiscard]] bool IsToy([[maybe_unused]] entt::entity object) const override { return false; }
	[[nodiscard]] std::optional<ecs::effects::EffectValues> Crush() const override { return std::nullopt; }
	void KickSway([[maybe_unused]] entt::entity creature, [[maybe_unused]] glm::vec3 force,
	              [[maybe_unused]] glm::vec3 point) override
	{
	}
	void ApplyEffect([[maybe_unused]] entt::entity object, [[maybe_unused]] const ecs::effects::EffectValues& values,
	                 [[maybe_unused]] const magic::EffectSource& source) override
	{
	}
	void UpdateAttitudeFromFeedback([[maybe_unused]] entt::entity creature, [[maybe_unused]] float feedback) override {}
	void ChangeDesireSource([[maybe_unused]] entt::entity creature, [[maybe_unused]] uint32_t source,
	                        [[maybe_unused]] float amount) override
	{
	}

private:
	ecs::Registry& _registry;
	test::creature_fakes::FakeHand& _hand;
	test::RecordingBuildingDamage* _buildings {nullptr};
};
} // namespace

TEST_F(ParticleCarriedObjectsTest, APotLetGoEndsItsPhysicsAsAPot)
{
	// the pot's own end of physics with no body: the hand's part is asked, with no interface (there is no physics record
	// to name one). A hand's pile of wood or food on land becomes a land pile there and is not put back in the map; any
	// other pot ends as any object does, back in the map
	test::creature_fakes::FakeHand hand;
	auto& dynamics = Locator::dynamicsSystem::value();
	dynamics.SetClassHooks(std::make_unique<ecs::PhysicsGameHooks>(std::make_unique<HandOnlyWorld>(Reg(), hand)));
	const auto makePot = [](const glm::vec3& p, PotInfo type) {
		const auto e = Reg().Create();
		object_index::Assign(e);
		Reg().Assign<Transform>(e, p, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<Pot>(e, Pot {.amount = 40, .maxAmount = 100, .type = type});
		map_cells::InsertMapObject(e);
		return e;
	};

	// a hand's food pile that becomes a land pile
	const auto handPile = makePot(glm::vec3(105.0f, 0.0f, 205.0f), PotInfo::HandFood);
	ASSERT_TRUE(particle_carried_objects::Take(handPile));
	hand.potBecomesPile = true;
	particle_carried_objects::Release(handPile, glm::mat3(1.0f), glm::vec3(305.0f, 40.0f, 405.0f));
	ASSERT_EQ(hand.potEnds.size(), 1u);
	EXPECT_EQ(hand.potEnds[0].first, handPile);
	EXPECT_FALSE(hand.potEnds[0].second);
	EXPECT_FALSE(map_cells::IsObjectInMap(handPile));
	EXPECT_FALSE(Reg().AllOf<CarriedByTornado>(handPile));

	// a pot that stays a pot: back in the map, out of the physics
	const auto pot = makePot(glm::vec3(115.0f, 0.0f, 215.0f), PotInfo::FoodPile);
	ASSERT_TRUE(particle_carried_objects::Take(pot));
	hand.potBecomesPile = false;
	particle_carried_objects::Release(pot, glm::mat3(1.0f), glm::vec3(315.0f, 40.0f, 415.0f));
	ASSERT_EQ(hand.potEnds.size(), 2u);
	EXPECT_EQ(hand.potEnds[1].first, pot);
	EXPECT_FALSE(hand.potEnds[1].second);
	EXPECT_TRUE(map_cells::IsObjectInMap(pot));
	EXPECT_FALSE(Reg().AllOf<InPhysics>(pot));

	// any other object is not the hand's: back in the map
	const auto rock = Rock(glm::vec3(125.0f, 0.0f, 225.0f));
	ASSERT_TRUE(particle_carried_objects::Take(rock));
	particle_carried_objects::Release(rock, glm::mat3(1.0f), glm::vec3(325.0f, 40.0f, 425.0f));
	EXPECT_EQ(hand.potEnds.size(), 2u);
	EXPECT_TRUE(map_cells::IsObjectInMap(rock));
	dynamics.SetClassHooks(std::make_unique<ecs::systems::PhysicsClassHooks>());
}

TEST_F(ParticleCarriedObjectsTest, AFragmentEndsItsPhysicsAsAFragment)
{
	// the fragment's own end of physics is the broken buildings' part, which puts it back in the map itself. With a body
	// a big piece would stick to its building through the body's rows and centre; let go by a particle system, with no
	// body, through its own matrix
	test::creature_fakes::FakeHand hand;
	test::RecordingBuildingDamage buildings;
	auto& dynamics = Locator::dynamicsSystem::value();
	auto hooks = std::make_unique<ecs::PhysicsGameHooks>(std::make_unique<HandOnlyWorld>(Reg(), hand, buildings));
	auto& game = *hooks;
	const auto piece = Reg().Create();
	object_index::Assign(piece);
	Reg().Assign<Transform>(piece, glm::vec3(105.0f, 0.0f, 205.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<Fragment>(piece);
	map_cells::InsertMapObject(piece);
	ASSERT_EQ(dynamics.ClassOf(piece), PhysicsClass::Fragment);
	EXPECT_TRUE(game.CallsBackInMap(dynamics, piece));

	// with a body: the body's rows and centre, and what the fragment's end keeps
	ecs::PhysicsEntry entry;
	entry.entity = piece;
	const auto kept = game.EndPhysics(dynamics, entry);
	ASSERT_TRUE(kept.has_value());
	EXPECT_EQ(*kept, piece);
	ASSERT_EQ(buildings.fragmentEnds.size(), 1u);
	EXPECT_EQ(buildings.fragmentEnds[0].fragment, piece);
	EXPECT_EQ(buildings.fragmentEnds[0].rows, entry.body.Axes());
	EXPECT_EQ(buildings.fragmentEnds[0].centre, entry.body.Centre());

	// let go by a particle system: the release's place and the atom's rows
	dynamics.SetClassHooks(std::move(hooks));
	ASSERT_TRUE(particle_carried_objects::Take(piece));
	const auto tilt = affine::AngleY(0.5f);
	particle_carried_objects::Release(piece, tilt, glm::vec3(305.0f, 40.0f, 405.0f));
	ASSERT_EQ(buildings.fragmentEnds.size(), 2u);
	EXPECT_EQ(buildings.fragmentEnds[1].fragment, piece);
	EXPECT_EQ(buildings.fragmentEnds[1].rows, Reg().Get<const Transform>(piece).rotation);
	EXPECT_EQ(buildings.fragmentEnds[1].centre, Reg().Get<const Transform>(piece).position);
	// the fake's fragment end does not put it back: the class does it itself, not the release
	EXPECT_FALSE(map_cells::IsObjectInMap(piece));
	EXPECT_FALSE(Reg().AllOf<CarriedByTornado>(piece));
	dynamics.SetClassHooks(std::make_unique<ecs::systems::PhysicsClassHooks>());
}

TEST_F(ParticleCarriedObjectsTest, ADeadTreeThatLandsTellsTheVillagersWithItsThrowersPlayer)
{
	// a dead tree's own end of physics with a body: back in the map (the class does it itself), then "wood here" with
	// the player of its physics record, the hand's when a hand threw it, none otherwise; a felled tree gets none
	namespace reactions = ecs::effects::reactions;
	reactions::Clear();
	test::creature_fakes::FakeHand hand;
	auto& dynamics = Locator::dynamicsSystem::value();
	ecs::PhysicsGameHooks game(std::make_unique<HandOnlyWorld>(Reg(), hand));
	const auto makeLog = [](const glm::vec3& p) {
		const auto e = Reg().Create();
		object_index::Assign(e);
		Reg().Assign<Transform>(e, p, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<DeadTree>(e, DeadTree {.type = TreeInfo::Pine});
		return e;
	};
	const auto land = [&](entt::entity log, std::optional<PlayerNames> throwerPlayer) {
		ecs::PhysicsEntry entry;
		entry.entity = log;
		entry.throwerPlayer = throwerPlayer;
		EXPECT_TRUE(game.CallsBackInMap(dynamics, log));
		const auto kept = game.EndPhysics(dynamics, entry);
		ASSERT_TRUE(kept.has_value());
		EXPECT_EQ(*kept, log);
		EXPECT_TRUE(map_cells::IsObjectInMap(log));
	};
	const auto playerOf = [](entt::entity log) -> std::optional<PlayerNames> {
		const auto* reaction = reactions::Find(reactions::GetReactionOfTypeInitiatedBy(log, Reaction::ReactToWood));
		return reaction != nullptr ? reaction->player : std::nullopt;
	};

	const auto thrown = makeLog(glm::vec3(105.0f, 0.0f, 205.0f));
	ASSERT_EQ(dynamics.ClassOf(thrown), PhysicsClass::DeadTree);
	land(thrown, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(playerOf(thrown), std::optional(PlayerNames::PLAYER_ONE));

	const auto knocked = makeLog(glm::vec3(115.0f, 0.0f, 215.0f));
	land(knocked, std::nullopt);
	ASSERT_NE(reactions::GetReactionOfTypeInitiatedBy(knocked, Reaction::ReactToWood), 0u);
	EXPECT_EQ(playerOf(knocked), std::nullopt);

	const auto felled = makeLog(glm::vec3(125.0f, 0.0f, 225.0f));
	Reg().Assign<FelledTree>(felled);
	land(felled, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(playerOf(felled).has_value());
	reactions::Clear();
}

TEST_F(ParticleCarriedObjectsTest, AThrownTreeThatBecomesADeadTreeTellsTheVillagersWithItsThrowersPlayer)
{
	// a tree that ends its flight as a dead tree: the same entity, now a dead tree with the tree's type and wood value
	// multiplier, out of the map, and "wood here" with the player that threw it, else none
	namespace reactions = ecs::effects::reactions;
	reactions::Clear();
	const auto makeTree = [](const glm::vec3& p) {
		const auto e = Reg().Create();
		object_index::Assign(e);
		Reg().Assign<Transform>(e, p, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<Tree>(e, Tree {.type = TreeInfo::Beech, .woodValueMultiplier = 0.5f});
		map_cells::InsertMapObject(e);
		return e;
	};
	const auto playerOf = [](entt::entity dead) -> std::optional<PlayerNames> {
		const auto* reaction = reactions::Find(reactions::GetReactionOfTypeInitiatedBy(dead, Reaction::ReactToWood));
		return reaction != nullptr ? reaction->player : std::nullopt;
	};

	const auto thrown = makeTree(glm::vec3(105.0f, 0.0f, 205.0f));
	ecs::TreeBecomesDeadTree(thrown, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(Reg().AllOf<Tree>(thrown));
	ASSERT_TRUE(Reg().AllOf<DeadTree>(thrown));
	EXPECT_EQ(Reg().Get<const DeadTree>(thrown).type, TreeInfo::Beech);
	EXPECT_FLOAT_EQ(Reg().Get<const DeadTree>(thrown).woodValueMultiplier, 0.5f);
	EXPECT_FALSE(map_cells::IsObjectInMap(thrown));
	EXPECT_EQ(playerOf(thrown), std::optional(PlayerNames::PLAYER_ONE));

	const auto knocked = makeTree(glm::vec3(115.0f, 0.0f, 215.0f));
	ecs::TreeBecomesDeadTree(knocked, PlayerNames::NEUTRAL);
	EXPECT_EQ(playerOf(knocked), std::optional(PlayerNames::NEUTRAL));
	reactions::Clear();
}

TEST_F(ParticleCarriedObjectsTest, AFelledTreeTellsTheVillagersWithTheNeutralPlayer)
{
	// a forester's felled tree is made with the neutral player, whoever the forester works for: its "wood here"
	// reaction carries the neutral player, not the local one
	namespace reactions = ecs::effects::reactions;
	reactions::Clear();
	EXPECT_EQ(ecs::k_FelledTreePlayer, PlayerNames::NEUTRAL);
	const auto felled = Reg().Create();
	object_index::Assign(felled);
	Reg().Assign<Transform>(felled, glm::vec3(105.0f, 0.0f, 205.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<DeadTree>(felled, DeadTree {.type = TreeInfo::Pine});
	const auto id = ecs::CreateDeadTreeReaction(felled, ecs::k_FelledTreePlayer);
	const auto* reaction = reactions::Find(reactions::GetReactionOfTypeInitiatedBy(felled, Reaction::ReactToWood));
	ASSERT_NE(reaction, nullptr);
	EXPECT_EQ(reaction->id, id);
	EXPECT_EQ(reaction->player, PlayerNames::NEUTRAL);
	reactions::Clear();
}

TEST_F(ParticleCarriedObjectsTest, ADeadTreesFireGoingOutTellsTheVillagersWithItsOwnPlayer)
{
	// a dead tree's fire going out: "wood here" with the dead tree's own player, the one it was made for (a land
	// script's or a thrower's, neutral for a felled tree or a log); an unavailable dead tree gets none
	namespace reactions = ecs::effects::reactions;
	reactions::Clear();
	const auto makeDeadTree = [](const glm::vec3& p, PlayerNames owner) {
		const auto e = Reg().Create();
		object_index::Assign(e);
		Reg().Assign<Transform>(e, p, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<DeadTree>(e, DeadTree {.type = TreeInfo::Pine, .owner = owner});
		return e;
	};
	const auto playerOf = [](entt::entity dead) -> std::optional<PlayerNames> {
		const auto* reaction = reactions::Find(reactions::GetReactionOfTypeInitiatedBy(dead, Reaction::ReactToWood));
		return reaction != nullptr ? reaction->player : std::nullopt;
	};

	const auto owned = makeDeadTree(glm::vec3(105.0f, 0.0f, 205.0f), PlayerNames::PLAYER_TWO);
	fire::traits::EndOnFire(owned);
	EXPECT_EQ(playerOf(owned), std::optional(PlayerNames::PLAYER_TWO));

	const auto neutral = makeDeadTree(glm::vec3(115.0f, 0.0f, 215.0f), PlayerNames::NEUTRAL);
	fire::traits::EndOnFire(neutral);
	EXPECT_EQ(playerOf(neutral), std::optional(PlayerNames::NEUTRAL));

	const auto unavailable = makeDeadTree(glm::vec3(125.0f, 0.0f, 225.0f), PlayerNames::PLAYER_ONE);
	Reg().Assign<Unavailable>(unavailable);
	fire::traits::EndOnFire(unavailable);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(unavailable, Reaction::ReactToWood), 0u);
	reactions::Clear();
}

TEST_F(ParticleCarriedObjectsTest, APotThatLandsIsPutDownByTheInterfaceThatThrewIt)
{
	// a pot's own end of physics with a body: the hand's part is asked with the interface its physics record names, the
	// hand's when a hand threw it (directly or through what struck it), none for a pot knocked by anything else
	test::creature_fakes::FakeHand hand;
	auto& dynamics = Locator::dynamicsSystem::value();
	ecs::PhysicsGameHooks game(std::make_unique<HandOnlyWorld>(Reg(), hand));
	const auto pile = Reg().Create();
	Reg().Assign<Transform>(pile, glm::vec3(105.0f, 0.0f, 205.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<Pot>(pile, Pot {.amount = 40, .maxAmount = 100, .type = PotInfo::HandWood});
	ASSERT_EQ(dynamics.ClassOf(pile), PhysicsClass::Pot);
	for (const auto throwerPlayer : {std::optional(PlayerNames::PLAYER_ONE), std::optional<PlayerNames> {}})
	{
		ecs::PhysicsEntry entry;
		entry.entity = pile;
		entry.throwerPlayer = throwerPlayer;
		const auto kept = game.EndPhysics(dynamics, entry);
		ASSERT_TRUE(kept.has_value());
		EXPECT_EQ(*kept, pile);
	}
	ASSERT_EQ(hand.potEnds.size(), 2u);
	EXPECT_EQ(hand.potEnds[0].first, pile);
	EXPECT_EQ(hand.potEnds[0].second, std::optional(PlayerNames::PLAYER_ONE));
	EXPECT_EQ(hand.potEnds[1].first, pile);
	EXPECT_FALSE(hand.potEnds[1].second.has_value());
}

TEST(ParticleCarriedObjects, OnlyAHandPileOutOfTheWaterBecomesALandPile)
{
	using ecs::systems::hand_detail::HandPotBecomesPile;
	EXPECT_TRUE(HandPotBecomesPile(PotInfo::HandFood, true));
	EXPECT_TRUE(HandPotBecomesPile(PotInfo::HandWood, true));
	// in the water it stays the hand's pile
	EXPECT_FALSE(HandPotBecomesPile(PotInfo::HandFood, false));
	EXPECT_FALSE(HandPotBecomesPile(PotInfo::HandWood, false));
	// a land pile or a pot stays as it is
	EXPECT_FALSE(HandPotBecomesPile(PotInfo::FoodPile, true));
	EXPECT_FALSE(HandPotBecomesPile(PotInfo::WoodPile_1, true));
}

TEST_F(ParticleCarriedObjectsTest, ReleasedLivingKeepsItsDrawnYaw)
{
	// a Living's Transform is the drawn rotation (the original's rows + the draw's quarter turn); the atom carries it,
	// and the release's angles (DecomposeYXZ) work on the original's rows: the drawn yaw comes back unchanged, not a
	// quarter turn more
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg()));
	const auto animal = Reg().Create();
	Reg().Assign<Animal>(animal);
	const auto drawn = affine::AngleY(0.9f + glm::half_pi<float>());
	const auto back =
	    dynamics.DrawQuarterTurn(dynamics.RotationFromRows(animal, openblack::physics::living::OriginalRows(drawn)));
	for (int c = 0; c < 3; ++c)
	{
		for (int r = 0; r < 3; ++r)
		{
			EXPECT_NEAR(back[c][r], drawn[c][r], 1e-6f);
		}
	}
}

TEST_F(ParticleCarriedObjectsTest, AnImmovableThingLetGoHangsWhereItWasLetGo)
{
	// a rock a script made immovable, held up by the hand (out of the map, at the hand's point) and let go: the physics
	// gives it no body, so it stays where it was let go, in the physics with no entry, out of the map
	const auto rock = Rock(glm::vec3(105.0f, 0.0f, 205.0f));
	object_flags::SetMoveable(rock, false);
	map_cells::RemoveMapObject(rock);
	const glm::vec3 hand {106.0f, 10.0f, 204.0f};
	Reg().Get<Transform>(rock).position = hand;
	EXPECT_TRUE(from_hand::HangsWhereLetGo(rock));
	EXPECT_FALSE(from_hand::Throw(rock, glm::vec3(0.0f), false, PlayerNames::PLAYER_ONE));
	EXPECT_EQ(Reg().Get<const Transform>(rock).position, hand);
	EXPECT_TRUE(Reg().AllOf<InPhysics>(rock));
	EXPECT_FALSE(map_cells::IsObjectInMap(rock));
	EXPECT_EQ(Locator::dynamicsSystem::value().Find(rock), nullptr);
	// the map's own syncing does not file it again
	map_cells::Sync();
	EXPECT_FALSE(map_cells::IsObjectInMap(rock));
	EXPECT_TRUE(Reg().AllOf<InPhysics>(rock));
	// nor does a tornado take it: it is in the physics already
	EXPECT_FALSE(particle_carried_objects::Take(rock));

	// a hand's pick of it fails: nothing goes into the hand, and the failed pick files it back in the map, where it stays
	// in the physics, in the air
	EXPECT_FALSE(from_hand::TakeOutOfPhysicsForHand(rock));
	EXPECT_TRUE(map_cells::IsObjectInMap(rock));
	EXPECT_TRUE(Reg().AllOf<InPhysics>(rock));
	EXPECT_EQ(Reg().Get<const Transform>(rock).position, hand);
	// every later pick fails the same way, and the map's syncing leaves it in
	EXPECT_FALSE(from_hand::TakeOutOfPhysicsForHand(rock));
	map_cells::Sync();
	EXPECT_TRUE(map_cells::IsObjectInMap(rock));
	EXPECT_TRUE(Reg().AllOf<InPhysics>(rock));
}

TEST(FromHandLanding, AThingRaisedOverWhatIsUnderItDoesNotLandUnlessAComputerPlayersVillager)
{
	EXPECT_TRUE(from_hand::MayLand(false, false, false));
	EXPECT_FALSE(from_hand::MayLand(false, true, false));
	// a villager let go of for a computer player may land though raised
	EXPECT_TRUE(from_hand::MayLand(false, true, true));
	// thrown, nothing lands
	EXPECT_FALSE(from_hand::MayLand(true, false, false));
	EXPECT_FALSE(from_hand::MayLand(true, true, true));
}

TEST(FromHandRemark, OnlyALetGoOfTheLocalPlayersIsMine)
{
	EXPECT_TRUE(from_hand::IsMyLetGo(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(from_hand::IsMyLetGo(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_TWO));
	// a computer player's hand or another player's creature, or no one's
	EXPECT_FALSE(from_hand::IsMyLetGo(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(from_hand::IsMyLetGo(std::nullopt, PlayerNames::PLAYER_ONE));
}

TEST(FromHandToy, AToyLetGoOfByAPlayersHandIsThatPlayersPlay)
{
	EXPECT_EQ(from_hand::ToyPlayer(PlayerNames::PLAYER_ONE, false), std::optional(PlayerNames::PLAYER_ONE));
	// a computer player's hand plays for its own player
	EXPECT_EQ(from_hand::ToyPlayer(PlayerNames::PLAYER_THREE, false), std::optional(PlayerNames::PLAYER_THREE));
	// a creature's let-go is no one's play, nor is a let-go credited to no one
	EXPECT_FALSE(from_hand::ToyPlayer(PlayerNames::PLAYER_ONE, true).has_value());
	EXPECT_FALSE(from_hand::ToyPlayer(std::nullopt, false).has_value());
}

TEST(FromHandTree, ALandedTreeIsReplantedUnlessHeldTiltedOrNotToBeReplantedAndAlwaysFromACreature)
{
	// let go of by a hand: upright it is replanted, tilted more than 0.2 either way or not to be replanted it stays in
	// the physics; just 0.2 is upright enough
	EXPECT_FALSE(from_hand::LandedTreeStaysInPhysics(false, false, glm::vec2(0.0f)));
	EXPECT_FALSE(from_hand::LandedTreeStaysInPhysics(false, false, glm::vec2(0.2f, -0.2f)));
	EXPECT_TRUE(from_hand::LandedTreeStaysInPhysics(false, false, glm::vec2(0.25f, 0.0f)));
	EXPECT_TRUE(from_hand::LandedTreeStaysInPhysics(false, false, glm::vec2(0.0f, -0.25f)));
	EXPECT_TRUE(from_hand::LandedTreeStaysInPhysics(false, true, glm::vec2(0.0f)));
	// let go of by a creature, its thrower: replanted whatever its tilt
	EXPECT_FALSE(from_hand::LandedTreeStaysInPhysics(true, false, glm::vec2(0.5f, -0.5f)));
	EXPECT_FALSE(from_hand::LandedTreeStaysInPhysics(true, true, glm::vec2(0.0f)));
}

TEST_F(ParticleCarriedObjectsTest, OnlyAThingThatIsNotLivingHangs)
{
	// an immovable villager or animal ends its physics at once, as today; a movable thing with no body (openblack's: no
	// mesh to build one from) is put down; an immovable pot hangs like the rock
	const auto villager = Reg().Create();
	Reg().Assign<Transform>(villager, glm::vec3(105.0f, 10.0f, 205.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<Villager>(villager);
	object_flags::SetMoveable(villager, false);
	EXPECT_FALSE(from_hand::HangsWhereLetGo(villager));
	const auto animal = Reg().Create();
	Reg().Assign<Animal>(animal);
	object_flags::SetMoveable(animal, false);
	EXPECT_FALSE(from_hand::HangsWhereLetGo(animal));
	const auto pot = Reg().Create();
	Reg().Assign<MobileObject>(pot);
	object_flags::SetMoveable(pot, false);
	EXPECT_TRUE(from_hand::HangsWhereLetGo(pot));
	EXPECT_FALSE(from_hand::HangsWhereLetGo(entt::null));
}

TEST_F(ParticleCarriedObjectsTest, AThingWithNoMeshLetGoIsPutDown)
{
	// a movable rock with no mesh: the physics cannot build it a body, and it is put down on the ground, back in the
	// map, not in the physics, as before
	const auto rock = Rock(glm::vec3(105.0f, 0.0f, 205.0f));
	map_cells::RemoveMapObject(rock);
	Reg().Get<Transform>(rock).position = glm::vec3(106.0f, 10.0f, 204.0f);
	EXPECT_FALSE(from_hand::HangsWhereLetGo(rock));
	EXPECT_TRUE(from_hand::Throw(rock, glm::vec3(0.0f), false, PlayerNames::PLAYER_ONE));
	const float ground =
	    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(106.0f, 204.0f)) : 0.0f;
	EXPECT_EQ(Reg().Get<const Transform>(rock).position.y, ground);
	EXPECT_TRUE(map_cells::IsObjectInMap(rock));
	EXPECT_FALSE(Reg().AllOf<InPhysics>(rock));
	EXPECT_EQ(Locator::dynamicsSystem::value().Find(rock), nullptr);
	// not in the physics: a hand may pick it up
	EXPECT_TRUE(from_hand::TakeOutOfPhysicsForHand(rock));
}

TEST_F(ParticleCarriedObjectsTest, ACreedNeverBecomesAPhysicsObjectAndHangsWhereTheHandLetsItGo)
{
	// the creed's row and the eyes' row are of the creed's kind; the barrel's is a plain mobile object's
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	info->mobileObject.at(static_cast<size_t>(MobileObjectInfo::Creed)).mobileType = MobileObjectInfo::Creed;
	info->mobileObject.at(static_cast<size_t>(MobileObjectInfo::Eyes)).mobileType = MobileObjectInfo::Creed;
	info->mobileObject.at(static_cast<size_t>(MobileObjectInfo::EgyptBarrel)).mobileType = MobileObjectInfo::EgyptBarrel;
	const InfoConstants* rows = info.get();
	Locator::infoConstants::reset(info.release());
	const auto mobileObject = [](MobileObjectInfo row, const glm::vec3& at) {
		const auto e = Reg().Create();
		object_index::Assign(e);
		Reg().Assign<Transform>(e, at, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<MobileObject>(e, row);
		map_cells::InsertMapObject(e);
		return e;
	};
	const auto creed = mobileObject(MobileObjectInfo::Creed, glm::vec3(105.0f, 0.0f, 205.0f));
	const auto eyes = mobileObject(MobileObjectInfo::Eyes, glm::vec3(115.0f, 0.0f, 215.0f));
	const auto barrel = mobileObject(MobileObjectInfo::EgyptBarrel, glm::vec3(125.0f, 0.0f, 225.0f));

	// never a physics object, so its physics constants are row 0; a barrel is one, with row 1
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg(), rows));
	EXPECT_FALSE(dynamics.CanBecomeAPhysicsObject(creed));
	EXPECT_FALSE(dynamics.CanBecomeAPhysicsObject(eyes));
	EXPECT_TRUE(dynamics.CanBecomeAPhysicsObject(barrel));
	EXPECT_EQ(dynamics.ConstantsType(creed), 0);
	EXPECT_EQ(dynamics.ConstantsType(eyes), 0);
	EXPECT_EQ(dynamics.ConstantsType(barrel), 1);
	EXPECT_FALSE(dynamics.CanWakeKnockedProxy(creed));
	// a script's start of physics and any other start leave it out
	const ecs::PhysicsGround flatLand(nullptr);
	EXPECT_EQ(script_create::PutIntoPhysics(dynamics, creed, flatLand), nullptr);
	EXPECT_EQ(dynamics.AddObject(creed, glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(0.0f)), nullptr);
	EXPECT_EQ(dynamics.Find(creed), nullptr);
	EXPECT_FALSE(Reg().AllOf<InPhysics>(creed));

	// held up by the hand (out of the map, at the hand's point) and let go: it stays where it was let go, in the physics
	// with no entry, out of the map, as an immovable thing does; a barrel is not one that hangs
	EXPECT_FALSE(from_hand::HangsWhereLetGo(barrel));
	EXPECT_TRUE(from_hand::HangsWhereLetGo(creed));
	map_cells::RemoveMapObject(creed);
	const glm::vec3 hand {106.0f, 10.0f, 204.0f};
	Reg().Get<Transform>(creed).position = hand;
	EXPECT_FALSE(from_hand::Throw(creed, glm::vec3(3.0f, 0.0f, 0.0f), false, PlayerNames::PLAYER_ONE));
	EXPECT_EQ(Reg().Get<const Transform>(creed).position, hand);
	EXPECT_TRUE(Reg().AllOf<InPhysics>(creed));
	EXPECT_FALSE(map_cells::IsObjectInMap(creed));
	EXPECT_EQ(Locator::dynamicsSystem::value().Find(creed), nullptr);
}

TEST_F(ParticleCarriedObjectsTest, APotBecomesAPhysicsObjectOnlyWhenItsRowSaysSo)
{
	// the shipped rows: the pots, the hand's wood and food and the things carried in the hand say yes; the storage pit's
	// food pile, the wood piles, the food pile and the magic piles say no
	auto info = std::make_unique<InfoConstants>();
	for (const auto row : {PotInfo::FoodPot, PotInfo::WoodPot, PotInfo::HandWood, PotInfo::HandFood, PotInfo::TreeLogsInHand})
	{
		info->pot.at(static_cast<size_t>(row)).canBecomeAPhysicsObject = 1;
	}
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg(), info.get()));
	const auto pot = [](PotInfo row) {
		const auto e = Reg().Create();
		object_index::Assign(e);
		Reg().Assign<Transform>(e, glm::vec3(105.0f, 0.0f, 205.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<Pot>(e, Pot {.amount = 40, .maxAmount = 100, .type = row});
		return e;
	};
	for (const auto row : {PotInfo::FoodPot, PotInfo::WoodPot, PotInfo::HandWood, PotInfo::HandFood, PotInfo::TreeLogsInHand})
	{
		EXPECT_TRUE(dynamics.CanBecomeAPhysicsObject(pot(row))) << static_cast<int>(row);
	}
	for (const auto row : {PotInfo::StoragePitFoodPile, PotInfo::WoodPile_1, PotInfo::WoodPile_5, PotInfo::FoodPile,
	                       PotInfo::MagicWood, PotInfo::MagicFood})
	{
		const auto pile = pot(row);
		EXPECT_FALSE(dynamics.CanBecomeAPhysicsObject(pile)) << static_cast<int>(row);
		// its physics constants stay a pot's, whatever the row says
		EXPECT_EQ(dynamics.ConstantsType(pile), 4) << static_cast<int>(row);
		EXPECT_FALSE(dynamics.CanWakeKnockedProxy(pile)) << static_cast<int>(row);
	}

	// nothing puts a pile whose row says no into the physics: a script's start of physics or any other start
	const auto magicWood = pot(PotInfo::MagicWood);
	const ecs::PhysicsGround flatLand(nullptr);
	EXPECT_EQ(script_create::PutIntoPhysics(dynamics, magicWood, flatLand), nullptr);
	EXPECT_EQ(dynamics.AddObject(magicWood, glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(0.0f)), nullptr);
	EXPECT_EQ(dynamics.Find(magicWood), nullptr);
	EXPECT_FALSE(Reg().AllOf<InPhysics>(magicWood));

	// with no info table, or with no row, a pot can as before
	ecs::systems::DynamicsSystem noRows(std::make_unique<test::FakeDynamicsWorld>(Reg()));
	EXPECT_TRUE(noRows.CanBecomeAPhysicsObject(pot(PotInfo::MagicWood)));
	EXPECT_TRUE(dynamics.CanBecomeAPhysicsObject(pot(PotInfo::_COUNT)));
}

TEST_F(ParticleCarriedObjectsTest, ACreedIsNotInTheWayOfWhatFlies)
{
	// what flies goes through a creed (the creed's row and the eyes' row, of the creed's kind), and nothing is raised
	// clear of it; a barrel is in the way
	auto info = std::make_unique<InfoConstants>();
	info->mobileObject.at(static_cast<size_t>(MobileObjectInfo::Creed)).mobileType = MobileObjectInfo::Creed;
	info->mobileObject.at(static_cast<size_t>(MobileObjectInfo::Eyes)).mobileType = MobileObjectInfo::Creed;
	info->mobileObject.at(static_cast<size_t>(MobileObjectInfo::EgyptBarrel)).mobileType = MobileObjectInfo::EgyptBarrel;
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg(), info.get()));
	const auto mobileObject = [](MobileObjectInfo row) {
		const auto e = Reg().Create();
		Reg().Assign<Transform>(e, glm::vec3(105.0f, 0.0f, 205.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<MobileObject>(e, row);
		return e;
	};
	EXPECT_FALSE(dynamics.InteractsWithPhysicsObjects(mobileObject(MobileObjectInfo::Creed)));
	EXPECT_FALSE(dynamics.InteractsWithPhysicsObjects(mobileObject(MobileObjectInfo::Eyes)));
	EXPECT_TRUE(dynamics.InteractsWithPhysicsObjects(mobileObject(MobileObjectInfo::EgyptBarrel)));
}

TEST_F(ParticleCarriedObjectsTest, ABonfireNeverBecomesAPhysicsObjectNorIsInTheWayOfWhatFlies)
{
	// the bonfire's class says no to both, whatever would move it; another mobile static, a rock, says yes to both
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg()));
	const auto bonfire = Rock(glm::vec3(105.0f, 0.0f, 205.0f));
	Reg().Get<MobileStatic>(bonfire).type = MobileStaticInfo::Bonfire;
	const auto rock = Rock(glm::vec3(125.0f, 0.0f, 225.0f));
	Reg().Get<MobileStatic>(rock).type = MobileStaticInfo::Rock;
	EXPECT_FALSE(dynamics.CanBecomeAPhysicsObject(bonfire));
	EXPECT_FALSE(dynamics.InteractsWithPhysicsObjects(bonfire));
	EXPECT_FALSE(dynamics.CanWakeKnockedProxy(bonfire));
	EXPECT_TRUE(dynamics.CanBecomeAPhysicsObject(rock));
	EXPECT_TRUE(dynamics.InteractsWithPhysicsObjects(rock));
	// a script's start of physics and any other start leave it out
	const ecs::PhysicsGround flatLand(nullptr);
	EXPECT_EQ(script_create::PutIntoPhysics(dynamics, bonfire, flatLand), nullptr);
	EXPECT_EQ(dynamics.AddObject(bonfire, glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(0.0f)), nullptr);
	EXPECT_EQ(dynamics.Find(bonfire), nullptr);
	EXPECT_FALSE(Reg().AllOf<InPhysics>(bonfire));
	// it answers that it is a rock, so its physics constants are the rock's row, 3
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
	EXPECT_EQ(dynamics.ConstantsType(bonfire), 3);
}

TEST_F(ParticleCarriedObjectsTest, AMobileStaticIsInTheWayOfWhatFliesOnlyByItsKind)
{
	// a toy, a fence, a gate totem, a weeping stone, the first singing stone and a static of a rock's or an idol's kind
	// are in the way of what flies whatever their life; any other answers as a building does, and a static is never
	// built, so a lantern, an altar, a creature cage or a sculpture in progress is not in the way, whatever its life.
	// The singing stone's base is a plain object, in the way with its model. The rows' models and kinds are the game's
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	const auto setRow = [&info](MobileStaticInfo row, int mesh, MobileStaticInfo kind) {
		auto& r = info->mobileStatic.at(static_cast<size_t>(row));
		r.meshId = static_cast<MeshId>(mesh);
		r.mobileType = kind;
	};
	setRow(MobileStaticInfo::CeltFenceShort, 81, MobileStaticInfo::CeltFenceShort);
	setRow(MobileStaticInfo::SingingStone_1, 410, MobileStaticInfo::SingingStone_1);
	setRow(MobileStaticInfo::SingingStoneBase, 409, MobileStaticInfo::SingingStoneBase);
	setRow(MobileStaticInfo::StreetLantern, 409, MobileStaticInfo::SingingStoneBase);
	setRow(MobileStaticInfo::CreatureCage, 0, MobileStaticInfo::CreatureCage);
	setRow(MobileStaticInfo::IdolReward, 277, MobileStaticInfo::Idol);
	setRow(MobileStaticInfo::WeepingStoneReward, 68, MobileStaticInfo::WeepingStone);
	setRow(MobileStaticInfo::Boulder1Chalk, 253, MobileStaticInfo::Rock);
	setRow(MobileStaticInfo::StandaloneAltar, 359, MobileStaticInfo::StandaloneAltar);
	setRow(MobileStaticInfo::GateTotemBlank, 363, MobileStaticInfo::GateTotemBlank);
	setRow(MobileStaticInfo::ToyBowlingBall, 400, MobileStaticInfo::ToyBall);
	setRow(MobileStaticInfo::SculptureInProgress, 408, MobileStaticInfo::SculptureInProgress);
	setRow(MobileStaticInfo::CountryLantern, 409, MobileStaticInfo::SingingStoneBase);
	const InfoConstants* rows = info.get();
	Locator::infoConstants::reset(info.release());
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg(), rows));
	const auto statics = [rows](MobileStaticInfo row, float life) {
		const auto e = Reg().Create();
		Reg().Assign<Transform>(e, glm::vec3(105.0f, 0.0f, 205.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<MobileStatic>(e, row);
		Reg().Assign<Mesh>(e, resources::HashIdentifier(rows->mobileStatic.at(static_cast<size_t>(row)).meshId),
		                   static_cast<int8_t>(0), static_cast<int8_t>(0));
		Reg().Assign<Life>(e, life);
		return e;
	};
	for (const auto row :
	     {MobileStaticInfo::CeltFenceShort, MobileStaticInfo::SingingStone_1, MobileStaticInfo::IdolReward,
	      MobileStaticInfo::WeepingStoneReward, MobileStaticInfo::Boulder1Chalk, MobileStaticInfo::GateTotemBlank,
	      MobileStaticInfo::ToyBowlingBall, MobileStaticInfo::SingingStoneBase})
	{
		EXPECT_TRUE(dynamics.InteractsWithPhysicsObjects(statics(row, 1.0f))) << static_cast<int>(row);
		EXPECT_TRUE(dynamics.InteractsWithPhysicsObjects(statics(row, 0.005f))) << static_cast<int>(row);
	}
	for (const auto row : {MobileStaticInfo::StreetLantern, MobileStaticInfo::CountryLantern, MobileStaticInfo::CreatureCage,
	                       MobileStaticInfo::StandaloneAltar, MobileStaticInfo::SculptureInProgress})
	{
		EXPECT_FALSE(dynamics.InteractsWithPhysicsObjects(statics(row, 1.0f))) << static_cast<int>(row);
		EXPECT_FALSE(dynamics.InteractsWithPhysicsObjects(statics(row, 0.005f))) << static_cast<int>(row);
	}
}

TEST_F(ParticleCarriedObjectsTest, AMovingBodyLeavesThePhysicsForTheHand)
{
	// an object with a moving body is in the physics with an entry there to take out: it may go into the hand
	const auto rock = Rock(glm::vec3(105.0f, 0.0f, 205.0f));
	auto& objects = Locator::dynamicsSystem::value().GetState().objects;
	auto body = std::make_unique<ecs::PhysicsEntry>();
	body->entity = rock;
	body->body.resting = false;
	objects.push_back(std::move(body));
	Reg().AssignOrReplaceState<InPhysics>(rock);
	EXPECT_TRUE(from_hand::TakeOutOfPhysicsForHand(rock));
	objects.pop_back();
}

TEST(ParticleCarriedObjects, FlingMotion)
{
	// the vortex's fling with fixed draws: v = (sin a B, C, -cos a B), w = ((0 - v.z) / h, 0, (v.x - 0) / h)
	// (v.z first, then v.x): the spin is (cos a B / h, 0, sin a B / h), NOT -C / h
	constexpr float k_H = 0.8f;
	constexpr float k_Angle = 0.6f;
	constexpr float k_Speed = 1.0f;
	const auto m = particle_carried_objects::FlingMotionOf(k_H, k_Angle, k_Speed, 2.5f, 1.5f);
	const float b = (2.5f + 8.0f) * k_Speed;
	const float c = (1.5f + 10.0f) * k_Speed;
	EXPECT_NEAR(m.velocity.x, std::sin(k_Angle) * b, 1e-4f);
	EXPECT_EQ(m.velocity.y, c);
	EXPECT_NEAR(m.velocity.z, -std::cos(k_Angle) * b, 1e-4f);
	EXPECT_NEAR(m.spin.x, std::cos(k_Angle) * b / k_H, 1e-4f);
	EXPECT_EQ(m.spin.y, 0.0f);
	EXPECT_NEAR(m.spin.z, std::sin(k_Angle) * b / k_H, 1e-4f);
}

TEST(PhysicsWeight, AFragmentWeighsAsItsRockButItsBodyByItsArea)
{
	// As an object a fragment weighs scale^3 x the weight of the rock it was made as; its body is set up with 30 x its
	// area, never below 0.01
	ecs::Registry registry;
	auto info = std::make_unique<InfoConstants>();
	info->mobileStatic.at(2).weight = 55.0f;
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(registry, info.get()));
	const auto piece = registry.Create();
	registry.Assign<ecs::components::Transform>(piece, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(2.0f));
	registry.Assign<ecs::components::Fragment>(piece).area = 2.0f;
	EXPECT_FLOAT_EQ(dynamics.WeightOf(piece), 8.0f * 55.0f);
	EXPECT_FLOAT_EQ(dynamics.BodyMassOf(piece), 60.0f);
	registry.Get<ecs::components::Fragment>(piece).area = 0.0f;
	EXPECT_FLOAT_EQ(dynamics.BodyMassOf(piece), 0.01f);
}

namespace
{
/// The five toys' rows of the mobile static table and their models, as the game's data gives them
constexpr std::array<std::pair<MobileStaticInfo, MeshId>, 5> k_Toys {{
    {MobileStaticInfo::ToyBall, MeshId::ObjectToyBall},
    {MobileStaticInfo::ToyCuddly, MeshId::ObjectToyCuddly},
    {MobileStaticInfo::ToyDie, MeshId::ObjectToyDice},
    {MobileStaticInfo::ToySkittle, MeshId::ObjectToySkittle},
    {MobileStaticInfo::ToyBowlingBall, MeshId::ObjectToyBowlingBall},
}};
} // namespace

TEST_F(ParticleCarriedObjectsTest, OfTheToysOnlyTheBowlingBallBreaksBuildings)
{
	// each toy takes the physics constants of its model: the ball 14, the cuddly toy 16, the die 15, the skittle 19,
	// the bowling ball 20. Only rows 3 and 20 break buildings, so of the toys only the bowling ball does, as a rock does
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	for (const auto& toy : k_Toys)
	{
		info->mobileStatic.at(static_cast<size_t>(toy.first)).mobileType = MobileStaticInfo::ToyBall;
	}
	info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Rock)).mobileType = MobileStaticInfo::Rock;
	const InfoConstants* rows = info.get();
	Locator::infoConstants::reset(info.release());
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg(), rows));
	const auto statics = [](MobileStaticInfo row, MeshId mesh) {
		const auto e = Reg().Create();
		Reg().Assign<Transform>(e, glm::vec3(105.0f, 0.0f, 205.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<MobileStatic>(e, row);
		Reg().Assign<Mesh>(e, resources::HashIdentifier(mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
		return e;
	};
	constexpr std::array<int, 5> k_ToyConstants {14, 16, 15, 19, 20};
	for (size_t i = 0; i < k_Toys.size(); ++i)
	{
		const auto toy = statics(k_Toys.at(i).first, k_Toys.at(i).second);
		EXPECT_EQ(dynamics.ConstantsType(toy), k_ToyConstants.at(i)) << i;
		EXPECT_EQ(dynamics.PhysicallyDestroysAbodes(toy), k_Toys.at(i).first == MobileStaticInfo::ToyBowlingBall) << i;
	}
	// a rock: row 3, and it breaks buildings
	const auto rock = statics(MobileStaticInfo::Rock, MeshId::ObjectToyBall);
	EXPECT_EQ(dynamics.ConstantsType(rock), 3);
	EXPECT_TRUE(dynamics.PhysicallyDestroysAbodes(rock));
}

TEST_F(ParticleCarriedObjectsTest, AToysKnockIsTheSoundKindOfItsRow)
{
	// what a toy sounds like when it strikes something is the collision sound kind of its own row of the mobile static
	// table
	auto info = std::make_unique<InfoConstants>();
	constexpr std::array k_Kinds {SoundCollisionType::SolidStone, SoundCollisionType::Fungus, SoundCollisionType::HollowWood,
	                              SoundCollisionType::FlintStone, SoundCollisionType::Pottery};
	for (size_t i = 0; i < k_Toys.size(); ++i)
	{
		info->mobileStatic.at(static_cast<size_t>(k_Toys.at(i).first)).collideSound = k_Kinds.at(i);
	}
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg(), info.get()));
	for (size_t i = 0; i < k_Toys.size(); ++i)
	{
		const auto toy = Reg().Create();
		Reg().Assign<MobileStatic>(toy, k_Toys.at(i).first);
		const auto* row = dynamics.ObjectInfo(toy);
		ASSERT_EQ(row, &info->mobileStatic.at(static_cast<size_t>(k_Toys.at(i).first))) << i;
		EXPECT_EQ(row->collideSound, k_Kinds.at(i)) << i;
	}
}

TEST_F(ParticleCarriedObjectsTest, AShieldSoundsWhenStruckAsItsOwnShieldRowSays)
{
	// what strikes a shield hears the collision sound kind of the shield's row: the physical shield's second, the
	// spiritual shield's first
	auto info = std::make_unique<InfoConstants>();
	info->mapShield.at(0).collideSound = SoundCollisionType::Pottery;
	info->mapShield.at(1).collideSound = SoundCollisionType::SolidStone;
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg(), info.get()));
	const auto shield = [this](MagicShield::Kind kind) {
		const auto entity = Reg().Create();
		Reg().Assign<MagicShield>(entity).kind = kind;
		return entity;
	};
	const auto* physical = dynamics.ObjectInfo(shield(MagicShield::Kind::Physical));
	ASSERT_EQ(physical, &info->mapShield.at(1));
	EXPECT_EQ(physical->collideSound, SoundCollisionType::SolidStone);
	const auto* spiritual = dynamics.ObjectInfo(shield(MagicShield::Kind::Spiritual));
	ASSERT_EQ(spiritual, &info->mapShield.at(0));
	EXPECT_EQ(spiritual->collideSound, SoundCollisionType::Pottery);
}

/// With OPENBLACK_GAME_PATH set to the install: the five toys' rows of the mobile static table. The ball and the
/// bowling ball knock as solid stone, the die and the skittle as hollow wood, the cuddly toy as the mushrooms do; no
/// toy is of the rock kind (so the hand's size limit for rocks never applies to one), and fire never hurts one
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it. Its synthetic twin is
// ParticleCarriedObjectsTest.AToysKnockIsTheSoundKindOfItsRow
TEST(ToyInfo, TheToysRowsInTheGamesInfoDat)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	struct Row
	{
		SoundCollisionType sound;
		float weight;
	};
	constexpr std::array<Row, 5> k_Rows {{
	    {.sound = SoundCollisionType::SolidStone, .weight = 100.0f},  // the ball
	    {.sound = SoundCollisionType::Fungus, .weight = 1000.0f},     // the cuddly toy
	    {.sound = SoundCollisionType::HollowWood, .weight = 1000.0f}, // the die
	    {.sound = SoundCollisionType::HollowWood, .weight = 250.0f},  // the skittle
	    {.sound = SoundCollisionType::SolidStone, .weight = 2000.0f}, // the bowling ball
	}};
	for (size_t i = 0; i < k_Toys.size(); ++i)
	{
		const auto& row = info->mobileStatic.at(static_cast<size_t>(k_Toys.at(i).first));
		EXPECT_EQ(row.meshId, k_Toys.at(i).second) << i;
		EXPECT_EQ(row.collideSound, k_Rows.at(i).sound) << i;
		EXPECT_FLOAT_EQ(row.weight, k_Rows.at(i).weight) << i;
		EXPECT_EQ(row.mobileType, MobileStaticInfo::ToyBall) << i;
		EXPECT_FLOAT_EQ(row.defenceMultiplierBurn, 0.0f) << i;
		EXPECT_FLOAT_EQ(row.heatCapacity, 5000.0f) << i;
		EXPECT_FLOAT_EQ(row.combustionTemperature, 1000.0f) << i;
	}
	// the magic mushroom knocks with the same soft sound as the cuddly toy
	EXPECT_EQ(info->mobileObject.at(static_cast<size_t>(MobileObjectInfo::MagicMushroom)).collideSound,
	          SoundCollisionType::Fungus);
}

TEST_F(ParticleCarriedObjectsTest, AThingAtRestOffTheMapIsDeletedAndOnItGoesBackInTheCells)
{
	// at the end of its physics an available object goes back in the map cells when its cell is on the 512 x 512 map,
	// and is deleted when it is not; one already marked for deletion is neither
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg()));
	const auto thing = [](const glm::vec3& at) {
		const auto e = Reg().Create();
		object_index::Assign(e);
		Reg().Assign<Transform>(e, at, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<MobileStatic>(e);
		return e;
	};
	const auto onTheMap = thing(glm::vec3(105.0f, 0.0f, 205.0f));
	EXPECT_TRUE(dynamics.EndPhysicsAsObject(onTheMap));
	EXPECT_TRUE(map_cells::IsObjectInMap(onTheMap));
	EXPECT_TRUE(ecs::IsAvailable(onTheMap));
	// 5120 is the first metre of cell 512, and a little below 0 is in cell -1
	for (const auto& at : {glm::vec3(5120.0f, 0.0f, 100.0f), glm::vec3(-0.01f, 0.0f, 100.0f), glm::vec3(100.0f, 0.0f, 5120.0f)})
	{
		const auto off = thing(at);
		EXPECT_FALSE(dynamics.EndPhysicsAsObject(off)) << at.x << ", " << at.z;
		EXPECT_FALSE(ecs::IsAvailable(off)) << at.x << ", " << at.z;
	}
	const auto marked = thing(glm::vec3(115.0f, 0.0f, 215.0f));
	Reg().Assign<Unavailable>(marked);
	EXPECT_FALSE(dynamics.EndPhysicsAsObject(marked));
	EXPECT_TRUE(Reg().Valid(marked));
	EXPECT_FALSE(map_cells::IsObjectInMap(marked));
}

TEST_F(ParticleCarriedObjectsTest, ABuildingWeighsItsScaleCubedTimesItsAbodesWeight)
{
	// as an object (for a push, a catch or a lift) a building weighs scale^3 x the weight of its abode's row, a storage
	// pit too; its body is always set up with 2000
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	info->abode.at(static_cast<size_t>(AbodeInfo::CelticHut)).weight = 2000.0f;
	info->abode.at(static_cast<size_t>(AbodeInfo::CelticTent)).weight = 300.0f;
	const InfoConstants* rows = info.get();
	Locator::infoConstants::reset(info.release());
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg(), rows));
	dynamics.SetClassHooks(std::make_unique<ecs::PhysicsGameHooks>());
	const auto building = [](AbodeInfo row, float scale) {
		const auto e = Reg().Create();
		Reg().Assign<Transform>(e, glm::vec3(105.0f, 0.0f, 205.0f), glm::mat3(1.0f), glm::vec3(scale));
		Reg().Assign<Abode>(e).info = row;
		return e;
	};
	const auto hut = building(AbodeInfo::CelticHut, 1.5f);
	EXPECT_FLOAT_EQ(dynamics.WeightOf(hut), 6750.0f);
	EXPECT_FLOAT_EQ(dynamics.BodyMassOf(hut), 2000.0f);
	const auto pit = building(AbodeInfo::CelticTent, 2.0f);
	Reg().Assign<StoragePit>(pit);
	EXPECT_FLOAT_EQ(dynamics.WeightOf(pit), 2400.0f);
	EXPECT_FLOAT_EQ(dynamics.BodyMassOf(pit), 2000.0f);
}

namespace
{
/// An upright box body with half sizes `half`, centred at `centre`: its top and its bottom, the faces a vertical push
/// meets, with outward normals
std::unique_ptr<ecs::PhysicsEntry> Box(entt::entity entity, glm::vec3 half, glm::vec3 centre)
{
	std::array<glm::vec3, 8> local {};
	for (uint32_t i = 0; i < local.size(); ++i)
	{
		local.at(i) = {(i & 1u) != 0 ? half.x : -half.x, (i & 2u) != 0 ? half.y : -half.y, (i & 4u) != 0 ? half.z : -half.z};
	}
	constexpr std::array<std::array<uint32_t, 3>, 4> k_Faces {{{2, 6, 3}, {3, 6, 7}, {0, 1, 4}, {1, 5, 4}}};
	constexpr openblack::physics::PhysicsData k_Data = {0.8f, 300.0f, 30.0f, 0.5f, 0.3f, 0.1f};
	auto entry = std::make_unique<ecs::PhysicsEntry>();
	entry->entity = entity;
	entry->body.Initialise(1.0f, half.y);
	entry->body.SetUpConstants(10.0f, k_Data, true);
	entry->body.BuildShape(local, k_Faces, glm::vec3(0.0f), glm::length(half), 1.0f, glm::mat3(1.0f), centre);
	return entry;
}
} // namespace

TEST_F(ParticleCarriedObjectsTest, ARaisedBodyRisesByTheDeepestPushUntilNothingPushesIt)
{
	// let go of where it sinks into the bodies under it, a body goes up by how deep its vertices are inside another's
	// top, again until none is: here it ends standing on the higher of the two boxes it sank into, 0.5 up. The turn's
	// start pose follows it
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(Reg()));
	auto& objects = dynamics.GetState().objects;
	// two wide boxes, their tops at 1 and at 1.25
	objects.push_back(Box(Reg().Create(), glm::vec3(1.0f, 0.5f, 1.0f), glm::vec3(105.0f, 0.5f, 205.0f)));
	objects.push_back(Box(Reg().Create(), glm::vec3(1.0f, 0.5f, 1.0f), glm::vec3(105.0f, 0.75f, 205.0f)));
	// a small box whose bottom is at 0.75
	objects.push_back(Box(Reg().Create(), glm::vec3(0.5f), glm::vec3(105.0f, 1.25f, 205.0f)));
	auto& raised = *objects.back();
	dynamics.RaiseClearOfWhatIsUnder(raised);
	EXPECT_FLOAT_EQ(raised.body.Centre().y, 1.75f);
	EXPECT_FLOAT_EQ(raised.body.Centre().x, 105.0f);
	EXPECT_FLOAT_EQ(raised.body.Centre().z, 205.0f);
	EXPECT_EQ(raised.turnStartCentre, raised.body.Centre());
	// what is under it stays where it is
	EXPECT_FLOAT_EQ(objects.at(0)->body.Centre().y, 0.5f);
	EXPECT_FLOAT_EQ(objects.at(1)->body.Centre().y, 0.75f);

	// a villager is not raised over a body a Living pushed
	objects.clear();
	objects.push_back(Box(Reg().Create(), glm::vec3(1.0f, 0.5f, 1.0f), glm::vec3(105.0f, 0.5f, 205.0f)));
	objects.back()->flags |= ecs::PhysicsEntry::k_PushedByLiving;
	objects.push_back(Box(Reg().Create(), glm::vec3(0.5f), glm::vec3(105.0f, 1.25f, 205.0f)));
	objects.back()->villager = true;
	auto& villager = *objects.back();
	dynamics.RaiseClearOfWhatIsUnder(villager);
	EXPECT_FLOAT_EQ(villager.body.Centre().y, 1.25f);
}
