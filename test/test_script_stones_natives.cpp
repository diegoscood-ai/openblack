/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The natives the Throwing Stones challenge asks of its boulder and of the stones the player throws at it: IS_OF_TYPE
// as a script calls it (a fake table of script things and a synthetic registry, no game data), and the boulder's
// distance from its marker along the ground. docs/wiki/scripts/land1-intro.md#script-look-ups-call-and-call_near

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <memory>
#include <string_view>
#include <vector>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/VillagerScriptRules.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
/// Every object a script names is itself, as the scripts' table gives back what it holds
class StonesScriptObjects final: public ecs::systems::ScriptObjectsSystemInterface
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

class ScriptStonesNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the natives report a bad call to the scripts' log
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		// entity 0 is "no thing" for a script id: the test's first entity must not be a scripted one
		registry.Create();
		Locator::vm::emplace<lhvm::LHVM>();
		Locator::scriptObjects::emplace<StonesScriptObjects>();
		// the info rows of the rocks' mobile statics say they are rocks (the Rock class's mobile type)
		auto info = std::make_unique<InfoConstants>();
		for (const auto rock : {MobileStaticInfo::Rock, MobileStaticInfo::Boulder3Lime})
		{
			info->mobileStatic.at(static_cast<size_t>(rock)).mobileType = static_cast<MobileStaticInfo>(2);
		}
		Locator::infoConstants::reset(info.release());
	}

	/// A script's "OBJECT type TYPE SUBTYPE": the object, the type and the subtype pushed, IS_OF_TYPE called
	[[nodiscard]] bool IsOfType(entt::entity object, int32_t type, int32_t subtype)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == std::string_view("IS_OF_TYPE"); });
		EXPECT_NE(native, table.end());
		if (native == table.end())
		{
			return false;
		}
		auto& vm = Locator::vm::value();
		vm.Pusho(static_cast<uint32_t>(object));
		vm.Pushi(type);
		vm.Pushi(subtype);
		native->impl();
		return vm.Pop().intVal != 0;
	}
	[[nodiscard]] bool IsOfType(entt::entity object, script::ObjectType type, int32_t subtype)
	{
		return IsOfType(object, static_cast<int32_t>(type), subtype);
	}

	/// A mobile static of a kind, a rock by its info row when it is of the rocks' kind
	[[nodiscard]] static entt::entity MobileStaticOf(MobileStaticInfo info)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<ecs::components::MobileStatic>(entity, info);
		return entity;
	}

	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::scriptObjects> _restoreScriptObjects;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	chlapi::CHLApi _api;
};
} // namespace

// The fisherman watches only what the player holds that is a rock of the rocks' kind; the subtype 5000 is any
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptStonesNatives, IsOfTypeMatchesTheTypeAndTheSubtypeOrAny)
{
	using script::ObjectType;
	const auto stone = MobileStaticOf(MobileStaticInfo::Rock);
	const auto rock = static_cast<int32_t>(MobileStaticInfo::Rock);
	EXPECT_TRUE(IsOfType(stone, ObjectType::Rock, rock));
	EXPECT_TRUE(IsOfType(stone, ObjectType::Rock, 5000));
	EXPECT_FALSE(IsOfType(stone, ObjectType::Rock, static_cast<int32_t>(MobileStaticInfo::Boulder3Lime)));
	EXPECT_FALSE(IsOfType(stone, ObjectType::MobileStatic, rock));

	const auto fence = MobileStaticOf(MobileStaticInfo::CeltFenceShort);
	EXPECT_TRUE(IsOfType(fence, ObjectType::MobileStatic, 5000));
	EXPECT_TRUE(IsOfType(fence, ObjectType::MobileStatic, static_cast<int32_t>(MobileStaticInfo::CeltFenceShort)));
	EXPECT_FALSE(IsOfType(fence, ObjectType::Rock, 5000));
}

// Types out of range and things that are gone are never of a type
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptStonesNatives, IsOfTypeIsFalseForABadTypeOrNoThing)
{
	const auto stone = MobileStaticOf(MobileStaticInfo::Rock);
	EXPECT_FALSE(IsOfType(stone, 0, 5000));
	EXPECT_FALSE(IsOfType(stone, 42, 5000));
	EXPECT_FALSE(IsOfType(stone, -1, 5000));
	// A script's object 0 is none
	EXPECT_FALSE(IsOfType(static_cast<entt::entity>(0), script::ObjectType::Rock, 5000));
	// A thing no longer in the world
	EXPECT_FALSE(IsOfType(static_cast<entt::entity>(1234), script::ObjectType::Rock, 5000));
	// Each call leaves only its answer on the stack
	EXPECT_TRUE(IsOfType(stone, script::ObjectType::Rock, 5000));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptStonesNatives, IsOfTypeTakesThreeAndGivesOne)
{
	const auto& table = _api.GetFunctionsTable();
	ASSERT_GT(table.size(), 192u);
	EXPECT_EQ(std::string_view(table[192].name), std::string_view("IS_OF_TYPE"));
	EXPECT_EQ(table[192].stackIn, 3);
	EXPECT_EQ(table[192].stackOut, 1u);
}

// The challenge waits while "[Rock1] near [Rock1Pos] radius 2". The boulder stands where the land's script puts it,
// (2030.81, 2325.50), 10.18 above the land; the marker is at (2031.129, 12.272, 2327.405). The game measures along the
// ground only, so the boulder is 1.9316 from the marker however high either stands (1.975 counting heights), and it
// is on its pillar until something moves it 0.07 across the ground
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptStonesBoulder, TheBoulderIsNearItsMarkerAlongTheGround)
{
	const glm::vec3 marker {2031.129f, 12.272f, 2327.405f};
	const glm::vec3 boulder {2030.81f, 1.675f + 10.184691f, 2325.50f};
	const float distance = ecs::villager_script_rules::ScriptDistance(boulder, marker);
	// Within the rounding of single-precision positions this far from the map's corner
	EXPECT_NEAR(distance, 1.9316f, 0.001f);
	EXPECT_LT(distance, 2.0f);
	// Heights don't count
	EXPECT_FLOAT_EQ(ecs::villager_script_rules::ScriptDistance({boulder.x, 100.0f, boulder.z}, marker), distance);
	// Knocked 0.07 further across the ground, it is off
	EXPECT_GT(ecs::villager_script_rules::ScriptDistance({boulder.x, boulder.y, boulder.z - 0.07f}, marker), 2.0f);
}
