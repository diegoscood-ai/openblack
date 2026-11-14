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
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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
#include "Audio/Game/AudioSystem.h"
#include "Audio/Game/Banks.h"
#include "Audio/Services/SpellSounds.h"
#include "Common/Zip.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
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
#include "support/ParticleShown.h"
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
		component.matrix.scale = scale;
		component.previousMatrix.scale = scale;
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
TEST_F(PhysicalShieldMiracle, ItKeepsAReactionFromOutsideOffWhatIsUnderIt)
{
	static_cast<void>(MakeShield(k_TestScale));
	// a living thing under the shield, the reaction's source far outside it: kept off
	EXPECT_TRUE(map_shield::IsReactionBlockedByShield(glm::vec3(105.0f, 0.0f, 100.0f), glm::vec3(500.0f, 0.0f, 500.0f)));
	// the source under the shield too: it reaches it
	EXPECT_FALSE(map_shield::IsReactionBlockedByShield(glm::vec3(105.0f, 0.0f, 100.0f), glm::vec3(100.0f, 0.0f, 100.0f)));
	// a living thing far outside the shield: nothing keeps it off
	EXPECT_FALSE(map_shield::IsReactionBlockedByShield(glm::vec3(500.0f, 0.0f, 500.0f), glm::vec3(900.0f, 0.0f, 900.0f)));
}

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
	EXPECT_FLOAT_EQ(Registry().Get<const MagicShield>(shield).matrix.scale, k_Final);
	// a thing that is not a shield keeps scale 1
	EXPECT_FLOAT_EQ(map_shield::CollisionScale(Registry().Create()), 1.0f);
}

// Each turn the shield's matrix becomes the last turn's whole, and the new one is its scale on the diagonal turned
// about Y by its angle, at its place; the draw goes from the last turn's to this turn's cell by cell, the rows carrying
// the scale (docs/wiki/magic/shields.md#shields, the shield's turn)
TEST_F(PhysicalShieldMiracle, EachTurnItsMatrixBecomesTheLastTurnsAndTheNewOneIsTurnedByItsAngle)
{
	constexpr float k_Final = 0.017f * 40.0f;
	const auto shield = MakeShield(k_Final);
	{
		auto& component = Registry().Get<MagicShield>(shield);
		component.creationTurn = 0;
		component.finalScale = k_Final;
		component.startScale = k_Final * 0.01f;
		component.startSpin = 1.0f;
		component.endSpin = 0.15f;
	}
	for (uint32_t turn = 0; turn <= 12; ++turn)
	{
		game_clock::SetTurn(turn);
		const auto before = Registry().Get<const MagicShield>(shield).matrix;
		map_shield::ProcessShields();
		const auto& component = Registry().Get<const MagicShield>(shield);
		EXPECT_EQ(component.previousMatrix, before) << "turn " << turn;
		EXPECT_EQ(component.matrix.rows, map_shield::TurnedRows(component.angle, component.matrix.scale)) << "turn " << turn;
	}
	map_shield::DrawShields();
	const auto& transform = Registry().Get<const ecs::components::Transform>(shield);
	EXPECT_EQ(transform.scale, glm::vec3(1.0f));
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
	openblack::test::ShowEveryAtom(effect);
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

namespace
{
/// One atom made with a create sound, released softly or not
std::string AnAtomWithACreateSound(bool softRelease)
{
	return std::string(
	           "BEGINPROPERTIES\nPROPERTY DeleteOnCloseDown BOOL 1\n"
	           "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
	           "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
	           "PROPERTY MaxSpellAge FLOAT -1\nENDPROPERTIES\n"
	           "BEGINCLASS ParticlePointCreator P0\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\nENDPROPERTIES\n"
	           "ENDCLASS\n"
	           "BEGINCLASS CreateRuleAnAtom A0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\nPROPERTY NextGroups ARRAY SIZE 0\n"
	           "PROPERTY PCreator PERSIS_PNTR P0\n"
	           "PROPERTY SoundOfCreate SOUND_ACTION SOUND_SPELL_PHYSICAL_SHIELD_CREATE LOOPING 0 ONLYONE 0 SOFTRELEASE ") +
	       (softRelease ? "1" : "0") + " USESURFACE 0\nENDPROPERTIES\nENDCLASS\n";
}

/// The game's Data\Spells\ZSpellFiles\<name>_txt.zzz: the length then a deflate stream, or nothing when it is missing
std::optional<particles::File> LoadGameSpellFile(const char* game, const std::string& name)
{
	std::ifstream stream(std::filesystem::path(game) / "Data" / "Spells" / "ZSpellFiles" / (name + "_txt.zzz"),
	                     std::ios::binary);
	if (!stream.is_open())
	{
		return std::nullopt;
	}
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	if (bytes.size() <= 4)
	{
		return std::nullopt;
	}
	uint32_t size = 0;
	std::memcpy(&size, bytes.data(), sizeof(size));
	const auto inflated = zip::Inflate(std::vector<uint8_t>(bytes.begin() + 4, bytes.end()), size);
	return particles::File::Parse(std::string(inflated.begin(), inflated.end()), name);
}
} // namespace

// A particle sound whose atom goes is let go to ring out when it has SOFTRELEASE, and stopped at once when it has not
// (docs/wiki/audio/magic-and-weather-sounds.md, the lifetime of the particle sound)
TEST_F(PhysicalShieldMiracleEffect, AGoneAtomsSoundRingsOutWithSoftReleaseAndStopsAtOnceWithout)
{
	const SyntheticSoundActions soundActions;
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& heard = static_cast<HeardAnimEffects&>(Locator::audio::emplace<HeardAnimEffects>());
	for (const bool soft : {true, false})
	{
		const auto file = Parse(AnAtomWithACreateSound(soft), "SoundReleaseTest");
		ASSERT_NE(file, nullptr);
		{
			particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), 1.0f);
			effect.Step(0.1f);
			const auto atoms = PointAtoms(effect);
			ASSERT_EQ(atoms.size(), 1u) << soft;
			ASSERT_EQ(SoundsOf(*atoms[0], k_CreateSound).size(), 1u) << soft;
		}
		heard.requests.clear();
		audio::spell_sounds::ProcessTurn(0.1f);
		const auto actions = heard.ActionsOf(k_CreateSound);
		ASSERT_EQ(actions.size(), 1u) << soft;
		EXPECT_EQ(actions.front(), soft ? audio::AnimAction::Release : audio::AnimAction::Stop) << soft;
		audio::spell_sounds::Clear();
	}
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it. The thrown fireball's whoosh
// (its root's create sound) has SOFTRELEASE in the plain ball's file and not in the two power-ups', so the plain one
// rings out as it ends and the power-ups' stop at once
TEST(FireballWhoosh, realDataThePlainBallsWhooshRingsOutAndThePowerUpsStop)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	for (const auto& [name, soft] : {std::pair {"SF_FireBallThrow", true}, std::pair {"SF_FireBallThrowPU", false},
	                                 std::pair {"SF_FireBallThrowPU2", false}})
	{
		const auto file = LoadGameSpellFile(game, name);
		ASSERT_TRUE(file.has_value()) << name;
		size_t roots = 0;
		for (const auto& object : file->objects)
		{
			if (object.className != "CreateWithInitialDirection")
			{
				continue;
			}
			++roots;
			const auto whoosh = particles::ReadSoundAction(object, "SoundOfCreate");
			EXPECT_EQ((whoosh.flags & particles::SoundAction::k_SoftRelease) != 0, soft) << name;
			EXPECT_EQ(whoosh.flags & particles::SoundAction::k_Looping, 0) << name;
		}
		EXPECT_EQ(roots, 1u) << name;
	}
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it. The powered-up heal's
// mushroom sounds as its atom is made, and has no SOFTRELEASE, so it is stopped at once if its effect goes; its chakras'
// heal sound rings out
TEST(HealMushroomSound, realDataTheMushroomSoundsAtItsMakingAndStopsAtOnce)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto file = LoadGameSpellFile(game, "SF_HealChakraPU");
	ASSERT_TRUE(file.has_value());
	size_t mushrooms = 0;
	size_t chakras = 0;
	for (const auto& object : file->objects)
	{
		if (object.className == "CreateRuleAnAtom" && object.Sound("SoundOfCreate").sound == "SOUND_SPELL_HEAL_MUSHROOM")
		{
			++mushrooms;
			const auto sound = particles::ReadSoundAction(object, "SoundOfCreate");
			EXPECT_EQ(sound.flags & particles::SoundAction::k_SoftRelease, 0);
			EXPECT_EQ(sound.flags & particles::SoundAction::k_Looping, 0);
		}
		if (object.className == "UR_HealSpellChakra")
		{
			++chakras;
			EXPECT_NE(particles::ReadSoundAction(object, "SoundHeal").flags & particles::SoundAction::k_SoftRelease, 0);
		}
	}
	EXPECT_EQ(mushrooms, 1u);
	EXPECT_EQ(chakras, 1u);
}

namespace
{
/// The tornado rule alone, its funnel's loop and, before it as in the game's storm file, the rule that takes that loop
/// off the funnel's atom when the effect closes, fading it by 10 a sound turn
const std::string k_TornadoRoar =
    "BEGINPROPERTIES\nPROPERTY DeleteOnCloseDown BOOL 0\n"
    "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "PROPERTY MaxSpellAge FLOAT -1\nENDPROPERTIES\n"
    "BEGINCLASS ParticlePointCreator P0\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\nENDPROPERTIES\nENDCLASS\n"
    "BEGINCLASS EventConditionTrueOnCloseDown E0\nBEGINPROPERTIES\nENDPROPERTIES\nENDCLASS\n"
    "BEGINCLASS RemoveSoundFromAtom R0\nBEGINPROPERTIES\nPROPERTY FadeStep INTEGER 10\nPROPERTY Group INTEGER 0\n"
    "PROPERTY RemoveOnCloseDown BOOL 0\n"
    "PROPERTY Sound SOUND_ACTION SOUND_SPELL_PHYSICAL_SHIELD_HUM LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0\n"
    "PROPERTY SoundCondition PERSIS_PNTR E0\nENDPROPERTIES\nENDCLASS\n"
    "BEGINCLASS UR_Tornado T0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\nPROPERTY GroupFlying INTEGER 13\n"
    "PROPERTY GroupDebris INTEGER 15\nPROPERTY GroupMesh INTEGER 14\nPROPERTY GroupToMoveToOnCloseDown INTEGER 12\n"
    "PROPERTY GroupToMoveToOnceDone INTEGER 12\nPROPERTY NextGroups ARRAY SIZE 0\nPROPERTY NumAtomsToCreate INTEGER 0\n"
    "PROPERTY PCreator PERSIS_PNTR P0\nPROPERTY RemoveOnCloseDown BOOL 0\n"
    "PROPERTY SoundTornado SOUND_ACTION SOUND_SPELL_PHYSICAL_SHIELD_HUM LOOPING 1 ONLYONE 0 SOFTRELEASE 1 USESURFACE 0\n"
    "ENDPROPERTIES\nENDCLASS\n";

/// An audio service whose channel for every owner plays at a volume the fades take down, and that hears the animation
/// effects asked of it
class FadingChannel final: public audio::AudioManagerNoOp
{
public:
	[[nodiscard]] audio::Channel PlayingChannel(audio::Owner /*owner*/, audio::BankId /*bank*/) override { return 7; }
	[[nodiscard]] int Volume(audio::Channel /*channel*/) override { return volume; }
	void SetVolume(audio::Channel /*channel*/, int to) override { volume = to; }
	audio::Channel PlayAnimationEffect(audio::Owner /*owner*/, float /*distance*/, const audio::AnimKey& key,
	                                   audio::AnimAction action, audio::BankId /*bank*/, bool /*track*/, float /*minDistance*/,
	                                   float /*maxDistance*/) override
	{
		if (key[4] == k_HumSound)
		{
			actions.push_back(action);
		}
		return audio::k_NoChannel;
	}
	int volume {100};
	std::vector<audio::AnimAction> actions;
};
} // namespace

// The tornado's roar starts with its funnel, looping and released softly; while the tornado stands it plays on, and as
// its effect closes the roar is taken off the funnel, let go to ring out once and faded by 10 each sound turn
// (docs/wiki/magic/storm-and-tornado.md, docs/wiki/audio/magic-and-weather-sounds.md)
TEST_F(PhysicalShieldMiracleEffect, ATornadosRoarLoopsWithItsFunnelAndFadesOutSoftlyWhenItCloses)
{
	const SyntheticSoundActions soundActions;
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::reset();
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& channel = static_cast<FadingChannel&>(Locator::audio::emplace<FadingChannel>());
	const auto file = Parse(k_TornadoRoar, "TornadoRoarTest");
	ASSERT_NE(file, nullptr);
	particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), 1.0f);
	effect.Step(0.1f);
	ASSERT_EQ(effect.GetRoots().front()->atoms.size(), 1u);
	const auto& funnel = *effect.GetRoots().front()->atoms.front();
	const auto roars = SoundsOf(funnel, k_HumSound);
	ASSERT_EQ(roars.size(), 1u);
	EXPECT_EQ(roars[0]->action.flags, particles::SoundAction::k_Looping | particles::SoundAction::k_SoftRelease);
	for (int i = 0; i < 5; ++i)
	{
		effect.Step(0.1f);
		audio::spell_sounds::ProcessTurn(0.1f);
	}
	EXPECT_EQ(SoundsOf(funnel, k_HumSound).size(), 1u);
	EXPECT_EQ(channel.volume, 100);
	for (const auto action : channel.actions)
	{
		EXPECT_EQ(action, audio::AnimAction::Play);
	}
	channel.actions.clear();
	effect.CloseDown();
	effect.Step(0.1f);
	ASSERT_EQ(effect.GetRoots().front()->atoms.size(), 1u);
	EXPECT_TRUE(SoundsOf(*effect.GetRoots().front()->atoms.front(), k_HumSound).empty());
	audio::spell_sounds::ProcessTurn(0.1f);
	EXPECT_EQ(channel.actions, (std::vector<audio::AnimAction> {audio::AnimAction::Release}));
	EXPECT_EQ(channel.volume, 90);
	audio::spell_sounds::ProcessTurn(0.1f);
	EXPECT_EQ(channel.actions.size(), 1u);
	EXPECT_EQ(channel.volume, 80);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it. The storm's file gives the
// tornado's roar a loop with soft release, and takes it off the funnel on close-down, faded by 10
TEST(TornadoRoar, realDataTheRoarLoopsAndIsTakenOffAtCloseDownFadedByTen)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto file = LoadGameSpellFile(game, "SF_LightningStormPush");
	ASSERT_TRUE(file.has_value());
	const particles::Object* tornado = nullptr;
	const particles::Object* remove = nullptr;
	for (const auto& object : file->objects)
	{
		if (object.className == "UR_Tornado")
		{
			tornado = &object;
		}
		if (object.className == "RemoveSoundFromAtom" && object.Sound("Sound").sound == "SOUND_SPELL_TORNADO")
		{
			remove = &object;
		}
	}
	ASSERT_NE(tornado, nullptr);
	ASSERT_NE(remove, nullptr);
	const auto roar = tornado->Sound("SoundTornado");
	EXPECT_EQ(roar.sound, "SOUND_SPELL_TORNADO");
	EXPECT_TRUE(roar.looping);
	EXPECT_TRUE(roar.softRelease);
	// the same group as the tornado's, so it reaches the funnel's atom, and on close-down
	EXPECT_EQ(remove->Int("Group", -1), tornado->Int("Group", -2));
	EXPECT_EQ(remove->Int("FadeStep", 0), 10);
	const auto* condition = file->Find(remove->String("SoundCondition"));
	ASSERT_NE(condition, nullptr);
	EXPECT_EQ(condition->className, "EventConditionTrueOnCloseDown");
}

namespace
{
/// One atom whose sound plays while the effect is enabled, as the food, wood and water files pour
const std::string k_PourWhileEnabled =
    "BEGINPROPERTIES\nPROPERTY DeleteOnCloseDown BOOL 0\n"
    "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "PROPERTY MaxSpellAge FLOAT -1\nENDPROPERTIES\n"
    "BEGINCLASS ParticlePointCreator P0\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\nENDPROPERTIES\nENDCLASS\n"
    "BEGINCLASS EventConditionTrueWhenEnabled E0\nBEGINPROPERTIES\nENDPROPERTIES\nENDCLASS\n"
    "BEGINCLASS CreateRuleAnAtom A0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\nPROPERTY NextGroups ARRAY SIZE 0\n"
    "PROPERTY PCreator PERSIS_PNTR P0\nENDPROPERTIES\nENDCLASS\n"
    "BEGINCLASS StartStopSoundOnCondition S0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\n"
    "PROPERTY Sound SOUND_ACTION SOUND_SPELL_PHYSICAL_SHIELD_HUM LOOPING 0 ONLYONE 0 SOFTRELEASE 1 USESURFACE 0\n"
    "PROPERTY SoundCondition PERSIS_PNTR E0\nENDPROPERTIES\nENDCLASS\n";
} // namespace

// A sound kept on while the effect is enabled starts with it, stays while it pours, and once the effect is no longer
// enabled (its miracle let go) is taken off its atom and let go softly, once (docs/wiki/audio/magic-and-weather-sounds.md)
TEST_F(PhysicalShieldMiracleEffect, APouringSoundStopsWhenTheEffectIsNoLongerEnabledAndRingsOut)
{
	const SyntheticSoundActions soundActions;
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& channel = static_cast<FadingChannel&>(Locator::audio::emplace<FadingChannel>());
	const auto file = Parse(k_PourWhileEnabled, "PourTest");
	ASSERT_NE(file, nullptr);
	particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), 1.0f);
	particles::ProcessInfo info;
	info.enabled = true;
	effect.SetProcessInfo(info);
	for (int i = 0; i < 3; ++i)
	{
		effect.Step(0.1f);
	}
	const auto atoms = PointAtoms(effect);
	ASSERT_EQ(atoms.size(), 1u);
	ASSERT_EQ(SoundsOf(*atoms[0], k_HumSound).size(), 1u);
	audio::spell_sounds::ProcessTurn(0.1f);
	EXPECT_TRUE(std::ranges::find(channel.actions, audio::AnimAction::Release) == channel.actions.end());
	channel.actions.clear();
	info.enabled = false;
	effect.SetProcessInfo(info);
	effect.Step(0.1f);
	EXPECT_TRUE(SoundsOf(*atoms[0], k_HumSound).empty());
	audio::spell_sounds::ProcessTurn(0.1f);
	audio::spell_sounds::ProcessTurn(0.1f);
	EXPECT_EQ(channel.actions, (std::vector<audio::AnimAction> {audio::AnimAction::Release}));
}

namespace
{
/// An explosion service that keeps the camera shakes asked of it
class HeardShakes final: public ecs::systems::ExplosionSystemInterface
{
public:
	struct Shake
	{
		glm::vec3 position;
		float radius;
		float strength;
		float seconds;
		bool verticalOnly;
	};
	void AddRubble(const glm::vec3& /*centre*/, float /*yaw*/) override {}
	void AddSmoke(const glm::vec3& /*centre*/, float /*size*/, dust_puff::Kind /*kind*/,
	              std::optional<uint32_t> /*colour*/) override
	{
	}
	void AddShake(const glm::vec3& position, float radius, float strength, float seconds, bool verticalOnly) override
	{
		shakes.push_back({position, radius, strength, seconds, verticalOnly});
	}
	[[nodiscard]] bool IsShaking() const override { return !shakes.empty(); }
	void Update(float /*milliseconds*/) override {}
	void Reset() override {}
	std::vector<Shake> shakes;
};

/// The blast's falling column as its file has it: an atom whose bang is added 0.35 s into its life, shaking the camera
/// up and down within 200 m for 0.7 s
const std::string k_DelayedBang =
    "BEGINPROPERTIES\nPROPERTY DeleteOnCloseDown BOOL 0\n"
    "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "PROPERTY MaxSpellAge FLOAT -1\nENDPROPERTIES\n"
    "BEGINCLASS ParticlePointCreator P0\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\nENDPROPERTIES\nENDCLASS\n"
    "BEGINCLASS CreateRuleAnAtom A0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\nPROPERTY NextGroups ARRAY SIZE 0\n"
    "PROPERTY PCreator PERSIS_PNTR P0\nENDPROPERTIES\nENDCLASS\n"
    "BEGINCLASS AddSoundToAtom B0\nBEGINPROPERTIES\nPROPERTY CameraShakeDuration FLOAT 0.7\n"
    "PROPERTY CameraShakeRadius FLOAT 200\nPROPERTY Delay FLOAT 0.35\nPROPERTY DoCameraShake BOOL 1\n"
    "PROPERTY Group INTEGER 0\n"
    "PROPERTY Sound SOUND_ACTION SOUND_SPELL_PHYSICAL_SHIELD_CREATE LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0\n"
    "PROPERTY StopOtherSoundsFirst BOOL 0\nENDPROPERTIES\nENDCLASS\n";
} // namespace

// A sound added to an atom waits for the atom to be Delay old, then plays once, with its camera shake: within the radius,
// at strength 1, for the shake's time, up and down only (docs/wiki/magic/lightning-explosion.md, the blast's sounds)
TEST_F(PhysicalShieldMiracleEffect, AnAddedSoundWaitsForItsDelayThenPlaysOnceAndShakesTheCamera)
{
	const SyntheticSoundActions soundActions;
	const test::RestoreService<Locator::explosionSystem> restoreExplosions;
	auto& heard = static_cast<HeardShakes&>(Locator::explosionSystem::emplace<HeardShakes>());
	const auto file = Parse(k_DelayedBang, "DelayedBangTest");
	ASSERT_NE(file, nullptr);
	particles::Effect effect(file, glm::vec3(100.0f, 0.0f, 100.0f), 1.0f);
	const auto Bangs = [&effect]() {
		const auto atoms = PointAtoms(effect);
		return atoms.size() == 1 ? SoundsOf(*atoms[0], k_CreateSound).size() : size_t {99};
	};
	// 0.3 s: not yet
	for (int i = 0; i < 6; ++i)
	{
		effect.Step(0.05f);
	}
	EXPECT_EQ(Bangs(), 0u);
	EXPECT_TRUE(heard.shakes.empty());
	// 0.45 s: once
	for (int i = 0; i < 3; ++i)
	{
		effect.Step(0.05f);
	}
	EXPECT_EQ(Bangs(), 1u);
	ASSERT_EQ(heard.shakes.size(), 1u);
	EXPECT_FLOAT_EQ(heard.shakes[0].radius, 200.0f);
	EXPECT_FLOAT_EQ(heard.shakes[0].strength, 1.0f);
	EXPECT_FLOAT_EQ(heard.shakes[0].seconds, 0.7f);
	EXPECT_TRUE(heard.shakes[0].verticalOnly);
	for (int i = 0; i < 10; ++i)
	{
		effect.Step(0.05f);
	}
	EXPECT_EQ(Bangs(), 1u);
	EXPECT_EQ(heard.shakes.size(), 1u);
}

namespace
{
/// One visible atom made at rest on the land of no island (flat at height 0) and pulled down by a floor rule's gravity,
/// with an impact sound on the land's surface
std::string AFallWithAnImpactSound(float gravity)
{
	return "BEGINPROPERTIES\nPROPERTY DeleteOnCloseDown BOOL 0\n"
	       "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
	       "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
	       "PROPERTY MaxSpellAge FLOAT -1\nENDPROPERTIES\n"
	       "BEGINCLASS ParticlePointCreator P0\nBEGINPROPERTIES\nPROPERTY ColorA INTEGER 255\n"
	       "PROPERTY InitialScale FLOAT 1\nENDPROPERTIES\nENDCLASS\n"
	       "BEGINCLASS CreateRuleAnAtom A0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\nPROPERTY NextGroups ARRAY SIZE 0\n"
	       "PROPERTY PCreator PERSIS_PNTR P0\nENDPROPERTIES\nENDCLASS\n"
	       "BEGINCLASS UpdateRuleGravityWithFloor G0\nBEGINPROPERTIES\nPROPERTY Group INTEGER 0\nPROPERTY Gravity FLOAT " +
	       std::to_string(gravity) +
	       "\nPROPERTY MaxSpeed FLOAT 100\nPROPERTY UseWind BOOL 0\n"
	       "PROPERTY ImpactSound SOUND_ACTION SOUND_SPELL_PHYSICAL_SHIELD_CREATE LOOPING 0 ONLYONE 0 SOFTRELEASE 0 "
	       "USESURFACE 1\nENDPROPERTIES\nENDCLASS\n";
}
} // namespace

// A falling atom that hits the land fast enough plays the floor rule's impact sound once, sized by how fast it hit (from
// 20 m/s medium, from 40 large, else small) and with the surface it hit; a hit slower than 5 m/s makes none
// (docs/wiki/audio/magic-and-weather-sounds.md)
TEST_F(PhysicalShieldMiracleEffect, AnAtomHittingTheLandSoundsByItsSpeedAndTheSurface)
{
	const SyntheticSoundActions soundActions;
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::reset();
	struct Case
	{
		float gravity;
		size_t sounds;
		int32_t size;
	};
	// a step in the air takes it to gravity x 0.1 m/s, and the next one into the land at that speed: 3, 10, 25 and 45 m/s
	for (const auto& [gravity, sounds, size] :
	     {Case {30.0f, 0, 0}, Case {100.0f, 1, 3}, Case {250.0f, 1, 2}, Case {450.0f, 1, 1}})
	{
		particles::Effect effect(Parse(AFallWithAnImpactSound(gravity), "ImpactTest"), glm::vec3(100.0f, 0.0f, 100.0f), 1.0f);
		for (int i = 0; i < 5; ++i)
		{
			effect.Step(0.1f);
		}
		const auto atoms = PointAtoms(effect);
		ASSERT_EQ(atoms.size(), 1u) << gravity;
		const auto hits = SoundsOf(*atoms[0], k_CreateSound);
		ASSERT_EQ(hits.size(), sounds) << gravity;
		if (sounds == 1)
		{
			EXPECT_EQ(hits[0]->action.size, size) << gravity;
			EXPECT_EQ(hits[0]->action.surface, audio::SurfaceType(glm::vec3(100.0f, 0.0f, 100.0f))) << gravity;
		}
		audio::spell_sounds::Clear();
	}
}
