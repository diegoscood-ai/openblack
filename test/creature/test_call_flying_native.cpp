/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// CALL_FLYING, as a script calls it: the first thing of the physics, in the physics list's order, of the type within
// the radius of the point, added to the script's things as a found one. The bodies are the creature class's empty ones,
// so they need no model. docs/wiki/scripts/script-commands.md (CALL_FLYING 383)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <memory>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/ScriptTypes.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Locator.h"
#include "Physics/Body.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "creature/CreatureSystemWorld.h"
#include "support/DynamicsFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The class hooks of a thing whose body needs no model: the creature class's own body, an empty one
class EmptyBodyHooks final: public ecs::systems::PhysicsClassHooks
{
public:
	bool SetUpBody([[maybe_unused]] ecs::systems::DynamicsSystemInterface& dynamics, [[maybe_unused]] entt::entity object,
	               [[maybe_unused]] openblack::physics::Body& body) override
	{
		return true;
	}
};

/// The scripts' table of objects, as far as the native reaches it: what each Register was given
class RegisteredThings final: public ecs::systems::ScriptObjectsSystemInterface
{
public:
	bool Register(entt::entity object, bool createdByScript) override
	{
		registered.emplace_back(object, createdByScript);
		return true;
	}
	void EnterNative(uint32_t /*native*/) override {}
	entt::entity Fetch(entt::entity object) override { return object; }
	void AddReference(entt::entity /*object*/) override {}
	void RemoveReference(entt::entity /*object*/) override {}
	void ReleaseFromScript(entt::entity /*object*/) override {}
	void Replace(entt::entity /*from*/, entt::entity /*to*/) override {}
	void Reset() override {}
	void ReleaseUnreferenced() override {}
	bool Disband(entt::entity /*container*/) override { return false; }
	[[nodiscard]] std::vector<ecs::systems::ScriptObjectPlace> Places() const override { return {}; }
	[[nodiscard]] uint32_t TimesFull() const override { return 0; }
	void ClearForNewLand() override {}

	std::vector<std::pair<entt::entity, bool>> registered;
};

class CallFlyingNative: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::vm::emplace<lhvm::LHVM>();
		Locator::chlapi::emplace<chlapi::CHLApi>();
		_things = &static_cast<RegisteredThings&>(Locator::scriptObjects::emplace<RegisteredThings>());
		auto& dynamics = Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>(
		    std::make_unique<test::FakeDynamicsWorld>(test::creature_world::World::Registry()));
		_dynamics = &static_cast<ecs::systems::DynamicsSystem&>(dynamics);
		_dynamics->SetClassHooks(std::make_unique<EmptyBodyHooks>());
		// a script's object 0 is no thing, so the first entity is not one the tests name
		std::ignore = test::creature_world::World::Registry().Create();
	}
	void TearDown() override
	{
		for (const auto thing : _flying)
		{
			_dynamics->RemoveObject(thing);
		}
		test::ResetMapAndVillagerDefaults();
	}

	/// A thing of the creature class flying from a point; marked for deletion, so that taking it out of the physics puts
	/// nothing back in the map's cells
	entt::entity Fly(const glm::vec3& at)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto thing = registry.Create();
		registry.Assign<Transform>(thing, at, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<MobileObject>(thing);
		registry.Assign<Creature>(thing);
		registry.Assign<Unavailable>(thing);
		EXPECT_NE(_dynamics->AddObject(thing, glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(0.0f)), nullptr);
		_flying.push_back(thing);
		return thing;
	}

	/// The native's answer: pushed the type, the sub-type, the point, the radius, then excludingScripted
	uint32_t Call(script::ObjectType type, const glm::vec3& at, float radius)
	{
		const auto& table = Locator::chlapi::value().GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "CALL_FLYING"; });
		EXPECT_NE(native, table.end());
		EXPECT_EQ(native->stackIn, 7);
		EXPECT_EQ(native->stackOut, 1u);
		auto& vm = Locator::vm::value();
		vm.Pushi(77);
		vm.Pushi(static_cast<int32_t>(type));
		vm.Pushi(static_cast<int32_t>(ecs::script_type::k_AnySubtype));
		vm.Pushv(at.x);
		vm.Pushv(at.y);
		vm.Pushv(at.z);
		vm.Pushf(radius);
		vm.Pushb(false);
		native->impl();
		const auto answer = vm.Pop().uintVal;
		EXPECT_EQ(vm.Pop().intVal, 77);
		return answer;
	}

	static uint32_t Id(entt::entity entity) { return static_cast<uint32_t>(entt::to_integral(entity)); }

	const test::ScopedWorldSystems _worldSystems;
	test::creature_world::World _world;
	const test::RestoreService<Locator::vm> _restoreVm;
	const test::RestoreService<Locator::scriptObjects> _restoreThings;
	const test::RestoreService<Locator::dynamicsSystem> _restoreDynamics;
	const test::RestoreService<Locator::chlapi> _restoreApi;
	RegisteredThings* _things {nullptr};
	ecs::systems::DynamicsSystem* _dynamics {nullptr};
	std::vector<entt::entity> _flying;
};
} // namespace

TEST_F(CallFlyingNative, TheFirstFlyingThingOfTheTypeWithinTheRadiusIsFound)
{
	const auto first = Fly(glm::vec3(110.0f, 0.0f, 100.0f));
	const auto second = Fly(glm::vec3(105.0f, 0.0f, 100.0f));
	// both within the radius: the first in the physics list, not the nearer
	EXPECT_EQ(Call(script::ObjectType::Creature, glm::vec3(100.0f, 0.0f, 100.0f), 20.0f), Id(first));
	ASSERT_EQ(_things->registered.size(), 1u);
	EXPECT_EQ(_things->registered.back(), std::make_pair(first, false));
	// only the second within it
	EXPECT_EQ(Call(script::ObjectType::Creature, glm::vec3(100.0f, 0.0f, 100.0f), 7.0f), Id(second));
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 0u);
}

TEST_F(CallFlyingNative, NoneOfTheTypeWithinTheRadiusIsNoThing)
{
	std::ignore = Fly(glm::vec3(110.0f, 0.0f, 100.0f));
	EXPECT_EQ(Call(script::ObjectType::Creature, glm::vec3(100.0f, 0.0f, 100.0f), 5.0f), 0u);
	EXPECT_EQ(Call(script::ObjectType::Villager, glm::vec3(100.0f, 0.0f, 100.0f), 50.0f), 0u);
	EXPECT_TRUE(_things->registered.empty());
}
