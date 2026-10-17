/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature animation system on a registry of its own: the breathing eases once a turn (the fatness shown is eased by
// the body, after the creature's actions), the eyes blink at the turn's end on the local stream, nothing is posed
// without a mesh or a rig, and the body's pose is put in the world for the turn at the turn's end, sampled as it plays then
// without moving anything on. The layers move on once a turn, in the original's order and by the whole milliseconds the
// creature lives at the size it is drawn at; the frame's pose is the sample of what the turn left. The body is drawn
// between its last two turn poses, seen from its drawn matrix, unless it has none or is in the hand. A swaying body leans
// across the way it faces, and a kick leaves a held action, a fight's faint and a turn alone. A body that looks good or
// evil makes its motes or smoke on the local numbers before its new pose. The falling film's copy of the creature starts
// with a step of its time, then plays the falling action on the film's time, and each step draws its sparkles while it
// sparkles, then its blink; it is posed twice standing as it starts, then by each step, and drawn between its last two
// poses.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <array>
#include <chrono>
#include <tuple>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "3D/SkeletalAnimation.h"
#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureAlignmentMotes.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSway.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Components/FallingCreature.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Systems/Implementations/CreatureAnimationSystem.h"
#include "Locator.h"
#include "Magic/Objects/FallingSpellCreature.h"
#include "Resources/ResourcesInterface.h"
#include "creature/CreatureSystemWorld.h"
#include "creature/SyntheticCreatureBlock.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureAnimationSystem;

namespace
{
class CreatureAnimationSystemTest: public ::testing::Test
{
protected:
	test::creature_world::World _world;
	CreatureAnimationSystem _system;
};

/// The ape with a stand and a walk that turns its first bone over a second
class CreatureAnimationSystemRigTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::creature_block::Block block;
		block.clips = {
		    test::creature_block::Clip {},
		    test::creature_block::Clip {.durationMs = 1000, .frames = {{glm::vec3(0.0f)}, {glm::vec3(0.0f, 1.0f, 0.0f)}}}};
		_world.LoadApeRig(block, {{"move", {"Cstand", "Wwalk"}}});
	}

	const test::ScopedDefaultFileSystem _fileSystem;
	test::creature_world::World _world;
	CreatureAnimationSystem _system;
};

/// A rest pose of two bones, the second a metre above the first
const std::vector<glm::mat4> k_TwoBoneRest {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f))};

/// A two bone body at its rest pose, a little way into a breath
CreatureAnimation TwoBoneBody()
{
	CreatureAnimation animation;
	const std::vector<uint32_t> parents {skeletal_animation::k_NoParent, 0};
	animation.skeleton = skeletal_animation::Skeleton::FromRestMatrices(parents, k_TwoBoneRest);
	animation.mirror = skeletal_animation::MirrorJoints(k_TwoBoneRest);
	animation.boneMatrices = k_TwoBoneRest;
	animation.breathPeriod = 5.0f;
	animation.breathPhase = 0.3f;
	return animation;
}

/// A rig of the base mesh alone: a stand that rocks the first bone, and a walk that turns the second over 600 ms
creature::CreatureRig TwoBoneRig()
{
	using skeletal_animation::Animation;
	creature::CreatureRig rig;
	const Animation stand {.duration = 1000,
	                       .looping = true,
	                       .rotatedJoints = {0},
	                       .translatedJoints = {},
	                       .frames = {{.eulerAngles = {glm::vec3(0.0f)}, .translations = {}},
	                                  {.eulerAngles = {glm::vec3(0.2f, 0.0f, 0.0f)}, .translations = {}}}};
	const Animation walk {.duration = 600,
	                      .looping = true,
	                      .rotatedJoints = {1},
	                      .translatedJoints = {},
	                      .frames = {{.eulerAngles = {glm::vec3(0.0f)}, .translations = {}},
	                                 {.eulerAngles = {glm::vec3(0.0f, 0.0f, 0.8f)}, .translations = {}},
	                                 {.eulerAngles = {glm::vec3(0.0f, 0.0f, -0.4f)}, .translations = {}}}};
	rig.animations.front() = {stand, walk};
	return rig;
}

/// The walk, played once from a time
creature_layers::BodyAction Walking(float timeMs)
{
	return {.kind = creature_layers::BodyAction::Kind::Once, .animations = {1, 1, 1}, .timeMs = timeMs};
}

const Transform k_Standing {.position = glm::vec3(10.0f, 0.0f, 20.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)};
} // namespace

TEST(CreatureAnimationSystemPose, TheFramesPoseIsTheSampleOfItsLayersAndMovesNothingOn)
{
	const auto rig = TwoBoneRig();
	for (const bool slotted : {false, true})
	{
		auto framed = TwoBoneBody();
		framed.body = Walking(100.0f);
		framed.lookAt = glm::vec3(20.0f, 3.0f, 30.0f);
		if (slotted)
		{
			framed.slots = {{.animation = 1, .timeMs = 250.0f, .weight = 1.0f, .mirrored = false}};
		}
		const auto before = framed;

		CreatureAnimationSystem::PoseBody(framed, rig, {}, k_Standing);

		// the frame's pose is the sample of the layers as the turn left them, bit for bit, and nothing moves on: the
		// action, the breath and the head move once a turn
		EXPECT_EQ(framed.body.timeMs, 100.0f);
		EXPECT_EQ(framed.breathPhase, before.breathPhase);
		EXPECT_EQ(framed.yaw.angle, before.yaw.angle);
		EXPECT_EQ(framed.pitch.angle, before.pitch.angle);
		auto sampler = before;
		EXPECT_EQ(CreatureAnimationSystem::SampleBody(sampler, rig, {}, before.body, before.slots), framed.boneMatrices);
		EXPECT_NE(framed.boneMatrices, k_TwoBoneRest);
	}
}

namespace
{
/// A rig of the base mesh alone: the stand and walk above, and three clips of their own lengths, 800, 900 and 700 ms
creature::CreatureRig LayeredRig()
{
	auto rig = TwoBoneRig();
	auto& animations = rig.animations.front();
	for (const uint32_t duration : {800u, 900u, 700u})
	{
		auto clip = *animations.at(1);
		clip.duration = duration;
		animations.emplace_back(clip);
	}
	return rig;
}

/// What a turn's advance passed, as (animation, from, to) for each stretch, in order
std::vector<std::tuple<size_t, int32_t, int32_t>> PassedOf(const CreatureAnimationSystem::TurnPassed& passed)
{
	std::vector<std::tuple<size_t, int32_t, int32_t>> stretches;
	auto layers = passed.overlays;
	if (passed.action.has_value())
	{
		layers.push_back(*passed.action);
	}
	for (const auto& layer : layers)
	{
		for (const auto& stretch : layer.advance.Passed())
		{
			stretches.emplace_back(layer.animation, stretch.fromMs, stretch.toMs);
		}
	}
	return stretches;
}

/// A body playing every layer: the face 2 held, the gesture 3 at its start, the wobble 4 near its end, and the walk once
CreatureAnimation PlayingEveryLayer()
{
	auto animation = TwoBoneBody();
	animation.face = creature_layers::FaceLayer {.current = 2, .timeMs = 0.0f, .wanted = 2, .remainingMs = 5000.0f};
	animation.gesture = {.animation = 3, .timeMs = 0.0f};
	animation.wobble = {.animation = 4, .timeMs = 650.0f};
	animation.body = Walking(100.0f);
	return animation;
}
} // namespace

TEST(CreatureAnimationSystemPose, ATurnMovesTheLayersOnInTheOriginalsOrderByItsClock)
{
	const auto rig = LayeredRig();
	auto animation = PlayingEveryLayer();
	// a turn of 100 ms at size 1: the face by 100 ms, the gesture, wobble and action by the step of 117, the breath and
	// head by 0.1 s
	const auto clock = creature_turn_pose::ClockOfTurn(1.0f, 100, 1.0f, false, false);
	ASSERT_EQ(clock.ms, 100);
	ASSERT_EQ(clock.step, 117);

	const auto passed = CreatureAnimationSystem::AdvanceTurnLayers(animation, rig, {}, k_Standing, 1.0f, clock);

	// the face first, then the gesture, the wobble (played to its end) and the action
	const std::vector<std::tuple<size_t, int32_t, int32_t>> expected {
	    {2, 0, 100},
	    {3, 0, 117},
	    {4, 650, 700},
	    {1, 100, 217},
	};
	EXPECT_EQ(PassedOf(passed), expected);
	EXPECT_EQ(animation.face.timeMs, 100.0f);
	EXPECT_EQ(animation.face.remainingMs, 4900.0f);
	EXPECT_EQ(animation.gesture.timeMs, 117.0f);
	EXPECT_FALSE(animation.wobble.animation.has_value());
	// whole milliseconds: the frames' float rate would have played 117.5
	EXPECT_EQ(animation.body.timeMs, 217.0f);
	EXPECT_FLOAT_EQ(animation.breathPhase, creature_animation::AdvanceBreath(0.3f, clock.seconds, 5.0f));
	// nothing is sampled: the turn's pose samples what the advance leaves
	EXPECT_EQ(animation.boneMatrices, k_TwoBoneRest);
}

TEST(CreatureAnimationSystemPose, AFrozenTurnMovesTheLayersOnByTheMillisecondsItLives)
{
	const auto rig = LayeredRig();
	auto animation = PlayingEveryLayer();
	animation.wobble = {};
	// half frozen, it lives 50 ms of the turn: the step at size 1 is 58
	const auto clock = creature_turn_pose::ClockOfTurn(0.5f, 100, 1.0f, false, false);
	ASSERT_EQ(clock.ms, 50);
	ASSERT_EQ(clock.step, 58);

	const auto passed = CreatureAnimationSystem::AdvanceTurnLayers(animation, rig, {}, k_Standing, 1.0f, clock);

	const std::vector<std::tuple<size_t, int32_t, int32_t>> expected {{2, 0, 50}, {3, 0, 58}, {1, 100, 158}};
	EXPECT_EQ(PassedOf(passed), expected);
	EXPECT_EQ(animation.body.timeMs, 158.0f);
}

TEST(CreatureAnimationSystemPose, ATurnMovesNothingOnWithoutTheSpeciesStand)
{
	auto animation = PlayingEveryLayer();
	const auto before = animation;
	const auto clock = creature_turn_pose::ClockOfTurn(1.0f, 100, 1.0f, false, false);

	const auto passed =
	    CreatureAnimationSystem::AdvanceTurnLayers(animation, creature::CreatureRig {}, {}, k_Standing, 1.0f, clock);
	EXPECT_TRUE(passed.overlays.empty());
	EXPECT_FALSE(passed.action.has_value());
	EXPECT_EQ(animation.body.timeMs, before.body.timeMs);
	EXPECT_EQ(animation.face.timeMs, before.face.timeMs);
	EXPECT_EQ(animation.gesture.timeMs, before.gesture.timeMs);
	EXPECT_EQ(animation.breathPhase, before.breathPhase);
}

TEST(CreatureAnimationSystemPose, AFramesClockIsItsTimeAndItsTimeAtTheDrawnSizesRate)
{
	for (const float drawnSize : {0.5f, 1.0f, 2.0f})
	{
		const auto clock = CreatureAnimationSystem::FrameClock(drawnSize, 33.0f, 0.033f);
		EXPECT_EQ(clock.milliseconds, 33.0f);
		EXPECT_EQ(clock.seconds, 0.033f);
		EXPECT_EQ(clock.playbackMs, 33.0f * creature_layers::PlaybackRate(drawnSize));
	}
}

TEST(CreatureAnimationSystemPose, SamplingMovesNothingOnAndPlaysWhatItIsGiven)
{
	const auto rig = TwoBoneRig();
	auto animation = TwoBoneBody();
	animation.body = Walking(100.0f);
	const std::vector<CreatureAnimation::Slot> walk {{.animation = 1, .timeMs = 250.0f, .weight = 1.0f, .mirrored = false}};

	const auto first = CreatureAnimationSystem::SampleBody(animation, rig, {}, animation.body, walk);
	const auto again = CreatureAnimationSystem::SampleBody(animation, rig, {}, animation.body, walk);

	EXPECT_EQ(first, again);
	EXPECT_EQ(animation.body.timeMs, 100.0f);
	EXPECT_EQ(animation.breathPhase, 0.3f);
	EXPECT_TRUE(animation.slots.empty());
	EXPECT_EQ(animation.boneMatrices, k_TwoBoneRest);
	// slots play in place of the body's action, as the frame's do; another time is another pose
	EXPECT_NE(CreatureAnimationSystem::SampleBody(animation, rig, {}, animation.body, {}), first);
	EXPECT_NE(CreatureAnimationSystem::SampleBody(animation, rig, {}, Walking(400.0f), {}),
	          CreatureAnimationSystem::SampleBody(animation, rig, {}, animation.body, {}));
	// without a stand there is nothing to sample
	auto bare = TwoBoneBody();
	EXPECT_TRUE(CreatureAnimationSystem::SampleBody(bare, creature::CreatureRig {}, {}, bare.body, walk).empty());
}

TEST(CreatureAnimationSystemSway, AKickLeavesAHeldActionAFightsFaintAndATurnAlone)
{
	using creature_fight::State;
	using Kind = creature_layers::BodyAction::Kind;
	using Phase = creature_layers::BodyAction::Phase;
	const creature_layers::BodyAction standing {};
	const creature_layers::BodyAction once {.kind = Kind::Once, .animations = {40, 40, 40}};
	// standing, walking, an action played once and a fight's moves take a kick
	EXPECT_FALSE(CreatureAnimationSystem::IgnoresKick(standing, std::nullopt, false));
	EXPECT_FALSE(CreatureAnimationSystem::IgnoresKick(once, std::nullopt, false));
	const creature_layers::BodyAction move {
	    .kind = Kind::Sequence, .phase = Phase::Loop, .animations = {150, 150, 150}, .timedByPlayer = true};
	for (const auto state : {State::Start, State::Stance, State::Action, State::Block, State::Cast})
	{
		EXPECT_FALSE(CreatureAnimationSystem::IgnoresKick(move, state, false)) << creature_fight::Name(state);
	}
	// a held action, in any of its parts: sitting, a cast, fainting where it stands
	for (const auto phase : {Phase::Start, Phase::Loop, Phase::End})
	{
		const creature_layers::BodyAction sitting {.kind = Kind::Sequence, .phase = phase, .animations = {37, 38, 39}};
		EXPECT_TRUE(CreatureAnimationSystem::IgnoresKick(sitting, std::nullopt, false));
	}
	// fainting, lying fainted and getting up in a fight, whatever the body shows
	for (const auto state : {State::Faint, State::Lying, State::GetUp})
	{
		EXPECT_TRUE(CreatureAnimationSystem::IgnoresKick(move, state, false)) << creature_fight::Name(state);
		EXPECT_TRUE(CreatureAnimationSystem::IgnoresKick(standing, state, false)) << creature_fight::Name(state);
	}
	// turning on the spot, or stepping off into a walk
	EXPECT_TRUE(CreatureAnimationSystem::IgnoresKick(standing, std::nullopt, true));
}

TEST(CreatureAnimationSystemSway, ASwayingBodyLeansAcrossTheWayItFaces)
{
	using skeletal_animation::Animation;
	auto rig = TwoBoneRig();
	// the lower body's sideways lean turns the second bone from one side, through upright, to the other
	auto& animations = rig.animations.front();
	animations.resize(creature_sway::k_LowerFrontBack + 1);
	animations.at(creature_sway::k_LowerSide) =
	    Animation {.duration = 1000,
	               .looping = false,
	               .rotatedJoints = {1},
	               .translatedJoints = {},
	               .frames = {{.eulerAngles = {glm::vec3(-0.5f, 0.0f, 0.0f)}, .translations = {}},
	                          {.eulerAngles = {glm::vec3(0.0f)}, .translations = {}},
	                          {.eulerAngles = {glm::vec3(0.5f, 0.0f, 0.0f)}, .translations = {}}}};
	auto body = TwoBoneBody();
	const auto settled = CreatureAnimationSystem::SampleBody(body, rig, {}, body.body, {});

	// an offset that has settled is not drawn
	body.sway.lowerOffset = glm::vec3(0.5f, 0.0f, 0.0f);
	EXPECT_EQ(CreatureAnimationSystem::SampleBody(body, rig, {}, body.body, {}), settled);

	// swaying, the offset along the body's side leans it sideways, relative to the lean's middle frame
	body.sway.active = true;
	const auto leaning = CreatureAnimationSystem::SampleBody(body, rig, {}, body.body, {});
	EXPECT_NE(leaning, settled);
	EXPECT_EQ(leaning.front(), settled.front());

	// turned a quarter round, the same offset is along its front, which this body has no lean for
	const glm::mat3 quarter {glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f)};
	EXPECT_EQ(CreatureAnimationSystem::SampleBody(body, rig, {}, body.body, {}, quarter), settled);

	// a lean too small to see is not drawn
	body.sway.lowerOffset = glm::vec3(creature_sway::k_DrawnLean, 0.0f, 0.0f);
	EXPECT_EQ(CreatureAnimationSystem::SampleBody(body, rig, {}, body.body, {}), settled);
}

TEST_F(CreatureAnimationSystemTest, ProcessTurnEasesTheBreathingButNotTheFatnessShown)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<Creature>(creature).fatness = 1.0f;
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.breathPeriod = 1.0f;
	const auto shown = registry.Get<CreatureMorph>(creature).shownFatness;
	const auto size = registry.Get<Creature>(creature).size;

	_system.ProcessTurn();

	EXPECT_FLOAT_EQ(registry.Get<CreatureMorph>(creature).shownFatness, shown);
	const auto resting = creature_animation::BreathPeriod(size);
	EXPECT_FLOAT_EQ(registry.Get<CreatureAnimation>(creature).breathPeriod,
	                creature_animation::EaseBreathPeriod(1.0f, resting, resting, 0.1f));
}

TEST_F(CreatureAnimationSystemTest, InItsPenItBreathesAtThePenSizesPeriod)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<Creature>(creature).size = 2.0f;
	registry.Get<CreatureDrawPose>(creature).size = 0.22f;
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.breathPeriod = 1.0f;

	_system.ProcessTurn();

	// 5 x sqrt(0.22), not its own size's 5 x sqrt(2)
	const auto resting = creature_animation::BreathPeriod(0.22f);
	EXPECT_FLOAT_EQ(resting, 5.0f * std::sqrt(0.22f));
	EXPECT_FLOAT_EQ(registry.Get<CreatureAnimation>(creature).breathPeriod,
	                creature_animation::EaseBreathPeriod(1.0f, resting, resting, 0.1f));
	EXPECT_EQ(registry.Get<Creature>(creature).size, 2.0f);
}

TEST_F(CreatureAnimationSystemTest, TheEyesBlinkAtTheTurnsEndOnTheLocalStream)
{
	const game_random::testing::ScopedState state;
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	const auto seeds = game_random::Current();
	const auto crt = game_random::crt::Seed();

	// the turn's start leaves the eyes as they are
	_system.ProcessTurn();
	EXPECT_EQ(registry.Get<CreatureEyes>(creature).blink.timerMs, creature_eyes::k_FirstBlinkMs);
	// the first blink comes after a second: ten turns' ends of 100 ms
	for (int turn = 0; turn < 10; ++turn)
	{
		_system.PoseTurn({});
	}
	EXPECT_EQ(registry.Get<CreatureEyes>(creature).blink.state, creature_eyes::Blink::State::Closing);
	EXPECT_EQ(game_random::Current().local, seeds.local);
	// closed for two turns, opening for two, then open again for the wait drawn from the local stream: the original's
	// first draw of 5000 on a new campaign's seed is 1878
	for (int turn = 0; turn < 4; ++turn)
	{
		_system.PoseTurn({});
	}
	const auto& blink = registry.Get<CreatureEyes>(creature).blink;
	EXPECT_EQ(blink.state, creature_eyes::Blink::State::Open);
	EXPECT_EQ(blink.timerMs, 1878 + 2500);
	EXPECT_EQ(game_random::Current().local, 0xE6F4C8D6u);
	// neither the synced stream nor the C runtime's is drawn from
	EXPECT_EQ(game_random::Current().synced, seeds.synced);
	EXPECT_EQ(game_random::crt::Seed(), crt);
}

TEST_F(CreatureAnimationSystemTest, AFrozenCreatureBlinksByTheTimeItLives)
{
	const game_random::testing::ScopedState state;
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	// at half speed it lives 50 ms of each turn: half way to its first blink after ten turns
	registry.Get<CreatureAnimation>(creature).playbackScale = 0.5f;
	for (int turn = 0; turn < 10; ++turn)
	{
		_system.PoseTurn({});
	}
	const auto& blink = registry.Get<CreatureEyes>(creature).blink;
	EXPECT_EQ(blink.state, creature_eyes::Blink::State::Open);
	EXPECT_EQ(blink.timerMs, creature_eyes::k_FirstBlinkMs - 500);
}

TEST_F(CreatureAnimationSystemTest, UpdateWithoutAMeshPosesNothing)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	const auto before = registry.Get<CreatureMorph>(creature).revision;

	_system.Update(0.5f, std::chrono::duration<float, std::milli>(33.0f));

	const auto& animation = registry.Get<CreatureAnimation>(creature);
	EXPECT_TRUE(animation.skeleton.Empty());
	EXPECT_TRUE(animation.boneMatrices.empty());
	EXPECT_EQ(registry.Get<CreatureMorph>(creature).revision, before);
}

TEST_F(CreatureAnimationSystemTest, TheRestPoseIsBuiltOncePerChangeOfTheBody)
{
	CreatureAnimation animation;
	// none yet
	EXPECT_TRUE(CreatureAnimationSystem::RestIsStale(animation, 0));
	// built for this body
	animation.skeleton.parents = {0};
	animation.builtRevision = 3;
	EXPECT_FALSE(CreatureAnimationSystem::RestIsStale(animation, 3));
	// the body has changed since
	EXPECT_TRUE(CreatureAnimationSystem::RestIsStale(animation, 4));
}

TEST_F(CreatureAnimationSystemTest, PosingOnDemandWithoutAMeshBuildsNothing)
{
	const auto creature = test::creature_world::World::MakeCreature();
	EXPECT_FALSE(_system.EnsurePosed(creature));
	EXPECT_TRUE(std::as_const(test::creature_world::World::Registry()).Get<const CreatureAnimation>(creature).skeleton.Empty());
	EXPECT_FALSE(_system.EnsurePosed(entt::null));
}

TEST_F(CreatureAnimationSystemTest, TheDrawnShapeTakesTheAttributesOnceATurnNotEachFrame)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	auto& morph = registry.Get<CreatureMorph>(creature);
	const auto drawn = morph.drawn;
	const auto revision = morph.revision;
	// the alignment moves to the other end: the frames draw the shape as it was
	auto& body = registry.Get<Creature>(creature);
	body.alignment = drawn.evilGood > 0.0f ? -1.0f : 1.0f;
	_system.Update(0.5f, std::chrono::duration<float, std::milli>(33.0f));
	EXPECT_EQ(morph.drawn.evilGood, drawn.evilGood);
	EXPECT_EQ(morph.revision, revision);
	// the turn's pose takes it, and the animations are rebuilt for it
	_system.PoseTurn({});
	EXPECT_EQ(morph.drawn.evilGood, body.alignment);
	EXPECT_EQ(morph.revision, revision + 1);
}

TEST_F(CreatureAnimationSystemTest, AShapeChangeUnderTheThresholdIsDrawnAsItWas)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	_system.PoseTurn({});
	auto& morph = registry.Get<CreatureMorph>(creature);
	const auto drawn = morph.drawn;
	const auto revision = morph.revision;
	// 0.02 is less than the 0.03 a shape axis waits for
	auto& body = registry.Get<Creature>(creature);
	body.alignment = drawn.evilGood > 0.0f ? drawn.evilGood - 0.02f : drawn.evilGood + 0.02f;
	_system.PoseTurn({});
	EXPECT_EQ(morph.drawn.evilGood, drawn.evilGood);
	EXPECT_EQ(morph.revision, revision);
}

TEST_F(CreatureAnimationSystemTest, NoRigMeansNoBoneOrDuration)
{
	const auto creature = test::creature_world::World::MakeCreature();
	EXPECT_FALSE(_system.AnimationDuration(creature, 0).has_value());
	EXPECT_FALSE(_system.BoneInAnimation(creature, 0, 0.0f, 0, false).has_value());
	// nor for something that is no creature
	const auto thing = test::creature_world::World::Registry().Create();
	EXPECT_FALSE(_system.AnimationDuration(thing, 0).has_value());
}

TEST_F(CreatureAnimationSystemTest, ABodyNotPosedYetHasNoTurnPose)
{
	const auto creature = test::creature_world::World::MakeCreature();
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	ASSERT_TRUE(registry.Get<const CreatureAnimation>(creature).boneMatrices.empty());

	_system.PoseTurn({});

	EXPECT_EQ(registry.TryGet<const CreatureTurnPose>(creature), nullptr);
}

TEST_F(CreatureAnimationSystemTest, TheTurnPoseIsTheBodysPoseInTheWorld)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	auto& animation = registry.Get<CreatureAnimation>(creature);
	const std::vector<glm::mat4> first {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f))};
	animation.boneMatrices = first;
	animation.body.timeMs = 250.0f;
	// in its pen, at the scale it is drawn at there
	registry.Get<CreatureDrawPose>(creature).scale = glm::vec3(0.5f);
	const auto& transform = registry.Get<const Transform>(creature);
	const auto world = affine::Model(transform.position, transform.rotation, glm::vec3(0.5f));

	_system.PoseTurn({});

	const auto* pose = std::as_const(registry).TryGet<const CreatureTurnPose>(creature);
	ASSERT_NE(pose, nullptr);
	EXPECT_EQ(pose->current, creature_turn_pose::InWorld(first, world));
	EXPECT_EQ(pose->previous, pose->current);
	EXPECT_FALSE(pose->frozen.has_value());
	// the pose is read, the animation's clock not moved
	EXPECT_EQ(registry.Get<CreatureAnimation>(creature).body.timeMs, 250.0f);
	EXPECT_EQ(registry.Get<CreatureAnimation>(creature).boneMatrices, first);
}

TEST_F(CreatureAnimationSystemTest, EachPoseRequestMovesTheLastOneToThePrevious)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	auto& animation = registry.Get<CreatureAnimation>(creature);
	const auto world = affine::Model(registry.Get<const Transform>(creature));
	const std::vector<glm::mat4> first {glm::mat4(1.0f)};
	const std::vector<glm::mat4> second {glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f))};
	animation.boneMatrices = first;
	_system.PoseTurn({});
	registry.Get<CreatureAnimation>(creature).boneMatrices = second;
	_system.PoseTurn({});

	const auto& pose = registry.Get<CreatureTurnPose>(creature);
	EXPECT_EQ(pose.previous, creature_turn_pose::InWorld(first, world));
	EXPECT_EQ(pose.current, creature_turn_pose::InWorld(second, world));
}

TEST_F(CreatureAnimationSystemTest, ACreatureThatLooksGoodOrEvilMakesItsMotesFromThePoseBefore)
{
	const game_random::testing::ScopedState state;
	// No physics: the puffs are dropped (the physics' pool would load the dust's texture from files the test lacks)
	const test::RestoreService<Locator::dynamicsSystem> restoreDynamics;
	Locator::dynamicsSystem::reset();
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	auto& body = registry.Get<Creature>(creature);
	body.alignment = 0.5f;
	registry.Get<CreatureAnimation>(creature).boneMatrices = k_TwoBoneRest;
	const auto start = game_random::Current().local;
	// the creature's making draws on the synced numbers; nothing after it does
	const auto synced = game_random::Current().synced;
	// not posed yet: there is no pose before this one to put motes on
	_system.PoseTurn({});
	EXPECT_EQ(game_random::Current().local, start);
	// near neutral: none
	body.alignment = 0.1f;
	_system.PoseTurn({});
	EXPECT_EQ(game_random::Current().local, start);
	// good, then evil: three motes or puffs a turn, on the local numbers, one bone of the two drawn for each
	const auto expectDraws = [&](float alignment) {
		body.alignment = alignment;
		uint32_t seed = game_random::Current().local;
		const creature_alignment_motes::LocalRandom random = [&seed](int32_t n) {
			return n == 0 ? 0u : game_random::SeededRandom(static_cast<uint32_t>(n), seed);
		};
		const auto size = ecs::creature_pose::DrawnSize(std::as_const(registry), creature);
		const auto puffs = creature_alignment_motes::PuffsOf(
		    alignment, size, 2, [](uint32_t) { return glm::vec3(0.0f); }, random);
		EXPECT_EQ(puffs.size(), 3u);
		_system.PoseTurn({});
		EXPECT_EQ(game_random::Current().local, seed);
	};
	expectDraws(0.5f);
	expectDraws(-0.5f);
	// the synced stream is not drawn from
	EXPECT_EQ(game_random::Current().synced, synced);
}

TEST_F(CreatureAnimationSystemTest, TheFrameAndTheTurnLeaveTheTurnPoseAlone)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<CreatureAnimation>(creature).boneMatrices = {glm::mat4(2.0f)};
	_system.PoseTurn({});
	const auto posed = registry.Get<CreatureTurnPose>(creature).current;
	registry.Get<CreatureAnimation>(creature).boneMatrices = {glm::mat4(3.0f)};

	_system.Update(0.5f, std::chrono::duration<float, std::milli>(33.0f));
	_system.ProcessTurn();

	// a reader before the next pose request still sees the last one
	EXPECT_EQ(registry.Get<CreatureTurnPose>(creature).current, posed);
	EXPECT_EQ(registry.Get<CreatureTurnPose>(creature).previous, posed);
}

namespace
{
/// Two turn poses of two bones a metre apart, the second walked two metres on and its top bone bent
CreatureTurnPose TwoTurns()
{
	CreatureTurnPose pose;
	creature_turn_pose::Advance(pose, creature_turn_pose::InWorld(k_TwoBoneRest, affine::Model(k_Standing)));
	auto bent = k_TwoBoneRest;
	bent[1] = glm::rotate(bent[1], 0.6f, glm::vec3(0.0f, 0.0f, 1.0f));
	const Transform on {.position = glm::vec3(12.0f, 0.0f, 20.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)};
	creature_turn_pose::Advance(pose, creature_turn_pose::InWorld(bent, affine::Model(on)));
	return pose;
}
} // namespace

TEST_F(CreatureAnimationSystemTest, TheDrawnBonesAreTheTurnPosesBlendSeenFromTheDrawnMatrix)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	const auto& pose = registry.Assign<CreatureTurnPose>(creature, TwoTurns());
	const auto& lookup = std::as_const(registry);

	const auto drawn = CreatureAnimationSystem::DrawnBones(lookup, creature, 2, 0.25f);

	EXPECT_EQ(drawn,
	          creature_turn_pose::RelativeTo(ecs::DrawnBodyModel(lookup, creature), creature_turn_pose::Drawn(pose, 0.25f)));
	// drawn by the body's row, the body is a quarter of the way from the turn before to the last
	const auto row = ecs::DrawnBodyModel(lookup, creature);
	ASSERT_EQ(drawn.size(), 2u);
	EXPECT_NEAR((row * drawn[0])[3].x, 10.5f, 1e-4f);
	EXPECT_NE(CreatureAnimationSystem::DrawnBones(lookup, creature, 2, 0.75f), drawn);
}

TEST_F(CreatureAnimationSystemTest, NoDrawnBonesWithoutATurnPoseThatFitsTheMesh)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	const auto& lookup = std::as_const(registry);
	// not posed for a turn yet
	EXPECT_TRUE(CreatureAnimationSystem::DrawnBones(lookup, creature, 2, 0.5f).empty());
	registry.Assign<CreatureTurnPose>(creature);
	EXPECT_TRUE(CreatureAnimationSystem::DrawnBones(lookup, creature, 2, 0.5f).empty());
	// posed with another number of bones than the mesh has, now or the turn before
	auto& pose = registry.Get<CreatureTurnPose>(creature);
	pose = TwoTurns();
	EXPECT_TRUE(CreatureAnimationSystem::DrawnBones(lookup, creature, 3, 0.5f).empty());
	pose.previous.pop_back();
	EXPECT_TRUE(CreatureAnimationSystem::DrawnBones(lookup, creature, 2, 0.5f).empty());
}

TEST_F(CreatureAnimationSystemTest, InTheHandOrInFlightTheBodyIsDrawnAsTheFramePosesIt)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	registry.Assign<CreatureTurnPose>(creature, TwoTurns());
	const auto& lookup = std::as_const(registry);
	ASSERT_FALSE(CreatureAnimationSystem::DrawnBones(lookup, creature, 2, 0.5f).empty());

	registry.Assign<HandDrawPose>(creature);
	EXPECT_TRUE(CreatureAnimationSystem::DrawnBones(lookup, creature, 2, 0.5f).empty());
	registry.Remove<HandDrawPose>(creature);
	registry.Assign<PhysicsDrawPose>(creature);
	EXPECT_TRUE(CreatureAnimationSystem::DrawnBones(lookup, creature, 2, 0.5f).empty());
}

TEST_F(CreatureAnimationSystemTest, TheDrawingReadsTheLiveTurnPosesNotTheCopyTheHandKeeps)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	auto& pose = registry.Assign<CreatureTurnPose>(creature, TwoTurns());
	const auto& lookup = std::as_const(registry);
	const auto live = CreatureAnimationSystem::DrawnBones(lookup, creature, 2, 0.5f);

	pose.frozen = creature_turn_pose::InWorld(k_TwoBoneRest, glm::mat4(1.0f));

	EXPECT_EQ(CreatureAnimationSystem::DrawnBones(lookup, creature, 2, 0.5f), live);
}

TEST_F(CreatureAnimationSystemTest, TheDrawnBlendInTheWorldIsTheTurnPosesBlendTheDrawnBonesAreSeenFrom)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	const auto& pose = registry.Assign<CreatureTurnPose>(creature, TwoTurns());
	const auto& lookup = std::as_const(registry);

	const auto world = CreatureAnimationSystem::DrawnInWorld(lookup, creature, 2, 0.25f);

	EXPECT_EQ(world, creature_turn_pose::Drawn(pose, 0.25f));
	EXPECT_EQ(CreatureAnimationSystem::DrawnBones(lookup, creature, 2, 0.25f),
	          creature_turn_pose::RelativeTo(ecs::DrawnBodyModel(lookup, creature), world));
	// none where the drawn bones are none
	EXPECT_TRUE(CreatureAnimationSystem::DrawnInWorld(lookup, creature, 3, 0.25f).empty());
	registry.Assign<HandDrawPose>(creature);
	EXPECT_TRUE(CreatureAnimationSystem::DrawnInWorld(lookup, creature, 2, 0.25f).empty());
}

TEST_F(CreatureAnimationSystemTest, AFrameWithoutAMeshLeavesNoDrawnBones)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	registry.Assign<CreatureTurnPose>(creature, TwoTurns());

	_system.Update(0.5f, std::chrono::duration<float, std::milli>(33.0f));

	EXPECT_TRUE(registry.Get<CreatureAnimation>(creature).drawnBones.empty());
	EXPECT_TRUE(registry.Get<CreatureAnimation>(creature).drawnInWorld.empty());
}

TEST_F(CreatureAnimationSystemRigTest, TheTurnPoseIsSampledAsItPlaysAtTheTurnsEndWithoutMovingItOn)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	{
		auto& animation = registry.Get<CreatureAnimation>(creature);
		const auto posed = TwoBoneBody();
		animation.skeleton = posed.skeleton;
		animation.mirror = posed.mirror;
		animation.boneMatrices = posed.boneMatrices;
		// the last frame drew the walk at its start
		animation.slots = {{.animation = 1, .timeMs = 0.0f, .weight = 1.0f, .mirrored = false}};
		animation.body.timeMs = 40.0f;
	}
	// the turn's end plays it half way through
	const CreatureAnimationSystem::TurnInputs inputs {
	    {creature, {.body = {}, .slots = {{.animation = 1, .timeMs = 500.0f, .weight = 1.0f, .mirrored = false}}}}};

	_system.PoseTurn(inputs);

	const auto& lookup = std::as_const(registry);
	const auto* pose = lookup.TryGet<const CreatureTurnPose>(creature);
	ASSERT_NE(pose, nullptr);
	auto sampler = lookup.Get<const CreatureAnimation>(creature);
	const auto& rig = *Locator::resources::value().GetCreatureRigs().Handle(creature::GetRigId(CreatureType::GiantApe));
	const auto& drawn = lookup.Get<const CreatureMorph>(creature).drawn;
	const auto& atEnd = inputs.front().second;
	const auto sampled = CreatureAnimationSystem::SampleBody(sampler, rig, drawn, atEnd.body, atEnd.slots);
	const auto& transform = lookup.Get<const Transform>(creature);
	const auto world = affine::Model(transform.position, transform.rotation, ecs::creature_pose::DrawnScale(lookup, creature));
	EXPECT_EQ(pose->current, creature_turn_pose::InWorld(sampled, world));
	// not the pose the frames last drew, nor what the body played before the turn's end
	const auto before = CreatureAnimationSystem::SampleBody(sampler, rig, drawn, sampler.body, sampler.slots);
	EXPECT_NE(sampled, k_TwoBoneRest);
	EXPECT_NE(sampled, before);
	// nothing is moved on: the frames' pose and what they play stay as they were
	const auto& animation = lookup.Get<const CreatureAnimation>(creature);
	EXPECT_EQ(animation.boneMatrices, k_TwoBoneRest);
	EXPECT_EQ(animation.body.timeMs, 40.0f);
	ASSERT_EQ(animation.slots.size(), 1u);
	EXPECT_EQ(animation.slots.front().timeMs, 0.0f);

	// a creature given nothing is sampled as it plays now
	const auto first = pose->current;
	_system.PoseTurn({});
	EXPECT_EQ(lookup.Get<const CreatureTurnPose>(creature).current, creature_turn_pose::InWorld(before, world));
	EXPECT_EQ(lookup.Get<const CreatureTurnPose>(creature).previous, first);
}

namespace
{
/// The two bone rig, with its walk as the falling action too
creature::CreatureRig FallingRig()
{
	auto rig = TwoBoneRig();
	auto& animations = rig.animations.front();
	const auto walk = animations.at(1);
	animations.resize(static_cast<size_t>(magic::falling_spell::k_CreatureAction) + 1);
	animations.at(static_cast<size_t>(magic::falling_spell::k_CreatureAction)) = walk;
	return rig;
}

/// The ranges of the local draws a step makes, each drawn as 0
struct DrawLog
{
	std::vector<int32_t> ranges;
	[[nodiscard]] CreatureAnimationSystem::LocalRandom Random()
	{
		return [this](int32_t range) {
			ranges.push_back(range);
			return 0u;
		};
	}
};
} // namespace

TEST(CreatureAnimationSystemFallingCopy, ItStartsWithAStepThenItsLookResetAndTheFallingActionOnTheFilmsTime)
{
	const auto rig = FallingRig();
	auto animation = TwoBoneBody();
	animation.lookAt = glm::vec3(1.0f, 2.0f, 3.0f);
	animation.yaw.angle = 0.5f;
	animation.pitch.velocity = 0.25f;
	animation.face.current = creature_layers::animations::k_FirstFace;
	CreatureEyes eyes;
	eyes.lookAt = glm::vec3(1.0f, 2.0f, 3.0f);
	FallingCreature copy {.species = CreatureType::GiantApe, .size = 1.0f, .alignment = 0.0f};
	DrawLog log;

	CreatureAnimationSystem::FallingCopyStart(animation, &eyes, &rig, {}, k_Standing, copy, log.Random());

	// its first step breathes a millisecond; with an alignment of 0 and its eyes long before a blink, it draws nothing
	EXPECT_EQ(animation.breathPhase, creature_animation::AdvanceBreath(0.3f, 0.001f, 5.0f));
	EXPECT_TRUE(log.ranges.empty());
	// then its time is the film's, it looks nowhere with its head straight and still, and its face is relaxed
	EXPECT_TRUE(copy.filmTimed);
	EXPECT_FALSE(animation.lookAt.has_value());
	EXPECT_FALSE(eyes.lookAt.has_value());
	EXPECT_EQ(animation.yaw.angle, 0.0f);
	EXPECT_EQ(animation.pitch.velocity, 0.0f);
	EXPECT_FALSE(animation.face.current.has_value());
	// and it plays the falling action once, from its start, not mirrored
	const auto action = static_cast<size_t>(magic::falling_spell::k_CreatureAction);
	EXPECT_EQ(animation.body.kind, creature_layers::BodyAction::Kind::Once);
	EXPECT_EQ(creature_layers::CurrentAnimation(animation.body), action);
	EXPECT_EQ(animation.body.timeMs, 0.0f);
	EXPECT_FALSE(animation.body.mirrored);

	// each step of the film's time moves the action on by the film's milliseconds, whatever its size
	CreatureAnimationSystem::FallingCopyStep(animation, &eyes, &rig, {}, k_Standing, copy, 40, log.Random());
	EXPECT_EQ(animation.body.timeMs, 40.0f);
}

TEST(CreatureAnimationSystemFallingCopy, ASpeciesWithoutTheFallingActionStands)
{
	const auto rig = TwoBoneRig();
	auto animation = TwoBoneBody();
	FallingCreature copy {.species = CreatureType::GiantApe, .size = 1.0f, .alignment = 0.0f};
	DrawLog log;

	CreatureAnimationSystem::FallingCopyStart(animation, nullptr, &rig, {}, k_Standing, copy, log.Random());

	EXPECT_TRUE(copy.filmTimed);
	EXPECT_FALSE(creature_layers::IsPlaying(animation.body));
}

TEST(CreatureAnimationSystemFallingCopy, AStepDrawsItsSparklesWhileItSparklesThenItsBlink)
{
	const auto rig = FallingRig();
	FallingCreature copy {.species = CreatureType::GiantApe, .size = 1.0f, .alignment = 0.5f, .filmTimed = true};
	for (const bool sparkling : {true, false})
	{
		copy.sparkling = sparkling;
		auto animation = TwoBoneBody();
		CreatureEyes eyes;
		// its eyes opening, a millisecond from open
		eyes.blink.state = creature_eyes::Blink::State::Opening;
		eyes.blink.timerMs = 1;
		DrawLog log;

		CreatureAnimationSystem::FallingCopyStep(animation, &eyes, &rig, {}, k_Standing, copy, 40, log.Random());

		// a good body's three sparkles: a place, a bone of its two and a colour, and a kind; then the next blink's wait
		std::vector<int32_t> expected;
		for (int i = 0; sparkling && i < 3; ++i)
		{
			expected.insert(expected.end(), {101, 101, 101, 2, 6, 2});
		}
		expected.push_back(eyes.blink.intervalMs);
		EXPECT_EQ(log.ranges, expected);
		EXPECT_EQ(eyes.blink.state, creature_eyes::Blink::State::Open);
	}
}

TEST(CreatureAnimationSystemFallingCopy, ItIsPosedStandingTwiceAtItsStartThenByEachStepAndDrawnBetweenItsLastTwo)
{
	const auto rig = FallingRig();
	auto animation = TwoBoneBody();
	FallingCreature copy {.species = CreatureType::GiantApe, .size = 1.0f, .alignment = 0.0f};
	DrawLog log;
	CreatureTurnPose poses;
	glm::mat4 model(1.0f);
	model[3] = glm::vec4(k_Standing.position, 1.0f);

	CreatureAnimationSystem::FallingCopyStart(animation, nullptr, &rig, {}, k_Standing, copy, log.Random(), &poses, model);

	// posed by its first step and again by the reset, both standing: the falling action shows from the next step's pose
	auto standing = TwoBoneBody();
	standing.breathPhase = animation.breathPhase;
	const auto standingBones = CreatureAnimationSystem::SampleBody(standing, rig, {}, {}, {}, k_Standing.rotation);
	ASSERT_FALSE(standingBones.empty());
	EXPECT_EQ(animation.boneMatrices, standingBones);
	EXPECT_EQ(poses.current, creature_turn_pose::InWorld(standingBones, model));
	EXPECT_EQ(poses.previous, poses.current);
	EXPECT_EQ(copy.lastStepMs, static_cast<uint32_t>(magic::falling_spell::k_CreatureFirstStepMs));

	// a step of the film's time poses it playing the action; the pose before becomes the one before
	const auto before = poses.current;
	CreatureAnimationSystem::FallingCopyStep(animation, nullptr, &rig, {}, k_Standing, copy, 40, log.Random(), &poses, model);
	EXPECT_EQ(copy.lastStepMs, 40u);
	EXPECT_EQ(poses.previous, before);
	EXPECT_EQ(poses.current, creature_turn_pose::InWorld(animation.boneMatrices, model));
	EXPECT_NE(poses.current, before);

	// drawn between the two, the share of the way from the one before, seen from its drawn matrix
	const auto boneCount = animation.boneMatrices.size();
	EXPECT_EQ(CreatureAnimationSystem::FallingCopyDrawnBones(poses, model, boneCount, 0.37f),
	          creature_turn_pose::RelativeTo(model, creature_turn_pose::Drawn(poses, 0.37f)));
	// all the way from the one before is the standing pose, all the way to the last is the step's
	const std::array<std::pair<float, const std::vector<glm::mat4>*>, 2> ends {
	    {{0.0f, &standingBones}, {1.0f, &animation.boneMatrices}}};
	for (const auto& [share, expected] : ends)
	{
		const auto drawn = CreatureAnimationSystem::FallingCopyDrawnBones(poses, model, boneCount, share);
		ASSERT_EQ(drawn.size(), boneCount);
		for (size_t bone = 0; bone < boneCount; ++bone)
		{
			for (int column = 0; column < 4; ++column)
			{
				for (int row = 0; row < 4; ++row)
				{
					EXPECT_NEAR(drawn.at(bone)[column][row], expected->at(bone)[column][row], 1e-5f);
				}
			}
		}
	}
	// none without a pose, or with a pose of another count of bones: it is drawn as last posed
	EXPECT_TRUE(CreatureAnimationSystem::FallingCopyDrawnBones(CreatureTurnPose {}, model, boneCount, 0.5f).empty());
	EXPECT_TRUE(CreatureAnimationSystem::FallingCopyDrawnBones(poses, model, boneCount + 1, 0.5f).empty());
}

TEST(CreatureAnimationSystemFallingCopy, WithoutPosesItsStepsAndStartLeaveItsBonesAlone)
{
	const auto rig = FallingRig();
	auto animation = TwoBoneBody();
	FallingCreature copy {.species = CreatureType::GiantApe, .size = 1.0f, .alignment = 0.0f};
	DrawLog log;

	CreatureAnimationSystem::FallingCopyStart(animation, nullptr, &rig, {}, k_Standing, copy, log.Random());
	CreatureAnimationSystem::FallingCopyStep(animation, nullptr, &rig, {}, k_Standing, copy, 40, log.Random());

	EXPECT_EQ(animation.boneMatrices, TwoBoneBody().boneMatrices);
}

TEST_F(CreatureAnimationSystemRigTest, TheTurnsAdvanceMovesEachBodyOnByTheMillisecondsItLivesAtTheSizeItIsDrawnAt)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<Creature>(creature).size = 1.0f;
	registry.Get<CreatureDrawPose>(creature).size = 1.0f;
	auto& animation = registry.Get<CreatureAnimation>(creature);
	const auto posed = TwoBoneBody();
	animation.skeleton = posed.skeleton;
	animation.mirror = posed.mirror;
	animation.boneMatrices = posed.boneMatrices;
	animation.builtRevision = registry.Get<CreatureMorph>(creature).revision;
	animation.breathPeriod = 5.0f;
	animation.body = Walking(40.0f);

	// a turn of 100 ms at size 1 steps the walk by 117 ms, and gives the stretch it passed for the creature's sounds
	const auto moments = _system.AdvanceTurn();
	EXPECT_EQ(registry.Get<CreatureAnimation>(creature).body.timeMs, 157.0f);
	ASSERT_EQ(moments.size(), 1u);
	EXPECT_EQ(moments.front().first, creature);
	EXPECT_EQ(PassedOf(moments.front().second), (std::vector<std::tuple<size_t, int32_t, int32_t>> {{1, 40, 157}}));
	// half frozen, it lives 50 ms: a step of 58
	registry.Get<CreatureAnimation>(creature).playbackScale = 0.5f;
	_system.AdvanceTurn();
	EXPECT_EQ(registry.Get<CreatureAnimation>(creature).body.timeMs, 215.0f);
	// in its pen, at the pen's size and not its own
	registry.Get<CreatureAnimation>(creature).playbackScale = 1.0f;
	registry.Get<CreatureDrawPose>(creature).size = 0.22f;
	_system.AdvanceTurn();
	EXPECT_EQ(registry.Get<CreatureAnimation>(creature).body.timeMs,
	          215.0f + static_cast<float>(creature_turn_pose::Step(100, 0.22f, false, false)));
	// nothing moves while its rest pose is not built for the shape it has
	const auto at = registry.Get<CreatureAnimation>(creature).body.timeMs;
	++registry.Get<CreatureMorph>(creature).revision;
	EXPECT_TRUE(_system.AdvanceTurn().empty());
	EXPECT_EQ(registry.Get<CreatureAnimation>(creature).body.timeMs, at);
}

TEST_F(CreatureAnimationSystemTest, WithoutARigTheTurnsAdvanceMovesNothing)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	auto& animation = registry.Get<CreatureAnimation>(creature);
	const auto posed = TwoBoneBody();
	animation.skeleton = posed.skeleton;
	animation.builtRevision = registry.Get<CreatureMorph>(creature).revision;
	animation.body = Walking(40.0f);

	EXPECT_TRUE(_system.AdvanceTurn().empty());

	EXPECT_EQ(registry.Get<CreatureAnimation>(creature).body.timeMs, 40.0f);
}
