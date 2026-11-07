/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A creature's creed power: the glows' rules (a place for each script number, a power of 0 removing a glow, the hum
// starting and stopping only as the glows gain or lose all power), the creature animation system keeping the glows on
// the creature with a fake of the sound tags that records the hum, SET_CREATURE_CREED_PROPERTIES as a script calls it,
// with a fake of the animation system that records each power given, and where each glow is drawn on a made-up body

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

#include <LHVM.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Audio/Game/BankTables.h"
#include "CHLApi.h"
#include "Creature/CreatureHandGlow.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/Implementations/CreatureAnimationSystem.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "Graphics/GlowShellLook.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::components;
using creature_hand_glow::Hum;
using creature_hand_glow::Place;

namespace
{
/// The sound tags: each thing's tag made and each removal is kept; everything else does nothing
class RecordingSoundTags final: public ecs::systems::SoundTagSystemInterface
{
public:
	struct Made
	{
		entt::entity thing;
		int sample;
		bool track;
		int mode;
		int loops;
		bool extra3DFlag;
		bool is3D;
		audio::SfxBank bank;
		int delay;
		bool operator==(const Made&) const = default;
	};
	struct Removed
	{
		entt::entity thing;
		int sample;
		audio::SfxBank bank;
		bool stop;
		bool operator==(const Removed&) const = default;
	};
	std::vector<Made> made;
	std::vector<Removed> removed;

	void ProcessTurn() override {}
	void SetActive(TagId, bool) override {}
	TagId CreatePointSound(const glm::vec3&, int, int, int, bool, bool, audio::SfxBank, int) override { return 0; }
	TagId CreatePointSound(const glm::vec3&, int, int, int, bool, bool, audio::AtmosType, int) override { return 0; }
	TagId Create(entt::entity thing, const glm::vec3&, int sample, bool track, int mode, int loops, bool extra3DFlag, bool is3D,
	             audio::SfxBank bank, int delay) override
	{
		made.push_back({.thing = thing,
		                .sample = sample,
		                .track = track,
		                .mode = mode,
		                .loops = loops,
		                .extra3DFlag = extra3DFlag,
		                .is3D = is3D,
		                .bank = bank,
		                .delay = delay});
		return static_cast<TagId>(made.size());
	}
	TagId CreateAtMarker(const glm::vec3&, int, int, int, bool, audio::SfxBank) override { return 0; }
	void Remove(entt::entity thing, int sample, audio::SfxBank bank, bool stop) override
	{
		removed.push_back({.thing = thing, .sample = sample, .bank = bank, .stop = stop});
	}
	void Delete(TagId) override {}
	[[nodiscard]] bool Exists(TagId) const override { return false; }
	void Clear() override {}
	[[nodiscard]] std::optional<glm::vec3> TagSoundPoint(TagId) const override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> Point(TagId) const override { return std::nullopt; }
};

/// The creature animation system as far as the native reaches it: each creed power given is kept
class RecordingAnimation final: public ecs::systems::CreatureAnimationSystemInterface
{
public:
	struct Power
	{
		entt::entity creature;
		Place place;
		float scale;
		float power;
		bool operator==(const Power&) const = default;
	};
	std::vector<Power> powers;

	void ProcessTurn() override {}
	void Update(float, std::chrono::duration<float, std::milli>) override {}
	TurnMoments AdvanceTurn() override { return {}; }
	void PoseTurn(const TurnInputs&) override {}
	[[nodiscard]] std::optional<glm::vec3> BoneInAnimation(entt::entity, size_t, float, uint32_t, bool) override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<float> AnimationDuration(entt::entity, size_t) override { return std::nullopt; }
	bool EnsurePosed(entt::entity) override { return false; }
	void UpdateCopies(uint32_t) override {}
	[[nodiscard]] bool AreSparklesShown() const override { return true; }
	void SetSparklesShown(bool) override {}
	void SetCreedPower(entt::entity creature, Place place, float scale, float power) override
	{
		powers.push_back({.creature = creature, .place = place, .scale = scale, .power = power});
	}
};

/// The sample the creature hums its creed power with, in the game's sound effect bank
constexpr int k_CreedHum = 166;

class CreatureCreedPowerSystem: public ::testing::Test
{
protected:
	CreatureCreedPowerSystem()
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_sounds = &static_cast<RecordingSoundTags&>(Locator::soundTagSystem::emplace<RecordingSoundTags>());
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity MakeCreature()
	{
		const auto creature = Reg().Create();
		Reg().Assign<Creature>(creature);
		return creature;
	}

	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	const test::RestoreService<Locator::soundTagSystem> _restoreSounds;
	RecordingSoundTags* _sounds {nullptr};
	ecs::systems::CreatureAnimationSystem _system;
};

class CreatureCreedPowerNative: public ::testing::Test
{
protected:
	CreatureCreedPowerNative()
	    : _vm(&Locator::vm::emplace<lhvm::LHVM>())
	{
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_animation = &static_cast<RecordingAnimation&>(Locator::creatureAnimationSystem::emplace<RecordingAnimation>());
		// a script's object 0 is no thing, so the first entity is not one the tests name
		Reg().Create();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// SET_CREATURE_CREED_PROPERTIES as a script calls it: the creature, the place, the scale, the power, then the time
	/// on top, over a value of the script's own that it must leave
	void SetCreedProperties(uint32_t object, int32_t place, float scale, float power, float time = 0.0f)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "SET_CREATURE_CREED_PROPERTIES"; });
		ASSERT_NE(native, table.end());
		EXPECT_EQ(native->stackIn, 5);
		EXPECT_EQ(native->stackOut, 0u);
		_vm->Pushi(k_ScriptsOwn);
		_vm->Pusho(object);
		_vm->Pushi(place);
		_vm->Pushf(scale);
		_vm->Pushf(power);
		_vm->Pushf(time);
		native->impl();
		EXPECT_EQ(_vm->Pop().intVal, k_ScriptsOwn);
	}

	static constexpr int32_t k_ScriptsOwn = 7777;
	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	const test::RestoreService<Locator::vm> _restoreVm;
	const test::RestoreService<Locator::creatureAnimationSystem> _restoreAnimation;
	lhvm::LHVM* _vm;
	RecordingAnimation* _animation {nullptr};
	chlapi::CHLApi _api;
};
} // namespace

TEST(CreatureCreedPower, AScriptNumberNamesAPlaceOnlyWhenItIsInRange)
{
	EXPECT_EQ(creature_hand_glow::PlaceOf(0), Place::LeftHand);
	EXPECT_EQ(creature_hand_glow::PlaceOf(1), Place::Belly);
	EXPECT_EQ(creature_hand_glow::PlaceOf(2), Place::RightHand);
	EXPECT_EQ(creature_hand_glow::PlaceOf(3), Place::Circling);
	EXPECT_FALSE(creature_hand_glow::PlaceOf(-1).has_value());
	EXPECT_FALSE(creature_hand_glow::PlaceOf(4).has_value());
}

TEST(CreatureCreedPower, APowerOfZeroOrNotANumberRemovesTheGlow)
{
	creature_hand_glow::Glows glows;
	creature_hand_glow::Set(glows, Place::Circling, 5.0f, 0.1f);
	ASSERT_TRUE(glows.at(3).has_value());
	EXPECT_EQ(*glows.at(3), (creature_hand_glow::Glow {.power = 0.1f, .scale = 5.0f}));
	creature_hand_glow::Set(glows, Place::Circling, 5.0f, 0.0f);
	EXPECT_FALSE(glows.at(3).has_value());
	creature_hand_glow::Set(glows, Place::Circling, 5.0f, 1.0f);
	creature_hand_glow::Set(glows, Place::Circling, 5.0f, std::numeric_limits<float>::quiet_NaN());
	EXPECT_FALSE(glows.at(3).has_value());
}

TEST(CreatureCreedPower, TheHumStartsAndStopsOnlyWhenTheGlowsGainOrLoseAllPower)
{
	creature_hand_glow::Glows glows;
	EXPECT_FALSE(creature_hand_glow::AnyActive(glows));
	EXPECT_EQ(creature_hand_glow::SetCreedPower(glows, Place::LeftHand, 1.0f, 0.5f), Hum::Start);
	EXPECT_EQ(creature_hand_glow::SetCreedPower(glows, Place::RightHand, 2.0f, 1.0f), Hum::Unchanged);
	EXPECT_EQ(creature_hand_glow::SetCreedPower(glows, Place::LeftHand, 1.0f, 0.0f), Hum::Unchanged);
	EXPECT_EQ(creature_hand_glow::SetCreedPower(glows, Place::RightHand, 1.0f, 0.0f), Hum::Stop);
	// a glow with a power below 0 stays on the body, but it has no power, so the creature does not hum
	EXPECT_EQ(creature_hand_glow::SetCreedPower(glows, Place::Belly, 1.0f, -1.0f), Hum::Unchanged);
	EXPECT_TRUE(glows.at(1).has_value());
	EXPECT_FALSE(creature_hand_glow::AnyActive(glows));
}

TEST_F(CreatureCreedPowerSystem, ACreedPowerGlowsOnTheCreatureAndHumsWhileItHasPower)
{
	const auto creature = MakeCreature();
	// Khazar's creature in the killing of Khazar: the circling glow at a scale of 5
	_system.SetCreedPower(creature, Place::Circling, 5.0f, 1.0f);
	ASSERT_TRUE(Reg().AllOf<CreatureHandGlows>(creature));
	const auto& glow = Reg().Get<CreatureHandGlows>(creature).glows.at(3);
	ASSERT_TRUE(glow.has_value());
	EXPECT_EQ(*glow, (creature_hand_glow::Glow {.power = 1.0f, .scale = 5.0f}));
	const std::vector<RecordingSoundTags::Made> hum {{.thing = creature,
	                                                  .sample = k_CreedHum,
	                                                  .track = true,
	                                                  .mode = 2,
	                                                  .loops = -1,
	                                                  .extra3DFlag = false,
	                                                  .is3D = true,
	                                                  .bank = audio::SfxBank::InGame,
	                                                  .delay = 0}};
	EXPECT_EQ(_sounds->made, hum);
	// a weaker power while it hums: no second hum
	_system.SetCreedPower(creature, Place::Circling, 5.0f, 0.1f);
	EXPECT_EQ(_sounds->made.size(), 1u);
	EXPECT_TRUE(_sounds->removed.empty());
	// no power: the glow goes and the hum's tags go, the sound let finish
	_system.SetCreedPower(creature, Place::Circling, 0.0f, 0.0f);
	EXPECT_FALSE(Reg().Get<CreatureHandGlows>(creature).glows.at(3).has_value());
	const std::vector<RecordingSoundTags::Removed> stopped {
	    {.thing = creature, .sample = k_CreedHum, .bank = audio::SfxBank::InGame, .stop = false}};
	EXPECT_EQ(_sounds->removed, stopped);
}

TEST_F(CreatureCreedPowerSystem, WithoutSoundTagsTheCreatureGlowsSilently)
{
	const auto creature = MakeCreature();
	Locator::soundTagSystem::reset();
	_system.SetCreedPower(creature, Place::LeftHand, 1.0f, 1.0f);
	EXPECT_TRUE(Reg().Get<CreatureHandGlows>(creature).glows.at(0).has_value());
}

TEST(CreatureCreedPower, TheCreedMomentsSetTheLeftHandThenTheRightHandThenTheBelly)
{
	EXPECT_FALSE(creature_hand_glow::CreedEventOf(94).has_value());
	EXPECT_FALSE(creature_hand_glow::CreedEventOf(110).has_value());
	EXPECT_FALSE(creature_hand_glow::CreedEventOf(3).has_value());
	constexpr std::array<Place, 3> k_Places {Place::LeftHand, Place::RightHand, Place::Belly};
	// the powers are exact: two times 0, a quarter, a half, three quarters and 1
	constexpr std::array<float, 5> k_Powers {0.0f, 0.5f, 1.0f, 1.5f, 2.0f};
	for (int32_t event = 95; event <= 109; ++event)
	{
		const auto glow = creature_hand_glow::CreedEventOf(event);
		ASSERT_TRUE(glow.has_value()) << event;
		EXPECT_EQ(glow->place, k_Places.at(static_cast<size_t>((event - 95) / 5))) << event;
		EXPECT_EQ(glow->power, k_Powers.at(static_cast<size_t>((event - 95) % 5))) << event;
	}
}

TEST_F(CreatureCreedPowerSystem, ACreedMomentSetsAGlowWithNoHumAndTheFirstMomentMakesTheGlows)
{
	const auto creature = MakeCreature();
	// another action sets nothing, and makes no glows
	_system.PlayCreedEvent(creature, 3);
	EXPECT_FALSE(Reg().AllOf<CreatureHandGlows>(creature));
	// the first moment makes the glows, even one that puts a glow out
	_system.PlayCreedEvent(creature, 100);
	ASSERT_TRUE(Reg().AllOf<CreatureHandGlows>(creature));
	EXPECT_FALSE(creature_hand_glow::AnyActive(Reg().Get<CreatureHandGlows>(creature).glows));
	// the right hand at a quarter (power a half), the left at a half (power 1), each at a scale of 1
	_system.PlayCreedEvent(creature, 101);
	_system.PlayCreedEvent(creature, 97);
	const auto& glows = Reg().Get<CreatureHandGlows>(creature).glows;
	ASSERT_TRUE(glows.at(2).has_value());
	EXPECT_EQ(*glows.at(2), (creature_hand_glow::Glow {.power = 0.5f, .scale = 1.0f}));
	ASSERT_TRUE(glows.at(0).has_value());
	EXPECT_EQ(*glows.at(0), (creature_hand_glow::Glow {.power = 1.0f, .scale = 1.0f}));
	// the creature does not hum for them
	EXPECT_TRUE(_sounds->made.empty());
	// level 0 puts each out, still with no sound
	_system.PlayCreedEvent(creature, 100);
	_system.PlayCreedEvent(creature, 95);
	EXPECT_FALSE(glows.at(2).has_value());
	EXPECT_FALSE(glows.at(0).has_value());
	EXPECT_TRUE(_sounds->removed.empty());
}

TEST_F(CreatureCreedPowerNative, ACreatureIsGivenTheScriptsPlaceScaleAndPower)
{
	const auto creature = Reg().Create();
	Reg().Assign<Creature>(creature);
	SetCreedProperties(static_cast<uint32_t>(creature), 3, 5.0f, 0.1f, 2.0f);
	const std::vector<RecordingAnimation::Power> expected {
	    {.creature = creature, .place = Place::Circling, .scale = 5.0f, .power = 0.1f}};
	EXPECT_EQ(_animation->powers, expected);
}

TEST_F(CreatureCreedPowerNative, NoThingAnotherThingOrAPlaceOutOfRangeChangesNothing)
{
	// no thing, a thing that is not a creature, and a creature with a number that names no place
	SetCreedProperties(0, 3, 5.0f, 1.0f);
	SetCreedProperties(static_cast<uint32_t>(Reg().Create()), 3, 5.0f, 1.0f);
	const auto creature = Reg().Create();
	Reg().Assign<Creature>(creature);
	SetCreedProperties(static_cast<uint32_t>(creature), 4, 5.0f, 1.0f);
	SetCreedProperties(static_cast<uint32_t>(creature), -1, 5.0f, 1.0f);
	EXPECT_TRUE(_animation->powers.empty());
}

// The glows as they are drawn

TEST(CreatureCreedGlowDraw, TheTextureSlidesAsTheOriginalSlidesIt)
{
	// the original's own steps for a frame of 50 ms at the creed glow's speeds, and of 100 ms at the citadel heart's
	const auto first = glow_shell::Slide({0.95f, 0.9f}, creature_hand_glow::k_SlideSpeed, 50);
	EXPECT_EQ(std::bit_cast<uint32_t>(first.x), 0x3D4CCCC0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(first.y), 0x3F68F5C2u);
	const auto second = glow_shell::Slide({0.3f, 0.99f}, {0.6f, 0.2f}, 100);
	EXPECT_EQ(std::bit_cast<uint32_t>(second.x), 0x3EB851ECu);
	EXPECT_EQ(std::bit_cast<uint32_t>(second.y), 0x3C23D700u);
}

TEST(CreatureCreedGlowDraw, AShellFarBeyondItsSizeIsLeftOut)
{
	// a half diagonal of 1 at a scale of 2 reaches 600 out: at 600 it is drawn, a little further it is not
	EXPECT_FALSE(glow_shell::TooFar(600.0f * 600.0f, 1.0f, 2.0f));
	EXPECT_TRUE(glow_shell::TooFar(600.0f * 600.0f + 64.0f, 1.0f, 2.0f));
}

TEST(CreatureCreedGlowDraw, TheModelIsScaledByTheBodyAndTheGlowWithItsBackAxisMirrored)
{
	const auto model = creature_hand_glow::ModelMatrix(glm::mat3(1.0f), {1.0f, 2.0f, 3.0f}, 2.0f, 5.0f);
	EXPECT_EQ(glm::vec3(model[0]), glm::vec3(10.0f, 0.0f, 0.0f));
	EXPECT_EQ(glm::vec3(model[1]), glm::vec3(0.0f, 10.0f, 0.0f));
	EXPECT_EQ(glm::vec3(model[2]), glm::vec3(0.0f, 0.0f, -10.0f));
	EXPECT_EQ(model[3], glm::vec4(1.0f, 2.0f, 3.0f, 1.0f));
	EXPECT_EQ(creature_hand_glow::Zoom({.power = 0.25f}), 0.5f);
}

namespace
{
/// A body of six bones: 0 the root, 1 the right arm, 2 the right hand with two fingers 3 and 4, and 5 the left hand,
/// the right hand's mirror; the right hand scaled, which its glow does not keep
struct MadeUpBody
{
	std::array<glm::mat4, 6> bones {};
	std::array<uint32_t, 6> parents {0, 0, 1, 2, 2, 0};
	std::array<uint32_t, 6> mirror {0, 1, 5, 3, 4, 2};

	MadeUpBody()
	{
		bones.fill(glm::mat4(1.0f));
		bones[2] = glm::translate(glm::mat4(1.0f), {10.0f, 0.0f, 0.0f}) * glm::scale(glm::mat4(1.0f), glm::vec3(3.0f));
		bones[3] = glm::translate(glm::mat4(1.0f), {12.0f, 2.0f, 0.0f});
		bones[4] = glm::translate(glm::mat4(1.0f), {12.0f, 4.0f, 0.0f});
		bones[5] = glm::translate(glm::mat4(1.0f), {-10.0f, 0.0f, 0.0f});
	}

	[[nodiscard]] creature_hand_glow::Body AsBody() const
	{
		return {.bones = bones,
		        .parents = parents,
		        .mirror = mirror,
		        .rightHand = 2,
		        .belly = 0,
		        .origin = {100.0f, 50.0f, 200.0f},
		        .scale = 2.0f,
		        .size = 2.0f};
	}
};
} // namespace

TEST(CreatureCreedGlowDraw, AHandGlowIsAtItsFingertipsAndTheBellyGlowAtTheBelly)
{
	const MadeUpBody made;
	creature_hand_glow::Glow glow {.power = 1.0f, .scale = 1.5f};
	const auto right = creature_hand_glow::PlaceOn(glow, Place::RightHand, made.AsBody(), 0, 0);
	ASSERT_TRUE(right.has_value());
	// between the two fingertips, the hand's axes made one long, times the body's scale and the glow's
	EXPECT_EQ(glm::vec3(right->model[3]), glm::vec3(12.0f, 3.0f, 0.0f));
	EXPECT_NEAR(right->model[0][0], 3.0f, 1e-3f);
	EXPECT_NEAR(right->model[2][2], -3.0f, 1e-3f);
	EXPECT_EQ(right->scale, 3.0f);
	EXPECT_EQ(right->zoom, 2.0f);
	// the left hand's mirror bone has nothing under it: it is its own end
	const auto left = creature_hand_glow::PlaceOn(glow, Place::LeftHand, made.AsBody(), 0, 0);
	ASSERT_TRUE(left.has_value());
	EXPECT_EQ(glm::vec3(left->model[3]), glm::vec3(-10.0f, 0.0f, 0.0f));
	const auto belly = creature_hand_glow::PlaceOn(glow, Place::Belly, made.AsBody(), 0, 0);
	ASSERT_TRUE(belly.has_value());
	EXPECT_EQ(glm::vec3(belly->model[3]), glm::vec3(0.0f));
}

TEST(CreatureCreedGlowDraw, TheCirclingGlowTurnsByTheGameTimeAboveTheBodyAndSlidesByTheRealTime)
{
	const MadeUpBody made;
	creature_hand_glow::Glow glow {.power = 1.0f, .scale = 5.0f};
	const auto placed = creature_hand_glow::PlaceOn(glow, Place::Circling, made.AsBody(), 50, 500);
	ASSERT_TRUE(placed.has_value());
	EXPECT_EQ(glow.turn, 0.5f);
	EXPECT_EQ(placed->model[3].x, 100.0f);
	EXPECT_FLOAT_EQ(placed->model[3].y, (1.3f * 2.0f) * 15.0f + 50.0f);
	EXPECT_EQ(placed->model[3].z, 200.0f);
	const auto turned = creature_hand_glow::CirclingAxes(0.5f);
	EXPECT_EQ(turned[0].x, static_cast<float>(std::cos(0.5)));
	EXPECT_EQ(turned[0].z, static_cast<float>(std::sin(0.5)));
	EXPECT_EQ(turned[2].x, -static_cast<float>(std::sin(0.5)));
	// the real time slides the texture, kept on the glow for the next frame
	EXPECT_EQ(placed->slide, glow.slide);
	EXPECT_EQ(glow.slide, glow_shell::Slide({0.0f, 0.0f}, creature_hand_glow::k_SlideSpeed, 50));
}

TEST(CreatureCreedGlowDraw, AGlowWhoseBoneTheBodyLacksIsNotPlacedAndDoesNotSlide)
{
	const MadeUpBody made;
	auto body = made.AsBody();
	body.rightHand = 9;
	creature_hand_glow::Glow glow {};
	EXPECT_FALSE(creature_hand_glow::PlaceOn(glow, Place::RightHand, body, 50, 50).has_value());
	EXPECT_FALSE(creature_hand_glow::PlaceOn(glow, Place::LeftHand, body, 50, 50).has_value());
	EXPECT_EQ(glow.slide, glm::vec2(0.0f));
}

TEST(CreatureCreedGlowDraw, EachDrawOfTheBodyPlacesItsGlowsAndMovesThemOn)
{
	// The glows move on with each draw of the body: drawn twice, the circling glow turns twice and the textures slide
	// twice
	const MadeUpBody made;
	creature::CreatureRig rig;
	rig.actionPoints = creature::CreatureRig::ActionPoints {.rightHand = 2, .belly = 0};
	CreatureAnimation animation;
	animation.skeleton.parents.assign(made.parents.begin(), made.parents.end());
	animation.mirror.assign(made.mirror.begin(), made.mirror.end());
	CreatureHandGlows glows;
	creature_hand_glow::Set(glows.glows, Place::Circling, 5.0f, 1.0f);
	creature_hand_glow::Set(glows.glows, Place::RightHand, 1.5f, 1.0f);
	const glm::vec3 origin {100.0f, 50.0f, 200.0f};

	ecs::systems::CreatureAnimationSystem::PlaceGlows(glows, animation, rig, made.bones, origin, 2.0f, 2.0f, 50, 500);
	ASSERT_TRUE(glows.placed.at(2).has_value());
	ASSERT_TRUE(glows.placed.at(3).has_value());
	EXPECT_FALSE(glows.placed.at(0).has_value());
	EXPECT_EQ(glm::vec3(glows.placed.at(2)->model[3]), glm::vec3(12.0f, 3.0f, 0.0f));
	EXPECT_EQ(glows.glows.at(3)->turn, 0.5f);
	const auto once = glow_shell::Slide({0.0f, 0.0f}, creature_hand_glow::k_SlideSpeed, 50);
	EXPECT_EQ(glows.glows.at(2)->slide, once);

	ecs::systems::CreatureAnimationSystem::PlaceGlows(glows, animation, rig, made.bones, origin, 2.0f, 2.0f, 50, 500);
	EXPECT_EQ(glows.glows.at(3)->turn, 1.0f);
	EXPECT_EQ(glows.glows.at(2)->slide, glow_shell::Slide(once, creature_hand_glow::k_SlideSpeed, 50));
	EXPECT_EQ(glows.placed.at(2)->slide, glows.glows.at(2)->slide);
}
