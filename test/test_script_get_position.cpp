/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// GET_POSITION as a script calls it, on a land whose height is a function of the point: an animal that has come within
// a step of its walk's goal gives that goal on the land there, as a villager gives its destination; an animal still
// on its way, or a thing that does not walk, gives where it is; a thing no longer there gives (0, 0, 0). The goal
// keeps its height above the land, which a script's walk gives it: the walker there gives it back

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <string_view>

#include <LHVM.h>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
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

class ScriptGetPosition: public ::testing::Test
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

	/// GET_POSITION of the thing, the only three values it leaves above the script's own
	glm::vec3 GetPosition(entt::entity thing)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "GET_POSITION"; });
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
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << "GET_POSITION left more than three values";
		return {x, y, z};
	}

	/// An animal standing at `at`, walking to `goal` at `speed` map units a turn
	static entt::entity Animal(glm::vec3 at, glm::vec2 goal, uint16_t speed)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto animal = registry.Create();
		registry.Assign<Transform>(animal, at, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<ecs::components::Animal>(animal);
		auto& brain = registry.Assign<ecs::components::AnimalBrain>(animal);
		brain.goal = goal;
		brain.speed = speed;
		return animal;
	}

	static constexpr int32_t k_ScriptsOwn = 7777;
	/// About a metre a turn
	static constexpr uint16_t k_Speed = 6554;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::terrainSystem> _restoreTerrain;
	chlapi::CHLApi _api;
};
} // namespace

TEST_F(ScriptGetPosition, AnAnimalWithinAStepOfItsGoalGivesTheGoalOnTheLand)
{
	const glm::vec2 goal(100.0f, 200.0f);
	const auto animal = Animal({100.4f, 23.0f, 200.3f}, goal, k_Speed);
	const auto position = GetPosition(animal);
	EXPECT_EQ(position.x, goal.x);
	EXPECT_EQ(position.z, goal.y);
	EXPECT_EQ(position.y, LandAt(goal));
}

TEST_F(ScriptGetPosition, AnAnimalOnItsWayGivesWhereItIs)
{
	const glm::vec3 at(110.0f, 7.0f, 200.0f);
	const auto animal = Animal(at, {100.0f, 200.0f}, k_Speed);
	EXPECT_EQ(GetPosition(animal), at);
}

TEST_F(ScriptGetPosition, AThingThatDoesNotWalkGivesWhereItIs)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto thing = registry.Create();
	const glm::vec3 at(50.0f, 3.0f, 60.0f);
	registry.Assign<Transform>(thing, at, glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_EQ(GetPosition(thing), at);
}

TEST_F(ScriptGetPosition, AThingNoLongerThereGivesTheOrigin)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto thing = registry.Create();
	registry.Destroy(thing);
	EXPECT_EQ(GetPosition(thing), glm::vec3(0.0f));
}

TEST_F(ScriptGetPosition, AVillagerThereGivesItsGoalWithTheGoalsHeightAboveTheLand)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.Create();
	const glm::vec2 goal(100.0f, 200.0f);
	registry.Assign<Transform>(villager, glm::vec3(100.2f, 21.0f, 200.1f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::Villager>(villager);
	auto& wallHug = registry.Assign<ecs::components::WallHug>(villager);
	wallHug.speed = 1.0f;
	// a script sent it to a point at y 0: the goal is that far below the land there
	ecs::walk_goal::Set(villager, wallHug.goal, goal);
	ecs::walk_goal::SetAltitude(villager, -LandAt(goal));
	const auto position = GetPosition(villager);
	EXPECT_EQ(position.x, goal.x);
	EXPECT_EQ(position.z, goal.y);
	EXPECT_EQ(position.y, 0.0f);
	// a walk the game plans has its goal on the land
	ecs::walk_goal::Set(villager, wallHug.goal, goal);
	EXPECT_EQ(GetPosition(villager).y, LandAt(goal));
}

TEST_F(ScriptGetPosition, AnAnimalThereGivesItsGoalWithTheGoalsHeightAboveTheLand)
{
	const glm::vec2 goal(100.0f, 200.0f);
	const auto animal = Animal({100.4f, 23.0f, 200.3f}, goal, k_Speed);
	Locator::entitiesRegistry::value().Get<ecs::components::AnimalBrain>(animal).goalAltitude = 12.5f;
	EXPECT_EQ(GetPosition(animal).y, LandAt(goal) + 12.5f);
}

TEST(WalkGoal, AGoalSetAgainLosesTheHeightAScriptGaveIt)
{
	const test::RestoreService<Locator::entitiesRegistry> restore;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	const auto walker = registry.Create();
	glm::vec2 goal(0.0f);
	EXPECT_EQ(ecs::walk_goal::AltitudeOf(walker), 0.0f);
	ecs::walk_goal::Set(walker, goal, {3.0f, 4.0f});
	ecs::walk_goal::SetAltitude(walker, 7.0f);
	EXPECT_EQ(goal, glm::vec2(3.0f, 4.0f));
	EXPECT_EQ(ecs::walk_goal::AltitudeOf(walker), 7.0f);
	ecs::walk_goal::Set(walker, goal, {5.0f, 6.0f});
	EXPECT_EQ(ecs::walk_goal::AltitudeOf(walker), 0.0f);
	EXPECT_FALSE(registry.AllOf<ecs::components::WalkGoalAltitude>(walker));
}
