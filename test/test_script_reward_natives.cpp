/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scripts' reward natives as a script calls them: CREATE_REWARD and CREATE_REWARD_IN_TOWN each take their
// arguments and leave exactly one value, the chest they give (a fake reward system makes it); IS_ACTIVE on a chest
// asks the reward system whether it was tapped open. CREATE and CREATE_WITH_ANGLE_AND_SCALE of a reward give a chest
// of nobody's, with no town, from the sky unless the angle is a whole radian or more; of a highlight, which has a create
// of its own (CREATE_HIGHLIGHT), they make nothing and give 0

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <memory>
#include <optional>
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
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Town.h"
#include "ECS/Registry.h"
#include "ECS/RewardRules.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
/// The reward system as far as the natives ask it: each chest a new entity
class MadeChests final: public ecs::systems::RewardSystemInterface
{
public:
	entt::entity Create(glm::vec3 position, RewardObjectInfo type, std::optional<PlayerNames> player, entt::entity town,
	                    bool fromSky) override
	{
		const auto chest = Locator::entitiesRegistry::value().Create();
		made.push_back({chest, town, position, type, player, fromSky});
		return chest;
	}
	bool Tap(entt::entity /*chest*/, PlayerNames /*player*/) override { return false; }
	[[nodiscard]] bool IsOpened(entt::entity chest) const override { return std::ranges::find(opened, chest) != opened.end(); }
	void Update(float /*milliseconds*/) override {}
	void ProcessTurn() override {}
	void CollectDrawFrame(particles::draw::Frame& /*frame*/) const override {}
	[[nodiscard]] const std::vector<ecs::reward::DustDraw>& GetDustSprites() const override { return _dust; }

	struct Made
	{
		entt::entity chest;
		entt::entity town;
		glm::vec3 position;
		RewardObjectInfo type;
		std::optional<PlayerNames> player;
		bool fromSky;
	};
	std::vector<Made> made;
	/// The chests that answer as tapped open
	std::vector<entt::entity> opened;

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

	/// IS_ACTIVE as a script calls it on a thing: what it answers, the only value it leaves above the script's
	bool IsActive(entt::entity thing)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "IS_ACTIVE"; });
		EXPECT_NE(native, table.end());
		if (native == table.end())
		{
			return false;
		}
		auto& vm = Locator::vm::value();
		vm.Pushi(k_ScriptsOwn);
		vm.Pusho(static_cast<uint32_t>(thing));
		native->impl();
		const bool active = vm.Pop().intVal != 0;
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << "IS_ACTIVE left more than one value";
		return active;
	}

	/// CREATE (no angle) or, with an angle in degrees, CREATE_WITH_ANGLE_AND_SCALE of a type as a script calls it: the
	/// angle and the scale first, then the type, the sub-type and the point. The thing it gives, the only value it leaves
	/// above the script's. The create reads the info tables, so a blank set goes in first
	uint32_t CallCreate(int32_t type, int32_t subtype, glm::vec3 at, std::optional<float> degrees = std::nullopt)
	{
		if (_info == nullptr)
		{
			_info = std::make_unique<InfoConstants>();
			Locator::infoConstants::emplace(*_info);
		}
		const std::string_view name = degrees.has_value() ? "CREATE_WITH_ANGLE_AND_SCALE" : "CREATE";
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
		if (degrees.has_value())
		{
			vm.Pushf(*degrees);
			vm.Pushf(2.0f);
		}
		vm.Pushi(type);
		vm.Pushi(subtype);
		vm.Pushv(at.x);
		vm.Pushv(at.y);
		vm.Pushv(at.z);
		native->impl();
		const auto thing = vm.Pop().uintVal;
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << name << " left more than one value";
		return thing;
	}

	static constexpr int32_t k_ScriptsOwn = 7777;
	static constexpr int32_t k_RewardType = 7;
	static constexpr int32_t k_HighlightType = 37;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::rewardSystem> _restoreRewards;
	test::RestoreService<Locator::scriptObjects> _restoreScriptObjects;
	chlapi::CHLApi _api;
	MadeChests* _chests {nullptr};
	std::unique_ptr<InfoConstants> _info;
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

TEST_F(ScriptRewardNatives, IsActiveOfAChestIsWhetherItWasTappedOpen)
{
	const auto chest = static_cast<entt::entity>(Call("CREATE_REWARD", {}, 3, glm::vec3(0.0f), false));
	// the fake's chest is a bare entity: it is a chest once it has the component
	Locator::entitiesRegistry::value().Assign<ecs::components::Reward>(chest);
	EXPECT_FALSE(IsActive(chest));
	_chests->opened.push_back(chest);
	EXPECT_TRUE(IsActive(chest));
}

TEST_F(ScriptRewardNatives, AnInvalidRewardGivesNothingAndLeavesOneValue)
{
	EXPECT_EQ(Call("CREATE_REWARD", {}, 61, glm::vec3(0.0f), false), 0u);
	EXPECT_TRUE(_chests->made.empty());
}

TEST_F(ScriptRewardNatives, CreateOfARewardDropsNobodysChestFromTheSky)
{
	const auto chest = CallCreate(k_RewardType, 12, glm::vec3(100.3f, 5.0f, 200.7f));
	ASSERT_EQ(_chests->made.size(), 1u);
	const auto& made = _chests->made.front();
	EXPECT_EQ(chest, static_cast<uint32_t>(made.chest));
	EXPECT_EQ(made.type, static_cast<RewardObjectInfo>(12));
	// no player: no hand can ever tap it open; and no town
	EXPECT_FALSE(made.player.has_value());
	EXPECT_EQ(made.town, entt::entity {entt::null});
	// the angle of a plain create is 0: it falls from the sky, onto the point on the map's grid
	EXPECT_TRUE(made.fromSky);
	EXPECT_EQ(made.position.x, map_coords::Quantise(100.3f));
	EXPECT_EQ(made.position.z, map_coords::Quantise(200.7f));
}

TEST_F(ScriptRewardNatives, CreateWithAnAngleOfAWholeRadianPutsTheRewardOnTheLand)
{
	// 30 degrees truncates to no whole radian: from the sky; 90 and -60 to one either way: on the land
	CallCreate(k_RewardType, 3, glm::vec3(10.0f, 0.0f, 20.0f), 30.0f);
	CallCreate(k_RewardType, 3, glm::vec3(10.0f, 0.0f, 20.0f), 90.0f);
	CallCreate(k_RewardType, 3, glm::vec3(10.0f, 0.0f, 20.0f), -60.0f);
	ASSERT_EQ(_chests->made.size(), 3u);
	EXPECT_TRUE(_chests->made[0].fromSky);
	EXPECT_FALSE(_chests->made[1].fromSky);
	EXPECT_FALSE(_chests->made[2].fromSky);
}

TEST_F(ScriptRewardNatives, CreateOfARewardPastTheTableGivesNothing)
{
	// the reward table's 61 rows, row 0 among them, are the only kinds; a create of 5000 is refused for a reward too
	EXPECT_EQ(CallCreate(k_RewardType, 61, glm::vec3(0.0f)), 0u);
	EXPECT_EQ(CallCreate(k_RewardType, 5000, glm::vec3(0.0f)), 0u);
	EXPECT_TRUE(_chests->made.empty());
	EXPECT_NE(CallCreate(k_RewardType, 0, glm::vec3(0.0f)), 0u);
}

TEST(RewardRules, ScriptCreateFallsFromTheSkyUnlessTheAngleIsAWholeRadian)
{
	// the truncation towards zero of the angle, as the exe's own create gives it (0.9999 -> 0, 1.0 -> 1, -1.2 -> -1)
	EXPECT_TRUE(ecs::reward::ScriptCreateFallsFromSky(0.0f));
	EXPECT_TRUE(ecs::reward::ScriptCreateFallsFromSky(0.9999f));
	EXPECT_TRUE(ecs::reward::ScriptCreateFallsFromSky(-0.9999f));
	EXPECT_FALSE(ecs::reward::ScriptCreateFallsFromSky(1.0f));
	EXPECT_FALSE(ecs::reward::ScriptCreateFallsFromSky(-1.2f));
	EXPECT_FALSE(ecs::reward::ScriptCreateFallsFromSky(3.14159f));
}

TEST_F(ScriptRewardNatives, CreateOfAHighlightMakesNothing)
{
	// a scroll is made only by its own native: CREATE and CREATE_WITH_ANGLE_AND_SCALE of one make nothing and give 0
	EXPECT_EQ(CallCreate(k_HighlightType, 1, glm::vec3(10.0f, 0.0f, 20.0f)), 0u);
	EXPECT_EQ(CallCreate(k_HighlightType, 0, glm::vec3(10.0f, 0.0f, 20.0f), 90.0f), 0u);
	EXPECT_EQ(Locator::entitiesRegistry::value().Size<ecs::components::ScriptHighlight>(), 0u);
	EXPECT_TRUE(_chests->made.empty());
}
