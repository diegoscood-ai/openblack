/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The leash system with a fake hand and fake locomotion: who may lead which creature, the refusals kept on the players'
// entities, putting the leash on and taking it off, the area the leash and the home keep the creature within and its
// walk back into it, the rope's ends on the bodies as they are drawn, and the desire each leash and a village it is tied
// to make dominant (the temple's leash posts are the heart's: test_citadel_plan)

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <chrono>
#include <source_location>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/LandAvoid.h"
#include "3D/LandAvoidState.h"
#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "Audio/AudioManagerNoOp.h"
#include "Audio/Game/Banks.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/LeashOwnership.h"
#include "Creature/LeashRope.h"
#include "Creature/LeashRules.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/LeashPost.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerLeash.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHeld.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/MapCells.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PlayerCreature.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/Implementations/LeashSystem.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Systems/LandAvoidSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Town/TownBelief.h"
#include "GameClock.h"
#include "Input/GamePackets.h"
#include "Locator.h"
#include "Worship/LeashPosts.h"
#include "Worship/TempleLeash.h"
#include "creature/CreatureSystemFakes.h"
#include "creature/CreatureSystemWorld.h"
#include "support/LandFakes.h"
#include "support/MagicFakes.h"
#include "support/MapFakes.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::LeashSystem;
using openblack::ecs::systems::PlayerSystem;
using openblack::test::creature_fakes::FakeHand;
using openblack::test::creature_fakes::FakeLocomotion;
using Refusal = openblack::creature_leash::Refusal;
using openblack::creature_desires::Desire;

namespace
{
/// The miracles as the leash sees them: which creatures let go of what they cast
/// The synced float draws asked for, each answered with the same fraction of its range
class PinnedRandom final: public GameRandomInterface
{
public:
	void Init() noexcept override {}
	void Reset() noexcept override {}
	void Load(const GameRandomSeeds& /*seeds*/) noexcept override {}
	[[nodiscard]] const GameRandomSeeds& Current() const noexcept override { return _seeds; }
	uint32_t GameRand(uint32_t /*n*/, std::source_location /*where*/) noexcept override
	{
		++otherDraws;
		return 0;
	}
	float GameFloatRand(float x, std::source_location /*where*/) noexcept override
	{
		floatDraws.push_back(x);
		return x * k_Share;
	}
	uint32_t LocalRand(int32_t /*n*/) noexcept override { return 0; }
	float LocalFloatRand(float /*x*/) noexcept override { return 0.0f; }
	[[nodiscard]] ParticleRandomStream GetParticleStream() const noexcept override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream /*stream*/) noexcept override {}
	void CountOutsideStep(const std::source_location& /*where*/) noexcept override {}
	[[nodiscard]] uint32_t OutsideStepDraws() const noexcept override { return 0; }
	int32_t CrtRand() noexcept override { return 0; }
	void CrtSrand(uint32_t /*seed*/) noexcept override {}
	[[nodiscard]] uint32_t CrtSeed() const noexcept override { return 0; }

	static constexpr float k_Share = 0.5f;
	std::vector<float> floatDraws;
	uint32_t otherDraws {0};

private:
	GameRandomSeeds _seeds {};
};

/// The creature sounds played at once, by creature and action
class HeardSounds final: public ecs::systems::CreatureAudioSystemInterface
{
public:
	void Update(float /*turnFraction*/, std::chrono::duration<float, std::milli> /*gameTime*/) override {}
	void PlayQueued() override {}
	void QueueTurn(const ecs::systems::CreatureAnimationSystemInterface::TurnMoments& /*passed*/,
	               const ecs::systems::CreatureAnimationSystemInterface::TurnInputs& /*atEnd*/) override
	{
	}
	[[nodiscard]] bool IsMuted() const override { return false; }
	void SetMuted(bool /*muted*/) override {}
	[[nodiscard]] bool AreOtherVoicesEnabled() const override { return true; }
	void SetOtherVoicesEnabled(bool /*enabled*/) override {}
	void Play(entt::entity /*creature*/, const creature_audio::SoundEvent& /*event*/) override {}
	void PlayNow(entt::entity creature, audio::SoundAction action) override
	{
		played.emplace_back(creature, action);
		if (heard != nullptr)
		{
			heard->emplace_back("creature's acknowledgement");
		}
	}

	std::vector<std::pair<entt::entity, audio::SoundAction>> played;
	/// Where every sound heard is written in turn, with the in-game bank's samples, when it is set
	std::vector<std::string>* heard {nullptr};
};

/// The in-game bank's samples played, not placed in the world, written in turn where the creature's sounds go too
class InGameSamples final: public audio::AudioManagerNoOp
{
public:
	using AudioManagerNoOp::PlaySoundEffect;
	audio::Channel PlaySoundEffect(const audio::PlayOptions& options) override
	{
		if (heard != nullptr && options.sample.bank == audio::Bank(audio::SfxBank::InGame) && !options.is3D)
		{
			heard->emplace_back("in-game sample " + std::to_string(options.sample.number));
		}
		return audio::k_NoChannel;
	}

	std::vector<std::string>* heard {nullptr};
};

/// The spot visuals started and closed down; each started one is a new entity of its own
class SpotVisuals final: public test::InertParticleSystem
{
public:
	struct Started
	{
		SpotVisualType type;
		glm::vec3 position;
		std::optional<int> turns;
		entt::entity owner;
		float magnitude;
	};
	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> turns, entt::entity owner,
	                             float magnitude) override
	{
		started.push_back({type, position, turns, owner, magnitude});
		// a mark is an object of the world while it is shown, as the game's are
		return test::creature_world::World::Registry().Create();
	}
	void CloseDownSpotVisual(entt::entity object) override { closed.push_back(object); }

	std::vector<Started> started;
	std::vector<entt::entity> closed;
};

class ReleasingMagic final: public test::InertMagicSystem
{
public:
	void ReleaseCreatureCast(entt::entity creature) override { released.push_back(creature); }
	std::vector<entt::entity> released;
};

/// The gestures, counting only how often the hand's path is wiped
class PathGestures final: public ecs::systems::GestureSystemInterface
{
public:
	int forgotten {0};
	std::vector<ecs::systems::GestureEvent> TakeEvents() override { return {}; }
	void Inject(const ecs::systems::GestureEvent& /*event*/) override {}
	void Reset() override {}
	void Update(const Frame& /*frame*/) override {}
	void ForgetPath() override { ++forgotten; }
	[[nodiscard]] float GetScreenAspect() const override { return 1.0f; }
	[[nodiscard]] float GetCircleSecondsLeft() const override { return 0.0f; }
	[[nodiscard]] bool IsGesturing() const override { return false; }
	[[nodiscard]] bool IsLeashPickerOpen() const override { return false; }
};

/// The land's number in the map script's globals
void SetLand(int32_t land)
{
	Locator::mapScriptSystem::value().Globals().landNumber = land;
}

/// The land's number, set for one test and put back at its end
class LandAt
{
public:
	explicit LandAt(int32_t land)
	    : _previous(Locator::mapScriptSystem::value().Globals().landNumber)
	{
		SetLand(land);
	}
	~LandAt() { SetLand(_previous); }
	LandAt(const LandAt&) = delete;
	LandAt& operator=(const LandAt&) = delete;
	LandAt(LandAt&&) = delete;
	LandAt& operator=(LandAt&&) = delete;

private:
	int32_t _previous;
};

/// The game's turn, set for one test and put back at its end
class TurnAt
{
public:
	explicit TurnAt(uint32_t turn)
	    : _previous(game_clock::Turn())
	{
		game_clock::SetTurn(turn);
	}
	~TurnAt() { game_clock::SetTurn(_previous); }
	TurnAt(const TurnAt&) = delete;
	TurnAt& operator=(const TurnAt&) = delete;
	TurnAt(TurnAt&&) = delete;
	TurnAt& operator=(TurnAt&&) = delete;

private:
	uint32_t _previous;
};

class LeashSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_hand = &static_cast<FakeHand&>(Locator::handSystem::emplace<FakeHand>());
		_locomotion = &static_cast<FakeLocomotion&>(Locator::creatureLocomotionSystem::emplace<FakeLocomotion>());
		// a player service of its own: the players' creatures the leash writes there do not reach the other tests
		_players = &static_cast<PlayerSystem&>(Locator::playerSystem::emplace<PlayerSystem>());
		// no spot visuals of the game's own: an order's target marker would start one
		Locator::particleSystem::emplace<test::InertParticleSystem>();
		// 64 x 64 cells of land the creature can stand on, 640 a side
		auto& land = Locator::landAvoidSystem::value().GetState();
		land.size = k_LandCells;
		land.avoid.assign(static_cast<size_t>(k_LandCells * k_LandCells), land_avoid::k_Land);
	}
	/// The land's cell under a point made water
	static void Flood(glm::vec3 point)
	{
		const auto x = static_cast<int32_t>(point.x * 0.1f);
		const auto z = static_cast<int32_t>(point.z * 0.1f);
		Locator::landAvoidSystem::value().GetState().avoid.at(static_cast<size_t>((z * k_LandCells) + x)) = land_avoid::k_Water;
	}

	/// The first player's creature, the one they lead, knowing the learning leash
	entt::entity Leadable(glm::vec3 position = glm::vec3(100.0f, 0.0f, 100.0f))
	{
		const auto creature = test::creature_world::World::MakeCreature(position, PlayerNames::PLAYER_ONE);
		EXPECT_TRUE(_system.SetLeashable(creature, true));
		_system.SetKnown(creature, LeashType::Rope, true);
		return creature;
	}
	static CreatureLeash& LeashOf(entt::entity creature)
	{
		return test::creature_world::World::Registry().Get<CreatureLeash>(creature);
	}
	/// Drawn in its temple's pen: at its own place and turn, smaller than it is
	static void PutInPen(entt::entity creature)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto& transform = registry.Get<Transform>(creature);
		registry.Get<CreatureDrawPose>(creature) = CreatureDrawPose {
		    .position = transform.position, .rotation = transform.rotation, .scale = transform.scale * k_PenShare};
	}
	/// High on the body, as with no collar bone (no rig is loaded here), the body drawn at `share` of its size
	static glm::vec3 HighCollar(entt::entity creature, float share)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto height = creature_morph::k_HeightAtSizeOne * registry.Get<Creature>(creature).size;
		return registry.Get<Transform>(creature).position +
		       glm::vec3(0.0f, height * share * creature_leash::k_CollarHeightShare, 0.0f);
	}
	/// How much of its size a creature in its pen is drawn at, as the drawn pose's scale over its own gives it
	static float PenShare(entt::entity creature)
	{
		const auto& transform = test::creature_world::World::Registry().Get<Transform>(creature);
		return (transform.scale.x * k_PenShare) / transform.scale.x;
	}
	static void ExpectPoint(const glm::vec3& point, const glm::vec3& expected)
	{
		EXPECT_FLOAT_EQ(point.x, expected.x);
		EXPECT_FLOAT_EQ(point.y, expected.y);
		EXPECT_FLOAT_EQ(point.z, expected.z);
	}

	/// Every desire of the creature active, each wanted a little more than the one before, none held down
	static CreatureMindState& GiveDesires(entt::entity creature)
	{
		auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
		creature_desires::Desires desires;
		for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
		{
			auto& state = desires.desires.at(i);
			state.activated = true;
			state.max = 1.0f;
			state.value = 0.2f + (0.01f * static_cast<float>(i));
		}
		mind.desires = desires;
		return mind;
	}
	/// The turns of a dominant desire counted, as the mind counts them
	static void CountTurns(CreatureMindState& mind, uint32_t turns)
	{
		for (uint32_t i = 0; i < turns; ++i)
		{
			static_cast<void>(creature_spell_mind::StepCheat(*mind.desires, mind.dominantDesire, 10.0f));
		}
	}
	/// A village of the first player, believing in them and in the second player as asked, and its centre
	entt::entity MakeTownCentre(glm::vec3 position, float beliefInOne, float beliefInTwo)
	{
		constexpr uint32_t k_TownId = 7;
		auto& info = _world.Info();
		info.abode.at(1).abodeNumber = AbodeNumber::TownCentre;
		info.abode.at(1).abodeType = AbodeType::TownCentre;
		auto& registry = test::creature_world::World::Registry();
		const auto town = registry.Create();
		auto& village = registry.Assign<Town>(town, k_TownId);
		village.owner = PlayerNames::PLAYER_ONE;
		ecs::town_belief::SetBelief(village.belief, PlayerNames::PLAYER_ONE, beliefInOne);
		ecs::town_belief::SetBelief(village.belief, PlayerNames::PLAYER_TWO, beliefInTwo);
		registry.Context().towns[k_TownId] = town;
		_town = town;
		const auto centre = registry.Create();
		registry.Assign<Abode>(centre, AbodeNumber::TownCentre, k_TownId, 0u, 0u);
		registry.Assign<Transform>(centre, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return centre;
	}

	static constexpr float k_PenShare = 0.22f;
	static constexpr int32_t k_LandCells = 64;
	static constexpr float k_Frame = 1.0f / 30.0f;
	/// A player's entity on the land, which their refusals are kept on
	static entt::entity MakePlayer(PlayerNames name)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto player = registry.Create();
		registry.Assign<Player>(player, name);
		return player;
	}

	test::creature_world::World _world;
	const test::RestoreService<Locator::handSystem> _restoreHand;
	const test::RestoreService<Locator::creatureLocomotionSystem> _restoreLocomotion;
	const test::RestoreService<Locator::playerSystem> _restorePlayers;
	const test::RestoreService<Locator::particleSystem> _restoreParticles;
	FakeHand* _hand {nullptr};
	FakeLocomotion* _locomotion {nullptr};
	PlayerSystem* _players {nullptr};
	LeashSystem _system;
	/// The village MakeTownCentre made last
	entt::entity _town {entt::null};
};
} // namespace

TEST_F(LeashSystemTest, WhoMayLeadWhichCreature)
{
	MakePlayer(PlayerNames::PLAYER_ONE);
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_ONE);
	// not yet the one its player leads
	EXPECT_FALSE(_system.PutOn(creature, LeashType::Rope));
	ASSERT_TRUE(_system.LastRefusal(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_EQ(_system.LastRefusal(PlayerNames::PLAYER_ONE)->why, Refusal::NotLeashable);
	ASSERT_TRUE(_system.SetLeashable(creature, true));
	// it must know the learning leash first
	EXPECT_EQ(_system.WhyNot(PlayerNames::PLAYER_ONE, creature, LeashType::Rope), Refusal::DoesNotKnowLearningLeash);
	_system.SetKnown(creature, LeashType::Rope, true);
	EXPECT_EQ(_system.WhyNot(PlayerNames::PLAYER_ONE, creature, LeashType::Rope), Refusal::None);
	// another player's, and what is no creature
	EXPECT_EQ(_system.WhyNot(PlayerNames::PLAYER_TWO, creature, LeashType::Rope), Refusal::OwnedByAnother);
	EXPECT_EQ(_system.WhyNot(PlayerNames::PLAYER_ONE, test::creature_world::World::Registry().Create(), LeashType::Rope),
	          Refusal::NotACreature);
	EXPECT_EQ(_system.PlayersCreature(PlayerNames::PLAYER_ONE), std::optional(creature));
	// nobody's creature can't be the one anyone leads
	const auto wild = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::NEUTRAL);
	EXPECT_FALSE(_system.SetLeashable(wild, true));
	EXPECT_FALSE(_system.IsLeashable(wild));
}

TEST_F(LeashSystemTest, TheOneLedIsKeptAsThePlayersCreature)
{
	const auto first = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_ONE);
	const auto second = test::creature_world::World::MakeCreature(glm::vec3(50.0f, 0.0f, 0.0f), PlayerNames::PLAYER_ONE);
	// each creature as it arrives, the newest winning, then the one chosen last
	_system.ClaimOnArrival(first);
	EXPECT_EQ(_players->GetPrimaryCreature(PlayerNames::PLAYER_ONE), std::optional(first));
	_system.ClaimOnArrival(second);
	EXPECT_EQ(_players->GetPrimaryCreature(PlayerNames::PLAYER_ONE), std::optional(second));
	ASSERT_TRUE(_system.SetLeashable(first, true));
	EXPECT_EQ(_players->GetPrimaryCreature(PlayerNames::PLAYER_ONE), std::optional(first));
	ASSERT_TRUE(_system.SetLeashable(second, true));
	EXPECT_EQ(_players->GetPrimaryCreature(PlayerNames::PLAYER_ONE), std::optional(second));
	// no longer the one led: none
	ASSERT_TRUE(_system.SetLeashable(second, false));
	EXPECT_FALSE(_players->GetPrimaryCreature(PlayerNames::PLAYER_ONE).has_value());
	// given away, it is its new owner's when they lead no other, and its last owner's no more
	ASSERT_TRUE(_system.SetLeashable(first, true));
	_system.SetOwner(first, PlayerNames::PLAYER_TWO);
	EXPECT_FALSE(_players->GetPrimaryCreature(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_EQ(_players->GetPrimaryCreature(PlayerNames::PLAYER_TWO), std::optional(first));
	// given to a player who already leads one, it is not theirs
	_system.ClaimOnArrival(second);
	ASSERT_TRUE(_system.IsLeashable(second));
	_system.SetOwner(second, PlayerNames::PLAYER_TWO);
	EXPECT_FALSE(_system.IsLeashable(second));
	EXPECT_FALSE(_players->GetPrimaryCreature(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_EQ(_players->GetPrimaryCreature(PlayerNames::PLAYER_TWO), std::optional(first));
	// nobody leads a creature of the neutral player, though the neutral player's slot names it as the newest made
	const auto wild = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::NEUTRAL);
	_system.ClaimOnArrival(wild);
	EXPECT_FALSE(_system.IsLeashable(wild));
	EXPECT_EQ(_players->GetPrimaryCreature(PlayerNames::NEUTRAL), std::optional(wild));
}

TEST_F(LeashSystemTest, ASecondCreatureTakesTheLeashFromTheFirst)
{
	MakePlayer(PlayerNames::PLAYER_ONE);
	const auto first = Leadable();
	ASSERT_TRUE(_system.PutOn(first, LeashType::Rope));
	const auto second = test::creature_world::World::MakeCreature(glm::vec3(150.0f, 0.0f, 100.0f), PlayerNames::PLAYER_ONE);
	_system.ClaimOnArrival(second);
	EXPECT_TRUE(_system.IsLeashable(second));
	EXPECT_FALSE(_system.IsLeashable(first));
	EXPECT_FALSE(_system.IsLeashed(first));
	EXPECT_EQ(_system.PlayersCreature(PlayerNames::PLAYER_ONE), std::optional(second));
	// another player's creature leaves this player's alone
	const auto theirs = test::creature_world::World::MakeCreature(glm::vec3(200.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	_system.ClaimOnArrival(theirs);
	EXPECT_TRUE(_system.IsLeashable(second));
	EXPECT_TRUE(_system.IsLeashable(theirs));
}

TEST_F(LeashSystemTest, TheNeutralPlayersCreatureIsNotLed)
{
	const auto wild = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::NEUTRAL);
	_players->SetPrimaryCreature(PlayerNames::NEUTRAL, wild);
	ASSERT_EQ(_players->GetPrimaryCreature(PlayerNames::NEUTRAL), std::optional(wild));
	EXPECT_FALSE(_system.PlayersCreature(PlayerNames::NEUTRAL).has_value());
	EXPECT_FALSE(_system.PlayersCreature(PlayerNames::_COUNT).has_value());
}

TEST_F(LeashSystemTest, ARefusalIsKeptOnThePlayersEntity)
{
	auto& registry = test::creature_world::World::Registry();
	const auto& lookup = std::as_const(registry);
	const auto storages = [&lookup]() {
		size_t count = 0;
		lookup.EachStorage([&count](entt::id_type, const auto&) { ++count; });
		return count;
	};
	// a player with no entity on the land keeps no refusal, and asking or refusing makes no storage
	const auto before = storages();
	EXPECT_FALSE(_system.PressKey(PlayerNames::PLAYER_TWO, creature_leash::LeashKey::Leash));
	EXPECT_FALSE(_system.LastRefusal(PlayerNames::PLAYER_TWO).has_value());
	EXPECT_EQ(storages(), before);

	// a player with no creature to lead: kept on their entity, with no creature
	const auto player = MakePlayer(PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(_system.LastRefusal(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_FALSE(_system.PressKey(PlayerNames::PLAYER_ONE, creature_leash::LeashKey::Leash));
	auto refused = _system.LastRefusal(PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(refused.has_value());
	EXPECT_EQ(refused->player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(refused->creature, entt::entity {entt::null});
	EXPECT_EQ(refused->why, Refusal::NotACreature);
	ASSERT_NE(lookup.TryGet<const PlayerLeashRefusal>(player), nullptr);

	// the next refusal takes its place; the other player still has none
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(_system.PutOn(creature, LeashType::Rope));
	refused = _system.LastRefusal(PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(refused.has_value());
	EXPECT_EQ(refused->creature, creature);
	EXPECT_EQ(refused->why, Refusal::NotLeashable);
	EXPECT_FALSE(_system.LastRefusal(PlayerNames::PLAYER_TWO).has_value());
}

TEST_F(LeashSystemTest, OnOffToggleAndChange)
{
	const auto creature = Leadable();
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	EXPECT_TRUE(_system.IsLeashed(creature));
	EXPECT_EQ(_system.TypeOf(creature), LeashType::Rope);
	// a leash it does not know can't be swapped in
	EXPECT_FALSE(_system.ChangeType(creature, LeashType::Evil));
	_system.SetKnown(creature, LeashType::Evil, true);
	EXPECT_TRUE(_system.ChangeType(creature, LeashType::Evil));
	EXPECT_EQ(_system.TypeOf(creature), LeashType::Evil);
	// the leash key takes it off, and puts the picked one on again
	EXPECT_TRUE(_system.Toggle(creature));
	EXPECT_FALSE(_system.IsLeashed(creature));
	EXPECT_EQ(_system.TypeOf(creature), LeashType::None);
	EXPECT_TRUE(_system.Toggle(creature));
	EXPECT_EQ(_system.TypeOf(creature), LeashType::Evil);
	_system.TakeOff(creature);
	EXPECT_FALSE(_system.IsLeashed(creature));
}

TEST_F(LeashSystemTest, SetTypeChangesTheLeashWhetherOrNotItIsKnown)
{
	const auto creature = Leadable();
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	ASSERT_FALSE(_system.Knows(creature, LeashType::Evil));
	// worn: the leash becomes it though the creature does not know it
	_system.SetType(creature, LeashType::Evil);
	EXPECT_EQ(_system.TypeOf(creature), LeashType::Evil);
	EXPECT_EQ(_system.Picked(creature), LeashType::Evil);
	EXPECT_FALSE(_system.Knows(creature, LeashType::Evil));
	// not worn: only the pick
	_system.TakeOff(creature);
	_system.SetType(creature, LeashType::Good);
	EXPECT_FALSE(_system.IsLeashed(creature));
	EXPECT_EQ(_system.Picked(creature), LeashType::Good);
}

TEST_F(LeashSystemTest, ThePickedLeashIsReportedWornOrNot)
{
	auto& registry = test::creature_world::World::Registry();
	const auto& lookup = std::as_const(registry);
	const auto storages = [&lookup]() {
		size_t count = 0;
		lookup.EachStorage([&count](entt::id_type, const auto&) { ++count; });
		return count;
	};
	// a thing with no leashes has none picked, and asking makes no storage
	const auto thing = registry.Create();
	const auto before = storages();
	EXPECT_EQ(_system.Picked(thing), LeashType::None);
	EXPECT_EQ(storages(), before);

	const auto creature = Leadable();
	// nothing picked yet: none, though the hotkeys would put the rope on
	EXPECT_EQ(_system.Picked(creature), LeashType::None);
	_system.SetKnown(creature, LeashType::Evil, true);
	ASSERT_TRUE(_system.ChangeType(creature, LeashType::Evil));
	// picked but not put on: no leash is worn, the pick is still reported
	EXPECT_EQ(_system.TypeOf(creature), LeashType::None);
	EXPECT_EQ(_system.Picked(creature), LeashType::Evil);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	EXPECT_EQ(_system.Picked(creature), LeashType::Rope);
	_system.TakeOff(creature);
	EXPECT_EQ(_system.Picked(creature), LeashType::Rope);
}

TEST_F(LeashSystemTest, ATautRopeMovesNothingWithinTheLeashsLength)
{
	const auto creature = Leadable();
	_hand->position = glm::vec3(150.0f, 0.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	LeashOf(creature).worn->rope.tension = 1.0f;
	_system.ProcessTurn();
	EXPECT_TRUE(_locomotion->moves.empty());
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Idle);
	EXPECT_FALSE(LeashOf(creature).returning);
}

TEST_F(LeashSystemTest, BeyondTheLeashsLengthItWalksBackToTheHand)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	mind.planActive = true;
	mind.planner.current = creature_planner::Plan {.desire = Desire::Hunger};
	// the hand high above the land 120 away: the creature is kept within the leash's full length of its place on the
	// ground
	_hand->position = glm::vec3(220.0f, 30.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	const auto radius = creature_leash::InHand(registry.Get<Creature>(creature).size).max;
	ASSERT_LT(radius, 120.0f);
	_system.ProcessTurn();
	EXPECT_EQ(LeashOf(creature).confinementCentre, glm::vec3(220.0f, 0.0f, 100.0f));
	EXPECT_EQ(LeashOf(creature).confinementRadius, radius);

	// it sets off at once, at its walk sped up by how far it strayed, until it is within the radius of the point
	ASSERT_EQ(_locomotion->moves.size(), 1u);
	const auto walk = _locomotion->moves.front();
	EXPECT_TRUE(walk.walkBack);
	EXPECT_EQ(walk.point, glm::vec2(220.0f, 100.0f));
	const auto distance =
	    gutils::GetDistanceInMetres(registry.Get<Transform>(creature).position, glm::vec3(220.0f, 0.0f, 100.0f));
	// the game's map distance, through its 1/sqrt table: 120 to within the table's precision, not exactly 120
	EXPECT_NEAR(distance, 120.0f, 120.0f / 1024.0f);
	EXPECT_EQ(walk.hurry, creature_leash::WalkBackHurry(distance, radius));
	EXPECT_FLOAT_EQ(walk.hurry, distance * 0.8f / (2.0f * radius));
	EXPECT_EQ(walk.value, creature_leash::WalkBackArrival(
	                          creature_morph::k_HeightAtSizeOne * registry.Get<Creature>(creature).size, radius));
	EXPECT_EQ(walk.value, radius);
	EXPECT_TRUE(LeashOf(creature).returning);
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Led);
	// its mind gives up what it was doing and obeys: its plan is to walk to the point, about itself
	EXPECT_TRUE(mind.leash.obeying);
	EXPECT_FALSE(mind.planActive);
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->desire, Desire::ObeyPlayer);
	EXPECT_EQ(mind.planner.current->action, creature_leash::k_WalkToPointAction);
	// forced on it, as pressing as a plan can be
	EXPECT_EQ(mind.planner.current->priority, creature_planner::k_ForcedScore);
	EXPECT_EQ(mind.planner.current->object, static_cast<uint32_t>(creature));

	// on its way, it is not sent again while the hand stays within half the radius of where it walks to
	_locomotion->moving = true;
	_system.ProcessTurn();
	EXPECT_EQ(_locomotion->moves.size(), 1u);
	_hand->position->x = 220.0f + (radius * 0.5f) - 1.0f;
	_system.ProcessTurn();
	EXPECT_EQ(_locomotion->moves.size(), 1u);
	// the hand moved farther: it sets off for its new place
	_hand->position->x = 220.0f + (radius * 0.5f) + 1.0f;
	_system.ProcessTurn();
	ASSERT_EQ(_locomotion->moves.size(), 2u);
	EXPECT_EQ(_locomotion->moves.back().point, glm::vec2(_hand->position->x, 100.0f));

	// arrived, its mind takes over again
	_locomotion->moving = false;
	registry.Get<Transform>(creature).position = glm::vec3(_hand->position->x - 10.0f, 0.0f, 100.0f);
	_system.ProcessTurn();
	EXPECT_EQ(_locomotion->moves.size(), 2u);
	EXPECT_FALSE(LeashOf(creature).returning);
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Idle);
	EXPECT_FALSE(mind.leash.obeying);
}

TEST_F(LeashSystemTest, SentToAPointOfTheLandItWalksThereAndIsNotPulledBackOnTheWay)
{
	// dry land everywhere, which the land's cells answer for
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	auto& island = static_cast<test::WaterCellIsland&>(Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {4}));
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	mind.planActive = true;
	mind.planner.current = creature_planner::Plan {.desire = Desire::Hunger};
	mind.turnsSinceMadeToAct = 50;
	_hand->position = glm::vec3(100.0f, 30.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));

	// it walks to within its height of the point, at most 8, and remembers the point as where it walks to
	const glm::vec3 point {160.0f, 0.0f, 100.0f};
	_system.ActOnPoint(creature, point);
	ASSERT_EQ(_locomotion->moves.size(), 1u);
	const auto walk = _locomotion->moves.front();
	EXPECT_FALSE(walk.walkBack);
	EXPECT_FALSE(walk.run);
	EXPECT_EQ(walk.point, glm::vec2(160.0f, 100.0f));
	EXPECT_EQ(walk.value,
	          creature_leash::PointOrderArrival(creature_morph::k_HeightAtSizeOne * registry.Get<Creature>(creature).size));
	EXPECT_TRUE(LeashOf(creature).returning);
	EXPECT_EQ(LeashOf(creature).returningTo, point);
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Sent);
	EXPECT_EQ(mind.turnsSinceMadeToAct, 0u);
	// its mind obeys: the walk to the point, about itself, forced on it
	EXPECT_TRUE(mind.leash.obeying);
	EXPECT_FALSE(mind.planActive);
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->desire, Desire::ObeyPlayer);
	EXPECT_EQ(mind.planner.current->action, creature_leash::k_WalkToPointAction);
	EXPECT_EQ(mind.planner.current->priority, creature_planner::k_ForcedScore);
	EXPECT_EQ(mind.planner.current->object, static_cast<uint32_t>(creature));

	// on its way, the hand far off pulls it back no more
	_locomotion->moving = true;
	_hand->position = glm::vec3(500.0f, 30.0f, 500.0f);
	_system.ProcessTurn();
	EXPECT_EQ(_locomotion->moves.size(), 1u);
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Sent);
	// (sent again while it obeys, it runs and then waits: SentAgainWhileItObeysItRunsThereAndWaitsAMoment)

	// once it stops, its mind takes over again
	_locomotion->moving = false;
	_hand->position = glm::vec3(100.0f, 30.0f, 100.0f);
	_system.ProcessTurn();
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Idle);
	EXPECT_FALSE(LeashOf(creature).returning);
	EXPECT_FALSE(mind.leash.obeying);

	// on the sea it doesn't walk, not wanting to drink from it
	auto& sea = island.Cell(glm::u16vec2(20, 20));
	sea.properties.hasWater = 1;
	_system.ActOnPoint(creature, glm::vec3(205.0f, 0.0f, 205.0f));
	EXPECT_EQ(_locomotion->moves.size(), 1u);
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Sent);
}

// Holding something, sent to a point of the land it walks to within its height of it, never running, carries the thing
// there and puts it down once it stops; the walk is over once its hands are free. Its turns since it was made to act go
// on, it waits nothing, and a walk it can't start puts nothing down
TEST_F(LeashSystemTest, SentToAPointHoldingSomethingItCarriesItThereAndPutsItDown)
{
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {4});
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	auto& hands = static_cast<openblack::test::creature_fakes::FakeObjectAction&>(
	    Locator::creatureObjectActionSystem::emplace<openblack::test::creature_fakes::FakeObjectAction>());
	hands.putDownStarts = true;
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	mind.turnsSinceMadeToAct = 50;
	registry.Assign<CreatureHeldObject>(creature, CreatureHeldObject {.object = registry.Create()});
	_hand->position = glm::vec3(100.0f, 30.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));

	const glm::vec3 point {160.0f, 0.0f, 100.0f};
	_system.ActOnPoint(creature, point);
	ASSERT_EQ(_locomotion->moves.size(), 1u);
	const auto walk = _locomotion->moves.front();
	EXPECT_FALSE(walk.run);
	EXPECT_EQ(walk.point, glm::vec2(160.0f, 100.0f));
	// within its height of the point, with no cap
	EXPECT_EQ(walk.value, creature_morph::k_HeightAtSizeOne * registry.Get<Creature>(creature).size);
	EXPECT_TRUE(LeashOf(creature).returning);
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Sent);
	EXPECT_TRUE(mind.leash.obeying);
	EXPECT_EQ(mind.turnsSinceMadeToAct, 50u);
	EXPECT_FALSE(LeashOf(creature).waitAfterWalk.has_value());
	EXPECT_EQ(LeashOf(creature).putDownAfterWalk, CreatureLeash::PutDownAfterWalk::Due);

	// on the way nothing is put down
	_locomotion->moving = true;
	_system.ProcessTurn();
	EXPECT_TRUE(hands.putDown.empty());

	// once it stops it puts the thing down, and stays sent while its hands are busy
	_locomotion->moving = false;
	_system.ProcessTurn();
	ASSERT_EQ(hands.putDown.size(), 1u);
	EXPECT_EQ(hands.putDown.front(), creature);
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Sent);
	hands.state = ecs::systems::CreatureObjectActionSystemInterface::State::Busy;
	_system.ProcessTurn();
	EXPECT_EQ(hands.putDown.size(), 1u);
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Sent);
	EXPECT_TRUE(mind.leash.obeying);
	// its hands free, its mind takes over again
	hands.state = ecs::systems::CreatureObjectActionSystemInterface::State::Done;
	_system.ProcessTurn();
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Idle);
	EXPECT_FALSE(mind.leash.obeying);
	EXPECT_EQ(LeashOf(creature).putDownAfterWalk, CreatureLeash::PutDownAfterWalk::None);

	// a walk that doesn't start puts nothing down, and the order ends the next turn
	_locomotion->moveToResult = ecs::systems::CreatureLocomotionSystemInterface::MoveResult::InvalidDestination;
	_system.ActOnPoint(creature, point);
	EXPECT_EQ(LeashOf(creature).putDownAfterWalk, CreatureLeash::PutDownAfterWalk::None);
	_system.ProcessTurn();
	EXPECT_EQ(hands.putDown.size(), 1u);
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Idle);
}

TEST_F(LeashSystemTest, SentToAPointHoldingSomethingOnTheAggressionLeashItWalksToThrowItThere)
{
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {4});
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	auto& hands = static_cast<openblack::test::creature_fakes::FakeObjectAction&>(
	    Locator::creatureObjectActionSystem::emplace<openblack::test::creature_fakes::FakeObjectAction>());
	hands.throwStarts = true;
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	mind.turnsSinceMadeToAct = 50;
	registry.Assign<CreatureHeldObject>(creature, CreatureHeldObject {.object = registry.Create()});
	_hand->position = glm::vec3(100.0f, 30.0f, 100.0f);
	_system.SetKnown(creature, LeashType::Evil, true);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Evil));
	const auto& at = registry.Get<Transform>(creature).position;
	const float height = creature_morph::k_HeightAtSizeOne * registry.Get<Creature>(creature).size;

	// far off: it walks towards the point, to stop eight times its height from it
	const glm::vec3 point {at.x + 20.0f * height, 0.0f, at.z};
	_system.ActOnPoint(creature, point);
	ASSERT_EQ(_locomotion->moves.size(), 1u);
	const auto walk = _locomotion->moves.front();
	EXPECT_FALSE(walk.run);
	EXPECT_EQ(walk.point, glm::vec2(point.x, point.z));
	EXPECT_EQ(walk.minimum, 8.0f * height);
	EXPECT_EQ(walk.value, 8.0f * height + 0.01f);
	EXPECT_TRUE(LeashOf(creature).returning);
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Sent);
	EXPECT_TRUE(mind.leash.obeying);
	EXPECT_EQ(mind.turnsSinceMadeToAct, 50u);
	EXPECT_EQ(LeashOf(creature).putDownAfterWalk, CreatureLeash::PutDownAfterWalk::Due);
	ASSERT_TRUE(LeashOf(creature).throwAt.has_value());
	const glm::vec3 target {point.x, Locator::terrainSystem::value().GetHeightAt({point.x, point.z}), point.z};
	EXPECT_EQ(*LeashOf(creature).throwAt, target);

	// on the way nothing is thrown; once it stops it throws the thing at the point on the ground, and is done once its
	// hands are free
	_locomotion->moving = true;
	_system.ProcessTurn();
	EXPECT_TRUE(hands.thrown.empty());
	_locomotion->moving = false;
	_system.ProcessTurn();
	ASSERT_EQ(hands.thrown.size(), 1u);
	EXPECT_EQ(hands.thrown.front().first, creature);
	EXPECT_EQ(hands.thrown.front().second, target);
	EXPECT_TRUE(hands.putDown.empty());
	hands.state = ecs::systems::CreatureObjectActionSystemInterface::State::Busy;
	_system.ProcessTurn();
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Sent);
	hands.state = ecs::systems::CreatureObjectActionSystemInterface::State::Done;
	_system.ProcessTurn();
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Idle);
	EXPECT_FALSE(mind.leash.obeying);

	// a walk that doesn't start throws nothing
	hands.state = ecs::systems::CreatureObjectActionSystemInterface::State::Done;
	_system.ProcessTurn();
	_locomotion->moveToResult = ecs::systems::CreatureLocomotionSystemInterface::MoveResult::InvalidDestination;
	_system.ActOnPoint(creature, point);
	EXPECT_EQ(LeashOf(creature).putDownAfterWalk, CreatureLeash::PutDownAfterWalk::None);
	EXPECT_FALSE(LeashOf(creature).throwAt.has_value());
	_system.ProcessTurn();
	EXPECT_EQ(hands.thrown.size(), 1u);
}

TEST_F(LeashSystemTest, SentAgainWhileItObeysItRunsThereAndWaitsAMoment)
{
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {4});
	const test::RestoreService<Locator::gameRandom> restoreRandom;
	auto& random = static_cast<PinnedRandom&>(Locator::gameRandom::emplace<PinnedRandom>());
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	_hand->position = glm::vec3(100.0f, 30.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	random.floatDraws.clear();
	random.otherDraws = 0;

	// the first order walks, with no wait and no draw
	_system.ActOnPoint(creature, glm::vec3(130.0f, 0.0f, 100.0f));
	ASSERT_EQ(_locomotion->moves.size(), 1u);
	EXPECT_FALSE(_locomotion->moves.back().run);
	EXPECT_FALSE(LeashOf(creature).waitAfterWalk.has_value());
	EXPECT_TRUE(random.floatDraws.empty());

	// the second, while it obeys, runs and draws the wait at once: 2 s and the draw of up to 3 more, in whole turns
	_system.ActOnPoint(creature, glm::vec3(140.0f, 0.0f, 100.0f));
	ASSERT_EQ(_locomotion->moves.size(), 2u);
	EXPECT_TRUE(_locomotion->moves.back().run);
	EXPECT_EQ(random.floatDraws, (std::vector<float> {3.0f}));
	EXPECT_EQ(random.otherDraws, 0u);
	const auto turns = creature_leash::SecondOrderWaitTurns(3.0f * PinnedRandom::k_Share, game_clock::MsPerTurn());
	ASSERT_EQ(LeashOf(creature).waitAfterWalk.value_or(-1), turns);
	ASSERT_GT(turns, 1);

	// arrived, it waits there, obeying, for all those turns, then its mind takes over
	_locomotion->moving = false;
	for (int32_t turn = 1; turn < turns; ++turn)
	{
		_system.ProcessTurn();
		EXPECT_EQ(LeashOf(creature).waitAfterWalk.value_or(-1), turns - turn);
		EXPECT_TRUE(LeashOf(creature).returning);
		EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Sent);
		EXPECT_TRUE(mind.leash.obeying);
	}
	_system.ProcessTurn();
	EXPECT_FALSE(LeashOf(creature).waitAfterWalk.has_value());
	EXPECT_FALSE(LeashOf(creature).returning);
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Idle);
	EXPECT_FALSE(mind.leash.obeying);
	EXPECT_EQ(_locomotion->moves.size(), 2u);

	// a first order again: no wait; and a new walk gives up a wait it had
	_system.ActOnPoint(creature, glm::vec3(130.0f, 0.0f, 100.0f));
	EXPECT_FALSE(LeashOf(creature).waitAfterWalk.has_value());
	_system.ActOnPoint(creature, glm::vec3(140.0f, 0.0f, 100.0f));
	ASSERT_TRUE(LeashOf(creature).waitAfterWalk.has_value());
	_system.PullAwayFromAction(creature);
	EXPECT_FALSE(LeashOf(creature).waitAfterWalk.has_value());
	EXPECT_EQ(random.floatDraws.size(), 2u);
}

TEST_F(LeashSystemTest, TakingAnOrderOnTheLandItAcknowledgesIt)
{
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {4});
	const test::RestoreService<Locator::creatureAudioSystem> restoreAudio;
	auto& sounds = static_cast<HeardSounds&>(Locator::creatureAudioSystem::emplace<HeardSounds>());
	const auto creature = Leadable();
	GiveDesires(creature);
	_hand->position = glm::vec3(100.0f, 30.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));

	// sent walking, it makes its acknowledging sound, once an order
	_system.ActOnPoint(creature, glm::vec3(130.0f, 0.0f, 100.0f));
	ASSERT_EQ(sounds.played.size(), 1u);
	EXPECT_EQ(sounds.played.front().first, creature);
	EXPECT_EQ(sounds.played.front().second, audio::SoundAction::Acknowledge);

	// held by a script it takes no notice, and makes no sound
	auto& registry = test::creature_world::World::Registry();
	registry.Assign<ScriptHeld>(creature);
	registry.Assign<ScriptControlled>(creature);
	_system.ActOnPoint(creature, glm::vec3(140.0f, 0.0f, 100.0f));
	EXPECT_EQ(sounds.played.size(), 1u);
	registry.Remove<ScriptHeld>(creature);
	registry.Remove<ScriptControlled>(creature);

	// nowhere to stand: it says it can't get there, with no sound
	auto& land = Locator::landAvoidSystem::value().GetState();
	land.avoid.assign(land.avoid.size(), land_avoid::k_Unreachable);
	_system.ActOnPoint(creature, glm::vec3(160.0f, 0.0f, 100.0f));
	EXPECT_EQ(sounds.played.size(), 1u);
}

TEST_F(LeashSystemTest, SentOntoAFieldItAcknowledgesTheOrderAndAgainAsItIsToldToActOnTheField)
{
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {4});
	const test::RestoreService<Locator::creatureAudioSystem> restoreCreatureAudio;
	auto& voices = static_cast<HeardSounds&>(Locator::creatureAudioSystem::emplace<HeardSounds>());
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& samples = static_cast<InGameSamples&>(Locator::audio::emplace<InGameSamples>());
	std::vector<std::string> heard;
	voices.heard = &heard;
	samples.heard = &heard;
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	_hand->position = glm::vec3(100.0f, 30.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	// a field of the land, filed in the cell of the point as info.dat's FIELD; it has no mesh, so the map files it in
	// the cell of its position only
	const test::RestoreService<Locator::mapShapeProvider> restoreShapes;
	auto& shapes = static_cast<test::FakeMapShapeProvider&>(Locator::mapShapeProvider::emplace<test::FakeMapShapeProvider>());
	shapes.meshShape = [](entt::entity, ecs::map_collide::Shape&, float&) { return false; };
	_world.Info().fieldType.at(0).type = ObjectType::Field;
	const auto field = registry.Create();
	registry.Assign<Transform>(field, glm::vec3(130.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Field>(field).town = -1;
	ecs::map_cells::InsertMapObject(field);
	ASSERT_EQ(ecs::map_cells::FindType(map_coords::CellOf(glm::vec3(132.0f, 0.0f, 103.0f)), ObjectType::Field), field);
	heard.clear();

	// the local player's creature: the order taken is heard and acknowledged, and then again as it is told to act on
	// the field, its player's sample before its own sound each time
	_system.ActOnPoint(creature, glm::vec3(132.0f, 0.0f, 103.0f));
	EXPECT_EQ(heard, (std::vector<std::string> {"in-game sample 1", "creature's acknowledgement", "in-game sample 1",
	                                            "creature's acknowledgement"}));
	EXPECT_EQ(mind.leash.actOn, (std::vector<uint32_t> {static_cast<uint32_t>(field)}));
	EXPECT_TRUE(_locomotion->moves.empty());

	// a walk is acknowledged once
	heard.clear();
	_system.ActOnPoint(creature, glm::vec3(160.0f, 0.0f, 100.0f));
	EXPECT_EQ(heard, (std::vector<std::string> {"in-game sample 1", "creature's acknowledgement"}));

	// another player's creature makes its own sounds only, twice on the field
	heard.clear();
	registry.Get<Creature>(creature).owner = PlayerNames::PLAYER_TWO;
	_system.ActOnPoint(creature, glm::vec3(132.0f, 0.0f, 103.0f));
	EXPECT_EQ(heard, (std::vector<std::string> {"creature's acknowledgement", "creature's acknowledgement"}));
}

TEST_F(LeashSystemTest, WhereTheLeashSendsItIsMarkedUntilItIsDone)
{
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {4});
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& visuals = static_cast<SpotVisuals&>(Locator::particleSystem::emplace<SpotVisuals>());
	const auto creature = Leadable();
	GiveDesires(creature);
	_hand->position = glm::vec3(100.0f, 30.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	visuals.started.clear();

	// the creature target's mark, on the ground at the point, for 20 turns, owned by the creature
	_system.ActOnPoint(creature, glm::vec3(130.0f, 0.0f, 100.0f));
	ASSERT_EQ(visuals.started.size(), 1u);
	const auto& first = visuals.started.front();
	EXPECT_EQ(first.type, SpotVisualType::CreatureTarget);
	EXPECT_EQ(first.position, glm::vec3(130.0f, test::WaterCellIsland::k_HeightMarker, 100.0f));
	EXPECT_EQ(first.turns, std::optional<int>(20));
	EXPECT_EQ(first.owner, creature);
	EXPECT_EQ(first.magnitude, 1.0f);
	const auto firstMark = LeashOf(creature).targetMarker;
	EXPECT_NE(firstMark, entt::entity {entt::null});
	EXPECT_TRUE(visuals.closed.empty());
	// the point 2 m above the place is its target
	EXPECT_TRUE(LeashOf(creature).hasTarget);
	EXPECT_TRUE(LeashOf(creature).targetThing == entt::entity {entt::null});
	EXPECT_EQ(LeashOf(creature).targetPoint, glm::vec3(130.0f, test::WaterCellIsland::k_HeightMarker + 2.0f, 100.0f));

	// a new order closes the mark before and shows its own
	_system.ActOnPoint(creature, glm::vec3(140.0f, 0.0f, 100.0f));
	ASSERT_EQ(visuals.started.size(), 2u);
	EXPECT_EQ(visuals.closed, (std::vector<entt::entity> {firstMark}));
	const auto secondMark = LeashOf(creature).targetMarker;

	// the walk over, the mark goes with it
	_locomotion->moving = false;
	for (int turn = 0; turn < 100 && LeashOf(creature).control != CreatureLeash::Control::Idle; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(visuals.closed.back(), secondMark);
	EXPECT_EQ(LeashOf(creature).targetMarker, entt::entity {entt::null});
	EXPECT_FALSE(LeashOf(creature).hasTarget);

	// a refusal shows none
	auto& registry = test::creature_world::World::Registry();
	registry.Assign<ScriptHeld>(creature);
	registry.Assign<ScriptControlled>(creature);
	_system.ActOnPoint(creature, glm::vec3(150.0f, 0.0f, 100.0f));
	EXPECT_EQ(visuals.started.size(), 2u);
}

TEST_F(LeashSystemTest, SentToAPointWithNowhereToStandItGoesNowhere)
{
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {4});
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	mind.turnsSinceMadeToAct = 50;
	_hand->position = glm::vec3(100.0f, 30.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	// no cell of the land can be stood on
	auto& land = Locator::landAvoidSystem::value().GetState();
	land.avoid.assign(land.avoid.size(), land_avoid::k_Unreachable);
	_system.ActOnPoint(creature, glm::vec3(160.0f, 0.0f, 100.0f));
	EXPECT_TRUE(_locomotion->moves.empty());
	EXPECT_FALSE(LeashOf(creature).returning);
	EXPECT_EQ(mind.turnsSinceMadeToAct, 50u);
	// it is doing what the leash sent it to do all the same
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Sent);
}

TEST_F(LeashSystemTest, TiedItWalksBackToWhatItIsTiedToBeyondTheFullLength)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	const auto post = registry.Create();
	registry.Assign<Transform>(post, glm::vec3(150.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(_system.TieTo(creature, post));
	// 50 from the post: the shortest tied leash, 180 at full stretch
	EXPECT_EQ(LeashOf(creature).worn->rope.maxLength, 180.0f);
	// past the rest length, within the full one: it stays
	registry.Get<Transform>(creature).position = glm::vec3(300.0f, 0.0f, 100.0f);
	_system.ProcessTurn();
	EXPECT_TRUE(_locomotion->moves.empty());
	// beyond it, it walks back to the post
	registry.Get<Transform>(creature).position = glm::vec3(400.0f, 0.0f, 100.0f);
	_system.ProcessTurn();
	ASSERT_EQ(_locomotion->moves.size(), 1u);
	EXPECT_TRUE(_locomotion->moves.front().walkBack);
	EXPECT_EQ(_locomotion->moves.front().point, glm::vec2(150.0f, 100.0f));
	EXPECT_EQ(_locomotion->moves.front().value, 180.0f);
	// sped up by the game's map distance to the post (250 through its 1/sqrt table) over twice the full length
	const auto distance = gutils::GetDistanceInMetres(glm::vec3(400.0f, 0.0f, 100.0f), glm::vec3(150.0f, 0.0f, 100.0f));
	EXPECT_NEAR(distance, 250.0f, 250.0f / 1024.0f);
	EXPECT_FLOAT_EQ(_locomotion->moves.front().hurry, distance * 0.8f / 360.0f);
}

TEST_F(LeashSystemTest, NoWalkBackOnALeashThatDoesNotWorkToWaterOrHeldByAScript)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	_hand->position = glm::vec3(300.0f, 0.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	_system.SetWorks(creature, false);
	_system.ProcessTurn();
	EXPECT_TRUE(_locomotion->moves.empty());
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Idle);
	// the hand over water
	_system.SetWorks(creature, true);
	Flood(*_hand->position);
	_system.ProcessTurn();
	EXPECT_TRUE(_locomotion->moves.empty());
	// held by a script
	registry.Assign<ScriptHeld>(creature);
	registry.Assign<ScriptControlled>(creature);
	_hand->position = glm::vec3(320.0f, 0.0f, 100.0f);
	_system.ProcessTurn();
	EXPECT_TRUE(_locomotion->moves.empty());
	registry.Remove<ScriptControlled>(creature);
	_system.ProcessTurn();
	EXPECT_EQ(_locomotion->moves.size(), 1u);
}

TEST_F(LeashSystemTest, AWalkBackToWhereItCannotStandStopsItAndIsTriedAgainEachTurn)
{
	using MoveResult = ecs::systems::CreatureLocomotionSystemInterface::MoveResult;
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	_hand->position = glm::vec3(300.0f, 0.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	// the hand over land the creature can't stand on, too near water or too steep: the walk is refused
	_locomotion->walkBackResult = MoveResult::InvalidDestination;
	_system.ProcessTurn();
	ASSERT_EQ(_locomotion->moves.size(), 1u);
	EXPECT_TRUE(_locomotion->moves.front().walkBack);
	// it stops on its way, a walk braking, sent all the same: its mind obeys for the turn
	ASSERT_EQ(_locomotion->stopped.size(), 1u);
	EXPECT_EQ(_locomotion->stopped.front(), creature);
	EXPECT_EQ(_locomotion->braked, (std::vector<entt::entity> {creature}));
	EXPECT_TRUE(LeashOf(creature).returning);
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Led);
	EXPECT_TRUE(mind.leash.obeying);
	// not moving, the walk is over by the next turn, and it is sent again
	_system.ProcessTurn();
	EXPECT_EQ(_locomotion->moves.size(), 2u);
	EXPECT_EQ(_locomotion->stopped.size(), 2u);
	EXPECT_TRUE(mind.leash.obeying);
	// the point within reach again, it sets off and is not stopped
	_locomotion->walkBackResult = MoveResult::Started;
	_system.ProcessTurn();
	EXPECT_EQ(_locomotion->moves.size(), 3u);
	EXPECT_EQ(_locomotion->stopped.size(), 2u);
	EXPECT_TRUE(LeashOf(creature).returning);
}

TEST_F(LeashSystemTest, KeptNearHomeItWalksBack)
{
	const auto creature = Leadable();
	_system.ConfineToHome(creature, 20.0f);
	ASSERT_TRUE(LeashOf(creature).home.has_value());
	EXPECT_EQ(*LeashOf(creature).home, glm::vec3(100.0f, 0.0f, 100.0f));
	_system.ProcessTurn();
	EXPECT_TRUE(_locomotion->moves.empty());
	// strayed off, it walks back
	test::creature_world::World::Registry().Get<Transform>(creature).position = glm::vec3(200.0f, 0.0f, 100.0f);
	_system.ProcessTurn();
	ASSERT_EQ(_locomotion->moves.size(), 1u);
	EXPECT_TRUE(_locomotion->moves.front().walkBack);
	EXPECT_EQ(_locomotion->moves.front().point, glm::vec2(100.0f, 100.0f));
	// until it is within its height or the radius of home, whichever is more
	EXPECT_EQ(_locomotion->moves.front().value,
	          std::max(creature_morph::k_HeightAtSizeOne * test::creature_world::World::Registry().Get<Creature>(creature).size,
	                   20.0f));
	EXPECT_TRUE(LeashOf(creature).returning);
	_system.ClearConfinement(creature);
	EXPECT_FALSE(LeashOf(creature).returning);
}

TEST_F(LeashSystemTest, AYoungCreatureOnTheFirstLandIsKeptAtItsHome)
{
	auto& registry = test::creature_world::World::Registry();
	const LandAt land(1);
	const auto creature = Leadable();
	_system.SetHome(creature, glm::vec3(100.0f, 0.0f, 100.0f));
	registry.Get<CreatureMindState>(creature).developmentPhase = 4;
	const auto keeping = _system.HomeKeepingOf(creature);
	EXPECT_FALSE(keeping.leashed);
	EXPECT_EQ(keeping.developmentPhase, 4u);
	EXPECT_TRUE(keeping.localPlayers);
	EXPECT_EQ(keeping.landNumber, 1);
	EXPECT_TRUE(creature_leash::KeptAtHome(keeping));

	// strayed 30 from home, it is kept within 10 and walks back
	registry.Get<Transform>(creature).position = glm::vec3(130.0f, 0.0f, 100.0f);
	_system.ProcessTurn();
	EXPECT_EQ(LeashOf(creature).confinementRadius, creature_leash::k_YoungHomeRadius);
	EXPECT_EQ(LeashOf(creature).confinementCentre, glm::vec3(100.0f, 0.0f, 100.0f));
	ASSERT_EQ(_locomotion->moves.size(), 1u);
	EXPECT_TRUE(_locomotion->moves.front().walkBack);
	EXPECT_EQ(_locomotion->moves.front().point, glm::vec2(100.0f, 100.0f));
	EXPECT_EQ(_locomotion->moves.front().value,
	          creature_leash::WalkBackArrival(creature_morph::k_HeightAtSizeOne * registry.Get<Creature>(creature).size,
	                                          creature_leash::k_YoungHomeRadius));
	EXPECT_TRUE(LeashOf(creature).returning);
	EXPECT_TRUE(registry.Get<CreatureMindState>(creature).leash.obeying);
}

TEST_F(LeashSystemTest, GrownUpOnAnotherLandOrSomeoneElsesItIsNotKeptAtHome)
{
	auto& registry = test::creature_world::World::Registry();
	const LandAt land(1);
	const auto creature = Leadable();
	_system.SetHome(creature, glm::vec3(100.0f, 0.0f, 100.0f));
	// fully grown up, as every creature starts
	_system.ProcessTurn();
	EXPECT_EQ(LeashOf(creature).confinementRadius, 0.0f);
	// young, on the second land
	registry.Get<CreatureMindState>(creature).developmentPhase = 4;
	SetLand(2);
	_system.ProcessTurn();
	EXPECT_EQ(LeashOf(creature).confinementRadius, 0.0f);
	// young on the first land, but leashed
	SetLand(1);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	EXPECT_FALSE(creature_leash::KeptAtHome(_system.HomeKeepingOf(creature)));
	_system.TakeOff(creature);
	// another player's young creature
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(300.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	registry.Get<CreatureMindState>(other).developmentPhase = 4;
	EXPECT_FALSE(_system.HomeKeepingOf(other).localPlayers);
	EXPECT_FALSE(creature_leash::KeptAtHome(_system.HomeKeepingOf(other)));
}

TEST_F(LeashSystemTest, TakingOffTheHeldLeashLeavesATiedOne)
{
	const auto creature = Leadable();
	auto& registry = test::creature_world::World::Registry();
	const auto post = registry.Create();
	registry.Assign<Transform>(post, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	ASSERT_TRUE(_system.TieTo(creature, post));
	EXPECT_EQ(_system.TiedTo(creature), std::optional(post));
	EXPECT_FALSE(_system.Shake(PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(_system.IsLeashed(creature));
	_system.UntieToHand(creature);
	EXPECT_TRUE(_system.Shake(PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(_system.IsLeashed(creature));
	// another player has no leash to take off
	EXPECT_FALSE(_system.Shake(PlayerNames::PLAYER_TWO));
}

// Tying the leash plays the two tying sounds of the in-game bank in turn, flat; giving a tied leash back to the hand
// plays them in a turn of its own
TEST_F(LeashSystemTest, TyingTheLeashPlaysTheTwoTyingSoundsInTurn)
{
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& samples = static_cast<InGameSamples&>(Locator::audio::emplace<InGameSamples>());
	std::vector<std::string> heard;
	samples.heard = &heard;
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	const auto post = registry.Create();
	registry.Assign<Transform>(post, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	heard.clear();
	ASSERT_TRUE(_system.TieTo(creature, post));
	_system.UntieToHand(creature);
	ASSERT_TRUE(_system.TieTo(creature, post));
	_system.UntieToHand(creature);
	ASSERT_TRUE(_system.TieTo(creature, post));
	EXPECT_EQ(heard, (std::vector<std::string> {"in-game sample 148", "in-game sample 149", "in-game sample 148"}));
	// untying by the hand alone is silent
	heard.clear();
	_system.UntieToHand(creature);
	EXPECT_TRUE(heard.empty());
	// given back to the hand: the first of the two again, in its own turn
	ASSERT_TRUE(_system.TieTo(creature, post));
	heard.clear();
	_system.ReturnToHand(creature);
	EXPECT_EQ(heard, (std::vector<std::string> {"in-game sample 148"}));
}

// The scripts' leash to the hand plays the untying sounds in the same turn as the tie packet's return to the hand
TEST_F(LeashSystemTest, TheScriptsLeashToTheHandPlaysTheUntyingSoundsInTheTiePacketsTurn)
{
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& samples = static_cast<InGameSamples&>(Locator::audio::emplace<InGameSamples>());
	std::vector<std::string> heard;
	samples.heard = &heard;
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	const auto post = registry.Create();
	registry.Assign<Transform>(post, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	heard.clear();
	_system.PutInHand(creature);
	ASSERT_TRUE(_system.TieTo(creature, post));
	_system.ReturnToHand(creature);
	_system.PutInHand(creature);
	// the tie's own turn starts at the first sound; the hand's goes on from the scripts' to the packet's and back
	EXPECT_EQ(heard, (std::vector<std::string> {"in-game sample 148", "in-game sample 148", "in-game sample 149",
	                                            "in-game sample 148"}));
}

TEST_F(LeashSystemTest, TheLeashGoingOnOrComingOffWipesTheLocalPlayersGesturePath)
{
	const test::RestoreService<Locator::gestureSystem> restoreGestures;
	auto& gestures = static_cast<PathGestures&>(Locator::gestureSystem::emplace<PathGestures>());
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	ASSERT_EQ(gestures.forgotten, 0);
	// going on, for the player at this machine
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	EXPECT_EQ(gestures.forgotten, 1);
	// tied while on, it is put on again
	const auto post = registry.Create();
	registry.Assign<Transform>(post, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(_system.TieTo(creature, post));
	EXPECT_EQ(gestures.forgotten, 2);
	// untied by the leash key, and its type changed, it is not
	_system.UntieToHand(creature);
	_system.SetKnown(creature, LeashType::Evil, true);
	ASSERT_TRUE(_system.ChangeType(creature, LeashType::Evil));
	EXPECT_EQ(gestures.forgotten, 2);
	// coming off; with no leash on, nothing comes off
	_system.TakeOff(creature);
	EXPECT_EQ(gestures.forgotten, 3);
	_system.TakeOff(creature);
	EXPECT_EQ(gestures.forgotten, 3);
	// another player's leash leaves the local hand's path as it is
	const auto theirs = test::creature_world::World::MakeCreature(glm::vec3(200.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	ASSERT_TRUE(_system.SetLeashable(theirs, true));
	_system.SetKnown(theirs, LeashType::Rope, true);
	ASSERT_TRUE(_system.PutOn(theirs, LeashType::Rope));
	_system.TakeOff(theirs);
	EXPECT_FALSE(_system.IsLeashed(theirs));
	EXPECT_EQ(gestures.forgotten, 3);
}

TEST_F(LeashSystemTest, TiedToATreeByItsHeightToAnythingElseByTheDistance)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	const auto height = creature_morph::k_HeightAtSizeOne * registry.Get<Creature>(creature).size;
	const auto tree = registry.Create();
	registry.Assign<Transform>(tree, glm::vec3(300.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Tree>(tree);
	ASSERT_TRUE(_system.TieTo(creature, tree));
	EXPECT_EQ(LeashOf(creature).worn->rope.maxLength, creature_leash::TiedToTree(height).max);
	EXPECT_EQ(LeashOf(creature).worn->rope.slackLength, creature_leash::TiedToTree(height).slack);

	// another creature 200 away goes by the distance, as a post does, not by the height
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(300.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	ASSERT_TRUE(_system.TieTo(creature, other));
	const auto distance =
	    gutils::GetDistanceInMetres(registry.Get<Transform>(creature).position, registry.Get<Transform>(other).position);
	EXPECT_EQ(LeashOf(creature).worn->rope.maxLength, creature_leash::TiedToObject(distance).max);
	EXPECT_EQ(LeashOf(creature).worn->rope.slackLength, creature_leash::TiedToObject(distance).slack);
	EXPECT_GT(LeashOf(creature).worn->rope.maxLength, 290.0f);
}

TEST_F(LeashSystemTest, LeashedToAnotherCreatureTheyWarmAtOnceThenByTheOthersLastStep)
{
	auto& registry = test::creature_world::World::Registry();
	const auto& lookup = std::as_const(registry);
	const TurnAt start(1000);
	const auto creature = Leadable();
	_system.SetKnown(creature, LeashType::Good, true);
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(130.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Good));
	ASSERT_TRUE(_system.TieTo(creature, other));
	const auto steps = [&lookup](entt::entity of) { return lookup.Get<const CreatureMindState>(of).leash.attitudes.size(); };

	// the first step at once, both ways, kept on the other creature
	_system.ProcessTurn();
	ASSERT_EQ(steps(creature), 1u);
	ASSERT_EQ(steps(other), 1u);
	const auto& mine = lookup.Get<const CreatureMindState>(creature).leash.attitudes.front();
	EXPECT_EQ(mine.creature, static_cast<uint32_t>(other));
	EXPECT_EQ(mine.change, creature_leash::k_AttitudeStep);
	EXPECT_EQ(lookup.Get<const CreatureMindState>(other).leash.attitudes.front().creature, static_cast<uint32_t>(creature));
	EXPECT_EQ(lookup.Get<const CreatureLeashAttitude>(other).lastStep, 1000u);
	EXPECT_FALSE(lookup.AllOf<CreatureLeashAttitude>(creature));

	// the next after more than 600 turns
	game_clock::SetTurn(1600);
	_system.ProcessTurn();
	EXPECT_EQ(steps(creature), 1u);
	game_clock::SetTurn(1601);
	_system.ProcessTurn();
	EXPECT_EQ(steps(creature), 2u);
	EXPECT_EQ(steps(other), 2u);

	// untied and tied again, the other's last step still holds
	_system.UntieToHand(creature);
	ASSERT_TRUE(_system.TieTo(creature, other));
	game_clock::SetTurn(1700);
	_system.ProcessTurn();
	EXPECT_EQ(steps(creature), 2u);
	EXPECT_EQ(lookup.Get<const CreatureLeashAttitude>(other).lastStep, 1601u);
}

TEST_F(LeashSystemTest, ThePensSmallBodyHoldsTheRopesEnd)
{
	const auto creature = Leadable();
	PutInPen(creature);
	// on the ground, about 68 away, within the leash's length: the rope from the full-size collar is pulled harder than
	// the one from the small body's collar, and neither moves the creature
	_hand->position = glm::vec3(167.7f, 0.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	_system.Update(k_Frame); // laid straight from the hand to the collar
	ExpectPoint(leash_rope::Point(LeashOf(creature).worn->rope, leash_rope::k_PointCount - 1),
	            HighCollar(creature, PenShare(creature)));
	_system.ProcessTurn();
	EXPECT_TRUE(_locomotion->moves.empty());
	_system.Update(k_Frame); // a frame of the rope moving
	ExpectPoint(leash_rope::Point(LeashOf(creature).worn->rope, leash_rope::k_PointCount - 1),
	            HighCollar(creature, PenShare(creature)));
	const auto penTension = LeashOf(creature).worn->rope.tension;

	// the same creature drawn at its own size, the rope laid afresh
	test::creature_world::World::Registry().Get<CreatureDrawPose>(creature).scale.reset();
	LeashOf(creature).worn->ropeStarted = false;
	_system.Update(k_Frame);
	ExpectPoint(leash_rope::Point(LeashOf(creature).worn->rope, leash_rope::k_PointCount - 1), HighCollar(creature, 1.0f));
	_system.ProcessTurn();
	EXPECT_TRUE(_locomotion->moves.empty());
	_system.Update(k_Frame);
	EXPECT_LT(penTension, LeashOf(creature).worn->rope.tension);
}

TEST_F(LeashSystemTest, AStandingCreatureOutOfThePenKeepsItsCollar)
{
	const auto creature = Leadable();
	_hand->position = glm::vec3(130.0f, 0.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	_system.Update(k_Frame);
	_system.Update(k_Frame);
	// where it sat on the Transform: the drawn body is the Transform's
	const auto end = leash_rope::Point(LeashOf(creature).worn->rope, leash_rope::k_PointCount - 1);
	const auto expected = HighCollar(creature, 1.0f);
	EXPECT_NEAR(end.x, expected.x, 1e-4f);
	EXPECT_NEAR(end.y, expected.y, 1e-4f);
	EXPECT_NEAR(end.z, expected.z, 1e-4f);
}

TEST_F(LeashSystemTest, TheRopesEndIsTheCollarBoneAsTheBodyIsDrawnBetweenItsTurns)
{
	// a Giant Ape's rig, which has a collar bone
	const test::ScopedDefaultFileSystem fileSystem;
	test::creature_block::Block block;
	block.clips = {test::creature_block::Clip {}};
	_world.LoadApeRig(block, {{"move", {"Cstand"}}});
	const auto creature = Leadable();
	auto& registry = test::creature_world::World::Registry();
	auto& animation = registry.Get<CreatureAnimation>(creature);
	constexpr uint32_t k_Collar = 11;
	animation.boneMatrices.assign(k_Collar + 1, glm::mat4(1.0f));
	// drawn between its last two turn poses with the collar bone up and to the side of the frame's pose
	animation.drawnBones = animation.boneMatrices;
	const glm::vec3 collar {3.0f, 20.0f, -2.0f};
	animation.drawnBones.at(k_Collar)[3] = glm::vec4(collar, 1.0f);
	_hand->position = glm::vec3(130.0f, 0.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	_system.Update(k_Frame);
	const auto drawn = ecs::DrawnBodyModel(std::as_const(registry), creature);
	ExpectPoint(leash_rope::Point(LeashOf(creature).worn->rope, leash_rope::k_PointCount - 1),
	            glm::vec3((drawn * animation.drawnBones.at(k_Collar))[3]));

	// without drawn bones (in the hand, in flight, before its first turn), the frame's pose
	animation.drawnBones.clear();
	LeashOf(creature).worn->ropeStarted = false;
	_system.Update(k_Frame);
	ExpectPoint(leash_rope::Point(LeashOf(creature).worn->rope, leash_rope::k_PointCount - 1), glm::vec3(drawn[3]));
}

TEST_F(LeashSystemTest, TiedToACreatureInItsPenTheRopeStartsAtItsSmallCollar)
{
	const auto creature = Leadable();
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(130.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	PutInPen(other);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	ASSERT_TRUE(_system.TieTo(creature, other));
	_system.Update(k_Frame);
	const auto& rope = LeashOf(creature).worn->rope;
	ExpectPoint(leash_rope::Point(rope, 0), HighCollar(other, PenShare(other)));
	ExpectPoint(leash_rope::Point(rope, leash_rope::k_PointCount - 1), HighCollar(creature, 1.0f));
}

TEST_F(LeashSystemTest, InItsPenTheLeashsLengthsFollowThePenSize)
{
	const auto creature = Leadable();
	auto& registry = test::creature_world::World::Registry();
	const auto own = registry.Get<Creature>(creature).size;
	_hand->position = glm::vec3(130.0f, 0.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	_system.Update(k_Frame);
	EXPECT_EQ(LeashOf(creature).worn->rope.slackLength, creature_leash::InHand(own).slack);
	EXPECT_EQ(LeashOf(creature).worn->rope.maxLength, creature_leash::InHand(own).max);

	// held in the hand, the lengths are refreshed by the size it is drawn at: 0.7 x 15 x 0.22 + 22 and 3 x 15 x 0.22 + 32
	registry.Get<CreatureDrawPose>(creature).size = k_PenShare;
	_system.Update(k_Frame);
	EXPECT_EQ(LeashOf(creature).worn->rope.slackLength, creature_leash::InHand(k_PenShare).slack);
	EXPECT_EQ(LeashOf(creature).worn->rope.maxLength, creature_leash::InHand(k_PenShare).max);
	EXPECT_NEAR(LeashOf(creature).worn->rope.slackLength, (0.7f * 15.0f * 0.22f) + 22.0f, 1e-4f);
	EXPECT_NEAR(LeashOf(creature).worn->rope.maxLength, (3.0f * 15.0f * 0.22f) + 32.0f, 1e-4f);
	// its own size is kept
	EXPECT_EQ(registry.Get<Creature>(creature).size, own);

	// tied to a tree, by the height it is drawn at
	const auto tree = registry.Create();
	registry.Assign<Transform>(tree, glm::vec3(130.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Tree>(tree);
	ASSERT_TRUE(_system.TieTo(creature, tree));
	const auto tied = creature_leash::TiedToTree(creature_morph::k_HeightAtSizeOne * k_PenShare);
	EXPECT_EQ(LeashOf(creature).worn->rope.slackLength, tied.slack);
	EXPECT_EQ(LeashOf(creature).worn->rope.maxLength, tied.max);
}

TEST_F(LeashSystemTest, TheCompassionAndAggressionLeashesHoldTheirDesireDominantWhileOn)
{
	const auto creature = Leadable();
	_system.SetKnown(creature, LeashType::Good, true);
	_system.SetKnown(creature, LeashType::Evil, true);
	auto& mind = GiveDesires(creature);

	// put on, compassion is dominant for 36000 s, the others held down as long and wanted as little as can be
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Good));
	ASSERT_TRUE(mind.dominantDesire.desire.has_value());
	EXPECT_EQ(*mind.dominantDesire.desire, Desire::Compassion);
	EXPECT_EQ(mind.dominantDesire.seconds, 36000u);
	EXPECT_EQ((*mind.desires)[Desire::Compassion].value, 1.0f);
	EXPECT_EQ((*mind.desires)[Desire::Play].value, 0.0f);
	EXPECT_EQ((*mind.desires)[Desire::Play].suppressedTurns, 360000u);
	// the body's needs stay free
	EXPECT_EQ((*mind.desires)[Desire::Hunger].suppressedTurns, 0u);

	// every turn it is made dominant again, so its count starts again
	CountTurns(mind, 5);
	EXPECT_EQ(mind.dominantDesire.turns, 5u);
	_system.ProcessTurn();
	EXPECT_EQ(mind.dominantDesire.turns, 0u);

	// changed, the new leash's desire
	ASSERT_TRUE(_system.ChangeType(creature, LeashType::Evil));
	EXPECT_EQ(mind.dominantDesire.desire, std::optional(Desire::Anger));
	_system.ProcessTurn();
	EXPECT_EQ(mind.dominantDesire.desire, std::optional(Desire::Anger));

	// taken off, nothing is dominant and nothing held down
	_system.TakeOff(creature);
	EXPECT_FALSE(mind.dominantDesire.desire.has_value());
	EXPECT_EQ((*mind.desires)[Desire::Play].suppressedTurns, 0u);
}

TEST_F(LeashSystemTest, TheCompassionAndAggressionLeashesLeaveHowItLooksAlone)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	_system.SetKnown(creature, LeashType::Good, true);
	_system.SetKnown(creature, LeashType::Evil, true);
	GiveDesires(creature);
	auto& body = registry.Get<Creature>(creature);
	body.alignment = 0.3f;
	body.pendingAlignment = 0.0f;
	body.fatness = 0.7f;
	body.strength = 0.2f;
	const auto unchanged = [&body]() {
		EXPECT_EQ(body.alignment, 0.3f);
		EXPECT_EQ(body.pendingAlignment, 0.0f);
		EXPECT_EQ(body.fatness, 0.7f);
		EXPECT_EQ(body.strength, 0.2f);
	};
	const auto post = registry.Create();
	registry.Assign<Transform>(post, glm::vec3(150.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));

	// they work on what it feels, not on what it has become: put on, worn, changed, tied, taken off
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Good));
	unchanged();
	_system.ProcessTurn();
	unchanged();
	ASSERT_TRUE(_system.ChangeType(creature, LeashType::Evil));
	unchanged();
	ASSERT_TRUE(_system.TieTo(creature, post));
	_system.ProcessTurn();
	unchanged();
	_system.TakeOff(creature);
	unchanged();
}

TEST_F(LeashSystemTest, TheLearningLeashLetsGoOfADominantDesireAsItGoesOnButNotAsItComesOff)
{
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	mind.dominantDesire = creature_spell_mind::SetCheatDominant(*mind.desires, Desire::Impress, false, 10.0f, 0.0f, 120.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	EXPECT_FALSE(mind.dominantDesire.desire.has_value());
	EXPECT_EQ((*mind.desires)[Desire::Play].suppressedTurns, 0u);

	// worn, it makes nothing dominant; taken off, it leaves what is
	mind.dominantDesire = creature_spell_mind::SetCheatDominant(*mind.desires, Desire::Impress, false, 10.0f, 0.0f, 120.0f);
	CountTurns(mind, 3);
	_system.ProcessTurn();
	EXPECT_EQ(mind.dominantDesire.desire, std::optional(Desire::Impress));
	EXPECT_EQ(mind.dominantDesire.turns, 3u);
	_system.TakeOff(creature);
	EXPECT_EQ(mind.dominantDesire.desire, std::optional(Desire::Impress));
	EXPECT_GT((*mind.desires)[Desire::Play].suppressedTurns, 0u);
}

TEST_F(LeashSystemTest, OnTheAggressionLeashACreatureTiedNearEnoughGetsAngryUnlessItIsAlready)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	_system.SetKnown(creature, LeashType::Evil, true);
	GiveDesires(creature);
	const auto reach =
	    creature_leash::k_AngerOtherReach * creature_morph::k_HeightAtSizeOne * registry.Get<Creature>(creature).size;
	const auto nearby =
	    test::creature_world::World::MakeCreature(glm::vec3(100.0f + reach - 1.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto beyond =
	    test::creature_world::World::MakeCreature(glm::vec3(100.0f + reach + 1.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	auto& nearbyMind = GiveDesires(nearby);
	auto& beyondMind = GiveDesires(beyond);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Evil));

	ASSERT_TRUE(_system.TieTo(creature, nearby));
	_system.ProcessTurn();
	EXPECT_EQ(nearbyMind.dominantDesire.desire, std::optional(Desire::Anger));
	// angry above all already, it is not made so again
	CountTurns(nearbyMind, 4);
	_system.ProcessTurn();
	EXPECT_EQ(nearbyMind.dominantDesire.turns, 4u);

	ASSERT_TRUE(_system.TieTo(creature, beyond));
	_system.ProcessTurn();
	EXPECT_FALSE(beyondMind.dominantDesire.desire.has_value());
}

TEST_F(LeashSystemTest, TiedToAVillageItWantsToImpressItWhileTheVillageBelievesTooLittle)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	// its own village, believing in its player half as much as in the second player
	const auto centre = MakeTownCentre(glm::vec3(150.0f, 0.0f, 100.0f), 2.0f, 4.0f);

	// tied, at once for 120 s
	ASSERT_TRUE(_system.TieTo(creature, centre));
	EXPECT_EQ(mind.dominantDesire.desire, std::optional(Desire::Impress));
	EXPECT_EQ(mind.dominantDesire.seconds, 120u);
	// already what it wants most: left as it is
	CountTurns(mind, 3);
	_system.ProcessTurn();
	EXPECT_EQ(mind.dominantDesire.turns, 3u);
	// wanting something else more, made dominant again
	(*mind.desires)[Desire::Impress].value = 0.0f;
	(*mind.desires)[Desire::Play].value = 0.5f;
	_system.ProcessTurn();
	EXPECT_EQ(mind.dominantDesire.turns, 0u);
	EXPECT_EQ((*mind.desires)[Desire::Impress].value, 1.0f);

	// believing in its player enough, nothing more
	auto& village = registry.Get<Town>(_town);
	ecs::town_belief::SetBelief(village.belief, PlayerNames::PLAYER_ONE, 2.5f);
	(*mind.desires)[Desire::Impress].value = 0.0f;
	(*mind.desires)[Desire::Play].value = 0.5f;
	CountTurns(mind, 2);
	_system.ProcessTurn();
	EXPECT_EQ(mind.dominantDesire.turns, 2u);
	// someone else's village it impresses all the same
	village.owner = PlayerNames::PLAYER_TWO;
	_system.ProcessTurn();
	EXPECT_EQ(mind.dominantDesire.turns, 0u);
}

TEST_F(LeashSystemTest, TiedToAVillagesTotemTheVillageIsImpressedEachTurnButNotAtTheTie)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	const auto centre = MakeTownCentre(glm::vec3(150.0f, 0.0f, 100.0f), 1.0f, 4.0f);
	const auto plinth = registry.Create();
	registry.Assign<VillageTotem>(plinth).townCentre = centre;
	registry.Assign<Transform>(plinth, glm::vec3(160.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));

	ASSERT_TRUE(_system.TieTo(creature, plinth));
	EXPECT_FALSE(mind.dominantDesire.desire.has_value());
	_system.ProcessTurn();
	EXPECT_EQ(mind.dominantDesire.desire, std::optional(Desire::Impress));
}

TEST_F(LeashSystemTest, OnTheAggressionLeashAVillageIsNotImpressed)
{
	const auto creature = Leadable();
	_system.SetKnown(creature, LeashType::Evil, true);
	auto& mind = GiveDesires(creature);
	const auto centre = MakeTownCentre(glm::vec3(150.0f, 0.0f, 100.0f), 0.0f, 4.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Evil));
	ASSERT_TRUE(_system.TieTo(creature, centre));
	EXPECT_EQ(mind.dominantDesire.desire, std::optional(Desire::Anger));
	_system.ProcessTurn();
	EXPECT_EQ(mind.dominantDesire.desire, std::optional(Desire::Anger));
}

TEST_F(LeashSystemTest, TiedTheLengthsAreTakenAtTheTieAndKept)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	const auto post = registry.Create();
	registry.Assign<Transform>(post, glm::vec3(300.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(_system.TieTo(creature, post));
	const auto tied = LeashOf(creature).worn->rope;
	// the post moves before the rope is laid, and the creature grows: the lengths stay those of the tie
	registry.Get<Transform>(post).position.x = 340.0f;
	registry.Get<Creature>(creature).size *= 2.0f;
	_system.Update(k_Frame);
	EXPECT_EQ(LeashOf(creature).worn->rope.maxLength, tied.maxLength);
	EXPECT_EQ(LeashOf(creature).worn->rope.slackLength, tied.slackLength);
	_system.Update(k_Frame);
	EXPECT_EQ(LeashOf(creature).worn->rope.maxLength, tied.maxLength);

	// back in the hand, they follow the creature's size again, once its body is given it (each turn)
	ecs::player_creature::GiveBodySize(registry, creature, registry.Get<Creature>(creature).size, false);
	_hand->position = glm::vec3(130.0f, 0.0f, 100.0f);
	_system.UntieToHand(creature);
	_system.Update(k_Frame);
	EXPECT_EQ(LeashOf(creature).worn->rope.maxLength, creature_leash::InHand(registry.Get<Creature>(creature).size).max);
}

TEST_F(LeashSystemTest, FightingTheLeashDoesNotPullItButItsMoodAndAreaGoOn)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	_system.SetKnown(creature, LeashType::Good, true);
	auto& mind = GiveDesires(creature);
	_hand->position = glm::vec3(300.0f, 0.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Good));
	registry.Assign<CreatureFighting>(creature);
	auto& worn = *LeashOf(creature).worn;
	LeashOf(creature).confinementRadius = 0.0f;
	CountTurns(mind, 3);

	_system.ProcessTurn();
	EXPECT_TRUE(_locomotion->moves.empty());
	EXPECT_EQ(mind.dominantDesire.desire, std::optional(Desire::Compassion));
	EXPECT_EQ(mind.dominantDesire.turns, 0u);
	EXPECT_EQ(LeashOf(creature).confinementRadius, worn.rope.maxLength);

	// the fight over, 200 from the hand, beyond the leash's length, it walks back
	registry.Remove<CreatureFighting>(creature);
	_system.ProcessTurn();
	EXPECT_EQ(_locomotion->moves.size(), 1u);
}

TEST_F(LeashSystemTest, TheHandsUntieTakesTheLeashOffAndPutsItBackInTheHandKeepingWhetherItWorks)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	const auto post = registry.Create();
	registry.Assign<Transform>(post, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	ASSERT_TRUE(_system.TieTo(creature, post));
	_system.SetWorks(creature, false);
	EXPECT_FALSE(_system.Works(creature));
	_system.ReturnToHand(creature);
	// on again, untied, in the first player's hand, the learning leash still, and still not working
	EXPECT_TRUE(_system.IsLeashed(creature));
	EXPECT_FALSE(_system.TiedTo(creature).has_value());
	EXPECT_EQ(LeashOf(creature).worn->holder, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(_system.TypeOf(creature), LeashType::Rope);
	EXPECT_FALSE(_system.Works(creature));
	// with no leash on, there is nothing to put back, and a leash that isn't worn doesn't work
	_system.TakeOff(creature);
	_system.ReturnToHand(creature);
	EXPECT_FALSE(_system.IsLeashed(creature));
	EXPECT_FALSE(_system.Works(creature));
}

TEST_F(LeashSystemTest, TheHandsUntiePutsTheLeashBackWithNoRuleAskedButTheLearningLeash)
{
	auto& registry = test::creature_world::World::Registry();
	MakePlayer(PlayerNames::PLAYER_ONE);
	const auto creature = Leadable();
	const auto post = registry.Create();
	registry.Assign<Transform>(post, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	ASSERT_TRUE(_system.TieTo(creature, post));
	// no longer the one its player leads: a leash put on would be refused, the untie's is not
	registry.Get<Creature>(creature).leashable = false;
	_system.ReturnToHand(creature);
	EXPECT_TRUE(_system.IsLeashed(creature));
	EXPECT_FALSE(_system.TiedTo(creature).has_value());
	EXPECT_FALSE(_system.LastRefusal(PlayerNames::PLAYER_ONE).has_value());
	// without the learning leash, it stays off
	LeashOf(creature).known.reset();
	_system.ReturnToHand(creature);
	EXPECT_FALSE(_system.IsLeashed(creature));
	EXPECT_FALSE(_system.LastRefusal(PlayerNames::PLAYER_ONE).has_value());
}

TEST_F(LeashSystemTest, TheScriptsLeashToTheHandPutsTheLeashOnAgainWhateverItWears)
{
	const test::RestoreService<Locator::gestureSystem> restoreGestures;
	auto& gestures = static_cast<PathGestures&>(Locator::gestureSystem::emplace<PathGestures>());
	auto& registry = test::creature_world::World::Registry();
	MakePlayer(PlayerNames::PLAYER_ONE);
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	_system.SetKnown(creature, LeashType::Evil, true);

	// none worn: the leash its owner picked goes on in their hand, with its mood, and the path is wiped
	LeashOf(creature).selected = LeashType::Evil;
	_system.PutInHand(creature);
	ASSERT_TRUE(_system.IsLeashed(creature));
	EXPECT_EQ(_system.TypeOf(creature), LeashType::Evil);
	EXPECT_EQ(LeashOf(creature).worn->holder, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(mind.dominantDesire.desire, std::optional(Desire::Anger));
	EXPECT_EQ(gestures.forgotten, 1);
	const auto lengths = LeashOf(creature).worn->rope;

	// held already: on again, the mood and the lengths taken afresh and the path wiped; how it leads the creature stays
	mind.dominantDesire.desire.reset();
	LeashOf(creature).worn->rope.maxLength = 1.0f;
	LeashOf(creature).control = CreatureLeash::Control::Led;
	_system.PutInHand(creature);
	ASSERT_TRUE(_system.IsLeashed(creature));
	EXPECT_EQ(mind.dominantDesire.desire, std::optional(Desire::Anger));
	EXPECT_EQ(LeashOf(creature).worn->rope.maxLength, lengths.maxLength);
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Led);
	EXPECT_EQ(gestures.forgotten, 2);

	// tied: untied back to the hand, with the hand's lengths
	const auto post = registry.Create();
	registry.Assign<Transform>(post, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(_system.TieTo(creature, post));
	_system.PutInHand(creature);
	EXPECT_FALSE(_system.TiedTo(creature).has_value());
	EXPECT_EQ(LeashOf(creature).worn->rope.maxLength, lengths.maxLength);
	EXPECT_EQ(LeashOf(creature).worn->rope.slackLength, lengths.slackLength);

	// without the learning leash it comes off, and with none on it is let go of; nothing is refused
	const auto rope = *creature_leash::IndexOf(LeashType::Rope);
	LeashOf(creature).known.reset(rope);
	_system.PutInHand(creature);
	EXPECT_FALSE(_system.IsLeashed(creature));
	LeashOf(creature).confinementRadius = 20.0f;
	_system.PutInHand(creature);
	EXPECT_FALSE(_system.IsLeashed(creature));
	EXPECT_EQ(LeashOf(creature).confinementRadius, 0.0f);
	EXPECT_FALSE(_system.LastRefusal(PlayerNames::PLAYER_ONE).has_value());

	// a creature its player doesn't lead is refused: the player's one leash is the led creature's
	LeashOf(creature).known.set(rope);
	registry.Get<Creature>(creature).leashable = false;
	_system.PutInHand(creature);
	EXPECT_FALSE(_system.IsLeashed(creature));
	ASSERT_TRUE(_system.LastRefusal(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_EQ(_system.LastRefusal(PlayerNames::PLAYER_ONE)->why, Refusal::NotLeashable);
}

TEST_F(LeashSystemTest, TheLeashKeyIsRefusedUnlessTheCreatureKnowsTheRopeLeash)
{
	auto& registry = test::creature_world::World::Registry();
	MakePlayer(PlayerNames::PLAYER_ONE);
	const auto creature = Leadable();
	const auto rope = *creature_leash::IndexOf(LeashType::Rope);
	const auto expectRefused = [this]() {
		const auto refused = _system.LastRefusal(PlayerNames::PLAYER_ONE);
		ASSERT_TRUE(refused.has_value());
		EXPECT_EQ(refused->why, Refusal::DoesNotKnowLearningLeash);
	};

	// knowing only the compassion leash, the key puts nothing on
	LeashOf(creature).known.reset();
	_system.SetKnown(creature, LeashType::Good, true);
	EXPECT_FALSE(_system.PressKey(PlayerNames::PLAYER_ONE, creature_leash::LeashKey::Leash));
	EXPECT_FALSE(_system.IsLeashed(creature));
	expectRefused();

	// with the rope leash known it puts the leash on
	_system.SetKnown(creature, LeashType::Rope, true);
	ASSERT_TRUE(_system.PressKey(PlayerNames::PLAYER_ONE, creature_leash::LeashKey::Leash));
	ASSERT_TRUE(_system.IsLeashed(creature));

	// worn, without the rope leash known, it does not take it off
	LeashOf(creature).known.reset(rope);
	EXPECT_FALSE(_system.PressKey(PlayerNames::PLAYER_ONE, creature_leash::LeashKey::Leash));
	EXPECT_TRUE(_system.IsLeashed(creature));
	expectRefused();

	// tied, without it, it does not untie it either
	const auto post = registry.Create();
	registry.Assign<Transform>(post, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	LeashOf(creature).known.set(rope);
	ASSERT_TRUE(_system.TieTo(creature, post));
	LeashOf(creature).known.reset(rope);
	EXPECT_FALSE(_system.PressKey(PlayerNames::PLAYER_ONE, creature_leash::LeashKey::Leash));
	EXPECT_EQ(_system.TiedTo(creature), std::optional(post));
	expectRefused();

	// known again: the key unties it back to the hand, then takes it off
	LeashOf(creature).known.set(rope);
	EXPECT_TRUE(_system.PressKey(PlayerNames::PLAYER_ONE, creature_leash::LeashKey::Leash));
	EXPECT_TRUE(_system.IsLeashed(creature));
	EXPECT_FALSE(_system.TiedTo(creature).has_value());
	EXPECT_TRUE(_system.PressKey(PlayerNames::PLAYER_ONE, creature_leash::LeashKey::Leash));
	EXPECT_FALSE(_system.IsLeashed(creature));
}

TEST_F(LeashSystemTest, TiedToATownsDesireFlagItHelpsWithThatDesireUntilTheLeashKeyUntiesIt)
{
	// the game's tables, through a fake mind service: wood has an action, food none
	const test::RestoreService<Locator::creatureMindSystem> restoreMind;
	auto& minds =
	    static_cast<test::creature_fakes::FakeMind&>(Locator::creatureMindSystem::emplace<test::creature_fakes::FakeMind>());
	creature_mind_tables::Tables tables;
	tables.townActions.at(static_cast<size_t>(TownDesireInfo::ForWood)) = {32};
	minds.tables = &tables;
	auto& registry = test::creature_world::World::Registry();
	MakePlayer(PlayerNames::PLAYER_ONE);
	const auto creature = Leadable();
	auto& helping = registry.Get<CreatureMindState>(creature).townCompassion;
	const auto flagFor = [&registry](TownDesireInfo desire, float x) {
		const auto flag = registry.Create();
		registry.Assign<TownDesireFlag>(flag).desire = desire;
		registry.Assign<Transform>(flag, glm::vec3(x, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return flag;
	};
	const auto wood = flagFor(TownDesireInfo::ForWood, 120.0f);
	const auto food = flagFor(TownDesireInfo::ForFood, 130.0f);

	// tied to the wood flag, it helps with wood and no longer chooses
	ASSERT_TRUE(_system.TieTo(creature, wood));
	EXPECT_EQ(helping.desire, std::optional<uint32_t>(1));
	EXPECT_FALSE(helping.choosesFreely);
	// the tie's packet bringing the leash back to the hand leaves it held to wood, as taking the leash off does
	_system.ReturnToHand(creature);
	EXPECT_FALSE(_system.TiedTo(creature).has_value());
	EXPECT_EQ(helping.desire, std::optional<uint32_t>(1));
	EXPECT_FALSE(helping.choosesFreely);
	// the leash key's untie from the flag frees it
	ASSERT_TRUE(_system.TieTo(creature, wood));
	ASSERT_TRUE(_system.PressKey(PlayerNames::PLAYER_ONE, creature_leash::LeashKey::Leash));
	EXPECT_FALSE(_system.TiedTo(creature).has_value());
	EXPECT_FALSE(helping.desire.has_value());
	EXPECT_TRUE(helping.choosesFreely);
	// a flag whose desire has no actions leaves it helping with none and choosing freely
	helping = {.desire = 3, .choosesFreely = false};
	ASSERT_TRUE(_system.TieTo(creature, food));
	EXPECT_FALSE(helping.desire.has_value());
	EXPECT_TRUE(helping.choosesFreely);
}

TEST_F(LeashSystemTest, EveryLeashThatGoesOnIsPickedAtItsOwnersTemple)
{
	auto& registry = test::creature_world::World::Registry();
	const auto templeOf = [&registry](PlayerNames owner, float x) {
		const auto temple = registry.Create();
		registry.Assign<Temple>(temple, owner);
		registry.Assign<CitadelWorship>(temple);
		registry.Assign<CitadelHeart>(temple);
		registry.Assign<TempleLeash>(temple);
		registry.Assign<Transform>(temple, glm::vec3(x, 0.0f, 300.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return temple;
	};
	const auto mine = templeOf(PlayerNames::PLAYER_ONE, 100.0f);
	const auto theirs = templeOf(PlayerNames::PLAYER_TWO, 400.0f);
	MakePlayer(PlayerNames::PLAYER_ONE);
	const auto creature = Leadable();
	_system.SetKnown(creature, LeashType::Evil, true);
	const auto post = registry.Create();
	registry.Assign<Transform>(post, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));

	// put on: the learning post is picked, and only at the owner's temple
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	EXPECT_EQ(worship::temple_leash::Pick(mine), 1);
	EXPECT_EQ(worship::temple_leash::Pick(theirs), worship::leash_posts::k_NoPick);
	// another post picked at the temple, the tie of the leash worn picks the worn one again
	worship::temple_leash::SetPick(mine, LeashType::Good);
	ASSERT_TRUE(_system.TieTo(creature, post));
	EXPECT_EQ(worship::temple_leash::Pick(mine), 1);
	// so does the leash put back in the hand
	registry.Get<TempleLeash>(mine).pick = worship::leash_posts::k_NoPick;
	_system.PutInHand(creature);
	EXPECT_EQ(worship::temple_leash::Pick(mine), 1);
	// taking it off leaves the pick
	registry.Get<TempleLeash>(mine).pick = worship::leash_posts::k_NoPick;
	_system.TakeOff(creature);
	EXPECT_EQ(worship::temple_leash::Pick(mine), worship::leash_posts::k_NoPick);
	// the aggression leash put on picks its post
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Evil));
	EXPECT_EQ(worship::temple_leash::Pick(mine), 0);
	EXPECT_EQ(worship::temple_leash::Pick(theirs), worship::leash_posts::k_NoPick);
}

TEST_F(LeashSystemTest, TheLeashStepKeysSendTheStepAsTheLeashTypesPacket)
{
	static std::vector<game_packets::Packet> sent;
	sent.clear();
	game_packets::Reset();
	game_packets::SetHandler(game_packets::Type::LeashType, [](const game_packets::Packet& packet) { sent.push_back(packet); });
	MakePlayer(PlayerNames::PLAYER_ONE);
	const auto creature = Leadable();
	_system.SetKnown(creature, LeashType::Evil, true);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));

	// B steps one number down, from the learning leash to the aggression leash; the leash stays as it is until the turn
	EXPECT_TRUE(_system.PressKey(PlayerNames::PLAYER_ONE, creature_leash::LeashKey::NextLeash));
	EXPECT_EQ(_system.TypeOf(creature), LeashType::Rope);
	// V would land on the compassion leash, which it doesn't know: nothing is sent
	EXPECT_FALSE(_system.PressKey(PlayerNames::PLAYER_ONE, creature_leash::LeashKey::PreviousLeash));
	// nothing else is asked: without the learning leash the step is sent all the same
	LeashOf(creature).known.reset(*creature_leash::IndexOf(LeashType::Rope));
	EXPECT_TRUE(_system.PressKey(PlayerNames::PLAYER_ONE, creature_leash::LeashKey::NextLeash));
	EXPECT_FALSE(_system.LastRefusal(PlayerNames::PLAYER_ONE).has_value());

	game_packets::Flush();
	game_packets::DispatchQueuedPackets();
	ASSERT_EQ(sent.size(), 2u);
	for (const auto& packet : sent)
	{
		EXPECT_EQ(packet.value, static_cast<int32_t>(LeashType::Evil));
		EXPECT_EQ(packet.player, PlayerNames::PLAYER_ONE);
	}
	game_packets::Reset();
	game_packets::ClearHandlers();
}

TEST_F(LeashSystemTest, PulledAwayFromItsActionItBrakesToAStopAndGivesUpItsPlanAndAnyWalk)
{
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	mind.planActive = true;
	mind.leash.obeying = true;
	mind.planner.current = creature_planner::Plan {.desire = Desire::Hunger, .action = 5, .object = 999};
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	LeashOf(creature).returning = true;
	LeashOf(creature).control = CreatureLeash::Control::Led;
	const test::RestoreService<Locator::magicSystem> restoreMagic;
	const auto& magic = static_cast<ReleasingMagic&>(Locator::magicSystem::emplace<ReleasingMagic>());
	_system.PullAwayFromAction(creature);
	// a walk braking to a stop, as the original's action finished unsuccessfully
	EXPECT_EQ(_locomotion->braked, (std::vector<entt::entity> {creature}));
	EXPECT_EQ(_locomotion->stopped, (std::vector<entt::entity> {creature}));
	// it lets go of a miracle it holds
	EXPECT_EQ(magic.released, (std::vector<entt::entity> {creature}));
	EXPECT_FALSE(LeashOf(creature).returning);
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Idle);
	EXPECT_FALSE(mind.leash.obeying);
	EXPECT_FALSE(mind.planActive);
	EXPECT_FALSE(mind.planner.current.has_value());
	// the leash stays on, and a pull away is not counted against the desire
	EXPECT_TRUE(_system.IsLeashed(creature));
}

// Pulled away from its action, the body's own action ends as the action does: a static action's loop plays its end,
// which the plan that follows lets play out
TEST_F(LeashSystemTest, PulledAwayFromItsActionItsBodyPlaysTheEndOfWhatItDid)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	if (!std::as_const(registry).AllOf<CreatureAnimation>(creature))
	{
		registry.Assign<CreatureAnimation>(creature);
	}
	auto& animation = registry.Get<CreatureAnimation>(creature);
	const auto sitting = creature_layers::PlaySequence({}, 10, 11, 12);
	ASSERT_TRUE(sitting.has_value());
	animation.body = *sitting;
	animation.body.phase = creature_layers::BodyAction::Phase::Loop;
	_system.PullAwayFromAction(creature);
	EXPECT_TRUE(animation.body.endWanted);
	EXPECT_TRUE(creature_layers::IsPlaying(animation.body));
	EXPECT_TRUE(mind.leash.bodyEndPlaysOut);
	// standing, there is nothing to play out
	animation.body = {};
	_system.PullAwayFromAction(creature);
	EXPECT_FALSE(mind.leash.bodyEndPlaysOut);
}

TEST_F(LeashSystemTest, ToldToActOnAThingItsMindIsToldEveryTime)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	const auto tree = registry.Create();
	registry.Assign<Transform>(tree, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Tree>(tree);
	auto& mind = registry.Get<CreatureMindState>(creature);
	// tied, the tie alone tells it nothing, as a script's tie; the act after the hand's tie tells it once
	ASSERT_TRUE(_system.TieTo(creature, tree));
	const auto told = static_cast<uint32_t>(tree);
	EXPECT_TRUE(mind.leash.actOn.empty());
	EXPECT_EQ(mind.leash.shown.size(), 1u);
	_system.ActOn(creature, tree);
	EXPECT_EQ(mind.leash.actOn, (std::vector<uint32_t> {told}));
	// two taps on the same thing are two acts
	mind.leash.actOn.clear();
	_system.ActOn(creature, tree);
	_system.ActOn(creature, tree);
	EXPECT_EQ(mind.leash.actOn, (std::vector<uint32_t> {told, told}));
	// itself and a thing gone are not added
	mind.leash.actOn.clear();
	_system.ActOn(creature, tree);
	_system.ActOn(creature, creature);
	const auto gone = registry.Create();
	registry.Destroy(gone);
	_system.ActOn(creature, gone);
	EXPECT_EQ(mind.leash.actOn, (std::vector<uint32_t> {static_cast<uint32_t>(tree)}));
}

TEST_F(LeashSystemTest, ToldToActOnAThingItAcknowledgesTheOrderEveryTime)
{
	const test::RestoreService<Locator::creatureAudioSystem> restoreCreatureAudio;
	auto& voices = static_cast<HeardSounds&>(Locator::creatureAudioSystem::emplace<HeardSounds>());
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& samples = static_cast<InGameSamples&>(Locator::audio::emplace<InGameSamples>());
	std::vector<std::string> heard;
	voices.heard = &heard;
	samples.heard = &heard;
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	const auto tree = registry.Create();
	registry.Assign<Transform>(tree, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Tree>(tree);
	heard.clear();
	_system.ActOn(creature, tree);
	_system.ActOn(creature, tree);
	EXPECT_EQ(heard, (std::vector<std::string> {"in-game sample 1", "creature's acknowledgement", "in-game sample 1",
	                                            "creature's acknowledgement"}));
	// nothing to act on, no order and no sound
	heard.clear();
	_system.ActOn(creature, creature);
	EXPECT_TRUE(heard.empty());
}

// Told to act on a thing it can pick up while its hands are empty, its desires don't weigh the thing: what it was doing
// stops as a failure, it is made curious about the thing and holds it up, and the thing is marked as its target. Holding
// something, or with a thing it can't pick up, the thing is left to its desires as before
TEST_F(LeashSystemTest, ToldToActOnAThingItCanPickUpWithItsHandsEmptyItHoldsItUp)
{
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	auto& hands = static_cast<openblack::test::creature_fakes::FakeObjectAction&>(
	    Locator::creatureObjectActionSystem::emplace<openblack::test::creature_fakes::FakeObjectAction>());
	const test::RestoreService<Locator::creatureMindSystem> restoreMind;
	auto& minds = static_cast<openblack::test::creature_fakes::FakeMind&>(
	    Locator::creatureMindSystem::emplace<openblack::test::creature_fakes::FakeMind>());
	minds.activityResult = true;
	hands.canPickUp = true;
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	auto& mind = registry.Get<CreatureMindState>(creature);
	const auto cow = registry.Create();
	registry.Assign<Transform>(cow, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	// on its way where the leash sent it before
	LeashOf(creature).returning = true;
	mind.leash.obeying = true;

	_system.ActOn(creature, cow);
	ASSERT_EQ(minds.activities.size(), 1u);
	EXPECT_EQ(minds.activities.back().first, creature);
	EXPECT_EQ(minds.activities.back().second.desire, Desire::Curiosity);
	EXPECT_EQ(minds.activities.back().second.action, "HoldObject");
	EXPECT_EQ(minds.activities.back().second.actionObject, cow);
	EXPECT_FALSE(minds.activities.back().second.activityObject.has_value());
	EXPECT_TRUE(mind.leash.actOn.empty());
	EXPECT_FALSE(LeashOf(creature).returning);
	EXPECT_FALSE(mind.leash.obeying);
	EXPECT_EQ(mind.turnsSinceMadeToAct, 0u);
	EXPECT_TRUE(LeashOf(creature).hasTarget);
	EXPECT_EQ(LeashOf(creature).targetThing, cow);

	// a plan it could not carry out marks nothing
	LeashOf(creature).hasTarget = false;
	LeashOf(creature).targetThing = entt::null;
	minds.activityResult = false;
	_system.ActOn(creature, cow);
	EXPECT_EQ(minds.activities.size(), 2u);
	EXPECT_FALSE(LeashOf(creature).hasTarget);
	EXPECT_TRUE(mind.leash.actOn.empty());

	// holding something, its desires weigh the thing
	registry.Assign<CreatureHeldObject>(creature, CreatureHeldObject {.object = registry.Create()});
	_system.ActOn(creature, cow);
	EXPECT_EQ(minds.activities.size(), 2u);
	EXPECT_EQ(mind.leash.actOn, (std::vector<uint32_t> {static_cast<uint32_t>(cow)}));

	// and so they do with its hands empty and a thing it can't pick up
	registry.Remove<CreatureHeldObject>(creature);
	mind.leash.actOn.clear();
	hands.canPickUp = false;
	_system.ActOn(creature, cow);
	EXPECT_EQ(minds.activities.size(), 2u);
	EXPECT_EQ(mind.leash.actOn, (std::vector<uint32_t> {static_cast<uint32_t>(cow)}));
}

// Told to act on a thing it can't pick up, or with its hands full, what it was doing stops as a failure before its mind
// takes the thing up, as before it holds a thing up: it stops, a walk the leash sent it on is over, it is no longer led
// and its plan is given up
TEST_F(LeashSystemTest, ToldToActOnAThingItIsPulledAwayFromWhatItWasDoingFirst)
{
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	Locator::creatureObjectActionSystem::emplace<openblack::test::creature_fakes::FakeObjectAction>();
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	const auto tree = registry.Create();
	registry.Assign<Transform>(tree, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Tree>(tree);
	const auto busy = [&] {
		mind.planActive = true;
		mind.leash.obeying = true;
		mind.planner.current = creature_planner::Plan {.desire = Desire::Hunger, .action = 5, .object = 999};
		LeashOf(creature).returning = true;
		LeashOf(creature).control = CreatureLeash::Control::Led;
	};
	// no longer led: sent now to act on the thing
	const auto pulledAway = [&] {
		EXPECT_FALSE(LeashOf(creature).returning);
		EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Sent);
		EXPECT_FALSE(mind.leash.obeying);
		EXPECT_FALSE(mind.planActive);
		EXPECT_FALSE(mind.planner.current.has_value());
	};

	// a thing it can't pick up, its hands empty
	busy();
	_system.ActOn(creature, tree);
	EXPECT_EQ(_locomotion->stopped, (std::vector<entt::entity> {creature}));
	pulledAway();
	EXPECT_EQ(mind.leash.actOn, (std::vector<uint32_t> {static_cast<uint32_t>(tree)}));

	// and with its hands full
	mind.leash.actOn.clear();
	registry.Assign<CreatureHeldObject>(creature, CreatureHeldObject {.object = registry.Create()});
	busy();
	_system.ActOn(creature, tree);
	EXPECT_EQ(_locomotion->stopped, (std::vector<entt::entity> {creature, creature}));
	pulledAway();
	EXPECT_EQ(mind.leash.actOn, (std::vector<uint32_t> {static_cast<uint32_t>(tree)}));
}

// A thing left to its mind is known as its player's order, which it is made curious about when its desires find nothing
// to do with it; a thing it holds up at once leaves no such order behind
TEST_F(LeashSystemTest, ToldToActOnAThingItsMindKnowsItAsItsPlayersOrder)
{
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	auto& hands = static_cast<openblack::test::creature_fakes::FakeObjectAction&>(
	    Locator::creatureObjectActionSystem::emplace<openblack::test::creature_fakes::FakeObjectAction>());
	const test::RestoreService<Locator::creatureMindSystem> restoreMind;
	Locator::creatureMindSystem::emplace<openblack::test::creature_fakes::FakeMind>();
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	auto& mind = registry.Get<CreatureMindState>(creature);
	const auto tree = registry.Create();
	registry.Assign<Transform>(tree, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto cow = registry.Create();
	registry.Assign<Transform>(cow, glm::vec3(130.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));

	_system.ActOn(creature, tree);
	EXPECT_EQ(mind.leash.ordered, std::optional(static_cast<uint32_t>(tree)));
	// the last order is the one it knows
	_system.ActOn(creature, cow);
	EXPECT_EQ(mind.leash.ordered, std::optional(static_cast<uint32_t>(cow)));
	// held up at once, the order is carried out and none is left
	hands.canPickUp = true;
	_system.ActOn(creature, cow);
	EXPECT_FALSE(mind.leash.ordered.has_value());
}

// Holding a thing up at its player's order, it carries the order out: sent to a point of the land then, it runs there and
// waits a moment, as when sent again on a walk. Pulled away from what it does, the order is over, and the next walk is
// a first one
TEST_F(LeashSystemTest, CarryingOutItsPlayersOrderASentWalkRunsAndAPullAwayEndsTheOrder)
{
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {4});
	const test::RestoreService<Locator::gameRandom> restoreRandom;
	auto& random = static_cast<PinnedRandom&>(Locator::gameRandom::emplace<PinnedRandom>());
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	auto& hands = static_cast<openblack::test::creature_fakes::FakeObjectAction&>(
	    Locator::creatureObjectActionSystem::emplace<openblack::test::creature_fakes::FakeObjectAction>());
	const test::RestoreService<Locator::creatureMindSystem> restoreMind;
	auto& minds = static_cast<openblack::test::creature_fakes::FakeMind&>(
	    Locator::creatureMindSystem::emplace<openblack::test::creature_fakes::FakeMind>());
	minds.activityResult = true;
	hands.canPickUp = true;
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	_hand->position = glm::vec3(100.0f, 30.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	const auto cow = registry.Create();
	registry.Assign<Transform>(cow, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_FALSE(mind.commanded);

	// held up at its player's order
	_system.ActOn(creature, cow);
	ASSERT_EQ(minds.activities.size(), 1u);
	EXPECT_TRUE(mind.commanded);

	// a walk it is sent on now runs, and draws its wait at once
	random.floatDraws.clear();
	_system.ActOnPoint(creature, glm::vec3(130.0f, 0.0f, 100.0f));
	ASSERT_EQ(_locomotion->moves.size(), 1u);
	EXPECT_TRUE(_locomotion->moves.back().run);
	EXPECT_TRUE(LeashOf(creature).waitAfterWalk.has_value());
	EXPECT_EQ(random.floatDraws.size(), 1u);

	// pulled away, the order is over, and the next walk walks with no wait
	_system.PullAwayFromAction(creature);
	EXPECT_FALSE(mind.commanded);
	EXPECT_FALSE(mind.leash.obeying);
	_system.ActOnPoint(creature, glm::vec3(140.0f, 0.0f, 100.0f));
	ASSERT_EQ(_locomotion->moves.size(), 2u);
	EXPECT_FALSE(_locomotion->moves.back().run);
	EXPECT_FALSE(LeashOf(creature).waitAfterWalk.has_value());
	EXPECT_EQ(random.floatDraws.size(), 1u);
}

// Told to act on a thing, whatever came of it, it is doing what the leash sent it to do (it is not walked back
// meanwhile) while its mind is still to take the thing up or carries out its plan for it, standing still or not; then its
// mark goes, as it stops what it was doing
TEST_F(LeashSystemTest, ToldToActOnAThingItIsSentUntilItsMindIsDoneWithTheThing)
{
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	Locator::creatureObjectActionSystem::emplace<openblack::test::creature_fakes::FakeObjectAction>();
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	_hand->position = glm::vec3(100.0f, 30.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	const auto tree = registry.Create();
	registry.Assign<Transform>(tree, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_locomotion->moving = false;

	_system.ActOn(creature, tree);
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Sent);
	EXPECT_TRUE(LeashOf(creature).sentToThing);
	// its mind is still to take the thing up
	_system.ProcessTurn();
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Sent);
	// taken up, it carries out its plan for the thing
	mind.leash.actOn.clear();
	mind.leash.ordered.reset();
	mind.planActive = true;
	mind.idle.agenda.resize(1);
	mind.idle.step = 0;
	LeashOf(creature).hasTarget = true;
	_system.ProcessTurn();
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Sent);
	EXPECT_TRUE(LeashOf(creature).hasTarget);
	// the plan over, it is done, and its mark goes
	mind.idle.step = 1;
	_system.ProcessTurn();
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Idle);
	EXPECT_FALSE(LeashOf(creature).sentToThing);
	EXPECT_FALSE(LeashOf(creature).hasTarget);
}

TEST_F(LeashSystemTest, WhetherTheLeashWorksIsKeptWithNoLeashOnForTheNextLeash)
{
	const auto creature = Leadable();
	// set before any leash is on, the next leash put on takes it
	_system.SetWorks(creature, false);
	EXPECT_FALSE(_system.Works(creature));
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	EXPECT_FALSE(_system.Works(creature));
	// set while it is on, it stays for the leash after it
	_system.SetWorks(creature, true);
	_system.TakeOff(creature);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	EXPECT_TRUE(_system.Works(creature));
}

TEST_F(LeashSystemTest, LetGoWithNoLeashOnItIsFreedOfItsAreaAndOfADominantDesireButOnTheLearningLeash)
{
	const auto creature = Leadable();
	auto& mind = GiveDesires(creature);
	_system.ConfineToHome(creature, 20.0f);
	ASSERT_EQ(LeashOf(creature).confinementRadius, 20.0f);

	// the learning leash picked: the area goes, the dominant desire stays
	LeashOf(creature).selected = LeashType::Rope;
	mind.dominantDesire = creature_spell_mind::SetCheatDominant(*mind.desires, Desire::Impress, false, 10.0f, 0.0f, 120.0f);
	_system.ReleaseUnleashed(creature);
	EXPECT_EQ(LeashOf(creature).confinementRadius, 0.0f);
	EXPECT_EQ(mind.dominantDesire.desire, std::optional(Desire::Impress));

	// any other leash picked: the dominant desire lets go too
	LeashOf(creature).selected = LeashType::Good;
	_system.ReleaseUnleashed(creature);
	EXPECT_FALSE(mind.dominantDesire.desire.has_value());

	// a worn leash is left alone
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	LeashOf(creature).confinementRadius = 30.0f;
	_system.ReleaseUnleashed(creature);
	EXPECT_EQ(LeashOf(creature).confinementRadius, 30.0f);
	EXPECT_TRUE(_system.IsLeashed(creature));
}

TEST_F(LeashSystemTest, LetGoWithNoLeashOnItStillWipesTheLocalOwnersGesturePath)
{
	const test::RestoreService<Locator::gestureSystem> restoreGestures;
	auto& gestures = static_cast<PathGestures&>(Locator::gestureSystem::emplace<PathGestures>());
	const auto creature = Leadable();
	ASSERT_FALSE(_system.IsLeashed(creature));
	// the player at this machine's creature, with nothing on: the path goes all the same, every time
	_system.ReleaseUnleashed(creature);
	EXPECT_EQ(gestures.forgotten, 1);
	_system.ReleaseUnleashed(creature);
	EXPECT_EQ(gestures.forgotten, 2);
	// another player's creature leaves the local hand's path as it is
	const auto theirs = test::creature_world::World::MakeCreature(glm::vec3(200.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	_system.ReleaseUnleashed(theirs);
	EXPECT_EQ(gestures.forgotten, 2);
	// a worn leash is left alone: its own taking off wipes the path
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	ASSERT_EQ(gestures.forgotten, 3);
	_system.ReleaseUnleashed(creature);
	EXPECT_EQ(gestures.forgotten, 3);
}

// A pull of the tied rope let go gives the tied leash its full length and half of it as its slack, and the turn keeps the
// creature within the new length of what it is tied to
TEST_F(LeashSystemTest, APulledLengthIsTheTiedLeashsFullLengthWithHalfOfItAsItsSlack)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	const auto post = registry.Create();
	registry.Assign<Transform>(post, glm::vec3(150.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(_system.TieTo(creature, post));
	_system.SetPulledLength(creature, 30.0f);
	EXPECT_EQ(LeashOf(creature).worn->rope.maxLength, 30.0f);
	EXPECT_EQ(LeashOf(creature).worn->rope.slackLength, 15.0f);
	// 50 from the post, beyond the 30: it walks back to it
	registry.Get<Transform>(creature).position = glm::vec3(200.0f, 0.0f, 100.0f);
	_system.ProcessTurn();
	ASSERT_EQ(_locomotion->moves.size(), 1u);
	EXPECT_TRUE(_locomotion->moves.front().walkBack);
	EXPECT_EQ(_locomotion->moves.front().value, 30.0f);
}

// Taking hold of the tied rope lays two more ropes from the hand, to the collar half as long again as the tied rope and to
// the post 5 slack, 2 full, both with the tied rope's look; they follow the hand each frame and go as it lets go. With
// the leash untied there is nothing to pull
TEST_F(LeashSystemTest, PullingTheTiedRopeLaysTwoRopesFromTheHandUntilItLetsGo)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = Leadable();
	_hand->position = glm::vec3(130.0f, 0.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	_system.StartPull(creature);
	EXPECT_FALSE(LeashOf(creature).worn->pull.has_value());
	const auto post = registry.Create();
	registry.Assign<Transform>(post, glm::vec3(150.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(_system.TieTo(creature, post));
	_system.Update(k_Frame);
	_system.Update(k_Frame);
	_hand->position = glm::vec3(170.0f, 0.0f, 120.0f);
	_system.StartPull(creature);
	const auto& worn = *LeashOf(creature).worn;
	ASSERT_TRUE(worn.pull.has_value());
	EXPECT_EQ(worn.pull->toCollar.slackLength, worn.rope.slackLength * 1.5f);
	EXPECT_EQ(worn.pull->toCollar.maxLength, worn.rope.maxLength * 1.5f);
	EXPECT_EQ(worn.pull->toTied.slackLength, 5.0f);
	EXPECT_EQ(worn.pull->toTied.maxLength, 2.0f);
	EXPECT_EQ(worn.pull->toCollar.look.halfWidth, worn.rope.look.halfWidth);
	EXPECT_EQ(worn.pull->toTied.start, glm::vec3(170.0f, 0.0f, 120.0f));
	EXPECT_EQ(worn.pull->toTied.end, glm::vec3(150.0f, 0.0f, 100.0f));
	// a frame on, the hand moved: both ropes start at it
	_hand->position = glm::vec3(175.0f, 0.0f, 125.0f);
	_system.Update(k_Frame);
	EXPECT_EQ(worn.pull->toCollar.start, glm::vec3(175.0f, 0.0f, 125.0f));
	EXPECT_EQ(worn.pull->toTied.start, glm::vec3(175.0f, 0.0f, 125.0f));
	_system.EndPull(creature);
	EXPECT_FALSE(LeashOf(creature).worn->pull.has_value());
}

// Sent to act on a thing, the plan it takes marks the thing: the creature target's mark at the thing, the thing its
// target and the point over the thing's top; a mark that has gone is forgotten, and once the thing goes the target goes
TEST_F(LeashSystemTest, AThingTheLeashSendsItToIsItsTargetUntilTheThingGoes)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& visuals = static_cast<SpotVisuals&>(Locator::particleSystem::emplace<SpotVisuals>());
	const auto creature = Leadable();
	GiveDesires(creature);
	_hand->position = glm::vec3(100.0f, 30.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	visuals.started.clear();
	auto& registry = test::creature_world::World::Registry();
	const auto rock = registry.Create();
	registry.Assign<Transform>(rock, glm::vec3(120.0f, 4.0f, 110.0f), glm::mat3(1.0f), glm::vec3(1.0f));

	_system.MarkTargetAt(creature, rock);
	ASSERT_EQ(visuals.started.size(), 1u);
	EXPECT_EQ(visuals.started.front().type, SpotVisualType::CreatureTarget);
	EXPECT_EQ(visuals.started.front().position, glm::vec3(120.0f, 4.0f, 110.0f));
	EXPECT_EQ(visuals.started.front().turns, std::optional<int>(20));
	const auto& leashes = LeashOf(creature);
	EXPECT_TRUE(leashes.hasTarget);
	EXPECT_TRUE(leashes.targetThing == rock);
	EXPECT_EQ(leashes.targetPoint, creature_leash::TargetAbove(glm::vec3(120.0f, 4.0f, 110.0f), ecs::object::GetHeight(rock)));

	// the mark fades: forgotten, the target stays
	registry.Destroy(LeashOf(creature).targetMarker);
	_system.ProcessTurn();
	EXPECT_TRUE(LeashOf(creature).targetMarker == entt::entity {entt::null});
	EXPECT_TRUE(LeashOf(creature).hasTarget);
	// the thing goes: no target
	registry.Destroy(rock);
	_system.ProcessTurn();
	EXPECT_TRUE(LeashOf(creature).targetThing == entt::entity {entt::null});
	EXPECT_FALSE(LeashOf(creature).hasTarget);
}

TEST(LeashTarget, ThePointIsTwoMetresUpThenTheThingsHeight)
{
	EXPECT_EQ(creature_leash::TargetAbove(glm::vec3(1.0f, 3.0f, 5.0f), 0.0f), glm::vec3(1.0f, 5.0f, 5.0f));
	EXPECT_EQ(creature_leash::TargetAbove(glm::vec3(1.0f, 3.0f, 5.0f), 10.5f), glm::vec3(1.0f, 15.5f, 5.0f));
}
