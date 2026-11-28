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

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <memory>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/Guidance.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Camera/FightWatch.h"
#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureArenaRing.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureFightHud.h"
#include "Creature/CreatureLean.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureTraining.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureBodyMetrics.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureNaming.h"
#include "ECS/CreaturePose.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/RegistryContext.h"
#include "ECS/SavedWorld/WriteCreatures.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/Implementations/CreatureFightSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/Implementations/ReactionSystem.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Input/InterfaceActive.h"
#include "Particles/LightSheet.h"
#include "creature/CreatureSystemFakes.h"
#include "creature/CreatureSystemWorld.h"
#include "support/CreatureFakes.h"
#include "support/FireFakes.h"
#include "support/LandFakes.h"
#include "support/ParticleFakes.h"
#include "support/WalkableLand.h"

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
		// walkable land for the arenas' ground
		test::MakeWalkableLand();
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

	/// Plays a fight's steps on, a turn at a time, until its arena has appeared at the end of the first creature's taunt
	/// (or the fight has stopped without one): each creature already faces the other, and its taunt or rude gesture
	/// ends on the turn it starts
	void ToArena(entt::entity first, entt::entity second)
	{
		auto& registry = test::creature_world::World::Registry();
		_animation->durations.try_emplace(fight::animations::k_Taunt, 1000.0f);
		_animation->durations.try_emplace(fight::animations::k_RudeGesture, 1000.0f);
		for (const auto& [self, other] : {std::pair(first, second), std::pair(second, first)})
		{
			const auto& here = registry.Get<const Transform>(self).position;
			const auto& there = registry.Get<const Transform>(other).position;
			registry.Get<CreatureLocomotion>(self).heading =
			    creature_locomotion::HeadingFromDirection(glm::vec2(there.x - here.x, there.z - here.z));
		}
		const auto waits = [&registry, first]() {
			return registry.AllOf<CreatureFighting>(first) && FightingOf(first).awaitsArena;
		};
		for (int turn = 0; turn < 40 && waits(); ++turn)
		{
			_system.ProcessTurn();
			for (const auto self : {first, second})
			{
				if (registry.AllOf<CreatureFighting>(self) && FightingOf(self).stage == CreatureFighting::Stage::Taunt)
				{
					registry.Get<CreatureAnimation>(self).body = {};
				}
			}
		}
		EXPECT_FALSE(waits());
	}

	/// A fight started and played on until its arena has appeared
	StartResult StartToArena(entt::entity first, entt::entity second)
	{
		const auto result = _system.StartFight(first, second);
		if (result == StartResult::Started)
		{
			ToArena(first, second);
		}
		return result;
	}

	/// A fight started and played on, turn by turn, until both fighters have started their duels, their start, stance
	/// and finish lasting so that the duels go no further
	StartResult StartToDuel(entt::entity first, entt::entity second)
	{
		for (const auto animation : {fight::animations::k_Start, fight::animations::k_Stance, fight::animations::k_Finish})
		{
			_animation->durations[animation] = 100000.0f;
		}
		// a fight a test has already started and taken to its arena is played on; any other is started first
		const auto result = test::creature_world::World::Registry().AllOf<CreatureFighting>(first)
		                        ? StartResult::Started
		                        : StartToArena(first, second);
		if (result != StartResult::Started)
		{
			return result;
		}
		const auto duelling = [this](entt::entity creature) {
			return FightingOf(creature).stage == CreatureFighting::Stage::Duel;
		};
		for (int turn = 0; turn < 400 && !(duelling(first) && duelling(second)); ++turn)
		{
			_system.ProcessTurn();
		}
		EXPECT_TRUE(duelling(first));
		EXPECT_TRUE(duelling(second));
		return result;
	}

	/// Two creatures duelling, facing each other 20 apart along z, both fought by the computer
	std::pair<entt::entity, entt::entity> Duel()
	{
		const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
		const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
		EXPECT_EQ(StartToArena(a, b), StartResult::Started);
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
		timeMs = fight::AdvanceClock(timeMs, stepMs, fight::animations::k_FirstAttack);
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
	[[nodiscard]] bool WantToQuitFight(glm::vec3 arenaCentre, float /*arenaRadius*/) const override
	{
		++asked;
		askedCentre = arenaCentre;
		return tooFar;
	}

	std::vector<Started> started;
	int ended {0};
	int endedNow {0};
	bool watching {false};
	bool tooFar {true};
	mutable int asked {0};
	mutable glm::vec3 askedCentre {0.0f};
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
/// The help lines raised, in order
class RaisedHelp final: public ecs::systems::CreatureHelpSystemInterface
{
public:
	void Raise(entt::entity /*creature*/, const creature_help::Message& message) override
	{
		raised.push_back(message.parameter);
	}
	void ProcessTurn() override {}
	void PromptKnown(entt::entity /*creature*/, creature_help::Prompt /*prompt*/) override {}
	void Fainting(entt::entity /*creature*/) override {}
	void SetHelpOn(bool /*on*/) override {}
	[[nodiscard]] bool IsHelpOn() const override { return true; }
	[[nodiscard]] const std::vector<Told>& GetTold() const override { return _told; }

	std::vector<uint32_t> raised;

private:
	std::vector<Told> _told;
};
} // namespace

// As a fight begins the local player directs their own creature and the computer fights for every other, a computer
// god's creature or a skirmish opponent's alike (docs/wiki/creature/fights.md, the builder's settings;
// docs/wiki/project/progress/rival_gods/creatures.md, a computer god's creature fights with the computer's moves)
TEST_F(CreatureFightTurnsTest, AsAFightBeginsOnlyTheLocalPlayersCreatureIsThePlayersAndTheComputerFightsForTheRest)
{
	const auto own = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_ONE);
	const auto rival = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 60.0f), PlayerNames::PLAYER_TWO);
	ASSERT_EQ(_system.StartFight(own, rival), StartResult::Started);
	EXPECT_EQ(FightingOf(own).fighter.control, fight::Control::Player);
	EXPECT_EQ(FightingOf(rival).fighter.control, fight::Control::Computer);
	// two other gods' creatures: the computer fights for both
	const auto c = test::creature_world::World::MakeCreature(glm::vec3(300.0f, 0.0f, 100.0f), PlayerNames::PLAYER_THREE);
	const auto d = test::creature_world::World::MakeCreature(glm::vec3(300.0f, 0.0f, 60.0f), PlayerNames::PLAYER_FOUR);
	ASSERT_EQ(_system.StartFight(c, d), StartResult::Started);
	EXPECT_EQ(FightingOf(c).fighter.control, fight::Control::Computer);
	EXPECT_EQ(FightingOf(d).fighter.control, fight::Control::Computer);
}

// One creature at a time: a fighter's turn of its fight goes on before its opponent's, which its own part leaves as it
// was; the opponent's turn then goes on in its own part
TEST_F(CreatureFightTurnsTest, OneFightersTurnGoesOnBeforeItsOpponents)
{
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 60.0f), PlayerNames::PLAYER_THREE);
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	const auto& lookup = std::as_const(test::creature_world::World::Registry());
	const auto stageSecondsOf = [&lookup](entt::entity creature) {
		return lookup.Get<const CreatureFighting>(creature).stageSeconds;
	};
	const auto aBefore = stageSecondsOf(a);
	const auto bBefore = stageSecondsOf(b);

	_system.BeginCreatureTurns();
	_system.ProcessCreatureTurn(a);
	EXPECT_GT(stageSecondsOf(a), aBefore);
	EXPECT_EQ(stageSecondsOf(b), bBefore);
	_system.ProcessCreatureTurn(b);
	EXPECT_GT(stageSecondsOf(b), bBefore);
	_system.EndCreatureTurns();
}

TEST_F(CreatureFightTurnsTest, WhatStartsAFight)
{
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 60.0f), PlayerNames::PLAYER_THREE);
	EXPECT_EQ(_system.StartFight(a, a), StartResult::NoOpponent);
	EXPECT_EQ(_system.StartFight(a, test::creature_world::World::Registry().Create()), StartResult::NoOpponent);
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	EXPECT_TRUE(_system.IsFighting(a));
	EXPECT_EQ(_system.OpponentOf(b), std::optional(a));
	// only the first creature stopped what it was doing; the other carries on until its first order
	EXPECT_EQ(_locomotion->stopped, (std::vector<entt::entity> {a}));
	EXPECT_EQ(_hands->cancelled, (std::vector<entt::entity> {a}));
	EXPECT_EQ(_system.StartFight(b, a), StartResult::Busy);
	// a creature too hurt to fight still starts one: the fight's start has no health test, only the fight plan's
	// planner has
	const auto c = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_TWO);
	const auto d = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_THREE);
	auto& needs = test::creature_world::World::Registry().Get<CreatureNeeds>(c);
	needs.needs.life = 0.0f;
	EXPECT_EQ(_system.StartFight(c, d), StartResult::Started);
	_system.AbortFight(c);
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

// Asked to fight, the asker's partner already agrees, and a creature with another partner refuses
// A magic fireball is no creature to fight: a fight started on one, either way round, has no opponent
TEST_F(CreatureFightTurnsTest, NoFightStartsWithAMagicFireball)
{
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	auto& registry = test::creature_world::World::Registry();
	const auto ball = registry.Create();
	registry.Assign<ecs::components::Transform>(ball, glm::vec3(100.0f, 5.0f, 90.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::MagicFireBall>(ball);
	EXPECT_EQ(_system.StartFight(a, ball), StartResult::NoOpponent);
	EXPECT_EQ(_system.StartFight(ball, a), StartResult::NoOpponent);
	EXPECT_FALSE(_system.IsFighting(a));
}

TEST_F(CreatureFightTurnsTest, APartnerAgreesToFightAndOneWithAnotherPartnerRefuses)
{
	auto& registry = test::creature_world::World::Registry();
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 60.0f), PlayerNames::PLAYER_THREE);
	const auto c = test::creature_world::World::MakeCreature(glm::vec3(-100.0f, 0.0f, 0.0f), PlayerNames::PLAYER_THREE);
	registry.AssignOrReplaceState<CreaturePartner>(b, CreaturePartner {.partner = c});
	EXPECT_EQ(_system.StartFight(a, b), StartResult::Refused);
	EXPECT_FALSE(_system.IsFighting(a));
	registry.AssignOrReplaceState<CreaturePartner>(b, CreaturePartner {.partner = a});
	EXPECT_EQ(_system.StartFight(a, b), StartResult::Started);
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

// The arena is sized by the bigger body as it is drawn: by the pen's size for creatures drawn in their temple's pen
TEST_F(CreatureFightTurnsTest, TheArenaIsSizedByTheBodiesAsDrawn)
{
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
	InPen(a);
	InPen(b);
	ASSERT_EQ(StartToArena(a, b), StartResult::Started);
	const auto arena = _system.ArenaOf(a);
	ASSERT_TRUE(arena.has_value());
	EXPECT_EQ(arena->radius, fight::ArenaRadius(k_PenSize, k_PenSize));
	EXPECT_NE(arena->radius, fight::ArenaRadius(test::creature_world::World::Registry().Get<Creature>(a).size,
	                                            test::creature_world::World::Registry().Get<Creature>(b).size));
}

// The fight's end takes a quarter of the fight health it does not have off its life, with nothing kept within 0 and 1:
// a fight health below none costs more than a quarter, and the life may go below none. An indestructible creature keeps
// its life against one of a hundredth or less
TEST_F(CreatureFightTurnsTest, TheFightsEndPaysForAFightHealthBelowNoneAndTheLifeMayGoBelowNone)
{
	auto& registry = test::creature_world::World::Registry();
	const auto [a, b] = Duel();
	auto& needs = registry.Get<CreatureNeeds>(a);
	needs.needs.life = 0.2f;
	FightingOf(a).fighter.health = -0.2f;
	fight::Enter(FightingOf(a).fighter, fight::State::Stance);
	_system.EndOwnFight(a);
	EXPECT_FLOAT_EQ(needs.needs.life, -0.1f);

	auto& other = registry.Get<CreatureNeeds>(b);
	other.needs.life = 0.2f;
	registry.Assign<Indestructible>(b);
	FightingOf(b).fighter.health = -0.2f;
	fight::Enter(FightingOf(b).fighter, fight::State::Stance);
	_system.EndOwnFight(b);
	EXPECT_FLOAT_EQ(other.needs.life, 0.2f);
}

// A fight called off in its duel costs each fighter life only if it is in the midst of the fight (starting, standing,
// in a move, a block or a cast); one finishing or fainting as it ends loses none
TEST_F(CreatureFightTurnsTest, AFightsEndCostsLifeOnlyInTheMidstOfTheFight)
{
	auto& registry = test::creature_world::World::Registry();
	const auto [a, b] = Duel();
	for (const auto self : {a, b})
	{
		registry.Get<CreatureNeeds>(self).needs.life = 1.0f;
		FightingOf(self).fighter.health = 0.6f;
	}
	fight::Enter(FightingOf(a).fighter, fight::State::Finish);
	fight::Enter(FightingOf(b).fighter, fight::State::Stance);
	_system.CallOff(a);
	EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(a).needs.life, 1.0f);
	EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(b).needs.life, 0.9f);
	// both ended all the same
	EXPECT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Respond);
	EXPECT_EQ(FightingOf(b).stage, CreatureFighting::Stage::Respond);
}

// A knock out in the duel by anything but a blow makes the winner show off at the loser, or have a poo on it when evil,
// as a plan forced on it with the priority 1 and carried out as an order
TEST_F(CreatureFightTurnsTest, AKnockOutForcesTheWinnersPlanForTheLoser)
{
	auto& registry = test::creature_world::World::Registry();
	for (const auto& [alignment, action] :
	     {std::pair(0.0f, std::string_view("ShowImpressiveAnimation")), std::pair(-0.6f, std::string_view("Poo"))})
	{
		_mind->activities.clear();
		const auto [a, b] = Duel();
		registry.Get<Creature>(b).alignment = alignment;
		_system.KnockOut(a);
		// the loser's faint is forced on it first, then the winner's plan
		ASSERT_EQ(_mind->activities.size(), 2U);
		EXPECT_TRUE(_mind->activities.front().first == a);
		EXPECT_EQ(_mind->activities.front().second.action, "Die");
		const auto& forcedOn = _mind->activities.back();
		EXPECT_TRUE(forcedOn.first == b);
		const auto& plan = forcedOn.second;
		EXPECT_EQ(plan.desire, creature_desires::Desire::ObeyPlayer);
		EXPECT_EQ(plan.action, action);
		EXPECT_TRUE(plan.actionObject == a);
		EXPECT_TRUE(plan.activityObject == std::optional(a));
		EXPECT_FLOAT_EQ(plan.score, 1.0f);
		const auto* mind = std::as_const(registry).TryGet<const CreatureMindState>(b);
		ASSERT_NE(mind, nullptr);
		EXPECT_TRUE(mind->commanded);
		EXPECT_TRUE(mind->fightEnded);
		EXPECT_EQ(FightingOf(b).stage, CreatureFighting::Stage::Celebrate);
	}
}

// A knock out stops the winner for its plan, which stops the loser, then the loser's faint stops it again: each stop
// ends the body's fight again in the state it was in, so with the two minds partners the winner's life pays twice and
// the loser's four times; with none, once and twice
TEST_F(CreatureFightTurnsTest, AKnockOutPaysTheWinnersLifeTwiceAndTheLosersFourTimesBetweenPartners)
{
	auto& registry = test::creature_world::World::Registry();
	const auto paid = [](float life, float health, int times) {
		for (int time = 0; time < times; ++time)
		{
			life = fight::LifeAfterFight(life, health);
		}
		return life;
	};
	for (const bool partnered : {true, false})
	{
		const auto [a, b] = Duel();
		if (partnered)
		{
			registry.AssignOrReplaceState<CreaturePartner>(a, CreaturePartner {.partner = b});
			registry.AssignOrReplaceState<CreaturePartner>(b, CreaturePartner {.partner = a});
		}
		registry.Get<CreatureNeeds>(a).needs.life = 1.0f;
		registry.Get<CreatureNeeds>(b).needs.life = 1.0f;
		FightingOf(a).fighter.health = -0.2f;
		FightingOf(b).fighter.health = 0.6f;
		_system.KnockOut(a);
		EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(a).needs.life, paid(1.0f, -0.2f, partnered ? 4 : 2));
		EXPECT_FLOAT_EQ(registry.Get<CreatureNeeds>(b).needs.life, paid(1.0f, 0.6f, partnered ? 2 : 1));
		// Paid below none, the loser does not die: nothing clamps the life or kills a creature for it, so it lies
		// fainted, as any knock-out's loser, until its rest brings it round
		if (partnered)
		{
			EXPECT_NEAR(registry.Get<CreatureNeeds>(a).needs.life, -0.2f, 1e-6f);
		}
		ASSERT_TRUE(registry.Valid(a));
		const auto* out = std::as_const(registry).TryGet<const CreatureKnockedOut>(a);
		ASSERT_NE(out, nullptr);
		EXPECT_FALSE(out->permanent);
		EXPECT_EQ(registry.Get<CreatureNeeds>(a).rest, CreatureNeeds::Rest::Unconscious);
	}
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

// A blow the player asks for waits in the queue until the button is let go, the body playing the power-up in its
// stance meanwhile; let go, the blow is taken and the power-up ends
TEST_F(CreatureFightTurnsTest, AHeldBlowPlaysThePowerUpInTheStanceUntilItIsLetGo)
{
	auto& registry = test::creature_world::World::Registry();
	const auto [a, b] = Duel();
	auto& fighting = FightingOf(a);
	fighting.reaches = {fight::Reach {.animation = fight::animations::k_FirstAttack, .reach = 1.0f, .height = 1.0f}};
	ASSERT_TRUE(fight::PlayerMove(fighting.fighter, fight::AttackMove(fight::Band::Mid), false));
	ASSERT_TRUE(fighting.fighter.queue.AwaitsRelease());
	_system.ProcessTurn();
	EXPECT_EQ(fighting.fighter.state, fight::State::Stance);
	EXPECT_EQ(registry.Get<CreatureAnimation>(a).body.animations[0], fight::animations::k_PowerUp);
	ASSERT_TRUE(fighting.fighter.queue.Release(600.0f));
	_system.ProcessTurn();
	EXPECT_FALSE(fighting.fighter.queue.AwaitsRelease());
	EXPECT_NE(registry.Get<CreatureAnimation>(a).body.animations[0], fight::animations::k_PowerUp);
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

// The arena's keep-in rule: with its opponent past the arena's edge fraction, a fighter in its stance steps back
// towards the middle and chooses no move, and one in its block ends the block, whoever controls them; before, the two
// stood there for good, the queue and the computer waiting for both to be inside (docs/wiki/creature/fights.md)
TEST_F(CreatureFightTurnsTest, WithEitherPastTheArenasEdgeABlockEndsAndTheStanceStepsBackIn)
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
	auto& registry = test::creature_world::World::Registry();
	for (const auto self : {a, b})
	{
		FightingOf(self).arena = {.centre = {100.0f, 90.0f}, .radius = 20.0f};
	}
	// the opponent 20 behind the middle, beyond the 16 of the edge fraction, blocking
	const glm::vec3 out {100.0f, 0.0f, 70.0f};
	auto& other = registry.Get<CreatureLocomotion>(b);
	other.fromPosition = out;
	other.toPosition = out;
	registry.Get<Transform>(b).position = out;
	fight::Enter(FightingOf(b).fighter, fight::State::Block);
	_system.ProcessTurn();
	// the fighter 10 short of the middle, facing it, steps forward; the blocker ends its block; neither drew
	EXPECT_EQ(FightingOf(a).fighter.state, fight::State::Action);
	EXPECT_EQ(FightingOf(a).fighter.animation, fight::animations::k_StepForward);
	EXPECT_EQ(FightingOf(b).fighter.state, fight::State::BlockEnd);
	EXPECT_TRUE(draws.empty());
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
	EXPECT_FLOAT_EQ(after, fight::AdvanceClock(200.0f, 100.0f, fight::animations::k_FirstAttack));
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

// A knock-out with no turns laid lies the seconds of the size its body is drawn at, as every faint's lie does
TEST_F(CreatureFightCarriedHomeTest, AKnockOutWithNoTurnsLaidLiesTheSecondsOfItsDrawnSize)
{
	const auto creature = LyingOutCold();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<Creature>(creature).size = 1.0f;
	registry.Get<CreatureDrawPose>(creature).size = 2.0f;
	// past the seconds of its size as a body, short of those of its drawn size: it lies on
	registry.Get<CreatureKnockedOut>(creature).seconds = fight::FaintSeconds(1.0f) + 1.0f;
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::Lying);
	// past those of its drawn size, it is taken home
	registry.Get<CreatureKnockedOut>(creature).seconds = fight::FaintSeconds(2.0f);
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::FadingOut);
}

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
	// on the twentieth it is moved home, fizzes back in over two seconds and comes round as far as it is moved; the move
	// ends its faint, and it stands there with no action
	auto& animation = test::creature_world::World::Registry().Get<CreatureAnimation>(creature);
	animation.body =
	    *creature_layers::PlaySequence({}, creature_layers::animations::k_Faint, creature_layers::animations::k_Faint,
	                                   creature_layers::animations::k_GetUp, true);
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::FadingIn);
	EXPECT_EQ(PositionOf(creature), glm::vec3(300.0f, 0.0f, 300.0f));
	EXPECT_FALSE(creature_layers::IsPlaying(animation.body));
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
	registry.Get<CreatureKnockedOut>(creature).homeWaited = true;
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

// A knock-out a save kept in its creature's Die action is read back at its step and taken up again there, as the game's
// load starts the step again: written once more, its Die action is at the same step with the same steps left. Taking it
// up draws nothing and fades nothing but the carry home, which starts fading out again
TEST_F(CreatureFightCarriedHomeTest, AKnockOutASaveKeptIsTakenUpAgainAtItsStep)
{
	using Stage = CreatureKnockedOut::Stage;
	using Step = KeptKnockOut::Step;
	const game_random::testing::ScopedState state;
	std::vector<uint32_t> draws;
	game_random::testing::SetGameRand(
	    [&draws](uint32_t n) {
		    draws.push_back(n);
		    return 0u;
	    },
	    [](float) { return 0.0f; });
	auto& registry = test::creature_world::World::Registry();
	constexpr gamesave::Ref k_Self {.number = 7};
	struct Case
	{
		CreatureKnockedOut knockedOut;
		Step step;
	};
	const std::array<Case, 8> cases {{
	    {{.lieTurns = 140, .rests = true}, Step::Faint},
	    {{.lieTurns = 140, .seenLying = true, .rests = true}, Step::Faint},
	    {{.lieTurns = 90, .seenLying = true, .lieCounting = true, .rests = true}, Step::Lie},
	    {{.lieTurns = 0, .seenLying = true, .lieCounting = true, .rests = true}, Step::CarryHome},
	    {{.stage = Stage::FadingIn, .lieTurns = 0, .seenLying = true, .lieCounting = true, .rests = false}, Step::CarryHome},
	    {{.stage = Stage::Waiting, .lieTurns = 0, .rests = true, .homeWaitTurns = 12}, Step::HomeWait},
	    {{.stage = Stage::Waiting, .lieTurns = 0, .rests = true, .homeWaitTurns = 0, .homeWaited = true}, Step::WaitForSpells},
	    {{.stage = Stage::Resting, .lieTurns = 0, .rests = true, .restTurns = 12, .restPosed = true}, Step::Rest},
	}};
	for (size_t i = 0; i < cases.size(); ++i)
	{
		const auto& [knockedOut, step] = cases[i];
		std::string why;
		const auto plan = ecs::saved_world_write::KnockedOutPlanOf(knockedOut, k_Self, k_Self, true, why);
		ASSERT_TRUE(plan.has_value()) << why;
		const auto read = ecs::saved_world_load::KnockedOutOf(*plan, true);
		ASSERT_TRUE(read.has_value()) << "case " << i;
		EXPECT_EQ(read->step, step) << "case " << i;
		EXPECT_EQ(read->rests, *knockedOut.rests) << "case " << i;
		const auto creature =
		    test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
		_fizz->sets.clear();
		draws.clear();
		_system.RestoreKnockedOut(creature, KeptKnockOut {.step = read->step,
		                                                  .lieTurns = read->lieTurns,
		                                                  .homeWaitTurns = read->homeWaitTurns,
		                                                  .rests = read->rests});
		EXPECT_TRUE(draws.empty()) << "case " << i;
		ASSERT_TRUE(_system.IsKnockedOut(creature));
		const auto& restored = std::as_const(registry).Get<const CreatureKnockedOut>(creature);
		EXPECT_EQ(restored.restartFaint, step == Step::Faint) << "case " << i;
		EXPECT_EQ(_fizz->sets.size(), step == Step::CarryHome ? 1u : 0u) << "case " << i;
		const auto again = ecs::saved_world_write::KnockedOutPlanOf(restored, k_Self, k_Self, true, why);
		ASSERT_TRUE(again.has_value()) << why;
		EXPECT_EQ(again->current, plan->current) << "case " << i;
		EXPECT_EQ(again->count, plan->count) << "case " << i;
		EXPECT_EQ(again->steps, plan->steps) << "case " << i;
	}
	// a neutral creature is not carried home: its lie over, it waits at home
	CreatureKnockedOut over {.lieTurns = 0, .seenLying = true, .lieCounting = true, .rests = false};
	std::string why;
	const auto neutral = ecs::saved_world_write::KnockedOutPlanOf(over, k_Self, k_Self, false, why);
	ASSERT_TRUE(neutral.has_value()) << why;
	EXPECT_EQ(ecs::saved_world_load::KnockedOutOf(*neutral, false)->step, Step::HomeWait);
	EXPECT_FALSE(ecs::saved_world_load::KnockedOutOf(*neutral, true).has_value());
}

// A faint a load left at its start starts again on the creature's next turn: the side it falls to is drawn then, it
// falls from the start, and those near it react as to a death
TEST_F(CreatureFightCarriedHomeTest, AFaintALoadLeftAtItsStartStartsAgainOnItsNextTurn)
{
	const game_random::testing::ScopedState state;
	std::vector<uint32_t> draws;
	game_random::testing::SetGameRand(
	    [&draws](uint32_t n) {
		    draws.push_back(n);
		    return 1u;
	    },
	    [](float) { return 0.0f; });
	const test::RestoreService<Locator::reactionSystem> keepReactions;
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	// making the creature draws its own numbers; only the restore's and the faint's are counted
	draws.clear();
	_system.RestoreKnockedOut(creature, KeptKnockOut {.step = KeptKnockOut::Step::Faint, .lieTurns = 140, .rests = true});
	EXPECT_TRUE(draws.empty());
	EXPECT_EQ(ecs::effects::reactions::GetReactionOfTypeInitiatedBy(creature, openblack::Reaction::ReactToDeath), 0u);
	_system.ProcessTurn();
	EXPECT_EQ(draws, std::vector<uint32_t> {2});
	const auto& animation = std::as_const(registry).Get<const CreatureAnimation>(creature);
	EXPECT_TRUE(animation.body.holdLoop);
	EXPECT_TRUE(animation.body.mirrored);
	EXPECT_EQ(animation.body.phase, creature_layers::BodyAction::Phase::Start);
	EXPECT_NE(ecs::effects::reactions::GetReactionOfTypeInitiatedBy(creature, openblack::Reaction::ReactToDeath), 0u);
	const auto& knockedOut = std::as_const(registry).Get<const CreatureKnockedOut>(creature);
	EXPECT_FALSE(knockedOut.restartFaint);
	EXPECT_FALSE(knockedOut.seenLying);
	EXPECT_EQ(knockedOut.lieTurns, std::optional<int32_t>(140));
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
TEST_F(CreatureFightCarriedHomeTest, EveryFaintSaysItIsTakenHomeThenThatItFaintedAndAForcedDeathOnlyTheFirst)
{
	// The Die action's steps, laid as it is forced, say it is taken home (6); the faint then says it fainted (3); a Die
	// action forced on it by a script says only the first
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::reactionSystem> keepReactions;
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	const test::RestoreService<Locator::creatureHelpSystem> restoreHelp;
	auto& help = static_cast<RaisedHelp&>(Locator::creatureHelpSystem::emplace<RaisedHelp>());
	const auto fainting = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	_system.ForceFaint(fainting);
	EXPECT_EQ(help.raised, (std::vector<uint32_t> {6, 3}));
	help.raised.clear();
	const auto dying = test::creature_world::World::MakeCreature(glm::vec3(200.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	_system.ForceDie(dying, dying);
	EXPECT_EQ(help.raised, (std::vector<uint32_t> {6}));
	// knocked out, as a fight's blow or a miracle leaves it, it is taken home and has fainted
	help.raised.clear();
	const auto knocked = test::creature_world::World::MakeCreature(glm::vec3(300.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	_system.KnockOut(knocked);
	ASSERT_TRUE(_system.IsKnockedOut(knocked));
	EXPECT_EQ(help.raised, (std::vector<uint32_t> {6, 3}));
}

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
	registry.AssignState<CreatureKnockedOut>(creature, CreatureKnockedOut {.stage = CreatureKnockedOut::Stage::Waiting,
	                                                                       .seconds = 1000.0f,
	                                                                       .rests = rests,
	                                                                       .homeWaited = true,
	                                                                       .actedOn = actedOn});
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

// Resting at home, it gets up only once its life is full and it is not exhausted at all, and then only after more than
// 30 turns of rest: healthy enough to have needed no rest is not enough
TEST_F(CreatureFightCarriedHomeTest, RestingItGetsUpOnlyFullyHealedAndRestedAfterMoreThanThirtyTurns)
{
	auto& registry = test::creature_world::World::Registry();
	// carried home, its body stands, so its first turn of rest lies it down in the sleep pose
	const auto creature = WaitedForcedToDie(true, std::nullopt);
	registry.Get<CreatureAnimation>(creature).body = {};
	auto& needs = registry.Get<CreatureNeeds>(creature).needs;
	needs.life = 0.5f;
	needs.exhaustion = 0.0f;
	_system.ProcessTurn();
	ASSERT_EQ(StageOf(creature), CreatureKnockedOut::Stage::Resting);
	for (int turn = 0; turn < 40; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::Resting);
	needs.life = 1.0f;
	needs.exhaustion = 0.0f;
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::GettingUp);

	// healed and rested from its first turn of rest, it still rests more than 30 turns
	const auto rested = WaitedForcedToDie(true, std::nullopt);
	registry.Get<CreatureAnimation>(rested).body = {};
	registry.Get<CreatureNeeds>(rested).needs.life = 1.0f;
	registry.Get<CreatureNeeds>(rested).needs.exhaustion = 0.0f;
	_system.ProcessTurn();
	ASSERT_EQ(StageOf(rested), CreatureKnockedOut::Stage::Resting);
	for (int turn = 0; turn < 30; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(StageOf(rested), CreatureKnockedOut::Stage::Resting);
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(rested), CreatureKnockedOut::Stage::GettingUp);
}

/// Each turn of rest at home in the sleep pose, the getting-up turn too, its temperature is brought to the ground's where
// it stands, so a fire on it stops; not while it waits for the spells, nor on the turn it lies down, nor once it is
// getting up
TEST_F(CreatureFightCarriedHomeTest, EachTurnOfRestItsFireIsPutOut)
{
	const test::RestoreService<Locator::fireSystem> restoreFires;
	auto& fires = static_cast<test::FakeFires&>(Locator::fireSystem::emplace<test::FakeFires>());
	auto& registry = test::creature_world::World::Registry();
	// carried home, its body stands, so its first turn of rest lies it down in the sleep pose
	const auto creature = WaitedForcedToDie(true, std::nullopt);
	registry.Get<CreatureAnimation>(creature).body = {};
	auto& needs = registry.Get<CreatureNeeds>(creature).needs;
	needs.life = 0.5f;
	needs.exhaustion = 0.0f;
	_system.ProcessTurn();
	ASSERT_EQ(StageOf(creature), CreatureKnockedOut::Stage::Resting);
	EXPECT_TRUE(fires.putOut.empty());
	for (int turn = 0; turn < 40; ++turn)
	{
		_system.ProcessTurn();
	}
	// the first of them lies it down in the pose
	EXPECT_EQ(fires.putOut, std::vector<entt::entity>(39, creature));
	needs.life = 1.0f;
	_system.ProcessTurn();
	ASSERT_EQ(StageOf(creature), CreatureKnockedOut::Stage::GettingUp);
	EXPECT_EQ(fires.putOut.size(), 40u);
	_system.ProcessTurn();
	EXPECT_EQ(fires.putOut.size(), 40u);
}

// The rest first lies the creature down in the sleep pose, and it heals only in the pose. A body that still lies in its
// faint, as a creature of no player is never carried home, refuses the pose on every try: it never heals or gets up
TEST_F(CreatureFightCarriedHomeTest, TheRestStartsTheSleepPoseOnceTheBodyIsFreeAndHealsOnlyInIt)
{
	auto& registry = test::creature_world::World::Registry();
	const auto standing = WaitedForcedToDie(true, std::nullopt);
	auto& animation = registry.Get<CreatureAnimation>(standing);
	animation.body = {};
	_system.ProcessTurn();
	ASSERT_EQ(StageOf(standing), CreatureKnockedOut::Stage::Resting);
	EXPECT_FALSE(registry.Get<CreatureKnockedOut>(standing).restPosed);
	_system.ProcessTurn();
	EXPECT_TRUE(registry.Get<CreatureKnockedOut>(standing).restPosed);
	EXPECT_EQ(animation.body.animations,
	          (std::array {creature_layers::animations::k_StartSleep, creature_layers::animations::k_Sleep,
	                       creature_layers::animations::k_EndSleep}));
	EXPECT_EQ(registry.Get<CreatureNeeds>(standing).rest, CreatureNeeds::Rest::Resting);

	const auto lying = WaitedForcedToDie(true, std::nullopt);
	registry.Get<CreatureNeeds>(lying).needs.life = 1.0f;
	registry.Get<CreatureNeeds>(lying).needs.exhaustion = 0.0f;
	for (int turn = 0; turn < 40; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(StageOf(lying), CreatureKnockedOut::Stage::Resting);
	EXPECT_FALSE(registry.Get<CreatureKnockedOut>(lying).restPosed);
	EXPECT_EQ(registry.Get<CreatureAnimation>(lying).body.animations.front(), creature_layers::animations::k_Faint);
	EXPECT_EQ(registry.Get<CreatureNeeds>(lying).rest, CreatureNeeds::Rest::Unconscious);
}

// The rest's sleep pose plays left to right when the draw of the try that starts it is a one, unless a script set its
// body to real time, the side drawn all the same
TEST_F(CreatureFightCarriedHomeTest, TheRestsSleepPoseIsMirroredByTheDrawOfTheTryThatStartsIt)
{
	auto& registry = test::creature_world::World::Registry();
	const auto restOf = [&](uint32_t draw, bool realSpeed) {
		const auto creature = WaitedForcedToDie(true, std::nullopt);
		auto& animation = registry.Get<CreatureAnimation>(creature);
		animation.body = {};
		animation.realSpeed = realSpeed;
		_system.ProcessTurn();
		const game_random::testing::ScopedState state;
		game_random::testing::SetGameRand([draw](uint32_t) { return draw; }, [](float) { return 0.0f; });
		_system.ProcessTurn();
		EXPECT_TRUE(registry.Get<CreatureKnockedOut>(creature).restPosed);
		return registry.Get<CreatureAnimation>(creature).body.mirrored;
	};
	EXPECT_TRUE(restOf(1u, false));
	EXPECT_FALSE(restOf(0u, false));
	EXPECT_FALSE(restOf(1u, true));
}

// Carried home, its wait there counts its thirty turns only while its body is free, from the turn after it faded back
// in: three turns of an action of its own hold the count back, so the wait ends on the thirty-third, and the spells are
// first looked at on the turn after, when, none on it and no rest decided, it gets up
TEST_F(CreatureFightCarriedHomeTest, TheWaitAtHomeCountsOnlyFreeTurnsAndTheSpellsAreLookedAtTheTurnAfter)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = LyingOutCold();
	auto& knockedOut = registry.Get<CreatureKnockedOut>(creature);
	knockedOut.stage = CreatureKnockedOut::Stage::FadingIn;
	knockedOut.fizzTurns = 1;
	knockedOut.rests = false;
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.body = {};
	_system.ProcessTurn();
	ASSERT_EQ(StageOf(creature), CreatureKnockedOut::Stage::Waiting);
	for (int turn = 1; turn <= 33; ++turn)
	{
		// an action of its own on the fifth to the seventh turns
		animation.body = turn >= 5 && turn <= 7 ? *creature_layers::PlayOnce({}, creature_layers::animations::k_Happy, false)
		                                        : creature_layers::BodyAction {};
		_system.ProcessTurn();
		ASSERT_EQ(StageOf(creature), CreatureKnockedOut::Stage::Waiting) << turn;
		EXPECT_EQ(registry.Get<CreatureKnockedOut>(creature).homeWaited, turn == 33) << turn;
	}
	EXPECT_EQ(registry.Get<CreatureKnockedOut>(creature).homeWaitTurns, std::optional(0));
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::GettingUp);
}

// Not carried home, its wait counts from the turn its lie ends, while it lies dead whatever its body does: thirty
// turns from there the wait ends, and the spells are looked at on the turn after
TEST_F(CreatureFightCarriedHomeTest, NotCarriedHomeItsWaitCountsFromTheTurnItsLieEndsWhileItLiesDead)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = LyingOutCold(PlayerNames::NEUTRAL);
	auto& knockedOut = registry.Get<CreatureKnockedOut>(creature);
	knockedOut.lieTurns = 0;
	knockedOut.seenLying = true;
	knockedOut.lieCounting = true;
	knockedOut.rests = false;
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.body =
	    *creature_layers::PlaySequence({}, creature_layers::animations::k_Faint, creature_layers::animations::k_Faint,
	                                   creature_layers::animations::k_GetUp, true);
	animation.body.phase = creature_layers::BodyAction::Phase::Loop;
	_system.ProcessTurn();
	ASSERT_EQ(StageOf(creature), CreatureKnockedOut::Stage::Waiting);
	EXPECT_EQ(registry.Get<CreatureKnockedOut>(creature).homeWaitTurns, std::optional(29));
	for (int turn = 2; turn <= 30; ++turn)
	{
		_system.ProcessTurn();
		ASSERT_EQ(StageOf(creature), CreatureKnockedOut::Stage::Waiting) << turn;
		EXPECT_EQ(registry.Get<CreatureKnockedOut>(creature).homeWaited, turn == 30) << turn;
	}
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::GettingUp);
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

// A faint lays its Die action's steps as it faints, so whether it rests once home goes by how it was then: a healthy
// creature that faints does not rest even if it is hurt afterwards, a hurt one does
TEST_F(CreatureFightCarriedHomeTest, AFaintDecidesItsRestAsItFaints)
{
	auto& registry = test::creature_world::World::Registry();
	const auto healthy = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	registry.Get<CreatureNeeds>(healthy).needs.life = 1.0f;
	registry.Get<CreatureNeeds>(healthy).needs.exhaustion = 0.0f;
	_system.ForceFaint(healthy);
	registry.Get<CreatureNeeds>(healthy).needs.life = 0.1f;
	EXPECT_EQ(registry.Get<CreatureKnockedOut>(healthy).rests, std::optional<bool> {false});

	const auto hurt = test::creature_world::World::MakeCreature(glm::vec3(200.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	registry.Get<CreatureNeeds>(hurt).needs.life = 0.1f;
	_system.ForceFaint(hurt);
	EXPECT_EQ(registry.Get<CreatureKnockedOut>(hurt).rests, std::optional<bool> {true});
}

// Every faint is the Die action, whose faint makes those near it react as to a death, with no player
TEST_F(CreatureFightCarriedHomeTest, AFaintIsReactedToAsADeath)
{
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::reactionSystem> keepReactions;
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	_system.ForceFaint(creature);
	const auto death = ecs::effects::reactions::GetReactionOfTypeInitiatedBy(creature, openblack::Reaction::ReactToDeath);
	ASSERT_NE(death, 0u);
	EXPECT_FALSE(ecs::effects::reactions::Find(death)->player.has_value());
}

// Every faint is a plan forced on the creature as its player's order is: to obey the player by dying, done to itself,
// at the priority 1, what it was doing stopped first. The knock-out carries out the plan's steps, so the plan's agenda
// is its one main step waiting for the body to be free; meanwhile it chooses nothing for itself
TEST_F(CreatureFightCarriedHomeTest, AFaintForcesThePlanToObeyThePlayerByDyingOnItself)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	_mind->activityResult = true;
	_system.ForceFaint(creature);
	ASSERT_EQ(_mind->activities.size(), 1U);
	const auto& [who, plan] = _mind->activities.front();
	EXPECT_TRUE(who == creature);
	EXPECT_EQ(plan.desire, creature_desires::Desire::ObeyPlayer);
	EXPECT_EQ(plan.action, "Die");
	EXPECT_TRUE(plan.actionObject == creature);
	EXPECT_FALSE(plan.activityObject.has_value());
	EXPECT_FALSE(plan.instrument.has_value());
	EXPECT_FLOAT_EQ(plan.score, 1.0f);
	EXPECT_TRUE(plan.byPlayer);
	EXPECT_TRUE(plan.endsWhatItDoes);
	ASSERT_TRUE(plan.agenda.has_value());
	ASSERT_EQ(plan.agenda->size(), 1U);
	EXPECT_EQ(plan.agenda->front().kind, creature_mind::Step::Kind::WaitUntilFree);
	EXPECT_TRUE(plan.agenda->front().main);
	EXPECT_TRUE(std::as_const(registry).Get<const CreatureMindState>(creature).commanded);
	EXPECT_TRUE(registry.AllOf<CreatureKnockedOut>(creature));
}

// A Die action a script forces already is the plan: its faint forces no second one
TEST_F(CreatureFightCarriedHomeTest, ForcedToDieItsFaintForcesNoSecondPlan)
{
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	_system.ForceDie(creature, creature);
	EXPECT_TRUE(test::creature_world::World::Registry().AllOf<CreatureKnockedOut>(creature));
	EXPECT_TRUE(_mind->activities.empty());
}

// Every faint is the Die action: it lies the turns of the size it is drawn at, counted from the turn after it is first
// seen lying, as a forced death does
TEST_F(CreatureFightCarriedHomeTest, AFaintLiesTheTurnsOfItsDrawnSizeFromWhenItIsSeenLying)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	// drawn at 2: (2 x 4 + 8) seconds, 160 turns at 10 a second
	registry.Get<CreatureDrawPose>(creature).size = 2.0f;
	_system.ForceFaint(creature);
	const auto& knockedOut = std::as_const(registry).Get<const CreatureKnockedOut>(creature);
	ASSERT_TRUE(knockedOut.lieTurns.has_value());
	EXPECT_EQ(*knockedOut.lieTurns, 160);
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.body.phase = creature_layers::BodyAction::Phase::Loop;
	_system.ProcessTurn();
	EXPECT_TRUE(knockedOut.seenLying);
	for (int turn = 0; turn < 160; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::Lying);
	_system.ProcessTurn();
	EXPECT_EQ(StageOf(creature), CreatureKnockedOut::Stage::FadingOut);
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
	// nothing is asked as the arena appears; each fighter's duel start asks
	ASSERT_EQ(StartToArena(a, b), StartResult::Started);
	EXPECT_EQ(_watch.asked, 0);
	ASSERT_EQ(StartToDuel(a, b), StartResult::Started);
	EXPECT_EQ(_watch.asked, 2);
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
	// not yet as the arena appears
	ASSERT_EQ(StartToArena(a, b), StartResult::Started);
	EXPECT_TRUE(_watch.started.empty());
	// each fighter's duel start tells the camera, the arena's maker first both times
	ASSERT_EQ(StartToDuel(a, b), StartResult::Started);
	ASSERT_EQ(_watch.started.size(), 2u);
	const auto& arena = FightingOf(a).arena;
	for (const auto& started : _watch.started)
	{
		EXPECT_EQ(started.fighterA, a);
		EXPECT_EQ(started.fighterB, b);
		EXPECT_EQ(started.centre, glm::vec3(arena.centre.x, 0.0f, arena.centre.y));
		EXPECT_FLOAT_EQ(started.radius, arena.radius);
	}
	EXPECT_EQ(_model->flights, 0);
}

TEST_F(CreatureFightCameraTest, TheCameraReadsTheArenasPlaceAsAMapPositionKeepsIt)
{
	// The fighters' middle off the map positions' grid: the arena stands at the map position the search for clear ground
	// found, which the camera's test and its watch read
	_watch.tooFar = false;
	using test::creature_world::World;
	const auto a = World::MakeCreature(glm::vec3(100.0001f, 0.0f, 100.0f), PlayerNames::PLAYER_THREE);
	const auto b = World::MakeCreature(glm::vec3(100.0001f, 0.0f, 60.0003f), PlayerNames::PLAYER_TWO);
	ASSERT_EQ(StartToDuel(a, b), StartResult::Started);
	ASSERT_EQ(_watch.started.size(), 2u);
	const auto& arena = FightingOf(a).arena;
	const glm::vec3 place(map_coords::Quantise(arena.centre.x), 0.0f, map_coords::Quantise(arena.centre.y));
	ASSERT_EQ(place.x, arena.centre.x);
	EXPECT_EQ(_watch.askedCentre, place);
	EXPECT_EQ(_watch.started.front().centre, place);
}

TEST_F(CreatureFightCameraTest, AScriptControlledFighterBringsItToThePlayersCreaturesFight)
{
	{
		const auto [a, b] = Fighters();
		// a script controls both, or the other creature refuses the fight
		ecs::script_held::SetControlledByScript(a, true);
		ecs::script_held::SetControlledByScript(b, true);
		ASSERT_EQ(StartToDuel(a, b), StartResult::Started);
		ASSERT_EQ(_watch.started.size(), 2u);
		EXPECT_EQ(_watch.started.front().fighterA, a);
	}
	// but not to a fight between two other gods' creatures
	_watch.started.clear();
	const auto [c, d] = Fighters(PlayerNames::PLAYER_THREE);
	ecs::script_held::SetControlledByScript(c, true);
	ecs::script_held::SetControlledByScript(d, true);
	ASSERT_EQ(StartToDuel(c, d), StartResult::Started);
	EXPECT_TRUE(_watch.started.empty());
}

TEST_F(CreatureFightCameraTest, NotWhileTheScriptHasTheScreenNorWithoutThePlayersCamera)
{
	_watch.tooFar = false;
	interface_active::SetActive(false);
	const auto [a, b] = Fighters();
	ASSERT_EQ(StartToDuel(a, b), StartResult::Started);
	EXPECT_EQ(_watch.asked, 0);
	EXPECT_TRUE(_watch.started.empty());
	// a camera model that has no fight watching: nothing is asked
	interface_active::SetActive(true);
	Locator::camera::value().SetModel(std::make_unique<FakeCameraModel>(nullptr));
	const auto [c, d] = Fighters();
	ASSERT_EQ(StartToDuel(c, d), StartResult::Started);
	EXPECT_EQ(_watch.asked, 0);
	EXPECT_TRUE(_watch.started.empty());
}

TEST_F(CreatureFightCameraTest, TheDebugSwitchKeepsTheCameraAway)
{
	_watch.tooFar = false;
	_system.SetCameraWatches(false);
	const auto [a, b] = Fighters();
	ASSERT_EQ(StartToDuel(a, b), StartResult::Started);
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

namespace
{
/// The advisors' remarks for a test, put back afterwards: every remark may be made, a point is always on the screen,
/// the texts of each list k are 1000 k + 1 to 1000 k + 3, and each text said is kept
class AdvisorRemarks
{
public:
	AdvisorRemarks()
	{
		s_Said.clear();
		audio::GameQueries queries;
		queries.turn = []() { return 100000u; };
		queries.landNumber = []() { return 2; };
		queries.helpLevel = []() { return 3; };
		queries.pointOnScreen = [](glm::vec3 /*point*/) { return true; };
		queries.helpRunMessage = [](uint32_t first, uint32_t /*last*/, std::string_view /*script*/) {
			s_Said.push_back(first);
			return true;
		};
		audio::Init(std::move(queries));
		std::array<audio::guidance::SpriteList, audio::guidance::k_SpriteLists> lists {};
		for (size_t k = 0; k < lists.size(); ++k)
		{
			lists.at(k) = {static_cast<uint32_t>(1000 * k + 1), static_cast<uint32_t>(1000 * k + 2),
			               static_cast<uint32_t>(1000 * k + 3)};
		}
		audio::guidance::SetSpriteLists(lists);
		audio::guidance::SetRandom([](uint32_t /*n*/) { return 0u; });
		audio::guidance::ResetForTests();
		audio::guidance::EndWaits();
	}
	AdvisorRemarks(const AdvisorRemarks&) = delete;
	AdvisorRemarks& operator=(const AdvisorRemarks&) = delete;
	AdvisorRemarks(AdvisorRemarks&&) = delete;
	AdvisorRemarks& operator=(AdvisorRemarks&&) = delete;
	~AdvisorRemarks()
	{
		audio::guidance::SetRandom({});
		audio::Init({});
		audio::Shutdown();
	}

	/// How many of the texts said are from a list
	[[nodiscard]] static size_t SaidFrom(size_t list)
	{
		return static_cast<size_t>(
		    std::ranges::count_if(s_Said, [list](uint32_t text) { return text > 1000 * list && text <= 1000 * list + 3; }));
	}

	static inline std::vector<uint32_t> s_Said;
};

constexpr size_t k_GeneralBadList = 13;
constexpr size_t k_GeneralGoodList = 14;
} // namespace

// The advisors remark how the local player's creature's fight went: the good remark when it knocks its opponent out,
// none when another god's creature does, and the bad one when its opponent leaves it
TEST_F(CreatureFightCameraTest, TheAdvisorsRemarkHowTheLocalPlayersCreaturesFightWent)
{
	const game_random::testing::ScopedState state;
	const AdvisorRemarks remarks;
	auto& registry = test::creature_world::World::Registry();
	{
		const auto [a, b] = Duel();
		registry.Get<Creature>(b).owner = PlayerNames::PLAYER_ONE;
		_system.KnockOut(a);
		EXPECT_EQ(AdvisorRemarks::SaidFrom(k_GeneralGoodList), 1u);
		EXPECT_EQ(AdvisorRemarks::SaidFrom(k_GeneralBadList), 0u);
	}
	audio::guidance::EndWaits();
	AdvisorRemarks::s_Said.clear();
	{
		const auto [a, b] = Duel();
		_system.KnockOut(a);
		EXPECT_EQ(AdvisorRemarks::SaidFrom(k_GeneralGoodList), 0u);
	}
	audio::guidance::EndWaits();
	AdvisorRemarks::s_Said.clear();
	const auto mine = DuelWithOpponentGone(PlayerNames::PLAYER_ONE, fight::State::Stance);
	_system.ProcessTurn();
	ASSERT_EQ(FightingOf(mine).stage, CreatureFighting::Stage::Respond);
	EXPECT_EQ(AdvisorRemarks::SaidFrom(k_GeneralBadList), 1u);
	EXPECT_EQ(AdvisorRemarks::SaidFrom(k_GeneralGoodList), 0u);
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
	// the camera watches the first fight only: the second, the local player's own, is too far from it. Each fighter's
	// duel start tells the camera, the maker first both times, so the first fight starts the watch twice
	ASSERT_EQ(StartToDuel(b, a), StartResult::Started);
	_watch.tooFar = true;
	ASSERT_EQ(StartToDuel(c, d), StartResult::Started);
	ASSERT_EQ(_watch.started.size(), 2u);
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
	ASSERT_EQ(StartToDuel(a, b), StartResult::Started);
	ASSERT_TRUE(_system.GetPanel().has_value());
	// the maker gone from its fight takes the arena, and the panel with it, though the camera still watches
	test::creature_world::World::Registry().RemoveState<CreatureFighting>(a);
	ASSERT_TRUE(_watch.IsWatchingFight());
	EXPECT_FALSE(_system.GetPanel().has_value());
}

TEST_F(CreatureFightTurnsTest, AngerPicksNoFightByItselfUnlessTheDebugSwitchIsOn)
{
	// the original starts a fight only as the planner's Fight action; the anger rule is a debug switch, off at first
	EXPECT_FALSE(_system.GetAngerStartsFights());
	_system.SetAngerStartsFights(true);
	EXPECT_TRUE(_system.GetAngerStartsFights());
}

// The fight system leaves the act-on to the mind: sent with the aggression leash to act on a creature close enough,
// the creature fights it only once its mind has taken up the fight plan the leash forces; the fight system itself
// starts no fight, takes nothing off the act-on and leaves the partners' agreement as it was
TEST_F(CreatureFightTurnsTest, TheFightSystemLeavesTheActOnToTheMind)
{
	auto& registry = test::creature_world::World::Registry();
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_ONE);
	// 15 high at size 1, so it reaches 120: this one 40 away
	const auto beside = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 60.0f), PlayerNames::PLAYER_TWO);
	for (const auto creature : {a, beside})
	{
		registry.Get<Creature>(creature).size = 1.0f;
		registry.Get<CreatureDrawPose>(creature).size = 1.0f;
	}
	auto& leash = registry.AssignState<CreatureLeash>(a);
	leash.worn.emplace();
	leash.worn->type = LeashType::Evil;
	auto& actOn = registry.Get<CreatureMindState>(a).leash.actOn;
	actOn = {entt::to_integral(beside)};
	const bool agreed = std::as_const(registry).Context().partnersAlwaysAgree;
	_system.ProcessTurn();
	EXPECT_FALSE(_system.IsFighting(a));
	EXPECT_FALSE(_system.IsFighting(beside));
	EXPECT_EQ(actOn, (std::vector<uint32_t> {entt::to_integral(beside)}));
	EXPECT_EQ(std::as_const(registry).Context().partnersAlwaysAgree, agreed);
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

// Life lost in a duel by anything else comes off the fight health too, never below none, and the last of it loses the
// fight; life gained goes on it. Before its duel or once its fight is over, the fight health is left alone
TEST_F(CreatureFightTurnsTest, LifeLostOrGainedInADuelMovesTheFightHealth)
{
	// the faint draws on the synced stream
	const game_random::testing::ScopedState state;
	const auto [a, b] = Duel();
	const auto& lookup = std::as_const(test::creature_world::World::Registry());
	FightingOf(a).fighter.health = 0.5f;
	_system.GainFightHealth(a, 0.7f);
	EXPECT_FLOAT_EQ(FightingOf(a).fighter.health, 1.0f);
	_system.LoseFightHealth(a, 0.25f);
	EXPECT_FLOAT_EQ(FightingOf(a).fighter.health, 0.75f);
	EXPECT_TRUE(_system.IsFighting(a));
	_system.LoseFightHealth(a, 2.0f);
	EXPECT_TRUE(_system.IsKnockedOut(a));
	EXPECT_FALSE(_system.IsFighting(a));
	const auto* record = lookup.TryGet<const CreatureFightRecord>(b);
	ASSERT_NE(record, nullptr);
	EXPECT_EQ(record->wins, 1u);
	// not in a fight, nothing
	const auto idle = test::creature_world::World::MakeCreature();
	_system.LoseFightHealth(idle, 1.0f);
	EXPECT_FALSE(_system.IsKnockedOut(idle));
}

// Starting a fight makes each fighter's record but never adds to its battles fought, which stay at none; winning still
// counts
TEST_F(CreatureFightTurnsTest, StartingAFightAddsNothingToTheBattlesFought)
{
	// the faint draws on the synced stream
	const game_random::testing::ScopedState state;
	const auto [a, b] = Duel();
	const auto& lookup = std::as_const(test::creature_world::World::Registry());
	for (const auto self : {a, b})
	{
		const auto* record = lookup.TryGet<const CreatureFightRecord>(self);
		ASSERT_NE(record, nullptr);
		EXPECT_EQ(record->fights, 0u);
	}
	_system.TakeMiracle(a, 0.0f, 1.0f);
	EXPECT_EQ(lookup.Get<const CreatureFightRecord>(b).wins, 1u);
	EXPECT_EQ(lookup.Get<const CreatureFightRecord>(b).fights, 0u);
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

TEST_F(CreatureFightTurnsTest, AFaintEndsBothFightsThatTurnTheOpponentsFirstAndEachGainsAFightsStrength)
{
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	// the fight row of the creature action table
	info->creatureAction.at(21).strengthGain = 0.012f;
	Locator::infoConstants::reset(info.release());
	loop_fakes::CallLog log;
	const test::RestoreService<Locator::creaturePhysiologySystem> restoreBody;
	auto& body =
	    static_cast<loop_fakes::FakePhysiology&>(Locator::creaturePhysiologySystem::emplace<loop_fakes::FakePhysiology>(log));
	const auto [a, b] = Duel();
	_system.ForceFaint(a);
	// the opponent's fight is over at once, with nobody to fight, and neither wins
	ASSERT_TRUE(_system.IsFighting(b));
	EXPECT_EQ(FightingOf(b).stage, CreatureFighting::Stage::Respond);
	EXPECT_EQ(FightingOf(b).opponent, entt::entity {entt::null});
	EXPECT_TRUE(FightingOf(b).fighter.stopping);
	EXPECT_EQ(test::creature_world::World::Registry().Get<CreatureFightRecord>(b).wins, 0u);
	// the one fainting gains a fight's strength first, then the opponent
	ASSERT_EQ(body.strengthened.size(), 2u);
	EXPECT_EQ(body.strengthened[0], std::pair(a, 0.012f));
	EXPECT_EQ(body.strengthened[1], std::pair(b, 0.012f));
}

// A faint gives up the fight's action: the fainter's stop stops its opponent, and with the two minds partners each stop
// fails the partner again, so the fainter's body ends its fight twice and its opponent's three times, each end in the
// midst of the fight paying the life again; with no partners each pays once
TEST_F(CreatureFightTurnsTest, AFaintBetweenPartnersPaysTheFaintersLifeTwiceAndItsOpponentsThreeTimes)
{
	auto& registry = test::creature_world::World::Registry();
	for (const bool partnered : {true, false})
	{
		const auto [a, b] = Duel();
		if (partnered)
		{
			registry.AssignOrReplaceState<CreaturePartner>(a, CreaturePartner {.partner = b});
			registry.AssignOrReplaceState<CreaturePartner>(b, CreaturePartner {.partner = a});
		}
		for (const auto self : {a, b})
		{
			registry.Get<CreatureNeeds>(self).needs.life = 1.0f;
			FightingOf(self).fighter.health = 0.6f;
		}
		_system.ForceFaint(a);
		EXPECT_NEAR(registry.Get<CreatureNeeds>(a).needs.life, partnered ? 0.8f : 0.9f, 1e-6f);
		EXPECT_NEAR(registry.Get<CreatureNeeds>(b).needs.life, partnered ? 0.7f : 0.9f, 1e-6f);
		EXPECT_TRUE(_system.IsKnockedOut(a));
	}
}

// The failures of a stop chain gain a fight's strength: in a knock-out or a faint between partners each creature gains
// two, with no partners one each
TEST_F(CreatureFightTurnsTest, AKnockOutOrAFaintBetweenPartnersGainsEachCreatureTwoFightsStrengths)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	// the fight row of the creature action table
	info->creatureAction.at(21).strengthGain = 0.012f;
	Locator::infoConstants::reset(info.release());
	loop_fakes::CallLog log;
	const test::RestoreService<Locator::creaturePhysiologySystem> restoreBody;
	auto& body =
	    static_cast<loop_fakes::FakePhysiology&>(Locator::creaturePhysiologySystem::emplace<loop_fakes::FakePhysiology>(log));
	auto& registry = test::creature_world::World::Registry();
	const auto count = [&body](entt::entity creature) {
		return static_cast<int>(std::ranges::count(body.strengthened, std::pair(creature, 0.012f)));
	};
	for (const bool knockOut : {true, false})
	{
		for (const bool partnered : {true, false})
		{
			body.strengthened.clear();
			const auto [a, b] = Duel();
			if (partnered)
			{
				registry.AssignOrReplaceState<CreaturePartner>(a, CreaturePartner {.partner = b});
				registry.AssignOrReplaceState<CreaturePartner>(b, CreaturePartner {.partner = a});
			}
			if (knockOut)
			{
				_system.KnockOut(a);
			}
			else
			{
				_system.ForceFaint(a);
			}
			EXPECT_EQ(count(a), partnered ? 2 : 1);
			EXPECT_EQ(count(b), partnered ? 2 : 1);
			EXPECT_EQ(body.strengthened.size(), partnered ? 4U : 2U);
		}
	}
}

// Between partners, a creature's stop while they duel each other fails its partner, which fails the creature again:
// the opponent's body ends its fight three times and the creature's once, each paying the life in the midst of the
// fight, the creature gains a fight's strength once and the opponent twice, and the creature's duel is over too. Its
// own end later that turn pays once more
TEST_F(CreatureFightTurnsTest, AStopBetweenPartnersInTheirDuelPaysTheOpponentThreeTimesAndEndsBothDuels)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	info->creatureAction.at(21).strengthGain = 0.012f;
	Locator::infoConstants::reset(info.release());
	loop_fakes::CallLog log;
	const test::RestoreService<Locator::creaturePhysiologySystem> restoreBody;
	auto& body =
	    static_cast<loop_fakes::FakePhysiology&>(Locator::creaturePhysiologySystem::emplace<loop_fakes::FakePhysiology>(log));
	auto& registry = test::creature_world::World::Registry();
	const auto [a, b] = Duel();
	registry.AssignOrReplaceState<CreaturePartner>(a, CreaturePartner {.partner = b});
	registry.AssignOrReplaceState<CreaturePartner>(b, CreaturePartner {.partner = a});
	for (const auto self : {a, b})
	{
		registry.Get<CreatureNeeds>(self).needs.life = 1.0f;
		FightingOf(self).fighter.health = 0.6f;
	}
	_system.PartnerStops(a);
	EXPECT_NEAR(registry.Get<CreatureNeeds>(a).needs.life, 0.9f, 1e-6f);
	EXPECT_NEAR(registry.Get<CreatureNeeds>(b).needs.life, 0.7f, 1e-6f);
	EXPECT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Respond);
	EXPECT_EQ(FightingOf(b).stage, CreatureFighting::Stage::Respond);
	EXPECT_EQ(body.strengthened, (std::vector<std::pair<entt::entity, float>> {{b, 0.012f}, {a, 0.012f}, {b, 0.012f}}));
	_system.EndOwnFight(a);
	EXPECT_NEAR(registry.Get<CreatureNeeds>(a).needs.life, 0.8f, 1e-6f);
	// and only once
	_system.EndOwnFight(a);
	EXPECT_NEAR(registry.Get<CreatureNeeds>(a).needs.life, 0.8f, 1e-6f);
}

// Stopped as an action that failed (the leash's order, a forced plan): in its duel the creature gains a fight's
// strength, its opponent gains one and stops, and each side of the duel ends paying its life once; with partners the
// creature pays twice and the opponent three times, as the chain of one failure gives
TEST_F(CreatureFightTurnsTest, StoppedAsAFailureInItsDuelBothSidesEndAsTheChainOfOneFailureGives)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	info->creatureAction.at(21).strengthGain = 0.012f;
	Locator::infoConstants::reset(info.release());
	loop_fakes::CallLog log;
	const test::RestoreService<Locator::creaturePhysiologySystem> restoreBody;
	auto& body =
	    static_cast<loop_fakes::FakePhysiology&>(Locator::creaturePhysiologySystem::emplace<loop_fakes::FakePhysiology>(log));
	auto& registry = test::creature_world::World::Registry();
	const auto paid = [&registry](entt::entity self) {
		return std::lround((1.0f - registry.Get<CreatureNeeds>(self).needs.life) / 0.1f);
	};
	for (const bool partners : {false, true})
	{
		body.strengthened.clear();
		const auto [a, b] = Duel();
		if (partners)
		{
			registry.AssignOrReplaceState<CreaturePartner>(a, CreaturePartner {.partner = b});
			registry.AssignOrReplaceState<CreaturePartner>(b, CreaturePartner {.partner = a});
		}
		for (const auto self : {a, b})
		{
			registry.Get<CreatureNeeds>(self).needs.life = 1.0f;
			FightingOf(self).fighter.health = 0.6f;
		}
		const auto chain = fight::RunStopChain({.duels = {true, true}, .partners = {partners, partners}},
		                                       std::array {fight::StopStep {.creature = 0, .call = fight::StopCall::Fail}});
		_system.StopsAsAFailure(a);
		EXPECT_EQ(paid(a), static_cast<long>(chain.bodyEnds[0])) << partners;
		EXPECT_EQ(paid(b), static_cast<long>(chain.bodyEnds[1])) << partners;
		EXPECT_EQ(body.strengthened.size(), static_cast<size_t>(chain.gains[0] + chain.gains[1])) << partners;
		EXPECT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Respond);
		EXPECT_EQ(FightingOf(b).stage, CreatureFighting::Stage::Respond);
	}
}

// The leash's walk back: the creature's body ends its fight, then the walk's stop keeps its body. Between partners in
// each other's duel that fails the opponent: the creature pays its life three times and gains a fight's strength once,
// the opponent pays once and gains once, and both duels end. With no partners only the creature's own side ends, once
TEST_F(CreatureFightTurnsTest, TheLeashsWalkBackEndsTheCreaturesFightAndBetweenPartnersItsOpponentsToo)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	info->creatureAction.at(21).strengthGain = 0.012f;
	Locator::infoConstants::reset(info.release());
	loop_fakes::CallLog log;
	const test::RestoreService<Locator::creaturePhysiologySystem> restoreBody;
	auto& body =
	    static_cast<loop_fakes::FakePhysiology&>(Locator::creaturePhysiologySystem::emplace<loop_fakes::FakePhysiology>(log));
	auto& registry = test::creature_world::World::Registry();
	{
		const auto [a, b] = Duel();
		registry.AssignOrReplaceState<CreaturePartner>(a, CreaturePartner {.partner = b});
		registry.AssignOrReplaceState<CreaturePartner>(b, CreaturePartner {.partner = a});
		for (const auto self : {a, b})
		{
			registry.Get<CreatureNeeds>(self).needs.life = 1.0f;
			FightingOf(self).fighter.health = 0.6f;
		}
		_system.WalksBack(a);
		EXPECT_NEAR(registry.Get<CreatureNeeds>(a).needs.life, 0.7f, 1e-6f);
		EXPECT_NEAR(registry.Get<CreatureNeeds>(b).needs.life, 0.9f, 1e-6f);
		EXPECT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Respond);
		EXPECT_EQ(FightingOf(b).stage, CreatureFighting::Stage::Respond);
		EXPECT_EQ(FightingOf(b).opponent, entt::entity {entt::null});
		EXPECT_EQ(body.strengthened, (std::vector<std::pair<entt::entity, float>> {{a, 0.012f}, {b, 0.012f}}));
	}
	body.strengthened.clear();
	{
		const auto [a, b] = Duel();
		for (const auto self : {a, b})
		{
			registry.Get<CreatureNeeds>(self).needs.life = 1.0f;
			FightingOf(self).fighter.health = 0.6f;
		}
		_system.WalksBack(a);
		EXPECT_NEAR(registry.Get<CreatureNeeds>(a).needs.life, 0.9f, 1e-6f);
		EXPECT_EQ(registry.Get<CreatureNeeds>(b).needs.life, 1.0f);
		EXPECT_EQ(FightingOf(b).stage, CreatureFighting::Stage::Duel);
		EXPECT_EQ(FightingOf(b).opponent, a);
		EXPECT_TRUE(body.strengthened.empty());
	}
}

// A creature that stops what it does while its opponent duels it: the opponent gains a fight's strength, its duel ends
// with nobody to fight, and the creature itself gains nothing and keeps its own fight
TEST_F(CreatureFightTurnsTest, ACreatureStoppingMidDuelEndsItsOpponentsFightWhichGainsAFightsStrength)
{
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	info->creatureAction.at(21).strengthGain = 0.012f;
	Locator::infoConstants::reset(info.release());
	loop_fakes::CallLog log;
	const test::RestoreService<Locator::creaturePhysiologySystem> restoreBody;
	auto& body =
	    static_cast<loop_fakes::FakePhysiology&>(Locator::creaturePhysiologySystem::emplace<loop_fakes::FakePhysiology>(log));
	const auto [a, b] = Duel();
	_system.PartnerStops(a);
	EXPECT_EQ(FightingOf(b).stage, CreatureFighting::Stage::Respond);
	EXPECT_EQ(FightingOf(b).opponent, entt::entity {entt::null});
	EXPECT_EQ(FightingOf(a).opponent, b);
	ASSERT_EQ(body.strengthened.size(), 1u);
	EXPECT_EQ(body.strengthened[0], std::pair(b, 0.012f));
}

// The stop reaches the opponent through the creature's own duel alone: an opponent whose own fight has ended still gains
// a fight's strength and is left with no opponent, and a creature whose own fight has ended stops nobody, though its
// opponent still duels it
TEST_F(CreatureFightTurnsTest, AStopReachesTheOpponentThroughTheStoppersOwnDuelAlone)
{
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	info->creatureAction.at(21).strengthGain = 0.012f;
	Locator::infoConstants::reset(info.release());
	loop_fakes::CallLog log;
	const test::RestoreService<Locator::creaturePhysiologySystem> restoreBody;
	auto& body =
	    static_cast<loop_fakes::FakePhysiology&>(Locator::creaturePhysiologySystem::emplace<loop_fakes::FakePhysiology>(log));
	{
		const auto [a, b] = Duel();
		FightingOf(b).stage = CreatureFighting::Stage::Respond;
		_system.PartnerStops(a);
		EXPECT_EQ(body.strengthened, (std::vector<std::pair<entt::entity, float>> {{b, 0.012f}}));
		EXPECT_EQ(FightingOf(b).opponent, entt::entity {entt::null});
		EXPECT_EQ(FightingOf(a).opponent, b);
	}
	body.strengthened.clear();
	{
		const auto [a, b] = Duel();
		FightingOf(a).stage = CreatureFighting::Stage::Respond;
		_system.PartnerStops(a);
		EXPECT_TRUE(body.strengthened.empty());
		EXPECT_EQ(FightingOf(b).stage, CreatureFighting::Stage::Duel);
		EXPECT_EQ(FightingOf(b).opponent, a);
	}
}

// What a creature does ending as a failure in the midst of its duel (no route, or the hand taking it) makes it gain a
// fight's strength; out of a duel nothing
TEST_F(CreatureFightTurnsTest, ACreatureFailingMidDuelGainsAFightsStrength)
{
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	info->creatureAction.at(21).strengthGain = 0.012f;
	Locator::infoConstants::reset(info.release());
	loop_fakes::CallLog log;
	const test::RestoreService<Locator::creaturePhysiologySystem> restoreBody;
	auto& body =
	    static_cast<loop_fakes::FakePhysiology&>(Locator::creaturePhysiologySystem::emplace<loop_fakes::FakePhysiology>(log));
	const auto [a, b] = Duel();
	_system.FailsWhileDueling(a);
	ASSERT_EQ(body.strengthened.size(), 1u);
	EXPECT_EQ(body.strengthened[0], std::pair(a, 0.012f));
	const auto alone = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_TWO);
	_system.FailsWhileDueling(alone);
	EXPECT_EQ(body.strengthened.size(), 1u);
}

TEST_F(CreatureFightTurnsTest, AFaintDrawsOnceForTheSideItFallsTo)
{
	// The original's faint draws one game random number of 2 as its fall starts: mirrored when it is not 0
	const game_random::testing::ScopedState state;
	auto& registry = test::creature_world::World::Registry();
	// kept outside the loop: the stub stays installed while the next creature is made
	std::vector<uint32_t> draws;
	for (const uint32_t side : {0u, 1u})
	{
		const auto creature =
		    test::creature_world::World::MakeCreature(glm::vec3(300.0f * static_cast<float>(side), 0.0f, 0.0f),
		                                              side == 0 ? PlayerNames::PLAYER_TWO : PlayerNames::PLAYER_THREE);
		draws.clear();
		game_random::testing::SetGameRand(
		    [&draws, side](uint32_t n) {
			    draws.push_back(n);
			    return side;
		    },
		    [](float) { return 0.0f; });
		_system.ForceFaint(creature);
		ASSERT_TRUE(_system.IsKnockedOut(creature));
		EXPECT_EQ(draws, std::vector<uint32_t> {2u});
		EXPECT_EQ(std::as_const(registry).Get<const CreatureAnimation>(creature).body.mirrored, side != 0);
	}
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
	ASSERT_EQ(FightingOf(a).stage, CreatureFighting::Stage::FaceOff);
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

// Only its own duel ends a fighter's: an opponent whose own side of the fight ended (its body stopped, as the hand's
// hold stops it) is fought on, and the fighter finishes only once it has no opponent left
TEST_F(CreatureFightTurnsTest, AFighterFightsOnAnOpponentWhoseOwnSideEndedAndFinishesOnlyWithNoOpponent)
{
	const game_random::testing::ScopedState state;
	const auto [a, b] = Duel();
	_system.EndOwnFight(b);
	ASSERT_EQ(FightingOf(b).stage, CreatureFighting::Stage::Respond);
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Duel);
	EXPECT_EQ(FightingOf(a).opponent, b);
	// its opponent cleared, as the opponent's stop in its own duel clears it, it finishes
	FightingOf(a).opponent = entt::null;
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Respond);
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
	ASSERT_EQ(StartToArena(a, b), StartResult::Started);
	ASSERT_EQ(StartToArena(d, c), StartResult::Started);
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
	// each at the nearest clear ground's middle to the fighters' middle (320, 300) and (100, 90): its square of cells
	// from the corner nearest, half its width past it
	const auto middleFrom = [](glm::ivec2 corner, float radius) {
		return map_coords::ToMetres(creature_cast_moves::ClearAreaMiddle(corner, radius + radius));
	};
	EXPECT_EQ(arenas[0].centre, middleFrom({28, 26}, arenas[0].radius));
	EXPECT_EQ(arenas[1].centre, middleFrom({6, 5}, arenas[1].radius));
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
		ASSERT_EQ(StartToArena(maker, opponent), StartResult::Started);
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
	ASSERT_EQ(StartToArena(a, b), StartResult::Started);
	const auto arena = _system.ArenaOf(a);
	ASSERT_TRUE(arena.has_value());
	const auto& made = FightingOf(a).arena.centre;
	EXPECT_EQ(arena->centre.x, map_coords::ToMetres(map_coords::ToFixed(made.x)));
	EXPECT_EQ(arena->centre.y, map_coords::ToMetres(map_coords::ToFixed(made.y)));
	// a new arena's middle is the map position its search for clear ground found
	EXPECT_EQ(arena->centre.x, made.x);
}

// With no arena near and no clear ground for even six tenths of one when the first creature's taunt ends, the fight
// stops there for both
TEST_F(CreatureFightTurnsTest, AFightWithNoRoomForAnArenaStopsAtTheEndOfTheTaunt)
{
	auto& land = Locator::landAvoidSystem::value().GetState();
	land.avoid.assign(land.avoid.size(), land_avoid::k_Avoid);
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	EXPECT_TRUE(_system.GetArenas().empty());
	ToArena(a, b);
	EXPECT_FALSE(_system.IsFighting(a));
	EXPECT_FALSE(_system.IsFighting(b));
	EXPECT_TRUE(_system.GetArenas().empty());
	// a strip of clear ground just wide enough for six tenths of the arena: it stands there at that size
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	const auto wanted =
	    fight::ArenaRadius(ecs::creature_pose::DrawnSize(registry, a), ecs::creature_pose::DrawnSize(registry, b));
	for (int32_t z = 0; z < land.size; ++z)
	{
		for (int32_t x = 0; x < land.size; ++x)
		{
			if (x >= 6 && x < 6 + 5 && z >= 5 && z < 5 + 5)
			{
				land.avoid.at(static_cast<size_t>(z * land.size + x)) = land_avoid::k_Land;
			}
		}
	}
	ASSERT_EQ(StartToArena(a, b), StartResult::Started);
	EXPECT_LT(FightingOf(a).arena.radius, 0.7f * wanted);
	EXPECT_GT(FightingOf(a).arena.radius, 0.59f * wanted);
}

// The steps before the duel come in the original's order: the face-off, both taunts, the arena (only then), the first
// creature's point at it, the walk to the places, the turn to face in the arena, then the duels, the other creature's
// first
TEST_F(CreatureFightTurnsTest, AFightFacesOffAndTauntsBeforeItsArenaAppears)
{
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
	auto& registry = test::creature_world::World::Registry();
	// each already faces the other
	registry.Get<CreatureLocomotion>(a).heading = 0.0f;
	registry.Get<CreatureLocomotion>(b).heading = std::numbers::pi_v<float>;
	for (const auto animation : {fight::animations::k_Taunt, fight::animations::k_RudeGesture})
	{
		_animation->durations[animation] = 1000.0f;
	}
	// the other creature is in the middle of an action of its own
	if (const auto once = creature_layers::PlayOnce({}, fight::animations::k_Taunt, false); once.has_value())
	{
		registry.Get<CreatureAnimation>(b).body = *once;
	}
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	EXPECT_EQ(FightingOf(a).stage, CreatureFighting::Stage::FaceOff);
	EXPECT_EQ(FightingOf(b).stage, CreatureFighting::Stage::FaceOff);
	EXPECT_TRUE(_system.GetArenas().empty());
	// the first creature stops what it does at once; the other carries on until its first order
	EXPECT_TRUE(FightingOf(a).ordered);
	EXPECT_FALSE(FightingOf(b).ordered);
	EXPECT_TRUE(creature_layers::IsPlaying(registry.Get<const CreatureAnimation>(b).body));
	EXPECT_FALSE(_system.ArenaOf(a).has_value());
	using Phase = creature_turn_to_face::Turn::Phase;
	// It asks, tells the other creature's mind to stand still, then tells the other creature to turn
	for (int turn = 0; turn < 3; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_NE(FightingOf(b).faceTurn.phase, Phase::Idle);
	EXPECT_EQ(FightingOf(a).faceTurn.phase, Phase::Idle);
	// its first order stops what it did
	EXPECT_TRUE(FightingOf(b).ordered);
	EXPECT_FALSE(creature_layers::IsPlaying(registry.Get<const CreatureAnimation>(b).body));
	// From the fourth turn it turns itself: it starts the turn, finds it faces the other creature, holds a turn and is
	// done; on the turn after, both taunt
	for (int turn = 0; turn < 4; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(FightingOf(a).faceTurn.phase, Phase::Done);
	EXPECT_EQ(FightingOf(a).stage, CreatureFighting::Stage::FaceOff);
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Taunt);
	EXPECT_EQ(FightingOf(b).stage, CreatureFighting::Stage::Taunt);
	EXPECT_TRUE(_system.GetArenas().empty());
	// Both play their taunt; the first creature's over, the arena appears and it points at it, while the other creature
	// waits for its next order
	_system.ProcessTurn();
	EXPECT_TRUE(FightingOf(a).taunted);
	EXPECT_TRUE(FightingOf(b).taunted);
	for (const auto self : {a, b})
	{
		registry.Get<CreatureAnimation>(self).body = {};
	}
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Point);
	EXPECT_EQ(FightingOf(b).stage, CreatureFighting::Stage::Ready);
	const auto arena = _system.ArenaOf(a);
	ASSERT_TRUE(arena.has_value());
	EXPECT_EQ(arena->opponent, b);
	EXPECT_EQ(FightingOf(b).arena.serial, FightingOf(a).arena.serial);
}

// Each creature's taunt is drawn from two as the fight starts, the other creature's first: any draw but 0 is the rude
// gesture. Each try to start it draws again for whether it is mirrored, also a try the busy body refuses
TEST_F(CreatureFightTurnsTest, TheTauntsAreDrawnAsTheFightStartsAndEachTryDrawsASide)
{
	// the creatures made first, as their making draws too
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
	const game_random::testing::ScopedState state;
	std::vector<uint32_t> ranges;
	std::vector<uint32_t> numbers {1, 0};
	game_random::testing::SetGameRand(
	    [&ranges, &numbers](uint32_t range) {
		    ranges.push_back(range);
		    const auto drawn = numbers.empty() ? 0u : numbers.front();
		    if (!numbers.empty())
		    {
			    numbers.erase(numbers.begin());
		    }
		    return drawn;
	    },
	    [](float) { return 0.0f; });
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	EXPECT_EQ(ranges, (std::vector<uint32_t> {2, 2}));
	EXPECT_TRUE(FightingOf(b).rude);
	EXPECT_FALSE(FightingOf(a).rude);
	EXPECT_EQ(fight::FaceOffAnimation(FightingOf(b).rude), fight::animations::k_RudeGesture);

	// Both at their taunts, the first creature's body busy with an action of its own
	_animation->durations[fight::animations::k_Taunt] = 1000.0f;
	_animation->durations[fight::animations::k_RudeGesture] = 1000.0f;
	auto& registry = test::creature_world::World::Registry();
	FightingOf(a).stage = CreatureFighting::Stage::Taunt;
	FightingOf(b).stage = CreatureFighting::Stage::Taunt;
	registry.Get<CreatureAnimation>(a).body.kind = creature_layers::BodyAction::Kind::Once;
	ranges.clear();
	for (int turn = 0; turn < 3; ++turn)
	{
		_system.ProcessTurn();
	}
	// the busy creature draws each turn and does not taunt; the other draws once and taunts
	EXPECT_FALSE(FightingOf(a).played);
	EXPECT_TRUE(FightingOf(b).played);
	EXPECT_EQ(registry.Get<CreatureAnimation>(b).body.animations[0], fight::animations::k_RudeGesture);
	EXPECT_EQ(std::ranges::count(ranges, 2u), 3 + 1);
}

// A fight started with the taunts its agenda drew as it was made draws none of its own: a fight its creature's mind
// plans draws its two taunts once, not again as it starts
TEST_F(CreatureFightTurnsTest, AFightGivenItsAgendasTauntsDrawsNoneOfItsOwn)
{
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
	const game_random::testing::ScopedState state;
	std::vector<uint32_t> ranges;
	game_random::testing::SetGameRand(
	    [&ranges](uint32_t range) {
		    ranges.push_back(range);
		    return 0u;
	    },
	    [](float) { return 0.0f; });
	ASSERT_EQ(_system.StartFight(a, b, fight::Taunts {.otherRude = false, .ownRude = true}), StartResult::Started);
	EXPECT_TRUE(ranges.empty());
	EXPECT_FALSE(FightingOf(b).rude);
	EXPECT_TRUE(FightingOf(a).rude);
}

// After the arena appears the first creature points at it for twenty turns, then both walk to their places with no
// time limit; the first creature waits for the other, both turn to face each other again, and the other creature is
// told to start its duel first
// A fighter drawn at its temple's pen size walks to the arena's spot and stops within the distance of that size, as the
// game places and brings in its fighters by the size they are drawn at
TEST_F(CreatureFightTurnsTest, AFighterInItsPenWalksToTheArenaByThePenSize)
{
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
	auto& registry = test::creature_world::World::Registry();
	// both fighters are drawn at the pen's size, so neither is too small or too big for the other to fight
	for (const auto creature : {a, b})
	{
		registry.Get<Creature>(creature).size = 1.0f;
		auto& pose = registry.Get<CreatureDrawPose>(creature);
		pose.size = 0.22f;
		pose.given = 0.22f;
	}
	ASSERT_EQ(StartToArena(a, b), StartResult::Started);
	for (int turn = 0; turn < 40 && FightingOf(a).stage != CreatureFighting::Stage::Approach; ++turn)
	{
		_system.ProcessTurn();
	}
	ASSERT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Approach);
	// the walk is asked for on a creature's turn in the approach, which may come after the turn that started it
	const auto walksOf = [this, a]() {
		return std::ranges::count(_locomotion->moves, a, &fakes::FakeLocomotion::Asked::creature);
	};
	const auto before = walksOf();
	for (int turn = 0; turn < 5 && walksOf() == before && FightingOf(a).stage == CreatureFighting::Stage::Approach; ++turn)
	{
		_system.ProcessTurn();
	}
	const auto found = std::ranges::find(_locomotion->moves | std::views::reverse, a, &fakes::FakeLocomotion::Asked::creature);
	ASSERT_NE(found, (_locomotion->moves | std::views::reverse).end());
	EXPECT_EQ(found->point, fight::ArenaSpot(FightingOf(a).arena, 0.22f, FightingOf(a).madeArena));
	EXPECT_EQ(found->value, fight::ArrivalDistance(0.22f));
}

TEST_F(CreatureFightTurnsTest, AfterItsPointBothWalkInSquareUpAndTheOtherCreatureIsToldToDuelFirst)
{
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
	for (const auto animation : {fight::animations::k_Start, fight::animations::k_Stance, fight::animations::k_Finish})
	{
		_animation->durations[animation] = 100000.0f;
	}
	ASSERT_EQ(StartToArena(a, b), StartResult::Started);
	ASSERT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Point);
	for (int turn = 0; turn < 1 + 19; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Point);
	EXPECT_EQ(FightingOf(a).pointTurns, 1u);
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Approach);
	EXPECT_EQ(FightingOf(b).stage, CreatureFighting::Stage::Approach);
	// the walks last as long as the bodies move
	_locomotion->moving = true;
	for (int turn = 0; turn < 1000; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Approach);
	EXPECT_EQ(FightingOf(b).stage, CreatureFighting::Stage::Approach);
	_locomotion->moving = false;
	// at their places, the first creature finds the other ready and squares up to it
	for (int turn = 0; turn < 10 && FightingOf(a).stage != CreatureFighting::Stage::Square; ++turn)
	{
		_system.ProcessTurn();
	}
	ASSERT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Square);
	// the other creature, told to start its duel, starts it no later than the first creature
	bool otherFirst = true;
	for (int turn = 0; turn < 20 && FightingOf(a).stage != CreatureFighting::Stage::Duel; ++turn)
	{
		_system.ProcessTurn();
		if (FightingOf(a).stage == CreatureFighting::Stage::Duel && FightingOf(b).stage != CreatureFighting::Stage::Duel)
		{
			otherFirst = false;
		}
	}
	EXPECT_EQ(FightingOf(a).stage, CreatureFighting::Stage::Duel);
	EXPECT_EQ(FightingOf(b).stage, CreatureFighting::Stage::Duel);
	EXPECT_TRUE(otherFirst);
	EXPECT_TRUE(_system.IsFighting(a));
	EXPECT_TRUE(_system.IsFighting(b));
}

// Each fighter's duel start, not the fight's start, makes its fight reaction for its own player and counts one more
// battle fought
TEST_F(CreatureFightTurnsTest, EachFightersDuelStartMakesItsFightReactionAndCountsABattleFought)
{
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::reactionSystem> keepReactions;
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	const auto& lookup = std::as_const(test::creature_world::World::Registry());
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
	ASSERT_EQ(StartToArena(a, b), StartResult::Started);
	for (const auto self : {a, b})
	{
		EXPECT_EQ(ecs::effects::reactions::GetReactionOfTypeInitiatedBy(self, openblack::Reaction::ReactToFight), 0u);
		EXPECT_EQ(lookup.Get<const CreatureFightRecord>(self).fights, 0u);
	}
	ASSERT_EQ(StartToDuel(a, b), StartResult::Started);
	for (const auto& [self, owner] : {std::pair(a, PlayerNames::PLAYER_TWO), std::pair(b, PlayerNames::PLAYER_THREE)})
	{
		const auto reaction = ecs::effects::reactions::GetReactionOfTypeInitiatedBy(self, openblack::Reaction::ReactToFight);
		ASSERT_NE(reaction, 0u);
		EXPECT_EQ(ecs::effects::reactions::Find(reaction)->player, std::optional(owner));
		EXPECT_EQ(lookup.Get<const CreatureFightRecord>(self).fights, 1u);
	}
}

// The arena a fight makes is a thing of the game's list: it starts its fight reaction for no player as it is made, which
// villagers heed while the fight is on in it, and it goes with its reaction once its maker leaves the fight
TEST_F(CreatureFightTurnsTest, AFightsOwnArenaIsAThingWithAFightReactionUntilItsMakerLeaves)
{
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::reactionSystem> keepReactions;
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto& registry = test::creature_world::World::Registry();
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
	ASSERT_EQ(StartToArena(a, b), StartResult::Started);
	const auto arena = _system.ArenaOf(a);
	ASSERT_TRUE(arena.has_value());
	const auto thing = arena->thing;
	ASSERT_TRUE(thing != entt::null);
	EXPECT_EQ(_system.ArenaOf(b)->thing, thing);
	const auto* made = std::as_const(registry).TryGet<const CreatureFightArena>(thing);
	ASSERT_NE(made, nullptr);
	EXPECT_EQ(made->maker, a);
	const auto& centre = FightingOf(a).arena.centre;
	EXPECT_EQ(made->place, glm::ivec2(map_coords::ToFixed(centre.x), map_coords::ToFixed(centre.y)));
	EXPECT_FLOAT_EQ(made->radius, FightingOf(a).arena.radius);
	const auto reaction = ecs::effects::reactions::GetReactionOfTypeInitiatedBy(thing, openblack::Reaction::ReactToFight);
	ASSERT_NE(reaction, 0u);
	EXPECT_FALSE(ecs::effects::reactions::Find(reaction)->player.has_value());
	// the fight on in it, by the creature that started it
	const auto on = _system.FightIn(thing);
	ASSERT_TRUE(on.has_value());
	EXPECT_EQ(on->maker, a);
	EXPECT_EQ(on->opponent, b);
	EXPECT_FALSE(_system.FightIn(entt::null).has_value());
	EXPECT_FALSE(_system.FightIn(a).has_value());
	// called off before the duel, both leave: the arena goes, and its reaction with it
	_system.AbortFight(a);
	ASSERT_FALSE(_system.IsFighting(a));
	EXPECT_FALSE(registry.Valid(thing));
	EXPECT_EQ(ecs::effects::reactions::Find(reaction), nullptr);
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
	ASSERT_EQ(StartToArena(a, b), StartResult::Started);
	ASSERT_EQ(StartToArena(c, d), StartResult::Started);
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

TEST_F(CreatureFightTurnsTest, ABlowsPointsArePlacedWithTheBodyLeanedOntoTheSlopeWhereItsTurnTakesIt)
{
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	// land rising along x and falling along z
	const auto ground = [](glm::vec2 xz) { return (0.1f * xz.x) - (0.05f * xz.y); };
	Locator::terrainSystem::emplace<test::HeightFieldIsland>(ground);
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	registry.AssignOrReplaceState<CreatureBodyMetrics>(creature, CreatureBodyMetrics {.restHeight = 60.0f, .reach = 8.0f});
	const auto& lookup = std::as_const(registry);
	const glm::vec3 at {104.0f, 3.0f, 97.0f};
	const auto turn = affine::AngleY(0.7f);
	const glm::vec3 scale {1.5f};
	const auto radius = creature_morph::Radius(ecs::creature_pose::DrawnSize(lookup, creature), 60.0f, 8.0f);

	// the land is sampled at the body's radius on the ground about where the turn takes it, and the body sheared by it
	const auto lean = creature_lean::FromHeights(creature_lean::Sample(at, radius, false, ground), radius);
	EXPECT_NE(lean.x, 0.0f);
	EXPECT_NE(lean.z, 0.0f);
	const auto placement = ecs::creature_pose::LeanedModel(lookup, creature, at, turn, scale);
	EXPECT_EQ(placement, creature_lean::Model(at, turn, scale, lean));
	// a blow's point ahead and up the body rises or falls with the slope the way it faces
	const glm::vec4 ahead {0.0f, 5.0f, -10.0f, 1.0f};
	EXPECT_GT(std::abs((placement * ahead).y - (affine::Model(at, turn, scale) * ahead).y), 0.1f);

	// carried by another creature it takes its own height all round, so it does not lean
	registry.AssignOrReplaceState<HeldByCreature>(creature, HeldByCreature {.creature = registry.Create()});
	const auto carried = creature_lean::FromHeights(creature_lean::Sample(at, radius, true, ground), radius);
	EXPECT_EQ(ecs::creature_pose::LeanedModel(lookup, creature, at, turn, scale),
	          creature_lean::Model(at, turn, scale, carried));
	registry.RemoveState<HeldByCreature>(creature);
	// without its body's measures it stands upright
	registry.RemoveState<CreatureBodyMetrics>(creature);
	EXPECT_EQ(ecs::creature_pose::LeanedModel(lookup, creature, at, turn, scale), affine::Model(at, turn, scale));
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
		const auto animation =
		    fighter.state == fight::State::Action ? std::optional<size_t>(fighter.animation) : std::optional<size_t> {};
		_system.AbortFight(a);
		return animation;
	};
	EXPECT_EQ(chosen(false, false), std::optional<size_t>(fight::animations::k_FirstAttack));
	// In its pen it reaches no deeper than 7.5 of the pen's size, and no step helps, so nothing is done
	EXPECT_EQ(chosen(true, false), std::nullopt);
	// An opponent in its pen is 15 of the pen's size tall, so the blow passes over it and nothing is done
	EXPECT_EQ(chosen(false, true), std::nullopt);
}

TEST_F(CreatureFightTurnsTest, ABlowLandsOnlyOnBonesItsLinesMeet)
{
	const game_random::testing::ScopedState state;
	_animation->durations[fight::animations::k_FirstAttack] = 1000.0f;
	// The blow reaches right into the middle of the opponent, as the old test of a point at its reach would have landed
	// it, pen or not; but here the animation poses no striking bone and the opponent has no bones to strike, so the
	// blow's lines meet nothing and it does not land
	const auto blow = [this](bool inPen) {
		const auto [a, b] = Duel();
		if (inPen)
		{
			InPen(a);
		}
		auto& fighting = FightingOf(a);
		fighting.reaches = {fight::Reach {.animation = fight::animations::k_FirstAttack, .reach = 20.0f, .height = 5.0f}};
		fight::Enter(fighting.fighter, fight::State::Action, fight::animations::k_FirstAttack);
		const auto before = FightingOf(b).fighter.health;
		_system.ProcessTurn();
		const auto after = FightingOf(b).fighter.health;
		_system.AbortFight(a);
		return std::pair {before, after};
	};
	for (const bool inPen : {false, true})
	{
		const auto [before, after] = blow(inPen);
		EXPECT_EQ(after, before) << inPen;
	}
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

// The computer's wait to take over the player's creature counts the milliseconds the creature lives through in each
// turn: the whole turn's, and fewer while it is frozen
TEST_F(CreatureFightTurnsTest, TheComputersWaitCountsTheMillisecondsTheCreatureLivesThrough)
{
	const game_random::testing::ScopedState state;
	game_random::testing::SetGameRand([](uint32_t /*n*/) { return 0u; }, [](float) { return 0.0f; });
	const auto [a, b] = Duel();
	const auto msPerTurn = static_cast<int32_t>(game_clock::MsPerTurn());
	auto& fighter = FightingOf(a).fighter;
	fighter.control = fight::Control::Player;
	fighter.state = fight::State::Stance;
	fighter.computerWaitMs = 15000.0f;
	_system.ProcessTurn();
	EXPECT_FLOAT_EQ(FightingOf(a).fighter.computerWaitMs, 15000.0f - static_cast<float>(msPerTurn));

	// frozen to a tenth, it lives through 90 of a 100 ms turn
	test::creature_world::World::Registry().Get<CreatureAnimation>(a).playbackScale = 0.9f;
	FightingOf(a).fighter.computerWaitMs = 15000.0f;
	_system.ProcessTurn();
	EXPECT_FLOAT_EQ(FightingOf(a).fighter.computerWaitMs,
	                15000.0f - static_cast<float>(creature_turn_pose::LivedMs(0.9f, msPerTurn)));
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

// The queue's move is taken from the stance whatever way the fighter faces: turned a quarter away from its opponent, a
// block queued for it starts at once
TEST_F(CreatureFightTurnsTest, AMoveIsTakenFromTheQueueWhicheverWayTheFighterFaces)
{
	const auto [a, b] = Duel();
	auto& registry = test::creature_world::World::Registry();
	// heading 0 faces its opponent
	registry.Get<CreatureLocomotion>(a).heading = std::numbers::pi_v<float> * 0.5f;
	EXPECT_TRUE(_system.QueueScriptMove(a, fight::BlockMove()));
	EXPECT_EQ(FightingOf(a).fighter.state, fight::State::BlockStart);
	EXPECT_EQ(FightingOf(a).fighter.queue.Size(), 0u);
	EXPECT_EQ(FightingOf(b).fighter.state, fight::State::Stance);
}

// Each look at the front of the queue from the stance, in range, teaches the lean the creature has learnt: another
// god's creature only from a step, a blow still waiting teaching nothing to it. The lean the computer chooses by stays
// the one the fight began with
TEST_F(CreatureFightTurnsTest, TheLeanIsLearntAsAMoveIsLookedAtAndTheComputerKeepsTheOneItBeganWith)
{
	const auto [a, b] = Duel();
	auto& registry = test::creature_world::World::Registry();
	auto& record = registry.Get<CreatureFightRecord>(a);
	record.tendency = 0.5f;
	const auto chosen = FightingOf(a).fighter.tendency;
	// a blow waits for its charge: each look at it teaches nothing to another god's creature
	EXPECT_TRUE(_system.QueueScriptMove(a, fight::AttackMove(fight::Band::Mid)));
	EXPECT_FLOAT_EQ(record.tendency, 0.5f);
	FightingOf(a).fighter.queue.Clear();
	// a step teaches it towards defence as it is looked at, and is made
	EXPECT_TRUE(_system.QueueScriptMove(a, fight::StepMove(fight::Step::Back)));
	EXPECT_FLOAT_EQ(record.tendency, (0.98f * 0.5f) - 0.02f);
	EXPECT_FLOAT_EQ(FightingOf(a).fighter.tendency, chosen);
}

// The creature that starts a fight chooses its moves by what it has learnt, against its alignment until it has
// performed a fight; the one it asks keeps the lean it chose by in the last fight it started, none before
TEST_F(CreatureFightTurnsTest, OnlyTheCreatureThatStartsAFightTakesItsLearntLeanToChooseBy)
{
	auto& registry = test::creature_world::World::Registry();
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
	registry.Get<Creature>(a).alignment = 0.5f;
	registry.AssignState<CreatureFightRecord>(a, CreatureFightRecord {.tendency = 0.7f});
	registry.AssignState<CreatureFightRecord>(b, CreatureFightRecord {.tendency = 0.7f});
	_mind->actionCounts[{b, creature_training::k_FightAction}] = 1;
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	EXPECT_FLOAT_EQ(FightingOf(a).fighter.tendency, -0.5f);
	EXPECT_FLOAT_EQ(registry.Get<CreatureFightRecord>(a).tendency, -0.5f);
	EXPECT_FLOAT_EQ(FightingOf(b).fighter.tendency, 0.0f);
	EXPECT_FLOAT_EQ(registry.Get<CreatureFightRecord>(b).tendency, 0.7f);
	_system.AbortFight(a);
}

// Once its mind counts a fight performed, the creature that starts a fight keeps the lean it has learnt
TEST_F(CreatureFightTurnsTest, AFightPerformedKeepsTheLearntLeanAtTheNextStart)
{
	auto& registry = test::creature_world::World::Registry();
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
	registry.Get<Creature>(a).alignment = 0.5f;
	registry.AssignState<CreatureFightRecord>(a, CreatureFightRecord {.tendency = 0.7f});
	_mind->actionCounts[{a, creature_training::k_FightAction}] = 1;
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	EXPECT_FLOAT_EQ(FightingOf(a).fighter.tendency, 0.7f);
	EXPECT_FLOAT_EQ(registry.Get<CreatureFightRecord>(a).tendency, 0.7f);
	_system.AbortFight(a);
}

// As its response after the fight ends, the creature that set out to fight as its plan to fight counts the fight as
// performed, once, when it is left with more life than its opponent: its mind's plan to fight, which the evil leash
// forces too. The other creature, a lower or equal life, a fight from telling the other to go away, or a fight with no
// plan count none
TEST_F(CreatureFightTurnsTest, AFightPlanCountsAsPerformedWhenItsResponseFindsItTheStronger)
{
	auto& registry = test::creature_world::World::Registry();
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
	const auto respond = [&](float life, float otherLife) {
		registry.Get<CreatureNeeds>(a).needs.life = life;
		registry.Get<CreatureNeeds>(b).needs.life = otherLife;
		// both responding, their response animations over
		registry.AssignOrReplaceState<CreatureFighting>(a, CreatureFighting {.stage = CreatureFighting::Stage::Respond,
		                                                                     .opponent = b,
		                                                                     .madeArena = true,
		                                                                     .played = true,
		                                                                     .ended = true});
		registry.AssignOrReplaceState<CreatureFighting>(
		    b, CreatureFighting {.stage = CreatureFighting::Stage::Respond, .opponent = a, .played = true, .ended = true});
		_system.ProcessTurn();
		EXPECT_FALSE(registry.AllOf<CreatureFighting>(a));
		EXPECT_FALSE(registry.AllOf<CreatureFighting>(b));
		return std::pair(_mind->GetActionCount(a, creature_training::k_FightAction),
		                 _mind->GetActionCount(b, creature_training::k_FightAction));
	};
	_mind->plansAction = "Fight";
	EXPECT_EQ(respond(0.8f, 0.5f), std::pair(1u, 0u));
	EXPECT_EQ(respond(0.4f, 0.5f), std::pair(1u, 0u));
	EXPECT_EQ(respond(0.5f, 0.5f), std::pair(1u, 0u));
	_mind->plansAction = "TellCreatureToSodOff";
	EXPECT_EQ(respond(0.8f, 0.5f), std::pair(1u, 0u));
	_mind->plansAction.reset();
	EXPECT_EQ(respond(0.8f, 0.5f), std::pair(1u, 0u));
}

// Its response after the fight over, a creature left with no more life than its opponent is made afraid of it: its fear
// is planned at once for its opponent, the creature that set out to fight and the other alike. One left the stronger,
// or one whose fight was called off and so showed nothing, is not (docs/wiki/creature/fights.md)
TEST_F(CreatureFightTurnsTest, AResponderLeftNoStrongerThanItsOpponentIsMadeAfraidOfIt)
{
	using creature_desires::Desire;
	using Forced = std::tuple<entt::entity, Desire, entt::entity>;
	auto& registry = test::creature_world::World::Registry();
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
	const auto respond = [&](float life, float otherLife, bool responded) {
		_mind->desiresForced.clear();
		registry.Get<CreatureNeeds>(a).needs.life = life;
		registry.Get<CreatureNeeds>(b).needs.life = otherLife;
		// both responding, their response animations over (or, called off, never played)
		registry.AssignOrReplaceState<CreatureFighting>(a, CreatureFighting {.stage = CreatureFighting::Stage::Respond,
		                                                                     .opponent = b,
		                                                                     .madeArena = true,
		                                                                     .played = true,
		                                                                     .ended = true,
		                                                                     .responded = responded});
		registry.AssignOrReplaceState<CreatureFighting>(b, CreatureFighting {.stage = CreatureFighting::Stage::Respond,
		                                                                     .opponent = a,
		                                                                     .played = true,
		                                                                     .ended = true,
		                                                                     .responded = responded});
		_system.ProcessTurn();
		EXPECT_FALSE(registry.AllOf<CreatureFighting>(a));
		EXPECT_FALSE(registry.AllOf<CreatureFighting>(b));
		return _mind->desiresForced;
	};
	EXPECT_EQ(respond(0.8f, 0.5f, true), std::vector<Forced>({{b, Desire::Fear, a}}));
	EXPECT_EQ(respond(0.4f, 0.5f, true), std::vector<Forced>({{a, Desire::Fear, b}}));
	// of two left with the same life, the first to finish is afraid, and its plan failing stops the other's response:
	// the other leaves the fight, what it was doing given up, and is not afraid
	_mind->abandoned.clear();
	const auto equal = respond(0.5f, 0.5f, true);
	ASSERT_EQ(equal.size(), 1u);
	const auto afraid = std::get<0>(equal.front());
	const auto stopped = afraid == a ? b : a;
	EXPECT_EQ(equal.front(), (Forced {afraid, Desire::Fear, stopped}));
	// the other has given up what it was doing, whichever fighter the turn takes first
	EXPECT_NE(std::ranges::find(_mind->abandoned, stopped), _mind->abandoned.end());
	EXPECT_TRUE(respond(0.4f, 0.5f, false).empty());
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

TEST_F(CreatureFightTurnsTest, AForcedFinishHurriesAFightersStartStanceMoveOrFinishUntilItStands)
{
	const auto [a, b] = Duel();
	// in its block it is not hurried
	fight::Enter(FightingOf(a).fighter, fight::State::Block);
	_system.Hurry(a);
	EXPECT_FALSE(FightingOf(a).fighter.hurried);
	// in its stance it is, and its opponent is not
	fight::Enter(FightingOf(a).fighter, fight::State::Stance);
	_system.Hurry(a);
	EXPECT_TRUE(FightingOf(a).fighter.hurried);
	EXPECT_FALSE(FightingOf(b).fighter.hurried);
	for (const auto state : {fight::State::Start, fight::State::Stance, fight::State::Action, fight::State::Finish})
	{
		EXPECT_TRUE(fight::HurriedByForcedFinish(state)) << fight::Name(state);
	}
	for (const auto state : {fight::State::Block, fight::State::Cast, fight::State::Faint, fight::State::GetUp})
	{
		EXPECT_FALSE(fight::HurriedByForcedFinish(state)) << fight::Name(state);
	}
}

namespace
{
/// A creature that has left its duel, in a fight state that only its animation moves on
entt::entity AloneIn(fight::State state)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& fighting = test::creature_world::World::Registry().AssignState<CreatureFighting>(creature);
	fighting.stage = CreatureFighting::Stage::Respond;
	fight::Enter(fighting.fighter, state);
	return creature;
}
} // namespace

// The start of a fight plays on by the span of the turn, and ends when its time and the span reach its end, before it
// moves on; what follows it moves on in the same turn. The finish plays at the fight's pace but ends by the span too.
TEST_F(CreatureFightTurnsTest, TheStartAndTheFinishEndByTheSpanAndWhatFollowsMovesOnInTheSameTurn)
{
	for (const auto animation : {fight::animations::k_Start, fight::animations::k_Finish})
	{
		_animation->durations[animation] = 1000.0f;
	}
	const auto creature = AloneIn(fight::State::Start);
	const auto span = fight::TestSpanMs(
	    100.0f, ecs::creature_pose::DrawnSize(std::as_const(test::creature_world::World::Registry()), creature), false);
	FightingOf(creature).fighter.timeMs = static_cast<float>(1000 - span - 1);
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(creature).fighter.state, fight::State::Start);
	EXPECT_FLOAT_EQ(FightingOf(creature).fighter.timeMs, 999.0f);
	FightingOf(creature).fighter.timeMs = static_cast<float>(1000 - span);
	_system.ProcessTurn();
	// with no opponent it finishes, and the finish moves on in the same turn
	const auto advance = fight::TurnAdvanceMs(100.0f, fight::animations::k_Finish);
	EXPECT_EQ(FightingOf(creature).fighter.state, fight::State::Finish);
	EXPECT_FLOAT_EQ(FightingOf(creature).fighter.timeMs, static_cast<float>(advance));
	FightingOf(creature).fighter.timeMs = static_cast<float>(1000 - span - 1);
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(creature).fighter.state, fight::State::Finish);
	EXPECT_FLOAT_EQ(FightingOf(creature).fighter.timeMs, static_cast<float>(1000 - span - 1 + advance));
	FightingOf(creature).fighter.timeMs = static_cast<float>(1000 - span);
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(creature).fighter.state, fight::State::Idle);
}

// A forced finish doubles only the span of the turn: the start moves on by it and the finish tests its end by it, while
// the finish and a blow move on at the fight speed as unhurried
TEST_F(CreatureFightTurnsTest, AHurriedFighterHasTheSpanOfItsTurnDoubledAndKeepsTheFightSpeed)
{
	for (const auto animation : {fight::animations::k_Start, fight::animations::k_Finish, fight::animations::k_FirstAttack})
	{
		_animation->durations[animation] = 1000.0f;
	}
	const auto creature = AloneIn(fight::State::Start);
	FightingOf(creature).fighter.hurried = true;
	const auto span =
	    2 * fight::TestSpanMs(
	            100.0f, ecs::creature_pose::DrawnSize(std::as_const(test::creature_world::World::Registry()), creature), false);
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(creature).fighter.state, fight::State::Start);
	EXPECT_FLOAT_EQ(FightingOf(creature).fighter.timeMs, static_cast<float>(span));
	FightingOf(creature).fighter.timeMs = static_cast<float>(1000 - span);
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(creature).fighter.state, fight::State::Finish);
	EXPECT_FLOAT_EQ(FightingOf(creature).fighter.timeMs,
	                static_cast<float>(fight::TurnAdvanceMs(100.0f, fight::animations::k_Finish)));

	auto& fighter = FightingOf(creature).fighter;
	fight::Enter(fighter, fight::State::Action, fight::animations::k_FirstAttack);
	fighter.timeMs = 200.0f;
	ASSERT_TRUE(fighter.hurried);
	_system.ProcessTurn();
	EXPECT_FLOAT_EQ(FightingOf(creature).fighter.timeMs, fight::AdvanceClock(200.0f, 100.0f, fight::animations::k_FirstAttack));
}

// The start of a cast ends before it moves on and the casting pose moves on in the same turn; the pose ends as the next
// turn begins, and the end of the cast moves on in that turn
TEST_F(CreatureFightTurnsTest, TheEndOfACastMovesOnInTheTurnThatThePoseEnds)
{
	for (const auto animation : {fight::animations::k_StartCast, fight::animations::k_Cast, fight::animations::k_EndCast})
	{
		_animation->durations[animation] = 1000.0f;
	}
	const auto creature = AloneIn(fight::State::CastStart);
	const auto advanceOf = [](size_t animation) { return static_cast<float>(fight::TurnAdvanceMs(100.0f, animation)); };
	FightingOf(creature).fighter.timeMs = 1000.0f - advanceOf(FightingOf(creature).fighter.animation);
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(creature).fighter.state, fight::State::Cast);
	EXPECT_FLOAT_EQ(FightingOf(creature).fighter.timeMs, advanceOf(FightingOf(creature).fighter.animation));
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(creature).fighter.state, fight::State::CastEnd);
	EXPECT_FLOAT_EQ(FightingOf(creature).fighter.timeMs, advanceOf(FightingOf(creature).fighter.animation));
}

/// Called off while its cast pose plays, a fighter casts nothing: at the pose's end it goes to the cast's end, its spell
// dropped, as the original skips the cast of a fight being ended
TEST_F(CreatureFightTurnsTest, CalledOffInItsCastPoseItCastsNothingAndGoesToTheCastsEnd)
{
	for (const auto animation : {fight::animations::k_StartCast, fight::animations::k_Cast, fight::animations::k_EndCast})
	{
		_animation->durations[animation] = 1000.0f;
	}
	const auto creature = AloneIn(fight::State::CastStart);
	auto& fighter = FightingOf(creature).fighter;
	fighter.spell = static_cast<uint32_t>(MagicType::LightningBolt);
	fighter.stopping = true;
	fighter.timeMs = 1000.0f - static_cast<float>(fight::TurnAdvanceMs(100.0f, fighter.animation));
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(creature).fighter.state, fight::State::CastEnd);
	EXPECT_FALSE(FightingOf(creature).fighter.spell.has_value());
}

// Called off, a fighter leaves its stance at once and its finish moves on in the same turn; a blow under way plays out
// and comes back to the stance first
TEST_F(CreatureFightTurnsTest, CalledOffItFinishesFromItsStanceAtOnceAndABlowComesBackToTheStanceFirst)
{
	for (const auto animation : {fight::animations::k_Stance, fight::animations::k_Finish, fight::animations::k_FirstAttack})
	{
		_animation->durations[animation] = 1000.0f;
	}
	const auto standing = AloneIn(fight::State::Stance);
	FightingOf(standing).fighter.stopping = true;
	const auto striking = test::creature_world::World::MakeCreature(glm::vec3(200.0f, 0.0f, 200.0f));
	auto& blow = test::creature_world::World::Registry().AssignState<CreatureFighting>(striking);
	blow.stage = CreatureFighting::Stage::Respond;
	fight::Enter(blow.fighter, fight::State::Action, fight::animations::k_FirstAttack);
	blow.fighter.timeMs = 950.0f;
	blow.fighter.stopping = true;
	_system.ProcessTurn();
	const auto advance = static_cast<float>(fight::TurnAdvanceMs(100.0f, fight::animations::k_Finish));
	EXPECT_EQ(FightingOf(standing).fighter.state, fight::State::Finish);
	EXPECT_FLOAT_EQ(FightingOf(standing).fighter.timeMs, advance);
	EXPECT_EQ(FightingOf(striking).fighter.state, fight::State::Stance);
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(striking).fighter.state, fight::State::Finish);
	EXPECT_FLOAT_EQ(FightingOf(striking).fighter.timeMs, advance);
}

// Back in its stance as the end of its block or its cast ends, the player's creature takes its queued move at once, and
// the move plays on in that same turn; the computer does not look at the queue
TEST_F(CreatureFightTurnsTest, AMoveQueuedForTheEndOfABlockOrACastPlaysOnInTheTurnItEnds)
{
	const game_random::testing::ScopedState state;
	for (const auto animation : {fight::animations::k_EndBlock, fight::animations::k_EndCast, fight::animations::k_StartBlock})
	{
		_animation->durations[animation] = 1000.0f;
	}
	const auto advance = static_cast<float>(fight::TurnAdvanceMs(100.0f, fight::animations::k_StartBlock));
	for (const auto ending : {fight::State::BlockEnd, fight::State::CastEnd})
	{
		for (const bool computer : {false, true})
		{
			const auto [a, b] = Duel();
			auto& fighter = FightingOf(a).fighter;
			fight::Enter(fighter, ending);
			fighter.timeMs = 1000.0f - static_cast<float>(fight::TurnAdvanceMs(100.0f, fighter.animation));
			if (computer)
			{
				ASSERT_TRUE(fighter.queue.Push(fight::BlockMove(), false));
			}
			else
			{
				ASSERT_TRUE(fight::PlayerMove(fighter, fight::BlockMove(), false));
			}
			_system.ProcessTurn();
			const auto& now = FightingOf(a).fighter;
			if (computer)
			{
				EXPECT_EQ(now.queue.Size(), 1u) << fight::Name(ending);
			}
			else
			{
				EXPECT_EQ(now.state, fight::State::BlockStart) << fight::Name(ending);
				EXPECT_FLOAT_EQ(now.timeMs, advance) << fight::Name(ending);
				EXPECT_EQ(now.queue.Size(), 0u) << fight::Name(ending);
			}
			_system.AbortFight(a);
		}
	}
}

// A blow of 1000 ms at the fight's pace moves on 66 ms a turn, in whole milliseconds: it plays 15 turns, to 990, and
// ends as the 16th begins, the stance it goes back to moving on in that same turn. A block's reel ends the same way
// into the block.
TEST_F(CreatureFightTurnsTest, ABlowAndABlocksReelEndAsTheirTurnBeginsAndWhatFollowsMovesOnInThatTurn)
{
	for (const auto animation : {fight::animations::k_FirstAttack, fight::animations::k_Stance,
	                             fight::animations::k_RecoilBlock, fight::animations::k_Block})
	{
		_animation->durations[animation] = 1000.0f;
	}
	const auto advance = fight::TurnAdvanceMs(100.0f, fight::animations::k_FirstAttack);
	ASSERT_EQ(advance, 66);
	const auto striking = AloneIn(fight::State::Action);
	fight::Enter(FightingOf(striking).fighter, fight::State::Action, fight::animations::k_FirstAttack);
	for (int turn = 1; turn <= 15; ++turn)
	{
		_system.ProcessTurn();
		ASSERT_EQ(FightingOf(striking).fighter.state, fight::State::Action) << turn;
		EXPECT_FLOAT_EQ(FightingOf(striking).fighter.timeMs, static_cast<float>(turn * advance)) << turn;
	}
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(striking).fighter.state, fight::State::Stance);
	EXPECT_FLOAT_EQ(FightingOf(striking).fighter.timeMs, static_cast<float>(advance));

	const auto reeling = AloneIn(fight::State::BlockRecoil);
	FightingOf(reeling).fighter.timeMs = static_cast<float>(1000 - advance - 1);
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(reeling).fighter.state, fight::State::BlockRecoil);
	EXPECT_FLOAT_EQ(FightingOf(reeling).fighter.timeMs, 999.0f);
	FightingOf(reeling).fighter.timeMs = static_cast<float>(1000 - advance);
	_system.ProcessTurn();
	EXPECT_EQ(FightingOf(reeling).fighter.state, fight::State::Block);
	EXPECT_FLOAT_EQ(FightingOf(reeling).fighter.timeMs, static_cast<float>(advance));
}

// The player's creature takes the move queued behind its blow as the blow ends, and the move plays on in that turn
TEST_F(CreatureFightTurnsTest, AMoveQueuedBehindABlowPlaysOnInTheTurnTheBlowEnds)
{
	const game_random::testing::ScopedState state;
	for (const auto animation : {fight::animations::k_FirstAttack, fight::animations::k_StartBlock})
	{
		_animation->durations[animation] = 1000.0f;
	}
	const auto [a, b] = Duel();
	auto& fighter = FightingOf(a).fighter;
	fight::Enter(fighter, fight::State::Action, fight::animations::k_FirstAttack);
	fighter.timeMs = static_cast<float>(1000 - fight::TurnAdvanceMs(100.0f, fight::animations::k_FirstAttack));
	ASSERT_TRUE(fight::PlayerMove(fighter, fight::BlockMove(), false));
	_system.ProcessTurn();
	const auto& now = FightingOf(a).fighter;
	EXPECT_EQ(now.state, fight::State::BlockStart);
	EXPECT_FLOAT_EQ(now.timeMs, static_cast<float>(fight::TurnAdvanceMs(100.0f, fight::animations::k_StartBlock)));
	EXPECT_EQ(now.queue.Size(), 0u);
}

// While a held blow waits, the stance's clock goes round the power-up's length: a time the stance left past the
// power-up's end starts it from 0, and the power-up comes round at its own end
TEST_F(CreatureFightTurnsTest, WhileABlowWaitsTheStanceGoesRoundThePowerUpsLength)
{
	const game_random::testing::ScopedState state;
	_animation->durations[fight::animations::k_Stance] = 1000.0f;
	_animation->durations[fight::animations::k_PowerUp] = 500.0f;
	const auto advance = static_cast<float>(fight::TurnAdvanceMs(100.0f, fight::animations::k_Stance));
	const auto [a, b] = Duel();
	auto& fighter = FightingOf(a).fighter;
	ASSERT_TRUE(fight::PlayerMove(fighter, fight::AttackMove(fight::Band::Mid), false));
	ASSERT_TRUE(fighter.queue.AwaitsRelease());
	fighter.timeMs = 700.0f;
	_system.ProcessTurn();
	ASSERT_EQ(FightingOf(a).fighter.state, fight::State::Stance);
	EXPECT_FLOAT_EQ(FightingOf(a).fighter.timeMs, advance);
	FightingOf(a).fighter.timeMs = 450.0f;
	_system.ProcessTurn();
	EXPECT_FLOAT_EQ(FightingOf(a).fighter.timeMs, 450.0f + advance - 500.0f);
	// let go, the stance goes round its own length again
	ASSERT_TRUE(FightingOf(a).fighter.queue.Release(600.0f));
	FightingOf(a).fighter.queue.Clear();
	FightingOf(a).fighter.timeMs = 700.0f;
	_system.ProcessTurn();
	EXPECT_FLOAT_EQ(FightingOf(a).fighter.timeMs, 700.0f + advance);
}
