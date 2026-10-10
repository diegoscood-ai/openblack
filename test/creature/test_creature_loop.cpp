/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creatures in the game loop (ECS/CreatureLoop.h): the order of the turn's and the frame's calls, the arguments
// they pass on, and the profile stages they are timed in, with recording fakes in the creature services

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/sparse_set.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureLayers.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/CreatureLoop.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Profiler.h"
#include "support/CreatureFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::test::creature_loop_fakes;

namespace
{
std::vector<std::string> NamesOf(const CallLog& log)
{
	std::vector<std::string> names;
	names.reserve(log.size());
	for (const auto& call : log)
	{
		names.push_back(call.name);
	}
	return names;
}

class CreatureLoopTest: public ::testing::Test
{
protected:
	CreatureLoopTest()
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::creaturePhysiologySystem::emplace<FakePhysiology>(log);
		Locator::creatureAnimationSystem::emplace<FakeAnimation>(log);
		Locator::creatureSkinSystem::emplace<FakeSkin>(log);
		Locator::creatureHairSystem::emplace<FakeHair>(log);
		Locator::creatureAudioSystem::emplace<FakeAudio>(log);
		Locator::footprintSystem::emplace<FakeFootprints>(log);
		locomotion = &static_cast<FakeLocomotion&>(Locator::creatureLocomotionSystem::emplace<FakeLocomotion>(log));
		Locator::creatureMindSystem::emplace<FakeMind>(log);
		objectActions = &static_cast<FakeObjectAction&>(Locator::creatureObjectActionSystem::emplace<FakeObjectAction>(log));
		fight = &static_cast<FakeFight&>(Locator::creatureFightSystem::emplace<FakeFight>(log));
		leash = &static_cast<FakeLeash&>(Locator::leashSystem::emplace<FakeLeash>(log));
	}

	/// Whether the stage was timed in the profiler's current frame
	[[nodiscard]] bool Timed(Profiler::Stage stage) const
	{
		const auto& entry = profiler->GetEntries().at(profiler->GetEntryIndex(0));
		return entry.stages.at(static_cast<uint8_t>(stage)).finalized;
	}

	CallLog log;
	std::unique_ptr<Profiler> profiler = std::make_unique<Profiler>();
	/// The leash service the test sets the player's creature and its leash in
	FakeLeash* leash {nullptr};
	/// What the fight, the walk and the actions set the bodies playing at a share of the turn
	FakeFight* fight {nullptr};
	FakeLocomotion* locomotion {nullptr};
	FakeObjectAction* objectActions {nullptr};

private:
	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	const test::RestoreService<Locator::creaturePhysiologySystem> _restorePhysiology;
	const test::RestoreService<Locator::creatureAnimationSystem> _restoreAnimation;
	const test::RestoreService<Locator::creatureSkinSystem> _restoreSkin;
	const test::RestoreService<Locator::creatureHairSystem> _restoreHair;
	const test::RestoreService<Locator::creatureAudioSystem> _restoreAudio;
	const test::RestoreService<Locator::footprintSystem> _restoreFootprints;
	const test::RestoreService<Locator::creatureLocomotionSystem> _restoreLocomotion;
	const test::RestoreService<Locator::creatureMindSystem> _restoreMind;
	const test::RestoreService<Locator::creatureObjectActionSystem> _restoreObjectAction;
	const test::RestoreService<Locator::creatureFightSystem> _restoreFight;
	const test::RestoreService<Locator::leashSystem> _restoreLeash;
};
} // namespace

TEST_F(CreatureLoopTest, TurnCallsTheServicesInOrder)
{
	ecs::creature_loop::ProcessTurn(*profiler);
	// the creatures' miracles come after the fights, from the spell code itself: with no creature they log nothing here;
	// the fatness the bodies show steps after everything the creatures did, then the bodies are posed for the turn, last
	const std::vector<std::string> expected {
	    "physiology.ProcessTurn",
	    "animation.ProcessTurn",
	    "skin.ProcessTurn",
	    "leash.ProcessTurn",
	    "mind.ProcessTurn",
	    "mind.PlanTurn",
	    "mind.LearnTurn",
	    "locomotion.ProcessTurn",
	    "objectAction.ProcessTurn",
	    "fight.ProcessTurn",
	    "physiology.ProcessShownFatness",
	    "animation.PoseTurn",
	};
	EXPECT_EQ(NamesOf(log), expected);
	// with no creature the bodies are given nothing to play, and no service is asked for it
	EXPECT_EQ(log.back().args, std::vector<float> {0.0f});
}

TEST_F(CreatureLoopTest, TheTurnsEndIsWhatTheFrameSetsInTheFramesOrder)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto creature = registry.Create();
	registry.AssignState<ecs::components::Creature>(creature);
	auto& animation = registry.AssignState<ecs::components::CreatureAnimation>(creature);
	animation.body.timeMs = 40.0f;
	animation.slots = {{.animation = 7, .timeMs = 10.0f, .weight = 1.0f, .mirrored = false}};
	const auto id = static_cast<float>(entt::to_integral(creature));
	const auto slotOf = [](size_t played) {
		return std::vector<ecs::components::CreatureAnimation::Slot> {
		    {.animation = played, .timeMs = 0.0f, .weight = 1.0f, .mirrored = false}};
	};

	// nothing set: what it plays now, the fight, the walk then the actions asked at the turn's end, as the frame calls
	// them
	auto inputs = ecs::creature_loop::AnimationInputsAt(1.0f);
	ASSERT_EQ(inputs.size(), 1u);
	EXPECT_EQ(inputs.front().first, creature);
	EXPECT_EQ(inputs.front().second.body.timeMs, 40.0f);
	ASSERT_EQ(inputs.front().second.slots.size(), 1u);
	EXPECT_EQ(inputs.front().second.slots.front().animation, 7u);
	const CallLog asked {{.name = "fight.AnimationAt", .args = {id, 1.0f}},
	                     {.name = "locomotion.AnimationAt", .args = {id, 1.0f}},
	                     {.name = "objectAction.AnimationAt", .args = {id, 1.0f}}};
	EXPECT_EQ(log, asked);

	// the fight's body plays in place of the slots, the walk's slots then over it, and the actions' over those
	fight->body = creature_layers::BodyAction {.timeMs = 300.0f};
	inputs = ecs::creature_loop::AnimationInputsAt(1.0f);
	EXPECT_EQ(inputs.front().second.body.timeMs, 300.0f);
	EXPECT_TRUE(inputs.front().second.slots.empty());
	locomotion->slots = slotOf(1);
	inputs = ecs::creature_loop::AnimationInputsAt(1.0f);
	EXPECT_EQ(inputs.front().second.body.timeMs, 300.0f);
	ASSERT_EQ(inputs.front().second.slots.size(), 1u);
	EXPECT_EQ(inputs.front().second.slots.front().animation, 1u);
	objectActions->slots = slotOf(2);
	inputs = ecs::creature_loop::AnimationInputsAt(1.0f);
	EXPECT_EQ(inputs.front().second.body.timeMs, 300.0f);
	ASSERT_EQ(inputs.front().second.slots.size(), 1u);
	EXPECT_EQ(inputs.front().second.slots.front().animation, 2u);
	// what it plays itself is left as it was
	EXPECT_EQ(animation.body.timeMs, 40.0f);
	EXPECT_EQ(animation.slots.front().animation, 7u);
}

TEST_F(CreatureLoopTest, TurnTimesEightStages)
{
	ecs::creature_loop::ProcessTurn(*profiler);
	for (const auto stage : {Profiler::Stage::CreaturePhysiologyUpdate, Profiler::Stage::CreatureLeashUpdate,
	                         Profiler::Stage::CreatureMindUpdate, Profiler::Stage::CreaturePlannerUpdate,
	                         Profiler::Stage::CreatureLearningUpdate, Profiler::Stage::CreatureLocomotionUpdate,
	                         Profiler::Stage::CreatureObjectActionUpdate, Profiler::Stage::CreatureCombatUpdate})
	{
		EXPECT_TRUE(Timed(stage)) << Profiler::k_StageNames.at(static_cast<uint8_t>(stage));
	}
	EXPECT_FALSE(Timed(Profiler::Stage::CreatureFrame));
	EXPECT_FALSE(Timed(Profiler::Stage::CreatureLeashFrame));
}

TEST_F(CreatureLoopTest, TurnMakesNoStorage)
{
	// the creatures' miracles walk the registry for creatures with spells: none, and no storage made for them
	const auto storages = [] {
		size_t count = 0;
		std::as_const(Locator::entitiesRegistry::value()).EachStorage([&count](entt::id_type, const entt::sparse_set&) {
			++count;
		});
		return count;
	};
	const auto before = storages();
	ecs::creature_loop::ProcessTurn(*profiler);
	EXPECT_EQ(storages(), before);
}

TEST_F(CreatureLoopTest, FrameCallsTheServicesInOrder)
{
	ecs::creature_loop::UpdateFrame(0.25f, 16, *profiler);
	const std::vector<std::string> expected {
	    "fight.Update",      "locomotion.Update",
	    "physiology.Update", "objectAction.UpdateDraw",
	    "animation.Update",  "objectAction.UpdateHeldDraw",
	    "hair.Update",       "audio.Update",
	    "footprints.Update", "skin.Update",
	};
	EXPECT_EQ(NamesOf(log), expected);
	EXPECT_TRUE(Timed(Profiler::Stage::CreatureFrame));
}

TEST_F(CreatureLoopTest, FrameTimesEachSystem)
{
	ecs::creature_loop::UpdateFrame(0.25f, 16, *profiler);
	for (const auto stage :
	     {Profiler::Stage::CreatureFrame, Profiler::Stage::CreatureAnimationUpdate, Profiler::Stage::CreatureHairUpdate,
	      Profiler::Stage::CreatureAudioUpdate, Profiler::Stage::CreatureFootprintsUpdate, Profiler::Stage::CreatureSkinUpdate})
	{
		EXPECT_TRUE(Timed(stage)) << Profiler::k_StageNames.at(static_cast<uint8_t>(stage));
	}
	// the turn's own stages are not timed by the frame, so the frame never overwrites the turn's time
	for (const auto stage : {Profiler::Stage::CreatureLeashUpdate, Profiler::Stage::CreatureMindUpdate,
	                         Profiler::Stage::CreaturePlannerUpdate, Profiler::Stage::CreatureLearningUpdate,
	                         Profiler::Stage::CreatureCombatUpdate, Profiler::Stage::CreatureLocomotionUpdate,
	                         Profiler::Stage::CreaturePhysiologyUpdate, Profiler::Stage::CreatureObjectActionUpdate})
	{
		EXPECT_FALSE(Timed(stage)) << Profiler::k_StageNames.at(static_cast<uint8_t>(stage));
	}
}

TEST_F(CreatureLoopTest, FramePassesTheTurnShareAndTheGameTime)
{
	ecs::creature_loop::UpdateFrame(0.25f, 16, *profiler);
	const CallLog expected {
	    {.name = "fight.Update", .args = {0.25f, 16.0f}},
	    {.name = "locomotion.Update", .args = {0.25f}},
	    {.name = "physiology.Update", .args = {16.0f * 0.001f}},
	    {.name = "objectAction.UpdateDraw", .args = {0.25f}},
	    {.name = "animation.Update", .args = {16.0f}},
	    {.name = "objectAction.UpdateHeldDraw", .args = {}},
	    {.name = "hair.Update", .args = {16.0f}},
	    {.name = "audio.Update", .args = {16.0f}},
	    {.name = "footprints.Update", .args = {16.0f}},
	    {.name = "skin.Update", .args = {}},
	};
	EXPECT_EQ(log, expected);
}

TEST_F(CreatureLoopTest, PausedFrameIsAZeroStep)
{
	// paused, the frame's game time is 0 and the turn's share stays where it was
	ecs::creature_loop::UpdateFrame(0.5f, 0, *profiler);
	ASSERT_EQ(log.size(), 10U);
	for (const auto& call : log)
	{
		if (call.name == "fight.Update")
		{
			EXPECT_EQ(call.args, (std::vector<float> {0.5f, 0.0f}));
		}
		else if (call.name == "locomotion.Update" || call.name == "objectAction.UpdateDraw")
		{
			EXPECT_EQ(call.args, std::vector<float> {0.5f});
		}
		else if (call.name != "skin.Update" && call.name != "objectAction.UpdateHeldDraw")
		{
			EXPECT_EQ(call.args, std::vector<float> {0.0f}) << call.name;
		}
	}
}

TEST_F(CreatureLoopTest, LeashSwingsByTheFrameSeconds)
{
	ecs::creature_loop::UpdateLeash(0.016f, *profiler);
	const CallLog expected {{.name = "leash.Update", .args = {0.016f}}};
	EXPECT_EQ(log, expected);
	EXPECT_TRUE(Timed(Profiler::Stage::CreatureLeashFrame));
}

TEST_F(CreatureLoopTest, NewLandClearsTheFootprints)
{
	ecs::creature_loop::OnLoadMap();
	const CallLog expected {{.name = "footprints.Reset", .args = {}}};
	EXPECT_EQ(log, expected);
}

TEST_F(CreatureLoopTest, AHandDemoTakesOffTheLeashHeldInTheHand)
{
	leash->playersCreature = static_cast<entt::entity>(7);
	leash->leashed = true;
	ecs::creature_loop::ReleaseLeashHeldInHand(PlayerNames::PLAYER_ONE);
	const CallLog expected {{.name = "leash.TakeOffHeldLeash", .args = {static_cast<float>(PlayerNames::PLAYER_ONE)}}};
	EXPECT_EQ(log, expected);
}

TEST_F(CreatureLoopTest, AHandDemoLeavesATiedLeashOrNone)
{
	// no creature, or one with no leash
	ecs::creature_loop::ReleaseLeashHeldInHand(PlayerNames::PLAYER_ONE);
	leash->playersCreature = static_cast<entt::entity>(7);
	ecs::creature_loop::ReleaseLeashHeldInHand(PlayerNames::PLAYER_ONE);
	// a leash tied to something stays
	leash->leashed = true;
	leash->tiedTo = static_cast<entt::entity>(9);
	ecs::creature_loop::ReleaseLeashHeldInHand(PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(log.empty());
}

TEST(CreatureLoopFrozen, OnlyPausedInsideTheCitadel)
{
	EXPECT_TRUE(ecs::creature_loop::FrozenInCitadel(true, true));
	EXPECT_FALSE(ecs::creature_loop::FrozenInCitadel(true, false));
	EXPECT_FALSE(ecs::creature_loop::FrozenInCitadel(false, true));
	EXPECT_FALSE(ecs::creature_loop::FrozenInCitadel(false, false));
}

TEST(CreatureLoopStages, NamesFitTheSummary)
{
	for (auto stage = static_cast<uint8_t>(Profiler::Stage::CreaturePhysiologyUpdate);
	     stage < static_cast<uint8_t>(Profiler::Stage::_count); ++stage)
	{
		EXPECT_LE(Profiler::k_StageNames.at(stage).size(), 22U) << Profiler::k_StageNames.at(stage);
		EXPECT_FALSE(Profiler::k_StageNames.at(stage).empty());
	}
}
