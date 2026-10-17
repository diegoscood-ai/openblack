/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scripts' reward natives as a script calls them: CREATE_REWARD and CREATE_REWARD_IN_TOWN each take their
// arguments and leave exactly one value, the chest they give (a fake reward system makes it)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Components/Town.h"
#include "ECS/Registry.h"
#include "ECS/RewardRules.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
/// The reward system as far as the natives ask it: each chest a new entity
class MadeChests final: public ecs::systems::RewardSystemInterface
{
public:
	entt::entity Create(glm::vec3 /*position*/, RewardObjectInfo /*type*/, std::optional<PlayerNames> /*player*/,
	                    entt::entity town, bool /*fromSky*/) override
	{
		const auto chest = Locator::entitiesRegistry::value().Create();
		made.push_back({chest, town});
		return chest;
	}
	void Update(float /*milliseconds*/) override {}
	void ProcessTurn() override {}
	void CollectDrawFrame(particles::draw::Frame& /*frame*/) const override {}
	[[nodiscard]] const std::vector<ecs::reward::DustDraw>& GetDustSprites() const override { return _dust; }

	struct Made
	{
		entt::entity chest;
		entt::entity town;
	};
	std::vector<Made> made;

private:
	std::vector<ecs::reward::DustDraw> _dust;
};

/// The scripts' table of objects, keeping nothing
class NoThings final: public ecs::systems::ScriptObjectsSystemInterface
{
public:
	bool Register(entt::entity /*object*/, bool /*createdByScript*/) override { return true; }
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
};

class ScriptRewardNatives: public ::testing::Test
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
		_chests = &static_cast<MadeChests&>(Locator::rewardSystem::emplace<MadeChests>());
		Locator::scriptObjects::emplace<NoThings>();
	}

	/// The native as a script calls it, its arguments pushed first, on top of a value of the script's own; the chest
	/// it gives, which must be the only value it leaves above the script's
	uint32_t Call(std::string_view name, const std::vector<uint32_t>& objects, int32_t type, glm::vec3 at, bool fromSky)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [name](const lhvm::NativeFunction& f) { return std::string_view(f.name) == name; });
		EXPECT_NE(native, table.end()) << name;
		if (native == table.end())
		{
			return 0;
		}
		auto& vm = Locator::vm::value();
		vm.Pushi(k_ScriptsOwn);
		vm.Pushi(type);
		for (const auto object : objects)
		{
			vm.Pusho(object);
		}
		vm.Pushv(at.x);
		vm.Pushv(at.y);
		vm.Pushv(at.z);
		vm.Pushi(fromSky ? 1 : 0);
		native->impl();
		const auto chest = vm.Pop().uintVal;
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << name << " left more than one value";
		return chest;
	}

	static constexpr int32_t k_ScriptsOwn = 7777;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::rewardSystem> _restoreRewards;
	test::RestoreService<Locator::scriptObjects> _restoreScriptObjects;
	chlapi::CHLApi _api;
	MadeChests* _chests {nullptr};
};
} // namespace

TEST_F(ScriptRewardNatives, CreateRewardLeavesOnlyTheChest)
{
	const auto chest = Call("CREATE_REWARD", {}, 3, glm::vec3(10.0f, 0.0f, 20.0f), false);
	ASSERT_EQ(_chests->made.size(), 1u);
	EXPECT_EQ(chest, static_cast<uint32_t>(_chests->made.front().chest));
	EXPECT_EQ(_chests->made.front().town, entt::entity {entt::null});
}

TEST_F(ScriptRewardNatives, CreateRewardInTownLeavesOnlyTheChest)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = registry.Create();
	registry.Assign<ecs::components::Town>(town);
	const auto chest = Call("CREATE_REWARD_IN_TOWN", {static_cast<uint32_t>(town)}, 4, glm::vec3(10.0f, 0.0f, 20.0f), true);
	ASSERT_EQ(_chests->made.size(), 1u);
	EXPECT_EQ(chest, static_cast<uint32_t>(_chests->made.front().chest));
	EXPECT_EQ(_chests->made.front().town, town);
}

TEST_F(ScriptRewardNatives, AnInvalidRewardGivesNothingAndLeavesOneValue)
{
	EXPECT_EQ(Call("CREATE_REWARD", {}, 61, glm::vec3(0.0f), false), 0u);
	EXPECT_TRUE(_chests->made.empty());
}
