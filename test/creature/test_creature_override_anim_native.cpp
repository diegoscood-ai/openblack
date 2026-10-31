/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// OVERRIDE_STATE_ANIMATION on a creature: a creature's body plays only its own animations, which a forced clip of the
// people's and animals' pack doesn't reach, so the native changes nothing and says nothing; it takes its two values

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <string_view>
#include <tuple>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Registry.h"
#include "creature/CreatureSystemWorld.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
class CreatureOverrideAnimNativeTest: public ::testing::Test
{
protected:
	CreatureOverrideAnimNativeTest()
	    : _vm(&Locator::vm::emplace<lhvm::LHVM>())
	{
		// the natives log their errors to the scripts' logger: a null sink, as the other script tests
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::chlapi::emplace<chlapi::CHLApi>();
		// a script's object 0 is no thing, so the first entity is not one the tests name
		std::ignore = test::creature_world::World::Registry().Create();
	}

	/// The native of that name
	static const lhvm::NativeFunction& Native(std::string_view name)
	{
		const auto& table = Locator::chlapi::value().GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [name](const lhvm::NativeFunction& f) { return std::string_view(f.name) == name; });
		EXPECT_NE(native, table.end());
		return *native;
	}

	static uint32_t Id(entt::entity entity) { return static_cast<uint32_t>(entt::to_integral(entity)); }

	test::creature_world::World _world;
	const test::RestoreService<Locator::vm> _restoreVm;
	const test::RestoreService<Locator::chlapi> _restoreApi;
	lhvm::LHVM* _vm;
};
} // namespace

TEST_F(CreatureOverrideAnimNativeTest, AForcedClipOnACreatureChangesNothingAndIsNoUnwrittenNative)
{
	const auto& native = Native("OVERRIDE_STATE_ANIMATION");
	const auto creature = test::creature_world::World::MakeCreature();
	// a value under the native's two, which it must leave there
	_vm->Pushi(77);
	// the script pushes the thing first, so the clip is on top
	_vm->Pusho(Id(creature));
	_vm->Pushi(60);
	native.impl();
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 0u);
	EXPECT_EQ(_vm->Pop().intVal, 77);
}

TEST_F(CreatureOverrideAnimNativeTest, AnInvalidClipOnACreatureIsSaidAndChangesNothing)
{
	const auto& native = Native("OVERRIDE_STATE_ANIMATION");
	const auto creature = test::creature_world::World::MakeCreature();
	for (const int32_t clip : {0, 441})
	{
		_vm->Pushi(77);
		_vm->Pusho(Id(creature));
		_vm->Pushi(clip);
		native.impl();
		EXPECT_EQ(_vm->Pop().intVal, 77) << clip;
	}
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 0u);
}
