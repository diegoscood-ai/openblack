/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <limits>
#include <vector>

#include <MindFile.h>
#include <gtest/gtest.h>

#include "Creature/CreatureMindFileBody.h"
#include "Creature/CreatureTattoo.h"

using namespace openblack;
using namespace openblack::creature_mind_body;

TEST(CreatureMindFileBody, SpeciesRowsStartWithTheGiantApe)
{
	EXPECT_EQ(SpeciesFromRow(0), CreatureType::GiantApe);
	EXPECT_EQ(SpeciesFromRow(1), CreatureType::Cow);
	EXPECT_EQ(SpeciesFromRow(4), CreatureType::Wolf);
	EXPECT_EQ(SpeciesFromRow(16), CreatureType::Gorilla);
	EXPECT_FALSE(SpeciesFromRow(17).has_value());
	EXPECT_FALSE(SpeciesFromRow(1013008280).has_value());
}

TEST(CreatureMindFileBody, TakesTheBodyAFileKeeps)
{
	creaturemind::MindFileData file;
	file.speciesRow = 3;
	file.name = u"Spot";
	file.alignment = -0.75f;
	file.physique.strength = 0.6f;
	file.physique.size = 1.5f;
	file.tattooSlots = std::array<uint32_t, 8> {0x04080C21, 0x000000F0, 0, 0, 0, 0, 0, 0};
	const auto body = FromMindFile(file);
	ASSERT_TRUE(body.has_value());
	EXPECT_EQ(body->species, CreatureType::Leopard);
	EXPECT_EQ(body->name, u"Spot");
	EXPECT_EQ(body->alignment, -0.75f);
	EXPECT_EQ(body->strength, 0.6f);
	EXPECT_EQ(body->size, 1.5f);
	ASSERT_TRUE(body->tattoos.has_value());
	const auto& first = body->tattoos->at(0);
	EXPECT_EQ(first.design, 1);
	EXPECT_EQ(first.site, 2);
	EXPECT_EQ(first.colour.r, 0x04);
	EXPECT_EQ(first.colour.g, 0x08);
	EXPECT_EQ(first.colour.b, 0x0C);
	EXPECT_TRUE(body->tattoos->at(1).Empty());
}

TEST(CreatureMindFileBody, OlderFilesLeaveTheRestToTheSpecies)
{
	creaturemind::MindFileData file;
	file.version = 10;
	file.speciesRow = 0;
	file.physique.strength = 0.4f;
	const auto body = FromMindFile(file);
	ASSERT_TRUE(body.has_value());
	EXPECT_EQ(body->species, CreatureType::GiantApe);
	EXPECT_FALSE(body->alignment.has_value());
	EXPECT_FALSE(body->size.has_value());
	EXPECT_FALSE(body->tattoos.has_value());
}

TEST(CreatureMindFileBody, OutOfRangeValuesAreKeptInBounds)
{
	creaturemind::MindFileData file;
	file.speciesRow = 2;
	file.alignment = 3.0f;
	file.physique.strength = std::numeric_limits<float>::quiet_NaN();
	file.physique.size = -1.0f;
	const auto body = FromMindFile(file);
	ASSERT_TRUE(body.has_value());
	EXPECT_EQ(body->alignment, 1.0f);
	EXPECT_EQ(body->strength, 0.5f);
	EXPECT_FALSE(body->size.has_value());
}

TEST(CreatureMindFileBody, UnknownSpeciesIsNoCreature)
{
	creaturemind::MindFileData file;
	file.speciesRow = 99;
	EXPECT_FALSE(FromMindFile(file).has_value());
}

TEST(CreatureMindFileBody, AGivenSpeciesTakesTheBodyAndIgnoresTheFilesRow)
{
	creaturemind::MindFileData file;
	file.speciesRow = 99;
	file.alignment = 0.5f;
	file.physique.strength = 0.2f;
	file.physique.size = 1.25f;
	const auto body = FromMindFile(file, CreatureType::Tortoise);
	EXPECT_EQ(body.species, CreatureType::Tortoise);
	EXPECT_EQ(body.alignment, 0.5f);
	EXPECT_EQ(body.strength, 0.2f);
	EXPECT_EQ(body.size, 1.25f);
}

namespace
{
/// A file as the creature was loaded from: the values the live body replaces, and some it keeps
creaturemind::MindFileData LoadedFile()
{
	creaturemind::MindFileData file;
	file.speciesRow = 14;
	file.physique = {.turns = 198003,
	                 .age = 88,
	                 .strength = 0.8f,
	                 .fatness = 0.4f,
	                 .previousFatness = 0.4f,
	                 .energy = 0.85f,
	                 .itchiness = 0.0f,
	                 .flags = 2,
	                 .needs = {0.0f, 0.3f, 0.1f, 0.55f, 0.75f, 0.95f},
	                 .size = 2.0f};
	file.inDevScript = 0;
	file.leashFlags = std::array<uint32_t, 4> {7, 0, 0, 0};
	file.tattooSlots = std::array<uint32_t, 8> {};
	file.tattoo = std::vector<uint8_t> {1, 2, 3};
	file.fightHealth = 0.75f;
	return file;
}
} // namespace

TEST(CreatureMindFileBody, TheLiveBodyIsSavedOverTheLoadedFile)
{
	creature_tattoo::Slots tattoos {};
	tattoos[2] = {.design = 13, .site = 5, .colour = {0xAB, 0xCD, 0xEF}};
	const LiveBody body {
	    .fatness = 0.6f,
	    .shownFatness = 0.55f,
	    .needs = creature_physiology::Needs {.age = 90,
	                                         .turns = 202500,
	                                         .warmth = 0.5f,
	                                         .energy = 0.25f,
	                                         .itchiness = 0.125f,
	                                         .poo = 0.5f,
	                                         .exhaustion = 0.375f,
	                                         .dehydration = 0.625f,
	                                         .life = 0.5f},
	    .fightHealth = 0.875f,
	    .inDevScript = true,
	    .leashes = Leashes {.evil = false, .rope = true, .good = true},
	    .tattoos = tattoos,
	};
	const auto saved = ToMindFile(LoadedFile(), body);
	EXPECT_EQ(saved.physique.fatness, 0.6f);
	EXPECT_EQ(saved.physique.previousFatness, 0.55f);
	EXPECT_EQ(saved.physique.turns, 202500u);
	EXPECT_EQ(saved.physique.age, 90u);
	EXPECT_EQ(saved.physique.energy, 0.25f);
	EXPECT_EQ(saved.physique.itchiness, 0.125f);
	EXPECT_EQ(saved.physique.needs, (std::array<float, 6> {0.5f, 0.375f, 0.625f, 0.55f, 0.75f, 0.95f}));
	EXPECT_EQ(saved.inDevScript, 1u);
	// evil, rope and good by the leash type's number; the first type's flag is kept
	EXPECT_EQ(saved.leashFlags, (std::array<uint32_t, 4> {7, 0, 1, 1}));
	ASSERT_TRUE(saved.tattooSlots.has_value());
	EXPECT_EQ(saved.tattooSlots->at(0), 0xF0u);
	EXPECT_EQ(saved.tattooSlots->at(2), 0xABCDEF5Du);
	EXPECT_EQ(saved.tattoo, (std::vector<uint8_t> {1, 2, 3}));
	EXPECT_EQ(saved.fightHealth, 0.875f);
	// what the body does not model stays
	EXPECT_EQ(saved.physique.flags, 2u);
	EXPECT_EQ(saved.physique.strength, 0.8f);
	EXPECT_EQ(saved.physique.size, 2.0f);
}

TEST(CreatureMindFileBody, WithoutABodyOrLeashRulesTheFilesNeedsAndLeashFlagsStay)
{
	const auto saved = ToMindFile(LoadedFile(), LiveBody {.fatness = 0.4f, .shownFatness = 0.4f});
	const auto file = LoadedFile();
	EXPECT_EQ(saved.physique.turns, file.physique.turns);
	EXPECT_EQ(saved.physique.age, file.physique.age);
	EXPECT_EQ(saved.physique.energy, file.physique.energy);
	EXPECT_EQ(saved.physique.itchiness, file.physique.itchiness);
	EXPECT_EQ(saved.physique.needs, file.physique.needs);
	EXPECT_EQ(saved.leashFlags, file.leashFlags);
	EXPECT_EQ(saved.fightHealth, 1.0f);
}

TEST(CreatureMindFileBody, TakesTheFatnessAndTheFatnessItsBodyShowed)
{
	auto file = LoadedFile();
	file.physique.fatness = 0.4f;
	file.physique.previousFatness = 0.37f;
	const auto body = FromMindFile(file);
	ASSERT_TRUE(body.has_value());
	EXPECT_EQ(body->fatness, 0.4f);
	EXPECT_EQ(body->previousFatness, 0.37f);
	// as kept, even out of 0..1
	file.physique.fatness = 1.5f;
	EXPECT_EQ(FromMindFile(file)->fatness, 1.5f);
}

TEST(CreatureMindFileBody, TheShownFatnessIsBroughtTwiceTowardsTheFatness)
{
	EXPECT_FLOAT_EQ(LoadedShownFatness(0.37f, 0.4f), 0.39f);
	EXPECT_EQ(LoadedShownFatness(0.4f, 0.4f), 0.4f);
	EXPECT_FLOAT_EQ(LoadedShownFatness(0.0f, 1.0f), 0.02f);
	EXPECT_FLOAT_EQ(LoadedShownFatness(1.0f, 0.0f), 0.98f);
	EXPECT_FLOAT_EQ(LoadedShownFatness(0.395f, 0.4f), 0.4f);
	// kept to 0..1
	EXPECT_EQ(LoadedShownFatness(0.995f, 2.0f), 1.0f);
	EXPECT_EQ(LoadedShownFatness(0.005f, -1.0f), 0.0f);
	// not a number: the fatness takes the step down, the shown fatness the lower limit
	const auto nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FLOAT_EQ(LoadedShownFatness(0.5f, nan), 0.48f);
	EXPECT_FLOAT_EQ(LoadedShownFatness(nan, 0.4f), 0.01f);
}

TEST(CreatureMindFileBody, TakesTheBodysNeeds)
{
	const auto body = FromMindFile(LoadedFile());
	ASSERT_TRUE(body.has_value());
	EXPECT_EQ(body->needs.turns, 198003u);
	EXPECT_EQ(body->needs.age, 88u);
	EXPECT_EQ(body->needs.energy, 0.85f);
	EXPECT_EQ(body->needs.itchiness, 0.0f);
	EXPECT_EQ(body->needs.poo, 0.0f);
	EXPECT_EQ(body->needs.exhaustion, 0.3f);
	EXPECT_EQ(body->needs.dehydration, 0.1f);
	// before version 6 a file keeps no itchiness
	auto older = LoadedFile();
	older.physique.itchiness.reset();
	EXPECT_FALSE(FromMindFile(older)->needs.itchiness.has_value());
}

TEST(CreatureMindFileBody, TheSavedNeedsReplaceTheBodysAndLeaveTheRest)
{
	const creature_physiology::Needs start {
	    .age = 0, .turns = 0, .warmth = 0.3f, .energy = 1.0f, .itchiness = 0.5f, .life = 0.9f, .meals = 4};
	const SavedNeeds saved {
	    .turns = 22500, .age = 10, .energy = 0.25f, .poo = 0.5f, .exhaustion = 0.75f, .dehydration = 0.125f};
	const auto needs = WithSavedNeeds(start, saved);
	EXPECT_EQ(needs.turns, 22500u);
	EXPECT_EQ(needs.age, 10u);
	EXPECT_EQ(needs.energy, 0.25f);
	EXPECT_EQ(needs.poo, 0.5f);
	EXPECT_EQ(needs.exhaustion, 0.75f);
	EXPECT_EQ(needs.dehydration, 0.125f);
	// no itchiness saved keeps the body's
	EXPECT_EQ(needs.itchiness, 0.5f);
	EXPECT_EQ(needs.warmth, 0.3f);
	EXPECT_EQ(needs.life, 0.9f);
	EXPECT_EQ(needs.meals, 4u);
	auto itchy = saved;
	itchy.itchiness = 0.0f;
	EXPECT_EQ(WithSavedNeeds(start, itchy).itchiness, 0.0f);
}

TEST(CreatureMindFileBody, TattooWordsRoundTrip)
{
	const creature_tattoo::Slot slot {.design = 13, .site = 5, .colour = {0xAB, 0xCD, 0xEF}};
	EXPECT_EQ(creature_tattoo::ToWord(slot), 0xABCDEF5Du);
	EXPECT_EQ(creature_tattoo::FromWord(creature_tattoo::ToWord(slot)), slot);
	EXPECT_TRUE(creature_tattoo::FromWord(0x000000F0).Empty());
}
