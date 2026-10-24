/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A thing's poison as each kind of thing keeps it (ECS/ObjectPoison.h), and the scripts' poison natives on it: what
// SET_POISONED poisons and cures, what IS_POISONED and ID_POISONED_SIZE count, and what CALL_POISONED_IN and
// CALL_NOT_POISONED_IN find in a town, each taking its arguments and leaving its one value

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

#include "CHLApi.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/ObjectPoison.h"
#include "ECS/Registry.h"
#include "ECS/ScriptTypes.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Town/TownVillagers.h"
#include "Enums.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace object_poison = openblack::ecs::object_poison;

namespace
{
/// The scripts' table of objects, as far as the natives reach it: what each Register was given
class PoisonRegisteredThings final: public ecs::systems::ScriptObjectsSystemInterface
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

class ScriptPoisonNatives: public ::testing::Test
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
		_things = &static_cast<PoisonRegisteredThings&>(Locator::scriptObjects::emplace<PoisonRegisteredThings>());
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	static uint32_t Id(entt::entity entity) { return static_cast<uint32_t>(entity); }

	const lhvm::NativeFunction* Native(std::string_view name)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [name](const lhvm::NativeFunction& f) { return std::string_view(f.name) == name; });
		EXPECT_NE(native, table.end()) << name;
		return native == table.end() ? nullptr : &*native;
	}

	/// The native as a script calls it: `push` pushes its arguments on top of a value of the script's own; the value
	/// it gives, which must be the only one it leaves above the script's
	lhvm::VMValue Call(std::string_view name, const std::function<void(lhvm::LHVM&)>& push)
	{
		const auto* native = Native(name);
		if (native == nullptr)
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

	/// SET_POISONED as a script calls it: the bool, then the object on top; it leaves nothing of the script's own
	void SetPoisoned(uint32_t object, bool poisoned)
	{
		const auto* native = Native("SET_POISONED");
		if (native == nullptr)
		{
			return;
		}
		auto& vm = Locator::vm::value();
		vm.Pushi(k_ScriptsOwn);
		vm.Pushb(poisoned);
		vm.Pusho(object);
		native->impl();
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << "SET_POISONED took or left a value";
	}

	/// CALL_POISONED_IN or CALL_NOT_POISONED_IN: the type, the sub-type, the container, then the excluding flag on top
	lhvm::VMValue CallIn(std::string_view name, script::ObjectType type, uint32_t container, bool excluding)
	{
		return Call(name, [&](lhvm::LHVM& vm) {
			vm.Pushi(static_cast<int32_t>(type));
			vm.Pushi(static_cast<int32_t>(ecs::script_type::k_AnySubtype));
			vm.Pusho(container);
			vm.Pushb(excluding);
		});
	}

	/// A town whose homeless are `count` villagers, the first made at the head of the list
	entt::entity MakeTown(uint32_t count, std::vector<entt::entity>& villagers)
	{
		const auto town = Reg().Create();
		Reg().Assign<Town>(town, 3u);
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto villager = Reg().Create();
			auto& v = Reg().Assign<Villager>(villager);
			v.town = town;
			v.abode = entt::null;
			ecs::town_villagers::AddToHomelessList(town, villager);
			villagers.push_back(villager);
		}
		return town;
	}

	/// A pile of food or wood, available or not, poisoned or not
	entt::entity MakePile(bool poisoned, bool available = true)
	{
		const auto pile = Reg().Create();
		Reg().Assign<Pot>(pile).poisoned = poisoned;
		if (!available)
		{
			Reg().Assign<Unavailable>(pile);
		}
		return pile;
	}

	/// A storage pit with a food pile and five wood piles
	entt::entity MakePit(entt::entity foodPile, entt::entity woodPile)
	{
		const auto pit = Reg().Create();
		auto& store = Reg().Assign<StoragePit>(pit);
		store.foodPile = foodPile;
		store.woodPiles.fill(entt::null);
		store.woodPiles[2] = woodPile;
		return pit;
	}

	static constexpr int32_t k_ScriptsOwn = 77;
	chlapi::CHLApi _api;
	PoisonRegisteredThings* _things {nullptr};
};
} // namespace

TEST_F(ScriptPoisonNatives, LivingThingsKeepTheirPoisonAsABitOfTheirOwn)
{
	// a villager, an animal and a creature are poisoned and cured alike
	const auto villager = Reg().Create();
	Reg().Assign<Villager>(villager);
	const auto animal = Reg().Create();
	Reg().Assign<Animal>(animal);
	const auto creature = Reg().Create();
	Reg().Assign<Creature>(creature);
	for (const auto living : {villager, animal, creature})
	{
		EXPECT_FALSE(object_poison::IsPoisoned(living));
		object_poison::SetPoisoned(living, true);
		EXPECT_TRUE(object_poison::IsPoisoned(living));
		EXPECT_TRUE(Reg().AllOf<Poisoned>(living));
		object_poison::SetPoisoned(living, false);
		EXPECT_FALSE(object_poison::IsPoisoned(living));
	}
}

TEST_F(ScriptPoisonNatives, APotKeepsItsOwnFlagAndAToadstoolIsAlwaysPoisoned)
{
	const auto pot = MakePile(false);
	object_poison::SetPoisoned(pot, true);
	EXPECT_TRUE(Reg().Get<const Pot>(pot).poisoned);
	EXPECT_TRUE(object_poison::IsPoisoned(pot));
	object_poison::SetPoisoned(pot, false);
	EXPECT_FALSE(object_poison::IsPoisoned(pot));

	// a toadstool by its kind, whatever is set; another mobile object never, and nothing marks it
	const auto toadstool = Reg().Create();
	Reg().Assign<MobileObject>(toadstool).type = MobileObjectInfo::Toadstool;
	const auto barrel = Reg().Create();
	Reg().Assign<MobileObject>(barrel).type = MobileObjectInfo::EgyptBarrel;
	object_poison::SetPoisoned(toadstool, false);
	object_poison::SetPoisoned(barrel, true);
	EXPECT_TRUE(object_poison::IsPoisoned(toadstool));
	EXPECT_FALSE(object_poison::IsPoisoned(barrel));
	EXPECT_FALSE(Reg().AllOf<Poisoned>(barrel));

	// a plain object is never poisoned
	EXPECT_FALSE(object_poison::IsPoisoned(Reg().Create()));
	EXPECT_FALSE(object_poison::IsPoisoned(entt::null));
}

TEST_F(ScriptPoisonNatives, AStoragePitIsPoisonedByItsPilesAndOnlyItsFoodPileIsEverPoisoned)
{
	// the wood piles count when they are there; one taken away does not
	const auto food = MakePile(false);
	const auto wood = MakePile(true);
	const auto pit = MakePit(food, wood);
	EXPECT_TRUE(object_poison::IsPoisoned(pit));
	Reg().Assign<Unavailable>(wood);
	EXPECT_FALSE(object_poison::IsPoisoned(pit));

	// poisoning it poisons the food pile; curing it leaves the food pile poisoned
	object_poison::SetPoisoned(pit, true);
	EXPECT_TRUE(Reg().Get<const Pot>(food).poisoned);
	object_poison::SetPoisoned(pit, false);
	EXPECT_TRUE(Reg().Get<const Pot>(food).poisoned);
	EXPECT_TRUE(object_poison::IsPoisoned(pit));

	// the wood piles are never touched, and a food pile that is not there is not poisoned
	const auto cleanWood = MakePile(false);
	const auto goneFood = MakePile(false, false);
	const auto other = MakePit(goneFood, cleanWood);
	object_poison::SetPoisoned(other, true);
	EXPECT_FALSE(Reg().Get<const Pot>(goneFood).poisoned);
	EXPECT_FALSE(Reg().Get<const Pot>(cleanWood).poisoned);
	EXPECT_FALSE(object_poison::IsPoisoned(other));
}

TEST_F(ScriptPoisonNatives, SetPoisonedOnATownPoisonsAndCuresEachVillager)
{
	std::vector<entt::entity> villagers;
	const auto town = MakeTown(3, villagers);
	SetPoisoned(Id(town), true);
	for (const auto villager : villagers)
	{
		EXPECT_TRUE(object_poison::IsPoisoned(villager));
	}
	EXPECT_EQ(Call("ID_POISONED_SIZE", [&](lhvm::LHVM& vm) { vm.Pusho(Id(town)); }).floatVal, 3.0f);

	// one cured: the town counts two poisoned, and IS_POISONED reads each one
	SetPoisoned(Id(villagers[1]), false);
	EXPECT_EQ(Call("ID_POISONED_SIZE", [&](lhvm::LHVM& vm) { vm.Pusho(Id(town)); }).floatVal, 2.0f);
	EXPECT_EQ(Call("IS_POISONED", [&](lhvm::LHVM& vm) { vm.Pusho(Id(villagers[0])); }).intVal, 1);
	EXPECT_EQ(Call("IS_POISONED", [&](lhvm::LHVM& vm) { vm.Pusho(Id(villagers[1])); }).intVal, 0);

	// the whole town cured
	SetPoisoned(Id(town), false);
	EXPECT_EQ(Call("ID_POISONED_SIZE", [&](lhvm::LHVM& vm) { vm.Pusho(Id(town)); }).floatVal, 0.0f);
}

TEST_F(ScriptPoisonNatives, NoThingOrAThingWithoutMembersGivesNought)
{
	// no thing: nought and false, and SET_POISONED changes nothing
	EXPECT_EQ(Call("ID_POISONED_SIZE", [](lhvm::LHVM& vm) { vm.Pusho(0); }).floatVal, 0.0f);
	EXPECT_EQ(Call("IS_POISONED", [](lhvm::LHVM& vm) { vm.Pusho(0); }).intVal, 0);
	SetPoisoned(0, true);

	// a poisoned pot is poisoned, but it has no members to count
	const auto pot = MakePile(true);
	EXPECT_EQ(Call("IS_POISONED", [&](lhvm::LHVM& vm) { vm.Pusho(Id(pot)); }).intVal, 1);
	EXPECT_EQ(Call("ID_POISONED_SIZE", [&](lhvm::LHVM& vm) { vm.Pusho(Id(pot)); }).floatVal, 0.0f);
}

TEST_F(ScriptPoisonNatives, CallInFindsTheFirstPoisonedOrHealthyVillagerOfATown)
{
	std::vector<entt::entity> villagers;
	const auto town = MakeTown(3, villagers);
	// the town's order is its homeless from the head: the last made first
	const auto& order = ecs::town_villagers::Homeless(town);
	ASSERT_EQ(order.size(), 3u);
	object_poison::SetPoisoned(order[1], true);

	EXPECT_EQ(CallIn("CALL_POISONED_IN", script::ObjectType::Villager, Id(town), false).uintVal, Id(order[1]));
	EXPECT_EQ(CallIn("CALL_NOT_POISONED_IN", script::ObjectType::Villager, Id(town), false).uintVal, Id(order[0]));
	// each found thing comes into the script's table as found, not created
	const std::vector<std::pair<entt::entity, bool>> expected {{order[1], false}, {order[0], false}};
	EXPECT_EQ(_things->registered, expected);

	// another type finds nothing; neither does a thing without members, nor no thing
	EXPECT_EQ(CallIn("CALL_POISONED_IN", script::ObjectType::Animal, Id(town), false).uintVal, 0u);
	EXPECT_EQ(CallIn("CALL_POISONED_IN", script::ObjectType::Villager, Id(order[1]), false).uintVal, 0u);
	EXPECT_EQ(CallIn("CALL_NOT_POISONED_IN", script::ObjectType::Villager, 0, false).uintVal, 0u);
	EXPECT_EQ(_things->registered.size(), 2u);
}
