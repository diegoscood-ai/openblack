/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// GET_OBJECT_DESTINATION as a script calls it, on a land whose height is a function of the point: a villager gives the
// goal of its walk, an animal the goal of its move and a creature the place it last set off to, each on the land there
// plus the goal's height above the land; a thing that does not live, or no thing, gives (0, 0, 0)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <string_view>
#include <utility>

#include <LHVM.h>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "CHLApi.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/ScriptDestination.h"
#include "ECS/WalkGoal.h"
#include "Locator.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using ecs::components::Transform;

namespace
{
/// The land's height at a point
[[nodiscard]] float LandAt(glm::vec2 xz)
{
	return 20.0f + (xz.x * 0.01f) + (xz.y * 0.001f);
}

class ScriptObjectDestination: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		// entity 0 is the scripts' "no thing"
		Locator::entitiesRegistry::value().Create();
		Locator::vm::emplace<lhvm::LHVM>();
		Locator::terrainSystem::emplace<test::HeightFieldIsland>(LandAt);
	}

	/// GET_OBJECT_DESTINATION of the thing, the only three values it leaves above the script's own
	glm::vec3 Destination(entt::entity thing)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "GET_OBJECT_DESTINATION"; });
		EXPECT_NE(native, table.end());
		if (native == table.end())
		{
			return {};
		}
		auto& vm = Locator::vm::value();
		vm.Pushi(k_ScriptsOwn);
		vm.Pusho(static_cast<uint32_t>(thing));
		native->impl();
		const float z = vm.Popf();
		const float y = vm.Popf();
		const float x = vm.Popf();
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << "GET_OBJECT_DESTINATION left more than three values";
		return {x, y, z};
	}

	/// A thing standing at `at`
	static entt::entity Thing(glm::vec3 at)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto thing = registry.Create();
		registry.Assign<Transform>(thing, at, glm::mat3(1.0f), glm::vec3(1.0f));
		return thing;
	}

	static constexpr int32_t k_ScriptsOwn = 7777;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::terrainSystem> _restoreTerrain;
	chlapi::CHLApi _api;
};
} // namespace

TEST_F(ScriptObjectDestination, AVillagerGivesTheGoalOfItsWalkOnTheLand)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = Thing({10.0f, 20.0f, 30.0f});
	registry.Assign<ecs::components::Villager>(villager);
	auto& wallHug = registry.Assign<ecs::components::WallHug>(villager);
	const glm::vec2 goal(100.0f, 200.0f);
	ecs::walk_goal::Set(villager, wallHug.goal, goal);
	// far from its goal, it gives the goal, not where it is
	EXPECT_EQ(Destination(villager), glm::vec3(goal.x, LandAt(goal), goal.y));
}

TEST_F(ScriptObjectDestination, AVillagersGoalKeepsTheHeightAScriptsWalkGaveIt)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = Thing({10.0f, 20.0f, 30.0f});
	registry.Assign<ecs::components::Villager>(villager);
	auto& wallHug = registry.Assign<ecs::components::WallHug>(villager);
	const glm::vec2 goal(100.0f, 200.0f);
	ecs::walk_goal::Set(villager, wallHug.goal, goal);
	ecs::walk_goal::SetAltitude(villager, 4.5f);
	EXPECT_EQ(Destination(villager).y, LandAt(goal) + 4.5f);
}

TEST_F(ScriptObjectDestination, AnAnimalGivesTheGoalOfItsMoveWithItsHeightAboveTheLand)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto animal = Thing({10.0f, 20.0f, 30.0f});
	registry.Assign<ecs::components::Animal>(animal);
	auto& brain = registry.Assign<ecs::components::AnimalBrain>(animal);
	const glm::vec2 goal(150.0f, 250.0f);
	brain.goal = goal;
	brain.goalAltitude = 12.5f;
	EXPECT_EQ(Destination(animal), glm::vec3(goal.x, LandAt(goal) + 12.5f, goal.y));
}

TEST_F(ScriptObjectDestination, ACreatureGivesThePlaceItLastSetOffTo)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto creature = Thing({10.0f, 20.0f, 30.0f});
	auto& locomotion = registry.AssignState<ecs::components::CreatureLocomotion>(creature);
	const glm::vec2 place(300.0f, 400.0f);
	locomotion.lastDestination = map_coords::FromMetres(place);
	locomotion.lastDestination.altitude = 3.0f;
	// it has arrived and stopped: the place is kept
	locomotion.destination.reset();
	EXPECT_EQ(Destination(creature), glm::vec3(place.x, LandAt(place) + 3.0f, place.y));
}

TEST_F(ScriptObjectDestination, AThingThatDoesNotLiveGivesTheOrigin)
{
	const auto rock = Thing({50.0f, 3.0f, 60.0f});
	EXPECT_EQ(Destination(rock), glm::vec3(0.0f));
	// a wall hug alone does not make a villager
	Locator::entitiesRegistry::value().Assign<ecs::components::WallHug>(rock);
	EXPECT_EQ(Destination(rock), glm::vec3(0.0f));
}

TEST_F(ScriptObjectDestination, NoThingGivesTheOrigin)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = Thing({10.0f, 20.0f, 30.0f});
	registry.Assign<ecs::components::Villager>(villager);
	registry.Assign<ecs::components::WallHug>(villager).goal = {100.0f, 200.0f};
	registry.Destroy(villager);
	EXPECT_EQ(Destination(villager), glm::vec3(0.0f));
	EXPECT_EQ(Destination(entt::entity {0}), glm::vec3(0.0f));
}

TEST_F(ScriptObjectDestination, TheDestinationIsAMapPosition)
{
	// the goal as a map position: whole map units, the height above the land kept apart
	auto& registry = Locator::entitiesRegistry::value();
	const auto animal = Thing({0.0f, 0.0f, 0.0f});
	registry.Assign<ecs::components::Animal>(animal);
	auto& brain = registry.Assign<ecs::components::AnimalBrain>(animal);
	brain.goal = {1.0f, 2.0f};
	brain.goalAltitude = 0.5f;
	const auto destination = ecs::script_destination::Of(std::as_const(registry), animal);
	ASSERT_TRUE(destination.has_value());
	EXPECT_EQ(*destination, (map_coords::MapCoords {map_coords::ToFixed(1.0f), map_coords::ToFixed(2.0f), 0.5f}));
	EXPECT_FALSE(ecs::script_destination::Of(std::as_const(registry), entt::null).has_value());
}
