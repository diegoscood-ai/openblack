/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The physical shield pinned to the original: its solid shape (a cone of its own radius and height), the collision
// scale that jumps behind the drawn one, the draw's alpha floor and its first half second unseen, and the sounds of its
// effect (a sound when it is made, sized by its radius, and a looping hum let go softly when it goes). Wiki:
// docs/wiki/magic/shields.md#shields.

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/AxisAlignedBoundingBox.h"
#include "Audio/AudioManagerNoOp.h"
#include "Audio/Engine/AnimEffects.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/Game/Banks.h"
#include "Audio/Services/SpellSounds.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "FileSystem/DefaultFileSystem.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Chants.h"
#include "Magic/Core/Spell.h"
#include "Magic/Objects/MapShield.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/SoundAction.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/Resources.h"
#include "Resources/ResourcesInterface.h"
#include "support/MagicFakes.h"
#include "support/MapFakes.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using openblack::ecs::components::MagicShield;
using openblack::ecs::components::Spell;
using openblack::ecs::components::SpellCreator;
namespace map_shield = openblack::magic::map_shield;
namespace reactions = openblack::ecs::effects::reactions;

namespace
{
/// The shield's mesh box in the tests: half extents 10 x 5 x 8, so at scale 2 its 2D radius is 20 (the larger of x and
/// z) and its height 20 (twice the half height)
constexpr float k_TestScale = 2.0f;

/// A registry with the world's lists, the inert miracles' service, zeroed tables but the physical shield's upkeep rows,
/// a mesh box for the shield's mesh and no land (the shield's points are above a flat ground); each put back when the
/// test ends
class PhysicalShieldMiracle: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		reactions::Clear();
		Locator::magicSystem::emplace<test::InertMagicSystem>();
		auto info = std::make_unique<InfoConstants>();
		info->magicEffect.at(static_cast<size_t>(MagicType::PhysicalShield)).costPerGameTurn = 20.0f;
		for (auto& shield : info->magicShield)
		{
			shield.radiusForNormalCost = 30.0f;
		}
		Locator::infoConstants::reset(info.release());
		test::FakeMeshBoxProvider boxes;
		boxes.meshBox = [](entt::id_type id) -> std::optional<AxisAlignedBoundingBox> {
			if (id == resources::HashIdentifier(map_shield::k_Mesh))
			{
				return AxisAlignedBoundingBox {{-10.0f, 0.0f, -8.0f}, {10.0f, 10.0f, 8.0f}};
			}
			return std::nullopt;
		};
		Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>(boxes);
		Locator::terrainSystem::reset();
	}
	void TearDown() override
	{
		reactions::Clear();
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
	}

	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }

	/// A physical shield at (100, 100) with the shield's mesh at `scale`, as its process and draw see it
	[[nodiscard]] static entt::entity MakeShield(float scale, entt::entity spell = entt::null)
	{
		const auto shield = Registry().Create();
		auto& component = Registry().Assign<MagicShield>(shield);
		component.kind = MagicShield::Kind::Physical;
		component.magicType = MagicType::PhysicalShield;
		component.spell = spell;
		component.position = glm::vec3(100.0f, 0.0f, 100.0f);
		component.objectScale = scale;
		component.scale = scale;
		component.previousScale = scale;
		Registry().Assign<ecs::components::Transform>(shield, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f),
		                                              glm::vec3(scale));
		Registry().Assign<ecs::components::Mesh>(shield, resources::HashIdentifier(map_shield::k_Mesh), static_cast<int8_t>(0),
		                                         static_cast<int8_t>(0));
		Registry().Assign<ecs::components::Alpha>(shield, 0.0f);
		return shield;
	}

	/// A physical shield miracle the first player cast at (100, 100) with that prayer power
	[[nodiscard]] static entt::entity MakeSpell(float prayerPower)
	{
		const auto spell = Registry().Create();
		auto& component = Registry().Assign<Spell>(spell);
		component.magicType = MagicType::PhysicalShield;
		component.spellClass = magic::ClassOf(MagicType::PhysicalShield);
		component.creator = {.kind = SpellCreator::Kind::Player, .player = PlayerNames::PLAYER_ONE, .entity = entt::null};
		component.player = PlayerNames::PLAYER_ONE;
		component.hasPlayer = true;
		magic::chants::SetChants(component, prayerPower);
		component.position = glm::vec3(100.0f, 0.0f, 100.0f);
		component.castPos = component.position;
		component.magnitude = 40.0f;
		return spell;
	}

private:
	// put back in reverse order; the registry and the world's lists go in TearDown, in the game's order
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::magicSystem> _restoreMagic;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::RestoreService<Locator::meshBoxProvider> _restoreBoxes;
	test::RestoreService<Locator::terrainSystem> _restoreTerrain;
};
} // namespace

// The physical shield's volume is a cone of its own 2D radius R and height H (both from its mesh's box at its scale):
// a point is inside when its (x, z) distance d from the centre has d^2 < R^2 and its height above the land is below
// H (1 - d / R); both tests strict. A thing that is not a shield is never inside one (docs/wiki/magic/shields.md,
// IsPointDefinitelyWithinShieldVolume: the physical shield's cone)
TEST_F(PhysicalShieldMiracle, ItsSolidShapeIsAConeOfItsRadiusAndHeight)
{
	const auto shield = MakeShield(k_TestScale);
	// R = 2 x max(10, 8) = 20, H = 2 x 5 x 2 = 20
	struct Case
	{
		glm::vec3 point;
		bool inside;
	};
	for (const auto& [point, inside] : {
	         Case {{100.0f, 19.9f, 100.0f}, true},  // the centre, just under the top
	         Case {{100.0f, 20.0f, 100.0f}, false}, // the top itself
	         Case {{100.0f, 0.0f, 100.0f}, true},   // the centre on the ground
	         Case {{110.0f, 9.9f, 100.0f}, true},   // half way out: under H / 2
	         Case {{110.0f, 10.0f, 100.0f}, false}, // on the cone's side
	         Case {{110.0f, 10.1f, 100.0f}, false}, // over it
	         Case {{100.0f, 4.9f, 115.0f}, true},   // three quarters out along z: under H / 4
	         Case {{100.0f, 5.1f, 115.0f}, false},  // over it
	         Case {{120.0f, 0.0f, 100.0f}, false},  // on the rim: d^2 = R^2
	         Case {{88.0f, 0.0f, 116.0f}, false},   // on the rim, (12, 16) out
	         Case {{121.0f, 0.0f, 100.0f}, false},  // beyond it
	         Case {{95.0f, 14.9f, 100.0f}, true},   // a quarter out on the other side: under 3 H / 4
	     })
	{
		EXPECT_EQ(map_shield::IsPointDefinitelyWithinShieldVolume(shield, point), inside)
		    << point.x << ", " << point.y << ", " << point.z;
	}
	// at half the scale the cone is half as wide and half as high
	Registry().Get<MagicShield>(shield).objectScale = 1.0f;
	EXPECT_TRUE(map_shield::IsPointDefinitelyWithinShieldVolume(shield, glm::vec3(105.0f, 4.9f, 100.0f)));
	EXPECT_FALSE(map_shield::IsPointDefinitelyWithinShieldVolume(shield, glm::vec3(105.0f, 5.1f, 100.0f)));
	EXPECT_FALSE(map_shield::IsPointDefinitelyWithinShieldVolume(shield, glm::vec3(110.0f, 0.0f, 100.0f)));
	// not a shield
	const auto other = Registry().Create();
	EXPECT_FALSE(map_shield::IsPointDefinitelyWithinShieldVolume(other, glm::vec3(100.0f, 0.0f, 100.0f)));
}

// The collision scale (the object scale the physics body is built at) moves only when the drawn scale has changed and
// is more than 0.3 away from it, so it jumps behind the drawn one: a shield of radius 40 (final scale 0.68) keeps 0.68
// while it shrinks unseen, jumps to 0.276 on its third turn, keeps it while it grows, jumps to 0.597 on its sixteenth
// and then keeps 0.597 at full size, as it is never again 0.3 away (docs/wiki/magic/shields.md#shields, the shield's
// turn and its rescale)
TEST_F(PhysicalShieldMiracle, TheCollisionScaleJumpsBehindTheDrawnOne)
{
	constexpr float k_Final = 0.017f * 40.0f;
	const auto shield = MakeShield(k_Final);
	{
		auto& component = Registry().Get<MagicShield>(shield);
		component.creationTurn = 0;
		component.finalScale = k_Final;
		component.startScale = k_Final * 0.01f;
	}
	game_clock::SetTurn(0);
	map_shield::ProcessShields();
	EXPECT_FLOAT_EQ(map_shield::CollisionScale(shield), k_Final);
	for (uint32_t turn = 1; turn <= 40; ++turn)
	{
		game_clock::SetTurn(turn);
		map_shield::ProcessShields();
		const float expected = turn < 3 ? 0.68f : turn < 16 ? 0.276f : 0.597f;
		EXPECT_NEAR(map_shield::CollisionScale(shield), expected, 5e-4f) << "turn " << turn;
	}
	// the drawn scale is the full one by then
	EXPECT_FLOAT_EQ(Registry().Get<const MagicShield>(shield).scale, k_Final);
	// a thing that is not a shield keeps scale 1
	EXPECT_FLOAT_EQ(map_shield::CollisionScale(Registry().Create()), 1.0f);
}

// The shield is drawn only after its first half second (its alpha pass is off until then), and while its miracle lasts
// its alpha is its strength (at most 1) x 255, never below 40: a miracle with no prayer power left still draws it at 40
// (docs/wiki/magic/shields.md#shields, the shield's draw)
TEST_F(PhysicalShieldMiracle, ItIsDrawnFromHalfASecondAndNeverFainterThanTheFloor)
{
	const auto spell = MakeSpell(0.0f);
	const auto shield = MakeShield(0.68f, spell);
	Registry().Get<MagicShield>(shield).creationTurn = 0;
	EXPECT_EQ(map_shield::k_MinAlpha, 40);
	for (uint32_t turn = 0; turn <= 8; ++turn)
	{
		game_clock::SetTurn(turn);
		map_shield::DrawShields();
		// 0.5 s is turn 5: drawn only past it
		EXPECT_FLOAT_EQ(Registry().Get<const ecs::components::Alpha>(shield).value, turn > 5 ? 1.0f : 0.0f) << "turn " << turn;
		EXPECT_EQ(Registry().Get<const MagicShield>(shield).alpha, 40) << "turn " << turn;
	}
	// with prayer power: its strength x 255, at least 40
	magic::chants::SetChants(Registry().Get<Spell>(spell), 1000.0f);
	map_shield::DrawShields();
	const float strength = std::min(magic::GetSpellStrength(spell), 1.0f);
	const int expected = std::max(static_cast<int>(strength * 255.0f), 40);
	EXPECT_EQ(Registry().Get<const MagicShield>(shield).alpha, expected);
}

namespace
{
/// The sound part of SF_PhysicalShieldFX, as the game's file has it: group 2's atom, made at once, plays the shield's
/// create sound once, sized by the shield's radius (its magnitude x 1.11062), and AddSoundToAtom adds the looping hum,
/// released softly
constexpr std::string_view k_ShieldFxSounds = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 1
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS MagnitudeFloatProvider MagnitudeFloatProvider0
BEGINPROPERTIES
PROPERTY Maximum FLOAT 1000
PROPERTY Minimum FLOAT 0
PROPERTY ScaleBy FLOAT 1.11062
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_ForSound
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 2
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY OffsetX FLOAT 0
PROPERTY OffsetY FLOAT 0
PROPERTY OffsetZ FLOAT 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION SOUND_SPELL_PHYSICAL_SHIELD_CREATE LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
PROPERTY SoundRadiusFP PERSIS_PNTR MagnitudeFloatProvider0
ENDPROPERTIES
ENDCLASS
BEGINCLASS AddSoundToAtom AddSoundToAtom0
BEGINPROPERTIES
PROPERTY Group INTEGER 2
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY Sound SOUND_ACTION SOUND_SPELL_PHYSICAL_SHIELD_HUM LOOPING 1 ONLYONE 0 SOFTRELEASE 1 USESURFACE 0
PROPERTY SoundCondition PERSIS_PNTR NULL_STRING
PROPERTY StopOtherSoundsFirst BOOL 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator0
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
PROPERTY LoopAnim BOOL 1
ENDPROPERTIES
ENDCLASS
)";

/// A tiny SoundAction.h with the physical shield's two sounds, at the values the game's own file gives them
constexpr std::string_view k_SyntheticSoundActions = "enum LHSoundAction\n{\n\tNO_SOUND_ACTION = 0,\n"
                                                     "\tSOUND_SPELL_PHYSICAL_SHIELD_CREATE = 110,\n"
                                                     "\tSOUND_SPELL_PHYSICAL_SHIELD_HUM = 111,\n};\n";
constexpr int32_t k_CreateSound = 110;
constexpr int32_t k_HumSound = 111;

std::shared_ptr<const particles::File> Parse(std::string_view text, const char* name)
{
	auto file = particles::File::Parse(text, name);
	return file.has_value() ? std::make_shared<const particles::File>(*file) : nullptr;
}

/// Puts the synthetic SoundAction.h into a fresh resource cache, under the path the sound actions are read from, so
/// no file is opened. The previous file system and resources come back when it goes
class SyntheticSoundActions
{
public:
	SyntheticSoundActions()
	{
		Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
		Locator::resources::emplace<resources::Resources>();
		const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Data>() / "SoundAction.h";
		Locator::resources::value().GetBlobs().Load(
		    resources::BlobId(path), resources::BlobLoader::FromBufferTag {},
		    std::vector<uint8_t>(k_SyntheticSoundActions.begin(), k_SyntheticSoundActions.end()));
	}

private:
	// built before the constructor's body, so they keep the services that were there before
	const test::RestoreService<Locator::filesystem> _restoreFilesystem;
	const test::RestoreService<Locator::resources> _restoreResources;
};

/// An audio service that hears every animation effect asked of it, and has a channel still playing for every owner,
/// so a particle sound whose atom has gone is released rather than forgotten
class HeardAnimEffects final: public audio::AudioManagerNoOp
{
public:
	struct Request
	{
		audio::AnimKey key;
		audio::AnimAction action;
	};
	[[nodiscard]] audio::Channel PlayingChannel(audio::Owner /*owner*/, audio::BankId /*bank*/) override { return 7; }
	audio::Channel PlayAnimationEffect(audio::Owner /*owner*/, float /*distance*/, const audio::AnimKey& key,
	                                   audio::AnimAction action, audio::BankId /*bank*/, bool /*track*/, float /*minDistance*/,
	                                   float /*maxDistance*/) override
	{
		requests.push_back({key, action});
		return audio::k_NoChannel;
	}
	/// The actions asked for the sounds of that action (the key's last attribute), in order
	[[nodiscard]] std::vector<audio::AnimAction> ActionsOf(int32_t sound) const
	{
		std::vector<audio::AnimAction> actions;
		for (const auto& request : requests)
		{
			if (request.key[4] == sound)
			{
				actions.push_back(request.action);
			}
		}
		return actions;
	}
	std::vector<Request> requests;
};

/// The drawn point atoms of an effect
std::vector<const particles::Atom*> PointAtoms(const particles::Effect& effect)
{
	std::vector<particles::Effect::DrawAtom> drawn;
	effect.Collect(1.0f, drawn, particles::Creator::Kind::Point);
	std::vector<const particles::Atom*> atoms;
	for (const auto& atom : drawn)
	{
		if (atom.atom != nullptr)
		{
			atoms.push_back(atom.atom);
		}
	}
	return atoms;
}

/// The atom's sounds of that action
std::vector<const audio::ParticleSound*> SoundsOf(const particles::Atom& atom, int32_t action)
{
	std::vector<const audio::ParticleSound*> found;
	for (const auto& sound : atom.sounds)
	{
		if (sound != nullptr && sound->action.action == action)
		{
			found.push_back(sound.get());
		}
	}
	return found;
}

/// The particle tests set the game turn: it goes back to 0 after each one, also when an ASSERT stops it
class PhysicalShieldMiracleEffect: public ::testing::Test
{
protected:
	void TearDown() override
	{
		audio::spell_sounds::Clear();
		game_clock::SetTurn(0);
	}
};
} // namespace

// When the shield is made its effect plays the shield's create sound once (not looping, not released softly), sized by
// the shield's radius: the effect's magnitude is the radius, x 1.11062, and under 200 m is small (3), under 500 m medium
// (2), else large (1) (docs/wiki/audio/magic-and-weather-sounds.md, the lifetime of the particle sound; the sounds of
// SF_PhysicalShieldFX)
TEST_F(PhysicalShieldMiracleEffect, ASoundWhenItIsMadeSizedByItsRadius)
{
	const SyntheticSoundActions soundActions;
	ASSERT_EQ(particles::SoundActionByName("SOUND_SPELL_PHYSICAL_SHIELD_CREATE"), k_CreateSound);
	const test::RestoreService<Locator::audio> restoreAudio;
	Locator::audio::emplace<HeardAnimEffects>();
	const auto file = Parse(k_ShieldFxSounds, "SF_PhysicalShieldFXSoundTest");
	ASSERT_NE(file, nullptr);
	struct Case
	{
		float radius;
		int32_t size;
	};
	// 44.4 m, 333.2 m and 666.4 m
	for (const auto& [radius, size] : {Case {40.0f, 3}, Case {300.0f, 2}, Case {600.0f, 1}})
	{
		particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), radius);
		effect.Step(0.1f);
		const auto atoms = PointAtoms(effect);
		ASSERT_EQ(atoms.size(), 1u) << "radius " << radius;
		const auto creates = SoundsOf(*atoms[0], k_CreateSound);
		ASSERT_EQ(creates.size(), 1u) << "radius " << radius;
		EXPECT_EQ(creates[0]->action.size, size) << "radius " << radius;
		EXPECT_EQ(creates[0]->action.flags, 0) << "radius " << radius;
	}
}

// The effect's atom also carries the shield's hum, looping and released softly: while the shield stands it keeps
// playing, and when the shield goes (its effect and atom with it) the next sound turn lets it go to ring out, once
// (docs/wiki/audio/magic-and-weather-sounds.md, the lifetime of the particle sound; the sounds of SF_PhysicalShieldFX)
TEST_F(PhysicalShieldMiracleEffect, ALoopingHumLetGoSoftlyWhenTheShieldGoes)
{
	const SyntheticSoundActions soundActions;
	ASSERT_EQ(particles::SoundActionByName("SOUND_SPELL_PHYSICAL_SHIELD_HUM"), k_HumSound);
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& heard = static_cast<HeardAnimEffects&>(Locator::audio::emplace<HeardAnimEffects>());
	const auto file = Parse(k_ShieldFxSounds, "SF_PhysicalShieldFXSoundTest");
	ASSERT_NE(file, nullptr);
	{
		particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), 40.0f);
		effect.Step(0.1f);
		effect.Step(0.1f);
		const auto atoms = PointAtoms(effect);
		ASSERT_EQ(atoms.size(), 1u);
		const auto hums = SoundsOf(*atoms[0], k_HumSound);
		ASSERT_EQ(hums.size(), 1u);
		EXPECT_EQ(hums[0]->action.flags, particles::SoundAction::k_Looping | particles::SoundAction::k_SoftRelease);
		// still one hum, however long it stands, and never let go while it does
		for (int i = 0; i < 5; ++i)
		{
			effect.Step(0.1f);
			audio::spell_sounds::ProcessTurn(0.1f);
		}
		const auto standing = PointAtoms(effect);
		ASSERT_EQ(standing.size(), 1u);
		EXPECT_EQ(SoundsOf(*standing[0], k_HumSound).size(), 1u);
		for (const auto action : heard.ActionsOf(k_HumSound))
		{
			EXPECT_EQ(action, audio::AnimAction::Play);
		}
	}
	heard.requests.clear();
	audio::spell_sounds::ProcessTurn(0.1f);
	const auto hum = heard.ActionsOf(k_HumSound);
	ASSERT_FALSE(hum.empty());
	EXPECT_EQ(hum.back(), audio::AnimAction::Release);
	// released once only
	const auto asked = heard.requests.size();
	audio::spell_sounds::ProcessTurn(0.1f);
	EXPECT_EQ(heard.requests.size(), asked);
}

namespace
{
/// The sparkles' rules of SF_PhysicalShieldFX, as the game's file has them, on one atom of their own here instead of the
/// atoms the file puts at the dome's extra points: SetScale_EP sizes each by the effect's magnitude (the shield's radius)
/// x 0.2, at most 10, and AR_FadeAlpha_EP fades it from 255 at 1 s to 100 at 6 s
constexpr std::string_view k_ShieldFxSparkles = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 1
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_Sparkle
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator_EP
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS SetScale SetScale_EP
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY Scale PERSIS_PNTR MagnitudeFloatProvider_EP
ENDPROPERTIES
ENDCLASS
BEGINCLASS AR_FadeAlpha AR_FadeAlpha_EP
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 0
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY StartAlpha INTEGER 255
PROPERTY StartTime FLOAT 1
PROPERTY StopAlpha INTEGER 100
PROPERTY StopTime FLOAT 6
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator_EP
BEGINPROPERTIES
PROPERTY ColorA INTEGER 255
PROPERTY ColorB INTEGER 255
PROPERTY ColorG INTEGER 255
PROPERTY ColorR INTEGER 255
PROPERTY InitialScale FLOAT 1
PROPERTY LoopAnim BOOL 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS MagnitudeFloatProvider MagnitudeFloatProvider_EP
BEGINPROPERTIES
PROPERTY Maximum FLOAT 10
PROPERTY Minimum FLOAT -1e+006
PROPERTY ScaleBy FLOAT 0.2
ENDPROPERTIES
ENDCLASS
)";
} // namespace

// The shield's sparkles are sized by its radius, the effect's magnitude, x 0.2 and never beyond 10; each is fully seen
// for its first second, then fades linearly to 100 at 6 s and stays there (docs/wiki/physics/particles.md, the window
// rules AR_FadeAlpha and the float providers; the sparkles of SF_PhysicalShieldFX)
TEST_F(PhysicalShieldMiracleEffect, TheSparklesGrowWithTheRadiusAndFadeDownOverSixSeconds)
{
	const auto file = Parse(k_ShieldFxSparkles, "SF_PhysicalShieldFXSparkleTest");
	ASSERT_NE(file, nullptr);
	struct Case
	{
		float radius;
		float scale;
	};
	for (const auto& [radius, scale] : {Case {20.0f, 4.0f}, Case {40.0f, 8.0f}, Case {50.0f, 10.0f}, Case {80.0f, 10.0f}})
	{
		particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), radius);
		effect.Step(0.1f);
		effect.Step(0.1f);
		const auto atoms = PointAtoms(effect);
		ASSERT_EQ(atoms.size(), 1u) << "radius " << radius;
		EXPECT_FLOAT_EQ(atoms[0]->ruleScale, scale) << "radius " << radius;
	}
	particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), 40.0f);
	const auto alpha = [&effect]() {
		const auto atoms = PointAtoms(effect);
		return atoms.size() == 1 ? static_cast<int>(atoms[0]->colour[3]) : -1;
	};
	// under a second old: full
	for (int i = 0; i < 9; ++i)
	{
		effect.Step(0.1f);
	}
	EXPECT_EQ(alpha(), 255);
	// about 3.5 s old: half way down, 255 - 155 x 2.5 / 5, give or take a step
	for (int i = 9; i < 35; ++i)
	{
		effect.Step(0.1f);
	}
	EXPECT_GE(alpha(), 174);
	EXPECT_LE(alpha(), 182);
	// past 6 s: 100, and it stays
	for (int i = 35; i < 70; ++i)
	{
		effect.Step(0.1f);
	}
	EXPECT_EQ(alpha(), 100);
	for (int i = 70; i < 90; ++i)
	{
		effect.Step(0.1f);
	}
	EXPECT_EQ(alpha(), 100);
}
