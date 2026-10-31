/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scripts' DANCE_CREATE as a script calls it: what it is danced about, the dance's row, the point and the time, in
// the order the script pushes them; it leaves exactly one value, the dance (a fake dance system makes it). The point
// becomes a map position as the game makes one, its height above the land kept (a fake land gives the heights)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <string_view>
#include <vector>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "CHLApi.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Locator.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
/// The dance system as far as the native asks it: each dance a new entity, unless told to make none
class MadeDances final: public ecs::systems::DanceSystemInterface
{
public:
	entt::entity Create(DanceInfo type, const map_coords::MapCoords& place, entt::entity owner, uint32_t duration,
	                    bool madeByScript, entt::entity town) override
	{
		made.push_back({type, place, owner, duration, madeByScript, town});
		return refuse ? entt::entity {entt::null} : Locator::entitiesRegistry::value().Create();
	}
	void Destroy(entt::entity /*dance*/) override {}
	void ProcessTurn() override {}

	struct Made
	{
		DanceInfo type;
		map_coords::MapCoords place;
		entt::entity owner;
		uint32_t duration;
		bool madeByScript;
		entt::entity town;
	};
	std::vector<Made> made;
	bool refuse {false};
};

/// The scripts' table of objects, keeping what it is given
class KeptThings final: public ecs::systems::ScriptObjectsSystemInterface
{
public:
	bool Register(entt::entity object, bool createdByScript) override
	{
		registered.push_back({object, createdByScript});
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

	struct Registered
	{
		entt::entity object;
		bool createdByScript;
	};
	std::vector<Registered> registered;
};

class ScriptDanceNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		// entity 0 is "no thing" for a script id
		registry.Create();
		Locator::vm::emplace<lhvm::LHVM>();
		_dances = &static_cast<MadeDances&>(Locator::danceSystem::emplace<MadeDances>());
		_things = &static_cast<KeptThings&>(Locator::scriptObjects::emplace<KeptThings>());
		// the land 4.25 high everywhere
		_land = &static_cast<test::HeightFieldIsland&>(
		    Locator::terrainSystem::emplace<test::HeightFieldIsland>([](glm::vec2) { return 4.25f; }));
	}

	/// DANCE_CREATE as a script calls it, its arguments pushed first, on top of a value of the script's own; the dance
	/// it gives, which must be the only value it leaves above the script's
	uint32_t DanceCreate(uint32_t object, int32_t type, glm::vec3 at, float time)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "DANCE_CREATE"; });
		EXPECT_NE(native, table.end());
		if (native == table.end())
		{
			return 0;
		}
		auto& vm = Locator::vm::value();
		vm.Pushi(k_ScriptsOwn);
		vm.Pusho(object);
		vm.Pushi(type);
		vm.Pushv(at.x);
		vm.Pushv(at.y);
		vm.Pushv(at.z);
		vm.Pushf(time);
		native->impl();
		const auto dance = vm.Pop().uintVal;
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << "DANCE_CREATE left more than one value";
		return dance;
	}

	static constexpr int32_t k_ScriptsOwn = 7777;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::danceSystem> _restoreDances;
	test::RestoreService<Locator::scriptObjects> _restoreScriptObjects;
	test::RestoreService<Locator::terrainSystem> _restoreTerrain;
	chlapi::CHLApi _api;
	MadeDances* _dances {nullptr};
	KeptThings* _things {nullptr};
	test::HeightFieldIsland* _land {nullptr};
};
} // namespace

TEST_F(ScriptDanceNatives, DanceCreateTakesItsArgumentsInTheScriptsOrder)
{
	const auto villager = Locator::entitiesRegistry::value().Create();
	const auto dance = DanceCreate(static_cast<uint32_t>(villager), 14, glm::vec3(1641.9f, 25.5f, 2314.7f), 30.9f);
	ASSERT_EQ(_dances->made.size(), 1u);
	const auto& made = _dances->made.front();
	EXPECT_EQ(made.type, DanceInfo::NewDanceAroundPerson);
	// x and z times 6553.6, each product rounded to a float, then truncated (worked out in single precision)
	EXPECT_EQ(made.place.x, 10760356);
	EXPECT_EQ(made.place.z, 15169618);
	// the land asked once, at the truncated position; the point's height above it kept: 25.5 - 4.25
	ASSERT_EQ(_land->asked.size(), 1u);
	EXPECT_EQ(_land->asked.front(), map_coords::ToMetres(made.place));
	EXPECT_EQ(made.place.altitude, 21.25f);
	EXPECT_EQ(made.owner, villager);
	// The time in whole turns, cut short; made by a script
	EXPECT_EQ(made.duration, 30u);
	// made for no town: its footpath comes from the town nearest it
	EXPECT_TRUE(made.town == entt::null);
	EXPECT_TRUE(made.madeByScript);
	// The dance is a thing the script made, and is given back
	ASSERT_EQ(_things->registered.size(), 1u);
	EXPECT_EQ(dance, static_cast<uint32_t>(_things->registered.front().object));
	EXPECT_TRUE(_things->registered.front().createdByScript);
}

TEST_F(ScriptDanceNatives, ADanceAboutNothingIsMadeAllTheSame)
{
	const auto dance = DanceCreate(0, 6, glm::vec3(10.0f, 0.0f, 20.0f), 0.0f);
	ASSERT_EQ(_dances->made.size(), 1u);
	EXPECT_EQ(_dances->made.front().owner, entt::entity {entt::null});
	EXPECT_EQ(_dances->made.front().duration, 0u);
	EXPECT_NE(dance, 0u);
}

TEST_F(ScriptDanceNatives, NoDanceMadeGivesNothingAndLeavesOneValue)
{
	_dances->refuse = true;
	EXPECT_EQ(DanceCreate(0, 99, glm::vec3(0.0f), 5.0f), 0u);
	EXPECT_TRUE(_things->registered.empty());
}
