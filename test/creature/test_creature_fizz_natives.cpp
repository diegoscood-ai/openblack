/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The script natives that fizz a creature, with a fake of the fizz that keeps each fizz set: SET_OBJECT_FADE_IN, which
// takes a creature out of sight at once and fizzes it back in, and OBJECT_DELETE's fizzle, which fizzes a creature out
// for good and does not delete it

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <string_view>
#include <tuple>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Registry.h"
#include "creature/CreatureSystemWorld.h"
#include "support/CreatureFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
namespace loop_fakes = openblack::test::creature_loop_fakes;
using Set = loop_fakes::FakeFizz::Set;

namespace
{
class CreatureFizzNativesTest: public ::testing::Test
{
protected:
	CreatureFizzNativesTest()
	    : _vm(&Locator::vm::emplace<lhvm::LHVM>())
	    , _fizz(&static_cast<loop_fakes::FakeFizz&>(Locator::creatureFizzSystem::emplace<loop_fakes::FakeFizz>(_log)))
	{
		// the natives log their errors to the scripts' logger: a null sink, as the other script tests
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		// a script's object 0 is no thing, so the first entity is not one the tests name
		std::ignore = test::creature_world::World::Registry().Create();
	}

	/// The native of that name
	const lhvm::NativeFunction& Native(std::string_view name)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [name](const lhvm::NativeFunction& f) { return std::string_view(f.name) == name; });
		EXPECT_NE(native, table.end());
		return *native;
	}

	static uint32_t Id(entt::entity entity) { return static_cast<uint32_t>(entt::to_integral(entity)); }

	test::creature_world::World _world;
	const test::RestoreService<Locator::vm> _restoreVm;
	const test::RestoreService<Locator::creatureFizzSystem> _restoreFizz;
	loop_fakes::CallLog _log;
	lhvm::LHVM* _vm;
	loop_fakes::FakeFizz* _fizz;
	chlapi::CHLApi _api;
};
} // namespace

TEST_F(CreatureFizzNativesTest, FadingInTakesACreatureOutOfSightAtOnceAndFizzesItBackIn)
{
	const auto& native = Native("SET_OBJECT_FADE_IN");
	EXPECT_EQ(native.stackIn, 2);
	EXPECT_EQ(native.stackOut, 0u);
	const auto creature = test::creature_world::World::MakeCreature();
	// a value under the native's two, which it must leave there
	_vm->Pushi(77);
	// the script pushes the object first, so the seconds are on top
	_vm->Pusho(Id(creature));
	_vm->Pushf(3.0f);
	native.impl();
	const std::vector<Set> expected {
	    {.creature = creature, .target = 1.0f, .seconds = 0.0f, .goesForGood = false},
	    {.creature = creature, .target = 0.0f, .seconds = 3.0f, .goesForGood = false},
	};
	EXPECT_EQ(_fizz->sets, expected);
	EXPECT_EQ(_vm->Pop().intVal, 77);
}

TEST_F(CreatureFizzNativesTest, NothingElseFadesIn)
{
	const auto& native = Native("SET_OBJECT_FADE_IN");
	const auto rock = test::creature_world::World::Registry().Create();
	_vm->Pushi(77);
	// anything but a creature, or no thing at all: nothing fizzes, and both values are popped
	_vm->Pusho(Id(rock));
	_vm->Pushf(3.0f);
	native.impl();
	_vm->Pusho(0);
	_vm->Pushf(3.0f);
	native.impl();
	EXPECT_TRUE(_fizz->sets.empty());
	EXPECT_EQ(_vm->Pop().intVal, 77);
}

TEST_F(CreatureFizzNativesTest, DeletingWithAFadeFizzlesACreatureAwayForGoodAndKeepsIt)
{
	const auto& native = Native("OBJECT_DELETE");
	const auto creature = test::creature_world::World::MakeCreature();
	// the object, then the mode on top: mode 1 is the fade
	_vm->Pusho(Id(creature));
	_vm->Pushi(1);
	native.impl();
	ASSERT_EQ(_fizz->sets.size(), 1u);
	EXPECT_EQ(_fizz->sets.front(), (Set {.creature = creature, .target = 1.0f, .seconds = 2.0f, .goesForGood = true}));
	EXPECT_TRUE(std::as_const(test::creature_world::World::Registry()).Valid(creature));
}
