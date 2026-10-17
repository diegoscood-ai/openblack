/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature object-action system with fake animations and locomotion: what can be eaten, picked up and knocked
// down, the actions playing on by the turn while the frame only draws, a villager eaten going out as any death does,
// the moments things are taken hold of and let go of drawn at once where they are, and what is held drawn in the hand
// where the body is drawn between turns

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>

#include <glm/gtx/vec_swizzle.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureEating.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureThrow.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreaturePose.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectFlags.h"
#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/ToBeDeleted.h"
#include "creature/CreatureSystemFakes.h"
#include "creature/CreatureSystemWorld.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureObjectActionSystem;
namespace fakes = openblack::test::creature_fakes;
using creature_object_actions::Kind;
using creature_object_actions::Status;
using Phase = CreatureObjectAction::Phase;

namespace
{
class CreatureObjectActionSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		_animation = &static_cast<fakes::FakeAnimation&>(Locator::creatureAnimationSystem::emplace<fakes::FakeAnimation>());
		_animation->bone = glm::vec3(0.0f, 10.0f, 0.0f);
		Locator::creatureLocomotionSystem::emplace<fakes::FakeLocomotion>();
		auto& info = _world.Info();
		info.villager.at(0).foodValue = 5.0f;
		info.villager.at(0).weight = 2.0f;
		info.mobileObject.at(static_cast<size_t>(MobileObjectInfo::LumpOfPoo)).foodValue = 0.0f;
		info.mobileObject.at(static_cast<size_t>(MobileObjectInfo::Ball)).weight = 1.0f;
	}
	void TearDown() override { test::ResetMapAndVillagerDefaults(); }

	static entt::entity Thing(glm::vec3 position)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}
	static entt::entity Rock(glm::vec3 position)
	{
		const auto entity = Thing(position);
		test::creature_world::World::Registry().Assign<MobileObject>(entity, MobileObjectInfo::Ball);
		return entity;
	}
	/// A Giant Ape with its rig in the cache, so that it has hands to act with
	entt::entity Ape()
	{
		test::creature_block::Block block;
		block.clips = {test::creature_block::Clip {}};
		_world.LoadApeRig(block, {{"move", {"Cstand"}}});
		return test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f));
	}
	/// An action at its moment, playing one animation
	static CreatureObjectAction AtItsMoment(Kind kind, std::optional<entt::entity> target)
	{
		CreatureObjectAction action {.kind = kind, .phase = Phase::Playing, .status = Status::Running, .target = target};
		action.animations.front() = creature_throw::k_Eat;
		action.weights.front() = 1.0f;
		action.animationCount = 1;
		action.durationMs = 1000.0f;
		return action;
	}
	static void Hold(entt::entity creature, entt::entity object)
	{
		auto& registry = test::creature_world::World::Registry();
		registry.AssignState<CreatureHeldObject>(creature, CreatureHeldObject {.object = object});
		registry.AssignState<HeldByCreature>(object, HeldByCreature {.creature = creature});
	}

	const test::ScopedDefaultFileSystem _fileSystem;
	// the world lists a creature eating or knocking a thing down reaches (the dead list, the fires...), reset after the
	// registry as the game's shutdown does
	const test::ScopedWorldSystems _worldSystems;
	test::creature_world::World _world;
	const test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	const test::RestoreService<Locator::creatureAnimationSystem> _restoreAnimation;
	const test::RestoreService<Locator::creatureLocomotionSystem> _restoreLocomotion;
	fakes::FakeAnimation* _animation {nullptr};
	CreatureObjectActionSystem _system;
};
} // namespace

TEST_F(CreatureObjectActionSystemTest, WhatCanBeEatenPickedUpAndKnockedDown)
{
	auto& registry = test::creature_world::World::Registry();
	const auto villager = Thing(glm::vec3(0.0f));
	registry.Assign<Villager>(villager);
	const auto rock = Rock(glm::vec3(0.0f));
	const auto tree = Thing(glm::vec3(0.0f));
	registry.Assign<Tree>(tree);
	const auto creature = test::creature_world::World::MakeCreature();

	EXPECT_EQ(_system.FoodValueOf(villager), std::optional(5.0f));
	EXPECT_FALSE(_system.FoodValueOf(rock).has_value());
	EXPECT_FALSE(_system.FoodValueOf(creature).has_value());
	// a villager and a mobile object go by the creature's reach, which a creature without its species' meshes has none of
	EXPECT_FALSE(_system.CanPickUp(creature, villager));
	EXPECT_FALSE(_system.CanPickUp(creature, rock));
	EXPECT_FALSE(_system.CanPickUp(creature, tree));
	EXPECT_FALSE(_system.CanPickUp(creature, creature));
	EXPECT_TRUE(_system.CanDestroy(tree));
	EXPECT_TRUE(_system.CanDestroy(rock));
	EXPECT_FALSE(_system.CanDestroy(villager));
	// something going is none of these
	ecs::ToBeDeleted(rock);
	EXPECT_FALSE(_system.CanPickUp(creature, rock));
	EXPECT_FALSE(_system.CanDestroy(rock));
}

TEST_F(CreatureObjectActionSystemTest, APotGoesByTheThingsTestAndAPileByItsOwn)
{
	// A pot by the test of things in general (no reach without the species' meshes: refused); a pile by its flag and
	// its row alone: a food pile and magic food never (their rows may not move), food in the hand's pile when its row
	// may, unless it cannot be picked up; on the map or off it alike
	auto& info = _world.Info();
	const auto row = [&info](PotInfo kind, PotType type, uint32_t physics) {
		auto& r = info.pot.at(static_cast<size_t>(kind));
		r.potType = type;
		r.canBecomeAPhysicsObject = physics;
		r.weight = 20.0f;
	};
	row(PotInfo::FoodPot, PotType::Pot, 1);
	row(PotInfo::FoodPile, PotType::PileFood, 0);
	row(PotInfo::MagicFood, PotType::PileFood, 0);
	row(PotInfo::HandFood, PotType::PileFood, 1);
	auto& registry = test::creature_world::World::Registry();
	const auto pot = [&registry](PotInfo kind, bool onMap) {
		const auto entity = Thing(glm::vec3(10.0f, 0.0f, 10.0f));
		registry.Assign<Pot>(entity, Pot {.amount = 500, .maxAmount = 1000, .type = kind});
		if (onMap)
		{
			ecs::map_cells::InsertMapObject(entity);
		}
		return entity;
	};
	const auto creature = test::creature_world::World::MakeCreature();
	EXPECT_FALSE(_system.CanPickUp(creature, pot(PotInfo::FoodPot, true)));
	EXPECT_FALSE(_system.CanPickUp(creature, pot(PotInfo::FoodPile, true)));
	EXPECT_FALSE(_system.CanPickUp(creature, pot(PotInfo::MagicFood, true)));
	const auto handFood = pot(PotInfo::HandFood, true);
	EXPECT_TRUE(_system.CanPickUp(creature, handFood));
	ecs::object_flags::SetPickupable(handFood, false);
	EXPECT_FALSE(_system.CanPickUp(creature, handFood));
	EXPECT_TRUE(_system.CanPickUp(creature, pot(PotInfo::HandFood, false)));
}

TEST_F(CreatureObjectActionSystemTest, TheTurnPlaysTheActionAndTheFrameOnlyDraws)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	auto action = AtItsMoment(Kind::Keep, std::nullopt);
	action.eventDone = true;
	action.durationMs = 5000.0f;
	registry.AssignState<CreatureObjectAction>(creature, action);
	_animation->durations[creature_throw::k_Eat] = 5000.0f;
	const auto rate = creature_layers::PlaybackRate(registry.Get<Creature>(creature).size);

	_system.ProcessTurn();
	const auto& played = registry.Get<CreatureObjectAction>(creature);
	EXPECT_FLOAT_EQ(played.timeMs, 100.0f * rate);
	EXPECT_EQ(played.status, Status::Running);

	// a frame half way through the turn draws the animation half a turn on, and moves nothing
	const auto before = played;
	_system.UpdateDraw(0.5f);
	const auto& after = registry.Get<CreatureObjectAction>(creature);
	EXPECT_FLOAT_EQ(after.timeMs, before.timeMs);
	EXPECT_EQ(after.status, before.status);
	EXPECT_EQ(after.eventDone, before.eventDone);
	const auto& slots = registry.Get<CreatureAnimation>(creature).slots;
	ASSERT_EQ(slots.size(), 1u);
	EXPECT_FLOAT_EQ(slots.front().timeMs, before.timeMs + (0.5f * 100.0f * rate));
}

TEST_F(CreatureObjectActionSystemTest, TheAnimationsAtAShareOfTheTurnAreWhatTheFrameDrawsThen)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	auto action = AtItsMoment(Kind::Keep, std::nullopt);
	action.eventDone = true;
	action.durationMs = 5000.0f;
	action.mirrored = true;
	registry.AssignState<CreatureObjectAction>(creature, action);
	_animation->durations[creature_throw::k_Eat] = 5000.0f;
	_system.ProcessTurn();
	const auto& lookup = std::as_const(registry);
	const auto& animation = lookup.Get<const CreatureAnimation>(creature);
	const auto size = ecs::creature_pose::DrawnSize(lookup, creature);
	const auto rate = creature_layers::PlaybackRate(size);

	for (const auto share : {0.5f, 1.0f})
	{
		_system.UpdateDraw(share);
		// the frame's slots, the query's and the pure part's are one, and the body's own action is left alone
		CreatureAnimationInputs inputs {.body = {.timeMs = 7.0f}, .slots = {}};
		_system.AnimationAt(creature, share, inputs);
		const auto& playing = lookup.Get<const CreatureObjectAction>(creature);
		const auto pure = CreatureObjectActionSystem::SlotsAt(playing, size, {5000.0f, 0.0f, 0.0f, 0.0f}, share);
		ASSERT_EQ(animation.slots.size(), 1u);
		ASSERT_EQ(inputs.slots.size(), 1u);
		ASSERT_EQ(pure.size(), 1u);
		EXPECT_EQ(animation.slots.front().timeMs, playing.timeMs + (share * 100.0f * rate));
		EXPECT_EQ(inputs.slots.front().timeMs, animation.slots.front().timeMs);
		EXPECT_EQ(pure.front().timeMs, animation.slots.front().timeMs);
		EXPECT_EQ(inputs.slots.front().animation, creature_throw::k_Eat);
		EXPECT_EQ(inputs.slots.front().weight, 1.0f);
		EXPECT_TRUE(inputs.slots.front().mirrored);
		EXPECT_EQ(inputs.body.timeMs, 7.0f);
	}
	// past the turn's end it is drawn at the end
	CreatureAnimationInputs past;
	_system.AnimationAt(creature, 1.5f, past);
	ASSERT_EQ(past.slots.size(), 1u);
	EXPECT_EQ(past.slots.front().timeMs, animation.slots.front().timeMs);
	// an action not playing its animations is not drawn, and gets nothing
	auto approaching = lookup.Get<const CreatureObjectAction>(creature);
	approaching.phase = Phase::Approach;
	EXPECT_FALSE(CreatureObjectActionSystem::IsDrawn(approaching));
	registry.Get<CreatureObjectAction>(creature).phase = Phase::Approach;
	CreatureAnimationInputs none;
	_system.AnimationAt(creature, 1.0f, none);
	EXPECT_TRUE(none.slots.empty());
}

TEST_F(CreatureObjectActionSystemTest, InItsPenTheActionPlaysAtThePenSizesRate)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<Creature>(creature).size = 2.0f;
	registry.Get<CreatureDrawPose>(creature).size = 0.22f;
	auto action = AtItsMoment(Kind::Keep, std::nullopt);
	action.eventDone = true;
	action.durationMs = 5000.0f;
	registry.AssignState<CreatureObjectAction>(creature, action);
	_animation->durations[creature_throw::k_Eat] = 5000.0f;
	// 1.6 - 0.425 x 0.22, twice its own size's 0.75
	const auto rate = creature_layers::PlaybackRate(0.22f);
	EXPECT_NEAR(rate, 1.5065f, 1e-6f);

	_system.ProcessTurn();
	const auto timeMs = registry.Get<CreatureObjectAction>(creature).timeMs;
	EXPECT_FLOAT_EQ(timeMs, 100.0f * rate);

	_system.UpdateDraw(0.5f);
	const auto& slots = registry.Get<CreatureAnimation>(creature).slots;
	ASSERT_EQ(slots.size(), 1u);
	EXPECT_FLOAT_EQ(slots.front().timeMs, timeMs + (0.5f * 100.0f * rate));
}

TEST_F(CreatureObjectActionSystemTest, AVillagerEatenDiesAsAnyDeathGoes)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto home = Thing(glm::vec3(0.0f));
	auto& abode = registry.Assign<Abode>(home);
	const auto villager = Thing(glm::vec3(100.0f, 10.0f, 100.0f));
	auto& lives = registry.Assign<Villager>(villager);
	lives.abode = home;
	lives.town = entt::null;
	lives.life = 1.0f;
	abode.inhabitants.push_back(villager);
	Hold(creature, villager);
	registry.AssignState<CreatureObjectAction>(creature, AtItsMoment(Kind::Eat, villager));

	// at the moment it eats, the hand lets go of it: neither held nor drawn, and not used up yet
	_system.ProcessTurn();
	EXPECT_FALSE(_system.GetHeld(creature).has_value());
	EXPECT_FALSE(std::as_const(registry).AllOf<CreatureHeldObject>(creature));
	EXPECT_TRUE(ecs::IsAvailable(villager));
	EXPECT_TRUE(std::as_const(registry).AllOf<NotDrawn>(villager));

	// once the eating is over, it dies as any death goes
	for (int turn = 0; turn < 50 && registry.Get<CreatureObjectAction>(creature).status != Status::Done; ++turn)
	{
		_system.ProcessTurn();
	}
	ASSERT_EQ(registry.Get<CreatureObjectAction>(creature).status, Status::Done);
	EXPECT_FALSE(ecs::IsAvailable(villager));
	const auto& inhabitants = registry.Get<Abode>(home).inhabitants;
	EXPECT_EQ(std::ranges::find(inhabitants, villager), inhabitants.end());
}

TEST_F(CreatureObjectActionSystemTest, WhatIsEatenIsJudgedOnceTheEatingIsOver)
{
	const test::RestoreService<Locator::creatureMindSystem> restoreMinds;
	auto& minds = static_cast<fakes::FakeMind&>(Locator::creatureMindSystem::emplace<fakes::FakeMind>());
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	// a lump of poo, worth no food: it is eaten all the same, and found not to be food
	const auto poo = Thing(glm::vec3(100.0f, 10.0f, 100.0f));
	registry.Assign<MobileObject>(poo, MobileObjectInfo::LumpOfPoo);
	// its row of the tables says what type of object it is, as the game's data does
	_world.Info().mobileObject.at(static_cast<size_t>(MobileObjectInfo::LumpOfPoo)).type = ObjectType::MobileObject;
	Hold(creature, poo);
	registry.AssignState<CreatureObjectAction>(creature, AtItsMoment(Kind::Eat, poo));
	_system.ProcessTurn();
	EXPECT_TRUE(minds.meals.empty());
	for (int turn = 0; turn < 50 && registry.Get<CreatureObjectAction>(creature).status != Status::Done; ++turn)
	{
		_system.ProcessTurn();
	}
	ASSERT_EQ(registry.Get<CreatureObjectAction>(creature).status, Status::Done);
	EXPECT_FALSE(ecs::IsAvailable(poo));
	ASSERT_EQ(minds.meals.size(), 1u);
	const auto& [who, type, value, end] = minds.meals.front();
	EXPECT_EQ(who, creature);
	EXPECT_EQ(type, static_cast<uint32_t>(ObjectType::MobileObject));
	EXPECT_EQ(value, 0.0f);
	EXPECT_EQ(end, creature_eating::MealEnd::NotFood);
	// told once only
	_system.ProcessTurn();
	EXPECT_EQ(minds.meals.size(), 1u);
}

TEST_F(CreatureObjectActionSystemTest, WhatHasNoFoodValueIsEatenAllTheSame)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto poo = Thing(glm::vec3(100.0f, 10.0f, 100.0f));
	registry.Assign<MobileObject>(poo, MobileObjectInfo::LumpOfPoo);
	Hold(creature, poo);
	EXPECT_TRUE(_system.EatHeld(creature));
	EXPECT_EQ(registry.Get<CreatureObjectAction>(creature).status, Status::Running);
}

TEST_F(CreatureObjectActionSystemTest, APortionIsMadeFromAPileAndThePileLosesItAsTheHandTakesHold)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto pile = Thing(glm::vec3(130.0f, 0.0f, 100.0f));
	registry.Assign<Pot>(pile, Pot {.amount = 2500, .maxAmount = 10000, .type = PotInfo::MagicFood});
	static_cast<void>(_system.PickUpPortion(creature, pile, PotInfo::WheatInHand));

	// a pot of wheat as big as the creature's drawn size eats, made towards the pile at its reach, drawn 3.75 times its
	// size; the pile keeps all it has until the hand takes hold
	const auto& lookup = std::as_const(registry);
	const float drawnSize = creature_morph::ClampScale(ecs::creature_pose::DrawnSize(lookup, creature));
	const auto* making = registry.TryGet<CreatureObjectAction>(creature);
	ASSERT_NE(making, nullptr);
	ASSERT_TRUE(making->target.has_value());
	const auto portion = *making->target;
	ASSERT_NE(portion, pile);
	const auto* pot = registry.TryGet<Pot>(portion);
	ASSERT_NE(pot, nullptr);
	EXPECT_EQ(pot->type, PotInfo::WheatInHand);
	const auto amount = creature_eating::PortionAmount(drawnSize, 2500);
	EXPECT_EQ(static_cast<uint32_t>(pot->amount), amount);
	EXPECT_EQ(registry.Get<Transform>(portion).scale, glm::vec3(creature_eating::PortionScale(drawnSize)));
	const auto at = creature_eating::PortionPoint({100.0f, 100.0f}, {130.0f, 100.0f},
	                                              CreatureObjectActionSystem::PickUpReach(creature).value_or(0.0f));
	EXPECT_EQ(glm::xz(registry.Get<Transform>(portion).position), at);
	EXPECT_EQ(registry.Get<Pot>(pile).amount, 2500u);

	// at the moment the hand takes hold of the portion, the pile loses what the portion holds
	auto moment = AtItsMoment(Kind::PickUp, portion);
	moment.source = pile;
	registry.AssignOrReplaceState<CreatureObjectAction>(creature, moment);
	_system.ProcessTurn();
	ASSERT_EQ(_system.GetHeld(creature), std::optional(portion));
	EXPECT_EQ(static_cast<uint32_t>(registry.Get<Pot>(pile).amount), 2500u - amount);
}

TEST_F(CreatureObjectActionSystemTest, APortionOfFruitFromATreeHoldsNoFoodAndTheTreeLosesNothing)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto tree = Thing(glm::vec3(130.0f, 0.0f, 100.0f));
	registry.Assign<Tree>(tree);
	static_cast<void>(_system.PickUpPortion(creature, tree, PotInfo::TreeFruitInHand));

	// a tree holds no food, so the portion of its kind holds none
	const auto* making = registry.TryGet<CreatureObjectAction>(creature);
	ASSERT_NE(making, nullptr);
	ASSERT_TRUE(making->target.has_value());
	const auto portion = *making->target;
	ASSERT_NE(portion, tree);
	const auto* pot = registry.TryGet<Pot>(portion);
	ASSERT_NE(pot, nullptr);
	EXPECT_EQ(pot->type, PotInfo::TreeFruitInHand);
	EXPECT_EQ(pot->amount, 0u);

	// as the hand takes hold the tree, not a pot, is left as it was
	auto moment = AtItsMoment(Kind::PickUp, portion);
	moment.source = tree;
	registry.AssignOrReplaceState<CreatureObjectAction>(creature, moment);
	_system.ProcessTurn();
	ASSERT_EQ(_system.GetHeld(creature), std::optional(portion));
	EXPECT_TRUE(registry.Valid(tree));
	EXPECT_FALSE(registry.AllOf<Pot>(tree));
}

TEST_F(CreatureObjectActionSystemTest, TakenHoldOfAndLetGoOfThingsAreDrawnAtOnceWhereTheyAre)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto rock = Rock(glm::vec3(110.0f, 0.0f, 100.0f));
	registry.AssignState<CreatureObjectAction>(creature, AtItsMoment(Kind::PickUp, rock));
	_world.teleported.clear();

	// at the moment the hand takes hold, the rock is put in the palm, in the animation as it plays
	_system.ProcessTurn();
	ASSERT_EQ(_system.GetHeld(creature), std::optional(rock));
	EXPECT_EQ(registry.Get<CreatureObjectAction>(creature).status, Status::Contact);
	ASSERT_EQ(_world.teleported.size(), 1u);
	EXPECT_EQ(_world.teleported.front(), rock);
	EXPECT_EQ(registry.Get<Transform>(rock).position, glm::vec3(100.0f, 10.0f, 100.0f));
	EXPECT_FALSE(ecs::map_cells::IsObjectInMap(rock));

	// every frame it is drawn in the hand where the body is drawn: standing, where the turn put the body
	auto& animation = registry.Get<CreatureAnimation>(creature);
	if (animation.boneMatrices.empty())
	{
		animation.boneMatrices.assign(1, glm::mat4(1.0f));
	}
	const auto body = registry.Get<Transform>(creature);
	auto& locomotion = registry.Get<CreatureLocomotion>(creature);
	locomotion.started = true;
	locomotion.toPosition = body.position;
	auto& pose = registry.Get<CreatureDrawPose>(creature);
	pose = {.position = body.position, .rotation = body.rotation};
	const auto held = registry.Get<Transform>(rock);
	_system.UpdateHeldDraw();
	const auto standing = registry.Get<PhysicsDrawPose>(rock);
	// between turns, moved on with the body by as much as the body is; the rock's Transform stays where the turn put it
	const glm::vec3 step {-4.0f, 0.0f, 2.0f};
	pose.position = body.position + step;
	_system.UpdateHeldDraw();
	const auto& between = registry.Get<PhysicsDrawPose>(rock);
	for (glm::length_t axis = 0; axis < 3; ++axis)
	{
		EXPECT_NEAR(between.position[axis], standing.position[axis] + step[axis], 1e-4f);
	}
	EXPECT_EQ(between.rotation, standing.rotation);
	EXPECT_EQ(registry.Get<Transform>(rock).position, held.position);
	EXPECT_EQ(registry.Get<Transform>(rock).rotation, held.rotation);
	EXPECT_EQ(registry.Get<Transform>(creature).position, body.position);

	// let go of where it is
	_world.teleported.clear();
	CreatureObjectActionSystem::ReleaseHeld(rock, glm::vec3(120.0f, 0.0f, 90.0f), glm::vec3(0.0f), creature);
	ASSERT_EQ(_world.teleported.size(), 1u);
	EXPECT_EQ(_world.teleported.front(), rock);
	EXPECT_EQ(registry.Get<Transform>(rock).position, glm::vec3(120.0f, 0.0f, 90.0f));
	EXPECT_FALSE(std::as_const(registry).AllOf<HeldByCreature>(rock));
	// the creature that held it no longer does, so the next turn finds its hands empty of it
	registry.RemoveState<CreatureHeldObject>(creature);
	EXPECT_FALSE(_system.GetHeld(creature).has_value());
}

TEST_F(CreatureObjectActionSystemTest, WhatIsHeldIsPlacedEachTurnInThePalmOfTheBodyAsPosedForTheLastTurn)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto rock = Rock(glm::vec3(110.0f, 0.0f, 100.0f));
	Hold(creature, rock);
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.boneMatrices.assign(2, glm::mat4(1.0f));

	// not posed for a turn yet: in the palm as the frame poses the body, under its Transform
	_system.ProcessTurn();
	EXPECT_EQ(registry.Get<Transform>(rock).position, glm::vec3(100.0f, 0.0f, 100.0f));

	// posed for the last turn: in the palm of that pose, in the world, whatever the frame's pose
	auto& pose = registry.AssignState<CreatureTurnPose>(creature);
	const auto at = glm::vec3(30.0f, 5.0f, 40.0f);
	pose.current.assign(2, glm::mat4(1.0f));
	pose.current[0][3] = glm::vec4(at, 1.0f);
	pose.previous = pose.current;
	_system.ProcessTurn();
	EXPECT_EQ(registry.Get<Transform>(rock).position, at);

	// a pose with other bones than the frame's is not read
	pose.current.pop_back();
	_system.ProcessTurn();
	EXPECT_EQ(registry.Get<Transform>(rock).position, glm::vec3(100.0f, 0.0f, 100.0f));
}
