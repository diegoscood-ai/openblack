/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scripts' query natives as a script calls them: each takes its arguments and leaves exactly one value, and a
// thing it finds comes into the script's table as a found thing, not one the script created. SET_FOCUS, which leaves
// nothing, turns a thing to face a point

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <functional>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/ObjectMatrix.h"
#include "CHLApi.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/HandClicked.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ScriptSetPosition.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Enums.h"
#include "Locator.h"
#include "creature/CreatureSystemFakes.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The scripts' table of objects, as far as the natives reach it: what each Register was given
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

class ScriptQueryNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		// entity 0 is "no thing" for a script id
		registry.Create();
		Locator::vm::emplace<lhvm::LHVM>();
		_things = &static_cast<RegisteredThings&>(Locator::scriptObjects::emplace<RegisteredThings>());
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// The native as a script calls it: `push` pushes its arguments on top of a value of the script's own; the value
	/// it gives, which must be the only one it leaves above the script's
	lhvm::VMValue Call(std::string_view name, const std::function<void(lhvm::LHVM&)>& push)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [name](const lhvm::NativeFunction& f) { return std::string_view(f.name) == name; });
		EXPECT_NE(native, table.end()) << name;
		if (native == table.end())
		{
			return {};
		}
		auto& vm = Locator::vm::value();
		vm.Pushi(k_ScriptsOwn);
		push(vm);
		native->impl();
		const auto given = vm.Pop();
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << name << " left more than one value";
		return given;
	}

	/// A native that leaves no value, as a script calls it: nothing of the script's own is taken
	void CallLeavingNothing(std::string_view name, const std::function<void(lhvm::LHVM&)>& push)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [name](const lhvm::NativeFunction& f) { return std::string_view(f.name) == name; });
		ASSERT_NE(native, table.end()) << name;
		auto& vm = Locator::vm::value();
		vm.Pushi(k_ScriptsOwn);
		push(vm);
		native->impl();
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << name << " took or left a value";
	}

	void SetFocus(entt::entity thing, const glm::vec3& at)
	{
		CallLeavingNothing("SET_FOCUS", [thing, at](lhvm::LHVM& vm) {
			vm.Pusho(static_cast<uint32_t>(thing));
			vm.Pushv(at.x);
			vm.Pushv(at.y);
			vm.Pushv(at.z);
		});
	}

	/// A town of key `key`, owned by `owner`, joined its owner's list `stamp`-th, with the script number `scriptId`
	static entt::entity MakeTown(uint32_t key, PlayerNames owner, uint32_t stamp, uint32_t scriptId)
	{
		const auto e = Reg().Create();
		auto& t = Reg().Assign<Town>(e, key);
		t.owner = owner;
		t.ownerListStamp = stamp;
		t.scriptId = scriptId;
		Reg().Context().towns[key] = e;
		return e;
	}

	uint32_t TownWithId(float id)
	{
		return Call("GET_TOWN_WITH_ID", [id](lhvm::LHVM& vm) { vm.Pushf(id); }).uintVal;
	}

	float BeliefForPlayer(entt::entity thing, float player)
	{
		return Call("BELIEF_FOR_PLAYER",
		            [thing, player](lhvm::LHVM& vm) {
			            vm.Pusho(static_cast<uint32_t>(thing));
			            vm.Pushf(player);
		            })
		    .floatVal;
	}

	bool IsActive(entt::entity thing)
	{
		return Call("IS_ACTIVE", [thing](lhvm::LHVM& vm) { vm.Pusho(static_cast<uint32_t>(thing)); }).intVal != 0;
	}

	static constexpr int32_t k_ScriptsOwn = 7777;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::scriptObjects> _restoreScriptObjects;
	chlapi::CHLApi _api;
	RegisteredThings* _things {nullptr};
};
} // namespace

TEST_F(ScriptQueryNatives, GetTownWithIdGivesTheTownAsAFoundThing)
{
	MakeTown(20, PlayerNames::PLAYER_ONE, 1, 10);
	const auto town = MakeTown(21, PlayerNames::PLAYER_TWO, 1, 12);
	EXPECT_EQ(TownWithId(12.0f), static_cast<uint32_t>(town));
	ASSERT_EQ(_things->registered.size(), 1u);
	EXPECT_EQ(_things->registered.front().first, town);
	EXPECT_FALSE(_things->registered.front().second);
}

TEST_F(ScriptQueryNatives, GetTownWithIdCutsTheIdTowardZero)
{
	const auto ten = MakeTown(20, PlayerNames::PLAYER_ONE, 1, 10);
	EXPECT_EQ(TownWithId(10.9f), static_cast<uint32_t>(ten));
	EXPECT_EQ(TownWithId(9.9f), 0u);
}

TEST_F(ScriptQueryNatives, GetTownWithIdOfANegativeIdComparesItsBits)
{
	// the cut id is compared as it is with the town's number, so -1 is the number 0xFFFFFFFF
	MakeTown(20, PlayerNames::PLAYER_ONE, 1, 1);
	const auto minusOne = MakeTown(21, PlayerNames::PLAYER_ONE, 2, 0xFFFFFFFFu);
	EXPECT_EQ(TownWithId(-1.5f), static_cast<uint32_t>(minusOne));
}

TEST_F(ScriptQueryNatives, GetTownWithIdOfNoTownGivesZeroAndRegistersNothing)
{
	MakeTown(20, PlayerNames::PLAYER_ONE, 1, 10);
	EXPECT_EQ(TownWithId(11.0f), 0u);
	EXPECT_TRUE(_things->registered.empty());
}

TEST_F(ScriptQueryNatives, GetTownWithIdTakesThePlayersInOrderThenTheNeutralOne)
{
	MakeTown(20, PlayerNames::NEUTRAL, 1, 0);
	const auto second = MakeTown(21, PlayerNames::PLAYER_TWO, 1, 0);
	const auto neutral = MakeTown(22, PlayerNames::NEUTRAL, 2, 3);
	EXPECT_EQ(TownWithId(0.0f), static_cast<uint32_t>(second));
	EXPECT_EQ(TownWithId(3.0f), static_cast<uint32_t>(neutral));
}

TEST_F(ScriptQueryNatives, BeliefForPlayerOfATownIsItsBeliefInTheGamePlayer)
{
	const auto town = MakeTown(20, PlayerNames::PLAYER_ONE, 1, 1);
	auto& belief = Reg().Get<Town>(town).belief;
	belief.belief.at(0) = 0.25f; // game player one = script player 1
	belief.belief.at(7) = 0.5f;  // the neutral slot = script player 0
	EXPECT_FLOAT_EQ(BeliefForPlayer(town, 1.0f), 0.25f);
	EXPECT_FLOAT_EQ(BeliefForPlayer(town, 1.9f), 0.25f);
	EXPECT_FLOAT_EQ(BeliefForPlayer(town, 0.0f), 0.5f);
}

TEST_F(ScriptQueryNatives, BeliefForPlayerOfACreatureIsWholeForItsOwnerOnly)
{
	const auto creature = Reg().Create();
	Reg().Assign<Creature>(creature).owner = PlayerNames::PLAYER_TWO;
	EXPECT_FLOAT_EQ(BeliefForPlayer(creature, 2.0f), 1.0f);
	EXPECT_FLOAT_EQ(BeliefForPlayer(creature, 1.0f), 0.0f);
}

TEST_F(ScriptQueryNatives, BeliefForPlayerOfNoThingOrNoPlayerIsZero)
{
	const auto town = MakeTown(20, PlayerNames::PLAYER_ONE, 1, 1);
	EXPECT_FLOAT_EQ(BeliefForPlayer(entt::entity {0}, 1.0f), 0.0f);
	EXPECT_FLOAT_EQ(BeliefForPlayer(town, 9.0f), 0.0f);
	EXPECT_FLOAT_EQ(BeliefForPlayer(town, -1.0f), 0.0f);
}

TEST_F(ScriptQueryNatives, IsActiveOfADispenserIsItsActiveFlag)
{
	const auto dispenser = Reg().Create();
	auto& d = Reg().Assign<SpellDispenser>(dispenser);
	EXPECT_FALSE(IsActive(dispenser));
	d.timer.active = true;
	EXPECT_TRUE(IsActive(dispenser));
}

TEST_F(ScriptQueryNatives, IsActiveOfNoThingOrAnotherThingIsFalse)
{
	EXPECT_FALSE(IsActive(entt::entity {0}));
	const auto town = MakeTown(20, PlayerNames::PLAYER_ONE, 1, 1);
	EXPECT_FALSE(IsActive(town));
}

TEST_F(ScriptQueryNatives, GetObjectClickedWithNoHandGivesZeroAndRegistersNothing)
{
	// no hand system in the locator: no click memory
	ASSERT_FALSE(Locator::handSystem::has_value());
	EXPECT_EQ(Call("GET_OBJECT_CLICKED", [](lhvm::LHVM& /*vm*/) {}).uintVal, 0u);
	EXPECT_TRUE(_things->registered.empty());
}

TEST_F(ScriptQueryNatives, GetObjectClickedGivesTheTappedThingAsAFoundThingWhileItIsThere)
{
	test::RestoreService<Locator::handSystem> restoreHand;
	auto& hand = static_cast<test::creature_fakes::FakeHand&>(Locator::handSystem::emplace<test::creature_fakes::FakeHand>());
	hand.left = Reg().Create();
	auto& clicked = Reg().Assign<HandClicked>(hand.left);
	const auto thing = Reg().Create();
	clicked.thing = thing;
	EXPECT_EQ(Call("GET_OBJECT_CLICKED", [](lhvm::LHVM& /*vm*/) {}).uintVal, static_cast<uint32_t>(thing));
	ASSERT_EQ(_things->registered.size(), 1u);
	EXPECT_EQ(_things->registered.front().first, thing);
	EXPECT_FALSE(_things->registered.front().second);
	// gone: nothing
	Reg().Destroy(thing);
	EXPECT_EQ(Call("GET_OBJECT_CLICKED", [](lhvm::LHVM& /*vm*/) {}).uintVal, 0u);
	// cleared (CLEAR_CLICKED_OBJECT): nothing
	clicked.thing = entt::null;
	EXPECT_EQ(Call("GET_OBJECT_CLICKED", [](lhvm::LHVM& /*vm*/) {}).uintVal, 0u);
	EXPECT_EQ(_things->registered.size(), 1u);
	Locator::handSystem::reset();
}

TEST_F(ScriptQueryNatives, FlockMemberIsWhetherTheLivingIsInTheFlock)
{
	const auto member = Reg().Create();
	Reg().Assign<Animal>(member);
	const auto other = Reg().Create();
	Reg().Assign<Animal>(other);
	const auto flock = Reg().Create();
	Reg().Assign<Flock>(flock).members = {member};
	const auto isMember = [this, flock](entt::entity thing) {
		// the flock is popped first, so the thing is pushed first
		return Call("FLOCK_MEMBER", [flock, thing](lhvm::LHVM& vm) {
			       vm.Pusho(static_cast<uint32_t>(thing));
			       vm.Pusho(static_cast<uint32_t>(flock));
		       }).intVal != 0;
	};
	EXPECT_TRUE(isMember(member));
	EXPECT_FALSE(isMember(other));
	// not a living, or no thing
	EXPECT_FALSE(isMember(flock));
	EXPECT_FALSE(isMember(entt::entity {0}));
}

TEST_F(ScriptQueryNatives, SetFocusStandsAnObjectUprightFacingThePoint)
{
	// a feature, tilted: it stands upright on the angle that faces the point, its tilt lost
	const glm::vec3 at(100.3f, 5.0f, 200.7f);
	const glm::vec3 target(110.0f, 0.0f, 205.0f);
	const auto feature = Reg().Create();
	Reg().Assign<Feature>(feature, FeatureInfo {});
	Reg().Assign<Transform>(feature, at, affine::RotationYXZ(0.5f, 0.2f, 0.1f), glm::vec3(2.0f));
	SetFocus(feature, target);
	const auto& transform = Reg().Get<Transform>(feature);
	EXPECT_EQ(transform.rotation, affine::AngleY(ecs::script_set_position::FocusYAngle(at, target)));
	EXPECT_EQ(transform.scale, glm::vec3(2.0f));
	EXPECT_EQ(transform.position, at);
}

TEST_F(ScriptQueryNatives, SetFocusTurnsAMobileStaticWithItsTiltOnlyWhenItHasNone)
{
	const glm::vec3 target(90.0f, 0.0f, 50.0f);
	const auto rock = [](const glm::mat3& rotation) {
		const auto e = Reg().Create();
		Reg().Assign<MobileStatic>(e, MobileStaticInfo::Rock);
		Reg().Assign<Transform>(e, glm::vec3(100.0f, 0.0f, 40.0f), rotation, glm::vec3(1.0f));
		return e;
	};
	// upright: turned about Y, x and z at 0
	const auto upright = rock(affine::AngleY(1.0f));
	SetFocus(upright, target);
	EXPECT_EQ(Reg().Get<Transform>(upright).rotation,
	          affine::RotationYXZ(ecs::script_set_position::FocusYAngle(glm::vec3(100.0f, 0.0f, 40.0f), target), 0.0f, 0.0f));
	// tilted: openblack keeps no x and z angles to turn it with, so it stays as it is, a stub's call counted
	const test::RestoreService<Locator::chlapi> restoreApi;
	Locator::chlapi::emplace<chlapi::CHLApi>();
	const auto tiltedRotation = affine::RotationYXZ(1.0f, 0.3f, 0.0f);
	const auto tilted = rock(tiltedRotation);
	SetFocus(tilted, target);
	EXPECT_EQ(Reg().Get<Transform>(tilted).rotation, tiltedRotation);
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 1u);
}

TEST_F(ScriptQueryNatives, SetFocusLeavesAThingThatIsNotAnObject)
{
	// a script marker cannot face anything
	const auto marker = Reg().Create();
	Reg().Assign<ScriptMarker>(marker);
	const auto turn = affine::AngleY(0.25f);
	Reg().Assign<Transform>(marker, glm::vec3(10.0f, 0.0f, 10.0f), turn, glm::vec3(1.0f));
	SetFocus(marker, glm::vec3(0.0f));
	EXPECT_EQ(Reg().Get<Transform>(marker).rotation, turn);
	// no thing: only a warning
	SetFocus(entt::entity {0}, glm::vec3(0.0f));
}
