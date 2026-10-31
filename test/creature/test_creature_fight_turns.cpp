/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature fight system, stepped by the turn, with fakes of the animations, skins, minds, hands and locomotion: what
// starts a fight, a duel's turn on the synced stream only, the frame drawing between the turns, the hand's press kept on
// the player's fighter, the creature knocked out taken home at once, fizzing out where it lies and back in at home,
// and the blow's clock at 100 ms steps against 30 frames a second; what the player's camera does as a fight starts;
// a miracle's blow and fainting where it stands; the
// fight health a creature leaves a fight with, kept after it; a creature withdrawn from its fight; the arenas of the fights now
// on, newest first, as the camera asks for them; in its temple's pen, the blow chosen, landing and hurting by the size it is
// drawn at; a fighter facing its opponent outright

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <bit>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Camera/FightWatch.h"
#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureArenaRing.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureFightHud.h"
#include "Creature/CreatureMorph.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureNaming.h"
#include "ECS/CreaturePose.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/Implementations/CreatureFightSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/Implementations/ReactionSystem.h"
#include "Input/InterfaceActive.h"
#include "Particles/LightSheet.h"
#include "creature/CreatureSystemFakes.h"
#include "creature/CreatureSystemWorld.h"
#include "support/CreatureFakes.h"
#include "support/ParticleFakes.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureFightSystem;
using StartResult = openblack::ecs::systems::CreatureFightSystemInterface::StartResult;
namespace fakes = openblack::test::creature_fakes;
namespace fight = openblack::creature_fight;
namespace loop_fakes = openblack::test::creature_loop_fakes;

namespace
{
/// The size a creature is drawn at deep in its temple's pen
constexpr float k_PenSize = 0.22f;

class CreatureFightTurnsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		_animation = &static_cast<fakes::FakeAnimation&>(Locator::creatureAnimationSystem::emplace<fakes::FakeAnimation>());
		_skin = &static_cast<fakes::FakeSkin&>(Locator::creatureSkinSystem::emplace<fakes::FakeSkin>());
		_mind = &static_cast<fakes::FakeMind&>(Locator::creatureMindSystem::emplace<fakes::FakeMind>());
		_hands =
		    &static_cast<fakes::FakeObjectAction&>(Locator::creatureObjectActionSystem::emplace<fakes::FakeObjectAction>());
		_locomotion = &static_cast<fakes::FakeLocomotion&>(Locator::creatureLocomotionSystem::emplace<fakes::FakeLocomotion>());
	}

	static CreatureFighting& FightingOf(entt::entity creature)
	{
		return test::creature_world::World::Registry().Get<CreatureFighting>(creature);
	}

	/// Two creatures duelling, facing each other 20 apart along z, both fought by the computer
	std::pair<entt::entity, entt::entity> Duel()
	{
		const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
		const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
		EXPECT_EQ(_system.StartFight(a, b), StartResult::Started);
		auto& registry = test::creature_world::World::Registry();
		for (const auto& [self, heading, at] : {std::tuple(a, 0.0f, glm::vec3(100.0f, 0.0f, 100.0f)),
		                                        std::tuple(b, std::numbers::pi_v<float>, glm::vec3(100.0f, 0.0f, 80.0f))})
		{
			auto& locomotion = registry.Get<CreatureLocomotion>(self);
			locomotion.heading = heading;
			locomotion.fromPosition = at;
			locomotion.toPosition = at;
			auto& fighting = FightingOf(self);
			fighting.stage = CreatureFighting::Stage::Duel;
			fighting.measured = true;
			fight::Enter(fighting.fighter, fight::State::Stance);
			fighting.fighter.control = fight::Control::Computer;
		}
		return {a, b};
	}

	/// Drawn deep in its temple's pen, its size and scale as the pen shrink leaves them
	static void InPen(entt::entity creature)
	{
		auto& registry = test::creature_world::World::Registry();
		auto& pose = registry.Get<CreatureDrawPose>(creature);
		pose.size = k_PenSize;
		pose.scale = registry.Get<Transform>(creature).scale * (k_PenSize / registry.Get<Creature>(creature).size);
	}

	/// Its one blow reaching `reach` ahead and `height` up, asked of it by the player and let go
	static void QueueBlow(entt::entity creature, float reach, float height)
	{
		auto& fighting = FightingOf(creature);
		fighting.reaches = {fight::Reach {.animation = fight::animations::k_FirstAttack, .reach = reach, .height = height}};
		// The player's move takes the creature back from the computer, which does not look at the queue
		EXPECT_TRUE(fight::PlayerMove(fighting.fighter, fight::AttackMove(fight::Band::Mid), false));
		EXPECT_TRUE(fighting.fighter.queue.Release(600.0f));
	}

	test::creature_world::World _world;
	const test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	const test::RestoreService<Locator::creatureAnimationSystem> _restoreAnimation;
	const test::RestoreService<Locator::creatureSkinSystem> _restoreSkin;
	const test::RestoreService<Locator::creatureMindSystem> _restoreMind;
	const test::RestoreService<Locator::creatureObjectActionSystem> _restoreHands;
	const test::RestoreService<Locator::creatureLocomotionSystem> _restoreLocomotion;
	fakes::FakeAnimation* _animation {nullptr};
	fakes::FakeSkin* _skin {nullptr};
	fakes::FakeMind* _mind {nullptr};
	fakes::FakeObjectAction* _hands {nullptr};
	fakes::FakeLocomotion* _locomotion {nullptr};
	CreatureFightSystem _system;
};

/// A blow of 1000 ms landing at 400 ms, played at the speed of a blow, stepped some milliseconds at a time for a while:
/// how many times it landed, and how long each blow took
struct Blows
{
	int landed {0};
	int finished {0};
	float lastBlowMs {0.0f};
};
Blows PlayBlows(float stepMs, float totalMs)
{
	constexpr float k_Duration = 1000.0f;
	constexpr float k_HitMs = 400.0f;
	Blows blows;
	float timeMs = 0.0f;
	float sinceStart = 0.0f;
	bool landed = false;
	for (float elapsed = 0.0f; elapsed + (stepMs * 0.5f) < totalMs; elapsed += stepMs)
	{
		const auto before = timeMs;
		timeMs = fight::AdvanceClock(timeMs, stepMs, 1.0f, fight::animations::k_FirstAttack);
		sinceStart += stepMs;
		if (!landed && fight::CrossesHit(before, timeMs, k_HitMs))
		{
			landed = true;
			++blows.landed;
		}
		// the next blow starts from its beginning: what went past the end is lost, as Enter does
		if (timeMs >= k_Duration)
		{
			++blows.finished;
			blows.lastBlowMs = sinceStart;
			sinceStart = 0.0f;
			timeMs = 0.0f;
			landed = false;
		}
	}
	return blows;
}

/// The player's camera's fight watching, recording what it is asked: whether it is too far from an arena is set by the
/// test
class FakeFightWatch final: public camera::FightWatch
{
public:
	struct Started
	{
		entt::entity fighterA;
		entt::entity fighterB;
		glm::vec3 centre;
		float radius;
	};

	void StartFight(entt::entity fighterA, entt::entity fighterB, glm::vec3 arenaCentre, float arenaRadius) override
	{
		started.push_back({fighterA, fighterB, arenaCentre, arenaRadius});
		watching = true;
	}
	void EndFightNow() override
	{
		++endedNow;
		watching = false;
	}
	void EndFight() override { ++ended; }
	[[nodiscard]] bool IsWatchingFight() const override { return watching; }
	[[nodiscard]] std::optional<entt::entity> GetWatchedFighter() const override
	{
		return watching && !started.empty() ? std::optional(started.back().fighterA) : std::nullopt;
	}
	[[nodiscard]] bool WantToQuitFight(glm::vec3 /*arenaCentre*/, float /*arenaRadius*/) const override
	{
		++asked;
		return tooFar;
	}

	std::vector<Started> started;
	int ended {0};
	int endedNow {0};
	bool watching {false};
	bool tooFar {true};
	mutable int asked {0};
};

/// A camera model that moves nothing, with the fight watching of the player's own camera or none
class FakeCameraModel final: public CameraModel
{
public:
	explicit FakeCameraModel(FakeFightWatch* watch)
	    : _watch(watch)
	{
	}
	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds /*dt*/, const Camera& /*camera*/) override
	{
		return std::nullopt;
	}
	void HandleActions(std::chrono::microseconds /*dt*/) override {}
	void SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/) override { ++flights; }
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return glm::vec3(0.0f); }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return glm::vec3(0.0f); }
	[[nodiscard]] camera::FightWatch* GetFightWatch() override { return _watch; }
	[[nodiscard]] const camera::FightWatch* GetFightWatch() const override { return _watch; }

	int flights {0};

private:
	FakeFightWatch* _watch;
};

/// The fight system's test with a camera whose model watches fights, and the interface's switch put back afterwards
class CreatureFightCameraTest: public CreatureFightTurnsTest
{
protected:
	void SetUp() override
	{
		CreatureFightTurnsTest::SetUp();
		auto model = std::make_unique<FakeCameraModel>(&_watch);
		_model = model.get();
		Locator::camera::emplace(glm::vec3(0.0f)).SetModel(std::move(model));
		interface_active::SetActive(true);
	}
	void TearDown() override { interface_active::SetFlags(_interfaceFlags); }

	/// A duel whose second fighter has gone, its first in a state that lasts and owned by a player
	entt::entity DuelWithOpponentGone(PlayerNames owner, fight::State state)
	{
		const auto [a, b] = Duel();
		for (const auto animation : {fight::animations::k_Stance, fight::animations::k_Finish, fight::animations::k_RecoilBlock,
		                             fight::animations::k_StartBlock})
		{
			_animation->durations[animation] = 100000.0f;
		}
		test::creature_world::World::Registry().Get<Creature>(a).owner = owner;
		fight::Enter(FightingOf(a).fighter, state);
		test::creature_world::World::Registry().RemoveState<CreatureFighting>(b);
		return a;
	}

	/// The local player's creature and another god's, 40 apart along z
	std::pair<entt::entity, entt::entity> Fighters(PlayerNames first = PlayerNames::PLAYER_ONE)
	{
		return {test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), first),
		        test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 60.0f), PlayerNames::PLAYER_TWO)};
	}

	// the watch outlives the camera model that points at it
	FakeFightWatch _watch;
	const test::RestoreService<Locator::camera> _restoreCamera;
	const uint8_t _interfaceFlags {interface_active::GetFlags()};
	FakeCameraModel* _model {nullptr};
};
} // namespace

TEST_F(CreatureFightTurnsTest, WhatStartsAFight)
{
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 60.0f), PlayerNames::PLAYER_THREE);
	EXPECT_EQ(_system.StartFight(a, a), StartResult::NoOpponent);
	EXPECT_EQ(_system.StartFight(a, test::creature_world::World::Registry().Create()), StartResult::NoOpponent);
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	EXPECT_TRUE(_system.IsFighting(a));
	EXPECT_EQ(_system.OpponentOf(b), std::optional(a));
	// both stopped what they were doing
	EXPECT_EQ(_locomotion->stopped.size(), 2u);
	EXPECT_EQ(_hands->cancelled.size(), 2u);
	EXPECT_EQ(_system.StartFight(b, a), StartResult::Busy);
	// a creature too hurt to fight
	const auto c = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_TWO);
	const auto d = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_THREE);
	auto& needs = test::creature_world::World::Registry().Get<CreatureNeeds>(c);
	needs.needs.life = 0.0f;
	EXPECT_EQ(_system.StartFight(c, d), StartResult::TooWeak);
	// the other creature, asked first, refuses one less than half or more than twice its height
	const auto big = test::creature_world::World::MakeCreature(glm::vec3(-100.0f, 0.0f, 0.0f), PlayerNames::PLAYER_TWO);
	const auto small = test::creature_world::World::MakeCreature(glm::vec3(-100.0f, 0.0f, 40.0f), PlayerNames::PLAYER_THREE);
	// it compares the heights they are drawn at, so their bodies are given those sizes too
	for (const auto& [creature, size] : {std::pair(big, 1.5f), std::pair(small, 0.6f)})
	{
		test::creature_world::World::Registry().Get<Creature>(creature).size = size;
		test::creature_world::World::Registry().Get<CreatureDrawPose>(creature).size = size;
	}
	EXPECT_EQ(_system.StartFight(big, small), StartResult::Refused);
	EXPECT_EQ(_system.StartFight(small, big), StartResult::Refused);
	EXPECT_FALSE(_system.IsFighting(big));
	EXPECT_FALSE(_system.IsFighting(small));
	_system.AbortFight(a);
	EXPECT_FALSE(_system.IsFighting(a));
	EXPECT_FALSE(_system.IsFighting(b));
}

TEST_F(CreatureFightTurnsTest, NoFightStartsWithACreatureLyingKnockedOut)
{
	auto& registry = test::creature_world::World::Registry();
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 60.0f), PlayerNames::PLAYER_THREE);
	// the one asking lies knocked out, then the one asked: neither fight starts, and nothing is stopped
	registry.AssignState<CreatureKnockedOut>(a, CreatureKnockedOut {});
	EXPECT_EQ(_system.StartFight(a, b), StartResult::Busy);
	EXPECT_EQ(_system.StartFight(b, a), StartResult::Busy);
	EXPECT_FALSE(_system.IsFighting(a));
	EXPECT_FALSE(_system.IsFighting(b));
	EXPECT_TRUE(_locomotion->stopped.empty());
	EXPECT_TRUE(_hands->cancelled.empty());
	// up again, it may fight
	registry.RemoveState<CreatureKnockedOut>(a);
	EXPECT_EQ(_system.StartFight(a, b), StartResult::Started);
}

TEST_F(CreatureFightTurnsTest, EndOwnFightEndsOnlyItsSideAndOnlyInTheMidstOfTheFight)
{
	const auto [a, b] = Duel();
	auto& needs = test::creature_world::World::Registry().Get<CreatureNeeds>(a);
	needs.needs.life = 1.0f;
	FightingOf(a).fighter.health = 0.6f;
	// fainted, it is not in the midst of the fight: nothing changes
	fight::Enter(FightingOf(a).fighter, fight::State::Faint);
	_system.EndOwnFight(a);
	EXPECT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Duel);
	EXPECT_FLOAT_EQ(needs.needs.life, 1.0f);
	// standing in its stance: its side ends, a quarter of the fight health it lost off its life
	fight::Enter(FightingOf(a).fighter, fight::State::Stance);
	_system.EndOwnFight(a);
	EXPECT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Respond);
	EXPECT_TRUE(FightingOf(a).fighter.stopping);
	EXPECT_FLOAT_EQ(needs.needs.life, 0.9f);
	// its opponent's fight is left as it is
	EXPECT_EQ(FightingOf(b).stage, CreatureFighting::Stage::Duel);
	EXPECT_FALSE(FightingOf(b).fighter.stopping);
}

TEST_F(CreatureFightTurnsTest, ADuelsTurnDrawsOnTheSyncedStreamOnly)
{
	const auto [a, b] = Duel();
	const game_random::testing::ScopedState state;
	std::vector<uint32_t> draws;
	game_random::testing::SetGameRand(
	    [&draws](uint32_t n) {
		    draws.push_back(n);
		    return 0u;
	    },
	    [](float) { return 0.0f; });
	const auto local = game_random::Current().local;
	const auto crt = game_random::crt::Seed();
	for (int turn = 0; turn < 10; ++turn)
	{
		_system.ProcessTurn();
	}
	// the computer chose its moves on the synced stream
	EXPECT_FALSE(draws.empty());
	EXPECT_EQ(game_random::Current().local, local);
	EXPECT_EQ(game_random::crt::Seed(), crt);
	EXPECT_TRUE(_system.IsFighting(a));
	EXPECT_TRUE(_system.IsFighting(b));
}

// In its stance a fighter faces its opponent outright in one turn, the heading the original works out from the way to it
TEST_F(CreatureFightTurnsTest, AFighterInItsStanceFacesItsOpponentOutright)
{
	const auto [a, b] = Duel();
	const game_random::testing::ScopedState state;
	game_random::testing::SetGameRand([](uint32_t /*n*/) { return 1u; }, [](float) { return 0.0f; });
	// the opponent stepped round to 7 left and 24 ahead of it
	auto& registry = test::creature_world::World::Registry();
	const glm::vec3 aside {93.0f, 0.0f, 76.0f};
	auto& other = registry.Get<CreatureLocomotion>(b);
	other.fromPosition = aside;
	other.toPosition = aside;
	registry.Get<Transform>(b).position = aside;
	_system.ProcessTurn();
	EXPECT_EQ(std::bit_cast<uint32_t>(registry.Get<const CreatureLocomotion>(a).heading), 0x3E914D76u);
	EXPECT_EQ(registry.Get<const CreatureLocomotion>(a).toHeading, registry.Get<const CreatureLocomotion>(a).heading);
}

TEST_F(CreatureFightTurnsTest, TheFrameOnlyDrawsBetweenTheTurns)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	// a blow playing out after a fight that ended: nothing but its animation moves it on
	auto& fighting = registry.AssignState<CreatureFighting>(creature);
	fighting.stage = CreatureFighting::Stage::Respond;
	fight::Enter(fighting.fighter, fight::State::Action, fight::animations::k_FirstAttack);
	fighting.fighter.timeMs = 200.0f;
	_animation->durations[fight::animations::k_FirstAttack] = 1000.0f;

	_system.ProcessTurn();
	const auto after = FightingOf(creature).fighter.timeMs;
	EXPECT_FLOAT_EQ(FightingOf(creature).drawFromMs, 200.0f);
	EXPECT_FLOAT_EQ(after, fight::AdvanceClock(200.0f, 100.0f, 1.0f, fight::animations::k_FirstAttack));
	const auto transform = registry.Get<Transform>(creature);

	_system.Update(0.5f, 16.0f);
	EXPECT_FLOAT_EQ(FightingOf(creature).fighter.timeMs, after);
	EXPECT_FLOAT_EQ(registry.Get<CreatureAnimation>(creature).body.timeMs, fight::DrawnTime(200.0f, after, 0.5f));
	EXPECT_EQ(registry.Get<Transform>(creature).position, transform.position);
}

TEST_F(CreatureFightTurnsTest, TheBodyAtAShareOfTheTurnIsWhatTheFrameDrawsThen)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	auto& fighting = registry.AssignState<CreatureFighting>(creature);
	fighting.stage = CreatureFighting::Stage::Respond;
	fight::Enter(fighting.fighter, fight::State::Action, fight::animations::k_FirstAttack);
	fighting.fighter.timeMs = 200.0f;
	_animation->durations[fight::animations::k_FirstAttack] = 1000.0f;
	_system.ProcessTurn();
	const auto& lookup = std::as_const(registry);
	const auto& animation = lookup.Get<const CreatureAnimation>(creature);

	for (const auto share : {0.5f, 1.0f})
	{
		_system.Update(share, 16.0f);
		// the frame's body, the query's and the pure part's are one: the fight's animation in place of any slots
		CreatureAnimationInputs inputs {.body = {},
		                                .slots = {{.animation = 3, .timeMs = 1.0f, .weight = 1.0f, .mirrored = false}}};
		_system.AnimationAt(creature, share, inputs);
		const auto& drawing = lookup.Get<const CreatureFighting>(creature);
		const auto pure = CreatureFightSystem::BodyAt(drawing, 1000.0f, share);
		EXPECT_EQ(animation.body.timeMs, fight::DrawnTime(drawing.drawFromMs, drawing.fighter.timeMs, share));
		EXPECT_EQ(inputs.body.timeMs, animation.body.timeMs);
		EXPECT_EQ(pure.timeMs, animation.body.timeMs);
		EXPECT_EQ(inputs.body.animations, animation.body.animations);
		EXPECT_EQ(pure.animations, animation.body.animations);
		EXPECT_EQ(inputs.body.kind, animation.body.kind);
		EXPECT_EQ(inputs.body.phase, animation.body.phase);
		EXPECT_EQ(inputs.body.mirrored, animation.body.mirrored);
		EXPECT_EQ(inputs.body.holdLoop, animation.body.holdLoop);
		EXPECT_TRUE(inputs.body.timedByPlayer);
		EXPECT_TRUE(inputs.slots.empty());
		EXPECT_TRUE(animation.slots.empty());
	}
	// past the turn's end it is drawn at the end
	CreatureAnimationInputs past;
	_system.AnimationAt(creature, 1.5f, past);
	EXPECT_EQ(past.body.timeMs, animation.body.timeMs);
	// someone not fighting gets nothing
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(200.0f, 0.0f, 200.0f));
	CreatureAnimationInputs none {.body = {.timeMs = 7.0f}, .slots = {}};
	_system.AnimationAt(other, 1.0f, none);
	EXPECT_EQ(none.body.timeMs, 7.0f);
}

// A press on the arena's ground, as the turn takes it, puts the step it makes into the creature's queue: in place of the
// moves queued, or after them
TEST_F(CreatureFightTurnsTest, AGroundPressQueuesTheStepTowardsThePoint)
{
	const auto [a, b] = Duel();
	auto& fighting = FightingOf(a);
	// before the duel the queue is only filled
	fighting.stage = CreatureFighting::Stage::Ready;
	ASSERT_TRUE(fight::PlayerMove(fighting.fighter, fight::BlockMove(), false));
	// heading 0 faces -z: a point ahead is a step forward, after the block
	_system.GroundPress(a, glm::vec3(100.0f, 0.0f, 90.0f), false);
	ASSERT_EQ(_system.QueuedMoves(a), 2u);
	EXPECT_EQ(FightingOf(a).fighter.queue.Moves()[1].move, fight::StepMove(fight::Step::Forward));
	// one beside it, in place of both
	_system.GroundPress(a, glm::vec3(90.0f, 0.0f, 100.0f), true);
	ASSERT_EQ(_system.QueuedMoves(a), 1u);
	EXPECT_EQ(FightingOf(a).fighter.queue.Moves()[0].move, fight::StepMove(fight::Step::Right));
	// the player's move takes the creature back from the computer
	EXPECT_EQ(FightingOf(a).fighter.control, fight::Control::Player);
	// nobody not fighting takes it
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(300.0f, 0.0f, 300.0f));
	_system.GroundPress(other, glm::vec3(300.0f, 0.0f, 290.0f), true);
	EXPECT_EQ(_system.QueuedMoves(other), 0u);
	EXPECT_EQ(_system.QueuedMoves(b), 0u);
}

// A line of sight meets a creature's skin only as its mesh is drawn, and a press needs its head's height: a creature
// without a loaded mesh or a posed head is met nowhere
TEST_F(CreatureFightTurnsTest, ALineOfSightMeetsNoCreatureWithoutItsSkin)
{
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f));
	const glm::vec3 above(100.0f, 50.0f, 100.0f);
	const glm::vec3 down(0.0f, -1.0f, 0.0f);
	EXPECT_FALSE(_system.BodyHit(creature, above, down).has_value());
	EXPECT_FALSE(_system.BodyHit(entt::null, above, down).has_value());
}

namespace
{
/// The particles, keeping the sheets of light they are given to draw
class SheetParticles final: public test::InertParticleSystem
{
public:
	void AddLightSheet(const std::shared_ptr<particles::LightSheet>& sheet) override { sheets.push_back(sheet); }
	std::vector<std::weak_ptr<particles::LightSheet>> sheets;
};
} // namespace

// While a fight is on its arena stands in a ring of light: one sheet of 32 points round its edge, on the land, a sixth of
// its radius high and at full strength from its first frame, and let go when the arena is no longer in use
TEST_F(CreatureFightTurnsTest, AnArenaStandsInARingOfLightWhileItsFightIsOn)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = static_cast<SheetParticles&>(Locator::particleSystem::emplace<SheetParticles>());
	const auto [a, b] = Duel();
	const auto arena = _system.ArenaOf(a);
	ASSERT_TRUE(arena.has_value());
	_system.Update(0.5f, 16.0f);
	_system.Update(0.5f, 16.0f);
	ASSERT_EQ(particles.sheets.size(), 1u);
	{
		// held here only while it is looked at, so that the fight system's is the one left
		const auto ring = particles.sheets.front().lock();
		ASSERT_NE(ring, nullptr);
		EXPECT_EQ(ring->Strengths().size(), creature_arena_ring::k_Points);
		EXPECT_FLOAT_EQ(ring->Spread(), creature_arena_ring::k_Spread);
		EXPECT_EQ(ring->Middle(), glm::vec3(0.0f));
	}
	// once the fight is over and its fighters have left it, the ring goes
	auto& registry = test::creature_world::World::Registry();
	registry.RemoveState<CreatureFighting>(a);
	registry.RemoveState<CreatureFighting>(b);
	_system.Update(0.5f, 16.0f);
	EXPECT_TRUE(particles.sheets.front().expired());
}

TEST_F(CreatureFightTurnsTest, KnockedOutItIsTakenHomeAtOnce)
{
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	auto& registry = test::creature_world::World::Registry();
	registry.AssignState<CreatureKnockedOut>(creature, CreatureKnockedOut {.stage = CreatureKnockedOut::Stage::FadingOut,
	                                                                       .home = glm::vec3(300.0f, 0.0f, 300.0f),
	                                                                       .fizzTurns = 1});
	_world.teleported.clear();
	_system.ProcessTurn();
	ASSERT_EQ(_world.teleported.size(), 1u);
	EXPECT_EQ(_world.teleported.front(), creature);
	EXPECT_EQ(registry.Get<Transform>(creature).position, glm::vec3(300.0f, 0.0f, 300.0f));
	const auto& locomotion = registry.Get<CreatureLocomotion>(creature);
	EXPECT_EQ(locomotion.fromPosition, locomotion.toPosition);
	EXPECT_EQ(registry.Get<CreatureKnockedOut>(creature).stage, CreatureKnockedOut::Stage::FadingIn);
	EXPECT_TRUE(_system.IsKnockedOut(creature));
}

namespace
{
/// The fight's turns with fakes of the fizz, which keeps each fizz set, and of the body, which keeps each creature woken
/// from its faint
class CreatureFightCarriedHomeTest: public CreatureFightTurnsTest
{
protected:
	CreatureFightCarriedHomeTest()
	    : _fizz(&static_cast<loop_fakes::FakeFizz&>(Locator::creatureFizzSystem::emplace<loop_fakes::FakeFizz>(_log)))
	    , _body(&static_cast<loop_fakes::FakePhysiology&>(
	          Locator::creaturePhysiologySystem::emplace<loop_fakes::FakePhysiology>(_log)))
	{
	}

	/// A creature of a player lying out cold long enough to be taken home at its next turn
	static entt::entity LyingOutCold(PlayerNames owner = PlayerNames::PLAYER_TWO)
	{
		const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), owner);
		test::creature_world::World::Registry().AssignState<CreatureKnockedOut>(
		    creature, CreatureKnockedOut {.stage = CreatureKnockedOut::Stage::Lying,
		                                  .seconds = 1000.0f,
		                                  .home = glm::vec3(300.0f, 0.0f, 300.0f)});
		return creature;
	}

	static CreatureKnockedOut::Stage StageOf(entt::entity creature)
	{
		return std::as_const(test::creature_world::World::Registry()).Get<const CreatureKnockedOut>(creature).stage;
	}

	static glm::vec3 PositionOf(entt::entity creature)
	{
		return std::as_const(test::creature_world::World::Registry()).Get<const Transform>(creature).position;
	}

	using Set = loop_fakes::FakeFizz::Set;

	loop_fakes::CallLog _log;
	const test::RestoreService<Locator::creatureFizzSystem> _restoreFizz;
	const test::RestoreService<Locator::creaturePhysiologySystem> _restoreBody;
	loop_fakes::FakeFizz* _fizz;
	loop_fakes::FakePhysiology* _body;
};
} // namespace

TEST_F(CreatureFightCarriedHomeTest, ItFizzesOutWhereItLiesAndBackInAtHome)
{
	const auto creature = LyingOutCold();
	// it starts fizzing out over two seconds, with the teleport's sound
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::FadingOut);
	ASSERT_EQ(_fizz->sets.size(), 1u);
	EXPECT_EQ(_fizz->sets.back(), (Set {.creature = creature, .target = 1.0f, .seconds = 2.0f, .goesForGood = false}));
	// twenty turns where it lies
	for (int turn = 0; turn < 19; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::FadingOut);
	EXPECT_EQ(PositionOf(creature), glm::vec3(100.0f, 0.0f, 100.0f));
	EXPECT_EQ(_fizz->sets.size(), 1u);
	EXPECT_TRUE(_body->woken.empty());
	// on the twentieth it is moved home, fizzes back in over two seconds and comes round as far as it is moved
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::FadingIn);
	EXPECT_EQ(PositionOf(creature), glm::vec3(300.0f, 0.0f, 300.0f));
	ASSERT_EQ(_fizz->sets.size(), 2u);
	EXPECT_EQ(_fizz->sets.back(), (Set {.creature = creature, .target = 0.0f, .seconds = 2.0f, .goesForGood = false}));
	EXPECT_EQ(_body->woken, std::vector<entt::entity> {creature});
	// twenty turns more at home, and the fade back in is set once more as it ends
	for (int turn = 0; turn < 19; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::FadingIn);
	EXPECT_EQ(_fizz->sets.size(), 2u);
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::Waiting);
	ASSERT_EQ(_fizz->sets.size(), 3u);
	EXPECT_EQ(_fizz->sets.back(), (Set {.creature = creature, .target = 0.0f, .seconds = 2.0f, .goesForGood = false}));
	EXPECT_EQ(_body->woken.size(), 1u);
}

TEST_F(CreatureFightCarriedHomeTest, AFaintedCreatureIsGotUpOnlyOnceItLies)
{
	// Its wait over, a rested creature still falling, as a big one's slow fall can: it is not got up
	auto& registry = test::creature_world::World::Registry();
	const auto creature = LyingOutCold(PlayerNames::NEUTRAL);
	registry.Get<CreatureKnockedOut>(creature).stage = CreatureKnockedOut::Stage::Waiting;
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->needs.life = 1.0f;
		needs->needs.exhaustion = 0.0f;
	}
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.body =
	    *creature_layers::PlaySequence({}, creature_layers::animations::k_Faint, creature_layers::animations::k_Faint,
	                                   creature_layers::animations::k_GetUp, true);
	for (int turn = 0; turn < 3; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::Waiting);
	EXPECT_FALSE(animation.body.endWanted);
	// lying, it is got up on the next turn
	animation.body.phase = creature_layers::BodyAction::Phase::Loop;
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::GettingUp);
	EXPECT_TRUE(animation.body.endWanted);
}

TEST_F(CreatureFightCarriedHomeTest, ANeutralCreatureNeverFizzes)
{
	const auto creature = LyingOutCold(PlayerNames::NEUTRAL);
	for (int turn = 0; turn < 50; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_TRUE(_fizz->sets.empty());
	EXPECT_EQ(PositionOf(creature), glm::vec3(100.0f, 0.0f, 100.0f));
}

TEST_F(CreatureFightCarriedHomeTest, StoppedWhileCarriedHomeItIsBackInSightAtOnce)
{
	auto& registry = test::creature_world::World::Registry();
	const auto fadingOut = LyingOutCold();
	const auto fadingIn = LyingOutCold();
	const auto lying = LyingOutCold();
	registry.Get<CreatureKnockedOut>(fadingOut).stage = CreatureKnockedOut::Stage::FadingOut;
	registry.Get<CreatureKnockedOut>(fadingIn).stage = CreatureKnockedOut::Stage::FadingIn;
	_system.Resurrect(fadingOut);
	ASSERT_EQ(_fizz->sets.size(), 1u);
	EXPECT_EQ(_fizz->sets.back(), (Set {.creature = fadingOut, .target = 0.0f, .seconds = 0.0f, .goesForGood = false}));
	_system.KillPermanently(fadingIn);
	ASSERT_EQ(_fizz->sets.size(), 2u);
	EXPECT_EQ(_fizz->sets.back(), (Set {.creature = fadingIn, .target = 0.0f, .seconds = 0.0f, .goesForGood = false}));
	// lying where it fell, it isn't fizzing
	_system.KillPermanently(lying);
	_system.Resurrect(lying);
	EXPECT_EQ(_fizz->sets.size(), 2u);
}

// A script's CREATURE_DIE on the creature itself (Land4Ogre, TheBigFightMain): its spells are left on it, it falls,
// and once it is seen lying it lies ftol((4 x the size it is drawn at + 8) x 10) turns, then the carry home starts
TEST_F(CreatureFightCarriedHomeTest, ForcedToDieItLiesTheTurnsOfItsDrawnSizeFromWhenItIsSeenLying)
{
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::reactionSystem> keepReactions;
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	// drawn at 1.5: (1.5 x 4 + 8) seconds, 140 turns at 10 a second
	registry.Get<CreatureDrawPose>(creature).size = 1.5f;
	auto& slot = registry.AssignOrReplaceState<CreatureSpells>(creature).spells[creature_spells::Spell::Big];
	slot.phase = creature_spells::Phase::Holding;
	slot.turnsLeft = 100;
	slot.holdTurns = 100;
	_system.ForceDie(creature, creature);
	ASSERT_TRUE(_system.IsKnockedOut(creature));
	// those near it react as to a death, with no player
	const auto death = ecs::effects::reactions::GetReactionOfTypeInitiatedBy(creature, openblack::Reaction::ReactToDeath);
	ASSERT_NE(death, 0u);
	EXPECT_FALSE(ecs::effects::reactions::Find(death)->player.has_value());
	const auto& knockedOut = std::as_const(registry).Get<const CreatureKnockedOut>(creature);
	ASSERT_TRUE(knockedOut.lieTurns.has_value());
	EXPECT_EQ(*knockedOut.lieTurns, 140);
	EXPECT_TRUE(knockedOut.actedOn == std::optional(creature));
	const auto& spell = std::as_const(registry).Get<const CreatureSpells>(creature).spells[creature_spells::Spell::Big];
	EXPECT_EQ(spell.turnsLeft, 100);
	EXPECT_EQ(spell.holdTurns, 100);
	// while it falls no turn of the lie counts
	auto& animation = registry.Get<CreatureAnimation>(creature);
	EXPECT_TRUE(animation.body.holdLoop);
	EXPECT_EQ(animation.body.phase, creature_layers::BodyAction::Phase::Start);
	for (int turn = 0; turn < 5; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_FALSE(knockedOut.seenLying);
	EXPECT_EQ(*knockedOut.lieTurns, 140);
	// the turn it is seen lying ends the faint; the lie's turns count from the next
	animation.body.phase = creature_layers::BodyAction::Phase::Loop;
	_system.ProcessTurn();
	EXPECT_TRUE(knockedOut.seenLying);
	EXPECT_EQ(*knockedOut.lieTurns, 140);
	for (int turn = 0; turn < 140; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::Lying);
	EXPECT_TRUE(_fizz->sets.empty());
	// the turn after its last it starts fizzing out, to be carried home
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::FadingOut);
	ASSERT_EQ(_fizz->sets.size(), 1u);
	EXPECT_EQ(_fizz->sets.back(), (Set {.creature = creature, .target = 1.0f, .seconds = 2.0f, .goesForGood = false}));
}

// Forced to die while knocked out already, the knock-out starts again from its faint: carried home part way, it is back
// in full sight at once; lying, it does not fall again
TEST_F(CreatureFightCarriedHomeTest, ForcedToDieWhileKnockedOutItStartsAgainFromItsFaint)
{
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::reactionSystem> keepReactions;
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto& registry = test::creature_world::World::Registry();
	const auto creature = LyingOutCold();
	registry.Get<CreatureKnockedOut>(creature).stage = CreatureKnockedOut::Stage::FadingOut;
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.body =
	    *creature_layers::PlaySequence({}, creature_layers::animations::k_Faint, creature_layers::animations::k_Faint,
	                                   creature_layers::animations::k_GetUp, true);
	animation.body.phase = creature_layers::BodyAction::Phase::Loop;
	_system.ForceDie(creature, creature);
	ASSERT_EQ(_fizz->sets.size(), 1u);
	EXPECT_EQ(_fizz->sets.back(), (Set {.creature = creature, .target = 0.0f, .seconds = 0.0f, .goesForGood = false}));
	const auto& knockedOut = std::as_const(registry).Get<const CreatureKnockedOut>(creature);
	EXPECT_EQ(knockedOut.stage, CreatureKnockedOut::Stage::Lying);
	EXPECT_FLOAT_EQ(knockedOut.seconds, 0.0f);
	EXPECT_TRUE(knockedOut.lieTurns.has_value());
	EXPECT_EQ(animation.body.phase, creature_layers::BodyAction::Phase::Loop);
	_system.ProcessTurn();
	EXPECT_TRUE(knockedOut.seenLying);
}

namespace
{
/// A creature of no player lying at the end of its wait, its body lying, its rest decided as a Die action was forced
entt::entity WaitedForcedToDie(bool rests, std::optional<entt::entity> actedOn)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::NEUTRAL);
	registry.AssignState<CreatureKnockedOut>(
	    creature, CreatureKnockedOut {
	                  .stage = CreatureKnockedOut::Stage::Waiting, .seconds = 1000.0f, .rests = rests, .actedOn = actedOn});
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.body =
	    *creature_layers::PlaySequence({}, creature_layers::animations::k_Faint, creature_layers::animations::k_Faint,
	                                   creature_layers::animations::k_GetUp, true);
	animation.body.phase = creature_layers::BodyAction::Phase::Loop;
	return creature;
}
} // namespace

// After the wait at home, the spells on it and on the creature the action was done to are brought to their end, turn
// after turn until none is left on either; then it rests if that was decided as the action was forced, whatever its
// life is now
TEST_F(CreatureFightCarriedHomeTest, ForcedToDieItWaitsForTheSpellsThenRestsAsDecidedWhenForced)
{
	auto& registry = test::creature_world::World::Registry();
	// out of life, but forced when it needed no rest
	const auto creature = WaitedForcedToDie(false, std::nullopt);
	registry.Get<CreatureNeeds>(creature).needs.life = 0.0f;
	auto& slot = registry.AssignOrReplaceState<CreatureSpells>(creature).spells[creature_spells::Spell::Big];
	slot.phase = creature_spells::Phase::Holding;
	slot.turnsLeft = 100;
	slot.holdTurns = 100;
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::Waiting);
	EXPECT_EQ(slot.turnsLeft, 0);
	EXPECT_EQ(slot.holdTurns, 0);
	slot.phase = creature_spells::Phase::Off;
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::GettingUp);

	// healthy, but forced when it needed a rest; done to another creature, whose spells it waits for too
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(200.0f, 0.0f, 100.0f), PlayerNames::NEUTRAL);
	const auto resting = WaitedForcedToDie(true, other);
	registry.Get<CreatureNeeds>(resting).needs.life = 1.0f;
	registry.Get<CreatureNeeds>(resting).needs.exhaustion = 0.0f;
	registry.AssignOrReplaceState<CreatureSpells>(other).spells[creature_spells::Spell::Big].phase =
	    creature_spells::Phase::Holding;
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(resting), CreatureKnockedOut::Stage::Waiting);
	registry.Get<CreatureSpells>(other).spells[creature_spells::Spell::Big].phase = creature_spells::Phase::Off;
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(resting), CreatureKnockedOut::Stage::Resting);
}

// Done to a thing that is not a creature, the wait for the spells never ends, and it never gets up
TEST_F(CreatureFightCarriedHomeTest, ForcedToDieOnAThingThatIsNotACreatureItNeverGetsUp)
{
	auto& registry = test::creature_world::World::Registry();
	const auto rock = registry.Create();
	const auto creature = WaitedForcedToDie(false, rock);
	for (int turn = 0; turn < 50; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::Waiting);
}

TEST(CreatureFightClock, ABlowLandsOnceAtTurnStepsAndAtFrameSteps)
{
	// 16 seconds of blows, at the turn's 100 ms and at three frames a turn
	const auto turns = PlayBlows(100.0f, 16000.0f);
	const auto frames = PlayBlows(100.0f / 3.0f, 16000.0f);
	// every blow lands once, at either rate
	EXPECT_EQ(turns.landed, turns.finished + (turns.landed > turns.finished ? 1 : 0));
	EXPECT_EQ(frames.landed, frames.finished + (frames.landed > frames.finished ? 1 : 0));
	// what goes past a blow's end is lost as the next one starts, so a blow takes the whole steps it needs: 16 turns
	// (1.6 s), or 46 frames (about 1.53 s). The turn-stepped duel is that much slower than one stepped by the frame.
	EXPECT_EQ(turns.finished, 10);
	EXPECT_NEAR(turns.lastBlowMs, 1600.0f, 1e-2f);
	EXPECT_NEAR(frames.lastBlowMs, 46.0f * 100.0f / 3.0f, 1e-1f);
	EXPECT_EQ(frames.finished, 10);
	EXPECT_EQ(frames.landed, 11);
	EXPECT_EQ(turns.landed, 10);
}

TEST_F(CreatureFightCameraTest, FromAfarThePlayersCreaturesFightOnlyHasTheSpiritsRemark)
{
	const auto [a, b] = Fighters();
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	EXPECT_EQ(_watch.asked, 1);
	EXPECT_TRUE(_watch.started.empty());
	// the fight system never flies the camera itself
	EXPECT_EQ(_model->flights, 0);
	_system.ProcessTurn();
	EXPECT_EQ(_model->flights, 0);
	EXPECT_TRUE(_watch.started.empty());
}

TEST_F(CreatureFightCameraTest, LookingAtTheArenaItWatchesAnyFight)
{
	_watch.tooFar = false;
	const auto [a, b] = Fighters(PlayerNames::PLAYER_THREE);
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	ASSERT_EQ(_watch.started.size(), 1u);
	const auto& started = _watch.started.front();
	EXPECT_EQ(started.fighterA, a);
	EXPECT_EQ(started.fighterB, b);
	const auto& arena = FightingOf(a).arena;
	EXPECT_EQ(started.centre, glm::vec3(arena.centre.x, 0.0f, arena.centre.y));
	EXPECT_FLOAT_EQ(started.radius, arena.radius);
	EXPECT_EQ(_model->flights, 0);
}

TEST_F(CreatureFightCameraTest, AScriptControlledFighterBringsItToThePlayersCreaturesFight)
{
	{
		const auto [a, b] = Fighters();
		// a script controls both, or the other creature refuses the fight
		ecs::script_held::SetControlledByScript(a, true);
		ecs::script_held::SetControlledByScript(b, true);
		ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
		ASSERT_EQ(_watch.started.size(), 1u);
		EXPECT_EQ(_watch.started.front().fighterA, a);
	}
	// but not to a fight between two other gods' creatures
	_watch.started.clear();
	const auto [c, d] = Fighters(PlayerNames::PLAYER_THREE);
	ecs::script_held::SetControlledByScript(c, true);
	ecs::script_held::SetControlledByScript(d, true);
	ASSERT_EQ(_system.StartFight(c, d), StartResult::Started);
	EXPECT_TRUE(_watch.started.empty());
}

TEST_F(CreatureFightCameraTest, NotWhileTheScriptHasTheScreenNorWithoutThePlayersCamera)
{
	_watch.tooFar = false;
	interface_active::SetActive(false);
	const auto [a, b] = Fighters();
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	EXPECT_EQ(_watch.asked, 0);
	EXPECT_TRUE(_watch.started.empty());
	// a camera model that has no fight watching: nothing is asked
	interface_active::SetActive(true);
	Locator::camera::value().SetModel(std::make_unique<FakeCameraModel>(nullptr));
	const auto [c, d] = Fighters();
	ASSERT_EQ(_system.StartFight(c, d), StartResult::Started);
	EXPECT_EQ(_watch.asked, 0);
	EXPECT_TRUE(_watch.started.empty());
}

TEST_F(CreatureFightCameraTest, TheDebugSwitchKeepsTheCameraAway)
{
	_watch.tooFar = false;
	_system.SetCameraWatches(false);
	const auto [a, b] = Fighters();
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	EXPECT_TRUE(_watch.started.empty());
}

TEST_F(CreatureFightCameraTest, AFightEndingInItsMiddleTellsTheCameraItIsOverWhoeverFights)
{
	// two other gods' creatures standing in a duel: each one's end tells it once
	const auto [a, b] = Duel();
	_system.AbortFight(a);
	EXPECT_EQ(_watch.ended, 2);
	EXPECT_EQ(_watch.endedNow, 0);
	// not while the script has the screen
	_watch.ended = 0;
	interface_active::SetActive(false);
	const auto [c, d] = Duel();
	_system.AbortFight(c);
	EXPECT_EQ(_watch.ended, 0);
}

TEST_F(CreatureFightCameraTest, ThePlayersCreatureLeftByItsOpponentTellsTheCameraItIsOver)
{
	// already finishing, the end itself says nothing, but it is the player's creature
	const auto mine = DuelWithOpponentGone(PlayerNames::PLAYER_ONE, fight::State::Finish);
	_system.ProcessTurn();
	ASSERT_EQ(FightingOf(mine).stage, CreatureFighting::Stage::Respond);
	EXPECT_EQ(_watch.ended, 1);
	EXPECT_EQ(_watch.endedNow, 0);
}

TEST_F(CreatureFightCameraTest, AnotherGodsCreatureLeftByItsOpponentTellsTheCameraOnlyFromTheMiddleOfTheFight)
{
	const auto standing = DuelWithOpponentGone(PlayerNames::PLAYER_THREE, fight::State::Stance);
	_system.ProcessTurn();
	ASSERT_EQ(FightingOf(standing).stage, CreatureFighting::Stage::Respond);
	EXPECT_EQ(_watch.ended, 1);
	_watch.ended = 0;
	const auto recoiling = DuelWithOpponentGone(PlayerNames::PLAYER_THREE, fight::State::BlockRecoil);
	_system.ProcessTurn();
	ASSERT_EQ(FightingOf(recoiling).stage, CreatureFighting::Stage::Respond);
	EXPECT_EQ(_watch.ended, 0);
}

TEST_F(CreatureFightCameraTest, TheCameraIsOnAFightWhileItsModelWatchesOne)
{
	EXPECT_FALSE(_system.IsCameraOnFight());
	_watch.watching = true;
	EXPECT_TRUE(_system.IsCameraOnFight());
	Locator::camera::value().SetModel(std::make_unique<FakeCameraModel>(nullptr));
	EXPECT_FALSE(_system.IsCameraOnFight());
}

TEST_F(CreatureFightCameraTest, ThePanelShowsOnlyTheFightTheCameraWatchesItsArenasMakerFirst)
{
	_watch.tooFar = false;
	const auto [a, b] = Fighters(PlayerNames::PLAYER_THREE);
	const auto c = test::creature_world::World::MakeCreature(glm::vec3(300.0f, 0.0f, 300.0f), PlayerNames::PLAYER_ONE);
	const auto d = test::creature_world::World::MakeCreature(glm::vec3(340.0f, 0.0f, 300.0f), PlayerNames::PLAYER_TWO);
	// the camera watches the first fight only: the second, the local player's own, is too far from it
	ASSERT_EQ(_system.StartFight(b, a), StartResult::Started);
	_watch.tooFar = true;
	ASSERT_EQ(_system.StartFight(c, d), StartResult::Started);
	ASSERT_EQ(_watch.started.size(), 1u);
	const auto panel = _system.GetPanel();
	ASSERT_TRUE(panel.has_value());
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	// Each side by its creature's own name
	EXPECT_EQ(panel->sides[0].name, ecs::creature_naming::NameOf(registry, b));
	EXPECT_EQ(panel->sides[1].name, ecs::creature_naming::NameOf(registry, a));
	EXPECT_EQ(panel->sides[0].health, FightingOf(b).fighter.health);
	EXPECT_EQ(panel->sides[1].health, FightingOf(a).fighter.health);
	EXPECT_EQ(panel->sides[1].stamina, FightingOf(a).fighter.stamina);
	// still while the camera lingers on it, and none once the camera has left it
	_watch.EndFight();
	EXPECT_TRUE(_system.GetPanel().has_value());
	_watch.watching = false;
	EXPECT_FALSE(_system.GetPanel().has_value());
}

TEST_F(CreatureFightCameraTest, ThePanelGoesWithTheWatchedFightsArena)
{
	_watch.tooFar = false;
	const auto [a, b] = Fighters();
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	ASSERT_TRUE(_system.GetPanel().has_value());
	// the maker gone from its fight takes the arena, and the panel with it, though the camera still watches
	test::creature_world::World::Registry().RemoveState<CreatureFighting>(a);
	ASSERT_TRUE(_watch.IsWatchingFight());
	EXPECT_FALSE(_system.GetPanel().has_value());
}

TEST_F(CreatureFightTurnsTest, ThereIsNoPanelWithoutTheCamerasWatch)
{
	const auto [a, b] = Duel();
	ASSERT_TRUE(_system.IsFighting(a));
	EXPECT_FALSE(_system.GetPanel().has_value());
}

TEST_F(CreatureFightTurnsTest, AMiracleStaggersAFighterOrRecoilsItsBlock)
{
	const auto [a, b] = Duel();
	_animation->durations[fight::animations::k_StartBlock] = 1000.0f;
	// standing, it staggers
	EXPECT_FALSE(_system.IsBlocking(a));
	_system.Recoil(a);
	EXPECT_EQ(FightingOf(a).fighter.state, fight::State::Action);
	EXPECT_EQ(FightingOf(a).fighter.animation, fight::animations::k_RecoilMid);
	// reeling already, it takes no more notice
	FightingOf(a).fighter.timeMs = 300.0f;
	_system.Recoil(a);
	EXPECT_FLOAT_EQ(FightingOf(a).fighter.timeMs, 300.0f);
	// half way into its block it blocks, and recoils in it
	fight::Enter(FightingOf(b).fighter, fight::State::BlockStart);
	FightingOf(b).fighter.timeMs = 400.0f;
	EXPECT_FALSE(_system.IsBlocking(b));
	FightingOf(b).fighter.timeMs = 600.0f;
	EXPECT_TRUE(_system.IsBlocking(b));
	_system.Recoil(b);
	EXPECT_EQ(FightingOf(b).fighter.state, fight::State::BlockRecoil);
	EXPECT_EQ(FightingOf(b).fighter.animation, fight::animations::k_RecoilBlock);
	// getting up, or not fighting at all, nothing
	fight::Enter(FightingOf(b).fighter, fight::State::GetUp);
	_system.Recoil(b);
	EXPECT_EQ(FightingOf(b).fighter.state, fight::State::GetUp);
	const auto idle = test::creature_world::World::MakeCreature();
	EXPECT_FALSE(_system.IsBlocking(idle));
	_system.Recoil(idle);
	EXPECT_FALSE(_system.IsFighting(idle));
}

// The winner of a duel knocked out makes the won-fight reaction, stamped, for its player (the neutral player's creature
// too); a creature of no player makes none
TEST_F(CreatureFightTurnsTest, WinningAFightImpressesThoseWhoSeeItForTheWinnersPlayer)
{
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::reactionSystem> keepReactions;
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto& registry = test::creature_world::World::Registry();
	namespace reactions = ecs::effects::reactions;
	{
		const auto [a, b] = Duel();
		_system.KnockOut(a);
		const auto id = reactions::GetReactionOfTypeInitiatedBy(b, openblack::Reaction::ReactToFightWon);
		ASSERT_NE(id, 0u);
		EXPECT_EQ(reactions::Find(id)->player, std::optional(PlayerNames::PLAYER_THREE));
		EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(a, openblack::Reaction::ReactToFightWon), 0u);
	}
	{
		const auto [a, b] = Duel();
		registry.Get<Creature>(b).owner = PlayerNames::NEUTRAL;
		_system.KnockOut(a);
		const auto id = reactions::GetReactionOfTypeInitiatedBy(b, openblack::Reaction::ReactToFightWon);
		ASSERT_NE(id, 0u);
		EXPECT_EQ(reactions::Find(id)->player, std::optional(PlayerNames::NEUTRAL));
	}
	{
		const auto [a, b] = Duel();
		registry.Get<Creature>(b).owner = PlayerNames::_COUNT;
		_system.KnockOut(a);
		EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(b, openblack::Reaction::ReactToFightWon), 0u);
	}
}

// A miracle's harm that takes the last of a fighter's fight health knocks it out, and its opponent wins the fight; a heal
// goes on the fight health
TEST_F(CreatureFightTurnsTest, AMiracleThatTakesTheLastFightHealthLosesTheFight)
{
	// the faint draws on the synced stream
	const game_random::testing::ScopedState state;
	const auto [a, b] = Duel();
	const auto& lookup = std::as_const(test::creature_world::World::Registry());
	FightingOf(a).fighter.health = 0.3f;
	_system.TakeMiracle(a, 0.2f, 0.1f);
	EXPECT_FLOAT_EQ(FightingOf(a).fighter.health, 0.4f);
	EXPECT_TRUE(_system.IsFighting(a));
	_system.TakeMiracle(a, 0.0f, 0.4f);
	EXPECT_TRUE(_system.IsKnockedOut(a));
	EXPECT_FALSE(_system.IsFighting(a));
	const auto* record = lookup.TryGet<const CreatureFightRecord>(b);
	ASSERT_NE(record, nullptr);
	EXPECT_EQ(record->wins, 1u);
	// not in a fight, nothing
	const auto idle = test::creature_world::World::MakeCreature();
	_system.TakeMiracle(idle, 0.0f, 1.0f);
	EXPECT_FALSE(_system.IsKnockedOut(idle));
}

TEST_F(CreatureFightTurnsTest, FaintingWhereItStandsLeavesTheFightUnlost)
{
	// the faint draws on the synced stream
	const game_random::testing::ScopedState state;
	const auto [a, b] = Duel();
	auto& registry = test::creature_world::World::Registry();
	const auto& lookup = std::as_const(registry);
	_system.ForceFaint(a);
	EXPECT_TRUE(_system.IsKnockedOut(a));
	EXPECT_FALSE(_system.IsFighting(a));
	// its opponent is not given the win
	EXPECT_FALSE(_system.IsKnockedOut(b));
	const auto* record = lookup.TryGet<const CreatureFightRecord>(b);
	ASSERT_NE(record, nullptr);
	EXPECT_EQ(record->wins, 0u);
	// fainted already, nothing more
	const auto stage = lookup.Get<const CreatureKnockedOut>(a).stage;
	_system.ForceFaint(a);
	EXPECT_EQ(lookup.Get<const CreatureKnockedOut>(a).stage, stage);
	// outside a fight it faints all the same
	const auto alone = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_TWO);
	_system.ForceFaint(alone);
	EXPECT_TRUE(_system.IsKnockedOut(alone));
}

TEST_F(CreatureFightTurnsTest, TheFightHealthAFightEndsWithStaysWhenItIsLeft)
{
	// the faint draws on the synced stream
	const game_random::testing::ScopedState state;
	const auto [a, b] = Duel();
	const auto& lookup = std::as_const(test::creature_world::World::Registry());
	FightingOf(a).fighter.health = 0.3f;
	FightingOf(b).fighter.health = 0.8f;
	_system.AbortFight(a);
	// ended but not left yet: the fighter's health is the one in play, a script's set included
	ASSERT_TRUE(_system.IsFighting(a));
	EXPECT_FALSE(lookup.AllOf<CreatureFightHealth>(a));
	FightingOf(a).fighter.health = 1.0f;
	_system.ForceFaint(a);
	ASSERT_FALSE(_system.IsFighting(a));
	EXPECT_FLOAT_EQ(lookup.Get<const CreatureFightHealth>(a).health, 1.0f);
	// its opponent keeps fighting with its own
	EXPECT_TRUE(_system.IsFighting(b));
	EXPECT_FALSE(lookup.AllOf<CreatureFightHealth>(b));
}

TEST_F(CreatureFightTurnsTest, AFightLeftBeforeItsEndKeepsItsFightHealthToo)
{
	// the faint draws on the synced stream
	const game_random::testing::ScopedState state;
	const auto [a, b] = Duel();
	const auto& lookup = std::as_const(test::creature_world::World::Registry());
	FightingOf(a).fighter.health = 0.4f;
	// fainting where it stands leaves the duel unended
	_system.ForceFaint(a);
	ASSERT_FALSE(_system.IsFighting(a));
	EXPECT_FLOAT_EQ(lookup.Get<const CreatureFightHealth>(a).health, 0.4f);
	// a fight given up on the way keeps the health it started with
	const auto c = test::creature_world::World::MakeCreature(glm::vec3(300.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto d = test::creature_world::World::MakeCreature(glm::vec3(300.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
	ASSERT_EQ(_system.StartFight(c, d), StartResult::Started);
	const auto start = FightingOf(c).fighter.health;
	_system.AbortFight(c);
	ASSERT_FALSE(_system.IsFighting(c));
	ASSERT_FALSE(_system.IsFighting(d));
	EXPECT_FLOAT_EQ(lookup.Get<const CreatureFightHealth>(c).health, start);
}

TEST_F(CreatureFightTurnsTest, ACreatureWithdrawnOnItsWayLeavesAtOnceAndSoDoesItsOpponent)
{
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 60.0f), PlayerNames::PLAYER_THREE);
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	ASSERT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Approach);
	const auto start = FightingOf(a).fighter.health;
	_system.Withdraw(a);
	const auto& lookup = std::as_const(test::creature_world::World::Registry());
	EXPECT_FALSE(lookup.AllOf<CreatureFighting>(a));
	EXPECT_FLOAT_EQ(lookup.Get<const CreatureFightHealth>(a).health, start);
	EXPECT_FALSE(_system.IsFighting(b));
	EXPECT_TRUE(_system.GetArenas().empty());
}

TEST_F(CreatureFightTurnsTest, ACreatureWithdrawnFromADuelLeavesAtOnceAndItsOpponentFinishesAlone)
{
	// the duel's end draws on the synced stream
	const game_random::testing::ScopedState state;
	const auto [a, b] = Duel();
	FightingOf(a).fighter.health = 0.3f;
	_system.Withdraw(a);
	const auto& lookup = std::as_const(test::creature_world::World::Registry());
	// it leaves rather than finishing and standing, with the health it had
	EXPECT_FALSE(lookup.AllOf<CreatureFighting>(a));
	EXPECT_FLOAT_EQ(lookup.Get<const CreatureFightHealth>(a).health, 0.3f);
	// its opponent plays out the end of its fight, and has forgotten it
	ASSERT_TRUE(_system.IsFighting(b));
	EXPECT_EQ(lookup.Get<const CreatureFighting>(b).opponent, entt::entity(entt::null));
	EXPECT_EQ(_system.OpponentOf(b), std::optional(entt::entity(entt::null)));
}

TEST_F(CreatureFightTurnsTest, WithdrawingACreatureNotFightingDoesNothing)
{
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	_system.Withdraw(a);
	const auto& lookup = std::as_const(test::creature_world::World::Registry());
	EXPECT_FALSE(lookup.AllOf<CreatureFighting>(a));
	EXPECT_FALSE(lookup.AllOf<CreatureFightHealth>(a));
}

TEST_F(CreatureFightTurnsTest, FaintingBringsTheSpellsOnItToTheirEnd)
{
	// the faint draws on the synced stream
	const game_random::testing::ScopedState state;
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_TWO);
	auto& registry = test::creature_world::World::Registry();
	auto& spells = registry.AssignState<CreatureSpells>(creature).spells;
	auto& slot = spells[creature_spells::Spell::Big];
	slot.phase = creature_spells::Phase::Holding;
	slot.turnsLeft = 100;
	slot.holdTurns = 100;
	_system.KnockOut(creature);
	ASSERT_TRUE(_system.IsKnockedOut(creature));
	const auto& after = std::as_const(registry).Get<const CreatureSpells>(creature).spells[creature_spells::Spell::Big];
	EXPECT_EQ(after.turnsLeft, 0);
	EXPECT_EQ(after.holdTurns, 0);
}

namespace
{
/// How many storages the registry has, which a read through the const registry must not change
size_t StorageCount(const ecs::Registry& registry)
{
	size_t count = 0;
	registry.EachStorage([&count](entt::id_type /*id*/, const entt::sparse_set& /*storage*/) { ++count; });
	return count;
}
} // namespace

TEST_F(CreatureFightTurnsTest, TheArenasAreListedNewestFirst)
{
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
	const auto c = test::creature_world::World::MakeCreature(glm::vec3(300.0f, 0.0f, 300.0f), PlayerNames::PLAYER_TWO);
	const auto d = test::creature_world::World::MakeCreature(glm::vec3(340.0f, 0.0f, 300.0f), PlayerNames::PLAYER_THREE);
	EXPECT_TRUE(_system.GetArenas().empty());
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	ASSERT_EQ(_system.StartFight(d, c), StartResult::Started);
	const auto arenas = _system.GetArenas();
	ASSERT_EQ(arenas.size(), 2u);
	// the one that made each arena first, its opponent second
	EXPECT_EQ(arenas[0].maker, d);
	EXPECT_EQ(arenas[0].opponent, c);
	EXPECT_EQ(arenas[1].maker, a);
	EXPECT_EQ(arenas[1].opponent, b);
	for (const auto& arena : arenas)
	{
		const auto& made = FightingOf(arena.maker).arena;
		EXPECT_EQ(arena.centre, glm::vec2(map_coords::Quantise(made.centre.x), map_coords::Quantise(made.centre.y)));
		EXPECT_FLOAT_EQ(arena.radius, made.radius);
		// both fighters keep the same arena
		EXPECT_EQ(FightingOf(arena.opponent).arena.serial, made.serial);
	}
	EXPECT_EQ(arenas[0].centre, glm::vec2(320.0f, 300.0f));
	EXPECT_EQ(arenas[1].centre, glm::vec2(100.0f, 90.0f));
	EXPECT_GT(FightingOf(d).arena.serial, FightingOf(a).arena.serial);
}

TEST_F(CreatureFightTurnsTest, TheArenasAreListedNewestFirstWhateverTheStorageOrder)
{
	std::vector<entt::entity> makers;
	for (int i = 0; i < 4; ++i)
	{
		const auto x = 100.0f + 200.0f * static_cast<float>(i);
		const auto maker = test::creature_world::World::MakeCreature(glm::vec3(x, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
		const auto opponent = test::creature_world::World::MakeCreature(glm::vec3(x, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
		ASSERT_EQ(_system.StartFight(maker, opponent), StartResult::Started);
		makers.push_back(maker);
	}
	// calling off the second fight fills its places in the storage with the newest ones
	_system.AbortFight(makers[1]);
	ASSERT_FALSE(_system.IsFighting(makers[1]));
	std::vector<uint32_t> stored;
	std::as_const(test::creature_world::World::Registry())
	    .Each<const CreatureFighting>([&stored](entt::entity, const CreatureFighting& fighting) {
		    if (fighting.madeArena)
		    {
			    stored.push_back(fighting.arena.serial);
		    }
	    });
	ASSERT_EQ(stored.size(), 3u);
	// the storage alone, either way round, would not list them in order
	ASSERT_FALSE(std::ranges::is_sorted(stored));
	ASSERT_FALSE(std::ranges::is_sorted(stored, std::ranges::greater {}));
	const auto arenas = _system.GetArenas();
	ASSERT_EQ(arenas.size(), 3u);
	EXPECT_EQ(arenas[0].maker, makers[3]);
	EXPECT_EQ(arenas[1].maker, makers[2]);
	EXPECT_EQ(arenas[2].maker, makers[0]);
}

TEST_F(CreatureFightTurnsTest, AnArenasMiddleIsGivenAsAMapPosition)
{
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.03f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.04f, 0.0f, 80.01f), PlayerNames::PLAYER_THREE);
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	const auto arena = _system.ArenaOf(a);
	ASSERT_TRUE(arena.has_value());
	const auto& made = FightingOf(a).arena.centre;
	EXPECT_EQ(arena->centre.x, map_coords::ToMetres(map_coords::ToFixed(made.x)));
	EXPECT_EQ(arena->centre.y, map_coords::ToMetres(map_coords::ToFixed(made.y)));
	EXPECT_NE(arena->centre.x, made.x);
}

TEST_F(CreatureFightTurnsTest, AnArenaIsFoundByEitherFighterOnly)
{
	const auto [a, b] = Duel();
	const auto idle = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_FOUR);
	for (const auto fighter : {a, b})
	{
		const auto arena = _system.ArenaOf(fighter);
		ASSERT_TRUE(arena.has_value());
		EXPECT_EQ(arena->maker, a);
		EXPECT_EQ(arena->opponent, b);
		EXPECT_FLOAT_EQ(arena->radius, FightingOf(a).arena.radius);
	}
	EXPECT_FALSE(_system.ArenaOf(idle).has_value());
	EXPECT_FALSE(_system.ArenaOf(entt::null).has_value());
}

TEST_F(CreatureFightTurnsTest, AnArenaGoesWithTheFightOfTheCreatureThatMadeIt)
{
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
	const auto c = test::creature_world::World::MakeCreature(glm::vec3(300.0f, 0.0f, 300.0f), PlayerNames::PLAYER_TWO);
	const auto d = test::creature_world::World::MakeCreature(glm::vec3(340.0f, 0.0f, 300.0f), PlayerNames::PLAYER_THREE);
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	ASSERT_EQ(_system.StartFight(c, d), StartResult::Started);
	// called off while they walk to their places: both leave, and their arena with them
	_system.AbortFight(b);
	ASSERT_FALSE(_system.IsFighting(a));
	auto arenas = _system.GetArenas();
	ASSERT_EQ(arenas.size(), 1u);
	EXPECT_EQ(arenas[0].maker, c);
	EXPECT_FALSE(_system.ArenaOf(a).has_value());
	EXPECT_FALSE(_system.ArenaOf(b).has_value());
	// the maker gone from its fight takes the arena, though its opponent is still in it
	test::creature_world::World::Registry().RemoveState<CreatureFighting>(c);
	ASSERT_TRUE(_system.IsFighting(d));
	EXPECT_TRUE(_system.GetArenas().empty());
	EXPECT_FALSE(_system.ArenaOf(d).has_value());
}

TEST_F(CreatureFightTurnsTest, AskingForTheArenasMakesNoStorage)
{
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_TWO);
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	const auto before = StorageCount(registry);
	EXPECT_TRUE(_system.GetArenas().empty());
	EXPECT_FALSE(_system.ArenaOf(creature).has_value());
	EXPECT_EQ(StorageCount(registry), before);
}

TEST_F(CreatureFightTurnsTest, InItsPenABlowIsChosenByTheSizesTheFightersAreDrawnAt)
{
	const game_random::testing::ScopedState state;
	_animation->durations[fight::animations::k_FirstAttack] = 1000.0f;
	_animation->durations[fight::animations::k_StepForward] = 1000.0f;
	// One blow reaching 3 into the opponent and 5 up its body: within 7.5 of the attacker's size, below the opponent's
	// height of 15 of its size
	const auto chosen = [this](bool attackerInPen, bool opponentInPen) {
		const auto [a, b] = Duel();
		if (attackerInPen)
		{
			InPen(a);
		}
		if (opponentInPen)
		{
			InPen(b);
		}
		const auto gap = 20.0f - test::creature_world::World::Registry().Get<CreatureLocomotion>(b).radius;
		QueueBlow(a, gap + 3.0f, 5.0f);
		_system.ProcessTurn();
		const auto& fighter = FightingOf(a).fighter;
		EXPECT_EQ(fighter.state, fight::State::Action);
		const auto animation = fighter.animation;
		_system.AbortFight(a);
		return animation;
	};
	EXPECT_EQ(chosen(false, false), fight::animations::k_FirstAttack);
	// In its pen it reaches no deeper than 7.5 of the pen's size, so it steps in first
	EXPECT_EQ(chosen(true, false), fight::animations::k_StepForward);
	// An opponent in its pen is 15 of the pen's size tall, so the blow passes over it
	EXPECT_EQ(chosen(false, true), fight::animations::k_StepForward);
}

TEST_F(CreatureFightTurnsTest, InItsPenABlowLandsAndHurtsByTheSizeItIsDrawnAt)
{
	const game_random::testing::ScopedState state;
	_animation->durations[fight::animations::k_FirstAttack] = 1000.0f;
	struct Landed
	{
		float before;
		float health;
		float expected;
	};
	// A blow 5 up the opponent's body, landing as the turn starts it, short of the opponent's radius or in its middle:
	// the health before and after, and the health a blow by the size the attacker is drawn at leaves
	const auto blow = [this](bool inPen, std::optional<float> shortOfRadius) {
		const auto [a, b] = Duel();
		const auto& lookup = std::as_const(test::creature_world::World::Registry());
		if (inPen)
		{
			InPen(a);
		}
		const auto& attacker = lookup.Get<const Creature>(a);
		const auto& defender = lookup.Get<const Creature>(b);
		const auto share = 5.0f / (creature_feedback::k_HeightAtSizeOne * defender.size);
		const auto damage = fight::ResolveBlow({.attackerSize = inPen ? k_PenSize : attacker.size,
		                                        .defenderSize = defender.size,
		                                        .attackerStrength = ecs::creature_pose::TargetMorph(lookup, a).weakStrong,
		                                        .defenderStrength = ecs::creature_pose::TargetMorph(lookup, b).weakStrong,
		                                        .heightShare = share},
		                                       fight::RecoilDirectionOf({0.0f, (share - 0.5f) * 2.0f}))
		                        .damage;
		auto& fighting = FightingOf(a);
		const auto radius = lookup.Get<const CreatureLocomotion>(b).radius;
		const auto reach = shortOfRadius.has_value() ? 20.0f - radius - *shortOfRadius : 20.0f;
		fighting.reaches = {fight::Reach {.animation = fight::animations::k_FirstAttack, .reach = reach, .height = 5.0f}};
		fight::Enter(fighting.fighter, fight::State::Action, fight::animations::k_FirstAttack);
		const auto before = FightingOf(b).fighter.health;
		_system.ProcessTurn();
		const auto after = FightingOf(b).fighter.health;
		_system.AbortFight(a);
		return Landed {.before = before, .health = after, .expected = std::max(before - damage, 0.0f)};
	};
	// 2 short of its radius lands within 0.2 x 15 of the attacker's own size, not within that of the pen's
	const auto near = blow(false, 2.0f);
	EXPECT_FLOAT_EQ(near.health, near.expected);
	EXPECT_LT(near.health, near.before);
	const auto missed = blow(true, 2.0f);
	EXPECT_EQ(missed.health, missed.before);
	// In the middle of the opponent it lands either way, and hurts by the pen's size against the opponent's
	const auto own = blow(false, std::nullopt);
	const auto pen = blow(true, std::nullopt);
	EXPECT_FLOAT_EQ(own.health, own.expected);
	EXPECT_FLOAT_EQ(pen.health, pen.expected);
	EXPECT_GT(pen.health, own.health);
}

TEST_F(CreatureFightTurnsTest, ScriptsCountAFightOnlyWhileItDuels)
{
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 60.0f), PlayerNames::PLAYER_THREE);
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	// walking to its place it is in a fight, but not yet fighting for the scripts
	EXPECT_TRUE(_system.IsFighting(a));
	EXPECT_FALSE(_system.IsDuelling(a));
	FightingOf(a).stage = CreatureFighting::Stage::Duel;
	EXPECT_TRUE(_system.IsDuelling(a));
	EXPECT_FALSE(_system.IsDuelling(b));
	// once its fight has ended it no longer is, though it still finishes
	FightingOf(b).stage = CreatureFighting::Stage::Duel;
	_system.AbortFight(a);
	EXPECT_TRUE(_system.IsFighting(a));
	EXPECT_FALSE(_system.IsDuelling(a));
	EXPECT_FALSE(_system.IsDuelling(b));
	EXPECT_FALSE(_system.IsDuelling(test::creature_world::World::Registry().Create()));
}

TEST_F(CreatureFightTurnsTest, NobodyFightsForACreatureAScriptLeftAlone)
{
	const game_random::testing::ScopedState state;
	game_random::testing::SetGameRand([](uint32_t /*n*/) { return 0u; }, [](float) { return 0.0f; });
	const auto [a, b] = Duel();
	for (const auto self : {a, b})
	{
		_system.SetAutoFighting(self, false);
		EXPECT_EQ(FightingOf(self).fighter.control, fight::Control::None);
		EXPECT_FALSE(_system.IsAutoFighting(self));
	}
	const auto wait = FightingOf(a).fighter.computerWaitMs;
	for (int turn = 0; turn < 50; ++turn)
	{
		_system.ProcessTurn();
	}
	// neither chose a move, and the computer never began to count down to taking over
	for (const auto self : {a, b})
	{
		EXPECT_EQ(FightingOf(self).fighter.state, fight::State::Stance);
		EXPECT_EQ(FightingOf(self).fighter.control, fight::Control::None);
	}
	EXPECT_FLOAT_EQ(FightingOf(a).fighter.computerWaitMs, wait);
	// a block a script queues is made, and still nobody fights for it
	EXPECT_TRUE(_system.QueueScriptMove(a, fight::BlockMove()));
	EXPECT_EQ(FightingOf(a).fighter.state, fight::State::BlockStart);
	EXPECT_EQ(_system.CurrentFightAction(a), fight::FightAction::Block);
	EXPECT_EQ(FightingOf(a).fighter.control, fight::Control::None);
	// switched on, the computer fights for it
	_system.SetAutoFighting(a, true);
	EXPECT_EQ(FightingOf(a).fighter.control, fight::Control::Computer);
	EXPECT_TRUE(_system.IsAutoFighting(a));
}

TEST_F(CreatureFightTurnsTest, AScriptsBlowsWaitInTheQueueAndTeachAnotherGodsCreatureNothing)
{
	const auto [a, b] = Duel();
	const auto tendency = FightingOf(a).fighter.tendency;
	// a blow waits for a charge only the hand gives, and the block behind it waits too
	EXPECT_TRUE(_system.QueueScriptMove(a, fight::AttackMove(fight::Band::Mid)));
	EXPECT_TRUE(_system.QueueScriptMove(a, fight::BlockMove()));
	EXPECT_EQ(_system.QueuedBlows(a), 1u);
	EXPECT_EQ(FightingOf(a).fighter.queue.Size(), 2u);
	EXPECT_EQ(FightingOf(a).fighter.state, fight::State::Stance);
	EXPECT_EQ(_system.CurrentFightAction(a), fight::FightAction::Other);
	EXPECT_FLOAT_EQ(FightingOf(a).fighter.tendency, tendency);
	EXPECT_EQ(_system.QueuedBlows(b), 0u);
	// not fighting, nothing is queued
	const auto idle = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_TWO);
	EXPECT_FALSE(_system.QueueScriptMove(idle, fight::BlockMove()));
	EXPECT_EQ(_system.QueuedBlows(idle), 0u);
}

TEST_F(CreatureFightTurnsTest, WhatAScriptIsToldACreatureDoes)
{
	// the faint draws on the synced stream
	const game_random::testing::ScopedState state;
	const auto [a, b] = Duel();
	EXPECT_EQ(_system.CurrentFightAction(a), fight::FightAction::Other);
	fight::Enter(FightingOf(a).fighter, fight::State::Action, fight::animations::k_AttackHigh);
	EXPECT_EQ(_system.CurrentFightAction(a), fight::FightAction::Blow);
	fight::Enter(FightingOf(b).fighter, fight::State::Action, fight::animations::k_RecoilLow);
	EXPECT_EQ(_system.CurrentFightAction(b), fight::FightAction::ReelingLow);
	// out of a fight: knocked out, fainting; otherwise nothing a fight counts
	const auto alone = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_TWO);
	EXPECT_EQ(_system.CurrentFightAction(alone), fight::FightAction::Other);
	_system.ForceFaint(alone);
	EXPECT_EQ(_system.CurrentFightAction(alone), fight::FightAction::Fainting);
	EXPECT_EQ(_system.CurrentFightAction(test::creature_world::World::Registry().Create()), fight::FightAction::Other);
}

TEST_F(CreatureFightTurnsTest, AFighterIsNamedByItsActionAndAKnockedOutCreatureByItsFaint)
{
	const auto a = Duel().first;
	// in its stance, the stand pose; in an action, the action
	EXPECT_EQ(_system.ActionAnimation(a), fight::k_NoActionAnimation);
	fight::Enter(FightingOf(a).fighter, fight::State::Action, fight::animations::k_FirstAttack);
	EXPECT_EQ(_system.ActionAnimation(a), fight::animations::k_FirstAttack);
	// out of a fight, nothing
	const auto alone = test::creature_world::World::MakeCreature(glm::vec3(300.0f, 0.0f, 300.0f));
	EXPECT_FALSE(_system.ActionAnimation(alone).has_value());
	// knocked out, its faint until it gets up
	_system.ForceFaint(alone);
	EXPECT_EQ(_system.ActionAnimation(alone), fight::animations::k_Faint);
	test::creature_world::World::Registry().Get<CreatureKnockedOut>(alone).stage = CreatureKnockedOut::Stage::GettingUp;
	EXPECT_EQ(_system.ActionAnimation(alone), fight::animations::k_GetUp);
}
