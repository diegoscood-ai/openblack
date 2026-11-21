/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureWebPage.h"
#include "support/GameInfoDat.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace openblack::creature_web_page;

namespace
{

/// A creature with a value of its own in every field
Facts FakeCreature()
{
	Facts facts;
	facts.name = u"Blossom";
	facts.health = 0.875f;
	facts.alignment = -0.004f;
	facts.age = 12;
	facts.energy = 0.5f;
	facts.strength = 0.125f;
	facts.fatness = 0.3f;
	facts.exhaustion = 0.0f;
	facts.dehydration = 1.0f;
	facts.itchiness = 0.25f;
	facts.poo = 0.995f;
	facts.tallies = {3, 4, 5, 6, 7, 8, 9};
	facts.desires = {{.name = "impress", .level = 0.5f}, {.name = "be nice", .level = 0.0f}};
	facts.beliefs.at(0) = 2;
	facts.beliefs.at(23) = 1;
	facts.spellReceived = 7;
	facts.attitudeToPlayer = 1.0f;
	facts.turnsLookedAt = 300;
	facts.turnsLived = 600;
	return facts;
}

std::string ValueOf(const std::vector<Field>& fields, std::string_view key)
{
	for (const auto& field : fields)
	{
		if (field.key == key)
		{
			return field.value;
		}
	}
	return "(none)";
}

std::string ReadBytes(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // namespace

TEST(CreatureWebPage, TheValuesAreTheGamesKeysInItsOrder)
{
	const auto fields = FieldsOf(FakeCreature());
	ASSERT_EQ(fields.size(), 11u + 7u + 2u + 24u + 1u + 2u);
	EXPECT_EQ(fields.at(0).key, "name");
	EXPECT_EQ(fields.at(1).key, "health");
	EXPECT_EQ(fields.at(3).key, "age");
	EXPECT_EQ(fields.at(10).key, "amount-of-poo");
	EXPECT_EQ(fields.at(11).key, "num-people-killed");
	EXPECT_EQ(fields.at(17).key, "num-mushrooms");
	EXPECT_EQ(fields.at(18).key, "desire-impress");
	EXPECT_EQ(fields.at(20).key, "beliefs_0");
	EXPECT_EQ(fields.at(43).key, "beliefs_23");
	EXPECT_EQ(fields.at(44).key, "spell_received_1053");
	EXPECT_EQ(fields.at(45).key, "how_nice_player_has_been");
	EXPECT_EQ(fields.at(46).key, "how_much_attention_player_has_given_me");
}

TEST(CreatureWebPage, FractionsHaveTwoDigitsRoundedAsTheGamesLibraryRounds)
{
	const auto fields = FieldsOf(FakeCreature());
	EXPECT_EQ(ValueOf(fields, "name"), "Blossom");
	// an exact half goes up, and a negative value that rounds to nothing keeps its sign
	EXPECT_EQ(ValueOf(fields, "health"), "0.88");
	EXPECT_EQ(ValueOf(fields, "strength"), "0.13");
	EXPECT_EQ(ValueOf(fields, "alignment"), "-0.00");
	// 0.995f is held as 0.99500000476..., up to 1.00
	EXPECT_EQ(ValueOf(fields, "amount-of-poo"), "1.00");
	EXPECT_EQ(ValueOf(fields, "fatness"), "0.30");
	EXPECT_EQ(ValueOf(fields, "dehydration"), "1.00");
	EXPECT_EQ(ValueOf(fields, "exhaustion"), "0.00");
	EXPECT_EQ(ValueOf(fields, "age"), "12");
	EXPECT_EQ(ValueOf(fields, "num-battles-won"), "7");
	EXPECT_EQ(ValueOf(fields, "desire-impress"), "0.50");
	EXPECT_EQ(ValueOf(fields, "beliefs_0"), "2");
	EXPECT_EQ(ValueOf(fields, "beliefs_1"), "0");
	EXPECT_EQ(ValueOf(fields, "spell_received_1053"), "1");
}

TEST(CreatureWebPage, NiceAndAttentionAreTheNumbersOfTheGamesTexts)
{
	auto facts = FakeCreature();
	auto fields = FieldsOf(facts);
	// the most good of the seven words, and half the turns looked at: the third of the five words
	EXPECT_EQ(ValueOf(fields, "how_nice_player_has_been"), std::to_string(0x46A + 6));
	EXPECT_EQ(ValueOf(fields, "how_much_attention_player_has_given_me"), std::to_string(0x471 + 2));
	facts.attitudeToPlayer = -1.0f;
	facts.turnsLived = 0;
	facts.spellReceived.reset();
	fields = FieldsOf(facts);
	EXPECT_EQ(ValueOf(fields, "how_nice_player_has_been"), std::to_string(0x46A));
	EXPECT_EQ(ValueOf(fields, "how_much_attention_player_has_given_me"), std::to_string(0x471));
	EXPECT_EQ(ValueOf(fields, "spell_received_1053"), "(none)");
}

TEST(CreatureWebPage, ThePageIsTheTemplateWithItsKeysFilledIn)
{
	const auto fields = FieldsOf(FakeCreature());
	const std::string templ = "<html>\r\n<b>[$name$]</b> is [$age$], [$health$]\r\n[$unknown$] cut\r\nend";
	// the line with the key that has no value is cut before it, its line break too
	EXPECT_EQ(Fill(templ, fields), "<html>\r\n<b>Blossom</b> is 12, 0.88\r\nend");
}

TEST(CreatureWebPage, AKeyWithNoValueEndsItsLineThere)
{
	const std::vector<Field> fields {{"a", "x"}, {"empty", ""}};
	EXPECT_EQ(Fill("1[$a$]2[$b$]3[$a$]4\n5", fields), "1x25");
	// an empty value takes nothing away
	EXPECT_EQ(Fill(">[$empty$]<", fields), "><");
	// the last value given of a key is the one used
	const std::vector<Field> twice {{"a", "first"}, {"a", "second"}};
	EXPECT_EQ(Fill("[$a$]", twice), "second");
}

TEST(CreatureWebPage, TheClosingCharactersArePassedOverAsTheGameDoes)
{
	const std::vector<Field> fields {{"a", "x"}};
	// the character after the closing "$" goes with it, whatever it is
	EXPECT_EQ(Fill("[$a$]]", fields), "x]");
	EXPECT_EQ(Fill("[$a$!", fields), "x");
	// a "$" at the end of the line is passed over alone
	EXPECT_EQ(Fill("[$a$", fields), "x");
	// a key the line ends in is looked up as it is
	EXPECT_EQ(Fill("<[$a", fields), "<x");
	// a lone "[" or "$" is text
	EXPECT_EQ(Fill("[a] $5", fields), "[a] $5");
}

TEST(CreatureWebPage, TheTemplateIsReadAndWrittenAsATextFile)
{
	const std::vector<Field> fields;
	// a carriage return before a line feed is dropped as the line is read, and every line feed is written with one
	EXPECT_EQ(Fill("a\r\nb\nc\r", fields), "a\r\nb\r\nc\r");
	EXPECT_EQ(Fill("a\r\r\nb", fields), "a\r\r\nb");
	// the text ends at a Ctrl+Z
	EXPECT_EQ(Fill(std::string("ab\x1A"
	                           "cd"),
	               fields),
	          "ab");
	// a zero byte ends its piece of a line
	EXPECT_EQ(Fill(std::string("ab\0cd\nef", 8), fields), "abef");
}

TEST(CreatureWebPage, LongLinesAreReadInPiecesOf2047Characters)
{
	const std::vector<Field> fields {{"a", "x"}};
	// a key split across two pieces is not a key in either
	const std::string line = std::string(2046, '.') + "[$a$]";
	const auto page = Fill(line, fields);
	EXPECT_EQ(page, line);
}

TEST(CreatureWebPage, AnAsciiNameStaysAsItIs)
{
	EXPECT_EQ(Narrow(u"Abc"), "Abc");
	EXPECT_EQ(Narrow(u""), "");
	// the name ends at its first zero, as the game's conversion reads it
	EXPECT_EQ(Narrow(std::u16string(u"Ab\0c", 4)), "Ab");
	// a name of 0x3FF bytes or more does not fit the game's buffer and gives nothing
	EXPECT_EQ(Narrow(std::u16string(0x3FE, u'a')), std::string(0x3FE, 'a'));
	EXPECT_EQ(Narrow(std::u16string(0x3FF, u'a')), "");
	EXPECT_EQ(PagePath(u"Blossom"), std::filesystem::path("html") / "Blossom.html");
}

// The machine's own code page with Windows' best fit, as the game converts the name: on a machine of the Western code
// page (1252) a letter it lacks is given as the letter it comes from, and a character with none as "?". Elsewhere the
// Western code page without best fit
TEST(CreatureWebPage, TheNameIsConvertedAsTheGameConvertsIt)
{
#ifdef _WIN32
	if (GetACP() != 1252)
	{
		GTEST_SKIP() << "the machine's code page is " << GetACP() << ", not the Western one these values are for";
	}
	EXPECT_EQ(Narrow(u"\u00E9t\u00E9"), "\xE9t\xE9");
	EXPECT_EQ(Narrow(u"\u20AC\u0160"), "\x80\x8A");
	EXPECT_EQ(Narrow(u"\u0100"), "A");
	EXPECT_EQ(Narrow(u"\u3042"), "?");
#else
	EXPECT_EQ(Narrow(u"\u00E9t\u00E9"), "\xE9t\xE9");
	EXPECT_EQ(Narrow(u"\u20AC\u0160"), "\x80\x8A");
	EXPECT_EQ(Narrow(u"\u0100"), "?");
	EXPECT_EQ(Narrow(u"\U0001F600"), "?");
#endif
}

TEST(CreatureWebPage, TheMindIsSavedEvery600TurnsFromTheFirst600)
{
	EXPECT_FALSE(IsSaveTurn(0));
	EXPECT_FALSE(IsSaveTurn(599));
	EXPECT_TRUE(IsSaveTurn(600));
	EXPECT_FALSE(IsSaveTurn(601));
	EXPECT_TRUE(IsSaveTurn(1200));
}

// The game's own page in the game's data, written by the original for a creature called sisi: the shipped template
// with these values gives it byte for byte. Skipped without the game's data
TEST(CreatureWebPage, TheShippedTemplateGivesTheOriginalsPage)
{
	const auto game = openblack::test::GamePath();
	if (!game.has_value() || !std::filesystem::exists(*game / "Profiles" / "_s_i_s_i" / "html" / "sisi.html"))
	{
		GTEST_SKIP() << "no game data with the original's page";
	}
	const auto templ = ReadBytes(*game / "Scripts" / "creature_html.tmpl");
	const auto original = ReadBytes(*game / "Profiles" / "_s_i_s_i" / "html" / "sisi.html");
	const std::vector<Field> fields {
	    {"name", "sisi"},   {"age", "0"},       {"alignment", "-0.01"},     {"strength", "0.54"},
	    {"health", "1.00"}, {"energy", "0.68"}, {"num-people-killed", "1"}, {"num-battles-won", "0"},
	};
	EXPECT_EQ(Fill(templ, fields), original);
}

// The desires are named on the page by a name of their row in info.dat: the first one's is "impress"
TEST(CreatureWebPage, TheDesiresAreNamedByTheirRowsInTheTables)
{
	const auto game = openblack::test::GamePath();
	const auto info = game.has_value() ? openblack::test::ReadInfoDat(*game) : nullptr;
	if (info == nullptr)
	{
		GTEST_SKIP() << "no info.dat";
	}
	const auto& name = info->creatureInitialDesire.at(0).field0xf0;
	EXPECT_EQ(std::string(name.data()), "impress");
}
