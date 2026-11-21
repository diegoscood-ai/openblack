/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstring>

#include <array>
#include <bit>
#include <memory>
#include <string>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "ECS/SavedWorld/ParticleCodecs.h"
#include "Particles/AtomSavedDraw.h"
#include "Particles/BeamData.h"
#include "Particles/Creators/Chain.h"
#include "Particles/Creators/Mesh.h"
#include "Particles/Creators/Mist.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/Rules/Fireball.h"
#include "Particles/Rules/ManaPath.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;

namespace
{
float F(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

/// A drawn matrix of no size at the place, as the trail's sprites keep it while they are not grown yet
particles::saved_draw::SavedDraw Unscaled(uint32_t x, uint32_t y, uint32_t z)
{
	particles::saved_draw::SavedDraw draw;
	draw.matrix = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, F(x), F(y), F(z)};
	draw.scale = 0.0f;
	draw.alpha = 0;
	return draw;
}

template <size_t N>
std::array<uint8_t, N> Bytes(const char* hex)
{
	std::array<uint8_t, N> bytes {};
	for (size_t i = 0; i < N; ++i)
	{
		const auto nibble = [](char c) { return static_cast<uint8_t>(c <= '9' ? c - '0' : c - 'A' + 10); };
		bytes.at(i) = static_cast<uint8_t>((nibble(hex[2 * i]) << 4U) | nibble(hex[2 * i + 1]));
	}
	return bytes;
}
} // namespace

// An atom's values as the original saves them, from one of the physical shield's trail sprites in profile D's slot 20
// Land 2 save: its place, colour, flags (stepped twice and in use, the unset bits 0xCD), its two drawn matrices with
// their three unset bytes, its animation (rate 22, one frame, not playing, looping) and its random byte. The animation
// info's last two bytes are stack memory the original never sets (FF FF in that file): one made in play writes 0xCD
TEST(ParticleCodecs, AnAtomsValuesAreWrittenAsTheOriginalSavesThem)
{
	particles::Creator creator;
	creator.kind = particles::Creator::Kind::Sprite;
	creator.className = "ParticleSpriteCreator";
	creator.numFrames = 1;
	creator.loopAnim = true;
	particles::Atom atom;
	atom.creator = &creator;
	atom.position = glm::vec3(F(0x4561816C), F(0x41103D8B), F(0x4557AFC1));
	atom.baseScale = 1.0f;
	atom.ruleScale = 0.0f;
	atom.colour = {0xFE, 0x45, 0x45, 0x00};
	atom.steps = 2;
	atom.visible = true;
	atom.frameRate = 22.0f;
	atom.playAnim = false;
	atom.random = 125;
	atom.savedDraw[0] = Unscaled(0x45617FED, 0x410FC4A4, 0x4557B017);
	atom.savedDraw[1] = Unscaled(0x4561816C, 0x41103D8B, 0x4557AFC1);

	const auto record = particle_codecs::AtomCoreValues(atom);
	EXPECT_EQ(record.point, (gamesave::Point {0.0f, 0.0f, 0.0f}));
	EXPECT_EQ(record.value, 0.0f);
	EXPECT_EQ(record.zoomer, Bytes<48>("0000803F0000000000000000000000000000803F000000000000000000000000"
	                                   "0000803F000000000000000000000000"));
	EXPECT_EQ(record.floats, (std::array<float, 3> {1.0f, 0.0f, 1.0f}));
	EXPECT_EQ(record.secondPoint, (gamesave::Point {F(0x4561816C), F(0x41103D8B), F(0x4557AFC1)}));
	EXPECT_EQ(record.word, 16663877u);
	EXPECT_EQ(record.secondWord, 0u);
	EXPECT_EQ(record.half, 52692u);
	EXPECT_EQ(record.drawPosition, Bytes<56>("0000000000000000000000000000000000000000000000000000000000000000"
	                                         "00000000ED7F6145A4C40F4117B057450000000000CDCDCD"));
	EXPECT_EQ(record.secondDrawPosition, Bytes<56>("0000000000000000000000000000000000000000000000000000000000000000"
	                                               "000000006C8161458B3D1041C1AF57450000000000CDCDCD"));
	EXPECT_EQ(record.lastFloat, 0.0f);
	EXPECT_EQ(record.thirdWord, 0u);
	EXPECT_EQ(record.animation, Bytes<12>("0000B041010000000001CDCD"));
	EXPECT_EQ(record.fourthWord, std::bit_cast<uint32_t>(1.0f));
	EXPECT_EQ(record.byte, 125u);
}

// The flags word: drawn on the first update, the steps, deflected and in use, over the unset bits
TEST(ParticleCodecs, AnAtomsFlagsAreItsStepsAndStatesOverTheUnsetBits)
{
	particles::Atom atom;
	atom.visible = false;
	EXPECT_EQ(particle_codecs::AtomFlags(atom), 0xCDC0u);
	atom.steps = 1;
	atom.drawOnFirstUpdate = true;
	atom.flags = 0x08;
	atom.visible = true;
	EXPECT_EQ(particle_codecs::AtomFlags(atom), 0xCDDBu);
}

// A collection's values as the original saves them, from the same trail's collection: the trail rule by its place in
// the physical shield's file, its birth, group, flags (its own coordinates, drawn interpolated, the unset bits 0xCC),
// no frame passed down and its alpha; a collection under an ancestor that passes its frame down clears the first bit
TEST(ParticleCodecs, ACollectionsValuesAreWrittenAsTheOriginalSavesThem)
{
	struct Rule final: particles::Modifier
	{
	};
	Rule trail;
	trail.fileIndex = 12;
	Rule detached;
	detached.fileIndex = 3;
	particles::Collection collection;
	collection.group = 1;
	collection.flags = 3;
	collection.alpha = 255.0f;
	collection.modifiers = {{.modifier = &trail}, {.modifier = &detached, .attached = false}};
	const auto record = particle_codecs::CollectionValues(collection, 67, false);
	ASSERT_EQ(record.persistents.elements.size(), 1u);
	EXPECT_EQ(record.persistents.elements[0], (gamesave::PersistentId {.first = 67, .second = 12}));
	EXPECT_EQ(record.value, 0.0f);
	EXPECT_EQ(record.word, 1u);
	EXPECT_EQ(record.byte, 207u);
	EXPECT_EQ(record.lastWord, 0u);
	EXPECT_EQ(record.lastByte, 255u);

	collection.hierarchy = true;
	const auto inFrame = particle_codecs::CollectionValues(collection, 116, true);
	EXPECT_EQ(inFrame.byte, 206u);
	EXPECT_EQ(inFrame.lastWord, 0xFFFFFFFEu);

	// openblack's own emitting flag (0x04, the town belief's sparkles) is not the original's: the byte's bit 2 is unset
	// memory in every saved collection, so the byte is the same with it or without it (and a load must not read that
	// bit as emitting)
	collection.flags = 3 | 0x04;
	EXPECT_EQ(particle_codecs::CollectionValues(collection, 116, true).byte, 206u);
	collection.flags = 3;
	EXPECT_EQ(particle_codecs::CollectionValues(collection, 116, true).byte, 206u);
}

// An emitter's data as the original saves it, from the bonfire's glow emitter in profile D's slot 20 Land 2 save: the
// rule by its place in the bonfire's file, the next emission time, the count emitted, the first update done
TEST(ParticleCodecs, AnEmittersDataIsWrittenAsTheOriginalSavesIt)
{
	struct Rule final: particles::Modifier
	{
	};
	Rule glow;
	glow.fileIndex = 13;
	particles::Collection::Slot slot {.modifier = &glow};
	slot.state.x = std::bit_cast<float>(1155228771u);
	slot.state.y = 3491.0f;
	slot.first = false;
	const auto record = particle_codecs::EmitterDataValues(slot, 116);
	EXPECT_EQ(record.persistent, (gamesave::PersistentId {.first = 116, .second = 13}));
	EXPECT_EQ(record.words, (std::array<uint32_t, 2> {1155228771u, 3491u}));
	EXPECT_EQ(record.byte, 0u);
}

// An emitter's data is made on its first use, as the original's, and the collection keeps the order its rules' data
// was made in
TEST(ParticleCodecs, AnEmitterNotesItsDataAsMadeOnItsFirstUse)
{
	const game_random::testing::ScopedState state;
	// every draw half way: the emitted point is drawn in the ball until one falls inside it, which all-zero draws (the
	// corner of the cube) never give
	game_random::testing::SetGameRand([](uint32_t n) { return n / 2; }, [](float range) { return range * 0.5f; });
	const std::string text = "BEGINPROPERTIES\n"
	                         "PROPERTY DeleteOnCloseDown BOOL 0\n"
	                         "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
	                         "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
	                         "ENDPROPERTIES\n"
	                         "BEGINCLASS ParticlePointCreator P\n"
	                         "BEGINPROPERTIES\n"
	                         "ENDPROPERTIES\n"
	                         "ENDCLASS\n"
	                         "BEGINCLASS EmitterRuleSimple E\n"
	                         "BEGINPROPERTIES\n"
	                         "PROPERTY Group INTEGER 0\n"
	                         "PROPERTY NextGroups ARRAY SIZE 0\n"
	                         "PROPERTY PCreator PERSIS_PNTR P\n"
	                         "PROPERTY EmissionFreq FLOAT 1\n"
	                         "ENDPROPERTIES\n"
	                         "ENDCLASS\n";
	auto file = particles::File::Parse(text, "SF_EmitterDataTest");
	ASSERT_TRUE(file.has_value());
	particles::Effect effect(std::make_shared<const particles::File>(std::move(*file)), glm::vec3(0.0f), 1.0f,
	                         game_random::particles::NetGameType::Synced);
	ASSERT_EQ(effect.GetRoots().size(), 1u);
	const auto& root = *effect.GetRoots().front();
	EXPECT_TRUE(root.dataMade.empty());
	effect.Step(0.1f);
	ASSERT_EQ(root.dataMade.size(), 1u);
	EXPECT_EQ(root.dataMade.front()->SavedData(), particles::RuleData::Emitter);
	EXPECT_EQ(root.dataMade.front()->fileIndex, 1u);
}

// A trail's data as the original saves it, from one of the physical shield's trails in profile D's slot 20 Land 2
// save: the rule by its place in the file, the first update done, the count of places and each place, the newest
// first, as its position, scale and turn (here the first two of the seventy it keeps)
TEST(ParticleCodecs, ATrailsDataIsWrittenAsTheOriginalSavesIt)
{
	struct Rule final: particles::Modifier
	{
	};
	Rule trail;
	trail.fileIndex = 12;
	particles::Collection::Slot slot {.modifier = &trail};
	slot.first = false;
	const std::vector<particles::fireball_rules::TrailPlace> places {
	    {.position = glm::vec3(F(0x4561CCFD), F(0x4122ACE7), F(0x4557BB1F)), .scale = F(0x3FA91D73)},
	    {.position = glm::vec3(F(0x4561CB97), F(0x41230680), F(0x4557BA57)), .scale = F(0x3FA91D73)},
	};
	const auto record = particle_codecs::TrailDataValues(slot, 67, places);
	EXPECT_EQ(record.persistent, (gamesave::PersistentId {.first = 67, .second = 12}));
	EXPECT_EQ(record.byte, 0u);
	EXPECT_EQ(record.word, 2u);
	ASSERT_EQ(record.places.values.size(), 2u);
	EXPECT_EQ(record.places.values[0], Bytes<64>("FDCC6145E7AC22411FBB5745731DA93F0000803F000000000000000000000000"
	                                             "0000803F0000000000000000000000000000803F000000000000000000000000"));
	EXPECT_EQ(record.places.values[1], Bytes<64>("97CB61458006234157BA5745731DA93F0000803F000000000000000000000000"
	                                             "0000803F0000000000000000000000000000803F000000000000000000000000"));
}

// The hand's sprinkle's, the atoms-at-a-target rule's and an atom's sound rule's data as the original saves them, from
// profile D's slot 20 Land 2 save: the water's sprinkle (rule 11 of the water's file, its first update done, its age
// 5.4 s), the physical shield's atoms at its target (rule 6, taken at turn 17553) and its sound atom (rule 2, its sound
// started)
TEST(ParticleCodecs, TheSprinklesTheTargetAtomsAndTheSoundAtomsDataAreWrittenAsTheOriginalSavesThem)
{
	struct Rule final: particles::Modifier
	{
	};
	Rule sprinkle;
	sprinkle.fileIndex = 11;
	particles::Collection::Slot sprinkleSlot {.modifier = &sprinkle};
	sprinkleSlot.first = false;
	sprinkleSlot.extra.x = std::bit_cast<float>(1085066439u);
	const auto water = particle_codecs::HandSprinkleDataValues(sprinkleSlot, 16);
	EXPECT_EQ(water.persistent, (gamesave::PersistentId {.first = 16, .second = 11}));
	EXPECT_EQ(water.byte, 0u);
	EXPECT_EQ(water.word, 1085066439u);

	Rule atTarget;
	atTarget.fileIndex = 6;
	particles::Collection::Slot targetSlot {.modifier = &atTarget};
	targetSlot.extra.x = std::bit_cast<float>(17553u);
	const auto target = particle_codecs::TargetAtomsDataValues(targetSlot, 67);
	EXPECT_EQ(target.persistent, (gamesave::PersistentId {.first = 67, .second = 6}));
	EXPECT_EQ(target.word, 17553u);

	Rule sound;
	sound.fileIndex = 2;
	particles::Atom atom;
	const auto silent = particle_codecs::SoundAtomDataValues(atom, sound, 67);
	EXPECT_EQ(silent.byte, 0u);
	atom.data[&sound].x = 1.0f;
	const auto started = particle_codecs::SoundAtomDataValues(atom, sound, 67);
	EXPECT_EQ(started.persistent, (gamesave::PersistentId {.first = 67, .second = 2}));
	EXPECT_EQ(started.byte, 1u);
}

// A sprite as the original saves it, from the physical shield's trail sprites in profile D's slot 20 Land 2 save:
// drawn, its creator's alpha 255, its creator (object 5 of the file), its file offset 34, its flags 0 over the unset
// high bits and, not stretched, both scales 1; a stretched one keeps the stretch of its last two steps
TEST(ParticleCodecs, ASpriteIsWrittenAsTheOriginalSavesIt)
{
	particles::Creator creator;
	creator.kind = particles::Creator::Kind::Sprite;
	creator.className = "ParticleSpriteCreator";
	creator.fileIndex = 5;
	creator.scaleAlpha = 255;
	creator.fileOffset = 34;
	particles::Atom atom;
	atom.creator = &creator;
	atom.steps = 2;
	const auto record = particle_codecs::SpriteValues(atom, 67);
	EXPECT_EQ(record.renderByte, 1u);
	EXPECT_EQ(record.byte, 255u);
	EXPECT_EQ(record.persistent, (gamesave::PersistentId {.first = 67, .second = 5}));
	EXPECT_EQ(record.bytes, (std::array<uint8_t, 2> {34, 192}));
	EXPECT_EQ(record.words, (std::array<uint32_t, 2> {std::bit_cast<uint32_t>(1.0f), std::bit_cast<uint32_t>(1.0f)}));

	creator.stretch = 2.0f;
	creator.useLandscapeColour = true;
	atom.previous.stretch = 1.5f;
	atom.current.stretch = 2.0f;
	const auto stretched = particle_codecs::SpriteValues(atom, 67);
	EXPECT_EQ(stretched.bytes[1], 0xCAu);
	EXPECT_EQ(stretched.words, (std::array<uint32_t, 2> {std::bit_cast<uint32_t>(1.5f), std::bit_cast<uint32_t>(2.0f)}));
	atom.steps = 1;
	EXPECT_EQ(particle_codecs::SpriteValues(atom, 67).words[0], std::bit_cast<uint32_t>(1.0f));
}

// A mist as the original saves it, from the water's cloud in profile D's slot 20 Land 2 save: drawn, its creator
// (object 0 of the water's file), its initial scale 0.2, no ratio from the matrix, 0, then, with no light map stamp
// drawn, its place unset and its alpha 1; a mist that has drawn its stamp keeps the stamp's place and alpha
TEST(ParticleCodecs, AMistIsWrittenAsTheOriginalSavesIt)
{
	particles::MistCreator creator;
	creator.className = "ParticleMistCreator";
	creator.fileIndex = 0;
	creator.initialScale = std::bit_cast<float>(0x3E4CCCCDu);
	particles::Atom atom;
	atom.creator = &creator;
	const auto record = particle_codecs::MistValues(atom, 16);
	EXPECT_EQ(record.renderByte, 1u);
	EXPECT_EQ(record.persistent, (gamesave::PersistentId {.first = 16, .second = 0}));
	EXPECT_EQ(record.word, 0x3E4CCCCDu);
	EXPECT_EQ(record.byte, 0u);
	EXPECT_EQ(record.half, 0u);
	for (const float cell : record.point)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(cell), 0xCDCDCDCDu);
	}
	EXPECT_EQ(record.lastWord, std::bit_cast<uint32_t>(1.0f));

	atom.mistStampPlace = glm::vec3(10.0f, 0.0f, 20.0f);
	atom.mistStampAlpha = 0.5f;
	const auto stamped = particle_codecs::MistValues(atom, 16);
	EXPECT_EQ(stamped.point, (gamesave::Point {10.0f, 0.0f, 20.0f}));
	EXPECT_EQ(stamped.lastWord, std::bit_cast<uint32_t>(0.5f));
}

// The animation of an atom whose creator gives none, as the physical shield's sound atom (a point) and the water's
// cloud (a mist, not looping) hold it in profile D's slot 20 Land 2 save: rate 1, one frame, not playing, not looping
TEST(ParticleCodecs, AnAtomOfAPointOrAMistHasTheAnimationTheOriginalGivesIt)
{
	particles::Creator point;
	point.kind = particles::Creator::Kind::Point;
	point.className = "ParticlePointCreator";
	particles::Atom atom;
	atom.creator = &point;
	EXPECT_EQ(particle_codecs::AnimInfoBytes(atom), Bytes<12>("0000803F010000000000CDCD"));
	particles::MistCreator mist;
	mist.className = "ParticleMistCreator";
	mist.loopAnim = false;
	atom.creator = &mist;
	EXPECT_EQ(particle_codecs::AnimInfoBytes(atom), Bytes<12>("0000803F010000000000CDCD"));
	mist.loopAnim = true;
	EXPECT_EQ(particle_codecs::AnimInfoBytes(atom), Bytes<12>("0000803F010000000001CDCD"));
}

// An animated textured mesh as the original saves it, from the water's rain cone in profile D's slot 20 Land 2 save:
// drawn, its creator (object 5 of the water's file), never clipped over the unset high bits, its 256 by 256 texture,
// sliding along V, 0, and the thousand frames of a sliding texture
TEST(ParticleCodecs, AnAnimatedTexturedMeshIsWrittenAsTheOriginalSavesIt)
{
	particles::MeshCreator creator;
	creator.className = "ParticleMeshCreatorAnimTextured";
	creator.animTextured = true;
	creator.fileIndex = 5;
	creator.neverClip = true;
	creator.textureHeight = 256;
	creator.textureWidth = 256;
	creator.slideV = true;
	particles::Atom atom;
	atom.creator = &creator;
	const auto record = particle_codecs::TexturedObjectValues(atom, 16);
	EXPECT_EQ(record.renderByte, 1u);
	EXPECT_EQ(record.persistent, (gamesave::PersistentId {.first = 16, .second = 5}));
	EXPECT_EQ(record.byte, 201u);
	EXPECT_EQ(record.words, (std::array<uint32_t, 2> {256, 256}));
	EXPECT_EQ(record.bytes, (std::array<uint8_t, 3> {0, 1, 0}));
	EXPECT_EQ(record.lastWord, 1000u);
}

// A particle's sound as the original saves it, from the physical shield's sound atom in profile D's slot 20 Land 2
// save: the count of the particles' sounds alive (two), its action (111, surface 1, size 2, alignment 2, no fade,
// looping with a soft release), no delay, not released
TEST(ParticleCodecs, AParticlesSoundIsWrittenAsTheOriginalSavesIt)
{
	audio::ParticleSound sound;
	sound.action = {.action = 111,
	                .surface = 1,
	                .size = 2,
	                .alignment = 2,
	                .fadeStep = 0,
	                .flags = particles::SoundAction::k_Looping | particles::SoundAction::k_SoftRelease};
	const auto record = particle_codecs::ParticleSoundValues(sound, 2);
	EXPECT_EQ(record.word, 2u);
	EXPECT_EQ(record.action, Bytes<24>("6F000000010000000200000002000000000000000500000"
	                                   "0"));
	EXPECT_EQ(record.secondWord, 0u);
	EXPECT_EQ(record.byte, 0u);
}

// A draw offset that follows the hand is written as its one atom's weight and point, and read back the same
TEST(ParticleCodecs, AHandDrawOffsetIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Atom::HandDrawOffset offset;
	offset.SetReference(glm::vec3(1.5f, -2.0f, 3.25f), 0.75f);
	const auto record = particle_codecs::HandDrawOffsetValues(offset);
	EXPECT_EQ(record.references, 1u);
	EXPECT_EQ(record.amount, 0.75f);
	EXPECT_EQ(record.point, (gamesave::Point {1.5f, -2.0f, 3.25f}));
	const auto read = particle_codecs::HandDrawOffsetOf(record);
	EXPECT_EQ(read.reference, offset.reference);
	EXPECT_EQ(read.weight, offset.weight);
}

// A decaying draw offset is written as the original keeps it between two frames: the original steps it after each
// frame's draw, openblack before the next one, so the age and the offset now take the last frame's ms; its decay rate is
// 0.5. Read back, it has no ms still to take, and the next frame draws the same offset and goes on as before
TEST(ParticleCodecs, ADecayingDrawOffsetIsWrittenAsTheOriginalKeepsItBetweenTwoFrames)
{
	auto offset = particles::Atom::DecayingDrawOffset::From(glm::vec3(4.0f, 0.0f, -2.0f));
	// made: the first frame draws it whole
	auto made = particle_codecs::DecayingDrawOffsetValues(offset, 2);
	EXPECT_EQ(made.references, 2u);
	EXPECT_EQ(made.start, (gamesave::Point {4.0f, 0.0f, -2.0f}));
	EXPECT_EQ(made.current, (gamesave::Point {4.0f, 0.0f, -2.0f}));
	EXPECT_EQ(made.age, 0.0f);
	EXPECT_EQ(made.rate, 0.5f);
	// two frames of 16 and 33 ms drawn: the second's ms are on the age already in the original
	offset.Step(16);
	offset.Step(33);
	const auto record = particle_codecs::DecayingDrawOffsetValues(offset, 1);
	const float age = static_cast<float>(33) * 0.001f + static_cast<float>(16) * 0.001f;
	const float k = 1.0f - age * 0.5f;
	EXPECT_EQ(record.age, age);
	EXPECT_EQ(record.current, (gamesave::Point {k * 4.0f, k * 0.0f, k * -2.0f}));
	auto read = particle_codecs::DecayingDrawOffsetOf(record);
	EXPECT_EQ(read.pendingMs, 0u);
	auto going = offset;
	going.Step(20);
	read.Step(20);
	EXPECT_EQ(read.current, going.current);
	EXPECT_EQ(read.age, going.age);
	EXPECT_EQ(particle_codecs::DecayingDrawOffsetValues(read, 1), particle_codecs::DecayingDrawOffsetValues(going, 1));
}

// A chain as the original saves it, from the first of the magic beam's six chains in the frozen slot 0 save (the
// SF_SimpleBeam file, particle type 119): its creator (object 3 of the file), a word left unset, the creator's frame
// height 256 and width 64, head and tail frames 0, the word 4 it is made with, the creator's four repeats, file offset 0
// and no dynamic lighting, its v-scroll, its twenty joints as the last frame drew them (here the last two, faded out:
// white under alpha 0), its v-scroll rate 1 and drawn; read back, the same
TEST(ParticleCodecs, AChainIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::ChainCreator creator;
	creator.kind = particles::Creator::Kind::Chain;
	creator.fileIndex = 3;
	creator.frameHeight = 256;
	creator.frameWidth = 64;
	creator.numTexturesForWholeChain = 4;
	particles::Collection collection;
	collection.chainCreator = &creator;
	const std::array<std::array<uint8_t, 28>, 2> saved {
	    Bytes<28>("9BEF0D4591FBDC42F9893145E85C753EFFFFFF000000000001000000"),
	    Bytes<28>("73F20D45F15FDC42BF8B3145CDCCCC3DFFFFFF000000000001000000"),
	};
	for (const auto& bytes : saved)
	{
		collection.chainJoints.push_back(particle_codecs::ChainJointOf(bytes));
	}
	EXPECT_EQ(collection.chainJoints[1].position, glm::vec3(F(0x450DF273), F(0x42DC5FF1), F(0x45318BBF)));
	EXPECT_EQ(collection.chainJoints[1].scale, F(0x3DCCCCCD));
	EXPECT_EQ(collection.chainJoints[1].colour, 0x00FFFFFFu);
	EXPECT_EQ(collection.chainJoints[1].specular, 0u);
	EXPECT_TRUE(collection.chainJoints[1].drawn);
	collection.chainScroll = F(1043677056);
	collection.chainScrollRate = 1.0f;
	collection.chainDrawn = true;
	const auto record = particle_codecs::ChainValues(collection, 119);
	EXPECT_EQ(record.persistent, (gamesave::PersistentId {.first = 119, .second = 3}));
	EXPECT_EQ(record.words, (std::array<uint32_t, 8> {0xCDCDCDCDu, 256, 64, 0, 0, 4, 4, 0}));
	EXPECT_EQ(record.byte, 0u);
	EXPECT_EQ(record.word, 1043677056u);
	ASSERT_EQ(record.joints.values.size(), 2u);
	EXPECT_EQ(record.joints.values[0], saved[0]);
	EXPECT_EQ(record.joints.values[1], saved[1]);
	EXPECT_EQ(record.lastWord, 1065353216u);
	EXPECT_EQ(record.lastByte, 1u);

	particles::Collection read;
	read.chainCreator = &creator;
	particle_codecs::ApplyChain(record, read);
	EXPECT_EQ(read.chainTextures, -1);
	EXPECT_EQ(particle_codecs::ChainValues(read, 119), record);
	// a chain whose repeats a rule set keeps them; with -1 the creator gives one a segment
	collection.chainTextures = 7;
	EXPECT_EQ(particle_codecs::ChainValues(collection, 119).words[6], 7u);
	particle_codecs::ApplyChain(particle_codecs::ChainValues(collection, 119), read);
	EXPECT_EQ(read.chainTextures, 7);
	collection.chainTextures = -1;
	creator.numTexturesForWholeChain = -1;
	EXPECT_EQ(particle_codecs::ChainValues(collection, 119).words[6], 1u);
}

// A chain joint's particle as the original saves it, from the same chain's newest joint: drawn, its number 19, its
// jitter 5 and its jitter switch off
TEST(ParticleCodecs, AChainJointIsWrittenAsTheOriginalSavesIt)
{
	particles::Atom atom;
	atom.chainJoint = 19;
	const auto record = particle_codecs::ChainJointValues(atom);
	EXPECT_EQ(record.renderByte, 1u);
	EXPECT_EQ(record.words, (std::array<uint32_t, 3> {19, 1084227584, 0}));
}

// The simple beam's data as the original saves it, from the first of the magic beam's two atoms in the frozen slot 0
// save (SF_SimpleBeam, its rule object 2 of the file): on the atom, a point beam's turn left unset, no object, its end
// and its start; on its first beam, the zero end slopes, the spline through its six key points with each key's share of
// the way and the spline's second derivatives there, bit for bit, its number 0 and the atom's three beams; read back,
// the same
TEST(ParticleCodecs, TheSimpleBeamsDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 2;
	particles::beam::Target target;
	target.target = glm::vec3(F(0x450D9F33), F(0x42E64E5C), F(0x4530E28F));
	target.origin = glm::vec3(F(0x450DEC70), F(0x42DBD212), F(0x45318A9C));
	const auto atomData = particle_codecs::BeamAtomDataValues(target, rule, 119);
	EXPECT_EQ(atomData.persistent, (gamesave::PersistentId {.first = 119, .second = 2}));
	EXPECT_EQ(atomData.word, 0xCDCDCDCDu);
	EXPECT_EQ(atomData.object.number, 0u);
	EXPECT_EQ(atomData.points[0], (gamesave::Point {F(0x450D9F33), F(0x42E64E5C), F(0x4530E28F)}));
	EXPECT_EQ(atomData.points[1], (gamesave::Point {F(0x450DEC70), F(0x42DBD212), F(0x45318A9C)}));
	const auto readTarget = particle_codecs::BeamTargetOf(atomData);
	EXPECT_EQ(readTarget.validatedTurn, target.validatedTurn);
	EXPECT_EQ(readTarget.target, target.target);
	EXPECT_EQ(readTarget.origin, target.origin);

	struct Saved
	{
		uint32_t t;
		std::array<uint32_t, 3> key;
		std::array<uint32_t, 3> second;
	};
	const std::array<Saved, 6> saved {{
	    {0x00000000, {0x450DEC70, 0x42DBD212, 0x45318A9C}, {0xC3089EFC, 0x437F0F34, 0xC2BDEFC6}},
	    {0x3E4CCCCD, {0x450DD718, 0x42E14038, 0x45317559}, {0x429249F2, 0xC2CDB28E, 0xC11645A6}},
	    {0x3ECCCCCD, {0x450DD96C, 0x42E42657, 0x453148DD}, {0x4283EF36, 0xC206109C, 0xC2AA5A27}},
	    {0x3F19999A, {0x450DE584, 0x42E4A427, 0x45310DA7}, {0xC37575E5, 0x42616AFB, 0x435408EE}},
	    {0x3F4CCCCD, {0x450DA65E, 0x42E6B622, 0x45310C0B}, {0x43527AB4, 0xC292FC75, 0xC35EF2E8}},
	    {0x3F800000, {0x450D9F33, 0x42E64E5C, 0x4530E28F}, {0xC28F4793, 0x424FCA79, 0x4398F75B}},
	}};
	particles::beam::Ribbon ribbon;
	ribbon.number = 0;
	ribbon.beams = 3;
	for (const auto& point : saved)
	{
		ribbon.keys.emplace_back(F(point.key[0]), F(point.key[1]), F(point.key[2]));
	}
	const auto record = particle_codecs::BeamRibbonValues(ribbon, rule, 119);
	EXPECT_EQ(record.persistent, (gamesave::PersistentId {.first = 119, .second = 2}));
	EXPECT_EQ(record.word, 1u);
	ASSERT_EQ(record.points.elements.size(), saved.size());
	for (size_t i = 0; i < saved.size(); ++i)
	{
		const auto& point = record.points.elements[i];
		EXPECT_EQ(point.word, saved[i].t) << i;
		EXPECT_EQ(point.first, (gamesave::Point {F(saved[i].key[0]), F(saved[i].key[1]), F(saved[i].key[2])})) << i;
		for (size_t axis = 0; axis < 3; ++axis)
		{
			EXPECT_EQ(std::bit_cast<uint32_t>(point.second.at(axis)), saved[i].second.at(axis)) << i << " " << axis;
		}
	}
	EXPECT_EQ(record.lastWords, (std::array<uint32_t, 2> {0, 3}));
	const auto readRibbon = particle_codecs::BeamRibbonOf(record);
	EXPECT_EQ(readRibbon.number, 0);
	EXPECT_EQ(readRibbon.beams, 3);
	EXPECT_EQ(readRibbon.keys, ribbon.keys);
	// a beam never laid has no spline
	EXPECT_TRUE(particle_codecs::BeamRibbonValues(particles::beam::Ribbon {}, rule, 119).points.elements.empty());
}

// A creature spell's first particle, standing for its creature, is written as the original saves it: enabled, then the
// game turn its creature was last found there (the creature, a reference, is the writer's), and read back the same
TEST(ParticleCodecs, ACreatureSpellsFirstParticleIsWrittenAsTheOriginalSavesItAndReadBack)
{
	gamesave::ParticleCreatureRefRecord saved;
	saved.renderByte = 1;
	saved.turn = 4321;
	saved.creature.number = 77;
	const auto read = particle_codecs::CreatureRefOf(saved);
	EXPECT_TRUE(read.enabled);
	EXPECT_EQ(read.validatedTurn, 4321u);
	EXPECT_TRUE(read.creature == entt::null);
	auto written = particle_codecs::CreatureRefValues(read);
	written.creature = saved.creature;
	EXPECT_EQ(written, saved);
	// one not enabled stays so
	saved.renderByte = 0;
	EXPECT_EQ(particle_codecs::CreatureRefValues(particle_codecs::CreatureRefOf(saved)).renderByte, 0u);
}

TEST(ParticleCodecs, AVortexsCarriedThingIsWrittenAsTheOriginalSavesItAndReadBack)
{
	// the second cow the Land 1 save 14/1's landscape vortex draws in: its vortex attract data (record 3277) and its
	// game object particle (record 3278), as the original wrote them
	gamesave::VortexAttractAtomDataRecord data;
	data.persistent = {.first = 76, .second = 0};
	data.word = 1067120376U;
	data.point = {-4.57602501f, -1.43535829f, -2.62490106f};
	data.bytes = {0, 0, 0};
	data.secondWord = 0;
	const std::string matrixHex =
	    "7C446D3F36C6E93E50FC0ABE43EC813E865063BEC16B7C3FAA65CE3E97F768BFB3039EBEBE3AD444085F0DC077C51D45";
	gamesave::ParticleGameObjectRecord object;
	object.renderByte = 1;
	object.word = 34001;
	for (size_t i = 0; i < object.zoomer.size(); ++i)
	{
		object.zoomer.at(i) = static_cast<uint8_t>(std::stoul(matrixHex.substr(i * 2, 2), nullptr, 16));
	}
	object.byte = 0;

	// read back, then written again the same
	const auto read = particle_codecs::VortexAttractDataOf(data);
	EXPECT_EQ(read.startHeight, std::bit_cast<float>(1067120376U));
	EXPECT_EQ(read.takenVelocity, glm::vec3(-4.57602501f, -1.43535829f, -2.62490106f));
	EXPECT_FALSE(read.indestructible);
	ASSERT_TRUE(read.look.has_value());
	EXPECT_FALSE(read.look->wasPaper);
	EXPECT_FALSE(read.look->wasDisappearing);
	EXPECT_EQ(read.look->importance, 0.0f);
	particles::Modifier rule;
	rule.fileIndex = 0;
	EXPECT_EQ(particle_codecs::VortexAttractDataValues(read, rule, 76), data);

	// the matrix written is the atom's drawn matrix of this step
	const auto carried = particle_codecs::CarriedObjectOf(object);
	EXPECT_EQ(carried.availableTurn, 34001U);
	EXPECT_EQ(carried.byte, 0);
	particles::saved_draw::SavedDraw drawn;
	std::memcpy(drawn.matrix.data(), object.zoomer.data(), object.zoomer.size());
	EXPECT_EQ(particle_codecs::GameObjectValues(carried, drawn), object);

	// one the first cow's take found disappearing (record 3274)
	data.bytes = {0, 0, 1};
	EXPECT_TRUE(particle_codecs::VortexAttractDataOf(data).look->wasDisappearing);
	EXPECT_EQ(particle_codecs::VortexAttractDataValues(particle_codecs::VortexAttractDataOf(data), rule, 76), data);
}

// The mana path rule's data on a spark as the original saves it (its class 0xC0): the rule, the path's end, its start,
// its way across the ground, the curve's four terms, the length, the weave's size and frequency, its phase and the
// progress, each point 12 bytes and each value a word in that order; read back the same
// (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, AManaPathSparksDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	const particles::mana_path::Settings settings {
	    .speed = 40.0f,
	    .timeToTravel = 3.0f,
	    .noiseFrequency = 0.0893805f,
	    .noiseAmplitude = 5.9885f,
	    .height = 0.4f,
	    .constantSpeed = true,
	};
	particles::mana_path::AtomData data;
	data.path = particles::mana_path::MakePath({10.0f, 2.0f, 20.0f}, {40.0f, 6.0f, 60.0f}, settings,
	                                           {.frequency = 0.25f, .amplitude = 0.5f, .phase = 0.75f});
	data.progress = 0.375f;
	particles::Modifier rule;
	rule.fileIndex = 0;
	const auto record = particle_codecs::ManaPathDataValues(data, rule, 22);
	EXPECT_EQ(record.type, 0xC0U);
	EXPECT_EQ(record.persistent, (gamesave::PersistentId {.first = 22, .second = 0}));
	ASSERT_EQ(record.values.size(), 7U * 12 + 5 * 4);
	EXPECT_TRUE(record.references.empty());
	const auto pointAt = [&record](size_t offset) {
		std::array<float, 3> point {};
		std::memcpy(point.data(), record.values.data() + offset, sizeof(point));
		return glm::vec3(point[0], point[1], point[2]);
	};
	const auto floatAt = [&record](size_t offset) {
		float value = 0.0f;
		std::memcpy(&value, record.values.data() + offset, sizeof(value));
		return value;
	};
	const auto& path = data.path;
	EXPECT_EQ(pointAt(0), path.to);
	EXPECT_EQ(pointAt(12), path.from);
	EXPECT_EQ(pointAt(24), path.side);
	EXPECT_EQ(pointAt(36), path.cubic);
	EXPECT_EQ(pointAt(48), path.square);
	EXPECT_EQ(pointAt(60), path.linear);
	EXPECT_EQ(pointAt(72), path.constant);
	EXPECT_EQ(floatAt(84), path.length);
	EXPECT_EQ(floatAt(88), path.amplitude);
	EXPECT_EQ(floatAt(92), path.frequency);
	EXPECT_EQ(floatAt(96), path.phase);
	EXPECT_EQ(floatAt(100), 0.375f);

	const auto read = particle_codecs::ManaPathDataOf(record);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->path.to, path.to);
	EXPECT_EQ(read->path.from, path.from);
	EXPECT_EQ(read->path.side, path.side);
	EXPECT_EQ(read->path.cubic, path.cubic);
	EXPECT_EQ(read->path.square, path.square);
	EXPECT_EQ(read->path.linear, path.linear);
	EXPECT_EQ(read->path.constant, path.constant);
	EXPECT_EQ(read->path.length, path.length);
	EXPECT_EQ(read->path.amplitude, path.amplitude);
	EXPECT_EQ(read->path.frequency, path.frequency);
	EXPECT_EQ(read->path.phase, path.phase);
	EXPECT_EQ(read->progress, 0.375f);
	EXPECT_EQ(particle_codecs::ManaPathDataValues(*read, rule, 22), record);
	// a record of another class is not the mana path's
	auto other = record;
	other.type = 0xBB;
	EXPECT_FALSE(particle_codecs::ManaPathDataOf(other).has_value());
}

// The glowing shell rule's data on an atom as the original saves it (its class 0xBB): the rule, the game turn its object
// was last found there, then its object, a reference the writer gives; read back the same
// (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, AGlowingShellsDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 0;
	const particles::volume_glow::AtomData data {.object = static_cast<entt::entity>(5), .validatedTurn = 34001};
	auto record = particle_codecs::VolumeGlowDataValues(data, rule, 63);
	EXPECT_EQ(record.type, 0xBBU);
	EXPECT_EQ(record.persistent, (gamesave::PersistentId {.first = 63, .second = 0}));
	EXPECT_EQ(record.Word(0), 34001U);
	// the object is the writer's
	EXPECT_EQ(record.Reference(particle_codecs::k_VolumeGlowObjectField), 0U);
	ASSERT_TRUE(record.SetReference(particle_codecs::k_VolumeGlowObjectField, 812));
	const auto read = particle_codecs::VolumeGlowDataOf(record);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->validatedTurn, 34001U);
	EXPECT_TRUE(read->object == entt::null);
	EXPECT_FALSE(particle_codecs::ManaPathDataOf(record).has_value());
	EXPECT_FALSE(particle_codecs::VolumeGlowDataOf(particle_codecs::ManaPathDataValues({}, rule, 22)).has_value());
}

// The plasma rule's data on a carrier atom as the original saves it (its class 0xBD): the rule, then the arc as it was
// asked for, the start, end and two tangents 12 bytes each, the life and speed a word each and the alpha a byte; read
// back the same (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, APlasmasArcIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 0;
	const particles::PlasmaCommand arc {.start = {100.0f, 40.0f, 200.0f},
	                                    .end = {130.0f, 35.0f, 200.0f},
	                                    .startTangent = {0.0f, 1.0f, 0.0f},
	                                    .endTangent = {0.0f, -1.0f, 0.0f},
	                                    .life = 2.0f,
	                                    .speed = 1.5f,
	                                    .alpha = 70};
	const auto record = particle_codecs::PlasmaDataValues(arc, rule, 21);
	EXPECT_EQ(record.type, 0xBDU);
	EXPECT_EQ(record.persistent, (gamesave::PersistentId {.first = 21, .second = 0}));
	ASSERT_EQ(record.values.size(), 4U * 12 + 2 * 4 + 1);
	float value = 0.0f;
	std::memcpy(&value, record.values.data() + 12, sizeof(value));
	EXPECT_EQ(value, 130.0f);
	std::memcpy(&value, record.values.data() + 48, sizeof(value));
	EXPECT_EQ(value, 2.0f);
	std::memcpy(&value, record.values.data() + 52, sizeof(value));
	EXPECT_EQ(value, 1.5f);
	EXPECT_EQ(record.values.back(), 70);
	const auto read = particle_codecs::PlasmaDataOf(record);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->start, arc.start);
	EXPECT_EQ(read->end, arc.end);
	EXPECT_EQ(read->startTangent, arc.startTangent);
	EXPECT_EQ(read->endTangent, arc.endTangent);
	EXPECT_EQ(read->life, arc.life);
	EXPECT_EQ(read->speed, arc.speed);
	EXPECT_EQ(read->alpha, arc.alpha);
	EXPECT_EQ(particle_codecs::PlasmaDataValues(*read, rule, 21), record);
}

// The belief sprite rule's data on a sprite as the original saves it (its class 0xBE): the rule, the sprite's start, 12
// bytes, and its start alpha, a byte; read back the same (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, ABeliefSpritesDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 0;
	const particles::belief_sprite::AtomData data {.start = {1.0f, 2.0f, 3.0f}, .alpha = 85};
	const auto record = particle_codecs::BeliefSpriteDataValues(data, rule, 24);
	EXPECT_EQ(record.type, 0xBEU);
	EXPECT_EQ(record.persistent, (gamesave::PersistentId {.first = 24, .second = 0}));
	ASSERT_EQ(record.values.size(), 13U);
	EXPECT_EQ(record.PointAt(0), (gamesave::Point {1.0f, 2.0f, 3.0f}));
	EXPECT_EQ(record.values.back(), 85);
	const auto read = particle_codecs::BeliefSpriteDataOf(record);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->start, data.start);
	EXPECT_EQ(read->alpha, data.alpha);
	EXPECT_EQ(particle_codecs::BeliefSpriteDataValues(*read, rule, 24), record);
}

// The creature spell wisps rule's data as the original saves it: on its collection (class 0xC1) the rule, its first
// update's byte, where the creature stood and the hand was, the byte again, the wisps due and the wisps made; on a wisp
// (class 0xC2) the rule, its two angles, their rates and its size; read back the same
// (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, TheCreatureSpellWispsDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 1;
	const particles::creature_spell::WispsData wisps {
	    .creature = {10.0f, 2.0f, 20.0f}, .hand = {50.0f, 30.0f, 60.0f}, .first = false, .due = 0.375f, .made = 2};
	const auto record = particle_codecs::CreatureSpellWispsValues(wisps, rule, 85);
	EXPECT_EQ(record.type, 0xC1U);
	EXPECT_EQ(record.persistent, (gamesave::PersistentId {.first = 85, .second = 1}));
	ASSERT_EQ(record.values.size(), 1U + 12 + 12 + 1 + 4 + 4);
	EXPECT_EQ(record.Byte(0), 0);
	EXPECT_EQ(record.PointAt(1), (gamesave::Point {10.0f, 2.0f, 20.0f}));
	EXPECT_EQ(record.PointAt(2), (gamesave::Point {50.0f, 30.0f, 60.0f}));
	EXPECT_EQ(record.Byte(3), 0);
	EXPECT_EQ(record.Float(4), 0.375f);
	EXPECT_EQ(record.Word(5), 2U);
	const auto readWisps = particle_codecs::CreatureSpellWispsOf(record);
	ASSERT_TRUE(readWisps.has_value());
	EXPECT_EQ(readWisps->creature, wisps.creature);
	EXPECT_EQ(readWisps->hand, wisps.hand);
	EXPECT_EQ(readWisps->first, wisps.first);
	EXPECT_EQ(readWisps->due, wisps.due);
	EXPECT_EQ(readWisps->made, wisps.made);
	EXPECT_EQ(particle_codecs::CreatureSpellWispsValues(*readWisps, rule, 85), record);

	const particles::creature_spell::WispData wisp {
	    .theta = 1.25f, .phi = 2.5f, .thetaRate = -3.0f, .phiRate = 0.75f, .scale = 1.5f};
	const auto atomRecord = particle_codecs::CreatureSpellWispValues(wisp, rule, 85);
	EXPECT_EQ(atomRecord.type, 0xC2U);
	ASSERT_EQ(atomRecord.values.size(), 5U * 4);
	EXPECT_EQ(atomRecord.Float(0), 1.25f);
	EXPECT_EQ(atomRecord.Float(2), -3.0f);
	EXPECT_EQ(atomRecord.Float(4), 1.5f);
	const auto readWisp = particle_codecs::CreatureSpellWispOf(atomRecord);
	ASSERT_TRUE(readWisp.has_value());
	EXPECT_EQ(particle_codecs::CreatureSpellWispValues(*readWisp, rule, 85), atomRecord);
	EXPECT_FALSE(particle_codecs::CreatureSpellWispOf(record).has_value());
	EXPECT_FALSE(particle_codecs::CreatureSpellWispsOf(atomRecord).has_value());
}

// The orient-with-velocity rule's data on a sprite as the original saves it (its class 0xC3): the rule, whether its
// first step is still to come, then its smoothing filter: the smoothed velocity, its rate and whether it has been
// started; read back the same (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, AnOrientWithVelocitySpritesDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 2;
	const particles::orient_velocity::AtomData data {.first = false, .velocity = {1.0f, -2.0f, 3.5f}, .rate = 6.5f};
	const auto record = particle_codecs::OrientWithVelocityValues(data, rule, 41);
	EXPECT_EQ(record.type, 0xC3U);
	ASSERT_EQ(record.values.size(), 1U + 12 + 4 + 1);
	EXPECT_EQ(record.Byte(0), 0);
	EXPECT_EQ(record.PointAt(1), (gamesave::Point {1.0f, -2.0f, 3.5f}));
	EXPECT_EQ(record.Float(2), 6.5f);
	// the filter is started on the first step
	EXPECT_EQ(record.Byte(3), 1);
	const auto read = particle_codecs::OrientWithVelocityOf(record);
	ASSERT_TRUE(read.has_value());
	EXPECT_FALSE(read->first);
	EXPECT_EQ(read->velocity, data.velocity);
	EXPECT_EQ(read->rate, data.rate);
	EXPECT_EQ(particle_codecs::OrientWithVelocityValues(*read, rule, 41), record);
}

// The creature spells' collection data as the original saves it: the hearts' (0xC5) its byte, the atoms due and made;
// the first rule's (0xC6) its first update, the turn its creature was last found available and the creature; the itch's
// (0xC7) its first update; the steam's (0xC8) its byte twice, the atoms due and made; read back the same
// (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, TheCreatureSpellsCollectionDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 0;
	const particles::creature_spell::AtomsMade made {.due = 2.5f, .made = 2};
	const auto hearts = particle_codecs::CreatureSpellAtomsMadeValues(0xC5, made, rule, 86);
	EXPECT_EQ(hearts.type, 0xC5U);
	ASSERT_EQ(hearts.values.size(), 1U + 4 + 4);
	EXPECT_EQ(hearts.Byte(0), 0);
	EXPECT_EQ(hearts.Float(1), 2.5f);
	EXPECT_EQ(hearts.Word(2), 2U);
	const auto steam = particle_codecs::CreatureSpellAtomsMadeValues(0xC8, made, rule, 90);
	ASSERT_EQ(steam.values.size(), 1U + 1 + 4 + 4);
	EXPECT_EQ(steam.Float(2), 2.5f);
	EXPECT_EQ(steam.Word(3), 2U);
	for (const auto* record : {&hearts, &steam})
	{
		const auto read = particle_codecs::CreatureSpellAtomsMadeOf(*record);
		ASSERT_TRUE(read.has_value());
		EXPECT_EQ(read->due, 2.5f);
		EXPECT_EQ(read->made, 2);
		EXPECT_EQ(particle_codecs::CreatureSpellAtomsMadeValues(record->type, *read, rule, record->persistent.first), *record);
	}

	const particles::creature_spell::CreatureHeld held {.first = false, .creature = entt::null, .validatedTurn = 1234};
	auto first = particle_codecs::CreatureSpellValues(held, rule, 85);
	EXPECT_EQ(first.type, 0xC6U);
	EXPECT_EQ(first.Byte(0), 0);
	EXPECT_EQ(first.Word(1), 1234U);
	EXPECT_EQ(first.Reference(particle_codecs::k_CreatureSpellCreatureField), 0U);
	const auto readHeld = particle_codecs::CreatureSpellOf(first);
	ASSERT_TRUE(readHeld.has_value());
	EXPECT_FALSE(readHeld->first);
	EXPECT_EQ(readHeld->validatedTurn, 1234U);

	const auto itch = particle_codecs::CreatureSpellItchValues(true, rule, 87);
	EXPECT_EQ(itch.type, 0xC7U);
	ASSERT_EQ(itch.values.size(), 1U);
	EXPECT_EQ(particle_codecs::CreatureSpellItchOf(itch), true);
	EXPECT_FALSE(particle_codecs::CreatureSpellItchOf(first).has_value());
	EXPECT_FALSE(particle_codecs::CreatureSpellAtomsMadeOf(itch).has_value());
}

// The sphere surface tracer's (class 0xCB) and the forest path's (class 0xCD) data on an atom as the original saves it:
// the rule, whether its first step is still to come and its three angles; read back the same
// (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, TheSphereAnglesDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 3;
	const particles::sphere_angles::AtomData data {.first = false, .theta = 0.5f, .phi = 1.25f, .psi = 6.0f};
	for (const uint32_t type : {0xCBU, 0xCDU})
	{
		const auto record = particle_codecs::SphereAnglesValues(type, data, rule, 30);
		EXPECT_EQ(record.type, type);
		ASSERT_EQ(record.values.size(), 1U + 3 * 4);
		EXPECT_EQ(record.Byte(0), 0);
		EXPECT_EQ(record.Float(1), 0.5f);
		EXPECT_EQ(record.Float(2), 1.25f);
		EXPECT_EQ(record.Float(3), 6.0f);
		const auto read = particle_codecs::SphereAnglesOf(record);
		ASSERT_TRUE(read.has_value());
		EXPECT_FALSE(read->first);
		EXPECT_EQ(read->theta, 0.5f);
		EXPECT_EQ(read->phi, 1.25f);
		EXPECT_EQ(read->psi, 6.0f);
		EXPECT_EQ(particle_codecs::SphereAnglesValues(type, *read, rule, 30), record);
	}
}

// The flocking rule's data on its collection as the original saves it (its class 0xD0): the rule, its first update's
// byte, which nothing clears, and the flock's velocity; read back the same
// (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, AFlocksVelocityIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 0;
	const auto record = particle_codecs::FlockingValues({1.5f, -0.25f, 3.0f}, rule, 45);
	EXPECT_EQ(record.type, 0xD0U);
	ASSERT_EQ(record.values.size(), 1U + 12);
	EXPECT_EQ(record.Byte(0), 1);
	EXPECT_EQ(record.PointAt(1), (gamesave::Point {1.5f, -0.25f, 3.0f}));
	EXPECT_EQ(particle_codecs::FlockingOf(record), glm::vec3(1.5f, -0.25f, 3.0f));
	EXPECT_EQ(particle_codecs::FlockingValues(*particle_codecs::FlockingOf(record), rule, 45), record);
}

// The cloud mover's data on its collection as the original saves it (its class 0xD1): the rule and whether its first
// call is still to come; read back the same (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, ACloudMoversDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 1;
	const auto record = particle_codecs::CloudMoverValues(false, rule, 4);
	EXPECT_EQ(record.type, 0xD1U);
	ASSERT_EQ(record.values.size(), 1U);
	EXPECT_EQ(record.Byte(0), 0);
	EXPECT_EQ(particle_codecs::CloudMoverOf(record), false);
	EXPECT_EQ(particle_codecs::CloudMoverOf(particle_codecs::CloudMoverValues(true, rule, 4)), true);
	EXPECT_FALSE(particle_codecs::FlockingOf(record).has_value());
}

// The cloud gather's data on a cloud as the original saves it (its class 0xD3): the rule, its radius and angle, a word the
// original never writes after making it, whether its lightning lights it, its age then and its height; read back the same
// (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, ACloudsDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 2;
	const particles::storm::CloudData cloud {
	    .radius = 30.5f, .theta = 2.25f, .flashing = true, .flashStart = 4.0f, .height = 1.5f};
	const auto record = particle_codecs::CloudValues(cloud, rule, 4);
	EXPECT_EQ(record.type, 0xD3U);
	ASSERT_EQ(record.values.size(), 3U * 4 + 1 + 2 * 4);
	EXPECT_EQ(record.Float(0), 30.5f);
	EXPECT_EQ(record.Float(1), 2.25f);
	EXPECT_EQ(record.Word(2), 0U);
	EXPECT_EQ(record.Byte(3), 1);
	EXPECT_EQ(record.Float(4), 4.0f);
	EXPECT_EQ(record.Float(5), 1.5f);
	const auto read = particle_codecs::CloudOf(record);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(particle_codecs::CloudValues(*read, rule, 4), record);
}

// The cloud gather's data on its collection as the original saves it (its class 0xD2): the rule, the heading, the
// clouds due and made, its first update's byte, the rate, the strike's ages, the spin, its lightning collection and the
// cloud it hangs from (references, the writer's), then four bytes; read back the same
// (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, ACloudGathersDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 0;
	const particles::storm::CloudGatherValues values {.heading = {1.0f, 0.0f, -2.0f},
	                                                  .emitted = 7.5f,
	                                                  .count = 7,
	                                                  .first = false,
	                                                  .rate = 2.5f,
	                                                  .nextStrike = 12.0f,
	                                                  .strikeEnd = 9.5f,
	                                                  .spin = -1.0f,
	                                                  .detached = true,
	                                                  .lightningOn = true,
	                                                  .rainOn = true,
	                                                  .registered = false};
	auto record = particle_codecs::CloudGatherValues(values, rule, 4);
	EXPECT_EQ(record.type, 0xD2U);
	ASSERT_EQ(record.values.size(), 12U + 2 * 4 + 1 + 4 * 4 + 4);
	ASSERT_EQ(record.references.size(), 2U);
	EXPECT_EQ(record.PointAt(0), (gamesave::Point {1.0f, 0.0f, -2.0f}));
	EXPECT_EQ(record.Float(1), 7.5f);
	EXPECT_EQ(record.Word(2), 7U);
	EXPECT_EQ(record.Byte(3), 0);
	EXPECT_EQ(record.Float(7), -1.0f);
	EXPECT_EQ(record.Byte(10), 1);
	EXPECT_EQ(record.Byte(13), 0);
	ASSERT_TRUE(record.SetReference(particle_codecs::k_CloudGatherLightningField, 501));
	EXPECT_EQ(record.Reference(8), 501U);
	EXPECT_EQ(record.Reference(9), 0U);
	const auto read = particle_codecs::CloudGatherOf(record);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->heading, values.heading);
	EXPECT_EQ(read->count, 7);
	EXPECT_EQ(read->strikeEnd, 9.5f);
	EXPECT_TRUE(read->detached);
	EXPECT_TRUE(read->lightningOn);
	EXPECT_FALSE(read->registered);
	auto again = particle_codecs::CloudGatherValues(*read, rule, 4);
	ASSERT_TRUE(again.SetReference(particle_codecs::k_CloudGatherLightningField, 501));
	EXPECT_EQ(again, record);
}

namespace
{
/// The bytes of the values as the original writes them, one after the other
template <class... T>
std::vector<uint8_t> BytesOf(const T&... values)
{
	std::vector<uint8_t> bytes;
	const auto put = [&bytes](const auto& value) {
		const auto at = bytes.size();
		bytes.resize(at + sizeof(value));
		std::memcpy(bytes.data() + at, &value, sizeof(value));
	};
	(put(values), ...);
	return bytes;
}
} // namespace

// The tornado's data as the original saves it: on its collection (0xD7) its first update, its base, top and their
// velocities, a point it never sets after making it, the scale, the strength, the alpha, the fade and whether it is
// closing down; on each debris and flying collection (0xD5, the class both write) the atoms made and due; on each
// flying atom (0xD4) its state, rise and spin. Read back the same
// (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, ATornadosDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 3;
	const particles::storm::TornadoData tornado {.first = false,
	                                             .base = {10.0f, 2.0f, -3.0f},
	                                             .top = {12.0f, 102.0f, -1.0f},
	                                             .baseVelocity = {0.5f, 0.0f, -0.25f},
	                                             .topVelocity = {1.0f, 0.125f, 2.0f},
	                                             .tornadoScale = 1.5f,
	                                             .strength = 1.0f,
	                                             .alpha = 0xFB,
	                                             .fade = 0.75f,
	                                             .closing = true};
	const auto record = particle_codecs::TornadoValues(tornado, rule, 4);
	EXPECT_EQ(record.type, 0xD7U);
	EXPECT_EQ(record.persistent.first, 4U);
	EXPECT_EQ(record.persistent.second, 3U);
	EXPECT_TRUE(record.references.empty());
	EXPECT_EQ(record.values, BytesOf(uint8_t {0}, std::array {10.0f, 2.0f, -3.0f}, std::array {12.0f, 102.0f, -1.0f},
	                                 std::array {0.5f, 0.0f, -0.25f}, std::array {1.0f, 0.125f, 2.0f},
	                                 std::array {0.0f, 0.0f, 0.0f}, 1.5f, 1.0f, uint8_t {0xFB}, 0.75f, uint8_t {1}));
	EXPECT_EQ(particle_codecs::TornadoOf(record), tornado);
	const particles::storm::TornadoData fresh;
	EXPECT_EQ(particle_codecs::TornadoOf(particle_codecs::TornadoValues(fresh, rule, 4)), fresh);

	const particles::storm::TornadoEmitterData emitter {.made = 6.0f, .owed = 6.5f};
	const auto emitted = particle_codecs::TornadoEmitterValues(emitter, rule, 4);
	EXPECT_EQ(emitted.type, 0xD5U);
	EXPECT_EQ(emitted.values, BytesOf(6.0f, 6.5f));
	EXPECT_EQ(particle_codecs::TornadoEmitterOf(emitted), emitter);

	const particles::storm::TornadoFlyingData flying {.state = 1, .blend = 0.625f, .speed = 1.25f};
	const auto flown = particle_codecs::TornadoFlyingValues(flying, rule, 4);
	EXPECT_EQ(flown.type, 0xD4U);
	EXPECT_EQ(flown.values, BytesOf(uint32_t {1}, 0.625f, 1.25f));
	EXPECT_EQ(particle_codecs::TornadoFlyingOf(flown), flying);

	// each reads only its own class
	EXPECT_FALSE(particle_codecs::TornadoOf(emitted).has_value());
	EXPECT_FALSE(particle_codecs::TornadoEmitterOf(flown).has_value());
	EXPECT_FALSE(particle_codecs::TornadoFlyingOf(record).has_value());
}

// The lightning sprite emitter's data on its collection as the original saves it (its class 0xD9): the collection age
// its flash began at (before the first, the fill of a new particle block), whether the flash lights the cloud, the next
// sprite's due time, the sprites made and whether its first update is to come. Read back the same
// (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, ALightningSpriteEmittersDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 2;
	const particles::storm_flash::State fresh;
	const auto made = particle_codecs::LightningSpriteValues(fresh, rule, 4);
	EXPECT_EQ(made.type, 0xD9U);
	EXPECT_EQ(made.values, BytesOf(0xCDCDCDCDU, uint8_t {0}, 0.0f, uint32_t {0}, uint8_t {1}));
	const particles::storm_flash::State state {
	    .first = false, .nextTime = 3.25f, .emitted = 4, .striking = true, .strikeAge = 2.5f};
	const auto record = particle_codecs::LightningSpriteValues(state, rule, 4);
	EXPECT_EQ(record.persistent.second, 2U);
	EXPECT_EQ(record.values, BytesOf(2.5f, uint8_t {1}, 3.25f, uint32_t {4}, uint8_t {0}));
	const auto read = particle_codecs::LightningSpriteOf(record);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(particle_codecs::LightningSpriteValues(*read, rule, 4), record);
	EXPECT_FALSE(read->first);
	EXPECT_EQ(read->emitted, 4);
	EXPECT_TRUE(read->striking);
	EXPECT_EQ(std::bit_cast<uint32_t>(particle_codecs::LightningSpriteOf(made)->strikeAge), 0xCDCDCDCDU);
	EXPECT_FALSE(particle_codecs::TornadoOf(record).has_value());
}

// The sound removal's data on an atom as the original saves it (its class 0xDD): whether it has stopped the atom's sound;
// read back the same (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, ASoundRemovalsDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 5;
	const auto record = particle_codecs::SoundRemovalValues(true, rule, 4);
	EXPECT_EQ(record.type, 0xDDU);
	EXPECT_EQ(record.persistent.second, 5U);
	EXPECT_EQ(record.values, BytesOf(uint8_t {1}));
	EXPECT_EQ(particle_codecs::SoundRemovalOf(record), true);
	EXPECT_EQ(particle_codecs::SoundRemovalValues(false, rule, 4).values, BytesOf(uint8_t {0}));
	EXPECT_EQ(particle_codecs::SoundRemovalOf(particle_codecs::SoundRemovalValues(false, rule, 4)), false);
	EXPECT_FALSE(particle_codecs::SoundRemovalOf(particle_codecs::LightningSpriteValues({}, rule, 4)).has_value());
}

// The sub-collections rule's data on an atom as the original saves it (its class 0xE2): whether it has put its groups
// under the atom; read back the same (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, ASubCollectionsRulesDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 6;
	const auto record = particle_codecs::SubCollectionsValues(true, rule, 4);
	EXPECT_EQ(record.type, 0xE2U);
	EXPECT_EQ(record.persistent.second, 6U);
	EXPECT_EQ(record.values, BytesOf(uint8_t {1}));
	EXPECT_EQ(particle_codecs::SubCollectionsOf(record), true);
	EXPECT_EQ(particle_codecs::SubCollectionsOf(particle_codecs::SubCollectionsValues(false, rule, 4)), false);
	EXPECT_FALSE(particle_codecs::SubCollectionsOf(particle_codecs::SoundRemovalValues(true, rule, 4)).has_value());
}

// The explosion's data on its collection as the original saves it (its class 0xCF): whether its blast is still to
// start and a target is left, its centre, the ring of its blast, its targets' turns (their objects the writer's), how
// many it shattered and removed, whether its smoke and beam are made, its beam's spot visual and whether a shield stopped
// it; read back the same, the spot visual's number kept for the link once the containers are made
// (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, AnExplosionsDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 4;
	particles::explosion::BlastData data;
	data.centre = {10.0f, 2.0f, 30.0f};
	data.notStarted = false;
	data.anyTarget = true;
	data.spread = 12.5f;
	data.targets = {{.turn = 41, .object = entt::null}, {.turn = 42, .object = entt::null}};
	data.exploded = 3;
	data.deleted = 2;
	data.smokeDone = true;
	data.beamDone = false;
	data.finished = true;
	const auto record = particle_codecs::ExplosionDataValues(data, rule, 9);
	EXPECT_EQ(gamesave::ExplosionCollectionRecord::k_Type, 0xCFU);
	EXPECT_EQ(record.persistent.first, 9U);
	EXPECT_EQ(record.persistent.second, 4U);
	EXPECT_EQ(record.bytes, (std::array<uint8_t, 2> {0, 1}));
	EXPECT_EQ(record.spread, std::bit_cast<uint32_t>(12.5f));
	ASSERT_EQ(record.targets.elements.size(), 2u);
	EXPECT_EQ(record.targets.elements[1].word, 42U);
	EXPECT_EQ(record.counts, (std::array<uint32_t, 2> {3, 2}));
	EXPECT_EQ(record.made, (std::array<uint8_t, 2> {1, 0}));
	EXPECT_EQ(record.stopped, 1);
	auto withFx = record;
	withFx.beamFx = {17};
	const auto read = particle_codecs::ExplosionDataOf(withFx);
	EXPECT_EQ(read.centre, data.centre);
	EXPECT_FALSE(read.notStarted);
	EXPECT_TRUE(read.anyTarget);
	ASSERT_EQ(read.targets.size(), 2u);
	EXPECT_EQ(read.targets[0].turn, 41U);
	EXPECT_EQ(read.loadedFx, 17U);
	EXPECT_EQ(particle_codecs::ExplosionDataValues(read, rule, 9), record);

	const auto factory = particles::FindModifierFactory("UR_Explosion");
	ASSERT_NE(factory, nullptr);
	const auto file = particles::File::Parse(
	    "BEGINPROPERTIES\nENDPROPERTIES\nBEGINCLASS UR_Explosion E\nBEGINPROPERTIES\nENDPROPERTIES\nENDCLASS\n", "blast");
	ASSERT_TRUE(file.has_value());
	ASSERT_FALSE(file->objects.empty());
	const auto blast = factory(file->objects.front());
	ASSERT_NE(blast, nullptr);
	EXPECT_EQ(blast->SavedData(), particles::RuleData::Explosion);
	particles::Collection collection;
	EXPECT_EQ(particles::explosion::DataOf(collection, *blast), nullptr);
	EXPECT_TRUE(particles::explosion::RestoreData(collection, *blast, read));
	const auto* restored = particles::explosion::DataOf(collection, *blast);
	ASSERT_NE(restored, nullptr);
	EXPECT_EQ(restored->exploded, 3);
	EXPECT_FALSE(particles::explosion::RestoreData(collection, rule, read));
}

// The plasma's data on one of its beams' collections as the original saves it (its class 0xBC): the simple beam's
// layout, the spline through its key points, its number and the count of beams; read back the same
// (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, APlasmasBeamDataIsWrittenAsTheSimpleBeamsUnderItsOwnClassAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 3;
	const particles::beam::Ribbon ribbon {
	    .number = 1, .beams = 2, .keys = {{0.0f, 1.0f, 2.0f}, {3.0f, 4.0f, 5.0f}, {6.0f, 7.0f, 8.0f}}};
	const auto beam = particle_codecs::BeamRibbonValues(ribbon, rule, 4);
	const auto plasma = particle_codecs::PlasmaRibbonValues(ribbon, rule, 4);
	EXPECT_EQ(gamesave::PlasmaSubCollectionRecord::k_Type, 0xBCU);
	EXPECT_EQ(plasma.persistent, beam.persistent);
	EXPECT_EQ(plasma.word, beam.word);
	EXPECT_EQ(plasma.points, beam.points);
	EXPECT_EQ(plasma.lastWords, (std::array<uint32_t, 2> {1, 2}));
	const auto read = particle_codecs::BeamRibbonOf(plasma);
	EXPECT_EQ(read.number, 1);
	EXPECT_EQ(read.beams, 2);
	EXPECT_EQ(read.keys, ribbon.keys);
}

// The fireball's rule's data on an atom as the original saves it (its class 0xDB): whether its first update is still to
// come and the game turn its ball was last found there, then the ball (a reference, the writer's); read back the same,
// and put back on the atom by the rule, noted as made, with no ball when the save names none
// (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, AFireballRulesDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier plain;
	plain.fileIndex = 7;
	const particles::fireball_rules::BallData data {.firstUpdate = false, .validatedTurn = 1234, .ball = entt::null};
	const auto record = particle_codecs::FireBallDataValues(data, plain, 4);
	EXPECT_EQ(record.type, 0xDBU);
	EXPECT_EQ(record.persistent.first, 4U);
	EXPECT_EQ(record.persistent.second, 7U);
	EXPECT_EQ(record.Byte(0), uint8_t {0});
	EXPECT_EQ(record.Word(1), 1234U);
	EXPECT_EQ(record.Reference(particle_codecs::k_FireBallBallField).value_or(1), 0U);
	const auto read = particle_codecs::FireBallDataOf(record);
	ASSERT_TRUE(read.has_value());
	EXPECT_FALSE(read->firstUpdate);
	EXPECT_EQ(read->validatedTurn, 1234U);
	EXPECT_EQ(particle_codecs::FireBallDataValues(*read, plain, 4), record);
	EXPECT_EQ(particle_codecs::FireBallDataValues({.firstUpdate = true}, plain, 4).Byte(0), uint8_t {1});
	EXPECT_FALSE(particle_codecs::FireBallDataOf(particle_codecs::SubCollectionsValues(true, plain, 4)).has_value());

	const auto factory = particles::FindModifierFactory("AttatchFireBallToAtom");
	ASSERT_NE(factory, nullptr);
	const auto file = particles::File::Parse(
	    "BEGINPROPERTIES\nENDPROPERTIES\nBEGINCLASS AttatchFireBallToAtom F\nBEGINPROPERTIES\nENDPROPERTIES\nENDCLASS\n",
	    "fireball");
	ASSERT_TRUE(file.has_value());
	ASSERT_FALSE(file->objects.empty());
	const auto rule = factory(file->objects.front());
	ASSERT_NE(rule, nullptr);
	EXPECT_EQ(rule->SavedData(), particles::RuleData::FireBall);
	particles::Atom atom;
	EXPECT_FALSE(particles::fireball_rules::BallDataOf(atom, *rule).has_value());
	EXPECT_TRUE(particles::fireball_rules::RestoreBallData(atom, *rule, data));
	const auto restored = particles::fireball_rules::BallDataOf(atom, *rule);
	ASSERT_TRUE(restored.has_value());
	EXPECT_FALSE(restored->firstUpdate);
	EXPECT_EQ(restored->validatedTurn, 1234U);
	EXPECT_EQ(restored->ball, entt::entity {entt::null});
	EXPECT_EQ(atom.dataMade, (std::vector<const particles::Modifier*> {rule.get()}));
	EXPECT_FALSE(particles::fireball_rules::RestoreBallData(atom, plain, data));
}

// The fade-out rule's data on an atom as the original saves it (its class 0xDA, a record of its own): the rule by its
// place in the file, whether its condition has held, then the age, alpha and scale the atom started fading from, each a
// float; read back the same, and put back on the atom by the rule, noted as made
// (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, AFadeOutRulesDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier plain;
	plain.fileIndex = 3;
	const particles::fade_out::AtomData data {
	    .conditionMet = true, .startTime = 1.5f, .startAlpha = 200.0f, .startScale = 0.75f};
	const auto record = particle_codecs::FadeOutDataValues(data, plain, 9);
	EXPECT_EQ(gamesave::FadeOutAtomDataRecord::k_Type, 0xDAU);
	EXPECT_EQ(record.persistent.first, 9U);
	EXPECT_EQ(record.persistent.second, 3U);
	EXPECT_EQ(record.byte, 1U);
	EXPECT_EQ(record.words,
	          (std::array {std::bit_cast<uint32_t>(1.5f), std::bit_cast<uint32_t>(200.0f), std::bit_cast<uint32_t>(0.75f)}));
	EXPECT_EQ(particle_codecs::FadeOutDataOf(record), data);

	const auto file = particles::File::Parse("BEGINPROPERTIES\nENDPROPERTIES\nBEGINCLASS AR_FadeOutOnceConditionTrue F\n"
	                                         "BEGINPROPERTIES\nPROPERTY TimeToFadeOut FLOAT 2\nENDPROPERTIES\nENDCLASS\n",
	                                         "fade");
	ASSERT_TRUE(file.has_value());
	ASSERT_FALSE(file->objects.empty());
	const auto factory = particles::FindModifierFactory("AR_FadeOutOnceConditionTrue");
	ASSERT_NE(factory, nullptr);
	const auto rule = factory(file->objects.front());
	ASSERT_NE(rule, nullptr);
	particles::Atom atom;
	EXPECT_FALSE(particles::fade_out::DataOf(atom, *rule).has_value());
	EXPECT_TRUE(particles::fade_out::RestoreData(atom, *rule, data));
	EXPECT_EQ(particles::fade_out::DataOf(atom, *rule), data);
	EXPECT_EQ(atom.dataMade, (std::vector<const particles::Modifier*> {rule.get()}));
	EXPECT_EQ(rule->SavedData(), particles::RuleData::FadeOut);
	EXPECT_FALSE(particles::fade_out::RestoreData(atom, plain, data));
}

// The willow wisp's data on its collection as the original saves it (its class 0xEE, a record of its own): the rule by
// its place in the file, its parent's last place, the atoms due and made (floats) and whether its first update is to
// come; put back on the slot the same (docs/wiki/engine/saved-games.md#the-particles-rules-data)
TEST(ParticleCodecs, AWillowWispsDataIsWrittenAsTheOriginalSavesItAndReadBack)
{
	particles::Modifier rule;
	rule.fileIndex = 5;
	particles::Collection::Slot slot;
	slot.modifier = &rule;
	slot.state = glm::vec4(1.0f, 2.0f, 3.0f, 0.0f);
	slot.extra = glm::vec4(4.5f, 4.0f, 0.0f, 0.0f);
	slot.first = false;
	const auto record = particle_codecs::WillowWispDataValues(slot, 11);
	EXPECT_EQ(gamesave::WillowWispCollectionRecord::k_Type, 0xEEU);
	EXPECT_EQ(record.persistent.first, 11U);
	EXPECT_EQ(record.persistent.second, 5U);
	EXPECT_EQ(record.point, (gamesave::Point {1.0f, 2.0f, 3.0f}));
	EXPECT_EQ(record.words, (std::array {std::bit_cast<uint32_t>(4.5f), std::bit_cast<uint32_t>(4.0f)}));
	EXPECT_EQ(record.byte, 0U);
	particles::Collection::Slot loaded;
	particle_codecs::ApplyWillowWispData(record, loaded);
	EXPECT_EQ(loaded.state, slot.state);
	EXPECT_EQ(loaded.extra.x, 4.5f);
	EXPECT_EQ(loaded.extra.y, 4.0f);
	EXPECT_FALSE(loaded.first);
}
