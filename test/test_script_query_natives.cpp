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
// nothing, turns a thing to face a point, and SET_OBJECT_CARRYING gives a villager an object to hold

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <bit>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "CHLApi.h"
#include "Camera/FieldOfView.h"
#include "Common/EventManager.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandomTesting.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/ComputerHand.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/HandClicked.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDeaths.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ComputerHand.h"
#include "ECS/Events/LandEvents.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectFlags.h"
#include "ECS/PlacedTurn.h"
#include "ECS/PuzzleGames.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ScriptHeld.h"
#include "ECS/ScriptSetPosition.h"
#include "ECS/ScriptTarget.h"
#include "ECS/ScriptTypes.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/Implementations/ComputerPlayerSystem.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "ECS/Systems/Implementations/VillageTotemSystem.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Weather/WeatherThing.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptPropertyRules.h"
#include "Worship/Citadel.h"
#include "creature/CreatureSystemFakes.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
#include "support/SkyFrameGuard.h"
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

/// The particles as far as a script's scale reaches them: the magnitudes given to the one spot visual it knows
class MagnitudeRecorder final: public test::InertParticleSystem
{
public:
	bool SetSpotVisualMagnitude(entt::entity object, float magnitude) override
	{
		if (object != visual)
		{
			return false;
		}
		magnitudes.push_back(magnitude);
		return true;
	}

	entt::entity visual {entt::null};
	std::vector<float> magnitudes;
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

	/// Call, with the type the native gave its value
	std::pair<lhvm::VMValue, lhvm::DataType> CallTyped(std::string_view name, const std::function<void(lhvm::LHVM&)>& push)
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
		lhvm::DataType type {};
		const auto given = vm.Pop(type);
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << name << " left more than one value";
		return {given, type};
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

	/// SET_OBJECT_CARRYING as a script calls it: the object is pushed first and the carried object last
	void SetObjectCarrying(entt::entity thing, int32_t carried)
	{
		CallLeavingNothing("SET_OBJECT_CARRYING", [thing, carried](lhvm::LHVM& vm) {
			vm.Pusho(static_cast<uint32_t>(thing));
			vm.Pushi(carried);
		});
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

	/// GET_PROPERTY's PLAYER (21) of a thing: the object is popped first, so it is pushed last
	lhvm::VMValue PlayerOf(entt::entity thing)
	{
		return Call("GET_PROPERTY", [thing](lhvm::LHVM& vm) {
			vm.Pushi(21);
			vm.Pusho(static_cast<uint32_t>(thing));
		});
	}

	/// GET_PROPERTY's `property` of a thing, as a float
	float PropertyOf(entt::entity thing, int32_t property)
	{
		return Call("GET_PROPERTY",
		            [thing, property](lhvm::LHVM& vm) {
			            vm.Pushi(property);
			            vm.Pusho(static_cast<uint32_t>(thing));
		            })
		    .floatVal;
	}

	/// SET_PROPERTY's `property` of a thing: the value is popped first, so it is pushed last
	void SetPropertyOf(entt::entity thing, int32_t property, float value)
	{
		CallLeavingNothing("SET_PROPERTY", [thing, property, value](lhvm::LHVM& vm) {
			vm.Pushi(property);
			vm.Pusho(static_cast<uint32_t>(thing));
			vm.Pushf(value);
		});
	}

	/// A building of the town of key `townKey`, `built` of it built
	static entt::entity MakeAbode(uint32_t townKey, float built = 1.0f)
	{
		const auto e = Reg().Create();
		auto& abode = Reg().Assign<Abode>(e, AbodeNumber {}, townKey, 0u, 0u);
		abode.percentBuilt = built;
		return e;
	}

	/// Call for a native whose name two natives share: the one of number `number`
	lhvm::VMValue CallNumber(size_t number, std::string_view name, const std::function<void(lhvm::LHVM&)>& push)
	{
		const auto& table = _api.GetFunctionsTable();
		EXPECT_LT(number, table.size()) << name;
		if (number >= table.size())
		{
			return {};
		}
		const auto& native = table.at(number);
		EXPECT_EQ(std::string_view(native.name), name);
		auto& vm = Locator::vm::value();
		vm.Pushi(k_ScriptsOwn);
		push(vm);
		native.impl();
		const auto given = vm.Pop();
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << name << " left more than one value";
		return given;
	}

	/// The creature's GET_OBJECT_HELD (273) of a thing
	uint32_t HeldByCreature(entt::entity thing)
	{
		return CallNumber(273, "GET_OBJECT_HELD", [thing](lhvm::LHVM& vm) { vm.Pusho(static_cast<uint32_t>(thing)); }).uintVal;
	}

	/// SET_DISCIPLE as a script calls it: the object is pushed first and the "with sound" bool last
	void SetDisciple(entt::entity thing, int32_t type, int32_t withSound = 1)
	{
		CallLeavingNothing("SET_DISCIPLE", [thing, type, withSound](lhvm::LHVM& vm) {
			vm.Pusho(static_cast<uint32_t>(thing));
			vm.Pushi(type);
			vm.Pushi(withSound);
		});
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

// SET_ACTIVE takes the thing as the scripts reach it before asking what it is: a scroll being deleted is no longer
// valid and keeps its state, as the original's look-up gives none (docs/wiki/scripts/script-commands.md, SET_ACTIVE)
TEST_F(ScriptQueryNatives, SetActiveLeavesAThingBeingDeletedAlone)
{
	const auto setActive = [this](entt::entity thing, bool on) {
		CallLeavingNothing("SET_ACTIVE", [thing, on](lhvm::LHVM& vm) {
			vm.Pushb(on);
			vm.Pusho(static_cast<uint32_t>(thing));
		});
	};
	const auto scroll = Reg().Create();
	Reg().Assign<ecs::components::ScriptHighlight>(scroll).active = true;
	Reg().Assign<ecs::components::Unavailable>(scroll);
	setActive(scroll, false);
	EXPECT_TRUE(Reg().Get<const ecs::components::ScriptHighlight>(scroll).active);
	// reached, it is switched off
	Reg().Remove<ecs::components::Unavailable>(scroll);
	setActive(scroll, false);
	EXPECT_FALSE(Reg().Get<const ecs::components::ScriptHighlight>(scroll).active);
}

TEST_F(ScriptQueryNatives, IsActiveOfNoThingOrAnotherThingIsFalse)
{
	EXPECT_FALSE(IsActive(entt::entity {0}));
	const auto town = MakeTown(20, PlayerNames::PLAYER_ONE, 1, 1);
	EXPECT_FALSE(IsActive(town));
}

// The reward row's help text sits where the original reads it in the info (the info.dat record starts 16 bytes into it)
static_assert(offsetof(GRewardInfo, helpTextEnum) == 0x120 - 0x10);

TEST_F(ScriptQueryNatives, GetHelpOfARewardChestIsItsRowsHelpTextAsAFloat)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	info->reward.at(static_cast<size_t>(RewardObjectInfo::SmallFood)).helpTextEnum = static_cast<HelpText>(1234);
	// read unsigned, as the original reads it
	info->reward.at(static_cast<size_t>(RewardObjectInfo::HealPu1)).helpTextEnum = static_cast<HelpText>(-1);
	Locator::infoConstants::reset(info.release());
	const auto getHelp = [this](entt::entity thing) {
		return CallTyped("GET_HELP", [thing](lhvm::LHVM& vm) { vm.Pusho(static_cast<uint32_t>(thing)); });
	};
	const auto chest = Reg().Create();
	Reg().Assign<Reward>(chest).type = RewardObjectInfo::SmallFood;
	const auto [given, type] = getHelp(chest);
	EXPECT_EQ(type, lhvm::DataType::Float);
	EXPECT_FLOAT_EQ(given.floatVal, 1234.0f);
	Reg().Get<Reward>(chest).type = RewardObjectInfo::HealPu1;
	EXPECT_FLOAT_EQ(getHelp(chest).first.floatVal, 4294967295.0f);
	// nothing is added to the script's things
	EXPECT_TRUE(_things->registered.empty());
}

TEST_F(ScriptQueryNatives, GetHelpOfNoThingIsZero)
{
	EXPECT_FLOAT_EQ(Call("GET_HELP", [](lhvm::LHVM& vm) { vm.Pusho(0); }).floatVal, 0.0f);
	EXPECT_FLOAT_EQ(Call("GET_HELP", [](lhvm::LHVM& vm) { vm.Pusho(12345); }).floatVal, 0.0f);
}

TEST_F(ScriptQueryNatives, GetHelpOfADispenserIsItsMiraclesFirstHelpText)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	info->magicWood[0].magicType = MagicType::Wood;
	info->magicEffect.at(static_cast<size_t>(MagicType::Wood)).helpStartEnum = static_cast<HelpText>(2000);
	Locator::infoConstants::reset(info.release());
	const auto dispenser = Reg().Create();
	Reg().Assign<SpellDispenser>(dispenser).magicType = MagicType::Wood;
	EXPECT_FLOAT_EQ(Call("GET_HELP", [dispenser](lhvm::LHVM& vm) { vm.Pusho(static_cast<uint32_t>(dispenser)); }).floatVal,
	                2000.0f);
}

TEST_F(ScriptQueryNatives, GetHelpOfAnyOtherThingIsLeftAtZero)
{
	// not a chest: told so, then answered as the first help text of the thing, which is not ported for a town
	const test::RestoreService<Locator::chlapi> restoreApi;
	Locator::chlapi::emplace<chlapi::CHLApi>();
	const auto town = MakeTown(20, PlayerNames::PLAYER_ONE, 1, 1);
	EXPECT_FLOAT_EQ(Call("GET_HELP", [town](lhvm::LHVM& vm) { vm.Pusho(static_cast<uint32_t>(town)); }).floatVal, 0.0f);
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 1u);
}

// The villager info's sex sits where the original reads it (the info.dat record starts 16 bytes into it)
static_assert(offsetof(GVillagerInfo, sex) == 0x1F8 - 0x10);

TEST_F(ScriptQueryNatives, SexIsMaleOfAVillagerIsItsInfosSex)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	info->villager[0].tribeType = Tribe::NORSE;
	info->villager[0].villagerNumber = VillagerNumber::Housewife;
	info->villager[0].sex = SexType::Female;
	info->villager[1].tribeType = Tribe::NORSE;
	info->villager[1].villagerNumber = VillagerNumber::Forester;
	info->villager[1].sex = SexType::Male;
	Locator::infoConstants::reset(info.release());
	const auto isMale = [this](entt::entity thing) {
		return CallTyped("SEX_IS_MALE", [thing](lhvm::LHVM& vm) { vm.Pusho(static_cast<uint32_t>(thing)); });
	};
	const auto villager = Reg().Create();
	auto& v = Reg().Assign<Villager>(villager);
	v.tribe = Tribe::NORSE;
	v.number = VillagerNumber::Housewife;
	// what the villager itself keeps is not read
	v.sex = Villager::Sex::MALE;
	const auto [woman, type] = isMale(villager);
	EXPECT_EQ(type, lhvm::DataType::Boolean);
	EXPECT_EQ(woman.intVal, 0);
	v.number = VillagerNumber::Forester;
	v.sex = Villager::Sex::FEMALE;
	EXPECT_EQ(isMale(villager).first.intVal, 1);
}

TEST_F(ScriptQueryNatives, SexIsMaleOfAnythingButAVillagerIsTrue)
{
	const auto town = MakeTown(20, PlayerNames::PLAYER_ONE, 1, 1);
	for (const uint32_t thing : {0u, 12345u, static_cast<uint32_t>(town)})
	{
		EXPECT_EQ(Call("SEX_IS_MALE", [thing](lhvm::LHVM& vm) { vm.Pusho(thing); }).intVal, 1) << thing;
	}
	EXPECT_TRUE(_things->registered.empty());
}

namespace
{
/// The citadel of a player at a point: a temple with its worship part, and its heart when `withHeart`
entt::entity MakeCitadel(PlayerNames owner, const glm::vec3& at, bool withHeart)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto citadel = registry.Create();
	registry.Assign<Transform>(citadel, at, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Temple>(citadel).owner = owner;
	registry.Assign<CitadelWorship>(citadel);
	if (withHeart)
	{
		registry.Assign<CitadelHeart>(citadel).citadel = citadel;
	}
	return citadel;
}

/// GET_TEMPLE_POSITION (no extra arguments) or GET_TEMPLE_ENTRANCE_POSITION as a script calls it: the player is
/// pushed first, then the radius and the height. The point it gives, which must be the only three values it leaves
glm::vec3 TemplePoint(chlapi::CHLApi& api, std::string_view name, float player, std::optional<glm::vec2> radiusHeight)
{
	const auto& table = api.GetFunctionsTable();
	const auto native =
	    std::ranges::find_if(table, [name](const lhvm::NativeFunction& f) { return std::string_view(f.name) == name; });
	EXPECT_NE(native, table.end()) << name;
	if (native == table.end())
	{
		return {};
	}
	auto& vm = Locator::vm::value();
	vm.Pushi(7777);
	vm.Pushf(player);
	if (radiusHeight.has_value())
	{
		vm.Pushf(radiusHeight->x);
		vm.Pushf(radiusHeight->y);
	}
	native->impl();
	glm::vec3 point;
	for (const auto axis : {2, 1, 0})
	{
		lhvm::DataType type {};
		point[axis] = vm.Pop(type).floatVal;
		EXPECT_EQ(type, lhvm::DataType::Vector) << name;
	}
	EXPECT_EQ(vm.Pop().intVal, 7777) << name << " left a value more or less";
	return point;
}
} // namespace

TEST_F(ScriptQueryNatives, GetTemplePositionIsThePlayersTemplePlace)
{
	// on the map's fixed point, at the temple's own height above the land (no land here: 0)
	const glm::vec3 at {1915.05f, 7.5f, 2508.89f};
	MakeCitadel(PlayerNames::PLAYER_TWO, glm::vec3(5.0f, 0.0f, 5.0f), true);
	MakeCitadel(PlayerNames::PLAYER_ONE, at, true);
	const auto place = TemplePoint(_api, "GET_TEMPLE_POSITION", 1.9f, std::nullopt);
	EXPECT_EQ(place.x, map_coords::ToMetres(map_coords::ToFixed(at.x)));
	EXPECT_EQ(place.y, at.y);
	EXPECT_EQ(place.z, map_coords::ToMetres(map_coords::ToFixed(at.z)));
	EXPECT_EQ(TemplePoint(_api, "GET_TEMPLE_POSITION", 2.0f, std::nullopt), glm::vec3(5.0f, 0.0f, 5.0f));
}

TEST_F(ScriptQueryNatives, GetTemplePositionsOfAPlayerWithoutATempleAreTheOrigin)
{
	MakeCitadel(PlayerNames::PLAYER_ONE, glm::vec3(10.0f, 0.0f, 20.0f), true);
	// script player 0 is the neutral player; 9 names no player
	for (const float player : {0.0f, 3.0f, 9.0f, -1.0f})
	{
		EXPECT_EQ(TemplePoint(_api, "GET_TEMPLE_POSITION", player, std::nullopt), glm::vec3(0.0f)) << player;
		EXPECT_EQ(TemplePoint(_api, "GET_TEMPLE_ENTRANCE_POSITION", player, glm::vec2(90.0f, 40.0f)), glm::vec3(0.0f))
		    << player;
	}
}

TEST_F(ScriptQueryNatives, GetTempleEntrancePositionOfATempleWithoutItsHeartIsItsPlace)
{
	MakeCitadel(PlayerNames::PLAYER_ONE, glm::vec3(10.0f, 3.0f, 20.0f), false);
	EXPECT_EQ(TemplePoint(_api, "GET_TEMPLE_ENTRANCE_POSITION", 1.0f, glm::vec2(90.0f, 40.0f)), glm::vec3(10.0f, 3.0f, 20.0f));
}

TEST_F(ScriptQueryNatives, GetTempleEntrancePositionWithNoPenPointIsAboveThePlace)
{
	// a heart whose mesh marks no pen point gives its own position: no way to go, only the height
	MakeCitadel(PlayerNames::PLAYER_ONE, glm::vec3(10.0f, 3.0f, 20.0f), true);
	EXPECT_EQ(TemplePoint(_api, "GET_TEMPLE_ENTRANCE_POSITION", 1.0f, glm::vec2(90.0f, 40.0f)), glm::vec3(10.0f, 43.0f, 20.0f));
}

TEST(TempleEntrance, IsTheRadiusTowardThePointAndTheHeightUp)
{
	using worship::citadel::EntrancePoint;
	// (3, 4, 0) of length 5: 0.6 and 0.8 of the radius, then the height on top
	const auto entrance = EntrancePoint(glm::vec3(100.0f, 10.0f, 200.0f), glm::vec3(103.0f, 14.0f, 200.0f), 90.0f, 40.0f);
	EXPECT_FLOAT_EQ(entrance.x, 154.0f);
	EXPECT_FLOAT_EQ(entrance.y, 122.0f);
	EXPECT_EQ(entrance.z, 200.0f);
	// every step a float, as the game takes it
	const float inverse = 1.0f / std::sqrt((0.0f + 16.0f) + 9.0f);
	EXPECT_EQ(entrance.x, (3.0f * inverse) * 90.0f + 100.0f);
	EXPECT_EQ(entrance.y, 40.0f + ((4.0f * inverse) * 90.0f + 10.0f));
	// a point below and behind: the direction is a unit one whatever its length
	const auto low = EntrancePoint(glm::vec3(0.0f), glm::vec3(0.0f, -300.0f, 0.0f), 50.0f, 10.0f);
	EXPECT_FLOAT_EQ(low.y, -40.0f);
}

TEST(TempleEntrance, OnThePlaceItselfIsOnlyTheHeightUp)
{
	const glm::vec3 place {12.5f, 3.0f, -7.25f};
	EXPECT_EQ(worship::citadel::EntrancePoint(place, place, 90.0f, 40.0f), glm::vec3(12.5f, 43.0f, -7.25f));
}

TEST_F(ScriptQueryNatives, GameTypeIsTheThingsScriptObjectTypeAsAnInt)
{
	const auto gameType = [this](uint32_t thing) {
		return CallTyped("GAME_TYPE", [thing](lhvm::LHVM& vm) { vm.Pusho(thing); });
	};
	const auto town = MakeTown(20, PlayerNames::PLAYER_ONE, 1, 1);
	const auto [townType, type] = gameType(static_cast<uint32_t>(town));
	EXPECT_EQ(type, lhvm::DataType::Int);
	EXPECT_EQ(townType.intVal, static_cast<int32_t>(script::ObjectType::Town));
	EXPECT_EQ(townType.intVal, static_cast<int32_t>(ecs::script_type::TypeOf(town)));
	const auto creature = Reg().Create();
	Reg().Assign<Creature>(creature);
	EXPECT_EQ(gameType(static_cast<uint32_t>(creature)).first.intVal, static_cast<int32_t>(script::ObjectType::Creature));
	// a thing of no class the scripts know: 0
	EXPECT_EQ(gameType(static_cast<uint32_t>(Reg().Create())).first.intVal, 0);
	EXPECT_TRUE(_things->registered.empty());
}

TEST_F(ScriptQueryNatives, GameTypeOfNoThingIsZero)
{
	for (const uint32_t thing : {0u, 12345u})
	{
		const auto [given, type] = CallTyped("GAME_TYPE", [thing](lhvm::LHVM& vm) { vm.Pusho(thing); });
		EXPECT_EQ(type, lhvm::DataType::Int) << thing;
		EXPECT_EQ(given.intVal, 0) << thing;
	}
}

TEST_F(ScriptQueryNatives, PlayedOfAnAnimalIsWhetherItEndsInTheScriptsState)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	// the table stays the test's to change: the locator holds it as const
	auto info = std::make_unique<InfoConstants>();
	auto& states = info->animalStateTable;
	Locator::infoConstants::reset(info.release());
	const test::RestoreService<Locator::chlapi> restoreApi;
	Locator::chlapi::emplace<chlapi::CHLApi>();
	const auto animal = Reg().Create();
	Reg().Assign<Animal>(animal, AnimalInfo::Cow, 0u);
	auto& brain = Reg().Assign<AnimalBrain>(animal);
	const auto played = [this, animal] {
		return Call("PLAYED", [animal](lhvm::LHVM& vm) { vm.Pusho(static_cast<uint32_t>(animal)); }).intVal != 0;
	};
	// on its way somewhere: a top state that is not a final one, so the state it heads for counts
	brain.topState = static_cast<uint8_t>(ecs::animal_ai::AnimalState::MoveToPos);
	brain.finalState = static_cast<uint8_t>(ecs::animal_ai::AnimalState::InScript);
	EXPECT_TRUE(played());
	brain.finalState = static_cast<uint8_t>(ecs::animal_ai::AnimalState::InDance);
	EXPECT_FALSE(played());
	// a final top state counts itself, whatever it was heading for
	states.at(4).field0xc = 1;
	brain.topState = static_cast<uint8_t>(ecs::animal_ai::AnimalState::InScript);
	EXPECT_TRUE(played());
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 0u);
}

TEST_F(ScriptQueryNatives, GetObjectStateOfALivingThingIsTheStateItEndsIn)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	auto& animalStates = info->animalStateTable;
	Locator::infoConstants::reset(info.release());
	const test::RestoreService<Locator::chlapi> restoreApi;
	Locator::chlapi::emplace<chlapi::CHLApi>();
	const auto stateOf = [this](entt::entity thing) {
		return Call("GET_OBJECT_STATE", [thing](lhvm::LHVM& vm) { vm.Pusho(static_cast<uint32_t>(thing)); }).intVal;
	};
	// an animal on its way somewhere: the state it heads for, until its top state is a final one
	const auto animal = Reg().Create();
	Reg().Assign<Animal>(animal, AnimalInfo::Sheep, 0u);
	auto& brain = Reg().Assign<AnimalBrain>(animal);
	brain.topState = static_cast<uint8_t>(ecs::animal_ai::AnimalState::MoveToPos);
	brain.finalState = static_cast<uint8_t>(ecs::animal_ai::AnimalState::Dead);
	EXPECT_EQ(stateOf(animal), 15);
	animalStates.at(1).field0xc = 1;
	EXPECT_EQ(stateOf(animal), 1);
	// a villager the same way, by its own table
	const auto villager = Reg().Create();
	Reg().Assign<Villager>(villager);
	auto& action = Reg().Assign<LivingAction>(villager, VillagerStates::MoveToPos, uint16_t {0});
	action.states.at(static_cast<size_t>(LivingAction::Index::Final)) = static_cast<uint8_t>(VillagerStates::InScript);
	EXPECT_EQ(stateOf(villager), 4);
	// a creature is always in state 0
	const auto creature = Reg().Create();
	Reg().Assign<Creature>(creature);
	EXPECT_EQ(stateOf(creature), 0);
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 0u);
}

TEST_F(ScriptQueryNatives, GetObjectStateOfAFishPuzzleIsNotBegunThenInProgressThenWon)
{
	const auto stateOf = [this](entt::entity thing) {
		return Call("GET_OBJECT_STATE", [thing](lhvm::LHVM& vm) { vm.Pusho(static_cast<uint32_t>(thing)); }).intVal;
	};
	const auto puzzle = Reg().Create();
	Reg().Assign<PuzzleGame>(puzzle).type = script::PuzzleGameType::Fishes1;
	EXPECT_EQ(stateOf(puzzle), static_cast<int32_t>(script::PuzzleGameStatus::GameNotYetBegun));
	// its bait made: in progress until the fish are kept in the net
	const auto bait = Reg().Create();
	Reg().Assign<FishBait>(bait);
	Reg().Get<PuzzleGame>(puzzle).bait = bait;
	EXPECT_EQ(stateOf(puzzle), static_cast<int32_t>(script::PuzzleGameStatus::GameInProgress));
	EXPECT_FALSE(Reg().Get<PuzzleGame>(puzzle).played);
	Reg().Get<FishBait>(bait).done = true;
	EXPECT_EQ(stateOf(puzzle), static_cast<int32_t>(script::PuzzleGameStatus::Won));
	// asking marks it played, as the played test does
	EXPECT_TRUE(Reg().Get<PuzzleGame>(puzzle).played);
	// a puzzle of a kind not ported never begins
	const auto lion = Reg().Create();
	Reg().Assign<PuzzleGame>(lion).type = script::PuzzleGameType::Tilt1;
	EXPECT_EQ(stateOf(lion), static_cast<int32_t>(script::PuzzleGameStatus::GameNotYetBegun));
}

TEST_F(ScriptQueryNatives, GetObjectStateOfNoThingOrAnotherThingIsZero)
{
	const auto stateOf = [this](entt::entity thing) {
		return Call("GET_OBJECT_STATE", [thing](lhvm::LHVM& vm) { vm.Pusho(static_cast<uint32_t>(thing)); }).intVal;
	};
	EXPECT_EQ(stateOf(entt::entity {0}), 0);
	EXPECT_EQ(stateOf(MakeTown(30, PlayerNames::PLAYER_ONE, 1, 1)), 0);
}

TEST_F(ScriptQueryNatives, PlayedOfAPuzzleWithNothingToPlayIsTrueFromTheStart)
{
	const auto played = [this](entt::entity puzzle) {
		return Call("PLAYED", [puzzle](lhvm::LHVM& vm) { vm.Pusho(static_cast<uint32_t>(puzzle)); }).intVal != 0;
	};
	for (const auto type : {script::PuzzleGameType::Maze1, script::PuzzleGameType::Maze2, script::PuzzleGameType::Fishes2})
	{
		const auto puzzle = Reg().Create();
		Reg().Assign<PuzzleGame>(puzzle).type = type;
		EXPECT_TRUE(played(puzzle)) << static_cast<int>(type);
		EXPECT_TRUE(Reg().Get<PuzzleGame>(puzzle).played);
		EXPECT_EQ(Call("GET_OBJECT_STATE", [puzzle](lhvm::LHVM& vm) { vm.Pusho(static_cast<uint32_t>(puzzle)); }).intVal,
		          static_cast<int32_t>(script::PuzzleGameStatus::Won));
	}
	// a puzzle with something to play that is not ported is never played
	const auto tilt = Reg().Create();
	Reg().Assign<PuzzleGame>(tilt).type = script::PuzzleGameType::Tilt1;
	EXPECT_FALSE(played(tilt));
	EXPECT_FALSE(Reg().Get<PuzzleGame>(tilt).played);
}

TEST(PuzzleGameCompletion, OnlyTheMazesTheSecondFishPuzzleAndNoPuzzleCompleteAtOnce)
{
	using script::PuzzleGameType;
	static_assert(ecs::CompletesAtOnce(PuzzleGameType::Maze1));
	static_assert(ecs::CompletesAtOnce(PuzzleGameType::Maze2));
	static_assert(ecs::CompletesAtOnce(PuzzleGameType::Fishes2));
	static_assert(ecs::CompletesAtOnce(PuzzleGameType::None));
	static_assert(ecs::CompletesAtOnce(PuzzleGameType::Last));
	static_assert(!ecs::CompletesAtOnce(PuzzleGameType::Fishes1));
	static_assert(!ecs::CompletesAtOnce(PuzzleGameType::Hanoi));
	static_assert(!ecs::CompletesAtOnce(PuzzleGameType::Tilt1));
	static_assert(!ecs::CompletesAtOnce(PuzzleGameType::Immersion));
	static_assert(!ecs::CompletesAtOnce(PuzzleGameType::Chess));
}

TEST(PuzzleGameStatusRule, FollowsTheGamesBeginningAndEnd)
{
	using ecs::PuzzleGameStatusFrom;
	static_assert(PuzzleGameStatusFrom(false, false) == script::PuzzleGameStatus::GameNotYetBegun);
	static_assert(PuzzleGameStatusFrom(false, true) == script::PuzzleGameStatus::GameInProgress);
	static_assert(PuzzleGameStatusFrom(true, false) == script::PuzzleGameStatus::Won);
	static_assert(PuzzleGameStatusFrom(true, true) == script::PuzzleGameStatus::Won);
	// the numbers the scripts compare with (the lion puzzle waits for 4, in progress)
	EXPECT_EQ(static_cast<int>(script::PuzzleGameStatus::GameInProgress), 4);
	EXPECT_EQ(static_cast<int>(script::PuzzleGameStatus::GameNotYetBegun), 5);
}

TEST(ScriptTargetRule, TheThrowLandsOnThePointAfterTheTime)
{
	namespace target = ecs::script_target;
	const glm::vec3 from {100.0f, 20.0f, 50.0f};
	const glm::vec3 to {110.0f, 20.0f, 30.0f};
	const auto v = target::LaunchVelocity(from, to, 2.0f);
	EXPECT_FLOAT_EQ(v.x, 5.0f);
	EXPECT_FLOAT_EQ(v.z, -10.0f);
	// on level ground it goes up at g t / 2 to come down again after t
	EXPECT_FLOAT_EQ(v.y, 9.81f);
	// under gravity alone it is at the point after the time
	for (const float time : {0.5f, 1.0f, 6.0f})
	{
		const glm::vec3 high {-40.0f, 270.0f, 400.0f};
		const auto w = target::LaunchVelocity(high, to, time);
		const auto at = high + w * time + glm::vec3(0.0f, -0.5f * target::k_Gravity * time * time, 0.0f);
		EXPECT_NEAR(at.x, to.x, 1e-2f);
		EXPECT_NEAR(at.y, to.y, 1e-2f);
		EXPECT_NEAR(at.z, to.z, 1e-2f);
	}
}

TEST(ScriptTargetRule, TheSpinIsADrawOf200CentredAndScaled)
{
	namespace target = ecs::script_target;
	EXPECT_FLOAT_EQ(target::SpinFromDraw(0.0f), -1.0f);
	EXPECT_EQ(target::SpinFromDraw(100.0f), 0.0f);
	EXPECT_FLOAT_EQ(target::SpinFromDraw(200.0f), 1.0f);
	EXPECT_FLOAT_EQ(target::SpinFromDraw(150.0f), 0.5f);
}

TEST_F(ScriptQueryNatives, CallNearInStateFindsALivingThingInTheStateAsAFoundThing)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
	const test::RestoreService<Locator::chlapi> restoreApi;
	Locator::chlapi::emplace<chlapi::CHLApi>();
	ecs::map_cells::Clear();
	ecs::object_index::OnLoadMap();
	// a sheep heading for a state (its top state's row is not a final one, so that state counts)
	const auto sheepAt = [](const glm::vec3& at, ecs::animal_ai::AnimalState state) {
		const auto e = Reg().Create();
		ecs::object_index::Assign(e);
		Reg().Assign<Transform>(e, at, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<Animal>(e, AnimalInfo::Sheep, 0u);
		auto& brain = Reg().Assign<AnimalBrain>(e);
		brain.topState = static_cast<uint8_t>(ecs::animal_ai::AnimalState::MoveToPos);
		brain.finalState = static_cast<uint8_t>(state);
		ecs::map_cells::InsertMapObject(e);
		return e;
	};
	const auto deadNear = [this](uint32_t state) {
		// the type is popped last, so it is pushed first; then the sub-type, the state, the point, the radius and
		// excludingScripted
		return Call("CALL_NEAR_IN_STATE",
		            [state](lhvm::LHVM& vm) {
			            vm.Pushi(static_cast<int32_t>(script::ObjectType::Animal));
			            vm.Pushi(static_cast<int32_t>(ecs::script_type::k_AnySubtype));
			            vm.Pushi(static_cast<int32_t>(state));
			            vm.Pushv(100.0f);
			            vm.Pushv(0.0f);
			            vm.Pushv(100.0f);
			            vm.Pushf(30.0f);
			            vm.Pushb(false);
		            })
		    .uintVal;
	};
	sheepAt(glm::vec3(105.0f, 0.0f, 100.0f), ecs::animal_ai::AnimalState::Wander);
	EXPECT_EQ(deadNear(15), 0u);
	EXPECT_TRUE(_things->registered.empty());
	const auto dead = sheepAt(glm::vec3(110.0f, 0.0f, 100.0f), ecs::animal_ai::AnimalState::Dead);
	EXPECT_EQ(deadNear(15), static_cast<uint32_t>(dead));
	ASSERT_EQ(_things->registered.size(), 1u);
	EXPECT_EQ(_things->registered.back().first, dead);
	EXPECT_FALSE(_things->registered.back().second);
	// a state is a byte: one past 255 never matches
	EXPECT_EQ(deadNear(15 + 256), 0u);
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 0u);
	ecs::map_cells::Clear();
}

TEST_F(ScriptQueryNatives, GetDeadLivingFindsTheNearestDeadAnimalAsAFoundThing)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
	const test::RestoreService<Locator::chlapi> restoreApi;
	Locator::chlapi::emplace<chlapi::CHLApi>();
	ecs::map_cells::Clear();
	ecs::object_index::OnLoadMap();
	const auto sheepAt = [](const glm::vec3& at, ecs::animal_ai::AnimalState top) {
		const auto e = Reg().Create();
		ecs::object_index::Assign(e);
		Reg().Assign<Transform>(e, at, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<Animal>(e, AnimalInfo::Sheep, 0u);
		Reg().Assign<AnimalBrain>(e).topState = static_cast<uint8_t>(top);
		ecs::map_cells::InsertMapObject(e);
		return e;
	};
	const auto deadAt = [this](float radius) {
		// the point, then the radius
		return Call("GET_DEAD_LIVING",
		            [radius](lhvm::LHVM& vm) {
			            vm.Pushv(100.0f);
			            vm.Pushv(0.0f);
			            vm.Pushv(100.0f);
			            vm.Pushf(radius);
		            })
		    .uintVal;
	};
	// a living sheep is no dead thing
	sheepAt(glm::vec3(101.0f, 0.0f, 100.0f), ecs::animal_ai::AnimalState::Wander);
	EXPECT_EQ(deadAt(10.0f), 0u);
	EXPECT_TRUE(_things->registered.empty());
	// of two dead ones, the nearer
	sheepAt(glm::vec3(108.0f, 0.0f, 100.0f), ecs::animal_ai::AnimalState::Dead);
	const auto nearer = sheepAt(glm::vec3(104.0f, 0.0f, 100.0f), ecs::animal_ai::AnimalState::Dead);
	EXPECT_EQ(deadAt(10.0f), static_cast<uint32_t>(nearer));
	ASSERT_EQ(_things->registered.size(), 1u);
	EXPECT_EQ(_things->registered.back(), std::make_pair(nearer, false));
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 0u);
	ecs::map_cells::Clear();
}

TEST_F(ScriptQueryNatives, CallNearInStateOfATypeWithNoSearchGivesZero)
{
	const auto call = [this](int32_t type) {
		return Call("CALL_NEAR_IN_STATE",
		            [type](lhvm::LHVM& vm) {
			            vm.Pushi(type);
			            vm.Pushi(static_cast<int32_t>(ecs::script_type::k_AnySubtype));
			            vm.Pushi(15);
			            vm.Pushv(0.0f);
			            vm.Pushv(0.0f);
			            vm.Pushv(0.0f);
			            vm.Pushf(30.0f);
			            vm.Pushb(false);
		            })
		    .uintVal;
	};
	EXPECT_EQ(call(0), 0u);
	EXPECT_EQ(call(0x2A), 0u);
	EXPECT_EQ(call(static_cast<int32_t>(script::ObjectType::Flock)), 0u);
	EXPECT_TRUE(_things->registered.empty());
}

TEST_F(ScriptQueryNatives, CallNearOfACreatureTakesTheNewestOfTheGameListFirst)
{
	// two creatures of a species within the radius: the one made last is first in the game's list of creatures, whatever
	// order the registry keeps them in
	const auto creatureAt = [](const glm::vec3& at, uint32_t creationIndex) {
		const auto e = Reg().Create();
		Reg().Assign<Transform>(e, at, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<Creature>(e).species = CreatureType::Cow;
		Reg().Assign<ecs::components::ObjectCreationIndex>(e, creationIndex);
		return e;
	};
	const auto newer = creatureAt(glm::vec3(105.0f, 0.0f, 100.0f), 20);
	const auto older = creatureAt(glm::vec3(102.0f, 0.0f, 100.0f), 10);
	const auto cows = script::property_rules::ScriptCreatureType(CreatureType::Cow);
	const auto near = [this](uint32_t subtype, float radius) {
		// the type is popped last, so it is pushed first; then the sub-type, the point, the radius and excludingScripted
		return Call("CALL_NEAR",
		            [subtype, radius](lhvm::LHVM& vm) {
			            vm.Pushi(static_cast<int32_t>(script::ObjectType::Creature));
			            vm.Pushi(static_cast<int32_t>(subtype));
			            vm.Pushv(100.0f);
			            vm.Pushv(0.0f);
			            vm.Pushv(100.0f);
			            vm.Pushf(radius);
			            vm.Pushb(false);
		            })
		    .uintVal;
	};
	EXPECT_EQ(near(cows, 10.0f), static_cast<uint32_t>(newer));
	ASSERT_EQ(_things->registered.size(), 1u);
	EXPECT_EQ(_things->registered.back().first, newer);
	EXPECT_FALSE(_things->registered.back().second);
	// only the older one is within the radius
	EXPECT_EQ(near(cows, 3.0f), static_cast<uint32_t>(older));
	// a sub-type that is no creature's species finds none
	EXPECT_EQ(near(cows + 1, 10.0f), 0u);
}

TEST_F(ScriptQueryNatives, SetTargetOfNoThingOrWithNoTimeThrowsNothingAndDrawsNothing)
{
	const game_random::testing::ScopedState randomState;
	uint32_t draws = 0;
	game_random::testing::SetGameRand(nullptr, [&draws](float range) {
		++draws;
		return range / 2.0f;
	});
	const auto setTarget = [this](entt::entity thing, float time) {
		// the object is popped last, so it is pushed first; the time is popped first
		CallLeavingNothing("SET_TARGET", [thing, time](lhvm::LHVM& vm) {
			vm.Pusho(static_cast<uint32_t>(thing));
			vm.Pushv(10.0f);
			vm.Pushv(0.0f);
			vm.Pushv(20.0f);
			vm.Pushf(time);
		});
	};
	setTarget(entt::entity {0}, 2.0f);
	const auto rock = Reg().Create();
	Reg().Assign<Transform>(rock);
	setTarget(rock, 0.0f);
	setTarget(rock, -1.0f);
	// a town is not an object: nothing is thrown
	setTarget(MakeTown(31, PlayerNames::PLAYER_ONE, 1, 1), 2.0f);
	EXPECT_EQ(draws, 0u);
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

namespace
{
// A hand that holds what the test gives it
class HoldingHand final: public ecs::systems::HandGrabSystemInterface
{
public:
	void ForceDrop() override { held.reset(); }
	[[nodiscard]] std::optional<entt::entity> GetHeld() const override { return held; }
	std::optional<entt::entity> held;
};
} // namespace

TEST_F(ScriptQueryNatives, GetObjectHeldGivesWhatTheHandHoldsAsAFoundThing)
{
	// the first GET_OBJECT_HELD of the table is the hand's, which takes nothing; the creature's takes the creature
	test::RestoreService<Locator::handGrabSystem> restoreHand;
	auto& hand = static_cast<HoldingHand&>(Locator::handGrabSystem::emplace<HoldingHand>());
	const auto rock = Reg().Create();
	hand.held = rock;
	EXPECT_EQ(Call("GET_OBJECT_HELD", [](lhvm::LHVM& /*vm*/) {}).uintVal, static_cast<uint32_t>(rock));
	ASSERT_EQ(_things->registered.size(), 1u);
	EXPECT_EQ(_things->registered.front().first, rock);
	EXPECT_FALSE(_things->registered.front().second);
	// an empty hand: nothing, and nothing registered
	hand.held.reset();
	EXPECT_EQ(Call("GET_OBJECT_HELD", [](lhvm::LHVM& /*vm*/) {}).uintVal, 0u);
	EXPECT_EQ(_things->registered.size(), 1u);
	Locator::handGrabSystem::reset();
}

TEST_F(ScriptQueryNatives, GetObjectHeldWithNoHandGivesZero)
{
	test::RestoreService<Locator::handGrabSystem> restoreHand;
	Locator::handGrabSystem::reset();
	EXPECT_EQ(Call("GET_OBJECT_HELD", [](lhvm::LHVM& /*vm*/) {}).uintVal, 0u);
	EXPECT_TRUE(_things->registered.empty());
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

TEST_F(ScriptQueryNatives, FirstAndNextInContainerGiveTheFlocksMembersAsFoundThings)
{
	const auto head = Reg().Create();
	Reg().Assign<Animal>(head);
	const auto tail = Reg().Create();
	Reg().Assign<Animal>(tail);
	const auto flock = Reg().Create();
	Reg().Assign<Flock>(flock).members = {tail, head}; // from the head: head, tail
	const auto first = Call("GET_FIRST_IN_CONTAINER", [flock](lhvm::LHVM& vm) { vm.Pusho(static_cast<uint32_t>(flock)); });
	EXPECT_EQ(first.uintVal, static_cast<uint32_t>(head));
	const auto next = [this, flock](entt::entity after) {
		// the thing it follows is popped first, so the flock is pushed first
		return Call("GET_NEXT_IN_CONTAINER",
		            [flock, after](lhvm::LHVM& vm) {
			            vm.Pusho(static_cast<uint32_t>(flock));
			            vm.Pusho(static_cast<uint32_t>(after));
		            })
		    .uintVal;
	};
	EXPECT_EQ(next(head), static_cast<uint32_t>(tail));
	EXPECT_EQ(next(tail), 0u);
	EXPECT_EQ(next(entt::entity {0}), static_cast<uint32_t>(head));
	// each member given comes into the script's table as a found thing; none for no member
	EXPECT_EQ(_things->registered, (std::vector<std::pair<entt::entity, bool>> {{head, false}, {tail, false}, {head, false}}));
}

TEST_F(ScriptQueryNatives, CallInNearFindsTheFirstMemberWithinTheRadiusAsAFoundThing)
{
	const auto sheepAt = [](const glm::vec3& at) {
		const auto e = Reg().Create();
		Reg().Assign<Transform>(e, at, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<Animal>(e, AnimalInfo::Sheep, 0u);
		return e;
	};
	const auto distant = sheepAt(glm::vec3(200.0f, 0.0f, 100.0f));
	const auto nearby = sheepAt(glm::vec3(110.0f, 0.0f, 100.0f));
	const auto flock = Reg().Create();
	Reg().Assign<Flock>(flock).members = {nearby, distant}; // from the head: distant, then nearby
	const auto callInNear = [this](uint32_t container, script::ObjectType type, float radius) {
		// the type is popped last, so it is pushed first; then the sub-type, the container, the point, the radius and
		// excludingScripted
		return Call("CALL_IN_NEAR",
		            [container, type, radius](lhvm::LHVM& vm) {
			            vm.Pushi(static_cast<int32_t>(type));
			            vm.Pushi(static_cast<int32_t>(ecs::script_type::k_AnySubtype));
			            vm.Pusho(container);
			            vm.Pushv(100.0f);
			            vm.Pushv(0.0f);
			            vm.Pushv(100.0f);
			            vm.Pushf(radius);
			            vm.Pushb(false);
		            })
		    .uintVal;
	};
	// the head is out of the radius: the next member, which is in it
	EXPECT_EQ(callInNear(static_cast<uint32_t>(flock), script::ObjectType::Animal, 30.0f), static_cast<uint32_t>(nearby));
	ASSERT_EQ(_things->registered.size(), 1u);
	EXPECT_EQ(_things->registered.back(), std::make_pair(nearby, false));
	// a wider radius takes the head
	EXPECT_EQ(callInNear(static_cast<uint32_t>(flock), script::ObjectType::Animal, 150.0f), static_cast<uint32_t>(distant));
	// none within it, none of the type, no container or a thing that is no container: 0, and nothing more is added
	EXPECT_EQ(callInNear(static_cast<uint32_t>(flock), script::ObjectType::Animal, 5.0f), 0u);
	EXPECT_EQ(callInNear(static_cast<uint32_t>(flock), script::ObjectType::VillagerChild, 150.0f), 0u);
	EXPECT_EQ(callInNear(0, script::ObjectType::Animal, 150.0f), 0u);
	EXPECT_EQ(callInNear(static_cast<uint32_t>(nearby), script::ObjectType::Animal, 150.0f), 0u);
	EXPECT_EQ(_things->registered.size(), 2u);
}

TEST_F(ScriptQueryNatives, CallInNotNearFindsTheFirstMemberBeyondTheRadiusAsAFoundThing)
{
	const auto sheepAt = [](const glm::vec3& at) {
		const auto e = Reg().Create();
		Reg().Assign<Transform>(e, at, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<Animal>(e, AnimalInfo::Sheep, 0u);
		return e;
	};
	const auto nearby = sheepAt(glm::vec3(110.0f, 0.0f, 100.0f));
	const auto distant = sheepAt(glm::vec3(200.0f, 0.0f, 100.0f));
	const auto flock = Reg().Create();
	Reg().Assign<Flock>(flock).members = {distant, nearby}; // from the head: nearby, then distant
	const auto callInNotNear = [this](uint32_t container, script::ObjectType type, float radius) {
		// pushed as CALL_IN_NEAR's: the type, the sub-type, the container, the point, the radius and excludingScripted
		return Call("CALL_IN_NOT_NEAR",
		            [container, type, radius](lhvm::LHVM& vm) {
			            vm.Pushi(static_cast<int32_t>(type));
			            vm.Pushi(static_cast<int32_t>(ecs::script_type::k_AnySubtype));
			            vm.Pusho(container);
			            vm.Pushv(100.0f);
			            vm.Pushv(0.0f);
			            vm.Pushv(100.0f);
			            vm.Pushf(radius);
			            vm.Pushb(false);
		            })
		    .uintVal;
	};
	// the head is within the radius: the next member, which is beyond it
	EXPECT_EQ(callInNotNear(static_cast<uint32_t>(flock), script::ObjectType::Animal, 30.0f), static_cast<uint32_t>(distant));
	ASSERT_EQ(_things->registered.size(), 1u);
	EXPECT_EQ(_things->registered.back(), std::make_pair(distant, false));
	// a smaller radius takes the head
	EXPECT_EQ(callInNotNear(static_cast<uint32_t>(flock), script::ObjectType::Animal, 5.0f), static_cast<uint32_t>(nearby));
	// one just at the radius is not beyond it
	const float distantAt = gutils::GetDistanceInMetres(map_coords::FromWorld(glm::vec3(100.0f, 0.0f, 100.0f)),
	                                                    map_coords::FromWorld(glm::vec3(200.0f, 0.0f, 100.0f)));
	EXPECT_EQ(callInNotNear(static_cast<uint32_t>(flock), script::ObjectType::Animal, distantAt), 0u);
	// none beyond it, none of the type, no container or a thing that is no container: 0, and nothing more is added
	EXPECT_EQ(callInNotNear(static_cast<uint32_t>(flock), script::ObjectType::Animal, 150.0f), 0u);
	EXPECT_EQ(callInNotNear(static_cast<uint32_t>(flock), script::ObjectType::VillagerChild, 5.0f), 0u);
	EXPECT_EQ(callInNotNear(0, script::ObjectType::Animal, 5.0f), 0u);
	EXPECT_EQ(callInNotNear(static_cast<uint32_t>(nearby), script::ObjectType::Animal, 5.0f), 0u);
	EXPECT_EQ(_things->registered.size(), 2u);
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

// A villager row's grown-up age sits where the original reads it (the info.dat record starts 16 bytes into it)
static_assert(offsetof(GVillagerInfo, grownUpAge) == 0x138 - 0x10);

TEST_F(ScriptQueryNatives, CreateRandomVillagerOfATribeOutsideTheTableDrawsThenMakesNothing)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
	const game_random::testing::ScopedState randomState;
	std::vector<uint32_t> draws;
	game_random::testing::SetGameRand(
	    [&draws](uint32_t n) {
		    draws.push_back(n);
		    return 3u;
	    },
	    nullptr);
	for (const int32_t tribe : {12, 99, -1})
	{
		// the tribe is popped last, so it is pushed first; then the point
		const auto made = Call("CREATE_RANDOM_VILLAGER_OF_TRIBE", [tribe](lhvm::LHVM& vm) {
			vm.Pushi(tribe);
			vm.Pushv(100.0f);
			vm.Pushv(0.0f);
			vm.Pushv(200.0f);
		});
		EXPECT_EQ(made.uintVal, 0u) << tribe;
	}
	// one draw of seven each time, before the row is looked at
	EXPECT_EQ(draws, (std::vector<uint32_t> {7, 7, 7}));
	EXPECT_TRUE(_things->registered.empty());
	size_t villagers = 0;
	Reg().Each<const Villager>([&villagers](entt::entity, const Villager&) { ++villagers; });
	EXPECT_EQ(villagers, 0u);
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

TEST_F(ScriptQueryNatives, SetFocusTurnsAMobileStaticWithTheLeansItKeeps)
{
	const glm::vec3 at(100.0f, 0.0f, 40.0f);
	const glm::vec3 target(90.0f, 0.0f, 50.0f);
	const auto rock = [at](const glm::mat3& rotation) {
		const auto e = Reg().Create();
		Reg().Assign<MobileStatic>(e, MobileStaticInfo::Rock);
		Reg().Assign<Transform>(e, at, rotation, glm::vec3(1.0f));
		return e;
	};
	const float facing = ecs::script_set_position::FocusYAngle(at, target);
	// upright: turned about Y, x and z at 0
	const auto upright = rock(affine::AngleY(1.0f));
	SetFocus(upright, target);
	EXPECT_EQ(Reg().Get<Transform>(upright).rotation, affine::RotationYXZ(facing, 0.0f, 0.0f));
	// leaning: turned about Y with the leans it keeps
	const auto leaning = rock(affine::RotationYXZ(1.0f, 0.3f, 0.0f));
	ecs::placed_turn::KeepAngles(Reg(), leaning, glm::vec3(0.3f, 1.0f, 0.0f));
	SetFocus(leaning, target);
	EXPECT_EQ(Reg().Get<Transform>(leaning).rotation, affine::RotationYXZ(facing, 0.3f, 0.0f));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(Reg(), leaning), glm::vec3(0.3f, facing, 0.0f));
	// moved by YPOS it stands upright on its angle about y, and keeps its leans for the next angle set
	constexpr int32_t k_YPos = 24;
	SetPropertyOf(leaning, k_YPos, 0.0f);
	EXPECT_EQ(Reg().Get<Transform>(leaning).rotation, affine::AngleY(facing));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(Reg(), leaning), glm::vec3(0.3f, facing, 0.0f));
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

TEST_F(ScriptQueryNatives, SetObjectCarryingGivesAVillagerTheObjectToHold)
{
	const test::RestoreService<Locator::chlapi> restoreApi;
	Locator::chlapi::emplace<chlapi::CHLApi>();
	const auto villager = Reg().Create();
	Reg().Assign<Villager>(villager);
	Reg().Assign<SkeletalAnimation>(villager);
	// Land 2's disciple is given a heavy mallet (9), Land 5's villager a fishing rod (3)
	SetObjectCarrying(villager, static_cast<int32_t>(CarriedObject::MalletHeavy));
	EXPECT_EQ(Reg().Get<SkeletalAnimation>(villager).carriedObject, static_cast<int32_t>(CarriedObject::MalletHeavy));
	SetObjectCarrying(villager, static_cast<int32_t>(CarriedObject::FishingRod));
	EXPECT_EQ(Reg().Get<SkeletalAnimation>(villager).carriedObject, static_cast<int32_t>(CarriedObject::FishingRod));
	// kept to a byte, as the game keeps it
	SetObjectCarrying(villager, 0x102);
	EXPECT_EQ(Reg().Get<SkeletalAnimation>(villager).carriedObject, static_cast<int32_t>(CarriedObject::Axe));
	// no longer a stub, and nothing is found or made for the script
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 0u);
	EXPECT_TRUE(_things->registered.empty());
}

TEST_F(ScriptQueryNatives, SetObjectCarryingGivesAVillagerWithoutAnAnimationOneToHoldIt)
{
	const auto villager = Reg().Create();
	Reg().Assign<Villager>(villager);
	SetObjectCarrying(villager, static_cast<int32_t>(CarriedObject::Axe));
	ASSERT_TRUE(Reg().AllOf<SkeletalAnimation>(villager));
	EXPECT_EQ(Reg().Get<SkeletalAnimation>(villager).carriedObject, static_cast<int32_t>(CarriedObject::Axe));
}

TEST_F(ScriptQueryNatives, SetObjectCarryingRefusesAThingThatIsNotAVillager)
{
	// a creature is not a villager: only the message
	const auto creature = Reg().Create();
	Reg().Assign<Creature>(creature);
	Reg().Assign<SkeletalAnimation>(creature);
	SetObjectCarrying(creature, static_cast<int32_t>(CarriedObject::Axe));
	EXPECT_EQ(Reg().Get<SkeletalAnimation>(creature).carriedObject, static_cast<int32_t>(CarriedObject::None));
	// no thing: only the message
	SetObjectCarrying(entt::entity {0}, static_cast<int32_t>(CarriedObject::Axe));
}

TEST_F(ScriptQueryNatives, PlayerOfATownIsItsOwnerNumberedFromOne)
{
	const auto one = MakeTown(20, PlayerNames::PLAYER_ONE, 1, 1);
	MakeAbode(20);
	const auto three = MakeTown(21, PlayerNames::PLAYER_THREE, 1, 2);
	MakeAbode(21);
	const auto value = PlayerOf(one);
	EXPECT_EQ(value.floatVal, 1.0f);
	// the answer is a float, not an integer
	EXPECT_EQ(std::bit_cast<uint32_t>(value.floatVal), 0x3F800000u);
	EXPECT_EQ(PlayerOf(three).floatVal, 3.0f);
}

TEST_F(ScriptQueryNatives, PlayerOfANeutralTownIsZero)
{
	const auto town = MakeTown(20, PlayerNames::NEUTRAL, 1, 1);
	MakeAbode(20);
	EXPECT_EQ(PlayerOf(town).floatVal, 0.0f);
}

TEST_F(ScriptQueryNatives, PlayerOfATownWithNoStandingBuildingIsOne)
{
	// a town none of whose buildings stands is 1, whoever it belongs to
	const auto ruined = MakeTown(20, PlayerNames::PLAYER_TWO, 1, 1);
	Reg().Assign<Life>(MakeAbode(20), 0.0f);
	EXPECT_EQ(PlayerOf(ruined).floatVal, 1.0f);
	const auto begun = MakeTown(21, PlayerNames::PLAYER_TWO, 1, 2);
	MakeAbode(21, 0.1f);
	EXPECT_EQ(PlayerOf(begun).floatVal, 1.0f);
	const auto empty = MakeTown(22, PlayerNames::NEUTRAL, 1, 3);
	EXPECT_EQ(PlayerOf(empty).floatVal, 1.0f);
	// a field is no standing building
	const auto fields = MakeTown(23, PlayerNames::PLAYER_TWO, 1, 4);
	Reg().Assign<Field>(MakeAbode(23), 23);
	EXPECT_EQ(PlayerOf(fields).floatVal, 1.0f);
	// one building a little more than a tenth built stands
	const auto standing = MakeTown(24, PlayerNames::PLAYER_TWO, 1, 5);
	MakeAbode(24, 0.11f);
	EXPECT_EQ(PlayerOf(standing).floatVal, 2.0f);
}

TEST_F(ScriptQueryNatives, PlayerOfAnAbodeIsItsTownsOwner)
{
	const test::RestoreService<Locator::chlapi> restoreApi;
	Locator::chlapi::emplace<chlapi::CHLApi>();
	MakeTown(20, PlayerNames::PLAYER_TWO, 1, 1);
	EXPECT_EQ(PlayerOf(MakeAbode(20)).floatVal, 2.0f);
	EXPECT_EQ(PlayerOf(MakeAbode(Abode::k_NoTown)).floatVal, 0.0f);
	// a field is not worked out yet: the unwritten property's 0
	const auto field = MakeAbode(20);
	Reg().Assign<Field>(field, 20);
	EXPECT_EQ(PlayerOf(field).floatVal, 0.0f);
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 1u);
}

TEST_F(ScriptQueryNatives, PlayerOfAVillagerIsItsTownsOwner)
{
	const auto town = MakeTown(20, PlayerNames::PLAYER_TWO, 1, 1);
	const auto villager = Reg().Create();
	Reg().Assign<Villager>(villager).town = town;
	EXPECT_EQ(PlayerOf(villager).floatVal, 2.0f);
	const auto homeless = Reg().Create();
	Reg().Assign<Villager>(homeless).town = entt::null;
	EXPECT_EQ(PlayerOf(homeless).floatVal, 0.0f);
}

TEST_F(ScriptQueryNatives, PlayerOfACreatureIsItsOwner)
{
	const auto creature = Reg().Create();
	Reg().Assign<Creature>(creature).owner = PlayerNames::PLAYER_ONE;
	EXPECT_EQ(PlayerOf(creature).floatVal, 1.0f);
	const auto wild = Reg().Create();
	Reg().Assign<Creature>(wild).owner = PlayerNames::NEUTRAL;
	EXPECT_EQ(PlayerOf(wild).floatVal, 0.0f);
}

TEST_F(ScriptQueryNatives, PlayerOfOtherThingsIsZero)
{
	const test::RestoreService<Locator::chlapi> restoreApi;
	Locator::chlapi::emplace<chlapi::CHLApi>();
	// a kind of thing whose player is not worked out yet: the unwritten property's float 0, counted as a stub
	const auto value = PlayerOf(Reg().Create());
	EXPECT_EQ(std::bit_cast<uint32_t>(value.floatVal), 0u);
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 1u);
	// a thing no longer there
	EXPECT_EQ(PlayerOf(entt::entity {0}).floatVal, 0.0f);
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 1u);
}

TEST_F(ScriptQueryNatives, SetDiscipleMakesAVillagerADiscipleOfTheType)
{
	const test::RestoreService<Locator::chlapi> restoreApi;
	Locator::chlapi::emplace<chlapi::CHLApi>();
	const auto villager = Reg().Create();
	auto& v = Reg().Assign<Villager>(villager);
	v.town = entt::null;
	v.flags = Villager::k_FlagDiscipleFollower;
	// a farmer (1): a disciple, no longer a follower, whatever the sound says
	SetDisciple(villager, 1, 0);
	EXPECT_EQ(Reg().Get<Villager>(villager).flags, Villager::k_FlagDisciple);
	EXPECT_EQ(Reg().Get<Villager>(villager).discipleType, 1);
	// from the vortex (12), the last of the table
	SetDisciple(villager, 12);
	EXPECT_EQ(Reg().Get<Villager>(villager).discipleType, 12);
	// 0: an ordinary villager again
	SetDisciple(villager, 0);
	EXPECT_EQ(Reg().Get<Villager>(villager).flags, 0);
	EXPECT_EQ(Reg().Get<Villager>(villager).discipleType, 0);
	// no longer a stub, and nothing is found or made for the script
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 0u);
	EXPECT_TRUE(_things->registered.empty());
}

TEST_F(ScriptQueryNatives, SetDiscipleOfATypeOutsideTheTableChangesNothing)
{
	const auto villager = Reg().Create();
	auto& v = Reg().Assign<Villager>(villager);
	v.town = entt::null;
	v.flags = Villager::k_FlagDisciple;
	v.discipleType = 3;
	SetDisciple(villager, 13);
	SetDisciple(villager, -1);
	EXPECT_EQ(Reg().Get<Villager>(villager).flags, Villager::k_FlagDisciple);
	EXPECT_EQ(Reg().Get<Villager>(villager).discipleType, 3);
}

TEST_F(ScriptQueryNatives, SetDiscipleRefusesAThingThatIsNotAVillager)
{
	// a creature is not a villager: only the message
	const auto creature = Reg().Create();
	Reg().Assign<Creature>(creature);
	SetDisciple(creature, 1);
	EXPECT_FALSE(Reg().AllOf<Villager>(creature));
	// no thing: only the message
	SetDisciple(entt::entity {0}, 1);
}

TEST_F(ScriptQueryNatives, GetObjectHeldOfACreatureGivesWhatItHoldsAsAFoundThing)
{
	test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	auto& hands = static_cast<test::creature_fakes::FakeObjectAction&>(
	    Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>());
	const auto creature = Reg().Create();
	Reg().Assign<Creature>(creature);
	// an empty hand: 0, nothing found
	EXPECT_EQ(HeldByCreature(creature), 0u);
	EXPECT_TRUE(_things->registered.empty());
	// a thing in its hand: that thing, found, not made by the script
	const auto rock = Reg().Create();
	hands.held = rock;
	EXPECT_EQ(HeldByCreature(creature), static_cast<uint32_t>(rock));
	ASSERT_EQ(_things->registered.size(), 1u);
	EXPECT_EQ(_things->registered.front().first, rock);
	EXPECT_FALSE(_things->registered.front().second);
	Locator::creatureObjectActionSystem::reset();
}

TEST_F(ScriptQueryNatives, GetObjectHeldOfACreatureOfNoCreatureGivesZero)
{
	test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	auto& hands = static_cast<test::creature_fakes::FakeObjectAction&>(
	    Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>());
	hands.held = Reg().Create();
	// a villager is not a creature ("Not creature"), and no thing is none ("No creature"): 0 either way
	const auto villager = Reg().Create();
	Reg().Assign<Villager>(villager).town = entt::null;
	EXPECT_EQ(HeldByCreature(villager), 0u);
	EXPECT_EQ(HeldByCreature(entt::entity {0}), 0u);
	EXPECT_TRUE(_things->registered.empty());
	Locator::creatureObjectActionSystem::reset();
}

TEST_F(ScriptQueryNatives, FlyingIsWhetherTheThingIsInThePhysicsAsABool)
{
	constexpr int32_t k_Flying = 5;
	const auto flying = [this](entt::entity thing) {
		return CallTyped("GET_PROPERTY", [thing](lhvm::LHVM& vm) {
			vm.Pushi(k_Flying);
			vm.Pusho(static_cast<uint32_t>(thing));
		});
	};
	// thrown and still moving, carried by a tornado or left hanging: the physics marks it, and it is flying
	const auto rock = Reg().Create();
	Reg().Assign<MobileStatic>(rock, MobileStaticInfo::Rock);
	Reg().Assign<Transform>(rock);
	Reg().Assign<ecs::components::InPhysics>(rock);
	const auto [inPhysics, type] = flying(rock);
	EXPECT_EQ(type, lhvm::DataType::Boolean);
	EXPECT_EQ(inPhysics.intVal, 1);
	// come to rest it loses the mark, though the physics may keep its body to be knocked again: not flying
	Reg().Remove<ecs::components::InPhysics>(rock);
	EXPECT_EQ(flying(rock).first.intVal, 0);
	// a thing without the mark, whatever it is, is not flying
	EXPECT_EQ(flying(Reg().Create()).first.intVal, 0);
}

TEST_F(ScriptQueryNatives, AnObjectsAnglesAreTheOnesItKeeps)
{
	using script::property_rules::AngleToScript;
	constexpr int32_t k_Angle = 2;
	constexpr int32_t k_XAngle = 3;
	constexpr int32_t k_ZAngle = 4;
	// a rock leaning 0.5 about x and 0.25 about z, turned 1 about y: the angles it keeps, not the ones of its turn
	const auto rock = Reg().Create();
	Reg().Assign<MobileStatic>(rock, MobileStaticInfo::Rock);
	Reg().Assign<Transform>(rock, glm::vec3(100.0f, 0.0f, 40.0f), affine::RotationYXZ(1.0f, 0.5f, 0.25f), glm::vec3(1.0f));
	ecs::placed_turn::KeepAngles(Reg(), rock, glm::vec3(0.5f, 1.0f, 0.25f));
	EXPECT_EQ(std::bit_cast<uint32_t>(PropertyOf(rock, k_Angle)), 0x42652EE0u);
	EXPECT_EQ(PropertyOf(rock, k_XAngle), AngleToScript(0.5f));
	EXPECT_EQ(PropertyOf(rock, k_ZAngle), AngleToScript(0.25f));
	// XANGLE 0: the other two kept, the turn made again from the three
	SetPropertyOf(rock, k_XAngle, 0.0f);
	EXPECT_EQ(ecs::placed_turn::AnglesOf(Reg(), rock), glm::vec3(0.0f, 1.0f, 0.25f));
	EXPECT_EQ(Reg().Get<Transform>(rock).rotation, affine::RotationYXZ(1.0f, 0.0f, 0.25f));
	// ANGLE 90: the original's 1.5707964 about y, the leans kept
	SetPropertyOf(rock, k_Angle, 90.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(ecs::placed_turn::AnglesOf(Reg(), rock).y), 0x3FC90FDBu);
	EXPECT_EQ(ecs::placed_turn::AnglesOf(Reg(), rock).z, 0.25f);
	// a feature never leans: its leans read 0 and setting one leaves its angles
	const auto feature = Reg().Create();
	Reg().Assign<Feature>(feature, FeatureInfo {});
	Reg().Assign<Transform>(feature, glm::vec3(100.0f, 0.0f, 40.0f), affine::AngleY(1.0f), glm::vec3(1.0f));
	ecs::placed_turn::KeepAngles(Reg(), feature, glm::vec3(0.0f, 1.0f, 0.0f));
	SetPropertyOf(feature, k_XAngle, 45.0f);
	EXPECT_EQ(PropertyOf(feature, k_XAngle), 0.0f);
	EXPECT_EQ(ecs::placed_turn::AnglesOf(Reg(), feature), glm::vec3(0.0f, 1.0f, 0.0f));
	EXPECT_EQ(Reg().Get<Transform>(feature).rotation, affine::AngleY(1.0f));
}

TEST_F(ScriptQueryNatives, ACreaturesAngleIsTheScriptsOwnNotTheWayItFaces)
{
	constexpr int32_t k_Angle = 2;
	constexpr int32_t k_XAngle = 3;
	constexpr int32_t k_ZAngle = 4;
	// a creature facing 1 about y: its angle is the 0 every creature is made with, and it never leans
	const auto creature = Reg().Create();
	Reg().Assign<Creature>(creature);
	const auto facing = affine::AngleY(1.0f);
	Reg().Assign<Transform>(creature, glm::vec3(100.0f, 0.0f, 40.0f), facing, glm::vec3(1.0f));
	EXPECT_EQ(PropertyOf(creature, k_Angle), 0.0f);
	EXPECT_EQ(PropertyOf(creature, k_XAngle), 0.0f);
	EXPECT_EQ(PropertyOf(creature, k_ZAngle), 0.0f);
	// ANGLE 90 keeps the original's 1.5707964; the way the creature faces stays its own
	SetPropertyOf(creature, k_Angle, 90.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(Reg().Get<Creature>(creature).scriptYAngle), 0x3FC90FDBu);
	EXPECT_EQ(Reg().Get<Transform>(creature).rotation, facing);
	// an angle of 1 reads 57.295776, as the original's
	Reg().Get<Creature>(creature).scriptYAngle = 1.0f;
	EXPECT_EQ(std::bit_cast<uint32_t>(PropertyOf(creature, k_Angle)), 0x42652EE0u);
	SetPropertyOf(creature, k_Angle, 160.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(Reg().Get<Creature>(creature).scriptYAngle), 0x4032B8C2u);
	// a lean is not kept and leaves the angle about y as it is
	SetPropertyOf(creature, k_XAngle, 45.0f);
	SetPropertyOf(creature, k_ZAngle, 45.0f);
	EXPECT_EQ(PropertyOf(creature, k_XAngle), 0.0f);
	EXPECT_EQ(PropertyOf(creature, k_ZAngle), 0.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(Reg().Get<Creature>(creature).scriptYAngle), 0x4032B8C2u);
	EXPECT_EQ(Reg().Get<Transform>(creature).rotation, facing);
}

TEST_F(ScriptQueryNatives, ACreaturesScaleIsTheSizeItsBodyIsDrawnAt)
{
	constexpr int32_t k_Scale = 8;
	const auto creature = Reg().Create();
	Reg().Assign<Creature>(creature, Creature {.size = 2.5f});
	Reg().Assign<Transform>(creature, glm::vec3(100.0f, 0.0f, 40.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	// before its body is given a size: its own
	EXPECT_EQ(PropertyOf(creature, k_Scale), 2.5f);
	// kept within the sizes a body may take, 0.05 to 4
	Reg().Get<Creature>(creature).size = 6.0f;
	EXPECT_EQ(PropertyOf(creature, k_Scale), 4.0f);
	Reg().Get<Creature>(creature).size = 0.01f;
	EXPECT_EQ(PropertyOf(creature, k_Scale), 0.05f);
	// in its temple's pen: the pen's size, its own kept
	Reg().Get<Creature>(creature).size = 2.5f;
	Reg().Assign<CreatureDrawPose>(creature, CreatureDrawPose {.size = 0.22f});
	EXPECT_EQ(PropertyOf(creature, k_Scale), 0.22f);
	EXPECT_EQ(Reg().Get<Creature>(creature).size, 2.5f);
	// what SET gives it is read back at once, so the guide's "grow by a tenth until 1.9" ends
	Reg().Get<CreatureDrawPose>(creature).size.reset();
	SetPropertyOf(creature, k_Scale, 0.1f);
	EXPECT_EQ(PropertyOf(creature, k_Scale), 0.1f);
	int steps = 0;
	for (; steps < 100 && PropertyOf(creature, k_Scale) < 1.9f; ++steps)
	{
		SetPropertyOf(creature, k_Scale, PropertyOf(creature, k_Scale) + 0.1f);
	}
	EXPECT_LT(steps, 100);
	EXPECT_GE(PropertyOf(creature, k_Scale), 1.9f);
	EXPECT_EQ(Reg().Get<Creature>(creature).size, PropertyOf(creature, k_Scale));
}

TEST_F(ScriptQueryNatives, EveryObjectsScaleIsItsOwnAndAnyOtherThingsIsOne)
{
	constexpr int32_t k_Scale = 8;
	// the Blind Woman's boulder, made at 0.3, then shrunk by the script while it reads more than 0.1
	const auto boulder = Reg().Create();
	Reg().Assign<MobileStatic>(boulder, MobileStaticInfo::Rock);
	Reg().Assign<Transform>(boulder, glm::vec3(100.0f, 0.0f, 40.0f), glm::mat3(1.0f), glm::vec3(0.3f));
	EXPECT_EQ(PropertyOf(boulder, k_Scale), 0.3f);
	SetPropertyOf(boulder, k_Scale, PropertyOf(boulder, k_Scale) - 0.05f);
	EXPECT_EQ(PropertyOf(boulder, k_Scale), 0.3f - 0.05f);
	// a villager's is its own as before
	const auto villager = Reg().Create();
	Reg().Assign<Villager>(villager).town = entt::null;
	Reg().Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.25f));
	EXPECT_EQ(PropertyOf(villager, k_Scale), 1.25f);
	// a mist is no object: the scale every thing has, not its own size
	const auto mist = Reg().Create();
	Reg().Assign<Mist>(mist).size = 4.0f;
	Reg().Assign<Transform>(mist, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(4.0f));
	EXPECT_EQ(PropertyOf(mist, k_Scale), 1.0f);
}

TEST_F(ScriptQueryNatives, AScriptsScaleOfASpecialEffectIsItsMagnitude)
{
	constexpr int32_t k_Scale = 8;
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = static_cast<MagnitudeRecorder&>(Locator::particleSystem::emplace<MagnitudeRecorder>());
	// a special effect a script made: its effect's magnitude, not its object's scale
	const auto effect = Reg().Create();
	Reg().Assign<Transform>(effect, glm::vec3(100.0f, 0.0f, 40.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	particles.visual = effect;
	SetPropertyOf(effect, k_Scale, 0.4f);
	EXPECT_EQ(particles.magnitudes, std::vector<float> {0.4f});
	EXPECT_EQ(Reg().Get<Transform>(effect).scale, glm::vec3(1.0f));
	// a mist is no object and keeps the scale it has
	const auto mist = Reg().Create();
	Reg().Assign<Mist>(mist).size = 4.0f;
	Reg().Assign<Transform>(mist, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(4.0f));
	SetPropertyOf(mist, k_Scale, 2.0f);
	EXPECT_EQ(Reg().Get<Transform>(mist).scale, glm::vec3(4.0f));
	EXPECT_EQ(particles.magnitudes.size(), 1u);
}

TEST_F(ScriptQueryNatives, AnObjectsHeightIsItsOwnAndAnyOtherThingHasNone)
{
	constexpr int32_t k_Height = 19;
	const test::RestoreService<Locator::chlapi> restoreApi;
	Locator::chlapi::emplace<chlapi::CHLApi>();
	const auto heightOf = [this](entt::entity thing) {
		return CallTyped("GET_PROPERTY", [thing](lhvm::LHVM& vm) {
			vm.Pushi(k_Height);
			vm.Pusho(static_cast<uint32_t>(thing));
		});
	};
	// a rock with no mesh loaded is as high as the object height gives, 0; a mist is no object and has no height
	const auto rock = Reg().Create();
	Reg().Assign<MobileStatic>(rock, MobileStaticInfo::Rock);
	Reg().Assign<Transform>(rock, glm::vec3(100.0f, 0.0f, 40.0f), glm::mat3(1.0f), glm::vec3(2.0f));
	const auto mist = Reg().Create();
	Reg().Assign<Mist>(mist).size = 4.0f;
	for (const auto thing : {rock, mist})
	{
		const auto [given, type] = heightOf(thing);
		EXPECT_EQ(type, lhvm::DataType::Float);
		EXPECT_EQ(std::bit_cast<uint32_t>(given.floatVal), 0u);
	}
	// setting either keeps no height and changes nothing
	SetPropertyOf(rock, k_Height, 5.0f);
	SetPropertyOf(mist, k_Height, 5.0f);
	EXPECT_EQ(Reg().Get<Transform>(rock).scale, glm::vec3(2.0f));
	EXPECT_EQ(Reg().Get<Transform>(rock).position, glm::vec3(100.0f, 0.0f, 40.0f));
	// none of it is counted as not written
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 0u);
}

TEST_F(ScriptQueryNatives, ACitadelFoundByAScriptIsItsHeartAndItsHealthIsTheHeartsLife)
{
	constexpr int32_t k_Health = 1;
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
	// the only thing of the citadel's script type is its heart, an object: its health is the heart's life, as Land 2
	// reads the other god's citadel to see it destroyed
	const auto heart = Reg().Create();
	Reg().Assign<Temple>(heart, PlayerNames::PLAYER_THREE);
	EXPECT_EQ(ecs::script_type::TypeOf(heart), script::ObjectType::Citadel);
	EXPECT_EQ(PropertyOf(heart, k_Health), 1.0f);
	Reg().Assign<Life>(heart).value = 0.05f;
	EXPECT_EQ(PropertyOf(heart, k_Health), 0.05f);
}

TEST_F(ScriptQueryNatives, ACreatureIsAskedForAFractionOfItsTopSpeed)
{
	constexpr int32_t k_Speed = 11;
	const auto creature = Reg().Create();
	Reg().Assign<Creature>(creature);
	Reg().Assign<CreatureLocomotion>(creature);
	SetPropertyOf(creature, k_Speed, 0.25f);
	EXPECT_EQ(Reg().Get<CreatureLocomotion>(creature).fraction, 0.25f);
	// the guide's 40.5 is all of it
	SetPropertyOf(creature, k_Speed, 40.5f);
	EXPECT_EQ(Reg().Get<CreatureLocomotion>(creature).fraction, 1.0f);
	SetPropertyOf(creature, k_Speed, -3.0f);
	EXPECT_EQ(Reg().Get<CreatureLocomotion>(creature).fraction, 0.0f);
}

TEST_F(ScriptQueryNatives, AFlockHasNoPropertyOfItsOwnButItsPlayer)
{
	constexpr int32_t k_Height = 19;
	constexpr int32_t k_BuiltPercentage = 22;
	// a float 0 for the height and the built share, which other things that are not built read as 1
	const auto flock = Reg().Create();
	Reg().Assign<Flock>(flock);
	for (const int32_t property : {k_Height, k_BuiltPercentage})
	{
		const auto [given, type] = CallTyped("GET_PROPERTY", [flock, property](lhvm::LHVM& vm) {
			vm.Pushi(property);
			vm.Pusho(static_cast<uint32_t>(flock));
		});
		EXPECT_EQ(type, lhvm::DataType::Float) << property;
		EXPECT_EQ(std::bit_cast<uint32_t>(given.floatVal), 0u) << property;
	}
}

TEST_F(ScriptQueryNatives, SetPropertyOfAFlockSetsEveryMemberAsItself)
{
	constexpr int32_t k_Scale = 8;
	// a flock of two villagers: each member gets the scale, the flock has none of its own to keep
	const auto flock = Reg().Create();
	std::vector<entt::entity> members;
	for (int i = 0; i < 2; ++i)
	{
		const auto villager = Reg().Create();
		Reg().Assign<Villager>(villager).town = entt::null;
		Reg().Assign<Transform>(villager, glm::vec3(10.0f * static_cast<float>(i), 0.0f, 0.0f), glm::mat3(1.0f),
		                        glm::vec3(1.0f));
		members.push_back(villager);
	}
	Reg().Assign<Flock>(flock).members = members;
	SetPropertyOf(flock, k_Scale, 1.5f);
	for (const auto member : members)
	{
		EXPECT_EQ(Reg().Get<Transform>(member).scale, glm::vec3(1.5f));
	}
	EXPECT_FALSE(Reg().AllOf<Transform>(flock));
}

// A weather thing is played once its storm is over, by the storm it still holds; anything else that is not living
// answers yes
TEST_F(ScriptQueryNatives, PlayedOfAWeatherThingIsWhetherItsStormIsOver)
{
	const test::RestoreService<Locator::chlapi> restoreApi;
	Locator::chlapi::emplace<chlapi::CHLApi>();
	const auto thing = Reg().Create();
	auto& weather = Reg().Assign<openblack::ecs::components::WeatherThing>(thing);
	const auto played = [this, thing] {
		return Call("PLAYED", [thing](lhvm::LHVM& vm) { vm.Pusho(static_cast<uint32_t>(thing)); }).intVal;
	};
	weather.storm = 7;
	EXPECT_EQ(played(), 0);
	weather.storm = openblack::weather::storms::k_NoStorm;
	EXPECT_EQ(played(), 1);
}

namespace
{
/// An animal of `species` at `at`, in the map cells the scripts' searches walk
entt::entity AnimalInCells(AnimalInfo species, const glm::vec3& at)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto e = registry.Create();
	ecs::object_index::Assign(e);
	registry.Assign<Transform>(e, at, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Animal>(e, species, 0u);
	ecs::map_cells::InsertMapObject(e);
	return e;
}

/// A script holds it in a variable: it is in a script
void HeldByAScript(entt::entity thing)
{
	ecs::script_held::AddScriptThing(thing, false);
	ecs::script_held::IncrementReference(thing);
}
} // namespace

TEST_F(ScriptQueryNatives, CallNearFindsTheNearestThingOfTheKindWithinTheRadiusAsAFoundThing)
{
	// The nearest thing of the map cells that is of the type and sub-type and within the radius of the point, added to
	// the script as a found thing and pushed as an object (docs/wiki/scripts/script-commands.md#finding-things)
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
	ecs::map_cells::Clear();
	ecs::object_index::OnLoadMap();
	const auto callNear = [this](uint32_t subtype, float radius) {
		// the type is popped last, so it is pushed first; then the sub-type, the point, the radius and excludingScripted
		return CallTyped("CALL_NEAR", [subtype, radius](lhvm::LHVM& vm) {
			vm.Pushi(static_cast<int32_t>(script::ObjectType::Animal));
			vm.Pushi(static_cast<int32_t>(subtype));
			vm.Pushv(100.0f);
			vm.Pushv(0.0f);
			vm.Pushv(100.0f);
			vm.Pushf(radius);
			vm.Pushb(false);
		});
	};
	const auto sheep = static_cast<uint32_t>(AnimalInfo::Sheep);
	// the farther sheep goes into the cells first: the nearer one is found all the same
	AnimalInCells(AnimalInfo::Sheep, glm::vec3(120.0f, 0.0f, 100.0f));
	const auto nearer = AnimalInCells(AnimalInfo::Sheep, glm::vec3(105.0f, 0.0f, 100.0f));
	const auto cow = AnimalInCells(AnimalInfo::Cow, glm::vec3(101.0f, 0.0f, 100.0f));
	const auto [found, type] = callNear(sheep, 30.0f);
	EXPECT_EQ(type, lhvm::DataType::Object);
	EXPECT_EQ(found.uintVal, static_cast<uint32_t>(nearer));
	ASSERT_EQ(_things->registered.size(), 1u);
	EXPECT_EQ(_things->registered.back(), std::make_pair(nearer, false));
	// any sub-type: the cow, the nearest of all
	EXPECT_EQ(callNear(ecs::script_type::k_AnySubtype, 30.0f).first.uintVal, static_cast<uint32_t>(cow));
	// the cow's own sub-type finds it; a radius short of every sheep, or a species that is not there, finds none
	EXPECT_EQ(callNear(static_cast<uint32_t>(AnimalInfo::Cow), 30.0f).first.uintVal, static_cast<uint32_t>(cow));
	EXPECT_EQ(callNear(sheep, 3.0f).first.uintVal, 0u);
	EXPECT_EQ(callNear(static_cast<uint32_t>(AnimalInfo::Lion), 30.0f).first.uintVal, 0u);
	EXPECT_EQ(_things->registered.size(), 3u);
	ecs::map_cells::Clear();
}

TEST_F(ScriptQueryNatives, CallNearPassesOverAThingInAScriptOnlyWhenExcludingScripted)
{
	// With excludingScripted a thing a script holds is rejected, and the search takes the next nearest
	// (docs/wiki/scripts/script-commands.md#finding-things)
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
	ecs::map_cells::Clear();
	ecs::object_index::OnLoadMap();
	const auto callNear = [this](bool excludingScripted) {
		return Call("CALL_NEAR",
		            [excludingScripted](lhvm::LHVM& vm) {
			            vm.Pushi(static_cast<int32_t>(script::ObjectType::Animal));
			            vm.Pushi(static_cast<int32_t>(AnimalInfo::Sheep));
			            vm.Pushv(100.0f);
			            vm.Pushv(0.0f);
			            vm.Pushv(100.0f);
			            vm.Pushf(30.0f);
			            vm.Pushb(excludingScripted);
		            })
		    .uintVal;
	};
	const auto nearer = AnimalInCells(AnimalInfo::Sheep, glm::vec3(105.0f, 0.0f, 100.0f));
	const auto farther = AnimalInCells(AnimalInfo::Sheep, glm::vec3(115.0f, 0.0f, 100.0f));
	EXPECT_EQ(callNear(true), static_cast<uint32_t>(nearer));
	HeldByAScript(nearer);
	EXPECT_EQ(callNear(true), static_cast<uint32_t>(farther));
	EXPECT_EQ(callNear(false), static_cast<uint32_t>(nearer));
	// every thing in a script: none
	HeldByAScript(farther);
	EXPECT_EQ(callNear(true), 0u);
	EXPECT_EQ(_things->registered.size(), 3u);
	ecs::map_cells::Clear();
}

TEST_F(ScriptQueryNatives, CallNearOfATypeOutsideTheTableOrWithNoFindFunctionGivesZero)
{
	// A type outside 1..41 is refused, and the marker, the dance, the flock, the influence ring and the weather thing
	// have no find function: 0 for each (docs/wiki/scripts/script-commands.md#finding-things)
	const auto callNear = [this](int32_t type) {
		return Call("CALL_NEAR",
		            [type](lhvm::LHVM& vm) {
			            vm.Pushi(type);
			            vm.Pushi(static_cast<int32_t>(ecs::script_type::k_AnySubtype));
			            vm.Pushv(0.0f);
			            vm.Pushv(0.0f);
			            vm.Pushv(0.0f);
			            vm.Pushf(30.0f);
			            vm.Pushb(false);
		            })
		    .uintVal;
	};
	for (const int32_t type : {0, 42, -1})
	{
		EXPECT_EQ(callNear(type), 0u) << type;
	}
	for (const auto type : {script::ObjectType::Marker, script::ObjectType::Dance, script::ObjectType::Flock,
	                        script::ObjectType::InfluenceRing, script::ObjectType::WeatherThing})
	{
		EXPECT_EQ(callNear(static_cast<int32_t>(type)), 0u) << static_cast<int>(type);
	}
	EXPECT_TRUE(_things->registered.empty());
}

namespace
{
/// CALL_IN as a script calls it: the type is popped last, so it is pushed first; then the sub-type, the container and
/// excludingScripted
std::function<void(lhvm::LHVM&)> CallInArguments(script::ObjectType type, uint32_t subtype, uint32_t container,
                                                 bool excludingScripted)
{
	return [=](lhvm::LHVM& vm) {
		vm.Pushi(static_cast<int32_t>(type));
		vm.Pushi(static_cast<int32_t>(subtype));
		vm.Pusho(container);
		vm.Pushb(excludingScripted);
	};
}
} // namespace

TEST_F(ScriptQueryNatives, CallInFindsTheFirstOfTheKindInAFlockFromItsHeadAsAFoundThing)
{
	// A flock is looked in from its head; the first member of the type and sub-type (not in a script when excluding)
	// is added to the script as a found thing (docs/wiki/scripts/script-commands.md#finding-things,
	// docs/wiki/beings/villager-speed-and-scripts.md#script-flocks-and-containers)
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
	const auto animal = [](AnimalInfo species) {
		const auto e = Reg().Create();
		Reg().Assign<Animal>(e, species, 0u);
		return e;
	};
	const auto head = animal(AnimalInfo::Sheep);
	const auto middle = animal(AnimalInfo::Cow);
	const auto tail = animal(AnimalInfo::Sheep);
	const auto flock = Reg().Create();
	Reg().Assign<Flock>(flock).members = {tail, middle, head}; // from the head: head, middle, tail
	const auto callIn = [this, flock](uint32_t subtype, bool excluding) {
		const auto arguments = CallInArguments(script::ObjectType::Animal, subtype, static_cast<uint32_t>(flock), excluding);
		return Call("CALL_IN", arguments).uintVal;
	};
	EXPECT_EQ(callIn(ecs::script_type::k_AnySubtype, false), static_cast<uint32_t>(head));
	EXPECT_EQ(callIn(static_cast<uint32_t>(AnimalInfo::Cow), false), static_cast<uint32_t>(middle));
	EXPECT_EQ(callIn(static_cast<uint32_t>(AnimalInfo::Lion), false), 0u);
	// the head in a script: passed over only when excluding
	HeldByAScript(head);
	EXPECT_EQ(callIn(static_cast<uint32_t>(AnimalInfo::Sheep), true), static_cast<uint32_t>(tail));
	EXPECT_EQ(callIn(static_cast<uint32_t>(AnimalInfo::Sheep), false), static_cast<uint32_t>(head));
	EXPECT_EQ(_things->registered,
	          (std::vector<std::pair<entt::entity, bool>> {{head, false}, {middle, false}, {tail, false}, {head, false}}));
}

TEST_F(ScriptQueryNatives, CallInFindsATownsVillagersInItsNewestAbodeFirstThenTheHomeless)
{
	// A town is looked in through its abodes, newest first, each one's inhabitants, then its homeless; type 4 finds a
	// grown-up, 5 a child (docs/wiki/beings/villager-speed-and-scripts.md#script-flocks-and-containers)
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
	constexpr uint32_t k_Key = 30;
	const auto town = MakeTown(k_Key, PlayerNames::PLAYER_ONE, 1, 0);
	const auto abode = [](uint32_t made) {
		const auto e = MakeAbode(k_Key);
		Reg().Assign<ObjectCreationIndex>(e, made);
		return e;
	};
	const auto older = abode(1);
	const auto newer = abode(2);
	const auto villager = [](bool child) {
		const auto e = Reg().Create();
		Reg().Assign<Villager>(e).flags = child ? Villager::k_FlagChild : uint16_t {0};
		return e;
	};
	const auto inOlder = villager(false);
	const auto inNewer = villager(false);
	const auto child = villager(true);
	Reg().Get<Abode>(older).inhabitants = {inOlder};
	Reg().Get<Abode>(newer).inhabitants = {inNewer};
	Reg().Get<Town>(town).homelessVillagers = {child};
	const auto callIn = [this, town](script::ObjectType type, bool excluding) {
		const auto arguments = CallInArguments(type, ecs::script_type::k_AnySubtype, static_cast<uint32_t>(town), excluding);
		return Call("CALL_IN", arguments).uintVal;
	};
	EXPECT_EQ(callIn(script::ObjectType::Villager, false), static_cast<uint32_t>(inNewer));
	EXPECT_EQ(callIn(script::ObjectType::VillagerChild, false), static_cast<uint32_t>(child));
	HeldByAScript(inNewer);
	EXPECT_EQ(callIn(script::ObjectType::Villager, true), static_cast<uint32_t>(inOlder));
	EXPECT_EQ(_things->registered,
	          (std::vector<std::pair<entt::entity, bool>> {{inNewer, false}, {child, false}, {inOlder, false}}));
}

TEST_F(ScriptQueryNatives, CallInOfNoContainerOrOfAThingThatIsNoContainerGivesZero)
{
	// No container, or a thing that is not one: a warning and 0 (docs/wiki/scripts/script-commands.md#finding-things)
	const auto sheep = Reg().Create();
	Reg().Assign<Animal>(sheep, AnimalInfo::Sheep, 0u);
	for (const uint32_t container : {0u, static_cast<uint32_t>(sheep), 12345u})
	{
		const auto arguments = CallInArguments(script::ObjectType::Animal, ecs::script_type::k_AnySubtype, container, false);
		EXPECT_EQ(Call("CALL_IN", arguments).uintVal, 0u) << container;
	}
	EXPECT_TRUE(_things->registered.empty());
}

TEST_F(ScriptQueryNatives, ImmersionExistsAnswersNoAsABool)
{
	// Whether the force-feedback mouse was made, as a bool: without such a mouse, which openblack never has, no
	// (docs/wiki/hand/states.md, "The force-feedback mouse")
	const auto [exists, type] = CallTyped("IMMERSION_EXISTS", [](lhvm::LHVM&) {});
	EXPECT_EQ(type, lhvm::DataType::Boolean);
	EXPECT_EQ(exists.intVal, 0);
}

TEST_F(ScriptQueryNatives, GetActionTextForObjectGivesHelpText828AndTakesNothing)
{
	// The game's function pops nothing and pushes 828: what the script pushed before stays under it, as the original
	// (docs/wiki/project/internals.md, the native argument counts)
	const auto [text, type] = CallTyped("GET_ACTION_TEXT_FOR_OBJECT", [](lhvm::LHVM&) {});
	EXPECT_EQ(text.intVal, 828);
	EXPECT_EQ(type, lhvm::DataType::Int);
}

namespace
{
/// The opening's specials, as far as IS_PLAYING_JC_SPECIAL asks them: whether the lifting hand's clip has come round
class LiftAnswer final: public ecs::systems::IntroSystemInterface
{
public:
	void Play(int32_t /*special*/) override {}
	void ReleaseAll() override {}
	[[nodiscard]] bool HasLiftFinished() const override { return finished; }
	void Update(uint32_t /*milliseconds*/) override {}
	void FillFrame(graphics::IntroLightOverlay& /*out*/) const override {}

	bool finished {false};
};
} // namespace

TEST_F(ScriptQueryNatives, IsPlayingJcSpecialIsTrueButSpecial13IsWhetherTheLiftHasFinished)
{
	// The value is popped as a float and truncated; it answers 1 except for 13, which answers whether the intro hand's
	// clip has finished, as the original (docs/wiki/world/sea-life-and-scenery.md, the CHL specials)
	const test::RestoreService<Locator::introSystem> restoreIntro;
	auto& intro = static_cast<LiftAnswer&>(Locator::introSystem::emplace<LiftAnswer>());
	const auto playing = [this](float special) {
		return Call("IS_PLAYING_JC_SPECIAL", [special](lhvm::LHVM& vm) { vm.Pushf(special); }).intVal;
	};
	for (const float special : {0.0f, 6.0f, 12.0f, 14.0f, 15.0f, 12.9f})
	{
		EXPECT_EQ(playing(special), 1) << special;
	}
	EXPECT_EQ(playing(13.0f), 0);
	EXPECT_EQ(playing(13.7f), 0);
	intro.finished = true;
	EXPECT_EQ(playing(13.0f), 1);
}

TEST_F(ScriptQueryNatives, GetTotemStatueOfATownCentreIsItsTotemAndOfAnythingElseZero)
{
	// A town centre with a totem gives the totem as an object; anything else, the town itself too, gives 0 with no
	// message (docs/wiki/magic/village-totem.md, GET_TOTEM_STATUE)
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	info->abode.at(static_cast<size_t>(AbodeInfo::CelticTownCentre)).abodeType = AbodeType::TownCentre;
	info->abode.at(static_cast<size_t>(AbodeInfo::CelticWorkshop)).abodeType = AbodeType::Workshop;
	Locator::infoConstants::reset(info.release());
	const test::RestoreService<Locator::villageTotemSystem> restoreTotems;
	Locator::villageTotemSystem::emplace<ecs::systems::VillageTotemSystem>();
	constexpr uint32_t k_Key = 40;
	const auto town = MakeTown(k_Key, PlayerNames::PLAYER_ONE, 1, 0);
	const auto building = [](AbodeInfo row) {
		const auto e = MakeAbode(k_Key);
		Reg().Get<Abode>(e).info = row;
		return e;
	};
	const auto centre = building(AbodeInfo::CelticTownCentre);
	const auto workshop = building(AbodeInfo::CelticWorkshop);
	const auto bareCentre = building(AbodeInfo::CelticTownCentre);
	const auto totem = Reg().Create();
	Reg().Assign<VillageTotem>(totem).townCentre = centre;
	Reg().Assign<VillageTotem>(Reg().Create()).townCentre = workshop;
	const auto totemOf = [this](uint32_t object) {
		return CallTyped("GET_TOTEM_STATUE", [object](lhvm::LHVM& vm) { vm.Pusho(object); });
	};
	const auto [found, type] = totemOf(static_cast<uint32_t>(centre));
	EXPECT_EQ(type, lhvm::DataType::Object);
	EXPECT_EQ(found.uintVal, static_cast<uint32_t>(totem));
	for (const auto other : {town, workshop, bareCentre, totem, entt::entity {0}})
	{
		EXPECT_EQ(totemOf(static_cast<uint32_t>(other)).first.uintVal, 0u) << static_cast<uint32_t>(other);
	}
}

TEST_F(ScriptQueryNatives, GetTownWorshipDeathsIsTheTownsDeathsFromWorshipOnly)
{
	// The town's deaths of the chant reason, the count the villagers' deaths keep (docs/wiki/magic/worship.md#script)
	const auto town = MakeTown(50, PlayerNames::PLAYER_ONE, 1, 0);
	Reg().Assign<TownMagic>(town);
	const auto deaths = [this](entt::entity thing) {
		return CallTyped("GET_TOWN_WORSHIP_DEATHS", [thing](lhvm::LHVM& vm) { vm.Pusho(static_cast<uint32_t>(thing)); });
	};
	// before the town's first death
	EXPECT_EQ(deaths(town).first.floatVal, 0.0f);
	auto& counts = Reg().Assign<TownDeaths>(town);
	counts.byReason.at(static_cast<size_t>(DeathReason::Chant)) = 3;
	counts.byReason.at(static_cast<size_t>(DeathReason::Starving)) = 5;
	counts.count = 8;
	const auto [given, type] = deaths(town);
	EXPECT_EQ(type, lhvm::DataType::Float);
	EXPECT_EQ(given.floatVal, 3.0f);
	// not a town, or no thing
	EXPECT_EQ(deaths(MakeAbode(50)).first.floatVal, 0.0f);
	EXPECT_EQ(deaths(entt::entity {0}).first.floatVal, 0.0f);
}

namespace
{
/// The computer players' system, as far as CALL_COMPUTER_PLAYER asks for it: only that the land has one
class ComputerMinds final: public ecs::systems::ComputerPlayerSystemInterface
{
public:
	void SwitchOn(PlayerNames /*player*/, bool /*on*/) override {}
	void SetCreatureLike(PlayerNames /*player*/, PlayerNames /*like*/) override {}
	void ProcessTurn() override {}
};
} // namespace

TEST_F(ScriptQueryNatives, CallComputerPlayerGivesThePlayersEntityAsAFoundThing)
{
	// The script's player number n is the game's player n - 1; its entity is added to the script as a found thing and
	// pushed as an object, and 0 is pushed when there is none
	const test::RestoreService<Locator::computerPlayerSystem> restoreMinds;
	const auto callPlayer = [this](float player) {
		return CallTyped("CALL_COMPUTER_PLAYER", [player](lhvm::LHVM& vm) { vm.Pushf(player); });
	};
	const auto god = Reg().Create();
	Reg().Assign<Player>(god).name = PlayerNames::PLAYER_TWO;
	// a land without computer players
	Locator::computerPlayerSystem::reset();
	EXPECT_EQ(callPlayer(2.0f).first.uintVal, 0u);
	Locator::computerPlayerSystem::emplace<ComputerMinds>();
	const auto [found, type] = callPlayer(2.0f);
	EXPECT_EQ(type, lhvm::DataType::Object);
	EXPECT_EQ(found.uintVal, static_cast<uint32_t>(god));
	EXPECT_EQ(_things->registered, (std::vector<std::pair<entt::entity, bool>> {{god, false}}));
	// player one has no entity; 0 is the neutral player, which has none either; 9 is no player
	for (const float player : {1.0f, 0.0f, 9.0f})
	{
		EXPECT_EQ(callPlayer(player).first.uintVal, 0u) << player;
	}
	EXPECT_EQ(_things->registered.size(), 1u);
}

namespace
{
/// The computer players' system, as the natives that switch a mind, put its hand, let it go and clear its actions reach
/// it: a record of what each asked for
class ScriptedComputerPlayers final: public ecs::systems::ComputerPlayerSystemInterface
{
public:
	struct PutHandCall
	{
		PlayerNames player;
		glm::vec3 point;
		bool overGround;
	};

	void SwitchOn(PlayerNames player, bool on) override { switched.emplace_back(player, on); }
	void SetCreatureLike(PlayerNames /*player*/, PlayerNames /*like*/) override {}
	void ProcessTurn() override {}
	void PutHand(PlayerNames player, const glm::vec3& point, bool overGround) override
	{
		put.push_back({.player = player, .point = point, .overGround = overGround});
	}
	void Release(PlayerNames player) override { released.push_back(player); }
	void ClearActions(PlayerNames player) override { cleared.push_back(player); }

	std::vector<std::pair<PlayerNames, bool>> switched;
	std::vector<PutHandCall> put;
	std::vector<PlayerNames> released;
	std::vector<PlayerNames> cleared;
};
} // namespace

TEST_F(ScriptQueryNatives, EnableDisableComputerPlayerPopsThePlayerThenTheSwitch)
{
	// As the original: the player is popped first, then the switch; any value but 0 switches the player's mind on and 0
	// off, and a number that names no game player switches nothing
	// (docs/wiki/creature/players-creature.md#a-computer-players-start-on-the-land). The first native of the name is
	// this one; the second, of the same name, pauses the mind
	const test::RestoreService<Locator::computerPlayerSystem> restoreMinds;
	auto& minds = static_cast<ScriptedComputerPlayers&>(Locator::computerPlayerSystem::emplace<ScriptedComputerPlayers>());
	const auto enable = [this](float player, int32_t on) {
		CallLeavingNothing("ENABLE_DISABLE_COMPUTER_PLAYER", [player, on](lhvm::LHVM& vm) {
			vm.Pushi(on);
			vm.Pushf(player);
		});
	};
	enable(2.0f, 1);
	enable(3.0f, 0);
	enable(4.0f, 7);
	enable(9.0f, 1);
	EXPECT_EQ(minds.switched,
	          (std::vector<std::pair<PlayerNames, bool>> {
	              {PlayerNames::PLAYER_TWO, true}, {PlayerNames::PLAYER_THREE, false}, {PlayerNames::PLAYER_FOUR, true}}));
}

TEST_F(ScriptQueryNatives, SetComputerPlayerPositionPopsTheFlagThePointAndThePlayer)
{
	// As the original: the flag is popped first, then the point and the player; a flag that is not 0 puts the hand above
	// the ground at the point, 0 at the point's own height, and a number that names no game player puts nothing
	// (docs/wiki/hand/computer-hands.md#the-scripts-natives)
	const test::RestoreService<Locator::computerPlayerSystem> restoreMinds;
	auto& minds = static_cast<ScriptedComputerPlayers&>(Locator::computerPlayerSystem::emplace<ScriptedComputerPlayers>());
	const auto setPosition = [this](float player, glm::vec3 point, bool overGround) {
		CallLeavingNothing("SET_COMPUTER_PLAYER_POSITION", [player, point, overGround](lhvm::LHVM& vm) {
			vm.Pushf(player);
			vm.Pushv(point.x);
			vm.Pushv(point.y);
			vm.Pushv(point.z);
			vm.Pushb(overGround);
		});
	};
	setPosition(2.0f, glm::vec3(1200.0f, 30.0f, 1500.0f), true);
	setPosition(3.0f, glm::vec3(100.0f, 50.0f, 200.0f), false);
	setPosition(9.0f, glm::vec3(1.0f), true);
	ASSERT_EQ(minds.put.size(), 2u);
	EXPECT_EQ(minds.put[0].player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(minds.put[0].point, glm::vec3(1200.0f, 30.0f, 1500.0f));
	EXPECT_TRUE(minds.put[0].overGround);
	EXPECT_EQ(minds.put[1].player, PlayerNames::PLAYER_THREE);
	EXPECT_EQ(minds.put[1].point, glm::vec3(100.0f, 50.0f, 200.0f));
	EXPECT_FALSE(minds.put[1].overGround);
}

TEST_F(ScriptQueryNatives, ReleaseAndClearComputerPlayerActionsAskForTheGamesPlayer)
{
	// As the original: each pops only the player, script player n being game player n - 1, and a number that names no
	// game player does nothing (docs/wiki/hand/computer-hands.md#the-scripts-natives)
	const test::RestoreService<Locator::computerPlayerSystem> restoreMinds;
	auto& minds = static_cast<ScriptedComputerPlayers&>(Locator::computerPlayerSystem::emplace<ScriptedComputerPlayers>());
	for (const float player : {2.0f, 9.0f})
	{
		CallLeavingNothing("RELEASE_COMPUTER_PLAYER", [player](lhvm::LHVM& vm) { vm.Pushf(player); });
		CallLeavingNothing("GAME_CLEAR_COMPUTER_PLAYER_ACTIONS", [player](lhvm::LHVM& vm) { vm.Pushf(player); });
	}
	EXPECT_EQ(minds.released, (std::vector<PlayerNames> {PlayerNames::PLAYER_TWO}));
	EXPECT_EQ(minds.cleared, (std::vector<PlayerNames> {PlayerNames::PLAYER_TWO}));
	EXPECT_TRUE(minds.switched.empty());
	EXPECT_TRUE(minds.put.empty());
}

namespace
{
/// The computer players' own system, made by the test, over the fixture's registry: what the natives ask of it is read
/// back from the components of the player's entity
class ComputerPlayerScripts: public ScriptQueryNatives
{
protected:
	/// Player two's entity, its hand holding a thing (gone from the land), part of the way through a step to a point,
	/// its steps going faster than the player's own speed, held and busy as a script leaves it
	entt::entity GodWithABusyHand()
	{
		const auto god = Reg().Create();
		Reg().Assign<Player>(god).name = PlayerNames::PLAYER_TWO;
		auto& hand = Reg().Assign<ComputerHand>(god);
		hand.playerSpeed = k_PlayersSpeed;
		hand.speed = 40.0f;
		hand.steps.push_back(computer_hand::StepToPoint(HandSubAction::MoveHandToPos, glm::vec3(1.0f, 2.0f, 3.0f)));
		hand.stage = 1;
		hand.checkPrerequisite = false;
		hand.prerequisiteInserted = true;
		hand.held = true;
		hand.busy = true;
		// a thing that is no longer on the land: letting go of it only empties the hand
		hand.heldObject = static_cast<entt::entity>(5000);
		return god;
	}

	static constexpr float k_PlayersSpeed = 150.0f;
	ecs::systems::ComputerPlayerSystem _players;
};
} // namespace

TEST_F(ComputerPlayerScripts, SwitchingTheMindOnOrOffLeavesThePlayersType)
{
	// As the original: native 311 writes only the mind's switch; the player's type stays as it is
	// (docs/wiki/creature/players-creature.md#a-computer-players-start-on-the-land)
	const auto god = Reg().Create();
	Reg().Assign<Player>(god).name = PlayerNames::PLAYER_TWO;
	Reg().Assign<PlayerMagic>(god).playerType = 0;
	_players.SwitchOn(PlayerNames::PLAYER_TWO, true);
	EXPECT_TRUE(Reg().Get<ComputerPlayer>(god).switchedOn);
	EXPECT_EQ(Reg().Get<PlayerMagic>(god).playerType, 0);
	_players.SwitchOn(PlayerNames::PLAYER_TWO, false);
	EXPECT_FALSE(Reg().Get<ComputerPlayer>(god).switchedOn);
	EXPECT_EQ(Reg().Get<PlayerMagic>(god).playerType, 0);
	EXPECT_FALSE(Reg().Get<ComputerPlayer>(god).paused);
}

TEST_F(ComputerPlayerScripts, PuttingTheHandGivesUpItsStepsForOneSetStepAtThePlayersSpeed)
{
	// As the original: the hand gives up its steps and lets go of what it holds, its steps take the player's own speed,
	// and its one step puts it at the point, above the ground or at the point's height; a script holds it and it is busy
	// (docs/wiki/hand/computer-hands.md#the-scripts-natives)
	const auto god = GodWithABusyHand();
	_players.PutHand(PlayerNames::PLAYER_TWO, glm::vec3(1200.0f, 30.0f, 1500.0f), true);
	const auto& hand = Reg().Get<const ComputerHand>(god);
	ASSERT_EQ(hand.steps.size(), 1u);
	EXPECT_EQ(hand.steps.front().type, HandSubAction::SetHandToPos);
	ASSERT_EQ(hand.steps.front().arguments.size(), 1u);
	EXPECT_EQ(hand.steps.front().arguments.front().kind, HandArgument::Kind::Point);
	EXPECT_EQ(hand.steps.front().arguments.front().point, glm::vec3(1200.0f, 30.0f, 1500.0f));
	EXPECT_EQ(hand.stage, 0);
	EXPECT_TRUE(hand.checkPrerequisite);
	EXPECT_FALSE(hand.prerequisiteInserted);
	EXPECT_EQ(hand.speed, k_PlayersSpeed);
	EXPECT_EQ(hand.playerSpeed, k_PlayersSpeed);
	EXPECT_TRUE(hand.heldObject == entt::null);
	EXPECT_TRUE(hand.held);
	EXPECT_TRUE(hand.busy);
	// at the point's own height
	_players.PutHand(PlayerNames::PLAYER_TWO, glm::vec3(100.0f, 50.0f, 200.0f), false);
	ASSERT_EQ(hand.steps.size(), 1u);
	EXPECT_EQ(hand.steps.front().type, HandSubAction::SetHandToPosDirectY);
	EXPECT_EQ(hand.steps.front().arguments.front().point, glm::vec3(100.0f, 50.0f, 200.0f));
}

TEST_F(ComputerPlayerScripts, ReleasingTheHandUnpausesTheMindAndGivesTheStepsThePlayersSpeed)
{
	// As the original: the release is the pause let go: the mind is not paused, no script holds the hand and its steps
	// take the player's own speed; the steps themselves and the busy flag stay as they were
	// (docs/wiki/hand/computer-hands.md#the-scripts-natives)
	const auto god = GodWithABusyHand();
	auto& mind = Reg().Assign<ComputerPlayer>(god);
	mind.switchedOn = true;
	mind.paused = true;
	_players.Release(PlayerNames::PLAYER_TWO);
	EXPECT_FALSE(Reg().Get<const ComputerPlayer>(god).paused);
	EXPECT_TRUE(Reg().Get<const ComputerPlayer>(god).switchedOn);
	const auto& hand = Reg().Get<const ComputerHand>(god);
	EXPECT_FALSE(hand.held);
	EXPECT_EQ(hand.speed, k_PlayersSpeed);
	EXPECT_EQ(hand.playerSpeed, k_PlayersSpeed);
	EXPECT_TRUE(hand.busy);
	ASSERT_EQ(hand.steps.size(), 1u);
	EXPECT_EQ(hand.steps.front().type, HandSubAction::MoveHandToPos);
	EXPECT_EQ(hand.stage, 1);
}

TEST_F(ComputerPlayerScripts, ClearingTheActionsGivesUpTheStepsAndTheQueueButNotTheScriptsHold)
{
	// As the original: the hand gives up its steps and lets go of what it holds, and every queued action is dropped; the
	// script's hold, the busy flag and the speeds stay as they were (docs/wiki/hand/computer-hands.md#the-scripts-natives)
	const auto god = GodWithABusyHand();
	auto& queued = Reg().Get<ComputerHand>(god);
	computer_hand::QueueAction(queued, "ShallCastWoodSpell", entt::null, entt::null);
	computer_hand::QueueAction(queued, "DIRECTPickupAndDropObject", entt::null, entt::null);
	_players.ClearActions(PlayerNames::PLAYER_TWO);
	const auto& hand = Reg().Get<const ComputerHand>(god);
	EXPECT_TRUE(hand.steps.empty());
	EXPECT_TRUE(hand.queue.empty());
	EXPECT_EQ(hand.stage, 0);
	EXPECT_TRUE(hand.checkPrerequisite);
	EXPECT_FALSE(hand.prerequisiteInserted);
	EXPECT_TRUE(hand.heldObject == entt::null);
	EXPECT_TRUE(hand.held);
	EXPECT_TRUE(hand.busy);
	EXPECT_EQ(hand.speed, 40.0f);
	EXPECT_EQ(hand.playerSpeed, k_PlayersSpeed);
}

TEST_F(ScriptQueryNatives, SetIdMoveableAndPickupableSetTheFlagsTheHandReads)
{
	// The object, then the bool: a false one sets the object's immovable or cannot-be-picked-up flag, which the hand's
	// checks read, and a true one clears it (docs/wiki/hand/grab-hold-and-throw.md, the pick-up flags)
	const auto set = [this](std::string_view native, entt::entity thing, bool on) {
		// the object is popped first, so it is pushed last
		CallLeavingNothing(native, [thing, on](lhvm::LHVM& vm) {
			vm.Pushb(on);
			vm.Pusho(static_cast<uint32_t>(thing));
		});
	};
	const auto rock = Reg().Create();
	Reg().Assign<MobileStatic>(rock, MobileStaticInfo::Rock);
	set("SET_ID_MOVEABLE", rock, false);
	EXPECT_TRUE(ecs::object_flags::IsImmovable(rock));
	EXPECT_FALSE(ecs::object_flags::IsCannotBePickedUp(rock));
	set("SET_ID_PICKUPABLE", rock, false);
	EXPECT_TRUE(ecs::object_flags::IsCannotBePickedUp(rock));
	set("SET_ID_MOVEABLE", rock, true);
	EXPECT_FALSE(ecs::object_flags::IsImmovable(rock));
	EXPECT_TRUE(ecs::object_flags::IsCannotBePickedUp(rock));
	set("SET_ID_PICKUPABLE", rock, true);
	EXPECT_FALSE(ecs::object_flags::IsCannotBePickedUp(rock));
	// no thing: nothing is set
	set("SET_ID_MOVEABLE", entt::entity {0}, false);
	set("SET_ID_PICKUPABLE", entt::entity {0}, false);
	EXPECT_FALSE(ecs::object_flags::IsImmovable(entt::entity {0}));
	EXPECT_FALSE(ecs::object_flags::IsCannotBePickedUp(entt::entity {0}));
}

TEST_F(ScriptQueryNatives, SquareRootIsTheFloatRootOfAnyValue)
{
	// The value's root, rounded once to a float, given as a float; the root of nought is nought. The sign is not
	// tested: a negative value (minus infinity too) gives the processor's default not-a-number and minus nought gives
	// minus nought, as the original
	const auto root = [this](float value) { return CallTyped("SQUARE_ROOT", [value](lhvm::LHVM& vm) { vm.Pushf(value); }); };
	const auto bits = [&root](float value) { return std::bit_cast<uint32_t>(root(value).first.floatVal); };
	const auto [four, type] = root(16.0f);
	EXPECT_EQ(type, lhvm::DataType::Float);
	EXPECT_EQ(four.floatVal, 4.0f);
	EXPECT_EQ(root(2.0f).first.floatVal, 1.41421354f);
	EXPECT_EQ(bits(0.0f), 0x00000000U);
	EXPECT_EQ(bits(-0.0f), 0x80000000U);
	EXPECT_EQ(bits(-4.0f), 0xFFC00000U);
	EXPECT_EQ(bits(-0.5f), 0xFFC00000U);
	EXPECT_EQ(bits(-std::numeric_limits<float>::infinity()), 0xFFC00000U);
	EXPECT_EQ(root(-4.0f).second, lhvm::DataType::Float);
}

namespace
{
/// W the identity: (X, Y, Z) = (x, y, z), Z the depth; the screen 640 x 480, near 1, tan(fov / 2) = 1
field_of_view::View ScreenTestView()
{
	field_of_view::View view;
	view.nearW = 1.0f;
	view.screen = {640, 480};
	view.eye = glm::vec3(0.0f);
	view.tanHalfFov = 1.0f;
	return view;
}
} // namespace

TEST_F(ScriptQueryNatives, FieldOfViewTakesAThingAsAnObjectByWhatItIsNotByItsMesh)
{
	// An object is tested by its mesh's sphere: one openblack keeps no mesh for (a fish farm) is never on the screen,
	// even standing in the middle of it. A thing that is not an object (a mist, a script's marker) is tested at its
	// point, mesh or none
	const auto view = ScreenTestView();
	const glm::vec3 seen {0.0f, 0.0f, 10.0f};
	const auto place = [](entt::entity thing, glm::vec3 at) {
		Reg().Assign<Transform>(thing, at, glm::mat3(1.0f), glm::vec3(1.0f));
		return thing;
	};
	const auto farm = place(Reg().Create(), seen);
	Reg().Assign<FishFarm>(farm);
	EXPECT_TRUE(field_of_view::IsObjectForTheScreenTest(farm));
	EXPECT_FALSE(field_of_view::ThingInView(view, false, farm));
	const auto mist = place(Reg().Create(), seen);
	Reg().Assign<Mist>(mist);
	EXPECT_FALSE(field_of_view::IsObjectForTheScreenTest(mist));
	EXPECT_TRUE(field_of_view::ThingInView(view, false, mist));
	const auto marker = place(Reg().Create(), seen);
	Reg().AssignState<ScriptMarker>(marker);
	EXPECT_FALSE(field_of_view::IsObjectForTheScreenTest(marker));
	EXPECT_TRUE(field_of_view::ThingInView(view, false, marker));
	// Off the screen, and inside the citadel, the point is not seen
	const auto aside = place(Reg().Create(), {20.0f, 0.0f, 10.0f});
	Reg().Assign<Mist>(aside);
	EXPECT_FALSE(field_of_view::ThingInView(view, false, aside));
	EXPECT_FALSE(field_of_view::ThingInView(view, true, mist));
}

TEST_F(ScriptQueryNatives, FieldOfViewTestsAThingThatIsNoObjectAtTheGamesPointOfIt)
{
	// The game keeps a thing's place as map coordinates: x and z on the fixed point, truncated towards 0. A point just
	// left of the screen's edge comes back a little to the right, on the first column
	const auto view = ScreenTestView();
	const glm::vec3 kept {-10.03126f, 0.0f, 10.0f};
	EXPECT_FALSE(field_of_view::PosInView(view, false, kept));
	const auto point = field_of_view::MapPointOf(kept);
	EXPECT_EQ(point.x, -10.0311279296875f); // -65740 fixed units
	EXPECT_EQ(point.z, 10.0f);
	const auto mist = Reg().Create();
	Reg().Assign<Transform>(mist, kept, glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<Mist>(mist);
	EXPECT_TRUE(field_of_view::ThingInView(view, false, mist));
}

TEST_F(ScriptQueryNatives, FieldOfViewTestsATimerAtTheMapsCornerAndADanceAtItsPlace)
{
	// The game keeps a script's timer at (0, 0, 0): its point is the land at the map's corner. A dance is tested at
	// the place it is danced about. Without a land the ground is 0
	const test::RestoreService<Locator::terrainSystem> restoreLand;
	Locator::terrainSystem::reset();
	auto view = ScreenTestView();
	view.worldToClipping.m[11] = 10.0f; // the world's origin 10 ahead of the camera, in the middle of the screen
	const auto timer = Reg().Create();
	Reg().Assign<ScriptTimer>(timer);
	EXPECT_EQ(field_of_view::PointOfThing(timer), glm::vec3(0.0f));
	EXPECT_TRUE(field_of_view::ThingInView(view, false, timer));
	const auto dance = Reg().Create();
	Reg().Assign<Dance>(dance).place = map_coords::MapCoords {.x = map_coords::ToFixed(5.0f), .z = 0, .altitude = 2.0f};
	EXPECT_EQ(field_of_view::PointOfThing(dance), glm::vec3(5.0f, 2.0f, 0.0f));
	EXPECT_TRUE(field_of_view::ThingInView(view, false, dance));
	// A dance at the screen's right edge is off it
	Reg().Get<Dance>(dance).place.x = map_coords::ToFixed(10.0f);
	EXPECT_FALSE(field_of_view::ThingInView(view, false, dance));
	// A thing openblack keeps no place for has no point
	EXPECT_FALSE(field_of_view::PointOfThing(Reg().Create()).has_value());
}

TEST_F(ScriptQueryNatives, CanViewCameraTakesTheThingAndTheDegreesAndIsFalseForWhatIsNotAnObject)
{
	// The degrees are popped first, then the thing; no thing, or a script's marker (no object), gives a bool 0
	const auto marker = Reg().Create();
	Reg().AssignState<ScriptMarker>(marker);
	for (const auto thing : {uint32_t {0}, static_cast<uint32_t>(marker)})
	{
		const auto [seen, type] = CallTyped("GAME_THING_CAN_VIEW_CAMERA", [thing](lhvm::LHVM& vm) {
			vm.Pusho(thing);
			vm.Pushf(180.0f);
		});
		EXPECT_EQ(type, lhvm::DataType::Boolean) << thing;
		EXPECT_EQ(seen.intVal, 0) << thing;
	}
}

TEST_F(ScriptQueryNatives, InsideTempleIsWhetherTheGameIsInTheCitadelsSequence)
{
	// A bool, true only in the citadel's sequence: not in a new game's, nor while the falling spell's film plays
	const auto before = game_clock::SequenceMode();
	const auto inside = [this](int32_t mode) {
		game_clock::SetSequenceMode(mode);
		return CallTyped("INSIDE_TEMPLE", [](lhvm::LHVM& /*vm*/) {});
	};
	const auto [citadel, type] = inside(game_clock::k_SequenceModeCitadel);
	EXPECT_EQ(type, lhvm::DataType::Boolean);
	EXPECT_EQ(citadel.intVal, 1);
	EXPECT_EQ(inside(game_clock::k_SequenceModeNone).first.intVal, 0);
	EXPECT_EQ(inside(game_clock::k_SequenceModeFallingSpell).first.intVal, 0);
	game_clock::SetSequenceMode(before);
}

TEST_F(ScriptQueryNatives, GetSpellIconInTempleIsTheIconOfTheMiraclesSeedInAnyOfTheSites)
{
	// The temple is popped first and the miracle second, so the miracle is pushed first. The miracle's first seed is
	// looked for among each site's icons, the sites in their slots' order; the icon found takes its place in the
	// scripts' table as a thing found, not made, and is given as an object, and 0 (nothing in the table) when no site
	// has one, or when the thing is not a temple
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	info->spellSeed.at(static_cast<size_t>(SpellSeedType::Heal)).magicTypes = {MagicType::Heal, MagicType::HealPowerUpOne,
	                                                                           MagicType::None, MagicType::None};
	Locator::infoConstants::reset(info.release());
	const auto citadel = MakeCitadel(PlayerNames::PLAYER_ONE, glm::vec3(0.0f), true);
	const auto makeSite = [](std::vector<entt::entity> icons) {
		const auto site = Reg().Create();
		Reg().Assign<WorshipSite>(site).icons = std::move(icons);
		return site;
	};
	const auto makeIcon = [](SpellSeedType seed) {
		const auto icon = Reg().Create();
		Reg().Assign<SpellIcon>(icon).seedType = seed;
		return icon;
	};
	const auto heal = makeIcon(SpellSeedType::Heal);
	auto& sites = Reg().Get<CitadelWorship>(citadel).sites;
	sites.at(1) = makeSite({makeIcon(SpellSeedType::Fire)});
	sites.at(4) = makeSite({makeIcon(SpellSeedType::Food), heal});
	const auto iconOf = [this](MagicType magic, entt::entity temple) {
		return CallTyped("GET_SPELL_ICON_IN_TEMPLE", [magic, temple](lhvm::LHVM& vm) {
			vm.Pushi(static_cast<int32_t>(magic));
			vm.Pusho(static_cast<uint32_t>(temple));
		});
	};
	_things->registered.clear();
	const auto [found, type] = iconOf(MagicType::HealPowerUpOne, citadel);
	EXPECT_EQ(type, lhvm::DataType::Object);
	EXPECT_EQ(found.uintVal, static_cast<uint32_t>(heal));
	ASSERT_EQ(_things->registered.size(), 1u);
	EXPECT_EQ(_things->registered.front().first, heal);
	EXPECT_FALSE(_things->registered.front().second);
	EXPECT_EQ(iconOf(MagicType::Heal, citadel).first.uintVal, static_cast<uint32_t>(heal));
	EXPECT_EQ(_things->registered.size(), 2u);
	// a miracle no site has an icon of
	const auto [none, noneType] = iconOf(MagicType::Fireball, citadel);
	EXPECT_EQ(noneType, lhvm::DataType::Object);
	EXPECT_EQ(none.uintVal, 0u);
	// not a temple, or no thing
	EXPECT_EQ(iconOf(MagicType::Heal, sites.at(4)).first.uintVal, 0u);
	EXPECT_EQ(iconOf(MagicType::Heal, entt::entity {0}).first.uintVal, 0u);
	EXPECT_EQ(_things->registered.size(), 2u);
}

TEST_F(ScriptQueryNatives, GetSpellIconInTempleLooksUpAnyMiracleValue)
{
	// The miracle is not range checked: nought (no miracle) finds the first seed whose list holds no miracle in one of
	// its slots, so a site's icon of that seed is given; a value past the last miracle, or minus one, is listed by no
	// seed and gives 0
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	for (auto& seed : info->spellSeed)
	{
		seed.magicTypes = {MagicType::Heal, MagicType::Heal, MagicType::Heal, MagicType::Heal};
	}
	info->spellSeed.at(static_cast<size_t>(SpellSeedType::Food)).magicTypes = {MagicType::Food, MagicType::None,
	                                                                           MagicType::None, MagicType::None};
	info->spellSeed.at(static_cast<size_t>(SpellSeedType::Water)).magicTypes = {MagicType::Water, MagicType::None,
	                                                                            MagicType::None, MagicType::None};
	Locator::infoConstants::reset(info.release());
	const auto citadel = MakeCitadel(PlayerNames::PLAYER_ONE, glm::vec3(0.0f), true);
	const auto makeIcon = [](SpellSeedType seed) {
		const auto icon = Reg().Create();
		Reg().Assign<SpellIcon>(icon).seedType = seed;
		return icon;
	};
	const auto food = makeIcon(SpellSeedType::Food);
	const auto site = Reg().Create();
	Reg().Assign<WorshipSite>(site).icons = {makeIcon(SpellSeedType::Water), food};
	Reg().Get<CitadelWorship>(citadel).sites.at(0) = site;
	const auto iconOf = [this, citadel](uint32_t magic) {
		return CallTyped("GET_SPELL_ICON_IN_TEMPLE", [magic, citadel](lhvm::LHVM& vm) {
			vm.Pushi(static_cast<int32_t>(magic));
			vm.Pusho(static_cast<uint32_t>(citadel));
		});
	};
	// the food seed comes before the water seed, so nought finds the food seed's icon
	const auto [none, type] = iconOf(static_cast<uint32_t>(MagicType::None));
	EXPECT_EQ(type, lhvm::DataType::Object);
	EXPECT_EQ(none.uintVal, static_cast<uint32_t>(food));
	EXPECT_EQ(iconOf(42u).first.uintVal, 0u);
	EXPECT_EQ(iconOf(0xFFFFFFFFu).first.uintVal, 0u);
}

TEST_F(ScriptQueryNatives, DllGettimeIsTheTurnsTheScriptsHaveRunTimesATenth)
{
	// The scripts' own turn count times a tenth, as a float: two turns are a fifth of a second
	EXPECT_EQ(Call("DLL_GETTIME", [](lhvm::LHVM& /*vm*/) {}).floatVal, 0.0f);
	auto& vm = Locator::vm::value();
	vm.LookIn(lhvm::ScriptType::All);
	vm.LookIn(lhvm::ScriptType::All);
	const auto [time, type] = CallTyped("DLL_GETTIME", [](lhvm::LHVM& /*vm*/) {});
	EXPECT_EQ(type, lhvm::DataType::Float);
	EXPECT_EQ(time.floatVal, 0.2f);
}

TEST_F(ScriptQueryNatives, LoadMapAsksForTheLandOfThePathWhileTheCallRuns)
{
	// The map script's path is popped as a string of the program's; the land change is asked for at once, so the land
	// changes before the calling script's next instruction, and nothing is left for the script
	const test::RestoreService<Locator::events> restoreEvents;
	Locator::events::emplace<EventManager>();
	std::vector<std::string> asked;
	Locator::events::value().AddHandler<ecs::events::LandChangeRequested>(
	    [&asked](const ecs::events::LandChangeRequested& request) { asked.push_back(request.path.generic_string()); });
	const std::string path = "Scripts/Land2.txt";
	std::vector<char> data(path.begin(), path.end());
	data.push_back('\0');
	ASSERT_EQ(Locator::vm::value().LoadBinary(lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, {}, {}, {}, {}, data)),
	          EXIT_SUCCESS);
	CallLeavingNothing("LOAD_MAP", [](lhvm::LHVM& vm) { vm.Pushi(0); });
	EXPECT_EQ(asked, (std::vector<std::string> {path}));
}

namespace
{
/// A sky with no meshes or textures: only the hour it is drawn at is kept
class HourOnlySky final: public ecs::systems::SkySystemInterface
{
public:
	void Initialize() override {}
	void KeepForNextLand() override {}
	void ProcessTurn() override {}
	void UpdateFrame(bool /*skyDrawn*/) override {}
	void SetTime(float time) override { hour = time; }
	[[nodiscard]] float GetTime() const override { return hour; }
	[[nodiscard]] DayNightTimes GetDayNightTimes() const override { return _clock.GetDayNightTimes(); }
	[[nodiscard]] DayNightClock& GetClock() override { return _clock; }
	[[nodiscard]] const DayNightClock& GetClock() const override { return _clock; }
	[[nodiscard]] graphics::L3DMesh& GetMesh() const noexcept override { std::abort(); }
	[[nodiscard]] graphics::Texture2D& GetTexture() const noexcept override { std::abort(); }
	[[nodiscard]] graphics::L3DMesh& GetSunMesh() const noexcept override { std::abort(); }
	[[nodiscard]] graphics::L3DMesh& GetMoonMesh() const noexcept override { std::abort(); }
	[[nodiscard]] graphics::L3DMesh& GetCloudMesh() const noexcept override { std::abort(); }
	[[nodiscard]] entt::entity GetDome() const override { return entt::null; }
	[[nodiscard]] entt::entity GetSun() const override { return entt::null; }
	[[nodiscard]] entt::entity GetMoon() const override { return entt::null; }

	float hour {-1.0f};

private:
	DayNightClock _clock;
};

/// The game's day and night clock for one test, at the start of a land, with a sky frame of its own
class GameTimeNatives: public ScriptQueryNatives
{
protected:
	void SetUp() override
	{
		ScriptQueryNatives::SetUp();
		_clock = &Locator::dayNightClock::emplace<ecs::systems::DayNightClockSystem>().Clock();
		_clock->Reset();
		_sky = &static_cast<HourOnlySky&>(Locator::skySystem::emplace<HourOnlySky>());
	}

	/// SET_GAME_TIME_PROPERTIES as a script calls it: the duration is pushed first, the night's share second and the
	/// share of dawn and dusk last
	void SetProperties(float duration, float night, float change)
	{
		CallLeavingNothing("SET_GAME_TIME_PROPERTIES", [duration, night, change](lhvm::LHVM& vm) {
			vm.Pushf(duration);
			vm.Pushf(night);
			vm.Pushf(change);
		});
	}

	/// A clock whose cycle is set to these, to compare with
	static DayNightClock CycleOf(float duration, float night, float change)
	{
		DayNightClock clock;
		clock.SetCycle(duration, night, change);
		return clock;
	}

	test::SkyFrameGuard _skyFrame;
	test::RestoreService<Locator::dayNightClock> _restoreClock;
	test::RestoreService<Locator::skySystem> _restoreSky;
	DayNightClock* _clock {nullptr};
	HourOnlySky* _sky {nullptr};
};
} // namespace

TEST_F(GameTimeNatives, SetGameTimeForcesTheClockToTheHourAndTheSkyFollows)
{
	// The hour is a script time: the clock's visual time jumps to the visual time of that hour, and the sky is drawn at
	// the clock's script time. A move of the time going on stops there
	_clock->MoveScriptTime(0.0f, 10.0f);
	CallLeavingNothing("SET_GAME_TIME", [](lhvm::LHVM& vm) { vm.Pushf(7.5f); });
	EXPECT_EQ(_clock->GetVisualTime(), _clock->ScriptToVisual(7.5f));
	EXPECT_EQ(_sky->hour, _clock->GetScriptTime());
	_clock->SetRunning(false);
	const auto set = _clock->GetVisualTime();
	_clock->ProcessTurn();
	EXPECT_EQ(_clock->GetVisualTime(), set);
}

TEST_F(GameTimeNatives, SetGameTimePropertiesSetsTheCycleInTheOrderTheScriptGivesIt)
{
	SetProperties(1000.0f, 0.2f, 0.1f);
	const auto expected = CycleOf(1000.0f, 0.2f, 0.1f);
	EXPECT_EQ(_clock->GetDuration(), 1000.0f);
	EXPECT_EQ(_clock->GetVisualTimes(), expected.GetVisualTimes());
}

TEST_F(GameTimeNatives, ResetGameTimePropertiesPutsBackADayOf1700SecondsWithItsNightAndChangeShares)
{
	// 1700 seconds a day, 0.083 of it night and 0.07 dawn and dusk
	SetProperties(1000.0f, 0.2f, 0.1f);
	CallLeavingNothing("RESET_GAME_TIME_PROPERTIES", [](lhvm::LHVM& /*vm*/) {});
	const auto expected = CycleOf(1700.0f, 0.083f, 0.07f);
	EXPECT_EQ(_clock->GetDuration(), 1700.0f);
	EXPECT_EQ(_clock->GetVisualTimes(), expected.GetVisualTimes());
}
