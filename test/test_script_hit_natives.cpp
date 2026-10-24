/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scripts' hit natives as a script calls them: GET_HIT_OBJECT, GET_OBJECT_WHICH_HIT, CLEAR_HIT_OBJECT and
// GAME_THING_HIT read and clear the physics' hit record (a physics with a fake world: no land and no files), and the
// objects they give are added as found script things (a fake table). docs/wiki/physics/physics.md#manager

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Locator.h"
#include "support/DynamicsFakes.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
/// The scripts' table of objects, keeping only what was added to it
class AddedThings final: public ecs::systems::ScriptObjectsSystemInterface
{
public:
	bool Register(entt::entity object, bool createdByScript) override
	{
		added.emplace_back(object, createdByScript);
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

	std::vector<std::pair<entt::entity, bool>> added;
};

class ScriptHitNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the natives log to the scripts' logger: a null sink, as the other script tests
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		// entity 0 is "no thing" for a script id: the test's first entity must not be a scripted one
		registry.Create();
		Locator::vm::emplace<lhvm::LHVM>();
		_dynamics = &static_cast<ecs::systems::DynamicsSystem&>(Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>(
		    std::make_unique<test::FakeDynamicsWorld>(registry)));
		_table = &static_cast<AddedThings&>(Locator::scriptObjects::emplace<AddedThings>());
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// The native as a script calls it, with the object it pops pushed first when it takes one
	void Call(std::string_view name, std::optional<uint32_t> object = std::nullopt)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [name](const lhvm::NativeFunction& f) { return std::string_view(f.name) == name; });
		ASSERT_NE(native, table.end()) << name;
		ASSERT_EQ(native->stackIn, object.has_value() ? 1 : 0) << name;
		if (object.has_value())
		{
			Locator::vm::value().Pusho(*object);
		}
		native->impl();
	}
	uint32_t CallForObject(std::string_view name)
	{
		Call(name);
		return Locator::vm::value().Pop().uintVal;
	}
	bool CallThingHit(uint32_t object)
	{
		Call("GAME_THING_HIT", object);
		return Locator::vm::value().Pop().intVal != 0;
	}

	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::dynamicsSystem> _restoreDynamics;
	test::RestoreService<Locator::scriptObjects> _restoreScriptObjects;
	chlapi::CHLApi _api;
	ecs::systems::DynamicsSystem* _dynamics {nullptr};
	AddedThings* _table {nullptr};
};
} // namespace

TEST_F(ScriptHitNatives, NothingRecordedGivesNoObjectAndNoThingWasHit)
{
	const auto thing = Reg().Create();
	EXPECT_EQ(CallForObject("GET_HIT_OBJECT"), 0u);
	EXPECT_EQ(CallForObject("GET_OBJECT_WHICH_HIT"), 0u);
	EXPECT_FALSE(CallThingHit(static_cast<uint32_t>(thing)));
	EXPECT_TRUE(_table->added.empty());
}

TEST_F(ScriptHitNatives, TheGettersGiveTheRecordAsFoundScriptThings)
{
	const auto hit = Reg().Create();
	const auto hitter = Reg().Create();
	_dynamics->RecordHit(hit, hitter);
	EXPECT_EQ(CallForObject("GET_HIT_OBJECT"), static_cast<uint32_t>(hit));
	EXPECT_EQ(CallForObject("GET_OBJECT_WHICH_HIT"), static_cast<uint32_t>(hitter));
	using Added = std::vector<std::pair<entt::entity, bool>>;
	EXPECT_EQ(_table->added, (Added {{hit, false}, {hitter, false}}));
}

TEST_F(ScriptHitNatives, ThingHitIsTrueOnlyForTheThingHit)
{
	const auto hit = Reg().Create();
	const auto hitter = Reg().Create();
	_dynamics->RecordHit(hit, hitter);
	EXPECT_TRUE(CallThingHit(static_cast<uint32_t>(hit)));
	EXPECT_FALSE(CallThingHit(static_cast<uint32_t>(hitter))); // what hit it is not the thing hit
	EXPECT_FALSE(CallThingHit(0));

	// an object gone is false, even when it is the one kept
	Reg().Destroy(hit);
	EXPECT_FALSE(CallThingHit(static_cast<uint32_t>(hit)));
	EXPECT_TRUE(_table->added.empty());
}

TEST_F(ScriptHitNatives, ClearForgetsBoth)
{
	const auto hit = Reg().Create();
	const auto hitter = Reg().Create();
	_dynamics->RecordHit(hit, hitter);
	Call("CLEAR_HIT_OBJECT");
	EXPECT_EQ(CallForObject("GET_HIT_OBJECT"), 0u);
	EXPECT_EQ(CallForObject("GET_OBJECT_WHICH_HIT"), 0u);
	EXPECT_FALSE(CallThingHit(static_cast<uint32_t>(hit)));
}
