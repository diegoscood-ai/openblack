/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/
// The miracle FX system (ecs::systems::MiracleFxSystem) over the hand's effects: the bracelets, sounds and in-hand
// effect a miracle in the hand gets, the tribes' names (where a ring starts, what a cast does with it, and that a land
// load clears them), and the steps of the hand, the globes and the piles with the time each is given.
// The system is made here; a fake hand, a camera, a registry, a recording audio service and a recording particle system
// are injected, and the state is read back through magic::hand_fx and the entities' components

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/Audio.h"
#include "Audio/AudioManagerNoOp.h"
#include "Audio/Game/Banks.h"
#include "Camera/Camera.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/MiracleFxSystem.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Enums.h"
#include "Graphics/ArgbColour.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Hand/HandMagicFX.h"
#include "Magic/MagicTables.h"
#include "Magic/TribalPowerSpin.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysManagerState.h"
#include "Particles/Rules/SurfRevol.h"
#include "Particles/SpellLink.h"
#include "creature/CreatureSystemFakes.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using magic::tribal_spin::Runner;
using openblack::ecs::systems::MiracleFxSystem;

namespace
{
constexpr auto k_Tribe = Tribe::AZTEC;
constexpr entt::entity k_NoSeed = entt::null;
/// G_SpellPowerUpBand in the InGame bank
constexpr int k_BandSound = 35;
/// G_ShakeHand_01 in the InGame bank
constexpr int k_ShakeSound = 119;
/// The magic of the seed's base level, whose in-hand effect is the leaves (SF_Forest)
constexpr auto k_Magic = static_cast<MagicType>(7);

/// A player's colour as a ring or column carries it
glm::u8vec4 ColourOf(PlayerNames player)
{
	const uint32_t rgb = psys::surf_revol::PlayerColour(static_cast<int>(player));
	return {static_cast<uint8_t>(argb_colour::Red(rgb)), static_cast<uint8_t>(argb_colour::Green(rgb)),
	        static_cast<uint8_t>(argb_colour::Blue(rgb)), 255};
}

/// A recording audio service: the sample and the bank of each sound played
class RecordingAudio final: public audio::AudioManagerNoOp
{
public:
	struct Played
	{
		int sample;
		audio::SfxBank bank;
	};
	std::vector<Played> played;

	using AudioManagerNoOp::PlaySoundEffect;
	audio::Channel PlaySoundEffect(audio::Owner /*owner*/, int sample, int /*mode*/, int /*loops*/, bool /*extra3DFlag*/,
	                               bool /*is3D*/, audio::SfxBank bank) override
	{
		played.push_back({sample, bank});
		return audio::k_NoChannel;
	}
	/// The sounds played with options
	std::vector<audio::Sample> withOptions;
	audio::Channel PlaySoundEffect(const audio::PlayOptions& options) override
	{
		withOptions.push_back(options.sample);
		return audio::k_NoChannel;
	}
};

/// A recording particle system: the miracle effects started, the time of each step and the effects deleted. Its one
/// effect is the test's
class RecordingParticles final: public test::InertParticleSystem
{
public:
	static constexpr EffectId k_Id = 7;
	struct Started
	{
		std::string file;
		glm::vec3 origin;
	};
	std::vector<Started> started;
	std::vector<float> steps;
	std::vector<EffectId> deleted;
	psys::Effect* effect {nullptr};

	EffectId StartForSpell(std::string_view file, glm::vec3 origin, glm::vec3 /*direction*/, float /*magnitude*/,
	                       psys::SpellSink* /*sink*/, bool /*synced*/) override
	{
		started.push_back({std::string(file), origin});
		return k_Id;
	}
	bool ProcessForSpell(EffectId id, const psys::ProcessInfo& /*info*/, float seconds) override
	{
		if (id == k_Id)
		{
			steps.push_back(seconds);
		}
		return true;
	}
	void Delete(EffectId id) override { deleted.push_back(id); }
	[[nodiscard]] bool IsRunning(EffectId id) const override { return id == k_Id && effect != nullptr; }
	[[nodiscard]] psys::Effect* Find(EffectId id) override { return id == k_Id ? effect : nullptr; }
};

class MiracleFxSystemTest: public ::testing::Test
{
protected:
	const glm::vec3 k_CameraAt {100.0f, 50.0f, 200.0f};
	const glm::vec3 k_HandAt {10.0f, 20.0f, 30.0f};
	const glm::vec3 k_CastAt {500.0f, 7.0f, 600.0f};

	void SetUp() override
	{
		Locator::camera::emplace(glm::vec3(0.0f)).SetOrigin(k_CameraAt);
		auto& hand =
		    static_cast<test::creature_fakes::FakeHand&>(Locator::handSystem::emplace<test::creature_fakes::FakeHand>());
		hand.matrix = glm::translate(glm::mat4(1.0f), k_HandAt);
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		// no game data: the bands are made without their model
		Locator::resources::reset();
		_audio = &static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());
		_particles = &static_cast<RecordingParticles&>(Locator::particleSystem::emplace<RecordingParticles>());
		_fx.Reset();
	}
	void TearDown() override { _fx.Reset(); }

	/// A seed of the base level whose magic's in-hand effect is the leaves, ready in the hand
	static entt::entity MakeReadySeed()
	{
		auto info = std::make_unique<InfoConstants>();
		info->spellSeed.at(static_cast<size_t>(SpellSeedType::Fire)).magicTypes = {k_Magic, MagicType::None, MagicType::None,
		                                                                           MagicType::None};
		const_cast<GMagicInfo&>(magic::GetMagicInfo(*info, k_Magic)).particleTypeInHand = ParticleType::Leaves;
		Locator::infoConstants::reset(info.release());
		auto& registry = Locator::entitiesRegistry::value();
		const auto seed = registry.Create();
		registry.Assign<ecs::components::SpellSeed>(
		    seed, ecs::components::SpellSeed {.seedType = SpellSeedType::Fire, .ready = true});
		return seed;
	}

	test::RestoreService<Locator::camera> _restoreCamera;
	test::RestoreService<Locator::handSystem> _restoreHand;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::resources> _restoreResources;
	test::RestoreService<Locator::audio> _restoreAudio;
	test::RestoreService<Locator::particleSystem> _restoreParticles;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	RecordingAudio* _audio {nullptr};
	RecordingParticles* _particles {nullptr};
	MiracleFxSystem _fx;
};
} // namespace

TEST_F(MiracleFxSystemTest, AMiracleInTheHandWearsABraceletForEachPowerUpAndTheyGoWhenItLeaves)
{
	_fx.SeedInHand(k_NoSeed, 1, 1);
	EXPECT_EQ(magic::hand_fx::PowerUpLevel(), 2);
	_fx.SeedLeftHand();
	EXPECT_EQ(magic::hand_fx::PowerUpLevel(), 0);
}

TEST_F(MiracleFxSystemTest, APowerUpPlaysTheBandsSoundThenNamesTheLevel)
{
	_fx.SeedInHand(k_NoSeed, 2, 0);
	ASSERT_EQ(_audio->played.size(), 2u);
	EXPECT_EQ(_audio->played[0].sample, k_BandSound);
	EXPECT_EQ(_audio->played[0].bank, audio::SfxBank::InGame);
	// the third level's voice
	EXPECT_EQ(_audio->played[1].sample, 12);
	EXPECT_EQ(_audio->played[1].bank, audio::SfxBank::SpellDialogue);
}

TEST_F(MiracleFxSystemTest, ALevelThatFellIsNamedWithoutTheBandsSound)
{
	_fx.SeedInHand(k_NoSeed, 1, 1);
	_audio->played.clear();
	_fx.SeedInHand(k_NoSeed, 0, 1);
	ASSERT_EQ(_audio->played.size(), 1u);
	EXPECT_EQ(_audio->played[0].sample, 10);
	EXPECT_EQ(_audio->played[0].bank, audio::SfxBank::SpellDialogue);
	EXPECT_EQ(magic::hand_fx::PowerUpLevel(), 1);
}

TEST_F(MiracleFxSystemTest, NoPowerUpHasNoVoice)
{
	_fx.SeedInHand(k_NoSeed, -1, -1);
	ASSERT_EQ(_audio->played.size(), 1u);
	EXPECT_EQ(_audio->played[0].sample, k_BandSound);
	EXPECT_EQ(magic::hand_fx::PowerUpLevel(), 0);
}

TEST_F(MiracleFxSystemTest, ARingStartsHeldAtTheCameraInTheLocalPlayersColour)
{
	_fx.StartTribalPowerRing(k_Tribe);
	const auto runners = magic::hand_fx::GetTribalPowerRunners();
	ASSERT_EQ(runners.size(), 1u);
	EXPECT_TRUE(runners[0]->Held());
	EXPECT_EQ(runners[0]->Position(), k_CameraAt);
	EXPECT_EQ(runners[0]->Colour(), ColourOf(PlayerNames::PLAYER_ONE));
}

TEST_F(MiracleFxSystemTest, ANewRingReplacesTheOldAndStoppingRemovesIt)
{
	_fx.StartTribalPowerRing(k_Tribe);
	_fx.StartTribalPowerRing(Tribe::JAPANESE);
	EXPECT_EQ(magic::hand_fx::GetTribalPowerRunners().size(), 1u);
	_fx.StopTribalPowerRing();
	EXPECT_TRUE(magic::hand_fx::GetTribalPowerRunners().empty());
}

TEST_F(MiracleFxSystemTest, ReleasingTheRingLetsItGoWhereTheHandIs)
{
	_fx.StartTribalPowerRing(k_Tribe);
	_fx.ReleaseTribalPowerRing(k_Tribe, k_HandAt);
	const auto runners = magic::hand_fx::GetTribalPowerRunners();
	ASSERT_EQ(runners.size(), 1u);
	EXPECT_FALSE(runners[0]->Held());
	EXPECT_EQ(runners[0]->Age(), 0.0f);
	EXPECT_EQ(runners[0]->Position(), k_HandAt);
	EXPECT_EQ(runners[0]->Colour(), ColourOf(PlayerNames::PLAYER_ONE));
}

TEST_F(MiracleFxSystemTest, ReleasingWithoutARingRaisesAColumnAtTheHandInTheLocalPlayersColour)
{
	_fx.ReleaseTribalPowerRing(k_Tribe, k_HandAt);
	const auto runners = magic::hand_fx::GetTribalPowerRunners();
	ASSERT_EQ(runners.size(), 1u);
	EXPECT_FALSE(runners[0]->Held());
	EXPECT_EQ(runners[0]->Position(), k_HandAt);
	EXPECT_EQ(runners[0]->Colour(), ColourOf(PlayerNames::PLAYER_ONE));
}

TEST_F(MiracleFxSystemTest, AnotherPlayersCastRaisesAColumnWhereItWasCastInTheCastersColour)
{
	_fx.StartTribalPowerRing(k_Tribe);
	_fx.TribalPowerColumn(k_Tribe, k_CastAt, PlayerNames::PLAYER_TWO);
	const auto runners = magic::hand_fx::GetTribalPowerRunners();
	ASSERT_EQ(runners.size(), 2u);
	// my ring stays round the hand, first
	EXPECT_TRUE(runners[0]->Held());
	EXPECT_FALSE(runners[1]->Held());
	EXPECT_EQ(runners[1]->Position(), k_CastAt);
	EXPECT_EQ(runners[1]->Colour(), ColourOf(PlayerNames::PLAYER_TWO));
}

TEST_F(MiracleFxSystemTest, ALandLoadClearsTheRingTheColumnsAndTheBracelets)
{
	_fx.SeedInHand(k_NoSeed, 0, 0);
	_fx.StartTribalPowerRing(k_Tribe);
	_fx.TribalPowerColumn(k_Tribe, k_CastAt, PlayerNames::PLAYER_TWO);
	_fx.Reset();
	EXPECT_TRUE(magic::hand_fx::GetTribalPowerRunners().empty());
	EXPECT_EQ(magic::hand_fx::PowerUpLevel(), 0);
}

TEST_F(MiracleFxSystemTest, WithoutAnInterfaceNothingIsWritten)
{
	_fx.SetInterface(nullptr);
	_fx.StartTribalPowerRing(k_Tribe);
	const auto runners = magic::hand_fx::GetTribalPowerRunners();
	ASSERT_EQ(runners.size(), 1u);
	EXPECT_TRUE(magic::hand_fx::GetTribalPowerText(*runners[0]).empty());
	EXPECT_EQ(magic::hand_fx::GetTribalPowerTexture(), nullptr);
}

TEST_F(MiracleFxSystemTest, TheSystemHandsOutTheHandsRingsAndColumnsForTheRenderer)
{
	_fx.SetInterface(nullptr);
	_fx.StartTribalPowerRing(k_Tribe);
	_fx.TribalPowerColumn(k_Tribe, k_CastAt, PlayerNames::PLAYER_TWO);
	const auto runners = _fx.GetTribalPowerRunners();
	EXPECT_EQ(runners, magic::hand_fx::GetTribalPowerRunners());
	ASSERT_EQ(runners.size(), 2u);
	EXPECT_TRUE(_fx.GetTribalPowerText(*runners[0]).empty());
	EXPECT_EQ(_fx.GetTextTexture(), nullptr);
}

TEST_F(MiracleFxSystemTest, AMiracleInTheHandStartsItsLevelsEffectAtTheHandAndItGoesWhenTheMiracleLeaves)
{
	_fx.SeedInHand(MakeReadySeed(), 0, -1);
	ASSERT_EQ(_particles->started.size(), 1u);
	EXPECT_EQ(_particles->started[0].file, "SF_Forest");
	EXPECT_EQ(_particles->started[0].origin, k_HandAt);
	_fx.SeedLeftHand();
	ASSERT_EQ(_particles->deleted.size(), 1u);
	EXPECT_EQ(_particles->deleted[0], RecordingParticles::k_Id);
}

TEST_F(MiracleFxSystemTest, TheHandsStepRunsTheInHandEffectByTheFramesGameTime)
{
	// an effect with nothing in it, for the step to reach
	psys::Effect effect(std::make_shared<const psys::File>(), k_HandAt, 1.0f);
	_particles->effect = &effect;
	_fx.SeedInHand(MakeReadySeed(), 0, -1);
	// a quarter of a second: a 250 ms step, which the effect is given in seconds
	_fx.UpdateHand(0.25f);
	ASSERT_EQ(_particles->steps.size(), 1u);
	EXPECT_FLOAT_EQ(_particles->steps[0], 0.25f);
	_fx.Reset();
	_particles->effect = nullptr;
}

TEST_F(MiracleFxSystemTest, TheHandsStepAgesTheRingByTheFramesGameSeconds)
{
	_fx.StartTribalPowerRing(k_Tribe);
	_fx.UpdateHand(0.25f);
	const auto runners = magic::hand_fx::GetTribalPowerRunners();
	ASSERT_EQ(runners.size(), 1u);
	EXPECT_FLOAT_EQ(runners[0]->Age(), 0.25f);
}

TEST_F(MiracleFxSystemTest, TheGlobesStepTheirFrameByTheFramesGameTime)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto orb = registry.Create();
	registry.Assign<ecs::components::OneOffSpellSeed>(orb);
	registry.Assign<ecs::components::UvScroll>(orb);
	// half a second at 18 frames a second: frame 9 of the 4 x 4 sheet, the second column of the third row
	_fx.UpdateGlobes(0.5f);
	EXPECT_FLOAT_EQ(registry.Get<ecs::components::OneOffSpellSeed>(orb).phase, 9.0f);
	EXPECT_EQ(registry.Get<ecs::components::UvScroll>(orb).u, 0.25f);
	EXPECT_EQ(registry.Get<ecs::components::UvScroll>(orb).v, 0.5f);
}

TEST_F(MiracleFxSystemTest, ThePilesSinkByTheFramesSeconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto pile = registry.Create();
	registry.Assign<ecs::components::Transform>(pile, glm::vec3(0.0f, 10.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& sink = registry.Assign<ecs::components::PileSink>(pile, ecs::components::PileSink {.baseY = 10.0f});
	// two down in one second
	sink.offset.SetDestinationWithSpeedAndTime(-2.0f, 0.0f, 1.0f);
	_fx.UpdatePiles(0.25f);
	const auto& stepped = registry.Get<ecs::components::PileSink>(pile).offset;
	EXPECT_FLOAT_EQ(stepped.time, 0.25f);
	// a quarter of the way in time, part of the way down
	EXPECT_LT(stepped.value, 0.0f);
	EXPECT_GT(stepped.value, -2.0f);
	EXPECT_EQ(registry.Get<ecs::components::Transform>(pile).position.y, 10.0f + stepped.value);
}

TEST_F(MiracleFxSystemTest, AMiracleShakenOffPlaysTheShakeSound)
{
	_fx.SeedShakenOff();
	ASSERT_EQ(_audio->withOptions.size(), 1u);
	EXPECT_EQ(_audio->withOptions[0].number, k_ShakeSound);
	EXPECT_EQ(_audio->withOptions[0].bank, audio::Bank(audio::SfxBank::InGame));
	EXPECT_TRUE(_audio->played.empty());
}
