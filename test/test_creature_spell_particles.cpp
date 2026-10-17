/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature spells' particle rules (Particles/ParticleCreatureSpellRules.cpp) run on small effects with a fake
// creature and a fake spell, the hearts' and the steam's arithmetic against the original's own numbers, and the
// creature's body as the game gives it to them
// (ECS/Systems/Implementations/CreatureSpellBodies.cpp) on a registry of its own. The game's list registers the rules
// (psys::RegisterAll).

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "Audio/Services/SpellSounds.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureSpellBodiesInterface.h"
#include "ECS/Systems/Implementations/CreatureSpellBodies.h"
#include "FileSystem/DefaultFileSystem.h"
#include "Locator.h"
#include "Particles/CreatureSpellMaths.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"
#include "Particles/ParticleObjectRules.h"
#include "Particles/SoundAction.h"
#include "Resources/Loaders.h"
#include "Resources/Resources.h"
#include "Resources/ResourcesInterface.h"
#include "support/RestoreService.h"

using namespace openblack;
using ecs::systems::CreatureSpellBody;

namespace
{
/// Steps of an eighth of a second add up without rounding
constexpr float k_Step = 0.125f;
constexpr float k_Epsilon = 1e-3f;
const auto k_Creature = static_cast<entt::entity>(7);
constexpr glm::vec3 k_Hand {60.0f, 40.0f, 60.0f};

/// One creature standing at (100, 0, 100), its bones a box: the feet, the eyes and the top of its head
class FakeBodies final: public ecs::systems::CreatureSpellBodiesInterface
{
public:
	[[nodiscard]] bool IsAvailable(entt::entity creature) const override { return creature == k_Creature && !gone; }
	[[nodiscard]] std::optional<CreatureSpellBody> CreatureBody(entt::entity creature) const override
	{
		if (creature != k_Creature || gone)
		{
			return std::nullopt;
		}
		return body;
	}

	CreatureSpellBody body {
	    .origin = {100.0f, 0.0f, 100.0f},
	    .size = 2.0f,
	    .bones = {{98.0f, 0.0f, 99.0f},
	              {102.0f, 0.0f, 99.0f},
	              {99.0f, 20.0f, 97.0f},
	              {101.0f, 20.0f, 97.0f},
	              {100.0f, 24.0f, 101.0f}},
	    .rightEye = 2,
	    .leftEye = 3,
	    .rightFoot = 0,
	    .leftFoot = 1,
	};
	bool gone {false};
};

/// A creature spell cast by a player's hand unless told otherwise, counting its close-downs
class FakeSpell final: public psys::SpellSink
{
public:
	int SpellEvent(const psys::SpellEventInfo& /*event*/) override { return 1; }
	[[nodiscard]] int PowerUpLevel() const override { return -1; }
	[[nodiscard]] bool IsScriptCasting() const override { return scriptCasting; }
	[[nodiscard]] bool IsCreatureCasting() const override { return creatureCasting; }
	[[nodiscard]] std::optional<int> CreatureSpellKind() const override { return kind; }
	[[nodiscard]] std::optional<float> MaxDirectionChange() const override { return maxDirectionChange; }
	void CloseDown() override { ++closeDowns; }

	bool creatureCasting {false};
	bool scriptCasting {false};
	std::optional<int> kind {1};
	std::optional<float> maxDirectionChange {1.0f};
	int closeDowns {0};
};

/// A tiny SoundAction.h with the two sounds of the first rule
constexpr std::string_view k_SyntheticSoundActions = "enum LHSoundAction\n{\n\tNO_SOUND_ACTION = 0,\n"
                                                     "\tSOUND_SPELL_CREATURE_WHISPS = 78,\n"
                                                     "\tSOUND_SPELL_CREATURE_SPELL_CAST = 117,\n};\n";
constexpr int32_t k_Whisps = 78;
constexpr int32_t k_Cast = 117;
constexpr int32_t k_CastByCreature = 152;

/// The synthetic SoundAction.h in a fresh resource cache, under the path the sound actions are read from; the previous
/// file system and resources come back when it goes
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
	const test::RestoreService<Locator::filesystem> _restoreFilesystem;
	const test::RestoreService<Locator::resources> _restoreResources;
};

std::string Header()
{
	return "BEGINPROPERTIES\nPROPERTY DeleteOnCloseDown BOOL 0\n"
	       "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
	       "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
	       "PROPERTY MaxSpellAge FLOAT -1\nENDPROPERTIES\n";
}

std::string Object(std::string_view className, std::string_view name, std::string_view properties)
{
	return "BEGINCLASS " + std::string(className) + " " + std::string(name) + "\nBEGINPROPERTIES\n" + std::string(properties) +
	       "ENDPROPERTIES\nENDCLASS\n";
}

/// The first rule, with these groups under its atom and the creator its atom is made by, as the game's files have it
/// but for the creator (none in the files), so that a test can find its atom among those drawn
std::string First(std::string_view nextGroups, std::string_view creator = "Point")
{
	return Object("UR_CreatureSpell", "UR_CreatureSpell0",
	              "PROPERTY Group INTEGER 0\nPROPERTY NextGroups ARRAY SIZE " + std::string(nextGroups) +
	                  "\nPROPERTY PCreator PERSIS_PNTR " + std::string(creator) +
	                  "\nPROPERTY SoundCreatureSpell SOUND_ACTION SOUND_SPELL_CREATURE_WHISPS LOOPING 1 ONLYONE 0 "
	                  "SOFTRELEASE 1 USESURFACE 0\n"
	                  "PROPERTY SoundCreatureSpellCast SOUND_ACTION SOUND_SPELL_CREATURE_SPELL_CAST LOOPING 0 ONLYONE 0 "
	                  "SOFTRELEASE 1 USESURFACE 0\n");
}

const std::string k_Point =
    Object("ParticlePointCreator", "Point", "PROPERTY ColorA INTEGER 255\nPROPERTY InitialScale FLOAT 1\n");

const std::string k_Sprite =
    Object("ParticleSpriteCreator", "Sprite",
           "PROPERTY ColorA INTEGER 50\nPROPERTY InitialScale FLOAT 1.5\nPROPERTY NumFrames INTEGER 1\n"
           "PROPERTY TextureFileName STRING .\\Data\\Textures\\S_SpriteSheet3.raw\n");

/// The wisps of SF_CreatureSpellGeneric: three of them, a second after the start
const std::string k_Wisps =
    Object("UR_CreatureSpellGeneric", "Wisps",
           "PROPERTY DelayBeforeEmit FLOAT 1\nPROPERTY Group INTEGER 1\nPROPERTY NextGroups ARRAY SIZE 0\n"
           "PROPERTY NumAtoms INTEGER 3\nPROPERTY PCreator PERSIS_PNTR Sprite\n"
           "PROPERTY PhiSpeed FLOAT 1.5\nPROPERTY TaperFrac FLOAT 0.5\nPROPERTY ThetaSpeed FLOAT 3.5\n");

/// The flies of SF_CreatureSpellItch, one of them, going to the head after a fifth of a second
const std::string k_Itch = Object("UR_CreatureSpellItch", "Itch",
                                  "PROPERTY Group INTEGER 4\nPROPERTY NextGroups ARRAY SIZE 0\nPROPERTY NumAtoms INTEGER 1\n"
                                  "PROPERTY PCreator PERSIS_PNTR Sprite\nPROPERTY PauseBeforeGotoCreature FLOAT 0.2\n");

/// Fake creatures in the locator; the previous ones come back after
class CreatureSpellParticlesTest: public ::testing::Test
{
protected:
	void SetUp() override { bodies = &static_cast<FakeBodies&>(Locator::creatureSpellBodies::emplace<FakeBodies>()); }
	void TearDown() override { audio::spell_sounds::Clear(); }

	/// An effect of the spell, cast on the creature from the hand
	std::unique_ptr<psys::Effect> Make(const std::string& body, bool withTarget = true)
	{
		auto file = psys::File::Parse(Header() + body, "test");
		EXPECT_TRUE(file.has_value());
		auto effect =
		    std::make_unique<psys::Effect>(std::make_shared<const psys::File>(std::move(*file)), bodies->body.origin, 1.0f);
		effect->SetSink(&spell);
		psys::ProcessInfo info;
		info.handPos = k_Hand;
		effect->SetProcessInfo(info);
		if (withTarget)
		{
			effect->AddTarget(k_Creature);
		}
		return effect;
	}

	static void Steps(psys::Effect& effect, int count)
	{
		for (int i = 0; i < count; ++i)
		{
			effect.Step(k_Step);
		}
	}

	static std::vector<psys::Effect::DrawAtom> Sprites(const psys::Effect& effect)
	{
		std::vector<psys::Effect::DrawAtom> atoms;
		effect.Collect(1.0f, atoms, psys::Creator::Kind::Sprite);
		return atoms;
	}

	/// The first rule's atoms, made by the point creator
	static std::vector<const psys::Atom*> Firsts(const psys::Effect& effect)
	{
		std::vector<psys::Effect::DrawAtom> drawn;
		effect.Collect(1.0f, drawn, psys::Creator::Kind::Point);
		std::vector<const psys::Atom*> atoms;
		for (const auto& atom : drawn)
		{
			atoms.push_back(atom.atom);
		}
		return atoms;
	}

	FakeBodies* bodies {nullptr};
	FakeSpell spell;

private:
	test::RestoreService<Locator::creatureSpellBodies> _restoreBodies;
};
} // namespace

TEST(CreatureSpellRules, TheGameRegistersTheRules)
{
	EXPECT_NE(psys::FindModifierFactory("UR_CreatureSpell"), nullptr);
	EXPECT_NE(psys::FindModifierFactory("UR_CreatureSpellGeneric"), nullptr);
	EXPECT_NE(psys::FindModifierFactory("UR_CreatureSpellItch"), nullptr);
	EXPECT_NE(psys::FindModifierFactory("UR_CreatureSpellCompassion"), nullptr);
	EXPECT_NE(psys::FindModifierFactory("UR_CreatureSpellFreeze"), nullptr);
}

TEST_F(CreatureSpellParticlesTest, AnAtomWhereTheCreatureStoodHumsWithTheCastingSound)
{
	const SyntheticSoundActions soundActions;
	ASSERT_EQ(psys::SoundActionByName("SOUND_SPELL_CREATURE_WHISPS"), k_Whisps);
	auto effect = Make(First("0") + k_Point);
	effect->Step(k_Step);
	auto atoms = Firsts(*effect);
	ASSERT_EQ(atoms.size(), 1u);
	EXPECT_EQ(atoms[0]->position, glm::vec3(100.0f, 0.0f, 100.0f));
	// The newest first: the casting sound, then the hum
	ASSERT_EQ(atoms[0]->sounds.size(), 2u);
	EXPECT_EQ(atoms[0]->sounds[0]->action.action, k_Cast);
	EXPECT_EQ(atoms[0]->sounds[1]->action.action, k_Whisps);
	EXPECT_TRUE(effect->GetTargets().empty());
	// The creature moves on; the atom stays where it was made
	bodies->body.origin = {110.0f, 0.0f, 100.0f};
	effect->Step(k_Step);
	atoms = Firsts(*effect);
	ASSERT_EQ(atoms.size(), 1u);
	EXPECT_EQ(atoms[0]->position, glm::vec3(100.0f, 0.0f, 100.0f));
}

TEST_F(CreatureSpellParticlesTest, ACreaturesCastSoundsCastByACreatureAndAScriptsMakesNone)
{
	const SyntheticSoundActions soundActions;
	spell.creatureCasting = true;
	auto byCreature = Make(First("0") + k_Point);
	byCreature->Step(k_Step);
	auto atoms = Firsts(*byCreature);
	ASSERT_EQ(atoms.size(), 1u);
	ASSERT_EQ(atoms[0]->sounds.size(), 2u);
	EXPECT_EQ(atoms[0]->sounds[0]->action.action, k_CastByCreature);
	spell.scriptCasting = true;
	auto byScript = Make(First("0") + k_Point);
	byScript->Step(k_Step);
	atoms = Firsts(*byScript);
	ASSERT_EQ(atoms.size(), 1u);
	ASSERT_EQ(atoms[0]->sounds.size(), 1u);
	EXPECT_EQ(atoms[0]->sounds[0]->action.action, k_Whisps);
}

TEST_F(CreatureSpellParticlesTest, WithoutACreatureTheFirstRuleLetsGoAndTheEffectEnds)
{
	auto none = Make(First("1 1") + k_Wisps + k_Point + k_Sprite, false);
	Steps(*none, 2);
	EXPECT_EQ(none->AtomCount(), 0u);
	EXPECT_TRUE(none->Finished());
	// A target that is not a creature is taken all the same
	auto notACreature = Make(First("1 1") + k_Wisps + k_Point + k_Sprite, false);
	notACreature->AddTarget(static_cast<entt::entity>(9));
	Steps(*notACreature, 2);
	EXPECT_EQ(notACreature->AtomCount(), 0u);
	EXPECT_TRUE(notACreature->GetTargets().empty());
}

TEST_F(CreatureSpellParticlesTest, WispsComeOutOfTheHandOneByOneAfterASecond)
{
	auto effect = Make(First("1 1") + k_Wisps + k_Point + k_Sprite);
	const auto wisps = [&effect]() { return effect->AtomCount() - 1; };
	// Three over two seconds: 0.1875 due a step once the collection is older than a second, from the tenth step
	Steps(*effect, 9);
	EXPECT_EQ(wisps(), 0u);
	Steps(*effect, 1);
	ASSERT_EQ(wisps(), 1u);
	const auto first = Sprites(*effect);
	ASSERT_EQ(first.size(), 1u);
	EXPECT_EQ(first[0].position, k_Hand);
	// Its own size times the creature's, at its least alpha
	EXPECT_FLOAT_EQ(first[0].scale, 1.5f * 2.0f);
	EXPECT_EQ(first[0].atom->colour[3], 20);
	Steps(*effect, 9);
	EXPECT_EQ(wisps(), 2u);
	Steps(*effect, 1);
	EXPECT_EQ(wisps(), 3u);
	Steps(*effect, 20);
	EXPECT_EQ(wisps(), 3u);
	// Flown for two seconds and more, every wisp has left the hand for the creature
	for (const auto& wisp : Sprites(*effect))
	{
		EXPECT_GT(glm::distance(wisp.position, k_Hand), 40.0f);
	}
}

TEST_F(CreatureSpellParticlesTest, AnInvisibleCreaturesWispsFadeWithIt)
{
	spell.kind = 7;
	bodies->body.invisible = 0.5f;
	auto effect = Make(First("1 1") + k_Wisps + k_Point + k_Sprite);
	Steps(*effect, 10);
	const auto atoms = Sprites(*effect);
	ASSERT_EQ(atoms.size(), 1u);
	// The collection at 127 of 255, the half truncated
	EXPECT_NEAR(atoms[0].alpha, 20.0f * 127.0f / 255.0f, 1e-4f);
}

TEST_F(CreatureSpellParticlesTest, TheWispsLetGoForASpellThatIsNoCreatureSpell)
{
	spell.kind = std::nullopt;
	auto effect = Make(First("1 1") + k_Wisps + k_Point + k_Sprite);
	Steps(*effect, 20);
	EXPECT_EQ(effect->AtomCount(), 1u);
	// Nor for the kind after the last
	spell.kind = 16;
	auto past = Make(First("1 1") + k_Wisps + k_Point + k_Sprite);
	Steps(*past, 20);
	EXPECT_EQ(past->AtomCount(), 1u);
}

TEST_F(CreatureSpellParticlesTest, ACreaturesCastClosesDownOnceTheWayFromItsHandsHasSwungTooFar)
{
	spell.creatureCasting = true;
	auto effect = Make(First("1 1") + k_Wisps + k_Point + k_Sprite);
	Steps(*effect, 3);
	EXPECT_EQ(spell.closeDowns, 0);
	// To the other side of the hands: the way has turned right round, past the cosine of 1
	bodies->body.origin = {20.0f, 0.0f, 20.0f};
	Steps(*effect, 1);
	EXPECT_EQ(spell.closeDowns, 1);
}

TEST_F(CreatureSpellParticlesTest, AHandsCastNeverClosesDownForTheSwing)
{
	auto effect = Make(First("1 1") + k_Wisps + k_Point + k_Sprite);
	Steps(*effect, 3);
	bodies->body.origin = {20.0f, 0.0f, 20.0f};
	Steps(*effect, 3);
	EXPECT_EQ(spell.closeDowns, 0);
}

TEST_F(CreatureSpellParticlesTest, TheFliesWaitAtTheHandThenCircleTheHead)
{
	auto effect = Make(First("1 4") + k_Itch + k_Point + k_Sprite);
	Steps(*effect, 2);
	auto atoms = Sprites(*effect);
	ASSERT_EQ(atoms.size(), 1u);
	EXPECT_EQ(atoms[0].position, k_Hand);
	Steps(*effect, 5);
	atoms = Sprites(*effect);
	ASSERT_EQ(atoms.size(), 1u);
	// Round the middle of the eyes, half as far again as each eye is from it
	const glm::vec3 middle(100.0f, 20.0f, 97.0f);
	EXPECT_NEAR(glm::distance(atoms[0].position, middle), 1.5f, k_Epsilon);
	EXPECT_NEAR(atoms[0].position.y, 20.0f, k_Epsilon);
}

namespace
{
/// The hearts of SF_CreatureSpellCompassion as its file has them, with a creator of their own
std::string Hearts(std::string_view maxAtoms = "20", std::string_view dieAge = "5")
{
	return Object("UR_CreatureSpellCompassion", "Hearts",
	              "PROPERTY DieAge FLOAT " + std::string(dieAge) + "\nPROPERTY Group INTEGER 4\nPROPERTY InitSpeed FLOAT 6\n" +
	                  "PROPERTY MaxAtoms INTEGER " + std::string(maxAtoms) +
	                  "\nPROPERTY NextGroups ARRAY SIZE 0\nPROPERTY PCreator PERSIS_PNTR Sprite\n");
}

/// An atom beside the first rule's, a little above the creature, with the hearts under it: an atom that gives its own
/// place when the hearts ask it for a point
const std::string k_Beside = Object("CreateRuleAnAtom", "Beside",
                                    "PROPERTY Group INTEGER 0\nPROPERTY NextGroups ARRAY SIZE 1 4\nPROPERTY OffsetY FLOAT 10\n"
                                    "PROPERTY PCreator PERSIS_PNTR Point\n");

/// The steam of SF_CreatureSpellFreeze, but for a count to make (its file's is 0) and a least height
std::string Steam(std::string_view maxAtoms)
{
	return Object(
	    "UR_CreatureSpellFreeze", "Steam",
	    "PROPERTY DieAge FLOAT 5\nPROPERTY Group INTEGER 1\nPROPERTY InitSpeed FLOAT 1.01\nPROPERTY MaxAtoms INTEGER " +
	        std::string(maxAtoms) +
	        "\nPROPERTY MinHeight FLOAT 0.5\nPROPERTY NextGroups ARRAY SIZE 0\nPROPERTY PCreator PERSIS_PNTR Sprite\n");
}

/// The hearts' atoms: those of the sprite creator
std::vector<const psys::Atom*> SpriteAtoms(const psys::Effect& effect)
{
	std::vector<psys::Effect::DrawAtom> drawn;
	effect.Collect(1.0f, drawn, psys::Creator::Kind::Sprite);
	std::vector<const psys::Atom*> atoms;
	for (const auto& atom : drawn)
	{
		atoms.push_back(atom.atom);
	}
	return atoms;
}

float Bits(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

uint32_t BitsOf(float value)
{
	return std::bit_cast<uint32_t>(value);
}

glm::vec3 PointOf(const std::array<uint32_t, 3>& bits)
{
	return {Bits(bits[0]), Bits(bits[1]), Bits(bits[2])};
}

struct AtomsDueCase
{
	int most;
	uint32_t lifetime;
	uint32_t dt;
	uint32_t due;
	uint32_t after;
};

struct AtomRateCase
{
	int most;
	uint32_t lifetime;
	bool comes;
};

struct SteamCase
{
	uint32_t size;
	std::array<uint32_t, 3> foot;
	std::array<uint32_t, 3> startRandom;
	uint32_t speed;
	std::array<uint32_t, 3> flingRandom;
	std::array<uint32_t, 3> start;
	std::array<uint32_t, 3> velocity;
};

struct SteamLandCase
{
	uint32_t x;
	uint32_t y;
	uint32_t z;
	uint32_t ground;
	uint32_t minHeight;
	uint32_t size;
	uint32_t after;
	uint32_t cellX;
	uint32_t cellZ;
};

// The original's own numbers: each rule's stretch run in the exe (herramientas/oracle), entered with its stack laid
// out by hand and its callees stubbed, at the game's 24-bit precision. 100 000 cases were run, every one matched.
// most, lifetime, dt, due before; due after (not a number: 0xFFC00000)
constexpr std::array<AtomsDueCase, 16> k_AtomsDueCases {{
    {0, 0x00000000, 0x00000000, 0x4955A818, 0xFFC00000},
    {-5, 0x00000000, 0x3D088889, 0x00000000, 0xFF800000},
    {20, 0x00000000, 0x3D088889, 0x40FC3C9C, 0x7F800000},
    {-5, 0xBF800000, 0x3E9A2EF8, 0x406BDB6A, 0x40A61C62},
    {-5, 0xC145E182, 0x00000000, 0x47CA313C, 0x47CA313C},
    {100, 0x3F9F2C3C, 0x3DCCCCCD, 0x40E89A17, 0x4174F756},
    {1, 0xBF800000, 0x3DCCCCCD, 0x41DF3070, 0x41DE63A3},
    {-5, 0xBF800000, 0x3DCCCCCD, 0x492AC7ED, 0x492AC7F5},
    {44, 0xBF800000, 0x3D088889, 0x4948B48F, 0x4948B478},
    {20, 0x40F7160A, 0x00000000, 0x00000000, 0x00000000},
    {0, 0x3FD48F5C, 0x00000000, 0x3FD4BE71, 0x3FD4BE71},
    {170, 0x0DA24260, 0x3DCCCCCD, 0x00000000, 0x735691F7},
    {100, 0xC180A749, 0x3D514253, 0x00000000, 0xBEA2A73E},
    {100, 0x40A00000, 0x3DCCCCCD, 0x00000000, 0x40000000},
    {-5, 0x00000000, 0x3D088889, 0x41E16755, 0xFF800000},
    {-5, 0x40B96DFA, 0x3D088889, 0x483EF383, 0x483EF381},
}};
// most, lifetime; whether the hearts come at all
constexpr std::array<AtomRateCase, 12> k_AtomRateCases {{
    {0, 0x40FCE175, false},
    {-5, 0x00000000, false},
    {0, 0x00000000, false},
    {20, 0xBF800000, false},
    {20, 0xBF849DD4, false},
    {0, 0xBF800000, false},
    {100, 0x0DA24260, true},
    {1, 0x40A00000, true},
    {20, 0x414624DF, true},
    {1, 0x4142EDF1, true},
    {72, 0x40A00000, true},
    {1, 0x0DA24260, true},
}};
// the creature's size, the foot, the random point for the start, the speed, the random point for the fling;
// the start and the velocity
constexpr std::array<SteamCase, 18> k_SteamCases {{
    {0x4030874C,
     {0x422E8072, 0x3F296F19, 0xC3E24DA5},
     {0x3EFD6D04, 0xBEF3E85A, 0xBE8FD130},
     0x41200000,
     {0x3E53B16B, 0x3E86D407, 0xBEE1B8F4},
     {0x423023DB, 0x3E890A38, 0xC3E26B65},
     {0x40B67852, 0x40E86E91, 0xC1429010}},
    {0x41EE59AD,
     {0x413D2871, 0xC20599E7, 0x45943E66},
     {0xBF35CED0, 0x3F183FAC, 0xBE8C041B},
     0x41200000,
     {0x3E945DB7, 0x3E84D1D8, 0xBE28021C},
     {0x40AF3024, 0xC1E0AD40, 0x45942AD8},
     {0x42ACABDF, 0x429A9405, 0xC2438804}},
    {0x410B1A2E,
     {0x45880ED3, 0x40D6FD18, 0x456FC3EB},
     {0xBE842FE2, 0xBF079D39, 0x3EE20232},
     0x41200000,
     {0x3F569597, 0xBEF7E836, 0x3CDA7806},
     {0x45880970, 0x40AAC68E, 0x456FD657},
     {0x4291BF76, 0xC228618C, 0x401462E4}},
    {0x3F800000,
     {0xC3CCDE28, 0xBE5B001C, 0x44B991B3},
     {0xBE61B2F8, 0x3EDFB57D, 0xBF0EA4E1},
     0x41200000,
     {0xBEC36C1E, 0x3F402857, 0xBCFFC2D8},
     {0xC3CCE69F, 0xBDA98CD6, 0x44B98C5A},
     {0xC0744726, 0x40F0326D, 0xBE9FD9C7}},
    {0x41E14645,
     {0x41B552ED, 0x45907F21, 0x4100C4D1},
     {0xBEDE94D5, 0x3B7B27A9, 0xBF4D052D},
     0x41200000,
     {0x3E60D7DC, 0x3F19CB95, 0xBF26A74E},
     {0x4197F1A4, 0x45907F63, 0x3FA42A94},
     {0x4277524C, 0x43292BAE, 0xC337507D}},
    {0x406C2E0F,
     {0xBFA1DADD, 0x4528E317, 0x3D9541C3},
     {0xBE4E44A4, 0xBEBA18AC, 0xBE259524},
     0x40C00000,
     {0x3F7224B7, 0xBE6743D8, 0xBE6AABA3},
     {0xBFBE6652, 0x4528DCA7, 0xBDD95FB5},
     {0x41A78C0F, 0xC0A0051D, 0xC0A26056}},
    {0x4188D6A5,
     {0x4242BCC4, 0x448C1C79, 0x44719B1E},
     {0x3DE4F1D9, 0x3F248856, 0x3BA23632},
     0x41232397,
     {0x3E7570F2, 0x3E41A8D9, 0x3F1958E8},
     {0x4245082C, 0x448C8602, 0x44719CBE},
     {0x422735E3, 0x4203EEF9, 0x42D0F0B6}},
    {0x416CF8FF,
     {0xC4D999B8, 0x44DA06EF, 0x41E66222},
     {0x3E982DEF, 0x3F45F358, 0x3F06797B},
     0x3F8147AE,
     {0xBE7B0F08, 0xBE85C9C7, 0xBEDF6504},
     {0xC4D96F75, 0x44DA74E0, 0x41F90E26},
     {0xC06AB8FC, 0xC07A2A5D, 0xC0D0DBCA}},
    {0x4119C8CE,
     {0x3FCD27D9, 0xC0A56C44, 0xC2408DCE},
     {0x3F086A8B, 0x3F0BDACD, 0x3E88F196},
     0x3F8147AE,
     {0x3EBBE697, 0x3EBCCA40, 0x3F329A85},
     {0x4048EA56, 0xC0660792, 0xC23D7810},
     {0x40640254, 0x40651695, 0x40D8BA38}},
    {0x41C12242,
     {0x4146E56F, 0x3FB90582, 0x40814331},
     {0x3E97343B, 0x3E9EBDCC, 0xBF59F5FF},
     0x40C00000,
     {0x3E8A5E6E, 0xBF607E9A, 0x3DFA87F1},
     {0x41691E38, 0x406C38C0, 0xC0081F1E},
     {0x421C9590, 0xC2FE0C3C, 0x418DC183}},
    {0x41DA02BB,
     {0x41E0301B, 0xC2330CD0, 0x4481D48F},
     {0x3D2C5E8C, 0x3F01602B, 0x3C412E83},
     0x40C00000,
     {0x3DE1BBED, 0x3E44F2E1, 0xBF435878},
     {0x41E2F0B3, 0xC2228606, 0x4481D7A5},
     {0x41902D54, 0x41FB9564, 0xC2F98921}},
    {0x41254A92,
     {0x448E0C80, 0x44063FE1, 0x407DF05D},
     {0xBECDA3ED, 0x3EB127E1, 0xBE84843C},
     0x40C00000,
     {0x3D895526, 0xBF6BE02A, 0xBD0947B4},
     {0x448DE4AB, 0x44068482, 0x404A9A11},
     {0x408501D4, 0xC2647249, 0xC004F4CE}},
    {0x3F800000,
     {0xC4D1A81D, 0xC131E44F, 0x44444DD6},
     {0xBE86BE0F, 0xBE73D1B5, 0x3F5402EB},
     0x40C00000,
     {0x3E0DA324, 0xBF33B156, 0xBF0A5630},
     {0xC4D1AAA4, 0xC13308E4, 0x44445DBD},
     {0x3F5474B6, 0xC086C500, 0xC04F8148}},
    {0x3F46C611,
     {0xC46F3323, 0x4515CA92, 0x45777451},
     {0xBF5533AD, 0x3EE2C879, 0x3E5D06B7},
     0x41200000,
     {0xBF158CE0, 0x3F4AF0B5, 0xBD2E3D71},
     {0xC46F3F8D, 0x4515CC39, 0x4577751F},
     {0xC0912655, 0x40C4F7FB, 0xBEA91CE9}},
    {0x41AF711B,
     {0x44F80EDB, 0xC1BB80DE, 0xC48EDC50},
     {0xBF10ABB1, 0x3E2C2D8C, 0xBEDAF38C},
     0x41200000,
     {0xBEC48317, 0xBEBFB6A6, 0x3EAD8A5A},
     {0x44F797E1, 0xC1B2A754, 0xC48F3658},
     {0xC2A8578B, 0xC2A43B33, 0x4294A9D0}},
    {0x3F800000,
     {0xC2063186, 0x451DA7E6, 0xC499632C},
     {0x3E3542F9, 0xBE3889FB, 0x3E8B4F60},
     0x40C00000,
     {0x3F0197BF, 0xBEF96225, 0x3E8E4295},
     {0xC205FB25, 0x451DA709, 0xC499608F},
     {0x4042639E, 0xC03B099C, 0x3FD563E0}},
    {0x3F800000,
     {0x43A61974, 0x44CD82EB, 0xC4CB2B72},
     {0xBD898F90, 0x3DABB2EA, 0x3F047A0F},
     0x3F8147AE,
     {0x3EF8E0A5, 0x3D8B2E6B, 0xBD559CAF},
     {0x43A616E0, 0x44CD83B9, 0xC4CB267A},
     {0x3EFB5DC5, 0x3D8C92B9, 0xBD57BF88}},
    {0x3F800000,
     {0x45793E61, 0xC14C262F, 0x423E9E8A},
     {0x3DD1561E, 0xBE35C67D, 0xBD766780},
     0x3F8147AE,
     {0xBF068907, 0xBF00BFF3, 0x3E99B1C8},
     {0x45793EDF, 0xC14D0050, 0x423E8C0F},
     {0xBF07E170, 0xBF02098C, 0x3E9B3B3D}},
}};
// x, y, z, the ground, the least height, the creature's size; the height after, and the map coordinates
constexpr std::array<SteamLandCase, 16> k_SteamLandCases {{
    {0xC4302AD0, 0x4280D1A4, 0x45361F0A, 0x4280D1A4, 0x4197978A, 0x3F800000, 0x42A6B786, 0xFFB9887A, 0x012364DE},
    {0xC4E2B56B, 0x3FD0E57C, 0x452F1495, 0x3FD0E57C, 0x40A00000, 0x403245F5, 0x4178F422, 0xFF4AA211, 0x011820EE},
    {0xC2144390, 0x4324D4DF, 0xBEB2DB65, 0x4324D4FC, 0x40A00000, 0x3F800000, 0x4329D4FC, 0xFFFC4B1D, 0xFFFFF70F},
    {0x44342599, 0xC0BCA676, 0xC41D5D3A, 0xC0BCA993, 0x00000000, 0x40361D74, 0xC0BCA676, 0x00480F0A, 0xFFC10DE9},
    {0x43025E9D, 0x40CBBBAB, 0x43E274DA, 0x40CBBBAB, 0x40A00000, 0x3F800000, 0x4135DDD6, 0x000D0976, 0x002D4A92},
    {0x44E38015, 0xBED2312E, 0x4516D444, 0xBED2312E, 0x41919079, 0x40433BF2, 0x425C61E2, 0x00B60011, 0x00F153A0},
    {0xC40A6AEB, 0xC174701A, 0xC19FCCFB, 0x00000000, 0x40A00000, 0x3F800000, 0x40A00000, 0xFFC8A209, 0xFFFE00A4},
    {0x43BCBD61, 0xC3050CF0, 0x44DE572E, 0xC073B3DB, 0x40A00000, 0x3E924C66, 0xC018441B, 0x0025BF79, 0x00B1DF58},
    {0xC4DAE047, 0xC301D952, 0x43FFF2F1, 0x42358496, 0x40A00000, 0x40752B7A, 0x42811116, 0xFF50E62D, 0x00333096},
    {0x42046FFB, 0x43086174, 0x44E6C892, 0xC0EFAF78, 0x418FECDF, 0x404ED7BE, 0x43086174, 0x00034F99, 0x00B8A075},
    {0xC3846192, 0x436E9F56, 0x451C9CF4, 0x00000000, 0x418457A4, 0x3F800000, 0x436E9F56, 0xFFE58616, 0x00FA94BA},
    {0x43D91BFA, 0x435811FF, 0x41B42795, 0x434CB30C, 0x00000000, 0x40489D28, 0x435811FF, 0x002B6BFE, 0x0002407E},
    {0x4591A9E0, 0x419E3D5B, 0x44B539BC, 0x00000000, 0x00000000, 0x3F800000, 0x419E3D5B, 0x01D21F9A, 0x0090FAFD},
    {0x410AE194, 0x4317B11D, 0x4536FEAA, 0xC0DCA37D, 0x403F5016, 0x3F800000, 0x4317B11D, 0x0000DE35, 0x0124CAAA},
    {0x455886F2, 0x411EBE1F, 0xC2430327, 0x00000000, 0x00000000, 0x3F800000, 0x411EBE1F, 0x015A7184, 0xFFFB1FEC},
    {0x41CA2269, 0x4369588E, 0xC266CC79, 0x430DA7BD, 0x40A00000, 0x40024F54, 0x4369588E, 0x000286D4, 0xFFFA3AE4},
}};

} // namespace

TEST(CreatureSpellMathsOracle, TheHeartsAndTheSteamFallDueAsTheOriginalReckonsThem)
{
	for (const auto& c : k_AtomsDueCases)
	{
		const float after = particles::maths::AtomsDue(Bits(c.due), c.most, Bits(c.lifetime), Bits(c.dt));
		if (std::isnan(Bits(c.after)))
		{
			EXPECT_TRUE(std::isnan(after)) << c.most << " " << c.lifetime;
			continue;
		}
		EXPECT_EQ(BitsOf(after), c.after) << c.most << " " << c.lifetime << " " << c.dt << " " << c.due;
	}
	for (const auto& c : k_AtomRateCases)
	{
		EXPECT_EQ(particles::maths::HasAtomRate(c.most, Bits(c.lifetime)), c.comes) << c.most << " " << c.lifetime;
	}
}

TEST(CreatureSpellMathsOracle, TheSteamStartsAboutAFootAndIsFlungAsTheOriginalReckonsIt)
{
	// The first twelve come out a bit off with the spread taken as 0.3 rather than the original's constant
	for (const auto& c : k_SteamCases)
	{
		const auto start = particles::maths::SteamStart(PointOf(c.foot), PointOf(c.startRandom), Bits(c.size));
		const auto velocity = particles::maths::FlungVelocity(PointOf(c.flingRandom), Bits(c.speed), Bits(c.size));
		for (int i = 0; i < 3; ++i)
		{
			EXPECT_EQ(BitsOf(start[i]), c.start.at(i)) << c.foot[0] << " part " << i;
			EXPECT_EQ(BitsOf(velocity[i]), c.velocity.at(i)) << c.flingRandom[0] << " part " << i;
		}
	}
}

TEST(CreatureSpellMathsOracle, TheSteamIsKeptOffTheLandAsTheOriginalReckonsIt)
{
	for (const auto& c : k_SteamLandCases)
	{
		const glm::vec3 position(Bits(c.x), Bits(c.y), Bits(c.z));
		const auto cell = psys::object_surface::LandTestCell(position);
		EXPECT_EQ(static_cast<uint32_t>(cell.x), c.cellX) << c.x;
		EXPECT_EQ(static_cast<uint32_t>(cell.y), c.cellZ) << c.z;
		const auto raised = particles::maths::SteamOffLand(Bits(c.y), Bits(c.ground), Bits(c.minHeight), Bits(c.size));
		EXPECT_EQ(BitsOf(raised.value_or(Bits(c.y))), c.after) << c.y << " " << c.ground;
	}
	// A height that is not a number is raised all the same
	EXPECT_EQ(particles::maths::SteamOffLand(std::numeric_limits<float>::quiet_NaN(), 2.0f, 0.5f, 2.0f), 3.0f);
}

TEST_F(CreatureSpellParticlesTest, HeartsComeOffWhatTheirAtomStandsForFourASecond)
{
	auto effect = Make(First("0") + k_Beside + Hearts() + k_Point + k_Sprite);
	// Twenty over five seconds: half a heart due each step
	effect->Step(k_Step);
	ASSERT_EQ(SpriteAtoms(*effect).size(), 1u);
	effect->Step(k_Step);
	EXPECT_EQ(SpriteAtoms(*effect).size(), 1u);
	Steps(*effect, 6);
	const auto hearts = SpriteAtoms(*effect);
	ASSERT_EQ(hearts.size(), 4u);
	for (const auto* heart : hearts)
	{
		ASSERT_NE(heart->collection->parent, nullptr);
		// Where the atom it hangs off stands, its own size times the creature's, flung off at six times that size
		EXPECT_EQ(heart->position, effect->GlobalPosition(*heart->collection->parent));
		EXPECT_FLOAT_EQ(heart->baseScale, 1.5f * 2.0f);
		EXPECT_LE(glm::length(heart->velocity), 6.0f * 2.0f + k_Epsilon);
	}
}

TEST_F(CreatureSpellParticlesTest, NoMoreHeartsThanMaxAtomsAndEachGoesOnceOld)
{
	auto effect = Make(First("0") + k_Beside + Hearts("2", "1") + k_Point + k_Sprite);
	size_t most = 0;
	for (int i = 0; i < 40; ++i)
	{
		effect->Step(k_Step);
		const auto hearts = SpriteAtoms(*effect);
		most = std::max(most, hearts.size());
		for (const auto* heart : hearts)
		{
			EXPECT_LE(effect->AtomAge(*heart), 1.0f + k_Step);
		}
	}
	EXPECT_EQ(most, 2u);
}

TEST_F(CreatureSpellParticlesTest, NoHeartComesOnceTheEffectCloses)
{
	auto effect = Make(First("0") + k_Beside + Hearts() + k_Point + k_Sprite);
	effect->Step(k_Step);
	ASSERT_EQ(SpriteAtoms(*effect).size(), 1u);
	effect->CloseDown();
	Steps(*effect, 8);
	EXPECT_EQ(SpriteAtoms(*effect).size(), 1u);
}

TEST_F(CreatureSpellParticlesTest, TheCreaturesOwnAtomGivesTheHeartsNoPointYet)
{
	// Under the first rule's atom the hearts would come from the creature's body, which is not ported: none is made
	auto effect = Make(First("1 4") + Hearts() + k_Point + k_Sprite);
	Steps(*effect, 8);
	EXPECT_TRUE(SpriteAtoms(*effect).empty());
	EXPECT_EQ(Firsts(*effect).size(), 1u);
	EXPECT_FALSE(effect->Finished());
}

TEST_F(CreatureSpellParticlesTest, TheHeartsLetGoWithoutARateOrACreature)
{
	auto none = Make(First("0") + k_Beside + Hearts("0") + k_Point + k_Sprite);
	Steps(*none, 8);
	EXPECT_TRUE(SpriteAtoms(*none).empty());
	bodies->gone = true;
	auto gone = Make(First("0") + k_Beside + Hearts() + k_Point + k_Sprite);
	Steps(*gone, 8);
	EXPECT_TRUE(SpriteAtoms(*gone).empty());
}

TEST_F(CreatureSpellParticlesTest, SteamComesOffEitherFootKeptOffTheLand)
{
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::reset();
	auto effect = Make(First("1 1") + Steam("20") + k_Point + k_Sprite);
	Steps(*effect, 8);
	const auto steam = SpriteAtoms(*effect);
	ASSERT_EQ(steam.size(), 4u);
	const auto& feet = bodies->body.bones;
	for (const auto* puff : steam)
	{
		// Within the spread (0.3 of the creature's size of 2) of one foot or the other, across the land
		const glm::vec2 at(puff->position.x, puff->position.z);
		const auto offRight = glm::distance(at, glm::vec2(feet[0].x, feet[0].z));
		const auto offLeft = glm::distance(at, glm::vec2(feet[1].x, feet[1].z));
		EXPECT_LE(std::min(offRight, offLeft), 0.6f + k_Epsilon);
		// The feet stand on the land, so every puff is raised to its least height times the creature's size
		EXPECT_EQ(puff->position.y, 1.0f);
		EXPECT_FLOAT_EQ(puff->baseScale, 1.5f * 2.0f);
		EXPECT_LE(glm::length(puff->velocity), 1.01f * 2.0f + k_Epsilon);
	}
}

TEST_F(CreatureSpellParticlesTest, TheFreezesOwnFileMakesNoSteam)
{
	// Its file makes no atoms at all: the rule goes on, letting nothing out
	auto effect = Make(First("1 1") + Steam("0") + k_Point + k_Sprite);
	Steps(*effect, 8);
	EXPECT_TRUE(SpriteAtoms(*effect).empty());
	EXPECT_FALSE(effect->Finished());
}

TEST_F(CreatureSpellParticlesTest, ACreatureWithNoFeetLetsNoSteamOut)
{
	bodies->body.rightFoot.reset();
	auto effect = Make(First("1 1") + Steam("20") + k_Point + k_Sprite);
	Steps(*effect, 8);
	EXPECT_TRUE(SpriteAtoms(*effect).empty());
}

namespace
{
/// The game's creature bodies on a registry of their own
class CreatureSpellBodiesTest: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static glm::mat4 At(const glm::vec3& place)
	{
		glm::mat4 bone(1.0f);
		bone[3] = glm::vec4(place, 1.0f);
		return bone;
	}

	static entt::entity MakeCreature(const glm::vec3& place, float size)
	{
		const auto entity = Reg().Create();
		Reg().Assign<ecs::components::Transform>(entity, place, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<ecs::components::Creature>(entity).size = size;
		return entity;
	}

	ecs::systems::CreatureSpellBodies bodies;

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};
} // namespace

TEST_F(CreatureSpellBodiesTest, ACreatureStandsWhereItIsAtItsSizeWithTheBonesOfTheTurnsPose)
{
	const auto creature = MakeCreature({10.0f, 2.0f, 30.0f}, 1.5f);
	auto body = bodies.CreatureBody(creature);
	ASSERT_TRUE(body.has_value());
	EXPECT_EQ(body->origin, glm::vec3(10.0f, 2.0f, 30.0f));
	EXPECT_FLOAT_EQ(body->size, 1.5f);
	EXPECT_TRUE(body->bones.empty());
	EXPECT_FLOAT_EQ(body->invisible, 0.0f);
	auto& pose = Reg().Assign<ecs::components::CreatureTurnPose>(creature);
	pose.current = {At({1.0f, 2.0f, 3.0f}), At({4.0f, 5.0f, 6.0f})};
	pose.previous = {At({0.0f, 0.0f, 0.0f}), At({0.0f, 0.0f, 0.0f})};
	body = bodies.CreatureBody(creature);
	ASSERT_TRUE(body.has_value());
	EXPECT_EQ(body->bones, (std::vector<glm::vec3> {{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}}));
	// Held off the game by the hand, the pose kept is the one read
	pose.frozen = std::vector<glm::mat4> {At({7.0f, 8.0f, 9.0f})};
	body = bodies.CreatureBody(creature);
	ASSERT_TRUE(body.has_value());
	EXPECT_EQ(body->bones, (std::vector<glm::vec3> {{7.0f, 8.0f, 9.0f}}));
}

TEST_F(CreatureSpellBodiesTest, TheInvisibleSpellsShareIsItsSlots)
{
	const auto creature = MakeCreature({0.0f, 0.0f, 0.0f}, 1.0f);
	auto& spells = Reg().Assign<ecs::components::CreatureSpells>(creature).spells;
	spells[creature_spells::Spell::Invisible].phase = creature_spells::Phase::Holding;
	EXPECT_FLOAT_EQ(bodies.CreatureBody(creature)->invisible, 1.0f);
	spells[creature_spells::Spell::Invisible].phase = creature_spells::Phase::Off;
	EXPECT_FLOAT_EQ(bodies.CreatureBody(creature)->invisible, 0.0f);
}

TEST_F(CreatureSpellBodiesTest, OnlyACreatureHasABodyAndOnlyWhileItIsAvailable)
{
	const auto thing = Reg().Create();
	Reg().Assign<ecs::components::Transform>(thing, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_FALSE(bodies.CreatureBody(thing).has_value());
	EXPECT_FALSE(bodies.CreatureBody(entt::null).has_value());
	EXPECT_FALSE(bodies.IsAvailable(entt::null));
	const auto creature = MakeCreature({0.0f, 0.0f, 0.0f}, 1.0f);
	EXPECT_TRUE(bodies.IsAvailable(creature));
	Reg().Assign<ecs::components::Unavailable>(creature);
	EXPECT_FALSE(bodies.IsAvailable(creature));
}
