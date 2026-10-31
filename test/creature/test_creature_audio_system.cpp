/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature audio system on a recording audio service: the shared bank for footsteps and blows and the species' own
// for its voice, the modes that stop a sound or let it run out, the creature as the sound's tracked owner on the ground
// it stands on, the script's switch for other players' voices, muting, and the footsteps leaving prints whether heard
// or not; the sounds a turn queues waiting for the drawing to reach them, and the next turn playing what is left; and
// the request the audio is asked for, built on its own

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <bit>
#include <bitset>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/AudioManagerNoOp.h"
#include "Audio/Engine/AnimEffects.h"
#include "Audio/Game/Banks.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/ScriptAudioState.h"
#include "Creature/CreatureAudio.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureThrow.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureAudio.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/Transform.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/Implementations/CreatureAudioSystem.h"
#include "creature/CreatureSystemWorld.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::audio::AnimAction;
using openblack::audio::SoundAction;
using openblack::audio::SoundSurface;
using openblack::creature_audio::EventKind;
using openblack::creature_audio::SoundEvent;
using openblack::ecs::systems::CreatureAudioSystem;

namespace
{
constexpr audio::BankId k_VoiceBank = 42;

/// Records what the creatures ask of the audio; every species has a voice bank
class RecordingAudio final: public audio::AudioManagerNoOp
{
public:
	struct Played
	{
		audio::Owner owner;
		float distance;
		audio::AnimKey key;
		AnimAction action;
		audio::BankId bank;
		bool track;
	};
	std::vector<Played> played;
	std::vector<std::string> voiceBanks;

	[[nodiscard]] audio::BankId CreatureBank(std::string_view species) override
	{
		voiceBanks.emplace_back(species);
		return k_VoiceBank;
	}
	audio::Channel PlayAnimationEffect(audio::Owner owner, float distance, const audio::AnimKey& key, AnimAction action,
	                                   audio::BankId bank, bool track, float, float) override
	{
		played.push_back({owner, distance, key, action, bank, track});
		return 5;
	}
};

/// Counts the footsteps that reach it, and how many sounds the audio had been asked for by then
class FakeFootprints final: public ecs::systems::FootprintSystemInterface
{
public:
	std::vector<entt::entity> steps;
	const RecordingAudio* audio {nullptr};
	std::vector<size_t> soundsBefore;

	void Update(std::chrono::duration<float, std::milli>) override {}
	void Step(entt::entity creature) override
	{
		steps.push_back(creature);
		soundsBefore.push_back(audio != nullptr ? audio->played.size() : 0);
	}
	void Reset() override {}
	[[nodiscard]] std::span<const creature_footprints::Footprint> GetPrints() const override { return {}; }
	[[nodiscard]] size_t GetDroppedCount() const override { return 0; }
	[[nodiscard]] bool IsShown() const override { return true; }
	void SetShown(bool) override {}
	[[nodiscard]] std::optional<bool> GetAprilFoolsOverride() const override { return std::nullopt; }
	void SetAprilFoolsOverride(std::optional<bool>) override {}
	[[nodiscard]] bool IsAprilFools() const override { return false; }
};

constexpr SoundEvent k_Footstep {.kind = EventKind::Generic, .timeMs = 100, .action = SoundAction::FootstepNormal, .mode = 0};
constexpr SoundEvent k_Roar {.kind = EventKind::Voice, .timeMs = 200, .action = SoundAction::RoarShort, .mode = 0};

TEST(CreatureAudioRequest, TheSharedBankForFootstepsAndTheSpeciesOwnForItsVoice)
{
	const auto footstep =
	    creature_audio::Request(k_Footstep, 1.0f, 0.0f, 3, SoundSurface::Grass, "ape_voice", CreatureType::GiantApe);
	EXPECT_TRUE(footstep.generic);
	EXPECT_TRUE(footstep.voiceStem.empty());
	EXPECT_EQ(footstep.action, AnimAction::Play);
	EXPECT_EQ(footstep.keys, creature_audio::Keys(1.0f, 0.0f, 3, SoundSurface::Grass, SoundAction::FootstepNormal));

	const auto roar = creature_audio::Request(k_Roar, 2.0f, -1.0f, 3, SoundSurface::Mud, "ape_voice", CreatureType::GiantApe);
	EXPECT_FALSE(roar.generic);
	EXPECT_EQ(roar.voiceStem, creature_audio::VoiceBankStem("ape_voice", CreatureType::GiantApe));
	EXPECT_EQ(roar.keys, creature_audio::Keys(2.0f, -1.0f, 3, SoundSurface::Mud, SoundAction::RoarShort));
}

TEST(CreatureAudioRequest, ModeOneStopsTheSoundAndTwoLetsItRunOut)
{
	auto event = k_Footstep;
	event.mode = 1;
	EXPECT_EQ(creature_audio::Request(event, 1.0f, 0.0f, 3, SoundSurface::Grass, "", CreatureType::GiantApe).action,
	          AnimAction::Stop);
	event.mode = 2;
	EXPECT_EQ(creature_audio::Request(event, 1.0f, 0.0f, 3, SoundSurface::Grass, "", CreatureType::GiantApe).action,
	          AnimAction::Release);
}

class CreatureAudioSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the listener at the origin, unless a test takes the camera away
		s_NoCamera = false;
		audio::GameQueries queries;
		queries.camera = []() -> std::optional<audio::CameraState> {
			return s_NoCamera ? std::nullopt : std::optional {audio::CameraState {}};
		};
		audio::Init(std::move(queries));
		// the creature rigs' loader reads the meshes the eyes sit on through the file system, which the game always
		// has; made after the audio's start so that it loads no sample banks
		_fileSystem.emplace();
		_restoreAudio.emplace();
		_audio = &static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());
		_restoreFootprints.emplace();
		_footprints = &static_cast<FakeFootprints&>(Locator::footprintSystem::emplace<FakeFootprints>());
		_footprints->audio = _audio;
	}
	void TearDown() override
	{
		_restoreFootprints.reset();
		_restoreAudio.reset();
		_fileSystem.reset();
		audio::Shutdown();
	}

	/// A Giant Ape whose standing animation has a footstep at 100 ms and a roar at 200 ms
	entt::entity Ape(PlayerNames owner)
	{
		test::creature_block::Block block;
		test::creature_block::Clip stand;
		stand.events = {{.type = 2, .frame = 100, .action = static_cast<uint32_t>(SoundAction::FootstepNormal), .mode = 0},
		                {.type = 0, .frame = 200, .action = static_cast<uint32_t>(SoundAction::RoarShort), .mode = 0}};
		block.clips = {stand};
		_world.LoadApeRig(block, {{"move", {"Cstand"}}});
		return test::creature_world::World::MakeCreature(glm::vec3(30.0f, 0.0f, 40.0f), owner);
	}
	/// The standing animation played at a time, by itself
	static void PlayAt(entt::entity creature, float timeMs)
	{
		auto& animation = test::creature_world::World::Registry().Get<CreatureAnimation>(creature);
		animation.slots = {{.animation = 0, .timeMs = timeMs, .weight = 1.0f, .mirrored = false}};
	}
	/// A creature turn that leaves the standing animation at a time: what is left plays, then what it passed is queued
	void TurnAt(entt::entity creature, float timeMs)
	{
		PlayAt(creature, timeMs);
		_system.PlayQueued();
		_system.QueueTurn({}, {});
	}
	/// Two turns, one leaving the stand at 50 ms and the next at 250 ms, which passes both moments, then a frame drawn to
	/// the turn's end, which plays them
	void TwoTurns(entt::entity creature)
	{
		TurnAt(creature, 50.0f);
		TurnAt(creature, 250.0f);
		_system.Update(1.0f, std::chrono::duration<float, std::milli>(16.0f));
	}

	std::optional<test::ScopedDefaultFileSystem> _fileSystem;
	test::creature_world::World _world;
	static inline bool s_NoCamera = false;
	std::optional<test::RestoreService<Locator::audio>> _restoreAudio;
	std::optional<test::RestoreService<Locator::footprintSystem>> _restoreFootprints;
	RecordingAudio* _audio {nullptr};
	FakeFootprints* _footprints {nullptr};
	CreatureAudioSystem _system;
};

TEST_F(CreatureAudioSystemTest, TheLocalCreaturesStepsAndVoicePlayFromItAndItsStepsLeavePrints)
{
	const auto creature = Ape(PlayerNames::PLAYER_ONE);
	TwoTurns(creature);

	ASSERT_EQ(_audio->played.size(), 2u);
	const auto& step = _audio->played.at(0);
	const auto& roar = _audio->played.at(1);
	EXPECT_EQ(step.owner.kind, audio::Owner::Kind::Thing);
	EXPECT_EQ(step.owner.thing, creature);
	EXPECT_TRUE(step.track);
	EXPECT_EQ(step.action, AnimAction::Play);
	EXPECT_EQ(step.bank, audio::Bank(audio::SfxBank::Creature));
	EXPECT_FLOAT_EQ(step.distance, 50.0f);
	const glm::vec3 at(30.0f, 0.0f, 40.0f);
	const auto surface = static_cast<SoundSurface>(ecs::sea_cells::GetSurfaceType(at));
	EXPECT_EQ(step.key, creature_audio::Keys(1.0f, 0.0f, 3, surface, SoundAction::FootstepNormal).ToArray());
	EXPECT_EQ(roar.bank, k_VoiceBank);
	EXPECT_EQ(roar.key[4], static_cast<int32_t>(SoundAction::RoarShort));
	ASSERT_EQ(_audio->voiceBanks.size(), 1u);
	EXPECT_EQ(_audio->voiceBanks.front(), creature_audio::VoiceBankStem("ape_voice", CreatureType::GiantApe));
	ASSERT_EQ(_footprints->steps.size(), 1u);
	EXPECT_EQ(_footprints->steps.front(), creature);

	const auto& heard = test::creature_world::World::Registry().Get<CreatureAudio>(creature).recent;
	ASSERT_EQ(heard.size(), 2u);
	EXPECT_TRUE(heard.front().played);
	EXPECT_EQ(heard.front().channel, 5u);
}

TEST_F(CreatureAudioSystemTest, AFootstepsPrintIsLaidAfterItsSound)
{
	const auto creature = Ape(PlayerNames::PLAYER_ONE);
	TwoTurns(creature);
	// the footstep is the first sound played, and its print comes after it
	ASSERT_EQ(_footprints->soundsBefore.size(), 1u);
	EXPECT_EQ(_footprints->soundsBefore.front(), 1u);
}

TEST_F(CreatureAudioSystemTest, ASoundsDistanceIsSummedDepthFirst)
{
	const auto creature = Ape(PlayerNames::PLAYER_ONE);
	test::creature_world::World::Registry().Get<Transform>(creature).position = glm::vec3(28.0f, 0.2f, 38.2f);
	TwoTurns(creature);

	ASSERT_FALSE(_audio->played.empty());
	// The depth's square, then the height's, then the width's; summed width first it would be one step nearer
	EXPECT_EQ(std::bit_cast<uint32_t>(_audio->played.front().distance), 0x423D7400U);
}

TEST_F(CreatureAudioSystemTest, AnotherPlayersCreatureSpeaksOnlyWhenTheScriptLetsIt)
{
	const auto creature = Ape(PlayerNames::PLAYER_TWO);
	_system.SetOtherVoicesEnabled(false);
	EXPECT_FALSE(_system.AreOtherVoicesEnabled());
	EXPECT_EQ(audio::GetScriptAudioState().creatureSound.load(), 0);
	TwoTurns(creature);
	// its footstep is heard, its roar is not
	ASSERT_EQ(_audio->played.size(), 1u);
	EXPECT_EQ(_audio->played.front().key[4], static_cast<int32_t>(SoundAction::FootstepNormal));

	_system.SetOtherVoicesEnabled(true);
	EXPECT_TRUE(_system.AreOtherVoicesEnabled());
	_audio->played.clear();
	TwoTurns(creature);
	EXPECT_EQ(_audio->played.size(), 2u);
}

TEST_F(CreatureAudioSystemTest, WithoutACameraItsSoundsDoNothingAtAll)
{
	// the game has no camera: no sound, and no print either
	s_NoCamera = true;
	const auto creature = Ape(PlayerNames::PLAYER_ONE);
	TwoTurns(creature);
	EXPECT_TRUE(_audio->played.empty());
	EXPECT_TRUE(_footprints->steps.empty());
}

TEST_F(CreatureAudioSystemTest, MutedNothingPlaysButTheStepsStillLeavePrints)
{
	const auto creature = Ape(PlayerNames::PLAYER_ONE);
	_system.SetMuted(true);
	TwoTurns(creature);
	EXPECT_TRUE(_audio->played.empty());
	EXPECT_EQ(_footprints->steps.size(), 1u);
}

TEST_F(CreatureAudioSystemTest, InItsPenItsSoundsAreKeyedByThePenSize)
{
	const auto creature = Ape(PlayerNames::PLAYER_ONE);
	auto& registry = test::creature_world::World::Registry();
	registry.Get<Creature>(creature).size = 2.0f;
	registry.Get<CreatureDrawPose>(creature).size = 0.22f;
	TwoTurns(creature);

	ASSERT_EQ(_audio->played.size(), 2u);
	const glm::vec3 at(30.0f, 0.0f, 40.0f);
	const auto surface = static_cast<SoundSurface>(ecs::sea_cells::GetSurfaceType(at));
	// clamp(ftol((2 - 0.22) x 1.5 + 1), 1, 3) = 3, small, where its own size 2 is large
	EXPECT_EQ(creature_audio::SizeKey(0.22f), audio::SoundSize::Small);
	EXPECT_EQ(creature_audio::SizeKey(2.0f), audio::SoundSize::Large);
	EXPECT_EQ(_audio->played.at(0).key, creature_audio::Keys(0.22f, 0.0f, 3, surface, SoundAction::FootstepNormal).ToArray());
	EXPECT_EQ(_audio->played.at(1).key[0], static_cast<int32_t>(audio::SoundSize::Small));
}

TEST_F(CreatureAudioSystemTest, ATurnsSoundsWaitForTheDrawingToReachThemAndTheNextTurnPlaysWhatIsLeft)
{
	const auto creature = Ape(PlayerNames::PLAYER_ONE);
	TurnAt(creature, 50.0f);
	TurnAt(creature, 250.0f);
	// the turn passed the step a quarter of the way through its 200 ms and the roar three quarters, but the body is
	// drawn a turn behind
	EXPECT_TRUE(_audio->played.empty());
	const auto drawnAt = [this](float turnFraction) {
		_system.Update(turnFraction, std::chrono::duration<float, std::milli>(16.0f));
		return _audio->played.size();
	};
	EXPECT_EQ(drawnAt(0.0f), 0u);
	EXPECT_EQ(drawnAt(0.25f), 0u);
	// once the drawing has passed it, the step plays, with its print
	EXPECT_EQ(drawnAt(0.3f), 1u);
	EXPECT_EQ(_audio->played.back().key[4], static_cast<int32_t>(SoundAction::FootstepNormal));
	EXPECT_EQ(_footprints->steps.size(), 1u);
	EXPECT_EQ(drawnAt(0.7f), 1u);
	// the next turn plays the roar the drawing did not reach before anything moves on
	_system.PlayQueued();
	ASSERT_EQ(_audio->played.size(), 2u);
	EXPECT_EQ(_audio->played.back().key[4], static_cast<int32_t>(SoundAction::RoarShort));
	EXPECT_EQ(drawnAt(2.0f), 2u);
}

TEST_F(CreatureAudioSystemTest, ATurnQueuesWhatTheBodysLayersPassedWhenItPlaysNothingByWeight)
{
	const auto creature = Ape(PlayerNames::PLAYER_ONE);
	// the body's action passed the stand from 0 to 250 ms, and it plays nothing by weight at the turn's end
	const ecs::systems::CreatureAnimationSystemInterface::TurnMoments passed {
	    {creature,
	     {.overlays = {},
	      .action = creature_layers::PassedAnimation {
	          .animation = 0, .advance = {.timeMs = 250, .stretches = {{{.fromMs = 0, .toMs = 250}, {}}}, .passedCount = 1}}}}};
	const ecs::systems::CreatureAnimationSystemInterface::TurnInputs atEnd {{creature, {}}};
	_system.QueueTurn(passed, atEnd);
	const auto& queued = test::creature_world::World::Registry().Get<CreatureAudio>(creature).queued.Pending();
	ASSERT_EQ(queued.size(), 2u);
	EXPECT_EQ(queued[0].fraction, 0.4f);
	EXPECT_EQ(queued[0].action, SoundAction::FootstepNormal);
	EXPECT_TRUE(queued[0].generic);
	EXPECT_EQ(queued[1].fraction, 0.8f);
	EXPECT_EQ(queued[1].action, SoundAction::RoarShort);
	EXPECT_FALSE(queued[1].generic);
	_system.Update(0.5f, std::chrono::duration<float, std::milli>(16.0f));
	EXPECT_EQ(_audio->played.size(), 1u);
}

TEST_F(CreatureAudioSystemTest, AMomentShowsOrHidesATuftOfHairAsTheTurnPassesIt)
{
	// the stand's moments, as the file lists them: tuft 1 hidden at 100 ms, tuft 2 hidden at 200 ms and shown again at
	// 220 ms, and a tuft past the eight a creature can have hidden at 230 ms
	test::creature_block::Block block;
	test::creature_block::Clip stand;
	stand.events = {{.type = 1, .frame = 100, .action = 1, .mode = 1},
	                {.type = 1, .frame = 200, .action = 2, .mode = 1},
	                {.type = 1, .frame = 220, .action = 2, .mode = 0},
	                {.type = 1, .frame = 230, .action = 9, .mode = 1}};
	block.clips = {stand};
	_world.LoadApeRig(block, {{"move", {"Cstand"}}});
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(30.0f, 0.0f, 40.0f));
	// the body's action passed the stand from 0 to 250 ms
	const ecs::systems::CreatureAnimationSystemInterface::TurnMoments passed {
	    {creature,
	     {.overlays = {},
	      .action = creature_layers::PassedAnimation {
	          .animation = 0, .advance = {.timeMs = 250, .stretches = {{{.fromMs = 0, .toMs = 250}, {}}}, .passedCount = 1}}}}};
	const ecs::systems::CreatureAnimationSystemInterface::TurnInputs atEnd {{creature, {}}};
	_system.QueueTurn(passed, atEnd);
	// the moments one stretch passes are taken from the last listed to the first: tuft 2 is shown, then hidden
	const auto& registry = test::creature_world::World::Registry();
	EXPECT_EQ(registry.Get<CreatureHair>(creature).hidden, std::bitset<CreatureHair::k_MaxGroups>("00000110"));
	// and none of them is a sound
	EXPECT_TRUE(registry.Get<CreatureAudio>(creature).queued.Pending().empty());
}

TEST_F(CreatureAudioSystemTest, EatingSoundsTheBitesAndChewsOfItsEatingAnimationInTheSpeciesOwnVoice)
{
	// A Giant Ape whose eating animation, alone in the spec's place for it, bites at 100 ms and chews at 200 ms
	test::creature_block::Block block;
	block.clips.resize(creature_throw::k_Eat + 1);
	test::creature_block::Clip eat;
	eat.events = {{.type = 0, .frame = 100, .action = static_cast<uint32_t>(SoundAction::Bite), .mode = 0},
	              {.type = 0, .frame = 200, .action = static_cast<uint32_t>(SoundAction::Chew), .mode = 0}};
	block.clips.at(creature_throw::k_Eat) = eat;
	std::vector<std::string> names;
	names.emplace_back("Cstand");
	for (size_t i = 1; i <= creature_throw::k_Eat; ++i)
	{
		names.push_back("Canimation" + std::to_string(i));
	}
	_world.LoadApeRig(block, {{"move", names}});
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(30.0f, 0.0f, 40.0f), PlayerNames::PLAYER_ONE);
	auto& animation = test::creature_world::World::Registry().Get<CreatureAnimation>(creature);
	const auto eatAt = [this, &animation](float timeMs) {
		animation.slots = {{.animation = creature_throw::k_Eat, .timeMs = timeMs, .weight = 1.0f, .mirrored = false}};
		_system.PlayQueued();
		_system.QueueTurn({}, {});
	};

	// the turns that pass the moments queue them, and the frame drawn to the turn's end plays them, from its voice bank
	eatAt(50.0f);
	eatAt(250.0f);
	_system.Update(1.0f, std::chrono::duration<float, std::milli>(16.0f));
	ASSERT_EQ(_audio->played.size(), 2u);
	EXPECT_EQ(_audio->played.at(0).bank, k_VoiceBank);
	EXPECT_EQ(_audio->played.at(0).key[4], static_cast<int32_t>(SoundAction::Bite));
	EXPECT_EQ(_audio->played.at(1).bank, k_VoiceBank);
	EXPECT_EQ(_audio->played.at(1).key[4], static_cast<int32_t>(SoundAction::Chew));
	for (const auto& sound : _audio->played)
	{
		EXPECT_EQ(sound.owner.thing, creature);
		EXPECT_EQ(sound.action, AnimAction::Play);
	}
	// each of the two asks for the species' voice bank
	ASSERT_EQ(_audio->voiceBanks.size(), 2u);
	for (const auto& bank : _audio->voiceBanks)
	{
		EXPECT_EQ(bank, creature_audio::VoiceBankStem("ape_voice", CreatureType::GiantApe));
	}
	// no step of its own: no print
	EXPECT_TRUE(_footprints->steps.empty());
}

// Stroked or slapped by the hand, the creature plays its reaction's animation, and that animation's moments sound in its
// species' own voice, owned by it: the stroke's reward animation and the slap's
TEST_F(CreatureAudioSystemTest, AStrokeOrASlapSoundsTheMomentsOfItsReactionsAnimation)
{
	const auto slap = creature_feedback::ClassifySlap(5.0f, 1000.0f, 10.0f, false);
	ASSERT_TRUE(slap.has_value());
	const size_t stroke = creature_feedback::k_RewardAnimations.front();
	const size_t last = std::max(stroke, slap->animation);
	test::creature_block::Block block;
	block.clips.resize(last + 1);
	test::creature_block::Clip reaction;
	reaction.events = {{.type = 0, .frame = 100, .action = static_cast<uint32_t>(SoundAction::Bite), .mode = 0},
	                   {.type = 0, .frame = 200, .action = static_cast<uint32_t>(SoundAction::Chew), .mode = 0}};
	block.clips.at(stroke) = reaction;
	block.clips.at(slap->animation) = reaction;
	std::vector<std::string> names;
	names.emplace_back("Cstand");
	for (size_t i = 1; i <= last; ++i)
	{
		names.push_back("Canimation" + std::to_string(i));
	}
	_world.LoadApeRig(block, {{"move", names}});
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(30.0f, 0.0f, 40.0f), PlayerNames::PLAYER_ONE);
	auto& animation = test::creature_world::World::Registry().Get<CreatureAnimation>(creature);
	for (const size_t played : {stroke, slap->animation})
	{
		_audio->played.clear();
		const auto at = [this, &animation, played](float timeMs) {
			animation.slots = {{.animation = played, .timeMs = timeMs, .weight = 1.0f, .mirrored = false}};
			_system.PlayQueued();
			_system.QueueTurn({}, {});
		};
		at(50.0f);
		at(250.0f);
		_system.Update(1.0f, std::chrono::duration<float, std::milli>(16.0f));
		ASSERT_EQ(_audio->played.size(), 2u) << "animation " << played;
		EXPECT_EQ(_audio->played.at(0).key[4], static_cast<int32_t>(SoundAction::Bite));
		EXPECT_EQ(_audio->played.at(1).key[4], static_cast<int32_t>(SoundAction::Chew));
		for (const auto& sound : _audio->played)
		{
			EXPECT_EQ(sound.bank, k_VoiceBank);
			EXPECT_EQ(sound.owner.thing, creature);
		}
	}
}

TEST_F(CreatureAudioSystemTest, PlayingAnEventSoundsItAtOnceAndOnlyCreaturesAreHeard)
{
	const auto creature = Ape(PlayerNames::PLAYER_TWO);
	_system.SetMuted(true);
	auto stop = k_Footstep;
	stop.mode = 1;
	_system.Play(creature, stop);
	ASSERT_EQ(_audio->played.size(), 1u);
	EXPECT_EQ(_audio->played.front().action, AnimAction::Stop);

	auto& registry = test::creature_world::World::Registry();
	const auto rock = registry.Create();
	registry.Assign<Transform>(rock, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_system.Play(rock, k_Footstep);
	EXPECT_EQ(_audio->played.size(), 1u);
	EXPECT_FALSE(std::as_const(registry).AllOf<CreatureAudio>(rock));
}
} // namespace
