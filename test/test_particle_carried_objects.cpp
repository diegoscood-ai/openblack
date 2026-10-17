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
// pick of it puts it back in the map.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

#include <glm/gtc/constants.hpp>
#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
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
#include "ECS/PhysicsEntry.h"
#include "ECS/PhysicsGameHooks.h"
#include "ECS/PhysicsHooksWorld.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/Systems/Implementations/HandSystemDetail.h"
#include "ECS/Systems/Implementations/TornadoSystem.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Physics/LivingRules.h"
#include "creature/CreatureSystemFakes.h"
#include "support/DynamicsFakes.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace particle_carried_objects = openblack::ecs::physics::particle_carried_objects;
namespace from_hand = openblack::ecs::physics::from_hand;

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
	EXPECT_EQ(dynamics.PushObject(rock, entt::null), 0.0f);
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
	const auto land = [&](entt::entity log, bool byPlayer) {
		ecs::PhysicsEntry entry;
		entry.entity = log;
		entry.byPlayer = byPlayer;
		EXPECT_TRUE(game.CallsBackInMap(dynamics, log));
		const auto kept = game.EndPhysics(dynamics, entry);
		ASSERT_TRUE(kept.has_value());
		EXPECT_EQ(*kept, log);
		EXPECT_TRUE(map_cells::IsObjectInMap(log));
	};
	const auto playerOf = [](entt::entity log) -> std::optional<PlayerNames> {
		const auto* reaction = reactions::Find(reactions::GetReactionOfTypeInitiatedBy(log, Reaction::ReactToWood));
		return reaction != nullptr ? std::optional(reaction->player) : std::nullopt;
	};

	const auto thrown = makeLog(glm::vec3(105.0f, 0.0f, 205.0f));
	ASSERT_EQ(dynamics.ClassOf(thrown), PhysicsClass::DeadTree);
	land(thrown, true);
	EXPECT_EQ(playerOf(thrown), std::optional(PlayerNames::PLAYER_ONE));

	const auto knocked = makeLog(glm::vec3(115.0f, 0.0f, 215.0f));
	land(knocked, false);
	EXPECT_EQ(playerOf(knocked), std::optional(PlayerNames::NEUTRAL));

	const auto felled = makeLog(glm::vec3(125.0f, 0.0f, 225.0f));
	Reg().Assign<FelledTree>(felled);
	land(felled, true);
	EXPECT_FALSE(playerOf(felled).has_value());
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
	for (const bool byPlayer : {true, false})
	{
		ecs::PhysicsEntry entry;
		entry.entity = pile;
		entry.byPlayer = byPlayer;
		const auto kept = game.EndPhysics(dynamics, entry);
		ASSERT_TRUE(kept.has_value());
		EXPECT_EQ(*kept, pile);
	}
	ASSERT_EQ(hand.potEnds.size(), 2u);
	EXPECT_EQ(hand.potEnds[0].first, pile);
	EXPECT_TRUE(hand.potEnds[0].second);
	EXPECT_EQ(hand.potEnds[1].first, pile);
	EXPECT_FALSE(hand.potEnds[1].second);
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
	EXPECT_FALSE(from_hand::Throw(rock, glm::vec3(0.0f), false));
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
	EXPECT_TRUE(from_hand::Throw(rock, glm::vec3(0.0f), false));
	const float ground =
	    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(106.0f, 204.0f)) : 0.0f;
	EXPECT_EQ(Reg().Get<const Transform>(rock).position.y, ground);
	EXPECT_TRUE(map_cells::IsObjectInMap(rock));
	EXPECT_FALSE(Reg().AllOf<InPhysics>(rock));
	EXPECT_EQ(Locator::dynamicsSystem::value().Find(rock), nullptr);
	// not in the physics: a hand may pick it up
	EXPECT_TRUE(from_hand::TakeOutOfPhysicsForHand(rock));
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
