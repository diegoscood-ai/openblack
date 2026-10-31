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
#include <optional>
#include <span>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/vector_relational.hpp>
#include <gtest/gtest.h>

#include "Audio/Game/BankTables.h"
#include "Creature/CreatureEating.h"
#include "Creature/CreatureHold.h"
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
#include "ECS/Components/Indestructible.h"
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
#include "ECS/PlacedTurn.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/Implementations/VillagerPhysics.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
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
/// The sound tags: each point's tag made is kept; everything else does nothing
class PointSounds final: public ecs::systems::SoundTagSystemInterface
{
public:
	struct Made
	{
		glm::vec3 point;
		int sample;
		int mode;
		int loops;
		bool extra3DFlag;
		bool is3D;
		audio::SfxBank bank;
		int delay;
	};
	std::vector<Made> made;

	void ProcessTurn() override {}
	void SetActive(TagId, bool) override {}
	TagId CreatePointSound(const glm::vec3& point, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
	                       audio::SfxBank bank, int delay) override
	{
		made.push_back({.point = point,
		                .sample = sample,
		                .mode = mode,
		                .loops = loops,
		                .extra3DFlag = extra3DFlag,
		                .is3D = is3D,
		                .bank = bank,
		                .delay = delay});
		return static_cast<TagId>(made.size());
	}
	TagId CreatePointSound(const glm::vec3&, int, int, int, bool, bool, audio::AtmosType, int) override { return 0; }
	TagId Create(entt::entity, const glm::vec3&, int, bool, int, int, bool, bool, audio::SfxBank, int) override { return 0; }
	TagId CreateAtMarker(const glm::vec3&, int, int, int, bool, audio::SfxBank) override { return 0; }
	void Remove(entt::entity, int, audio::SfxBank, bool) override {}
	void Delete(TagId) override {}
	[[nodiscard]] bool Exists(TagId) const override { return false; }
	void Clear() override {}
	[[nodiscard]] std::optional<glm::vec3> TagSoundPoint(TagId) const override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> Point(TagId) const override { return std::nullopt; }
};

/// Broken buildings that never break, but keep every strike with no body
class KickedBuildings final: public ecs::systems::BuildingDamageSystemInterface
{
public:
	struct Strike
	{
		entt::entity building;
		entt::entity hitter;
		std::optional<PlayerNames> player;
		float radius;
		std::vector<BodilessBlow> blows;
	};
	std::vector<Strike> strikes;

	bool ReactToImpact(entt::entity, ecs::PhysicsEntry&) override { return true; }
	bool StrikeWithoutBody(entt::entity building, entt::entity hitter, std::optional<PlayerNames> player, float radius,
	                       std::span<const BodilessBlow> blows) override
	{
		strikes.push_back({.building = building,
		                   .hitter = hitter,
		                   .player = player,
		                   .radius = radius,
		                   .blows = std::vector<BodilessBlow>(blows.begin(), blows.end())});
		return true;
	}
	entt::entity PieceAtRest(entt::entity fragment, const glm::mat3&, glm::vec3) override { return fragment; }
	void ProcessTurn() override {}
	void DestroyFragment(entt::entity) override {}
	void ConnectDamageListener() override {}
	void Redraw(entt::entity) override {}
	void RemoveDamage(entt::entity) override {}
	void ForgetHitter(entt::entity, entt::entity) override {}
	void AppendDamageMeshes(graphics::world_triangles::Frame&, const physics::damage::Mesh::FrameLight&, float) override {}
	void SnapshotDamageMeshes(std::vector<ecs::systems::DamageMeshDraw>& out, const physics::damage::Mesh::FrameLight&,
	                          float) override
	{
		out.clear();
	}
	void AppendDamageMeshes(graphics::world_triangles::Frame&, const std::vector<ecs::systems::DamageMeshDraw>&) override {}
};

class CreatureObjectActionSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		_animation = &static_cast<fakes::FakeAnimation&>(Locator::creatureAnimationSystem::emplace<fakes::FakeAnimation>());
		_animation->bone = glm::vec3(0.0f, 10.0f, 0.0f);
		_locomotion = &static_cast<fakes::FakeLocomotion&>(Locator::creatureLocomotionSystem::emplace<fakes::FakeLocomotion>());
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
	const test::RestoreService<Locator::handSystem> _restoreHand;
	fakes::FakeAnimation* _animation {nullptr};
	fakes::FakeLocomotion* _locomotion {nullptr};
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

TEST_F(CreatureObjectActionSystemTest, AStandingTreeKnockedDownStaysStanding)
{
	// The knock-down strikes homes; a tree is kicked over, never knocked down, and no creature fells one as a forester
	// does
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto tree = Thing(glm::vec3(100.0f, 0.0f, 110.0f));
	registry.Assign<Tree>(tree);
	registry.AssignState<CreatureObjectAction>(creature, AtItsMoment(Kind::Destroy, tree));

	_system.ProcessTurn();
	EXPECT_EQ(registry.Get<CreatureObjectAction>(creature).status, Status::Contact);
	const auto& lookup = std::as_const(registry);
	EXPECT_TRUE(ecs::IsAvailable(tree));
	EXPECT_TRUE(lookup.AllOf<Tree>(tree));
	EXPECT_FALSE((lookup.AnyOf<DeadTree, FelledTree>(tree)));
}

TEST_F(CreatureObjectActionSystemTest, AMobileObjectKnockedDownIsLeftAsItIs)
{
	// The knock-down's blow does nothing to a mobile object, and its end does something only to a living thing
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto ball = Rock(glm::vec3(100.0f, 0.0f, 110.0f));
	registry.AssignState<CreatureObjectAction>(creature, AtItsMoment(Kind::Destroy, ball));

	_system.ProcessTurn();
	EXPECT_EQ(registry.Get<CreatureObjectAction>(creature).status, Status::Contact);
	EXPECT_TRUE(ecs::IsAvailable(ball));
	for (int turn = 0; turn < 50 && registry.Get<CreatureObjectAction>(creature).status != Status::Done; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_TRUE(ecs::IsAvailable(ball));
	EXPECT_EQ(registry.Get<Transform>(ball).position, glm::vec3(100.0f, 0.0f, 110.0f));
}

TEST_F(CreatureObjectActionSystemTest, AStaticThatIsNotARockKnockedDownIsLeftAsItIs)
{
	// The knock-down's blow breaks only what answers it is a rock; a fence takes nothing
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto fence = Thing(glm::vec3(100.0f, 0.0f, 110.0f));
	registry.Assign<MobileStatic>(fence, MobileStaticInfo::CeltFenceShort);
	registry.AssignState<CreatureObjectAction>(creature, AtItsMoment(Kind::Destroy, fence));

	_system.ProcessTurn();
	EXPECT_EQ(registry.Get<CreatureObjectAction>(creature).status, Status::Contact);
	EXPECT_TRUE(ecs::IsAvailable(fence));
	EXPECT_EQ(registry.Get<Transform>(fence).position, glm::vec3(100.0f, 0.0f, 110.0f));
}

TEST_F(CreatureObjectActionSystemTest, AKickedHomeTakesTwoBlowsOfTheFootWhenItIsBuiltAndNotBurning)
{
	const test::RestoreService<Locator::buildingDamageSystem> restoreDamage;
	auto& buildings = static_cast<KickedBuildings&>(Locator::buildingDamageSystem::emplace<KickedBuildings>());
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto home = Thing(glm::vec3(100.0f, 0.0f, 110.0f));
	registry.Assign<Abode>(home);
	// the right foot grasps where the creature stands: the blows strike there, on the flat land
	_animation->grasp = glm::vec3(0.0f);
	registry.AssignState<CreatureObjectAction>(creature, AtItsMoment(Kind::Kick, home));
	_system.ProcessTurn();
	ASSERT_EQ(buildings.strikes.size(), 1u);
	const auto& strike = buildings.strikes.front();
	EXPECT_EQ(strike.building, home);
	EXPECT_EQ(strike.hitter, creature);
	const auto owner = registry.Get<Creature>(creature).owner;
	EXPECT_EQ(strike.player, owner != PlayerNames::NEUTRAL ? std::optional(owner) : std::nullopt);
	ASSERT_EQ(strike.blows.size(), 2u);
	const auto& at = registry.Get<Transform>(creature).position;
	for (const auto& blow : strike.blows)
	{
		EXPECT_LT(glm::distance(glm::vec2(blow.point.x, blow.point.z), glm::vec2(at.x, at.z)), 1e-3f);
		EXPECT_NEAR(glm::length(blow.velocity), creature_object_actions::k_HomeKickBlowSpeed, 1e-4f);
	}
	// the second blow along the land
	EXPECT_EQ(strike.blows.back().velocity.y, 0.0f);
	// the home still stands
	EXPECT_TRUE(ecs::IsAvailable(home));

	// a home not built yet takes no kick
	registry.Get<Abode>(home).percentBuilt = 0.5f;
	registry.AssignOrReplaceState<CreatureObjectAction>(creature, AtItsMoment(Kind::Kick, home));
	_system.ProcessTurn();
	EXPECT_EQ(buildings.strikes.size(), 1u);
}

TEST_F(CreatureObjectActionSystemTest, AKickedStandingTreeCreaksWhereTheFootMeetsIt)
{
	const test::RestoreService<Locator::soundTagSystem> restoreTags;
	auto& tags = static_cast<PointSounds&>(Locator::soundTagSystem::emplace<PointSounds>());
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto tree = Thing(glm::vec3(100.0f, 0.0f, 110.0f));
	registry.Assign<Tree>(tree);
	// the right foot grasps where the creature stands in the kicking animation
	_animation->grasp = glm::vec3(0.0f);
	registry.AssignState<CreatureObjectAction>(creature, AtItsMoment(Kind::Kick, tree));
	_system.ProcessTurn();
	EXPECT_EQ(registry.Get<CreatureObjectAction>(creature).status, Status::Contact);
	ASSERT_EQ(tags.made.size(), 1u);
	const auto& creak = tags.made.front();
	EXPECT_LT(glm::distance(creak.point, registry.Get<Transform>(creature).position), 1e-3f);
	// one of the three creaks of the second set, played once, in 3D, at once
	EXPECT_GE(creak.sample, 136);
	EXPECT_LT(creak.sample, 139);
	EXPECT_EQ(creak.mode, 3);
	EXPECT_EQ(creak.loops, 0);
	EXPECT_FALSE(creak.extra3DFlag);
	EXPECT_TRUE(creak.is3D);
	EXPECT_EQ(creak.bank, audio::SfxBank::InGame);
	EXPECT_EQ(creak.delay, 0);
	// the point is the right foot's in the kicking animation at the moment the hand takes from the player's hand
	ASSERT_EQ(_animation->graspsAsked.size(), 1u);
	EXPECT_EQ(_animation->graspsAsked.front().animation, creature_object_actions::k_KickAnimation);
	EXPECT_EQ(_animation->graspsAsked.front().timeMs, 310.0f);
	EXPECT_EQ(_animation->graspsAsked.front().bone, 5u);

	// a tree already flying makes no creak
	const auto flying = Thing(glm::vec3(100.0f, 0.0f, 110.0f));
	registry.Assign<Tree>(flying);
	registry.AssignState<InPhysics>(flying);
	registry.AssignOrReplaceState<CreatureObjectAction>(creature, AtItsMoment(Kind::Kick, flying));
	_system.ProcessTurn();
	EXPECT_EQ(tags.made.size(), 1u);
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

TEST_F(CreatureObjectActionSystemTest, AMealMarkedIndestructibleIsSpatOutWholeAndLetGoOf)
{
	// A thing marked indestructible is not eaten: the mind hears it was refused, and the thing is let go of where it
	// is, drawn again and on the land (docs/wiki/creature/plans-and-needs.md)
	const test::RestoreService<Locator::creatureMindSystem> restoreMinds;
	auto& minds = static_cast<fakes::FakeMind&>(Locator::creatureMindSystem::emplace<fakes::FakeMind>());
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto poo = Thing(glm::vec3(100.0f, 10.0f, 100.0f));
	registry.Assign<MobileObject>(poo, MobileObjectInfo::LumpOfPoo);
	registry.Assign<Indestructible>(poo);
	_world.Info().mobileObject.at(static_cast<size_t>(MobileObjectInfo::LumpOfPoo)).type = ObjectType::MobileObject;
	Hold(creature, poo);
	registry.AssignState<CreatureObjectAction>(creature, AtItsMoment(Kind::Eat, poo));
	_world.teleported.clear();
	for (int turn = 0; turn < 50 && registry.Get<CreatureObjectAction>(creature).status != Status::Done; ++turn)
	{
		_system.ProcessTurn();
	}
	ASSERT_EQ(registry.Get<CreatureObjectAction>(creature).status, Status::Done);
	_system.ProcessTurn();
	EXPECT_TRUE(ecs::IsAvailable(poo));
	EXPECT_FALSE(std::as_const(registry).AllOf<NotDrawn>(poo));
	EXPECT_FALSE(_system.GetHeld(creature).has_value());
	ASSERT_EQ(minds.meals.size(), 1u);
	EXPECT_EQ(std::get<3>(minds.meals.front()), creature_eating::MealEnd::Refused);
	ASSERT_FALSE(_world.teleported.empty());
	EXPECT_EQ(_world.teleported.back(), poo);
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

TEST_F(CreatureObjectActionSystemTest, AVillagerTakenHoldOfIsMarkedHeldAndKeepsItsYAngle)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto villager = Thing(glm::vec3(110.0f, 0.0f, 100.0f));
	auto& lives = registry.Assign<Villager>(villager);
	lives.town = entt::null;
	lives.life = 1.0f;
	// turned about the upright, so that the angle it keeps is not the one the hand gives it
	registry.Get<Transform>(villager).rotation =
	    glm::mat3(glm::vec3(0.6f, 0.0f, -0.8f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.8f, 0.0f, 0.6f));
	const auto before = ecs::villager_physics::ObjectYAngle(villager);
	registry.AssignState<CreatureObjectAction>(creature, AtItsMoment(Kind::PickUp, villager));

	// at the moment the hand takes hold, the villager is in the hand as the player's hand takes one: marked held, with
	// the y angle it had before
	_system.ProcessTurn();
	ASSERT_EQ(_system.GetHeld(creature), std::optional(villager));
	EXPECT_NE(registry.Get<Villager>(villager).flags & Villager::k_FlagInHand, 0);
	const auto* kept = std::as_const(registry).TryGet<const ecs::villager_physics::HeldYAngle>(villager);
	ASSERT_NE(kept, nullptr);
	EXPECT_EQ(kept->value, before);
}

TEST_F(CreatureObjectActionSystemTest, WhatIsHeldIsLetGoOfOnlyOnceTheAnimationReachesTheSpeciesMoment)
{
	const test::RestoreService<Locator::creatureMindSystem> restoreMinds;
	Locator::creatureMindSystem::emplace<fakes::FakeMind>();
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto poo = Thing(glm::vec3(100.0f, 10.0f, 100.0f));
	registry.Assign<MobileObject>(poo, MobileObjectInfo::LumpOfPoo);
	Hold(creature, poo);
	// the animations last longer than the species' moments (the rig's put-down at 800 ms, toss at 500, eating at 600)
	for (const auto animation : {creature_throw::k_Eat, creature_throw::k_PutDown, creature_throw::k_Discard})
	{
		_animation->durations[animation] = 2000.0f;
	}
	const auto& action = [&registry, creature]() -> const CreatureObjectAction& {
		return registry.Get<const CreatureObjectAction>(creature);
	};

	// each action on what it holds takes its moment from the species' rig
	ASSERT_TRUE(_system.PutDown(creature));
	EXPECT_EQ(action().eventMs, 800.0f);
	// an animation shorter than its moment lets go by its end at the latest
	_system.Cancel(creature);
	_animation->durations[creature_throw::k_PutDown] = 700.0f;
	ASSERT_TRUE(_system.PutDown(creature));
	EXPECT_EQ(action().eventMs, 699.0f);
	_system.Cancel(creature);
	ASSERT_TRUE(_system.Discard(creature));
	EXPECT_EQ(action().eventMs, 500.0f);
	_system.Cancel(creature);
	ASSERT_TRUE(_system.EatHeld(creature));
	ASSERT_EQ(action().phase, Phase::Playing);
	EXPECT_EQ(action().eventMs, 600.0f);

	// the hand keeps hold every turn the animation is short of the moment, and lets go in the turn it reaches it
	bool reached = false;
	for (int turn = 0; turn < 20 && !reached; ++turn)
	{
		_system.ProcessTurn();
		reached = action().timeMs >= 600.0f;
		EXPECT_EQ(action().eventDone, reached) << "at " << action().timeMs << " ms";
		EXPECT_EQ(_system.GetHeld(creature).has_value(), !reached) << "at " << action().timeMs << " ms";
	}
	EXPECT_TRUE(reached);
}

TEST_F(CreatureObjectActionSystemTest, AThrowIsTimedAsItStartsByTheDistanceAlongTheGround)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto rock = Rock(glm::vec3(110.0f, 0.0f, 100.0f));
	Hold(creature, rock);

	// 30 along the ground and 40 up: the time is the one for the 30, not for the 50 between them
	const glm::vec3 target {130.0f, 40.0f, 100.0f};
	ASSERT_TRUE(_system.Throw(creature, target));
	const auto& action = registry.Get<CreatureObjectAction>(creature);
	EXPECT_EQ(action.phase, Phase::Approach);
	EXPECT_EQ(action.point, target);
	EXPECT_EQ(action.flightSeconds, creature_throw::ThrowFlightTime(30.0f));
	EXPECT_FALSE(action.givenFlightSeconds.has_value());

	// Given a time, it takes that one
	ASSERT_TRUE(_system.ThrowTaking(creature, target, 2.5f));
	EXPECT_EQ(registry.Get<CreatureObjectAction>(creature).flightSeconds, 2.5f);
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
	// turned by the hand's bone: its forward row up, its side row and its up row turned back, the rows made of unit
	// length as the original makes them (a hair under 1)
	EXPECT_EQ(registry.Get<Transform>(rock).rotation, creature_hold::HeldTurn(glm::mat3(1.0f), false));
	const auto& turned = registry.Get<Transform>(rock).rotation;
	EXPECT_EQ(glm::sign(turned[0]), glm::vec3(-1.0f, 0.0f, 0.0f));
	EXPECT_EQ(glm::sign(turned[1]), glm::vec3(0.0f, 0.0f, 1.0f));
	EXPECT_EQ(glm::sign(turned[2]), glm::vec3(0.0f, -1.0f, 0.0f));

	// a pose with other bones than the frame's is not read
	pose.current.pop_back();
	_system.ProcessTurn();
	EXPECT_EQ(registry.Get<Transform>(rock).position, glm::vec3(100.0f, 0.0f, 100.0f));
}

TEST_F(CreatureObjectActionSystemTest, WhatItPutsDownTakesTheAnglesOfItsHandsBoneAsTheBodyWasPosedForTheLastTurn)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto rock = Rock(glm::vec3(110.0f, 0.0f, 100.0f));
	Hold(creature, rock);
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.boneMatrices.assign(2, glm::mat4(1.0f));
	// the hand's bone as the body was posed for the last turn, turned a quarter round its side row (a bone whose up row
	// stays upright gives the thing an upright forward row, whose angles come out not a number)
	const glm::mat3 quarter {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)};
	auto& pose = registry.AssignState<CreatureTurnPose>(creature);
	pose.current.assign(2, glm::mat4(quarter));
	pose.previous = pose.current;
	registry.AssignState<CreatureObjectAction>(creature, AtItsMoment(Kind::PutDown, std::nullopt));

	_system.ProcessTurn();
	ASSERT_FALSE(_system.GetHeld(creature).has_value());
	const bool canLean = ecs::placed_turn::CanLean(registry, rock);
	const auto kept = ecs::placed_turn::KeptAngles(creature_hold::AnglesAtRelease(quarter, false, 1.0f), canLean);
	ASSERT_FALSE(glm::any(glm::isnan(kept)));
	EXPECT_EQ(registry.Get<Transform>(rock).rotation, ecs::placed_turn::TurnOf(kept, canLean));
}

TEST_F(CreatureObjectActionSystemTest, AfterAWalkTheHandsActFromTheNextTurnAndStraightAfterTheirOwnTurn)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	auto& transform = registry.Get<Transform>(creature);
	transform.rotation = glm::mat3(1.0f);
	const auto phase = [&registry, creature]() { return registry.Get<const CreatureObjectAction>(creature).phase; };

	// Asked while it walks, it points only from the turn after the walk ends. The hands set about it before the bodies
	// move (ProcessApproach), and the walk ends as they move
	_locomotion->moving = true;
	ASSERT_TRUE(_system.PointAt(creature, glm::vec3(100.0f, 0.0f, 50.0f)));
	_system.ProcessApproach();
	_system.ProcessTurn();
	EXPECT_EQ(phase(), Phase::Approach);
	_system.ProcessApproach();
	_locomotion->moving = false;
	_system.ProcessTurn();
	EXPECT_EQ(phase(), Phase::Approach);
	_system.ProcessApproach();
	EXPECT_EQ(phase(), Phase::Playing);
	_system.ProcessTurn();

	// Standing, it points in the turn it is asked, before the bodies move
	_system.Cancel(creature);
	ASSERT_TRUE(_system.PointAt(creature, glm::vec3(100.0f, 0.0f, 50.0f)));
	_system.ProcessApproach();
	EXPECT_EQ(phase(), Phase::Playing);
	_system.ProcessTurn();

	// Behind it, it turns to face the point first, and points in the turn that turn ends
	_system.Cancel(creature);
	ASSERT_TRUE(_system.PointAt(creature, glm::vec3(100.0f, 0.0f, 150.0f)));
	_system.ProcessApproach();
	EXPECT_EQ(phase(), Phase::Approach);
	_locomotion->moving = true;
	_system.ProcessTurn();
	EXPECT_EQ(phase(), Phase::Approach);
	_system.ProcessApproach();
	transform.rotation = glm::mat3(glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	_locomotion->moving = false;
	_system.ProcessTurn();
	EXPECT_EQ(phase(), Phase::Playing);
}

TEST_F(CreatureObjectActionSystemTest, StopPointingEndsAPointingThatPlaysAndLeavesOneUnderWay)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<Transform>(creature).rotation = glm::mat3(1.0f);
	const auto& action = [&registry, creature]() -> const CreatureObjectAction& {
		return registry.Get<const CreatureObjectAction>(creature);
	};
	ASSERT_TRUE(_system.PointAt(creature, glm::vec3(100.0f, 0.0f, 50.0f)));
	_system.ProcessApproach();
	ASSERT_EQ(action().phase, Phase::Playing);
	ASSERT_GT(action().holdMs, 0.0f);
	_system.StopPointing(creature);
	EXPECT_EQ(action().status, Status::Done);
	EXPECT_EQ(action().holdMs, 0.0f);

	// Behind it, it is still turning to face the point: that goes on
	_system.Cancel(creature);
	ASSERT_TRUE(_system.PointAt(creature, glm::vec3(100.0f, 0.0f, 150.0f)));
	_system.ProcessApproach();
	ASSERT_EQ(action().phase, Phase::Approach);
	_system.StopPointing(creature);
	EXPECT_EQ(action().status, Status::Running);
}

TEST_F(CreatureObjectActionSystemTest, WhatItDoesWithWhatItHoldsWaitsWhileItsBodyIsBusy)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto rock = Rock(glm::vec3(110.0f, 0.0f, 100.0f));
	Hold(creature, rock);
	auto& animation = registry.Get<CreatureAnimation>(creature);
	const auto phase = [&registry, creature]() { return registry.Get<const CreatureObjectAction>(creature).phase; };

	// Standing, it tosses it away at once
	ASSERT_TRUE(_system.Discard(creature));
	EXPECT_EQ(phase(), Phase::Playing);

	// Walking, it waits for the walk to end, which it does not stop, and tosses it away the turn after
	_locomotion->stopped.clear();
	_locomotion->moving = true;
	_system.Cancel(creature);
	ASSERT_TRUE(_system.Discard(creature));
	EXPECT_EQ(phase(), Phase::Approach);
	_system.ProcessApproach();
	EXPECT_EQ(phase(), Phase::Approach);
	EXPECT_TRUE(_locomotion->stopped.empty());
	_locomotion->moving = false;
	_system.ProcessTurn();
	EXPECT_EQ(phase(), Phase::Approach);
	_system.ProcessApproach();
	EXPECT_EQ(phase(), Phase::Playing);

	// Playing an action of its own, it waits for that to end too
	animation.body.kind = creature_layers::BodyAction::Kind::Once;
	_system.Cancel(creature);
	ASSERT_TRUE(_system.PutDown(creature));
	EXPECT_EQ(phase(), Phase::Approach);
	_system.ProcessApproach();
	EXPECT_EQ(phase(), Phase::Approach);
	EXPECT_EQ(animation.body.kind, creature_layers::BodyAction::Kind::Once);
	animation.body = {};
	_system.ProcessApproach();
	EXPECT_EQ(phase(), Phase::Playing);
}

TEST_F(CreatureObjectActionSystemTest, WhatItLetsGoOfCountsAsDroppedFromWhenItSetsAboutIt)
{
	const test::RestoreService<Locator::creatureMindSystem> restoreMinds;
	Locator::creatureMindSystem::emplace<fakes::FakeMind>();
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto rock = Rock(glm::vec3(110.0f, 0.0f, 100.0f));
	Hold(creature, rock);
	// The animations outlast the species' moments, so each lets go part of the way through
	for (const auto animation : {creature_throw::k_Eat, creature_throw::k_PutDown, creature_throw::k_Discard})
	{
		_animation->durations[animation] = 2000.0f;
	}
	const auto dropped = [&registry, creature]() {
		const auto* last = std::as_const(registry).TryGet<const CreatureDroppedObject>(creature);
		return last != nullptr ? last->object : entt::null;
	};
	const auto phase = [&registry, creature]() { return registry.Get<const CreatureObjectAction>(creature).phase; };

	// While it walks a toss waits, and nothing counts as dropped yet
	_locomotion->moving = true;
	ASSERT_TRUE(_system.Discard(creature));
	EXPECT_EQ(phase(), Phase::Approach);
	EXPECT_TRUE(dropped() == entt::null);
	// It counts from the turn it sets about it, still holding it
	_locomotion->moving = false;
	_system.ProcessTurn();
	_system.ProcessApproach();
	ASSERT_EQ(phase(), Phase::Playing);
	EXPECT_EQ(dropped(), rock);
	EXPECT_EQ(_system.GetHeld(creature), rock);

	// Eating what it holds is no letting go of it
	registry.RemoveState<CreatureDroppedObject>(creature);
	_system.Cancel(creature);
	ASSERT_TRUE(_system.EatHeld(creature));
	EXPECT_TRUE(dropped() == entt::null);

	// Put down, it counts again as it lets go
	_system.Cancel(creature);
	ASSERT_TRUE(_system.PutDown(creature));
	EXPECT_EQ(dropped(), rock);
	registry.RemoveState<CreatureDroppedObject>(creature);
	for (int turn = 0; turn < 100 && _system.GetHeld(creature).has_value(); ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_FALSE(_system.GetHeld(creature).has_value());
	EXPECT_EQ(dropped(), rock);
}

TEST_F(CreatureObjectActionSystemTest, WhatIsAskedWhileTheHandsStillPlayWaitsForThemToEnd)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<Transform>(creature).rotation = glm::mat3(1.0f);
	auto keep = AtItsMoment(Kind::Keep, std::nullopt);
	keep.eventDone = true;
	keep.durationMs = 1.0f;
	registry.AssignState<CreatureObjectAction>(creature, keep);
	const auto kind = [&registry, creature]() { return registry.Get<const CreatureObjectAction>(creature).kind; };

	// The keep plays on; the point waits, under way
	ASSERT_TRUE(_system.PointAt(creature, glm::vec3(100.0f, 0.0f, 50.0f)));
	EXPECT_EQ(kind(), Kind::Keep);
	EXPECT_EQ(_system.GetState(creature), CreatureObjectActionSystem::State::Busy);
	_system.ProcessApproach();
	EXPECT_EQ(kind(), Kind::Keep);
	// The keep ends this turn, and the point is set about the next
	_system.ProcessTurn();
	EXPECT_EQ(kind(), Kind::Keep);
	EXPECT_EQ(_system.GetState(creature), CreatureObjectActionSystem::State::Busy);
	_system.ProcessApproach();
	EXPECT_EQ(kind(), Kind::Point);
	EXPECT_EQ(registry.Get<const CreatureObjectAction>(creature).phase, Phase::Playing);
	EXPECT_FALSE(std::as_const(registry).AllOf<CreatureObjectActionWaiting>(creature));

	// Called off, what waits goes with what plays
	ASSERT_TRUE(_system.PointAt(creature, glm::vec3(100.0f, 0.0f, 50.0f)));
	EXPECT_TRUE(std::as_const(registry).AllOf<CreatureObjectActionWaiting>(creature));
	_system.Cancel(creature);
	EXPECT_FALSE(std::as_const(registry).AllOf<CreatureObjectActionWaiting>(creature));
	EXPECT_EQ(_system.GetState(creature), CreatureObjectActionSystem::State::Idle);
}

TEST_F(CreatureObjectActionSystemTest, WhatIsAskedWhileItCatchesWaitsForTheCatchToEnd)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<Transform>(creature).rotation = glm::mat3(1.0f);
	registry.AssignState<CreatureCatching>(creature, CreatureCatching {.stage = CreatureCatching::Stage::Reaching});

	// Asked while it catches, the point waits, under way, and the catch goes on
	ASSERT_TRUE(_system.PointAt(creature, glm::vec3(100.0f, 0.0f, 50.0f)));
	EXPECT_EQ(_system.GetState(creature), CreatureObjectActionSystem::State::Busy);
	EXPECT_TRUE(_system.IsCatching(creature));
	_system.ProcessApproach();
	EXPECT_FALSE(std::as_const(registry).AllOf<CreatureObjectAction>(creature));
	EXPECT_TRUE(std::as_const(registry).AllOf<CreatureObjectActionWaiting>(creature));

	// Once the catch is over, it points
	registry.Get<CreatureCatching>(creature).stage = CreatureCatching::Stage::Over;
	_system.ProcessApproach();
	ASSERT_TRUE(std::as_const(registry).AllOf<CreatureObjectAction>(creature));
	EXPECT_EQ(registry.Get<const CreatureObjectAction>(creature).kind, Kind::Point);
	EXPECT_EQ(registry.Get<const CreatureObjectAction>(creature).phase, Phase::Playing);
	EXPECT_FALSE(std::as_const(registry).AllOf<CreatureObjectActionWaiting>(creature));
}

TEST_F(CreatureObjectActionSystemTest, HoldingSomethingItTakesNothingFromTheHandAndTheTakeIsDone)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto rock = Rock(glm::vec3(110.0f, 0.0f, 100.0f));
	Hold(creature, rock);
	const auto gift = Rock(glm::vec3(120.0f, 0.0f, 100.0f));
	auto& hand = static_cast<fakes::FakeHand&>(Locator::handSystem::emplace<fakes::FakeHand>());
	hand.held = gift;

	ASSERT_TRUE(_system.TakeFromHand(creature, gift, false));
	EXPECT_EQ(_system.GetState(creature), CreatureObjectActionSystem::State::Done);
	EXPECT_FALSE(_system.GetTakingFromHand(creature).has_value());
	EXPECT_EQ(_system.GetHeld(creature), std::optional(rock));

	// With its hand empty it reaches for it
	registry.RemoveState<CreatureHeldObject>(creature);
	registry.RemoveState<HeldByCreature>(rock);
	ASSERT_TRUE(_system.TakeFromHand(creature, gift, false));
	EXPECT_EQ(_system.GetState(creature), CreatureObjectActionSystem::State::Busy);
	EXPECT_EQ(registry.Get<const CreatureObjectAction>(creature).phase, Phase::Playing);
}

TEST_F(CreatureObjectActionSystemTest, TheTakeFailsWithNothingInThePlayersHandAndReachesForWhatItHolds)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto gift = Rock(glm::vec3(120.0f, 0.0f, 100.0f));

	// No hand, or a hand that holds nothing: the take fails, whether the creature holds something or not
	Locator::handSystem::reset();
	EXPECT_FALSE(_system.TakeFromHand(creature, gift, false));
	EXPECT_EQ(_system.GetState(creature), CreatureObjectActionSystem::State::Failed);
	auto& hand = static_cast<fakes::FakeHand&>(Locator::handSystem::emplace<fakes::FakeHand>());
	EXPECT_FALSE(_system.TakeFromHand(creature, gift, false));
	const auto rock = Rock(glm::vec3(110.0f, 0.0f, 100.0f));
	Hold(creature, rock);
	EXPECT_FALSE(_system.TakeFromHand(creature, gift, false));
	EXPECT_EQ(_system.GetState(creature), CreatureObjectActionSystem::State::Failed);

	// The hand holding another thing by then, it reaches for that one
	registry.RemoveState<CreatureHeldObject>(creature);
	registry.RemoveState<HeldByCreature>(rock);
	const auto other = Rock(glm::vec3(130.0f, 0.0f, 100.0f));
	hand.held = other;
	ASSERT_TRUE(_system.TakeFromHand(creature, gift, false));
	const auto taking = _system.GetTakingFromHand(creature);
	ASSERT_TRUE(taking.has_value());
	EXPECT_EQ(taking->object, other);
}

TEST_F(CreatureObjectActionSystemTest, ACalledOffTakePlaysOnToItsMomentAndClosesOnNothing)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto gift = Rock(glm::vec3(120.0f, 0.0f, 100.0f));
	auto& hand = static_cast<fakes::FakeHand&>(Locator::handSystem::emplace<fakes::FakeHand>());
	hand.held = gift;
	ASSERT_TRUE(_system.TakeFromHand(creature, gift, false));
	ASSERT_TRUE(_system.GetTakingFromHand(creature).has_value());
	EXPECT_TRUE(_system.IsSetOnTakingFromHand(creature));

	// Called off, it reaches for nothing now, and its mind giving up does not stop its hand
	_system.CallOffTake(creature);
	EXPECT_FALSE(_system.GetTakingFromHand(creature).has_value());
	EXPECT_TRUE(CreatureObjectActionSystem::IsCalledOffTake(registry.Get<const CreatureObjectAction>(creature)));
	_system.Cancel(creature);
	ASSERT_TRUE(std::as_const(registry).AllOf<CreatureObjectAction>(creature));
	EXPECT_EQ(_system.GetState(creature), CreatureObjectActionSystem::State::Busy);

	// At its moment its hand closes on nothing, and it stands there with its hands free
	registry.Get<CreatureObjectAction>(creature).timeMs = registry.Get<const CreatureObjectAction>(creature).eventMs;
	_system.ProcessTurn();
	EXPECT_EQ(_system.GetState(creature), CreatureObjectActionSystem::State::Failed);
	EXPECT_FALSE(_system.GetHeld(creature).has_value());
	EXPECT_FALSE(_system.GetProgress(creature).has_value());
	EXPECT_FALSE(_system.IsSetOnTakingFromHand(creature));

	// Once it has taken the thing, a call-off changes nothing
	auto taken = AtItsMoment(Kind::TakeFromHand, gift);
	taken.status = Status::Contact;
	registry.AssignOrReplaceState<CreatureObjectAction>(creature, taken);
	_system.CallOffTake(creature);
	EXPECT_EQ(registry.Get<const CreatureObjectAction>(creature).target, std::optional(gift));
	EXPECT_FALSE(CreatureObjectActionSystem::IsCalledOffTake(registry.Get<const CreatureObjectAction>(creature)));
}

TEST_F(CreatureObjectActionSystemTest, OnlyAnActionUnderWayKeepsACatchFromStarting)
{
	EXPECT_FALSE(CreatureObjectActionSystem::KeepsFromCatching(nullptr));
	CreatureObjectAction action;
	for (const auto status : {Status::Running, Status::Contact})
	{
		action.status = status;
		action.phase = Phase::Approach;
		EXPECT_TRUE(CreatureObjectActionSystem::KeepsFromCatching(&action));
		action.phase = Phase::Playing;
		EXPECT_TRUE(CreatureObjectActionSystem::KeepsFromCatching(&action));
	}
	for (const auto status : {Status::Done, Status::Failed})
	{
		action.status = status;
		EXPECT_FALSE(CreatureObjectActionSystem::KeepsFromCatching(&action));
	}
}
