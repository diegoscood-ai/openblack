/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// SET_ANIMATION_MODIFY, as a script calls it: only the value 1 sets real time on a creature's body, any other clears
// it; with it the body's animations step by the real time rather than at its size's rate.
// docs/wiki/creature/pose-and-animation.md (the script's real time)

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
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Registry.h"
#include "creature/CreatureSystemWorld.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
class CreatureAnimationModifyNative: public ::testing::Test
{
protected:
	CreatureAnimationModifyNative()
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

	/// The native as a script calls it: the value pushed first, then the creature; a value under them must stay
	void Call(uint32_t object, int32_t value)
	{
		const auto& table = Locator::chlapi::value().GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "SET_ANIMATION_MODIFY"; });
		ASSERT_NE(native, table.end());
		ASSERT_EQ(native->stackIn, 2);
		ASSERT_EQ(native->stackOut, 0u);
		_vm->Pushi(77);
		_vm->Pushi(value);
		_vm->Pusho(object);
		native->impl();
		EXPECT_EQ(_vm->Pop().intVal, 77);
	}

	static uint32_t Id(entt::entity entity) { return static_cast<uint32_t>(entt::to_integral(entity)); }
	static bool RealSpeed(entt::entity creature)
	{
		return test::creature_world::World::Registry().Get<ecs::components::CreatureAnimation>(creature).realSpeed;
	}

	test::creature_world::World _world;
	const test::RestoreService<Locator::vm> _restoreVm;
	const test::RestoreService<Locator::chlapi> _restoreApi;
	lhvm::LHVM* _vm;
};
} // namespace

TEST_F(CreatureAnimationModifyNative, OnlyOneSetsRealTimeAndAnyOtherValueClearsIt)
{
	const auto creature = test::creature_world::World::MakeCreature();
	EXPECT_FALSE(RealSpeed(creature));
	Call(Id(creature), 1);
	EXPECT_TRUE(RealSpeed(creature));
	Call(Id(creature), 2);
	EXPECT_FALSE(RealSpeed(creature));
	Call(Id(creature), 1);
	Call(Id(creature), 0);
	EXPECT_FALSE(RealSpeed(creature));
	EXPECT_EQ(Locator::chlapi::value().GetStubCalls().Total(), 0u);
}

TEST_F(CreatureAnimationModifyNative, NoThingOrNoCreatureIsLeftAlone)
{
	auto& registry = test::creature_world::World::Registry();
	const auto rock = registry.Create();
	const auto gone = registry.Create();
	registry.Destroy(gone);
	Call(0, 1);
	Call(Id(gone), 1);
	Call(Id(rock), 1);
	EXPECT_FALSE(registry.AllOf<ecs::components::CreatureAnimation>(rock));
}

TEST(CreatureRealSpeed, TheBodyStepsByTheRealTimeInsteadOfItsSizesRate)
{
	// at real speed the turn's own milliseconds, else the size's rate
	EXPECT_EQ(creature_layers::BodyRate(1.0f, true), 1.0f);
	EXPECT_EQ(creature_layers::BodyRate(1.0f, false), creature_layers::PlaybackRate(1.0f));
	EXPECT_EQ(creature_turn_pose::ClockOfTurn(1.0f, 100, 1.0f, true, false).step, 100);
	EXPECT_EQ(creature_turn_pose::ClockOfTurn(1.0f, 100, 1.0f, false, false).step,
	          creature_turn_pose::Step(100, 1.0f, false, false));
}
