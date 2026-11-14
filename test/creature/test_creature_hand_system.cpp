/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature hand system with a fake hand and a fake mind: who the hand may hold, the creature along a line of sight,
// strokes and slaps over the frames, and what the hand is given to send on as it lets go

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/sparse_set.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Components/HandOnCreature.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Systems/Implementations/CreatureHandSystem.h"
#include "creature/CreatureSystemFakes.h"
#include "creature/CreatureSystemWorld.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureHandSystem;
using openblack::test::creature_fakes::FakeHand;
using openblack::test::creature_fakes::FakeMind;

namespace
{
class CreatureHandSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto& hand = static_cast<FakeHand&>(Locator::handSystem::emplace<FakeHand>());
		hand.left = test::creature_world::World::Registry().Create();
		_hand = &hand;
		_mind = &static_cast<FakeMind&>(Locator::creatureMindSystem::emplace<FakeMind>());
	}

	/// The hand held to a creature, as Grab leaves it
	HandOnCreature& HoldTo(entt::entity creature)
	{
		return test::creature_world::World::Registry().Assign<HandOnCreature>(_hand->left,
		                                                                      HandOnCreature {.creature = creature});
	}

	/// An ape standing upright: a body of two bones from its feet to its head, its rig in the cache
	entt::entity PosedApe()
	{
		test::creature_block::Block block;
		block.clips = {test::creature_block::Clip {}};
		_world.LoadApeRig(block, {{"move", {"Cstand"}}});
		const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f));
		auto& animation = test::creature_world::World::Registry().Get<CreatureAnimation>(creature);
		animation.skeleton.parents = {skeletal_animation::k_NoParent, 0};
		animation.boneMatrices = {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 15.0f, 0.0f))};
		test::creature_world::World::Registry().Assign<CreatureTurnPose>(creature, CreatureTurnPose {.current = TurnPose()});
		return creature;
	}

	/// The ape's pose for the game turn, in the world: every bone the rig's parts name at its feet, one of them, if any,
	/// at a place of its own
	static std::vector<glm::mat4> TurnPose(std::optional<std::pair<uint32_t, glm::vec3>> moved = std::nullopt)
	{
		std::vector<glm::mat4> bones(k_TurnPoseBones, glm::translate(glm::mat4(1.0f), k_Feet));
		if (moved.has_value())
		{
			bones.at(moved->first) = glm::translate(glm::mat4(1.0f), moved->second);
		}
		return bones;
	}

	/// The synthetic rig names its parts' bones up to 12, and the ape stands with its feet here
	static constexpr size_t k_TurnPoseBones = 13;
	static constexpr glm::vec3 k_Feet {100.0f, 0.0f, 100.0f};

	const test::ScopedDefaultFileSystem _fileSystem;
	test::creature_world::World _world;
	const test::RestoreService<Locator::handSystem> _restoreHand;
	const test::RestoreService<Locator::creatureMindSystem> _restoreMind;
	FakeHand* _hand {nullptr};
	FakeMind* _mind {nullptr};
	CreatureHandSystem _system;
};
/// The cursor still since last frame: no sweep, whatever the eye
CreatureHandSystem::CursorSweep Still()
{
	return {.nearNow = glm::vec3(0.0f, -1000.0f, 0.0f), .nearLast = glm::vec3(0.0f, -1000.0f, 0.0f), .distance = 4.0f};
}
} // namespace

TEST_F(CreatureHandSystemTest, OnlyAPlayersCreatureMayBeHeld)
{
	const auto own = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_ONE);
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_THREE);
	const auto nobodys = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::NEUTRAL);
	EXPECT_TRUE(_system.MayHold(own));
	EXPECT_TRUE(_system.MayHold(other));
	EXPECT_FALSE(_system.MayHold(nobodys));
	const auto thing = test::creature_world::World::Registry().Create();
	EXPECT_FALSE(_system.MayHold(thing));
	// what may not be held is not taken hold of
	EXPECT_FALSE(_system.Grab(nobodys));
	EXPECT_FALSE(_system.Grab(thing));
	EXPECT_FALSE(_system.GetCreature().has_value());
	// a creature that may be is
	EXPECT_TRUE(_system.Grab(own));
	EXPECT_EQ(_system.GetCreature(), std::optional(own));
}

TEST_F(CreatureHandSystemTest, AKnockedOutCreatureIsNotHeldUntilItIsHome)
{
	auto& registry = test::creature_world::World::Registry();
	const auto own = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_ONE);
	registry.AssignState<CreatureKnockedOut>(own, CreatureKnockedOut {.stage = CreatureKnockedOut::Stage::FadingOut});
	EXPECT_FALSE(_system.MayHold(own));
	EXPECT_FALSE(_system.Grab(own));
	// back home it may be held again
	registry.Get<CreatureKnockedOut>(own).stage = CreatureKnockedOut::Stage::Waiting;
	EXPECT_TRUE(_system.MayHold(own));
	EXPECT_TRUE(_system.Grab(own));
}

TEST_F(CreatureHandSystemTest, TheTurnHoldsTheCreatureThePressSentWhateverItBecameSince)
{
	auto& registry = test::creature_world::World::Registry();
	const auto own = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_ONE);
	// knocked out since the press: the hand would not take hold of it now, but the turn does
	registry.AssignState<CreatureKnockedOut>(own, CreatureKnockedOut {.stage = CreatureKnockedOut::Stage::Lying});
	ASSERT_FALSE(_system.MayHold(own));
	EXPECT_TRUE(_system.HoldAtTurn(own));
	EXPECT_EQ(_system.GetCreature(), std::optional(own));
	// it still holds only a creature
	EXPECT_FALSE(_system.HoldAtTurn(registry.Create()));
}

TEST_F(CreatureHandSystemTest, TheNearestCreatureAlongASightAndHowFar)
{
	const auto creature = PosedApe();
	// from 50 in front of the ape, at its middle: its body some way off, nearer than 50
	const auto hit = _system.CreatureAlong(glm::vec3(100.0f, 7.0f, 150.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->creature, creature);
	EXPECT_GT(hit->distance, 40.0f);
	EXPECT_LT(hit->distance, 50.0f);
	// looking away from it: none
	EXPECT_FALSE(_system.CreatureAlong(glm::vec3(100.0f, 7.0f, 150.0f), glm::vec3(0.0f, 0.0f, 1.0f)).has_value());
}

TEST_F(CreatureHandSystemTest, TheHandTouchesTheBodyAsItIsDrawnBetweenItsTurns)
{
	const auto creature = PosedApe();
	auto& registry = test::creature_world::World::Registry();
	auto& animation = registry.Get<CreatureAnimation>(creature);
	// drawn between its last two turn poses some way to the side of where the frame poses it
	const auto aside = glm::translate(glm::mat4(1.0f), glm::vec3(40.0f, 0.0f, 0.0f));
	animation.drawnBones = {aside * animation.boneMatrices[0], aside * animation.boneMatrices[1]};
	const auto drawnAt = glm::vec3((ecs::DrawnBodyModel(std::as_const(registry), creature) * animation.drawnBones[0])[3]);
	ASSERT_GT(std::abs(drawnAt.x - 100.0f), 10.0f);
	// the sight through the frame's pose misses; the one through the drawn body finds it
	EXPECT_FALSE(_system.CreatureAlong(glm::vec3(100.0f, 7.0f, 150.0f), glm::vec3(0.0f, 0.0f, -1.0f)).has_value());
	const auto hit = _system.CreatureAlong(glm::vec3(drawnAt.x, 7.0f, 150.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->creature, creature);
	// without drawn bones (in the hand, in flight, before its first turn) it is touched as the frame poses it
	animation.drawnBones.clear();
	EXPECT_TRUE(_system.CreatureAlong(glm::vec3(100.0f, 7.0f, 150.0f), glm::vec3(0.0f, 0.0f, -1.0f)).has_value());
}

TEST_F(CreatureHandSystemTest, InItsPenTheBodyTheHandTouchesIsAsThickAsThePenSizes)
{
	const auto creature = PosedApe();
	const glm::vec3 from {100.0f, 7.0f, 150.0f};
	const glm::vec3 ahead {0.0f, 0.0f, -1.0f};
	const auto own = _system.CreatureAlong(from, ahead);
	ASSERT_TRUE(own.has_value());
	// the same bones, their capsules as thick as the size it is drawn at: 0.22, not its own 1
	auto& registry = test::creature_world::World::Registry();
	ASSERT_EQ(registry.Get<Creature>(creature).size, 1.0f);
	registry.Get<CreatureDrawPose>(creature).size = 0.22f;
	const auto penned = _system.CreatureAlong(from, ahead);
	ASSERT_TRUE(penned.has_value());
	EXPECT_EQ(penned->creature, creature);
	EXPECT_NEAR(penned->distance - own->distance,
	            creature_feedback::k_BodyRadiusShare * creature_feedback::k_HeightAtSizeOne * (1.0f - 0.22f), 1e-3f);
}

TEST_F(CreatureHandSystemTest, TheCreatureUnderTheHandIsWhatTheHandStored)
{
	EXPECT_FALSE(_system.CreatureUnderHand().has_value());
	const auto creature = test::creature_world::World::MakeCreature();
	_system.SetCreatureUnderHand(creature);
	EXPECT_EQ(_system.CreatureUnderHand(), std::optional(creature));
	_system.SetCreatureUnderHand(std::nullopt);
	EXPECT_FALSE(_system.CreatureUnderHand().has_value());
}

TEST_F(CreatureHandSystemTest, WithoutAContactUpdateMakesNoStorage)
{
	const auto storages = []() {
		std::vector<std::pair<entt::id_type, size_t>> all;
		std::as_const(test::creature_world::World::Registry())
		    .EachStorage([&all](entt::id_type id, const entt::sparse_set& storage) { all.emplace_back(id, storage.size()); });
		return all;
	};
	const auto before = storages();
	EXPECT_FALSE(
	    _system.Update(glm::vec3(0.0f, 10.0f, 50.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f), 800.0f, 0.016f, Still()));
	EXPECT_FALSE(_system.Release().has_value());
	EXPECT_EQ(storages(), before);
	EXPECT_TRUE(_mind->forced.empty());
}

TEST_F(CreatureHandSystemTest, Resting_TheHandStrokes_SweptFastItSlaps)
{
	const auto creature = PosedApe();
	HoldTo(creature);
	const glm::vec3 ahead {0.0f, 0.0f, -1.0f};
	constexpr float k_ScreenWidth = 800.0f;
	// resting on the body, the cursor moving on the left of the screen: the first stroke two seconds after taking hold
	for (int frame = 0; frame < 22; ++frame)
	{
		const auto cursor = glm::vec2(300.0f + static_cast<float>(frame), 300.0f);
		const auto pose = _system.Update(glm::vec3(100.0f, 7.0f, 150.0f), ahead, cursor, k_ScreenWidth, 0.1f, Still());
		ASSERT_TRUE(pose.has_value());
		EXPECT_TRUE(pose->onBody);
		EXPECT_EQ(_system.Pose()->position, pose->position);
		EXPECT_EQ(_mind->forced.size(), frame < 19 ? 0u : 1u) << "frame " << frame;
	}
	ASSERT_EQ(_mind->forced.size(), 1u);
	EXPECT_FLOAT_EQ(_system.GetFeedbackSum(), creature_feedback::k_StrokeAmount);
	EXPECT_EQ(_system.Pose()->cycle, creature_feedback::TouchCycle::Tickle);

	// beside the body for over a second after the stroke, the hand wiggles
	for (int frame = 0; frame < 11; ++frame)
	{
		EXPECT_FALSE(
		    _system.Update(glm::vec3(95.0f, 7.0f, 150.0f), ahead, glm::vec2(295.0f, 300.0f), k_ScreenWidth, 0.1f, Still())
		        ->onBody);
	}
	EXPECT_EQ(_system.Pose()->cycle, creature_feedback::TouchCycle::Wiggle);
	// swept fast onto the body from beside it: last frame's cursor 5.5 to the left, both seen 4 from the eye, a gentle
	// slap, about 102 a second across the land against the 75 of a creature 15 high, which takes the stroke back
	EXPECT_TRUE(
	    _system
	        .Update(glm::vec3(100.5f, 7.0f, 150.0f), ahead, glm::vec2(305.0f, 300.0f), k_ScreenWidth, 0.05f,
	                {.nearNow = glm::vec3(100.5f, 7.0f, 149.0f), .nearLast = glm::vec3(95.0f, 7.0f, 149.0f), .distance = 4.0f})
	        ->onBody);
	ASSERT_EQ(_mind->forced.size(), 2u);
	EXPECT_LT(_system.GetFeedbackSum(), creature_feedback::k_StrokeAmount);
	EXPECT_EQ(_system.Pose()->cycle, creature_feedback::TouchCycle::Slap);
}

TEST_F(CreatureHandSystemTest, MissingTheBodyTheHandLooksAgainAlongTheSecondLine)
{
	const auto creature = PosedApe();
	HoldTo(creature);
	const glm::vec3 ahead {0.0f, 0.0f, -1.0f};
	constexpr float k_ScreenWidth = 800.0f;
	const glm::vec3 beside {95.0f, 7.0f, 150.0f};
	const glm::vec3 onBody {100.0f, 7.0f, 150.0f};
	// the line through the cursor misses, and there is no second line: beside the body
	EXPECT_FALSE(_system.Update(beside, ahead, glm::vec2(295.0f, 300.0f), k_ScreenWidth, 0.1f, Still())->onBody);
	// the line through the cursor misses, the second one meets the body: on it, where the second line meets it
	auto again = Still();
	again.secondLook = CreatureHandSystem::SightLine {.origin = onBody, .direction = ahead};
	const auto pose = _system.Update(beside, ahead, glm::vec2(296.0f, 300.0f), k_ScreenWidth, 0.1f, again);
	ASSERT_TRUE(pose.has_value());
	EXPECT_TRUE(pose->onBody);
	EXPECT_FLOAT_EQ(pose->position.x, onBody.x);
	EXPECT_FLOAT_EQ(pose->position.y, onBody.y);
	EXPECT_EQ(test::creature_world::World::Registry().Get<HandOnCreature>(_hand->left).secondLookHits, 1u);
	// the line through the cursor meets the body: the second line is not looked along
	again.secondLook = CreatureHandSystem::SightLine {.origin = beside, .direction = ahead};
	const auto first = _system.Update(onBody, ahead, glm::vec2(297.0f, 300.0f), k_ScreenWidth, 0.1f, again);
	ASSERT_TRUE(first.has_value());
	EXPECT_TRUE(first->onBody);
	EXPECT_FLOAT_EQ(first->position.x, onBody.x);
	EXPECT_EQ(test::creature_world::World::Registry().Get<HandOnCreature>(_hand->left).secondLookHits, 1u);
}

TEST(CreatureFeedback, TheSecondLookIsHalfwayBetweenTheCursorAndItsLastPixelRoundedDown)
{
	EXPECT_EQ(creature_feedback::SecondLookPixel({300, 200}, {300, 200}), glm::ivec2(300, 200));
	EXPECT_EQ(creature_feedback::SecondLookPixel({301, 200}, {304, 197}), glm::ivec2(302, 198));
	EXPECT_EQ(creature_feedback::SecondLookPixel({-1, 0}, {0, -3}), glm::ivec2(-1, -2));
}

TEST_F(CreatureHandSystemTest, TheStrokeLandsOnThePartNearestInTheLatestTurnPose)
{
	const auto creature = PosedApe();
	auto& registry = test::creature_world::World::Registry();
	auto& pose = registry.Get<CreatureTurnPose>(creature);
	// the synthetic rig's belly and groin bones
	constexpr uint32_t k_Belly = 7;
	constexpr uint32_t k_Groin = 10;
	const glm::vec3 touched {100.0f, 7.0f, 100.0f};
	// the pose kept as the hand takes hold has the belly where the hand touches; the turns after it, the groin
	pose.current = TurnPose(std::pair(k_Belly, touched));
	creature_turn_pose::Freeze(pose);
	creature_turn_pose::Advance(pose, TurnPose(std::pair(k_Groin, touched)));
	// and the body as it is drawn this frame has the belly there too
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.drawnBones = animation.boneMatrices;
	animation.drawnBones.resize(k_TurnPoseBones, glm::mat4(1.0f));
	animation.drawnBones.at(k_Belly) = glm::translate(glm::mat4(1.0f), touched - k_Feet);
	HoldTo(creature);
	const glm::vec3 ahead {0.0f, 0.0f, -1.0f};
	for (int frame = 0; frame < 22; ++frame)
	{
		const auto cursor = glm::vec2(300.0f + static_cast<float>(frame), 300.0f);
		ASSERT_TRUE(_system.Update(glm::vec3(100.0f, 7.0f, 150.0f), ahead, cursor, 800.0f, 0.1f, Still())->onBody);
	}
	// the groin's pleased animation: the stroke weighs the latest pose, not the kept one nor the drawn body
	ASSERT_EQ(_mind->forced.size(), 1u);
	EXPECT_EQ(_mind->forced.front(),
	          creature_feedback::k_RewardAnimations.at(static_cast<size_t>(creature_feedback::BodyPart::Groin)));
}

TEST(CreatureFeedback, TheHeldCreatureLooksAtTheEyeUntilTheHandIsFarFromItsBottomThenNearTheHand)
{
	constexpr glm::vec3 k_Eye {0.0f, 20.0f, -40.0f};
	constexpr glm::vec3 k_Sight {0.0f, 0.0f, 1.0f};
	constexpr glm::vec3 k_Bottom {0.0f, 5.0f, 0.0f};
	constexpr float k_Height = 15.0f;
	// 9 from the bottom, 0.6 of the height: still at the eye
	EXPECT_EQ(creature_feedback::HeldLookPoint(k_Eye, k_Sight, {0.0f, 14.0f, 0.0f}, k_Bottom, k_Height), k_Eye);
	// further: the hand moved back 0.3 of the height along the line of sight, 4.5 towards the eye
	const auto look = creature_feedback::HeldLookPoint(k_Eye, k_Sight, {0.0f, 15.0f, 0.0f}, k_Bottom, k_Height);
	EXPECT_FLOAT_EQ(look.x, 0.0f);
	EXPECT_FLOAT_EQ(look.y, 15.0f);
	EXPECT_FLOAT_EQ(look.z, -4.5f);
	// a height that can't be compared counts as beyond: near the hand
	const auto unknown =
	    creature_feedback::HeldLookPoint(k_Eye, k_Sight, {1.0f, 5.0f, 0.0f}, k_Bottom, std::numeric_limits<float>::quiet_NaN());
	EXPECT_NE(unknown, k_Eye);
}

TEST_F(CreatureHandSystemTest, TheCaressWaitsForASecondOnTheBodyInAllWithTheCursorMoving)
{
	const auto creature = PosedApe();
	HoldTo(creature);
	const glm::vec3 ahead {0.0f, 0.0f, -1.0f};
	const glm::vec3 onBody {100.0f, 7.0f, 150.0f};
	const glm::vec3 beside {95.0f, 7.0f, 150.0f};
	constexpr float k_ScreenWidth = 800.0f;
	// taking hold starts the caress; beside the body it turns to the wiggle
	EXPECT_FALSE(_system.Update(beside, ahead, glm::vec2(290.0f, 300.0f), k_ScreenWidth, 0.1f, Still())->onBody);
	EXPECT_EQ(_system.Pose()->cycle, creature_feedback::TouchCycle::Wiggle);
	// on the body, the cursor moving: no caress until the hand has rested there more than a second
	for (int frame = 0; frame < 10; ++frame)
	{
		const auto pose =
		    _system.Update(onBody, ahead, glm::vec2(300.0f + static_cast<float>(frame), 300.0f), k_ScreenWidth, 0.1f, Still());
		ASSERT_TRUE(pose.has_value());
		EXPECT_TRUE(pose->onBody);
		EXPECT_EQ(pose->cycle, creature_feedback::TouchCycle::Wiggle) << "frame " << frame;
	}
	// past the second, still, it waits for the cursor to move
	EXPECT_EQ(_system.Update(onBody, ahead, glm::vec2(309.0f, 300.0f), k_ScreenWidth, 0.1f, Still())->cycle,
	          creature_feedback::TouchCycle::Wiggle);
	EXPECT_EQ(_system.Update(onBody, ahead, glm::vec2(310.0f, 300.0f), k_ScreenWidth, 0.1f, Still())->cycle,
	          creature_feedback::TouchCycle::Tickle);
	// the time on the body is kept beside it: back on it, the moving hand caresses at once
	EXPECT_EQ(_system.Update(beside, ahead, glm::vec2(305.0f, 300.0f), k_ScreenWidth, 0.1f, Still())->cycle,
	          creature_feedback::TouchCycle::Wiggle);
	EXPECT_EQ(_system.Update(onBody, ahead, glm::vec2(310.0f, 300.0f), k_ScreenWidth, 0.1f, Still())->cycle,
	          creature_feedback::TouchCycle::Tickle);
}

TEST_F(CreatureHandSystemTest, OnlyTheChangesToTheCaressOrTheSlapStartTheHandsBlend)
{
	const auto creature = PosedApe();
	HoldTo(creature);
	const glm::vec3 ahead {0.0f, 0.0f, -1.0f};
	const glm::vec3 onBody {100.0f, 7.0f, 150.0f};
	const glm::vec3 beside {95.0f, 7.0f, 150.0f};
	constexpr float k_ScreenWidth = 800.0f;
	// the caress turning to the wiggle beside the body starts none
	EXPECT_EQ(_system.Update(beside, ahead, glm::vec2(290.0f, 300.0f), k_ScreenWidth, 0.1f, Still())->blendStarts, 0);
	// resting on the body, the cursor moving: none until the caress comes back, then one, then none while it goes on
	for (int frame = 0; frame < 13; ++frame)
	{
		const auto pose =
		    _system.Update(onBody, ahead, glm::vec2(300.0f + static_cast<float>(frame), 300.0f), k_ScreenWidth, 0.1f, Still());
		ASSERT_TRUE(pose.has_value());
		EXPECT_EQ(pose->blendStarts, frame == 10 ? 1 : 0) << "frame " << frame;
	}
	EXPECT_EQ(test::creature_world::World::Registry().Get<HandOnCreature>(_hand->left).cycleBlends, 1u);
	// a frame with no time starts none
	EXPECT_EQ(_system.Update(onBody, ahead, glm::vec2(320.0f, 300.0f), k_ScreenWidth, 0.0f, Still())->blendStarts, 0);
}

TEST_F(CreatureHandSystemTest, ACommandStrokesAndSlapsAsTheButtonWouldAndHoldsTheCreature)
{
	const auto creature = PosedApe();
	// a stroke on the head: the hand takes hold by the command and the creature reacts
	EXPECT_TRUE(_system.Stroke(creature, creature_feedback::BodyPart::Head));
	EXPECT_EQ(_system.GetCreature(), creature);
	EXPECT_TRUE(_system.IsHeldByCommand());
	ASSERT_EQ(_mind->forced.size(), 1u);
	EXPECT_FLOAT_EQ(_system.GetFeedbackSum(), creature_feedback::k_StrokeAmount);
	// a hard slap across the middle takes some of it back
	EXPECT_TRUE(_system.Slap(creature, 0.5f, false, true));
	ASSERT_EQ(_mind->forced.size(), 2u);
	EXPECT_LT(_system.GetFeedbackSum(), creature_feedback::k_StrokeAmount);
	// held to it, another creature is not taken by a command: the hand has to let go first
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(120.0f, 0.0f, 100.0f));
	EXPECT_FALSE(_system.Stroke(other, creature_feedback::BodyPart::Belly));
	EXPECT_EQ(_system.GetCreature(), creature);
	// let go, the hand is held by nothing
	EXPECT_TRUE(_system.Release().has_value());
	EXPECT_FALSE(_system.IsHeldByCommand());
}

TEST_F(CreatureHandSystemTest, AHoldByTheButtonIsNotByCommand)
{
	const auto creature = PosedApe();
	EXPECT_TRUE(_system.Grab(creature));
	EXPECT_FALSE(_system.IsHeldByCommand());
	// a command on the creature it holds keeps the hold and marks it
	EXPECT_TRUE(_system.Stroke(creature, creature_feedback::BodyPart::Belly));
	EXPECT_TRUE(_system.IsHeldByCommand());
}

TEST_F(CreatureHandSystemTest, LettingGoGivesHowTheCreatureWasTreated)
{
	const auto creature = test::creature_world::World::MakeCreature();
	HoldTo(creature).sum = 0.6f;
	EXPECT_FLOAT_EQ(_system.GetLastFeedbackSum(), 0.6f);
	const auto given = _system.Release();
	ASSERT_TRUE(given.has_value());
	EXPECT_EQ(given->creature, creature);
	EXPECT_FLOAT_EQ(given->feedback, creature_feedback::Delivered(0.6f));
	// the hand sends it on as a packet: the mind is not told here
	EXPECT_TRUE(_mind->feedback.empty());
	// let go, the sum it let go with is kept for the panel
	EXPECT_FALSE(_system.GetCreature().has_value());
	EXPECT_FALSE(_system.Pose().has_value());
	EXPECT_FLOAT_EQ(_system.GetFeedbackSum(), 0.0f);
	EXPECT_FLOAT_EQ(_system.GetLastFeedbackSum(), 0.6f);
	EXPECT_TRUE(std::as_const(test::creature_world::World::Registry()).AllOf<HandLastFeedback>(_hand->left));
	// letting go again gives nothing more
	EXPECT_FALSE(_system.Release().has_value());
}

TEST_F(CreatureHandSystemTest, TheFeedbackGivenIsClampedToOne)
{
	const auto creature = test::creature_world::World::MakeCreature();
	HoldTo(creature).sum = 3.0f;
	EXPECT_FLOAT_EQ(_system.Release()->feedback, 1.0f);
	HoldTo(creature).sum = -3.0f;
	EXPECT_FLOAT_EQ(_system.Release()->feedback, -1.0f);
}

TEST_F(CreatureHandSystemTest, TakingHoldStartsWithACleanSlate)
{
	const auto own = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_ONE);
	auto& registry = test::creature_world::World::Registry();
	// what an earlier hold left: a part stroked, a slap waited for, a sum, and the sum it last let go with
	auto& earlier = HoldTo(own);
	earlier.sum = 0.6f;
	earlier.slapReadyMs = 500.0f;
	earlier.lastPart = creature_feedback::BodyPart::Belly;
	earlier.strokedOrSlapped = true;
	registry.Assign<HandLastFeedback>(_hand->left, HandLastFeedback {.sum = 0.4f});
	ASSERT_TRUE(_system.Grab(own));
	const auto& fresh = std::as_const(registry).Get<const HandOnCreature>(_hand->left);
	EXPECT_EQ(fresh.creature, own);
	EXPECT_FLOAT_EQ(fresh.sum, 0.0f);
	EXPECT_FLOAT_EQ(fresh.slapReadyMs, 0.0f);
	EXPECT_FALSE(fresh.lastPart.has_value());
	EXPECT_FALSE(fresh.strokedOrSlapped);
	EXPECT_FALSE(std::as_const(registry).AllOf<HandLastFeedback>(_hand->left));
}

// Held, the creature is made to look at the eye each frame; once the button has let go, the let-go's clearing of where
// it looks stands, frame after frame, so its head turns back ahead and the hold can end, as the original's, whose
// held look is written only while the hand acts on it (docs/wiki/hand/states.md#the-hand-on-a-creature-in-openblack)
TEST_F(CreatureHandSystemTest, LetGoTheHandNoLongerTellsTheCreatureWhereToLook)
{
	const auto creature = PosedApe();
	auto& registry = test::creature_world::World::Registry();
	HoldTo(creature);
	// the hand 7 above the ape's bottom, within 0.6 of its height of it
	_hand->position = glm::vec3(100.0f, 7.0f, 102.0f);
	const glm::vec3 eye {100.0f, 7.0f, 150.0f};
	const glm::vec3 ahead {0.0f, 0.0f, -1.0f};
	ASSERT_TRUE(_system.Update(eye, ahead, glm::vec2(300.0f, 300.0f), 800.0f, 0.1f, Still()).has_value());
	EXPECT_EQ(registry.Get<const CreatureAnimation>(creature).lookAt, std::optional(eye));

	// let go: the hand's lock clears where it looks and marks the hold released
	registry.Get<HandOnCreature>(_hand->left).released = true;
	registry.Get<CreatureAnimation>(creature).lookAt.reset();
	for (int frame = 0; frame < 3; ++frame)
	{
		ASSERT_TRUE(_system.Update(eye, ahead, glm::vec2(300.0f, 300.0f), 800.0f, 0.1f, Still()).has_value());
		EXPECT_FALSE(registry.Get<const CreatureAnimation>(creature).lookAt.has_value()) << "frame " << frame;
	}
}
