/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The script native PLAYED_PERCENTAGE, with a fake of the creatures' animations that knows their lengths: a creature
// answers how far through what its body plays it is, anything else 1

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <optional>
#include <string_view>
#include <tuple>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Creature/CreatureCatch.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureThrow.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Registry.h"
#include "creature/CreatureSystemWorld.h"
#include "support/CreatureFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
namespace loop_fakes = openblack::test::creature_loop_fakes;

namespace
{
class PlayedPercentageNativeTest: public ::testing::Test
{
protected:
	PlayedPercentageNativeTest()
	    : _vm(&Locator::vm::emplace<lhvm::LHVM>())
	    , _animation(&static_cast<loop_fakes::FakeAnimation&>(
	          Locator::creatureAnimationSystem::emplace<loop_fakes::FakeAnimation>(_log)))
	    , _fight(&static_cast<loop_fakes::FakeFight&>(Locator::creatureFightSystem::emplace<loop_fakes::FakeFight>(_log)))
	{
		// the natives log their errors to the scripts' logger: a null sink, as the other script tests
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		// a script's object 0 is no thing, so the first entity is not one the tests name
		std::ignore = test::creature_world::World::Registry().Create();
	}

	/// PLAYED_PERCENTAGE of a script object, with a value under it that it must leave there
	float PlayedPercentage(uint32_t object)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "PLAYED_PERCENTAGE"; });
		EXPECT_NE(native, table.end());
		EXPECT_EQ(native->stackIn, 1);
		EXPECT_EQ(native->stackOut, 1u);
		_vm->Pushi(77);
		_vm->Pusho(object);
		native->impl();
		const auto share = _vm->Popf();
		EXPECT_EQ(_vm->Pop().intVal, 77);
		return share;
	}

	static uint32_t Id(entt::entity entity) { return static_cast<uint32_t>(entt::to_integral(entity)); }

	test::creature_world::World _world;
	const test::RestoreService<Locator::vm> _restoreVm;
	const test::RestoreService<Locator::creatureAnimationSystem> _restoreAnimation;
	const test::RestoreService<Locator::creatureFightSystem> _restoreFight;
	loop_fakes::CallLog _log;
	lhvm::LHVM* _vm;
	loop_fakes::FakeAnimation* _animation;
	loop_fakes::FakeFight* _fight;
	chlapi::CHLApi _api;
};
} // namespace

TEST_F(PlayedPercentageNativeTest, ACreatureAnswersHowFarThroughItsActionItIs)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& body = test::creature_world::World::Registry().Get<ecs::components::CreatureAnimation>(creature);
	body.body = *creature_layers::PlayOnce({}, creature_layers::animations::k_Tired, false);
	body.body.timeMs = 1650.0f;
	_animation->durations[creature_layers::animations::k_Tired] = 2000.0f;
	EXPECT_FLOAT_EQ(PlayedPercentage(Id(creature)), 0.825f);
	// standing, it is the stand pose's time at the breath's point
	body.body = {};
	body.breathPhase = 0.25f;
	_animation->durations[creature_layers::animations::k_Stand] = 2208.0f;
	EXPECT_FLOAT_EQ(PlayedPercentage(Id(creature)), 552.0f / 2208.0f);
}

TEST_F(PlayedPercentageNativeTest, WalkingOrTurningTheLegsTimeIsOverTheStandsLength)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& legs = test::creature_world::World::Registry().Get<ecs::components::CreatureLocomotion>(creature);
	_animation->durations[creature_layers::animations::k_Stand] = 2000.0f;
	// running: the run's time at the end of the turn, the first of the legs' animations
	legs.tracks = {{.animation = creature_locomotion::animations::k_Run,
	                .fromMs = 400.0f,
	                .advanceMs = 101.5f,
	                .durationMs = 800.0f,
	                .weight = 1.0f,
	                .looping = true,
	                .breathing = false}};
	EXPECT_FLOAT_EQ(PlayedPercentage(Id(creature)), 501.0f / 2000.0f);
	// turning on the spot: the turn's own time
	legs.move = ecs::components::CreatureLocomotion::Move {.animations = std::nullopt,
	                                                       .timeMs = 300.0f,
	                                                       .durationMs = 900.0f,
	                                                       .displacement = {},
	                                                       .startHeading = 0.0f,
	                                                       .routeHeading = 0.0f};
	EXPECT_FLOAT_EQ(PlayedPercentage(Id(creature)), 300.0f / 2000.0f);
}

TEST_F(PlayedPercentageNativeTest, InAFightTheBodyIsMeasuredAgainstTheFightersAction)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& body = test::creature_world::World::Registry().Get<ecs::components::CreatureAnimation>(creature);
	// the fight shows its block start, while the fighter's action is the block itself
	constexpr size_t k_BlockStart = 120;
	constexpr size_t k_Block = 121;
	body.body = {.kind = creature_layers::BodyAction::Kind::Sequence,
	             .phase = creature_layers::BodyAction::Phase::Loop,
	             .animations = {k_BlockStart, k_BlockStart, k_BlockStart},
	             .timeMs = 450.0f,
	             .timedByPlayer = true};
	_animation->durations[k_BlockStart] = 500.0f;
	_animation->durations[k_Block] = 1500.0f;
	_animation->durations[creature_layers::animations::k_Stand] = 3000.0f;
	_fight->actionAnimations[creature] = k_Block;
	EXPECT_FLOAT_EQ(PlayedPercentage(Id(creature)), 450.0f / 1500.0f);
	// in its stance, against the stand pose
	_fight->actionAnimations[creature] = creature_fight::k_NoActionAnimation;
	EXPECT_FLOAT_EQ(PlayedPercentage(Id(creature)), 450.0f / 3000.0f);
	// out of the fight, against what the body plays
	_fight->actionAnimations.clear();
	EXPECT_FLOAT_EQ(PlayedPercentage(Id(creature)), 450.0f / 500.0f);
}

TEST_F(PlayedPercentageNativeTest, AnActionOnAThingIsMeasuredByItsTimeAgainstTheAnimationItIsNamedBy)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	_animation->durations[creature_layers::animations::k_Stand] = 2000.0f;
	_animation->durations[creature_throw::k_PutDown] = 800.0f;
	auto& action = registry.AssignOrReplace<ecs::components::CreatureObjectAction>(creature);
	action.kind = creature_object_actions::Kind::PutDown;
	action.phase = ecs::components::CreatureObjectAction::Phase::Playing;
	action.animations = {creature_throw::k_PutDown, 0, 0, 0};
	action.timeMs = 400.6f;
	action.durationMs = 800.0f;
	// letting go: against what it plays
	EXPECT_FLOAT_EQ(PlayedPercentage(Id(creature)), 0.5f);
	// throwing: against the stand pose
	action.kind = creature_object_actions::Kind::Throw;
	action.animations = {creature_throw::k_HurlFlat, creature_throw::k_HurlHigh, 0, 0};
	EXPECT_FLOAT_EQ(PlayedPercentage(Id(creature)), 400.0f / 2000.0f);
	// walking up to it, or done with it, the body as without one
	action.phase = ecs::components::CreatureObjectAction::Phase::Approach;
	EXPECT_FLOAT_EQ(PlayedPercentage(Id(creature)), 0.0f);
}

TEST_F(PlayedPercentageNativeTest, ACatchIsMeasuredAgainstTheStandPoseUntilItStepsThenAgainstTheStep)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	_animation->durations[creature_layers::animations::k_Stand] = 2000.0f;
	_animation->durations[creature_catch::k_CatchStep] = 600.0f;
	auto& catching = registry.AssignOrReplace<ecs::components::CreatureCatching>(creature);
	catching.timeMs = 300;
	// reaching with no step: the stand pose
	catching.stage = ecs::components::CreatureCatching::Stage::Reaching;
	EXPECT_FLOAT_EQ(PlayedPercentage(Id(creature)), 300.0f / 2000.0f);
	// stepping, and waiting and reaching after the step: the step
	catching.stepped = true;
	for (const auto stage :
	     {ecs::components::CreatureCatching::Stage::Stepping, ecs::components::CreatureCatching::Stage::Ready,
	      ecs::components::CreatureCatching::Stage::Reaching})
	{
		catching.stage = stage;
		EXPECT_FLOAT_EQ(PlayedPercentage(Id(creature)), 0.5f);
	}
}

TEST_F(PlayedPercentageNativeTest, AnythingButACreatureAnswersOne)
{
	const auto rock = test::creature_world::World::Registry().Create();
	EXPECT_FLOAT_EQ(PlayedPercentage(Id(rock)), 1.0f);
	EXPECT_FLOAT_EQ(PlayedPercentage(0), 1.0f);
	// a creature whose animation has no length here
	EXPECT_FLOAT_EQ(PlayedPercentage(Id(test::creature_world::World::MakeCreature())), 1.0f);
}
