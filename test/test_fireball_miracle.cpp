/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fireball miracle, pinned to the original: a ball starts as hot as its miracle is strong and holds heat by its size
// and by the strength its miracle has now; every step its atom sends a point event at itself; a ball cast by no human is
// lobbed on a ballistic arc from the gesture point, whatever the hand does; a ball almost still on the land is dragged
// along a fixed direction; and the nearest burning thing holds the fires' crackle. The values are the wiki's
// (docs/wiki/magic/fireball-and-lightning.md#fireball-magic_type-1-3-seed-2-fire, docs/wiki/world/fire.md#object-values,
// docs/wiki/world/fire.md#the-fires-sound, docs/wiki/physics/particle-rules.md#fireball-rules).

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/Audio.h"
#include "Audio/AudioManagerNoOp.h"
#include "Camera/Camera.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/Transform.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Fire/FireSound.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/Implementations/FireSystem.h"
#include "ECS/Systems/Implementations/ObjectCreationIndexSystem.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Objects/MagicFireBall.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/SpellLink.h"
#include "support/FireFakes.h"
#include "support/MagicFakes.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
std::shared_ptr<const particles::File> Parse(const std::string& text)
{
	auto file = particles::File::Parse(text, "test");
	return file.has_value() ? std::make_shared<const particles::File>(std::move(*file)) : nullptr;
}

/// A spell file's header: group 0 is made when the effect starts, and no group is a hierarchy
const std::string k_Header = "BEGINPROPERTIES\n"
                             "PROPERTY DeleteOnCloseDown BOOL 0\n"
                             "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                             "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                             "ENDPROPERTIES\n";

const std::string k_Point = "BEGINCLASS ParticlePointCreator P0\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\n"
                            "ENDPROPERTIES\nENDCLASS\n";

/// One atom made once at the effect's origin, at rest
const std::string k_AnAtom = "BEGINCLASS CreateRuleAnAtom C0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
                             "PROPERTY PCreator PERSIS_PNTR P0\nPROPERTY NextGroups ARRAY SIZE 0\nENDPROPERTIES\nENDCLASS\n";

/// The one atom an effect has after its steps; null when it has none or more than one
const particles::Atom* TheAtom(const particles::Effect& effect)
{
	std::vector<particles::Effect::DrawAtom> atoms;
	effect.Collect(1.0f, atoms, particles::Creator::Kind::Point);
	return atoms.size() == 1 ? atoms[0].atom : nullptr;
}

/// The particles with one running effect, a fireball's: Find gives it for its id and nothing for any other
class OneFireballEffect final: public test::InertParticleSystem
{
public:
	static constexpr EffectId k_Id = 7;

	[[nodiscard]] particles::Effect* Find(EffectId id) override { return id == k_Id ? &_effect : nullptr; }

	/// The strength the effect's miracle has now
	void SetStrength(float strength)
	{
		auto info = _effect.GetProcessInfo();
		info.power = strength;
		_effect.SetProcessInfo(info);
	}

private:
	particles::Effect _effect {Parse(k_Header + k_Point), glm::vec3(0.0f), 1.0f};
};

/// A registry, the creation counter, the miracles' stores (with their fireball list), fires that only keep their
/// temperature, one fireball effect and a table of constants with info.dat's three fireball rows, each put back as it
/// was when the test ends
class FireballMiracleHeat: public ::testing::Test
{
protected:
	/// info.dat's starting temperatures of the three rows; the plain ball takes row 1, the power-ups row 0
	static constexpr std::array<float, 3> k_InitialTemperatures {6000.0f, 10000.0f, 14000.0f};
	/// Stand-ins: the ball must be able to burn, and its heat capacity is scaled from this one
	static constexpr float k_CombustionTemperature = 100.0f;
	static constexpr float k_HeatCapacity = 400.0f;

	FireballMiracleHeat()
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::objectCreationIndexSystem::emplace<ecs::systems::ObjectCreationIndexSystem>();
		Locator::magicSystem::emplace<test::InertMagicSystem>();
		_fires = &static_cast<test::FakeFires&>(Locator::fireSystem::emplace<test::FakeFires>());
		_particles = &static_cast<OneFireballEffect&>(Locator::particleSystem::emplace<OneFireballEffect>());
		auto info = std::make_unique<InfoConstants>();
		for (size_t row = 0; row < k_InitialTemperatures.size(); ++row)
		{
			auto& ball = info->magicFireBall.at(row);
			ball.initialTemperature = k_InitialTemperatures.at(row);
			ball.deletionTemperature = 400.0f;
			ball.catchIncreaseFactor = 0.05f;
			ball.combustionTemperature = k_CombustionTemperature;
			ball.heatCapacity = k_HeatCapacity;
		}
		Locator::infoConstants::reset(info.release());
	}

	[[nodiscard]] static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] test::FakeFires& Fires() const { return *_fires; }
	[[nodiscard]] OneFireballEffect& Particles() const { return *_particles; }

	/// A ball of that info row whose atom is in that effect, its scale (and so its radius) `scale`
	static entt::entity Ball(int row, uint32_t effect, float scale)
	{
		const auto ball = Reg().Create();
		Reg().Assign<ecs::components::Transform>(ball, glm::vec3(10.0f, 2.0f, 20.0f), glm::mat3(1.0f), glm::vec3(scale));
		auto& component = Reg().Assign<ecs::components::MagicFireBall>(ball);
		component.infoRow = row;
		component.effect = effect;
		return ball;
	}

private:
	// put back in reverse order: the tables, the particles, the fires, the miracles' service with its stores, the
	// counter, then the registry
	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	const test::RestoreService<Locator::objectCreationIndexSystem> _restoreIndex;
	const test::RestoreService<Locator::magicSystem> _restoreMagic;
	const test::RestoreService<Locator::fireSystem> _restoreFires;
	const test::RestoreService<Locator::particleSystem> _restoreParticles;
	const test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::FakeFires* _fires {nullptr};
	OneFireballEffect* _particles {nullptr};
};

/// The spell behind an effect, as its rules see it: it keeps every event it is sent
class RecordingSink final: public particles::SpellSink
{
public:
	int SpellEvent(const particles::SpellEventInfo& event) override
	{
		events.push_back(event);
		return 1;
	}
	[[nodiscard]] int PowerUpLevel() const override { return -1; }

	std::vector<particles::SpellEventInfo> events;
};

/// The velocity of the one ball a throw rule makes in its first step, for a cast by no human: from `gesture`, in an
/// effect at `origin` with that hand movement, and no gravity rule in the ball's group
glm::vec3 LobVelocity(glm::vec3 gesture, glm::vec3 origin, glm::vec3 handMovement)
{
	const auto file = Parse(k_Header + k_Point +
	                        "BEGINCLASS CreateWithInitialDirection T0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                        "PROPERTY PCreator PERSIS_PNTR P0\nPROPERTY NextGroups ARRAY SIZE 0\n"
	                        "PROPERTY NumAtoms INTEGER 1\nENDPROPERTIES\nENDCLASS\n");
	if (file == nullptr)
	{
		ADD_FAILURE() << "the throw's file does not parse";
		return glm::vec3(0.0f);
	}
	particles::Effect effect(file, origin, 1.0f);
	effect.SetProcessInfo(particles::ProcessInfo {.handPos = gesture, .direction = handMovement});
	effect.SetDirection(handMovement);
	effect.Step(0.1f);
	const auto* atom = TheAtom(effect);
	if (atom == nullptr)
	{
		ADD_FAILURE() << "the throw made no ball";
		return glm::vec3(0.0f);
	}
	// it leaves from the gesture point
	EXPECT_EQ(atom->position, gesture);
	return atom->velocity;
}

/// The velocity of one atom made at rest on the land of no island (flat at height 0, facing up) after two steps of a
/// gravity with floor rule (gravity 10, both bounce dampings 0.5, no wind) with that GroundDrag property: the first step
/// it falls, the second it lands
glm::vec3 LandedVelocity(const std::string& groundDrag)
{
	const auto file = Parse(k_Header + k_Point + k_AnAtom +
	                        "BEGINCLASS UpdateRuleGravityWithFloor G0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                        "PROPERTY Gravity FLOAT 10\nPROPERTY MaxSpeed FLOAT 100\nPROPERTY UseWind BOOL 0\n"
	                        "PROPERTY DampingHorozontalBounce FLOAT 0.5\nPROPERTY DampingVerticalBounce FLOAT 0.5\n"
	                        "PROPERTY GroundDrag FLOAT " +
	                        groundDrag + "\nENDPROPERTIES\nENDCLASS\n");
	if (file == nullptr)
	{
		ADD_FAILURE() << "the bounce's file does not parse";
		return glm::vec3(0.0f);
	}
	particles::Effect effect(file, glm::vec3(0.0f), 1.0f);
	effect.Step(0.1f);
	const auto* atom = TheAtom(effect);
	if (atom == nullptr)
	{
		ADD_FAILURE() << "no atom";
		return glm::vec3(0.0f);
	}
	// in the air for one step: gravity 10 for 0.1 s
	EXPECT_FLOAT_EQ(atom->velocity.y, -1.0f);
	effect.Step(0.1f);
	atom = TheAtom(effect);
	if (atom == nullptr)
	{
		ADD_FAILURE() << "the atom went";
		return glm::vec3(0.0f);
	}
	return atom->velocity;
}

/// The game's fires, real, over a registry of their own, and a camera 30 m above the origin: fires are put by hand on
/// objects that far from it along x, each put back as it was when the test ends
class FireballMiracleCrackle: public ::testing::Test
{
protected:
	FireballMiracleCrackle()
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_fires = &Locator::fireSystem::emplace<ecs::systems::FireSystem>();
		Locator::camera::emplace(glm::vec3(0.0f)).SetOrigin(glm::vec3(0.0f, 30.0f, 0.0f));
	}

	/// A burning object `distance` metres from the camera, on the land
	ecs::fire::FireEffect& Burning(float distance)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto object = registry.Create();
		registry.Assign<ecs::components::Transform>(object, glm::vec3(distance, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		auto made = std::make_unique<ecs::fire::FireEffect>();
		made->id = _fires->TakeId();
		made->object = object;
		made->temperature = 1000.0f;
		return _fires->Insert(std::move(made));
	}

	/// One turn of the fires' sound: the slots' distances again, every fire loud enough to be heard in turn, then the
	/// slots' fires play
	static void Turn(const std::vector<ecs::fire::FireEffect*>& loud)
	{
		ecs::fire::sound::RefreshDistances();
		for (auto* fire : loud)
		{
			ecs::fire::sound::Consider(*fire, true);
		}
		ecs::fire::sound::StartSlots();
	}

	[[nodiscard]] static bool Crackles(const ecs::fire::FireEffect& fire)
	{
		return (fire.flags & ecs::fire::FireEffect::k_SoundPlaying) != 0;
	}

private:
	// put back in reverse order: the registry first (as the game's shutdown does), the fires, then the camera
	const test::RestoreService<Locator::camera> _restoreCamera;
	const test::RestoreService<Locator::fireSystem> _restoreFires;
	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	ecs::systems::FireSystemInterface* _fires {nullptr};
};

/// The audio as a ball's fly-by reaches it: it keeps every plain sound effect it is asked for, and its clock reads 12
class FlyByAudio final: public audio::AudioManagerNoOp
{
public:
	using audio::AudioManagerNoOp::PlaySoundEffect;

	/// One plain sound effect asked for
	struct Played
	{
		audio::Owner owner;
		int sample;
		int mode;
		int loops;
		bool extra3DFlag;
		bool is3D;
		audio::SfxBank bank;
	};

	audio::Channel PlaySoundEffect(audio::Owner owner, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
	                               audio::SfxBank bank) override
	{
		played.push_back({owner, sample, mode, loops, extra3DFlag, is3D, bank});
		return audio::k_NoChannel;
	}
	[[nodiscard]] uint32_t TickCount() override { return 12; }

	std::vector<Played> played;
};

/// The heat tests' services, with the audio above and a camera at (0, 10, 0)
class FireballMiracleFlyBy: public FireballMiracleHeat
{
protected:
	FireballMiracleFlyBy()
	{
		_audio = &static_cast<FlyByAudio&>(Locator::audio::emplace<FlyByAudio>());
		Locator::camera::emplace(glm::vec3(0.0f)).SetOrigin(glm::vec3(0.0f, 10.0f, 0.0f));
	}

	[[nodiscard]] FlyByAudio& Audio() const { return *_audio; }

	/// A ball thrown by no human from `gesture` at the height of the camera towards a point `distance` metres further
	/// along -x, at the speed that throw gives (no gravity: it keeps it), stepped 30 times: the steps at which a fly-by
	/// played. With `ballGoesAt`, its ball goes before that step
	std::vector<int> FlyBySteps(glm::vec3 gesture, float distance, std::optional<int> ballGoesAt = std::nullopt)
	{
		const auto file = Parse(k_Header + k_Point +
		                        "BEGINCLASS CreateWithInitialDirection T0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
		                        "PROPERTY PCreator PERSIS_PNTR P0\nPROPERTY NextGroups ARRAY SIZE 0\n"
		                        "PROPERTY NumAtoms INTEGER 1\nENDPROPERTIES\nENDCLASS\n"
		                        "BEGINCLASS UpdateRuleGravityWithFloor G0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
		                        "PROPERTY Gravity FLOAT 0\nPROPERTY UseWind BOOL 0\nENDPROPERTIES\nENDCLASS\n"
		                        "BEGINCLASS AttatchFireBallToAtom A0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
		                        "ENDPROPERTIES\nENDCLASS\n");
		if (file == nullptr)
		{
			ADD_FAILURE() << "the fly-by's file does not parse";
			return {};
		}
		// the throw's target is 0.8 of the way to the effect's origin
		particles::Effect effect(file, gesture - glm::vec3(distance / 0.8f, 0.0f, 0.0f), 1.0f);
		effect.SetProcessInfo(particles::ProcessInfo {.handPos = gesture});
		std::vector<int> steps;
		for (int step = 1; step <= 30; ++step)
		{
			if (ballGoesAt == step && !magic::fireball::All().empty())
			{
				Reg().Destroy(magic::fireball::All().front());
			}
			const auto before = Audio().played.size();
			effect.Step(0.1f);
			if (Audio().played.size() != before)
			{
				steps.push_back(step);
			}
		}
		return steps;
	}

private:
	// put back in reverse order: the camera, then the audio
	const test::RestoreService<Locator::audio> _restoreAudio;
	const test::RestoreService<Locator::camera> _restoreCamera;
	FlyByAudio* _audio {nullptr};
};
} // namespace

TEST_F(FireballMiracleHeat, ABallStartsAsHotAsItsMiracleIsStrong)
{
	using magic::fireball::Create;
	using magic::fireball::InfoRowFor;
	const glm::vec3 at(10.0f, 2.0f, 20.0f);
	constexpr auto k_Effect = OneFireballEffect::k_Id;
	// at full strength the plain ball (level 0) starts at its row's 10000 degrees, a power-up's ball at 6000
	const auto plain = Create(at, InfoRowFor(0), k_Effect, 0, PlayerNames::PLAYER_ONE, false, entt::null);
	const auto powerUp = Create(at, InfoRowFor(1), k_Effect, 1, PlayerNames::PLAYER_ONE, false, entt::null);
	EXPECT_FLOAT_EQ(Fires().GetTemperature(plain), 10000.0f);
	EXPECT_FLOAT_EQ(Fires().GetTemperature(powerUp), 6000.0f);
	// the fire is the ball's player's
	ASSERT_NE(Fires().FindByObject(plain), nullptr);
	EXPECT_EQ(Fires().FindByObject(plain)->player, std::optional(PlayerNames::PLAYER_ONE));
	// a miracle at a quarter of its strength makes its new balls a quarter as hot
	Particles().SetStrength(0.25f);
	const auto weak = Create(at, InfoRowFor(0), k_Effect, 2, PlayerNames::PLAYER_ONE, false, entt::null);
	const auto weakPowerUp = Create(at, InfoRowFor(2), k_Effect, 3, PlayerNames::PLAYER_ONE, false, entt::null);
	EXPECT_FLOAT_EQ(Fires().GetTemperature(weak), 2500.0f);
	EXPECT_FLOAT_EQ(Fires().GetTemperature(weakPowerUp), 1500.0f);
	// only the start: the balls already burning keep their heat
	EXPECT_FLOAT_EQ(Fires().GetTemperature(plain), 10000.0f);
}

TEST_F(FireballMiracleHeat, ABallHoldsHeatByItsSizeAndTheStrengthItsMiracleHasNow)
{
	// the square of its radius (its scale) x 0.0625 x its row's heat capacity x its miracle's strength now
	const auto ball = Ball(0, OneFireballEffect::k_Id, 2.0f);
	EXPECT_FLOAT_EQ(ecs::fire::traits::HeatCapacity(ball), 2.0f * 2.0f * 0.0625f * k_HeatCapacity);
	Particles().SetStrength(0.5f);
	EXPECT_FLOAT_EQ(ecs::fire::traits::HeatCapacity(ball), 50.0f);
	Particles().SetStrength(0.125f);
	EXPECT_FLOAT_EQ(ecs::fire::traits::HeatCapacity(ball), 12.5f);
	// a ball with no effect behind it counts at full strength
	const auto orphan = Ball(1, 99, 4.0f);
	EXPECT_FLOAT_EQ(ecs::fire::traits::HeatCapacity(orphan), 4.0f * 4.0f * 0.0625f * k_HeatCapacity);
}

TEST(FireballMiracleRules, EveryStepABallSendsAPointEventAtItsAtom)
{
	// one atom falling (a gravity with floor rule, no wind), then the event rule in the same group
	const auto file = Parse(k_Header + k_Point + k_AnAtom +
	                        "BEGINCLASS UpdateRuleGravityWithFloor G0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
	                        "PROPERTY Gravity FLOAT 10\nPROPERTY UseWind BOOL 0\nENDPROPERTIES\nENDCLASS\n"
	                        "BEGINCLASS EventAlways E0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\nENDPROPERTIES\n"
	                        "ENDCLASS\n");
	ASSERT_NE(file, nullptr);
	RecordingSink sink; // it outlives the effect
	particles::Effect effect(file, glm::vec3(10.0f, 50.0f, 20.0f), 1.0f);
	effect.SetSink(&sink);
	sink.events.clear(); // the effect's start
	for (size_t step = 1; step <= 4; ++step)
	{
		effect.Step(0.1f);
		const auto* atom = TheAtom(effect);
		ASSERT_NE(atom, nullptr);
		// one event a step, a point event where the atom is, at strength 1 and with no shield check
		ASSERT_EQ(sink.events.size(), step);
		const auto& event = sink.events.back();
		EXPECT_EQ(event.type, particles::SpellEventInfo::Type::Point);
		EXPECT_EQ(event.position, effect.GlobalPosition(*atom));
		EXPECT_FLOAT_EQ(event.strength, 1.0f);
		EXPECT_FALSE(event.checkShields);
	}
	// the ball fell, and its events with it
	EXPECT_LT(sink.events.back().position.y, sink.events.front().position.y);
}

TEST(FireballMiracleRules, ABallCastByNoHumanIsLobbedFromTheGesturePointToEightTenthsOfTheWayToTheOrigin)
{
	// the target is 0.8 of the way from the gesture point to the origin, reached in max(0.025 x its distance, 0.5) s
	// under the gravity 30 (no gravity rule): v = delta / t - (0, -30 x 0.5 t, 0). From 30 m up to an origin 100 m off:
	// delta (80, -24, 0), t 2 s, v (40, 18, 0), below 30 degrees so it stays
	const glm::vec3 gesture(0.0f, 30.0f, 0.0f);
	const glm::vec3 origin(100.0f, 0.0f, 0.0f);
	const auto lob = LobVelocity(gesture, origin, glm::vec3(0.0f));
	EXPECT_NEAR(lob.x, 40.0f, 1e-4f);
	EXPECT_NEAR(lob.y, 18.0f, 1e-4f);
	EXPECT_NEAR(lob.z, 0.0f, 1e-4f);
	// no swing: the hand's movement changes nothing
	EXPECT_EQ(LobVelocity(gesture, origin, glm::vec3(0.0f, 0.0f, 50.0f)), lob);
	EXPECT_EQ(LobVelocity(gesture, origin, glm::vec3(-30.0f, 20.0f, 5.0f)), lob);
}

TEST(FireballMiracleRules, AShortLobSteeperThanThirtyDegreesIsThrownAtThirtyDegrees)
{
	// from the ground to an origin 4 m off: delta (3.2, 0, 0), first v (6.4, 7.5, 0), steeper than the tangent of the
	// double 0.52370351552963257; the new time is sqrt((delta.y - distance x that tangent) x (-2 / g))
	const double slope = std::tan(0.52370351552963257);
	const double time = std::sqrt((0.0 - 3.2 * slope) * (-2.0 / 30.0));
	const auto lob = LobVelocity(glm::vec3(0.0f), glm::vec3(4.0f, 0.0f, 0.0f), glm::vec3(0.0f));
	EXPECT_NEAR(lob.x, 3.2 / time, 1e-4);
	EXPECT_NEAR(lob.y, 30.0 * 0.5 * time, 1e-4);
	EXPECT_NEAR(lob.z, 0.0f, 1e-4f);
	EXPECT_NEAR(static_cast<double>(lob.y) / static_cast<double>(lob.x), slope, 1e-5);
}

TEST(FireballMiracleRules, AnOriginRightAboveTheGesturePointIsMovedToItsSideAtItsHeight)
{
	// x^2 + z^2 under 0.1 from the gesture point: the origin becomes the gesture point + (0.1, 0, 0.1), so the target is
	// (0.08, 0, 0.08) away and level with it (the origin's own height is not used); that lob is steep, so it goes at 30
	// degrees
	const glm::vec3 gesture(5.0f, 10.0f, 5.0f);
	const auto lob = LobVelocity(gesture, glm::vec3(5.1f, 0.0f, 5.1f), glm::vec3(0.0f));
	const double slope = std::tan(0.52370351552963257);
	const double distance = std::sqrt(0.08 * 0.08 * 2.0);
	const double time = std::sqrt((0.0 - distance * slope) * (-2.0 / 30.0));
	EXPECT_NEAR(lob.x, 0.08 / time, 1e-4);
	EXPECT_NEAR(lob.y, 30.0 * 0.5 * time, 1e-4);
	EXPECT_NEAR(lob.z, 0.08 / time, 1e-4);
}

TEST(FireballMiracleRules, AnAlmostStillBallIsDraggedAlongTheFixedFallbackDirection)
{
	// landing straight down, its sliding part is 0: under 1e-4 squared the drag takes the fallback direction (1e-4, 0,
	// 0), so the ground drag (dt x GroundDrag, at most that direction's length 1e-4) pushes it towards -x, then the
	// horizontal bounce 0.5; the vertical part bounces back by the vertical 0.5
	const auto dragged = LandedVelocity("1");
	EXPECT_FLOAT_EQ(dragged.x, -0.0001f * 0.5f);
	EXPECT_FLOAT_EQ(dragged.y, 0.5f);
	EXPECT_FLOAT_EQ(dragged.z, 0.0f);
	// with no ground drag nothing pushes it
	const auto still = LandedVelocity("0");
	EXPECT_FLOAT_EQ(still.x, 0.0f);
	EXPECT_FLOAT_EQ(still.y, 0.5f);
	EXPECT_FLOAT_EQ(still.z, 0.0f);
}

TEST_F(FireballMiracleCrackle, TheNearestBurningThingHoldsTheCrackleLoop)
{
	// a fire nearer than the farthest slot (or with no slot taken) takes the first slot that is empty or not nearer than
	// that, so each nearer fire takes the loop from the one before it: the nearest holds it
	auto& ten = Burning(10.0f);
	auto& thirty = Burning(30.0f);
	auto& twenty = Burning(20.0f);
	auto& five = Burning(5.0f);
	const std::vector<ecs::fire::FireEffect*> loud {&ten, &thirty, &twenty, &five};
	Turn(loud);
	EXPECT_TRUE(Crackles(five));
	EXPECT_FALSE(Crackles(ten));
	EXPECT_FALSE(Crackles(twenty));
	EXPECT_FALSE(Crackles(thirty));
	// the next turn it keeps it
	Turn(loud);
	EXPECT_TRUE(Crackles(five));
	EXPECT_FALSE(Crackles(ten));
	// a nearer one takes the loop over, and the one before stops
	auto& two = Burning(2.0f);
	Turn({&ten, &thirty, &twenty, &five, &two});
	EXPECT_TRUE(Crackles(two));
	EXPECT_FALSE(Crackles(five));
	// once it burns too low to be heard it gives the loop back, and the nearest loud one takes it again
	ecs::fire::sound::RefreshDistances();
	ecs::fire::sound::Consider(two, false);
	EXPECT_FALSE(Crackles(two));
	for (auto* fire : loud)
	{
		ecs::fire::sound::Consider(*fire, true);
	}
	ecs::fire::sound::StartSlots();
	EXPECT_TRUE(Crackles(five));
	EXPECT_FALSE(Crackles(ten));
}

TEST_F(FireballMiracleFlyBy, ABallPassingFastWithinFortyMetresOfTheCameraWhooshesOnce)
{
	// from 101 m off at 40 m/s (4 m a step): its drawn position comes within 40 m between the 16th and 17th steps, and
	// it is faster than 20, so the 17th step plays one fly-by, not again while it stays near
	EXPECT_EQ(FlyBySteps(glm::vec3(101.0f, 10.0f, 0.0f), 200.0f), std::vector<int> {17});
	ASSERT_EQ(Audio().played.size(), 1u);
	const auto& played = Audio().played.front();
	// one of the five fly-by sounds by the clock (12 % 5 = 2), a plain 2D sound with no owner, played once
	EXPECT_EQ(played.sample, 0x40 + 2);
	EXPECT_EQ(played.mode, 2);
	EXPECT_EQ(played.loops, 0);
	EXPECT_FALSE(played.extra3DFlag);
	EXPECT_FALSE(played.is3D);
	EXPECT_EQ(played.bank, audio::SfxBank::InGame);
	EXPECT_EQ(played.owner, audio::Owner::None());
}

TEST_F(FireballMiracleFlyBy, ASlowBallMakesNoFlyBy)
{
	// a short throw goes at 16 m/s: it comes within 40 m all the same, but no faster than 20
	EXPECT_TRUE(FlyBySteps(glm::vec3(42.1f, 10.0f, 0.0f), 8.0f).empty());
}

TEST_F(FireballMiracleFlyBy, AnAtomWhoseBallHasGoneMakesNoFlyBy)
{
	// the fast throw above, its ball gone before it comes near
	EXPECT_TRUE(FlyBySteps(glm::vec3(101.0f, 10.0f, 0.0f), 200.0f, 10).empty());
}
