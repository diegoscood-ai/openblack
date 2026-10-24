/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The natives a land's scripts use as its creature leaves through the vortex (raffclar's cases, PR #349): property 36
// lets a creature through a vortex that leads away, GAME_SUB_TYPE gives the row of a thing's kind, and
// SET_HEADING_AND_SPEED's velocity and tumble, as pure rules. Each native leaves exactly what the scripts expect.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <bit>
#include <functional>
#include <limits>
#include <string_view>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptPropertyRules.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::script::property_rules;

namespace
{
class ScriptVortexNatives: public ::testing::Test
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
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// The native as a script calls it: `push` pushes its arguments on top of a value of the script's own; the values
	/// it gives above the script's (none, or one)
	std::vector<lhvm::VMValue> Call(std::string_view name, const std::function<void(lhvm::LHVM&)>& push, int gives)
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
		std::vector<lhvm::VMValue> given;
		for (int i = 0; i < gives; ++i)
		{
			given.push_back(vm.Pop());
		}
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << name << " left the wrong number of values";
		return given;
	}

	float GetProperty(script::ObjectPropertyType property, entt::entity thing)
	{
		// the object is popped first, so it is pushed last
		const auto given = Call(
		    "GET_PROPERTY",
		    [property, thing](lhvm::LHVM& vm) {
			    vm.Pushi(static_cast<int32_t>(property));
			    vm.Pusho(static_cast<uint32_t>(thing));
		    },
		    1);
		return given.empty() ? -1.0f : given.front().floatVal;
	}

	void SetProperty(script::ObjectPropertyType property, entt::entity thing, float value)
	{
		Call(
		    "SET_PROPERTY",
		    [property, thing, value](lhvm::LHVM& vm) {
			    vm.Pushi(static_cast<int32_t>(property));
			    vm.Pusho(static_cast<uint32_t>(thing));
			    vm.Pushf(value);
		    },
		    0);
	}

	int32_t SubType(entt::entity thing)
	{
		const auto given = Call(
		    "GAME_SUB_TYPE", [thing](lhvm::LHVM& vm) { vm.Pusho(static_cast<uint32_t>(thing)); }, 1);
		return given.empty() ? -1 : given.front().intVal;
	}

	static entt::entity MakeCreature(CreatureType species)
	{
		const auto creature = Reg().Create();
		Reg().Assign<Transform>(creature);
		auto& data = Reg().Assign<Creature>(creature);
		data.species = species;
		return creature;
	}

	static entt::entity MakeVortex(VortexType type)
	{
		const auto vortex = Reg().Create();
		Reg().Assign<Transform>(vortex);
		Reg().Assign<LandscapeVortex>(vortex).type = type;
		return vortex;
	}

	static constexpr int32_t k_ScriptsOwn = 7777;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::vm> _restoreVm;
	chlapi::CHLApi _api;
};
} // namespace

TEST_F(ScriptVortexNatives, TheNativesTakeAndGiveWhatTheGameDoes)
{
	const auto& table = _api.GetFunctionsTable();
	struct Expected
	{
		uint32_t index;
		std::string_view name;
	};
	for (const auto& [index, name] :
	     {Expected {127, "SET_HEADING_AND_SPEED"}, Expected {221, "GAME_SUB_TYPE"}, Expected {328, "VORTEX_PARAMETERS"}})
	{
		ASSERT_LT(index, table.size());
		EXPECT_EQ(std::string_view(table[index].name), name);
	}
	EXPECT_EQ(static_cast<int>(script::ObjectPropertyType::CreatureCanGoThroughVortex), 36);
}

// Land 1 forbids the creature the vortex, then lets it through once the scroll is clicked
TEST_F(ScriptVortexNatives, ScriptsLetTheCreatureThroughTheVortex)
{
	using script::ObjectPropertyType;
	const auto creature = MakeCreature(CreatureType::Ogre);
	// no creature may until told
	EXPECT_FALSE(Reg().Get<Creature>(creature).mayGoThroughVortex);
	EXPECT_EQ(GetProperty(ObjectPropertyType::CreatureCanGoThroughVortex, creature), 0.0f);

	SetProperty(ObjectPropertyType::CreatureCanGoThroughVortex, creature, 0.5f);
	EXPECT_TRUE(Reg().Get<Creature>(creature).mayGoThroughVortex);
	EXPECT_EQ(GetProperty(ObjectPropertyType::CreatureCanGoThroughVortex, creature), 1.0f);

	SetProperty(ObjectPropertyType::CreatureCanGoThroughVortex, creature, 0.0f);
	EXPECT_FALSE(Reg().Get<Creature>(creature).mayGoThroughVortex);

	// anything else is not a creature, and answers 0
	const auto stone = Reg().Create();
	SetProperty(ObjectPropertyType::CreatureCanGoThroughVortex, stone, 1.0f);
	EXPECT_EQ(GetProperty(ObjectPropertyType::CreatureCanGoThroughVortex, stone), 0.0f);
}

// any number but 0 lets it through, below 0 too; a value that is not a number is taken as 0
TEST(ScriptVortexLetThrough, AnyNumberButZeroLetsTheCreatureThroughAndNotANumberDoesNot)
{
	EXPECT_FALSE(LetsThroughVortex(0.0f));
	EXPECT_FALSE(LetsThroughVortex(-0.0f));
	EXPECT_TRUE(LetsThroughVortex(1.0f));
	EXPECT_TRUE(LetsThroughVortex(-1.0f));
	EXPECT_TRUE(LetsThroughVortex(std::numeric_limits<float>::denorm_min()));
	EXPECT_FALSE(LetsThroughVortex(std::numeric_limits<float>::quiet_NaN()));
}

TEST_F(ScriptVortexNatives, SubTypesAreTheRowsTheScriptsKnow)
{
	// the scripts number the species from the giant ape
	EXPECT_EQ(SubType(MakeCreature(CreatureType::GiantApe)), 0);
	EXPECT_EQ(SubType(MakeCreature(CreatureType::Ogre)), static_cast<int32_t>(CreatureType::Ogre));
	EXPECT_EQ(SubType(MakeVortex(VortexType::Out)), 1);
	EXPECT_EQ(SubType(MakeVortex(VortexType::Volcano)), 2);
	// a thing whose kind can't be told gives a number no kind has; a thing that is gone gives 0
	EXPECT_EQ(SubType(Reg().Create()), 9999);
	const auto gone = Reg().Create();
	Reg().Destroy(gone);
	EXPECT_EQ(SubType(gone), 0);
}

TEST_F(ScriptVortexNatives, SettingOffAThingThatIsGoneDoesNothing)
{
	const auto gone = Reg().Create();
	Reg().Destroy(gone);
	Call(
	    "SET_HEADING_AND_SPEED",
	    [gone](lhvm::LHVM& vm) {
		    vm.Pusho(static_cast<uint32_t>(gone));
		    vm.Pushv(10.0f);
		    vm.Pushv(2.0f);
		    vm.Pushv(24.0f);
		    vm.Pushf(16.0f);
	    },
	    0);
}

TEST(ScriptHeading, TheSpeedIsAlongTheLineToThePlace)
{
	const auto velocity = HeadingVelocity({0.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 4.0f}, 10.0f);
	EXPECT_FLOAT_EQ(velocity.x, 6.0f);
	EXPECT_FLOAT_EQ(velocity.z, 8.0f);
	EXPECT_FLOAT_EQ(glm::length(HeadingVelocity({1.0f, 2.0f, 3.0f}, {-4.0f, 9.0f, 1.0f}, 16.0f)), 16.0f);
	// Land 1's returning shepherd, flung towards the camera along z
	const auto thrown = HeadingVelocity({10.0f, 2.0f, 20.0f}, {10.0f, 2.0f, 24.0f}, 16.0f);
	EXPECT_EQ(thrown, glm::vec3(0.0f, 0.0f, 16.0f));
	// already there: no velocity at all
	EXPECT_EQ(HeadingVelocity({1.0f, 2.0f, 3.0f}, {1.0f, 2.0f, 3.0f}, 16.0f), glm::vec3(0.0f));
}

TEST(ScriptHeading, TheSquaresAreSummedFromZToX)
{
	// The game adds the squares as z, then y, then x, each sum rounded to a float: for this line the length comes out
	// one bit apart from the x, y, z order, and so does every part of the velocity
	const glm::vec3 line(std::bit_cast<float>(0x42131C93u), std::bit_cast<float>(0xC194EF82u),
	                     std::bit_cast<float>(0x423776BAu));
	const auto velocity = HeadingVelocity(glm::vec3(0.0f), line, 10.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(velocity.x), 0x40BED83Eu);
	EXPECT_EQ(std::bit_cast<uint32_t>(velocity.y), 0xC04135FCu);
	EXPECT_EQ(std::bit_cast<uint32_t>(velocity.z), 0x40EE00E9u);
}

TEST(ScriptHeading, TheTumbleIsMinusOneToOne)
{
	EXPECT_FLOAT_EQ(HeadingSpin(0.0f), -1.0f);
	EXPECT_FLOAT_EQ(HeadingSpin(100.0f), 0.0f);
	EXPECT_FLOAT_EQ(HeadingSpin(200.0f), 1.0f);
	EXPECT_FLOAT_EQ(HeadingSpin(50.0f), -0.5f);
}
