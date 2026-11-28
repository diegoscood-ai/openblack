/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the story's land scripts make, counted from their lines as openblack's land-script reader takes them: the
// LHScriptX lexer's tokens, a line being a command, its brackets and its arguments with a comma between each two. Each
// land's test checks that every line of its script is a command of the command table with the arguments its format
// asks for, so that the reader runs the script from its first line to its last, and that the script makes what the
// land's progress page counts (docs/wiki/project/progress/scripts/land1_script.md and the other lands' pages). The
// reading and the counting are first checked on a small script written here; the lands' tests need the original
// game's data (OPENBLACK_GAME_PATH) and skip themselves without it.

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "Enums.h"
#include "LHScriptX/Lexer.h"
#include "LHScriptX/ScriptWriter.h"

using namespace openblack;
using openblack::lhscriptx::Lexer;
using openblack::lhscriptx::LexerException;
using openblack::lhscriptx::Operator;
using openblack::lhscriptx::Token;

namespace
{
/// One command line of a land script: the command, its arguments in order and the line it is on, from 1
struct ScriptLine
{
	std::string command;
	std::vector<Token> arguments;
	int line;
};

/// A land script as the reader takes it: its command lines, and the lines it skips (a character the lexer does not
/// take, or anything but a command with its arguments in brackets)
struct ReadScript
{
	std::vector<ScriptLine> lines;
	std::vector<int> skippedLines;
};

/// One line's tokens as a command line: a name, "(", nothing or the arguments with a comma between each two, then ")";
/// nothing when the tokens are anything else
std::optional<ScriptLine> AsCommandLine(const std::vector<Token>& tokens, int line)
{
	if (tokens.size() < 3 || !tokens.front().IsIdentifier() || !tokens.at(1).IsOP(Operator::LeftParentheses) ||
	    !tokens.back().IsOP(Operator::RightParentheses))
	{
		return std::nullopt;
	}
	ScriptLine command {.command = tokens.front().Identifier(), .arguments = {}, .line = line};
	const auto last = tokens.size() - 1;
	for (size_t i = 2; i < last; i += 2)
	{
		// an argument is a name, a quoted text or a number, followed by the closing bracket or by a comma and another
		const auto& argument = tokens.at(i);
		if (argument.GetType() == Token::Type::Operator)
		{
			return std::nullopt;
		}
		command.arguments.push_back(argument);
		if (i + 1 == last)
		{
			break;
		}
		if (!tokens.at(i + 1).IsOP(Operator::Comma) || i + 2 == last)
		{
			return std::nullopt;
		}
	}
	return command;
}

/// A land script's text read line by line with the reader's lexer, which passes over remarks, star lines and comments
ReadScript ReadLandScript(const std::string& source)
{
	ReadScript script;
	Lexer lexer(source);
	std::vector<Token> tokens;
	bool unreadable = false;
	int line = 1;
	const auto endOfLine = [&]() {
		if (unreadable || !tokens.empty())
		{
			std::optional<ScriptLine> command;
			if (!unreadable)
			{
				command = AsCommandLine(tokens, line);
			}
			if (command.has_value())
			{
				script.lines.push_back(std::move(*command));
			}
			else
			{
				script.skippedLines.push_back(line);
			}
		}
		tokens.clear();
		unreadable = false;
	};
	while (true)
	{
		std::optional<Token> token;
		try
		{
			token = lexer.GetToken();
		}
		catch (const LexerException&)
		{
			// the lexer has stepped over the character it does not take; the line is skipped
			unreadable = true;
			continue;
		}
		if (token->IsEOF())
		{
			endOfLine();
			break;
		}
		if (token->IsEOL())
		{
			endOfLine();
			++line;
			continue;
		}
		tokens.push_back(std::move(*token));
	}
	return script;
}

/// Whether a line is a command of the command table with one argument for each value its format asks for: a number
/// (an integer or a float, as the reader turns one into the other) for an N or an F, a text for an A or an L
bool MatchesItsFormat(const ScriptLine& line)
{
	const auto* format = lhscriptx::FindCommandFormat(line.command);
	if (format == nullptr)
	{
		return false;
	}
	std::string values;
	std::ranges::copy_if(format->types, std::back_inserter(values),
	                     [](char letter) { return letter == 'N' || letter == 'F' || letter == 'A' || letter == 'L'; });
	if (values.size() != line.arguments.size())
	{
		return false;
	}
	for (size_t i = 0; i < values.size(); ++i)
	{
		const auto type = line.arguments.at(i).GetType();
		const bool number = type == Token::Type::Integer || type == Token::Type::Float;
		const bool text = type == Token::Type::String || type == Token::Type::Identifier;
		const bool wantsNumber = values.at(i) == 'N' || values.at(i) == 'F';
		if (wantsNumber ? !number : !text)
		{
			return false;
		}
	}
	return true;
}

/// The lines the reader does not run, in order: those it skips and the command lines that do not match their format
std::vector<int> LinesNotLoaded(const ReadScript& script)
{
	auto lines = script.skippedLines;
	for (const auto& line : script.lines)
	{
		if (!MatchesItsFormat(line))
		{
			lines.push_back(line.line);
		}
	}
	std::ranges::sort(lines);
	return lines;
}

/// A text argument that ends with this
struct EndsWith
{
	std::string_view suffix;
};

/// What one argument of a line is to be: this text, this integer (a kind, a row of an info table) or a text ending so
struct Where
{
	size_t argument;
	std::variant<std::string_view, int32_t, EndsWith> value;
};

[[nodiscard]] Where Text(size_t argument, std::string_view text)
{
	return {.argument = argument, .value = text};
}

[[nodiscard]] Where Ending(size_t argument, std::string_view suffix)
{
	return {.argument = argument, .value = EndsWith {.suffix = suffix}};
}

template <typename Info>
[[nodiscard]] Where Kind(size_t argument, Info kind)
{
	return {.argument = argument, .value = static_cast<int32_t>(kind)};
}

/// Whether a line's argument is what is asked
bool Holds(const ScriptLine& line, const Where& where)
{
	if (where.argument >= line.arguments.size())
	{
		return false;
	}
	const auto& argument = line.arguments.at(where.argument);
	const bool text = argument.IsString() || argument.IsIdentifier();
	if (const auto* want = std::get_if<std::string_view>(&where.value))
	{
		return text && argument.StringValue() == *want;
	}
	if (const auto* want = std::get_if<EndsWith>(&where.value))
	{
		return text && argument.StringValue().ends_with(want->suffix);
	}
	return argument.GetType() == Token::Type::Integer && *argument.IntegerValue() == std::get<int32_t>(where.value);
}

/// How many of the script's lines that the reader runs are this command with every argument asked
[[nodiscard]] size_t Count(const ReadScript& script, std::string_view command, std::initializer_list<Where> where = {})
{
	return static_cast<size_t>(std::ranges::count_if(script.lines, [&](const ScriptLine& line) {
		return line.command == command && MatchesItsFormat(line) &&
		       std::ranges::all_of(where, [&line](const Where& w) { return Holds(line, w); });
	}));
}

// The argument that says what a command makes, by its place in the line from 0
constexpr size_t k_TownOwner = 2;          // CREATE_TOWN(number, place, owner, ..., tribe)
constexpr size_t k_TownTribe = 4;          // CREATE_TOWN's tribe
constexpr size_t k_TownSpellName = 1;      // CREATE_NEW_TOWN_SPELL(town, miracle)
constexpr size_t k_PlannedAbodeName = 2;   // CREATE_PLANNED_ABODE(town, place, building, ...)
constexpr size_t k_SiteOwner = 2;          // CREATE_WORSHIP_SITE(place, ..., owner, tribe, ...)
constexpr size_t k_SiteTribe = 3;          // CREATE_WORSHIP_SITE's tribe
constexpr size_t k_DispenserMiracle = 3;   // CREATE_SPELL_DISPENSER(..., place, building, miracle, ...)
constexpr size_t k_OneShotMiracle = 1;     // CREATE_ONE_SHOT_SPELL_PU(place, miracle)
constexpr size_t k_TreeKind = 2;           // CREATE_NEW_TREE(forest, place, kind, ...)
constexpr size_t k_FeatureName = 1;        // CREATE_NEW_FEATURE(place, name, ...)
constexpr size_t k_FieldKind = 2;          // CREATE_NEW_TOWN_FIELD(town, place, kind, ...)
constexpr size_t k_AnimalKind = 1;         // CREATE_NEW_ANIMAL(place, kind, ...)
constexpr size_t k_MobileStaticKind = 1;   // CREATE_MOBILE_STATIC(place, kind, ...)
constexpr size_t k_MobileObjectKind = 1;   // CREATE_MOBILEOBJECT(place, kind, ...)
constexpr size_t k_AnimatedStaticName = 1; // CREATE_ANIMATED_STATIC(place, name, ...)
constexpr size_t k_LanternKind = 1;        // CREATE_STREET_LANTERN(place, kind)

[[nodiscard]] size_t Trees(const ReadScript& script, TreeInfo kind)
{
	return Count(script, "CREATE_NEW_TREE", {Kind(k_TreeKind, kind)});
}

[[nodiscard]] size_t Fields(const ReadScript& script, FieldTypeInfo kind)
{
	return Count(script, "CREATE_NEW_TOWN_FIELD", {Kind(k_FieldKind, kind)});
}

[[nodiscard]] size_t Animals(const ReadScript& script, AnimalInfo kind)
{
	return Count(script, "CREATE_NEW_ANIMAL", {Kind(k_AnimalKind, kind)});
}

[[nodiscard]] size_t Statics(const ReadScript& script, MobileStaticInfo kind)
{
	return Count(script, "CREATE_MOBILE_STATIC", {Kind(k_MobileStaticKind, kind)});
}

[[nodiscard]] size_t Objects(const ReadScript& script, MobileObjectInfo kind)
{
	return Count(script, "CREATE_MOBILEOBJECT", {Kind(k_MobileObjectKind, kind)});
}

[[nodiscard]] size_t Lanterns(const ReadScript& script, MobileStaticInfo kind)
{
	return Count(script, "CREATE_STREET_LANTERN", {Kind(k_LanternKind, kind)});
}

/// A count added to a running total, and given back
size_t Tally(size_t& total, size_t count)
{
	total += count;
	return count;
}

/// The original game's install, from OPENBLACK_GAME_PATH; absent when it is not set
std::optional<std::filesystem::path> RealGamePath()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	return std::filesystem::path(game);
}

/// A land script of the install's Scripts folder as the reader takes it, or nothing without the file
std::optional<ReadScript> ReadRealLandScript(const std::filesystem::path& game, std::string_view land)
{
	std::ifstream file(game / "Scripts" / (std::string(land) + ".txt"), std::ios::binary);
	if (!file.is_open())
	{
		return std::nullopt;
	}
	const std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	return ReadLandScript(source);
}

/// A land's footpath file as the game looks for it: the script's name in lower case with ".fot", in the landscapes'
/// folder
std::filesystem::path FootpathFileOf(const std::filesystem::path& game, std::string_view land)
{
	std::string name(land);
	std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return game / "Data" / "Landscape" / (name + ".fot");
}
} // namespace

// The reader's lines of a small script: the remark, star, empty and comment lines are passed over; a line with a
// character the lexer does not take or a comma with no argument after it is skipped; a command missing from the table,
// with too many arguments or a text where its format has a number is not run. Only the lines run are counted, by
// command and by their arguments' texts, endings and kinds.
TEST(LandScriptContents, TheReaderTakesEachLineAndCountsWhatItsCommandsMake)
{
	const std::string source = "VERSION(2.300000)\r\n"
	                           "REM a remark\r\n"
	                           "**** Player[0] ****\r\n"
	                           "\r\n"
	                           "LOAD_LANDSCAPE(\".\\DATA\\LANDSCAPE\\Land9.lnd\")\r\n"
	                           "CREATE_TOWN(0, \"10.00,20.00\", \"PLAYER_ONE\", 7, \"NORSE\")\r\n"
	                           "CREATE_TOWN(1, \"30.00,20.00\", \"NEUTRAL\", 0, \"CELTIC\")\r\n"
	                           "CREATE_NEW_TREE(-1, \"1.00,2.00\", 13, 1, 0.500000, 1.000000, 1.000000)\r\n"
	                           "CREATE_NEW_TREE(-1, \"1.00,3.00\", 13, 1, 0.500000, 1.000000, 1.000000)\r\n"
	                           "CREATE_NEW_TREE(-1, \"1.00,4.00\", 4, 1, 0.500000, 1.000000, 1.000000)\r\n"
	                           "CREATE_PLANNED_ABODE(0, \"10.00,20.00\", \"NORSE_ABODE_A\", 0, 1000, 0, 0)\r\n"
	                           "CREATE_PLANNED_ABODE(1, \"30.00,20.00\", \"CELTIC_ABODE_WONDER\", 0, 1000, 0, 0)\r\n"
	                           "CREATE_WORSHIP_SITE(\"5.00,5.00\", 3, \"PLAYER_TWO\", \"NORSE\", 0, 1000)\r\n"
	                           "// CREATE_TOWN(2, \"50.00,20.00\", \"NEUTRAL\", 0, \"GREEK\")\r\n"
	                           "CREATE_SOMETHING_ELSE(1)\r\n"
	                           "SET_TOWN_UNINHABITABLE(1, 2)\r\n"
	                           "CREATE_STREAM(\"0\")\r\n"
	                           "CREATE_STREAM(0#)\r\n"
	                           "CREATE_STREAM(0, )\r\n"
	                           "CREATE_STREAM_POINT(0, \"1.00,1.00\")\n";
	const auto script = ReadLandScript(source);

	std::vector<int> lines;
	std::ranges::transform(script.lines, std::back_inserter(lines), &ScriptLine::line);
	EXPECT_EQ(lines, (std::vector<int> {1, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 20}));
	EXPECT_EQ(script.skippedLines, (std::vector<int> {18, 19}));
	EXPECT_EQ(LinesNotLoaded(script), (std::vector<int> {15, 16, 17, 18, 19}));
	ASSERT_EQ(script.lines.at(1).arguments.size(), 1u);
	EXPECT_EQ(script.lines.at(1).arguments.front().StringValue(), ".\\DATA\\LANDSCAPE\\Land9.lnd");

	EXPECT_EQ(Count(script, "LOAD_LANDSCAPE", {Ending(0, "\\Land9.lnd")}), 1u);
	EXPECT_EQ(Count(script, "LOAD_LANDSCAPE", {Ending(0, "\\Land1.lnd")}), 0u);
	EXPECT_EQ(Count(script, "CREATE_TOWN"), 2u);
	EXPECT_EQ(Count(script, "CREATE_TOWN", {Text(k_TownTribe, "NORSE")}), 1u);
	EXPECT_EQ(Count(script, "CREATE_TOWN", {Text(k_TownOwner, "NEUTRAL")}), 1u);
	EXPECT_EQ(Count(script, "CREATE_TOWN", {Text(k_TownOwner, "PLAYER_TWO")}), 0u);
	EXPECT_EQ(Count(script, "CREATE_NEW_TREE"), 3u);
	EXPECT_EQ(Trees(script, TreeInfo::Bush), 2u);
	EXPECT_EQ(Trees(script, TreeInfo::ConiferA), 1u);
	EXPECT_EQ(Trees(script, TreeInfo::Oak), 0u);
	EXPECT_EQ(Count(script, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_A")}), 1u);
	EXPECT_EQ(Count(script, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_WONDER")}), 1u);
	EXPECT_EQ(Count(script, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_D")}), 0u);
	EXPECT_EQ(Count(script, "CREATE_WORSHIP_SITE", {Text(k_SiteTribe, "NORSE"), Text(k_SiteOwner, "PLAYER_TWO")}), 1u);
	EXPECT_EQ(Count(script, "CREATE_WORSHIP_SITE", {Text(k_SiteTribe, "CELTIC"), Text(k_SiteOwner, "PLAYER_TWO")}), 0u);
	// an argument past the line's last is never what is asked
	EXPECT_EQ(Count(script, "CREATE_STREAM_POINT", {Text(5, "1.00,1.00")}), 0u);
	// the lines the reader does not run make nothing
	EXPECT_EQ(Count(script, "SET_TOWN_UNINHABITABLE"), 0u);
	EXPECT_EQ(Count(script, "CREATE_STREAM"), 0u);
	EXPECT_EQ(Count(script, "CREATE_STREAM_POINT"), 1u);

	size_t trees = 0;
	EXPECT_EQ(Tally(trees, Trees(script, TreeInfo::Bush)), 2u);
	EXPECT_EQ(Tally(trees, Trees(script, TreeInfo::ConiferA)), 1u);
	EXPECT_EQ(trees, 3u);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
/// With OPENBLACK_GAME_PATH set to the install: Land 1's script loads from its first line to its last and
/// makes what its progress page counts (docs/wiki/project/progress/scripts/land1_script.md)
TEST(LandScriptContents, realLand1Script)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto script = ReadRealLandScript(*game, "Land1");
	ASSERT_TRUE(script.has_value());
	const auto& land = *script;

	// every line is a command of the table with the arguments its format asks for, so none is skipped
	EXPECT_FALSE(land.lines.empty());
	EXPECT_EQ(LinesNotLoaded(land), std::vector<int> {});

	// the land's footpath file is where the game looks for it
	EXPECT_TRUE(std::filesystem::exists(FootpathFileOf(*game, "Land1")));

	// the landscape
	EXPECT_EQ(Count(land, "LOAD_LANDSCAPE", {Ending(0, "\\Land1.lnd")}), 1u);

	// the towns, by tribe and by owner
	EXPECT_EQ(Count(land, "CREATE_TOWN"), 6u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "NORSE")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "CELTIC")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "JAPANESE")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "AZTEC")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownOwner, "NEUTRAL")}), 5u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownOwner, "PLAYER_ONE")}), 1u);

	// the towns left uninhabitable
	EXPECT_EQ(Count(land, "SET_TOWN_UNINHABITABLE"), 4u);

	// the planned buildings, by the kind of building
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE"), 6u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_A")}), 3u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_WONDER")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_D")}), 1u);

	// the trees and bushes, by kind
	EXPECT_EQ(Count(land, "CREATE_NEW_TREE"), 1395u);
	EXPECT_EQ(Trees(land, TreeInfo::Bush), 378u);
	EXPECT_EQ(Trees(land, TreeInfo::ConiferA), 206u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmB), 194u);
	EXPECT_EQ(Trees(land, TreeInfo::Conifer), 116u);
	EXPECT_EQ(Trees(land, TreeInfo::Pine), 113u);
	EXPECT_EQ(Trees(land, TreeInfo::Palm), 108u);
	EXPECT_EQ(Trees(land, TreeInfo::Birch), 77u);
	EXPECT_EQ(Trees(land, TreeInfo::Beech), 73u);
	EXPECT_EQ(Trees(land, TreeInfo::Oak), 38u);
	EXPECT_EQ(Trees(land, TreeInfo::BushB), 36u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmA), 16u);
	EXPECT_EQ(Trees(land, TreeInfo::OakA), 11u);
	EXPECT_EQ(Trees(land, TreeInfo::Olive), 10u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmC), 10u);
	EXPECT_EQ(Trees(land, TreeInfo::BushA), 6u);
	EXPECT_EQ(Trees(land, TreeInfo::Burnt), 3u);

	// the big forest models
	EXPECT_EQ(Count(land, "CREATE_NEW_BIG_FOREST"), 3u);

	// the features, by name
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Aztec Statue Feature")}), 9u);
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Fat Pilar Lime")}), 3u);
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Pilar2 Lime")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Pilar3 Lime")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Spikey Pilar Lime")}), 2u);

	// the patches of mist
	EXPECT_EQ(Count(land, "CREATE_MIST"), 17u);

	// the fields, by kind
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_FIELD"), 19u);
	EXPECT_EQ(Fields(land, FieldTypeInfo::Wheat), 18u);
	EXPECT_EQ(Fields(land, FieldTypeInfo::Corn), 1u);

	// the animals, by kind
	EXPECT_EQ(Animals(land, AnimalInfo::Dove), 40u);
	EXPECT_EQ(Animals(land, AnimalInfo::Seagull), 22u);
	EXPECT_EQ(Animals(land, AnimalInfo::Swallow), 14u);
	EXPECT_EQ(Animals(land, AnimalInfo::Horse), 12u);
	EXPECT_EQ(Animals(land, AnimalInfo::Cow), 10u);
	EXPECT_EQ(Animals(land, AnimalInfo::Pig), 7u);
	EXPECT_EQ(Animals(land, AnimalInfo::Tortoise), 6u);
	EXPECT_EQ(Animals(land, AnimalInfo::Bat), 5u);

	// the flocks
	EXPECT_EQ(Count(land, "CREATE_FLOCK"), 16u);

	// the rocks, fences, toys, totems and stones, by kind
	size_t rocks = 0;
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Rock)), 70u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder3Lime)), 20u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder1Lime)), 19u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::LongrockLimestone)), 6u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::RockLimestone)), 5u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::SharprockLimestone)), 5u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder2Lime)), 5u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder1Sand)), 1u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder3Chalk)), 1u);
	EXPECT_EQ(rocks, 132u);
	size_t fences = 0;
	EXPECT_EQ(Tally(fences, Statics(land, MobileStaticInfo::CeltFenceTall)), 64u);
	EXPECT_EQ(Tally(fences, Statics(land, MobileStaticInfo::CeltFenceShort)), 38u);
	EXPECT_EQ(fences, 102u);
	size_t toys = 0;
	EXPECT_EQ(Tally(toys, Statics(land, MobileStaticInfo::ToyDie)), 3u);
	EXPECT_EQ(Tally(toys, Statics(land, MobileStaticInfo::ToyCuddly)), 1u);
	EXPECT_EQ(toys, 4u);
	size_t totems = 0;
	EXPECT_EQ(Tally(totems, Statics(land, MobileStaticInfo::GateTotemBlank)), 1u);
	EXPECT_EQ(Tally(totems, Statics(land, MobileStaticInfo::GateTotemTiger)), 1u);
	EXPECT_EQ(Tally(totems, Statics(land, MobileStaticInfo::GateTotemApe)), 1u);
	EXPECT_EQ(totems, 3u);

	// the objects that can be picked up, by kind
	EXPECT_EQ(Objects(land, MobileObjectInfo::MagicMushroom), 36u);
	EXPECT_EQ(Objects(land, MobileObjectInfo::Toadstool), 9u);
	EXPECT_EQ(Objects(land, MobileObjectInfo::EgyptBarrel), 4u);
	EXPECT_EQ(Objects(land, MobileObjectInfo::Champi), 2u);

	// the animated statics, by name
	EXPECT_EQ(Count(land, "CREATE_ANIMATED_STATIC", {Text(k_AnimatedStaticName, "Piper Cave Entrance")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_ANIMATED_STATIC", {Text(k_AnimatedStaticName, "Norse Gate")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_ANIMATED_STATIC", {Text(k_AnimatedStaticName, "Gate Stone Plinth")}), 1u);

	// the lanterns, by kind
	EXPECT_EQ(Count(land, "CREATE_STREET_LANTERN"), 12u);
	EXPECT_EQ(Lanterns(land, MobileStaticInfo::StreetLantern), 8u);
	EXPECT_EQ(Lanterns(land, MobileStaticInfo::CountryLantern), 4u);

	// the streams and their points
	EXPECT_EQ(Count(land, "CREATE_STREAM"), 11u);
	EXPECT_EQ(Count(land, "CREATE_STREAM_POINT"), 187u);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
/// With OPENBLACK_GAME_PATH set to the install: Land 2's script loads from its first line to its last and
/// makes what its progress page counts (docs/wiki/project/progress/scripts/land2_script.md)
TEST(LandScriptContents, realLand2Script)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto script = ReadRealLandScript(*game, "Land2");
	ASSERT_TRUE(script.has_value());
	const auto& land = *script;

	// every line is a command of the table with the arguments its format asks for, so none is skipped
	EXPECT_FALSE(land.lines.empty());
	EXPECT_EQ(LinesNotLoaded(land), std::vector<int> {});

	// the land's footpath file is where the game looks for it
	EXPECT_TRUE(std::filesystem::exists(FootpathFileOf(*game, "Land2")));

	// the landscape
	EXPECT_EQ(Count(land, "LOAD_LANDSCAPE", {Ending(0, "\\Land2.lnd")}), 1u);

	// the towns, by tribe and by owner
	EXPECT_EQ(Count(land, "CREATE_TOWN"), 15u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "CELTIC")}), 5u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "NORSE")}), 4u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "INDIAN")}), 4u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "GREEK")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownOwner, "NEUTRAL")}), 9u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownOwner, "PLAYER_THREE")}), 3u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownOwner, "PLAYER_TWO")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownOwner, "PLAYER_ONE")}), 1u);

	// the towns left uninhabitable
	EXPECT_EQ(Count(land, "SET_TOWN_UNINHABITABLE"), 4u);

	// the miracles the towns offer at their village centres
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL"), 22u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "HEAL")}), 3u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "FIRE")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "FOOD")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "TELEPORT")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "PHYSICAL_SHIELD")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "LIGHTNING_BOLT")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "SHIELD")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "NATURE")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "WOOD")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "CREATURE_SPELL_ITCHY")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "WATER")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "CREATURE_SPELL_ANGRY")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "STORM")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "CREATURE_SPELL_STRONG")}), 1u);

	// the planned buildings, by the kind of building
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE"), 89u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_A")}), 32u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_F")}), 18u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_C")}), 8u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_E")}), 7u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_WORKSHOP")}), 6u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_CRECHE")}), 6u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_B")}), 4u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_WONDER")}), 4u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_GRAVEYARD")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_D")}), 2u);

	// the worship sites, by tribe and by owner
	EXPECT_EQ(Count(land, "CREATE_WORSHIP_SITE"), 2u);
	EXPECT_EQ(Count(land, "CREATE_WORSHIP_SITE", {Text(k_SiteTribe, "NORSE"), Text(k_SiteOwner, "PLAYER_TWO")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_WORSHIP_SITE", {Text(k_SiteTribe, "CELTIC"), Text(k_SiteOwner, "PLAYER_THREE")}), 1u);

	// the trees and bushes, by kind
	EXPECT_EQ(Count(land, "CREATE_NEW_TREE"), 921u);
	EXPECT_EQ(Trees(land, TreeInfo::ConiferA), 264u);
	EXPECT_EQ(Trees(land, TreeInfo::Bush), 187u);
	EXPECT_EQ(Trees(land, TreeInfo::Pine), 178u);
	EXPECT_EQ(Trees(land, TreeInfo::Conifer), 104u);
	EXPECT_EQ(Trees(land, TreeInfo::Oak), 43u);
	EXPECT_EQ(Trees(land, TreeInfo::BushB), 43u);
	EXPECT_EQ(Trees(land, TreeInfo::Beech), 29u);
	EXPECT_EQ(Trees(land, TreeInfo::Birch), 21u);
	EXPECT_EQ(Trees(land, TreeInfo::OakA), 18u);
	EXPECT_EQ(Trees(land, TreeInfo::Palm), 10u);
	EXPECT_EQ(Trees(land, TreeInfo::BushA), 6u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmB), 5u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmC), 5u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmA), 4u);
	EXPECT_EQ(Trees(land, TreeInfo::Olive), 4u);

	// the big forest models
	EXPECT_EQ(Count(land, "CREATE_NEW_BIG_FOREST"), 10u);

	// the features, by name
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Spikey Pilar Lime")}), 7u);
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Pilar2 Lime")}), 3u);
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Pilar3 Lime")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Pier")}), 1u);

	// the fields, by kind
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_FIELD"), 60u);
	EXPECT_EQ(Fields(land, FieldTypeInfo::Wheat), 34u);
	EXPECT_EQ(Fields(land, FieldTypeInfo::WheatWithFence), 22u);
	EXPECT_EQ(Fields(land, FieldTypeInfo::Corn), 4u);

	// the animals, by kind
	EXPECT_EQ(Animals(land, AnimalInfo::Sheep), 111u);
	EXPECT_EQ(Animals(land, AnimalInfo::Horse), 79u);
	EXPECT_EQ(Animals(land, AnimalInfo::Cow), 50u);
	EXPECT_EQ(Animals(land, AnimalInfo::Seagull), 47u);
	EXPECT_EQ(Animals(land, AnimalInfo::Crow), 41u);
	EXPECT_EQ(Animals(land, AnimalInfo::Pig), 33u);
	EXPECT_EQ(Animals(land, AnimalInfo::Dove), 19u);
	EXPECT_EQ(Animals(land, AnimalInfo::Pigeon), 18u);
	EXPECT_EQ(Animals(land, AnimalInfo::Wolf), 16u);
	EXPECT_EQ(Animals(land, AnimalInfo::Tiger), 3u);
	EXPECT_EQ(Animals(land, AnimalInfo::Tortoise), 3u);
	EXPECT_EQ(Animals(land, AnimalInfo::Lion), 2u);

	// the flocks
	EXPECT_EQ(Count(land, "CREATE_FLOCK"), 45u);

	// the rocks, fences, toys, totems and stones, by kind
	size_t rocks = 0;
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::RockLimestone)), 239u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder3Lime)), 32u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder2Lime)), 31u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder1Lime)), 28u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::SharprockLimestone)), 15u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::SquarerockLimestone)), 11u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::FlatrockLimestone)), 8u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::LongrockLimestone)), 7u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder3Sand)), 4u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder2Sand)), 2u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::LongrockSandstone)), 1u);
	EXPECT_EQ(rocks, 378u);
	size_t fences = 0;
	EXPECT_EQ(Tally(fences, Statics(land, MobileStaticInfo::CeltFenceTall)), 181u);
	EXPECT_EQ(Tally(fences, Statics(land, MobileStaticInfo::CeltFenceShort)), 84u);
	EXPECT_EQ(fences, 265u);

	// the lanterns, by kind
	EXPECT_EQ(Count(land, "CREATE_STREET_LANTERN"), 39u);
	EXPECT_EQ(Lanterns(land, MobileStaticInfo::StreetLantern), 36u);
	EXPECT_EQ(Lanterns(land, MobileStaticInfo::CountryLantern), 3u);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
/// With OPENBLACK_GAME_PATH set to the install: Land 3's script loads from its first line to its last and
/// makes what its progress page counts (docs/wiki/project/progress/scripts/land3_script.md)
TEST(LandScriptContents, realLand3Script)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto script = ReadRealLandScript(*game, "Land3");
	ASSERT_TRUE(script.has_value());
	const auto& land = *script;

	// every line is a command of the table with the arguments its format asks for, so none is skipped
	EXPECT_FALSE(land.lines.empty());
	EXPECT_EQ(LinesNotLoaded(land), std::vector<int> {});

	// the land's footpath file is where the game looks for it
	EXPECT_TRUE(std::filesystem::exists(FootpathFileOf(*game, "Land3")));

	// the landscape
	EXPECT_EQ(Count(land, "LOAD_LANDSCAPE", {Ending(0, "\\Land3.lnd")}), 1u);

	// the towns, by tribe and by owner
	EXPECT_EQ(Count(land, "CREATE_TOWN"), 6u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "CELTIC")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "TIBETAN")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "EGYPTIAN")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "INDIAN")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "JAPANESE")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownOwner, "NEUTRAL")}), 3u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownOwner, "PLAYER_TWO")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownOwner, "PLAYER_ONE")}), 1u);

	// the towns left uninhabitable
	EXPECT_EQ(Count(land, "SET_TOWN_UNINHABITABLE"), 1u);

	// the miracles the towns offer at their village centres
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL"), 23u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "FIRE")}), 3u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "LIGHTNING_BOLT")}), 3u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "HEAL")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "FOOD")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "STORM")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "WATER")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "TELEPORT")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "SHIELD")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "PHYSICAL_SHIELD")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "FIRE_PU1")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "NATURE")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "WATER_PU1")}), 1u);

	// the planned buildings, by the kind of building
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE"), 70u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_F")}), 21u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_B")}), 11u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_C")}), 11u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_A")}), 9u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_D")}), 9u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_E")}), 4u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_CRECHE")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_TOWN_CENTRE")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_WORKSHOP")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_WONDER")}), 1u);

	// the worship sites, by tribe and by owner
	EXPECT_EQ(Count(land, "CREATE_WORSHIP_SITE"), 2u);
	EXPECT_EQ(Count(land, "CREATE_WORSHIP_SITE", {Text(k_SiteTribe, "EGYPTIAN"), Text(k_SiteOwner, "PLAYER_TWO")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_WORSHIP_SITE", {Text(k_SiteTribe, "TIBETAN"), Text(k_SiteOwner, "PLAYER_TWO")}), 1u);

	// the miracle dispensers, by miracle
	EXPECT_EQ(Count(land, "CREATE_SPELL_DISPENSER"), 1u);
	EXPECT_EQ(Count(land, "CREATE_SPELL_DISPENSER", {Text(k_DispenserMiracle, "WOOD")}), 1u);

	// the trees and bushes, by kind
	EXPECT_EQ(Count(land, "CREATE_NEW_TREE"), 1397u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmB), 454u);
	EXPECT_EQ(Trees(land, TreeInfo::Palm), 279u);
	EXPECT_EQ(Trees(land, TreeInfo::BushB), 132u);
	EXPECT_EQ(Trees(land, TreeInfo::Conifer), 76u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmA), 71u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmC), 69u);
	EXPECT_EQ(Trees(land, TreeInfo::Beech), 54u);
	EXPECT_EQ(Trees(land, TreeInfo::Cedar), 52u);
	EXPECT_EQ(Trees(land, TreeInfo::Bush), 39u);
	EXPECT_EQ(Trees(land, TreeInfo::HedgeA), 39u);
	EXPECT_EQ(Trees(land, TreeInfo::Pine), 38u);
	EXPECT_EQ(Trees(land, TreeInfo::BushA), 33u);
	EXPECT_EQ(Trees(land, TreeInfo::Olive), 26u);
	EXPECT_EQ(Trees(land, TreeInfo::Hedge), 15u);
	EXPECT_EQ(Trees(land, TreeInfo::ConiferA), 8u);
	EXPECT_EQ(Trees(land, TreeInfo::Copse), 6u);
	EXPECT_EQ(Trees(land, TreeInfo::Cypress), 5u);
	EXPECT_EQ(Trees(land, TreeInfo::Birch), 1u);

	// the big forest models
	EXPECT_EQ(Count(land, "CREATE_NEW_BIG_FOREST"), 16u);

	// the features, by name
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Spikey Pilar Lime")}), 20u);
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Pilar2 Lime")}), 6u);
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Prison Pillar 1")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Prison Pillar 2")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Prison Pillar 3")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Tibetan Large Pillar Feature")}), 1u);

	// the patches of mist
	EXPECT_EQ(Count(land, "CREATE_MIST"), 70u);

	// the fields, by kind
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_FIELD"), 18u);
	EXPECT_EQ(Fields(land, FieldTypeInfo::Wheat), 14u);
	EXPECT_EQ(Fields(land, FieldTypeInfo::WheatWithFence), 4u);

	// the animals, by kind
	EXPECT_EQ(Animals(land, AnimalInfo::Dove), 27u);
	EXPECT_EQ(Animals(land, AnimalInfo::Sheep), 9u);
	EXPECT_EQ(Animals(land, AnimalInfo::Tortoise), 8u);
	EXPECT_EQ(Animals(land, AnimalInfo::Horse), 8u);
	EXPECT_EQ(Animals(land, AnimalInfo::Seagull), 6u);
	EXPECT_EQ(Animals(land, AnimalInfo::Cow), 6u);
	EXPECT_EQ(Animals(land, AnimalInfo::Wolf), 5u);

	// the flocks
	EXPECT_EQ(Count(land, "CREATE_FLOCK"), 15u);

	// the rocks, fences, toys, totems and stones, by kind
	size_t rocks = 0;
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Rock)), 83u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder1Sand)), 7u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder3Lime)), 3u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::RockLimestone)), 3u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder2Lime)), 3u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::SharprockLimestone)), 2u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder1Lime)), 2u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::FlatrockLimestone)), 2u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::LongrockLimestone)), 1u);
	EXPECT_EQ(rocks, 106u);
	size_t fences = 0;
	EXPECT_EQ(Tally(fences, Statics(land, MobileStaticInfo::CeltFenceShort)), 49u);
	EXPECT_EQ(Tally(fences, Statics(land, MobileStaticInfo::CeltFenceTall)), 38u);
	EXPECT_EQ(fences, 87u);
	EXPECT_EQ(Statics(land, MobileStaticInfo::WeepingStoneReward), 8u);

	// the objects that can be picked up, by kind
	EXPECT_EQ(Objects(land, MobileObjectInfo::MagicMushroom), 13u);
	EXPECT_EQ(Objects(land, MobileObjectInfo::Champi), 7u);
	EXPECT_EQ(Objects(land, MobileObjectInfo::Toadstool), 6u);

	// the lanterns, by kind
	EXPECT_EQ(Count(land, "CREATE_STREET_LANTERN"), 1u);
	EXPECT_EQ(Lanterns(land, MobileStaticInfo::StreetLantern), 1u);

	// the streams and their points
	EXPECT_EQ(Count(land, "CREATE_STREAM"), 2u);
	EXPECT_EQ(Count(land, "CREATE_STREAM_POINT"), 18u);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
/// With OPENBLACK_GAME_PATH set to the install: Land 4's script loads from its first line to its last and
/// makes what its progress page counts (docs/wiki/project/progress/scripts/land4_script.md)
TEST(LandScriptContents, realLand4Script)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto script = ReadRealLandScript(*game, "Land4");
	ASSERT_TRUE(script.has_value());
	const auto& land = *script;

	// every line is a command of the table with the arguments its format asks for, so none is skipped
	EXPECT_FALSE(land.lines.empty());
	EXPECT_EQ(LinesNotLoaded(land), std::vector<int> {});

	// the land's number, 4, which places the ark and the dinosaur skeleton: no line of the script makes them
	// (docs/wiki/world/sea-life-and-scenery.md#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur)
	EXPECT_EQ(Count(land, "SET_LAND_NUMBER"), 1u);
	EXPECT_EQ(Count(land, "SET_LAND_NUMBER", {Kind(0, 4)}), 1u);

	// the land's footpath file is where the game looks for it
	EXPECT_TRUE(std::filesystem::exists(FootpathFileOf(*game, "Land4")));

	// the landscape
	EXPECT_EQ(Count(land, "LOAD_LANDSCAPE", {Ending(0, "\\Land4.lnd")}), 1u);

	// the towns, by tribe and by owner
	EXPECT_EQ(Count(land, "CREATE_TOWN"), 8u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "NORSE")}), 3u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "AZTEC")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "JAPANESE")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "CELTIC")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "INDIAN")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownOwner, "NEUTRAL")}), 7u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownOwner, "PLAYER_ONE")}), 1u);

	// the towns left uninhabitable
	EXPECT_EQ(Count(land, "SET_TOWN_UNINHABITABLE"), 3u);

	// the miracles the towns offer at their village centres
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL"), 11u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "HEAL_PU1")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "FOOD")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "SHIELD")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "WOOD")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "WATER_PU1")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "FIRE")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "LIGHTNING_BOLT_PU2")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "CREATURE_SPELL_ANGRY")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "TELEPORT")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "NATURE")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "CREATURE_SPELL_WEAK")}), 1u);

	// the planned buildings, by the kind of building
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE"), 25u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_A")}), 7u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_C")}), 5u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_F")}), 3u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_D")}), 3u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_WORKSHOP")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_B")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_E")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_TOWN_CENTRE")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_GRAVEYARD")}), 1u);

	// the trees and bushes, by kind
	EXPECT_EQ(Count(land, "CREATE_NEW_TREE"), 799u);
	EXPECT_EQ(Trees(land, TreeInfo::Bush), 315u);
	EXPECT_EQ(Trees(land, TreeInfo::Conifer), 87u);
	EXPECT_EQ(Trees(land, TreeInfo::OakA), 75u);
	EXPECT_EQ(Trees(land, TreeInfo::ConiferA), 70u);
	EXPECT_EQ(Trees(land, TreeInfo::Oak), 60u);
	EXPECT_EQ(Trees(land, TreeInfo::Birch), 59u);
	EXPECT_EQ(Trees(land, TreeInfo::Olive), 29u);
	EXPECT_EQ(Trees(land, TreeInfo::Beech), 26u);
	EXPECT_EQ(Trees(land, TreeInfo::BushB), 18u);
	EXPECT_EQ(Trees(land, TreeInfo::Hedge), 15u);
	EXPECT_EQ(Trees(land, TreeInfo::Pine), 8u);
	EXPECT_EQ(Trees(land, TreeInfo::BushA), 7u);
	EXPECT_EQ(Trees(land, TreeInfo::Palm), 7u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmB), 7u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmA), 6u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmC), 6u);
	EXPECT_EQ(Trees(land, TreeInfo::Cedar), 4u);

	// the big forest models
	EXPECT_EQ(Count(land, "CREATE_NEW_BIG_FOREST"), 9u);

	// the features, by name
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Aztec Statue Feature")}), 8u);
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Spikey Pilar Volcanic")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Aztec Suntemple Feature")}), 1u);

	// the patches of mist
	EXPECT_EQ(Count(land, "CREATE_MIST"), 14u);

	// the fields, by kind
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_FIELD"), 16u);
	EXPECT_EQ(Fields(land, FieldTypeInfo::Wheat), 12u);
	EXPECT_EQ(Fields(land, FieldTypeInfo::Cereal), 3u);
	EXPECT_EQ(Fields(land, FieldTypeInfo::Corn), 1u);

	// the animals, by kind
	EXPECT_EQ(Animals(land, AnimalInfo::Crow), 29u);
	EXPECT_EQ(Animals(land, AnimalInfo::Pig), 26u);
	EXPECT_EQ(Animals(land, AnimalInfo::Sheep), 26u);
	EXPECT_EQ(Animals(land, AnimalInfo::Bat), 20u);
	EXPECT_EQ(Animals(land, AnimalInfo::Horse), 7u);
	EXPECT_EQ(Animals(land, AnimalInfo::Cow), 5u);
	EXPECT_EQ(Animals(land, AnimalInfo::Dove), 5u);

	// the flocks
	EXPECT_EQ(Count(land, "CREATE_FLOCK"), 15u);

	// the rocks, fences, toys, totems and stones, by kind
	size_t rocks = 0;
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder3Lime)), 17u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::LongrockVolcanic)), 12u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder2Sand)), 11u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::LongrockLimestone)), 10u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder2Lime)), 10u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder1Lime)), 8u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder1Sand)), 7u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::SharprockLimestone)), 5u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder1Volcanic)), 5u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::LongrockSandstone)), 4u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::FlatrockSandstone)), 4u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder3Volcanic)), 4u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder3Sand)), 3u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::RockLimestone)), 3u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::SquarerockLimestone)), 3u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::RockSandstone)), 3u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::SharprockSandstone)), 2u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::FlatrockLimestone)), 2u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::RockVolcanic)), 2u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::SharprockVolcanic)), 1u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder2Volcanic)), 1u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::SquarerockSandstone)), 1u);
	EXPECT_EQ(rocks, 118u);
	size_t fences = 0;
	EXPECT_EQ(Tally(fences, Statics(land, MobileStaticInfo::CeltFenceShort)), 62u);
	EXPECT_EQ(Tally(fences, Statics(land, MobileStaticInfo::CeltFenceTall)), 27u);
	EXPECT_EQ(fences, 89u);

	// the objects that can be picked up, by kind
	EXPECT_EQ(Objects(land, MobileObjectInfo::EgyptBarrel), 1u);

	// the animated statics, by name
	EXPECT_EQ(Count(land, "CREATE_ANIMATED_STATIC", {Text(k_AnimatedStaticName, "Norse Gate")}), 1u);

	// the lanterns, by kind
	EXPECT_EQ(Count(land, "CREATE_STREET_LANTERN"), 16u);
	EXPECT_EQ(Lanterns(land, MobileStaticInfo::StreetLantern), 8u);
	EXPECT_EQ(Lanterns(land, MobileStaticInfo::CountryLantern), 8u);

	// the streams and their points
	EXPECT_EQ(Count(land, "CREATE_STREAM"), 23u);
	EXPECT_EQ(Count(land, "CREATE_STREAM_POINT"), 388u);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
/// With OPENBLACK_GAME_PATH set to the install: Land 5's script loads from its first line to its last and
/// makes what its progress page counts (docs/wiki/project/progress/scripts/land5_script.md)
TEST(LandScriptContents, realLand5Script)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto script = ReadRealLandScript(*game, "Land5");
	ASSERT_TRUE(script.has_value());
	const auto& land = *script;

	// every line is a command of the table with the arguments its format asks for, so none is skipped
	EXPECT_FALSE(land.lines.empty());
	EXPECT_EQ(LinesNotLoaded(land), std::vector<int> {});

	// the land's footpath file is where the game looks for it
	EXPECT_TRUE(std::filesystem::exists(FootpathFileOf(*game, "Land5")));

	// the landscape
	EXPECT_EQ(Count(land, "LOAD_LANDSCAPE", {Ending(0, "\\Land5.lnd")}), 1u);

	// the towns, by tribe and by owner
	EXPECT_EQ(Count(land, "CREATE_TOWN"), 8u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "NORSE")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "TIBETAN")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "AZTEC")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "GREEK")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "CELTIC")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "JAPANESE")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownOwner, "PLAYER_TWO")}), 4u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownOwner, "PLAYER_ONE")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownOwner, "NEUTRAL")}), 2u);

	// the towns left uninhabitable
	EXPECT_EQ(Count(land, "SET_TOWN_UNINHABITABLE"), 1u);

	// the miracles the towns offer at their village centres
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL"), 51u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "LIGHTNING_BOLT")}), 5u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "HEAL")}), 4u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "HEAL_PU1")}), 4u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "LIGHTNING_BOLT_PU2")}), 4u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "FIRE")}), 3u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "TELEPORT")}), 3u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "STORM")}), 3u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "BEAM_EXPLOSION")}), 3u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "WATER")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "WATER_PU1")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "STORM_PU2")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "CREATURE_SPELL_STRONG")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "FIRE_PU2")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "NATURE")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "PHYSICAL_SHIELD")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "BEAM_EXPLOSION_PU2")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "FOOD")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "BEAM_EXPLOSION_PU1")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "SHIELD")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "WOOD")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "GROUND_FLOCK")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "CREATURE_SPELL_INVISIBLE")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_SPELL", {Text(k_TownSpellName, "CREATURE_SPELL_COMPASSION")}), 1u);

	// the planned buildings, by the kind of building
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE"), 68u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_A")}), 13u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_D")}), 11u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_B")}), 8u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_C")}), 8u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_E")}), 6u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_WONDER")}), 6u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_F")}), 6u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_GRAVEYARD")}), 4u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_TOWN_CENTRE")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_CRECHE")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_TOTEM")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_STORAGE_PIT")}), 1u);

	// the worship sites, by tribe and by owner
	EXPECT_EQ(Count(land, "CREATE_WORSHIP_SITE"), 4u);
	EXPECT_EQ(Count(land, "CREATE_WORSHIP_SITE", {Text(k_SiteTribe, "JAPANESE"), Text(k_SiteOwner, "PLAYER_TWO")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_WORSHIP_SITE", {Text(k_SiteTribe, "GREEK"), Text(k_SiteOwner, "PLAYER_TWO")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_WORSHIP_SITE", {Text(k_SiteTribe, "TIBETAN"), Text(k_SiteOwner, "PLAYER_TWO")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_WORSHIP_SITE", {Text(k_SiteTribe, "AZTEC"), Text(k_SiteOwner, "PLAYER_TWO")}), 1u);

	// the miracle dispensers, by miracle
	EXPECT_EQ(Count(land, "CREATE_SPELL_DISPENSER"), 1u);
	EXPECT_EQ(Count(land, "CREATE_SPELL_DISPENSER", {Text(k_DispenserMiracle, "FIRE_PU2")}), 1u);

	// the trees and bushes, by kind
	EXPECT_EQ(Count(land, "CREATE_NEW_TREE"), 804u);
	EXPECT_EQ(Trees(land, TreeInfo::Bush), 222u);
	EXPECT_EQ(Trees(land, TreeInfo::Oak), 137u);
	EXPECT_EQ(Trees(land, TreeInfo::OakA), 111u);
	EXPECT_EQ(Trees(land, TreeInfo::ConiferA), 39u);
	EXPECT_EQ(Trees(land, TreeInfo::Olive), 34u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmB), 34u);
	EXPECT_EQ(Trees(land, TreeInfo::Palm), 34u);
	EXPECT_EQ(Trees(land, TreeInfo::Conifer), 32u);
	EXPECT_EQ(Trees(land, TreeInfo::BushB), 25u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmA), 23u);
	EXPECT_EQ(Trees(land, TreeInfo::Pine), 22u);
	EXPECT_EQ(Trees(land, TreeInfo::Beech), 20u);
	EXPECT_EQ(Trees(land, TreeInfo::Birch), 19u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmC), 16u);
	EXPECT_EQ(Trees(land, TreeInfo::Cypress), 12u);
	EXPECT_EQ(Trees(land, TreeInfo::CypressA), 11u);
	EXPECT_EQ(Trees(land, TreeInfo::BushA), 7u);
	EXPECT_EQ(Trees(land, TreeInfo::Cedar), 6u);

	// the big forest models
	EXPECT_EQ(Count(land, "CREATE_NEW_BIG_FOREST"), 8u);

	// the features, by name
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Crater")}), 5u);

	// the patches of mist
	EXPECT_EQ(Count(land, "CREATE_MIST"), 10u);

	// the fields, by kind
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_FIELD"), 47u);
	EXPECT_EQ(Fields(land, FieldTypeInfo::Wheat), 39u);
	EXPECT_EQ(Fields(land, FieldTypeInfo::Corn), 6u);
	EXPECT_EQ(Fields(land, FieldTypeInfo::Cereal), 2u);

	// the animals, by kind
	EXPECT_EQ(Animals(land, AnimalInfo::Sheep), 38u);
	EXPECT_EQ(Animals(land, AnimalInfo::Seagull), 34u);
	EXPECT_EQ(Animals(land, AnimalInfo::Cow), 28u);
	EXPECT_EQ(Animals(land, AnimalInfo::Bat), 24u);
	EXPECT_EQ(Animals(land, AnimalInfo::Pig), 23u);
	EXPECT_EQ(Animals(land, AnimalInfo::Tortoise), 17u);
	EXPECT_EQ(Animals(land, AnimalInfo::Swallow), 11u);
	EXPECT_EQ(Animals(land, AnimalInfo::Horse), 11u);
	EXPECT_EQ(Animals(land, AnimalInfo::Wolf), 10u);
	EXPECT_EQ(Animals(land, AnimalInfo::Crow), 9u);
	EXPECT_EQ(Animals(land, AnimalInfo::Tiger), 4u);

	// the flocks
	EXPECT_EQ(Count(land, "CREATE_FLOCK"), 28u);

	// the rocks, fences, toys, totems and stones, by kind
	size_t rocks = 0;
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder1Lime)), 25u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder3Lime)), 20u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder2Lime)), 20u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::LongrockLimestone)), 16u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::SharprockLimestone)), 10u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::FlatrockLimestone)), 6u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder1Chalk)), 4u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::SquarerockLimestone)), 4u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::FlatrockSandstone)), 3u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder2Sand)), 3u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::RockLimestone)), 2u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::RockSandstone)), 2u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::LongrockSandstone)), 1u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder3Sand)), 1u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::LongrockChalk)), 1u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Rock)), 1u);
	EXPECT_EQ(rocks, 119u);
	size_t fences = 0;
	EXPECT_EQ(Tally(fences, Statics(land, MobileStaticInfo::CeltFenceShort)), 68u);
	EXPECT_EQ(Tally(fences, Statics(land, MobileStaticInfo::CeltFenceTall)), 43u);
	EXPECT_EQ(fences, 111u);

	// the objects that can be picked up, by kind
	EXPECT_EQ(Objects(land, MobileObjectInfo::MagicMushroom), 17u);
	EXPECT_EQ(Objects(land, MobileObjectInfo::MagicWood), 1u);
	EXPECT_EQ(Objects(land, MobileObjectInfo::EgyptPotA), 1u);

	// the animated statics, by name
	EXPECT_EQ(Count(land, "CREATE_ANIMATED_STATIC", {Text(k_AnimatedStaticName, "Piper Cave Entrance")}), 2u);

	// the lanterns, by kind
	EXPECT_EQ(Count(land, "CREATE_STREET_LANTERN"), 21u);
	EXPECT_EQ(Lanterns(land, MobileStaticInfo::CountryLantern), 14u);
	EXPECT_EQ(Lanterns(land, MobileStaticInfo::StreetLantern), 7u);

	// the streams and their points
	EXPECT_EQ(Count(land, "CREATE_STREAM"), 19u);
	EXPECT_EQ(Count(land, "CREATE_STREAM_POINT"), 422u);

	// the one-shot miracles lying on the land
	EXPECT_EQ(Count(land, "CREATE_ONE_SHOT_SPELL_PU"), 2u);
	EXPECT_EQ(Count(land, "CREATE_ONE_SHOT_SPELL_PU", {Text(k_OneShotMiracle, "WATER")}), 2u);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
/// With OPENBLACK_GAME_PATH set to the install: The tutorial land's script loads from its first line to its last and
/// makes what its progress page counts (docs/wiki/project/progress/scripts/landT_script.md)
TEST(LandScriptContents, realLandTScript)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto script = ReadRealLandScript(*game, "LandT");
	ASSERT_TRUE(script.has_value());
	const auto& land = *script;

	// every line is a command of the table with the arguments its format asks for, so none is skipped
	EXPECT_FALSE(land.lines.empty());
	EXPECT_EQ(LinesNotLoaded(land), std::vector<int> {});

	// the tutorial land has no footpath file
	EXPECT_FALSE(std::filesystem::exists(FootpathFileOf(*game, "LandT")));

	// the landscape
	EXPECT_EQ(Count(land, "LOAD_LANDSCAPE", {Ending(0, "\\LandT.lnd")}), 1u);

	// the towns, by tribe and by owner
	EXPECT_EQ(Count(land, "CREATE_TOWN"), 3u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "JAPANESE")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "CELTIC")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownTribe, "INDIAN")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownOwner, "PLAYER_ONE")}), 2u);
	EXPECT_EQ(Count(land, "CREATE_TOWN", {Text(k_TownOwner, "NEUTRAL")}), 1u);

	// the planned buildings, by the kind of building
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE"), 5u);
	EXPECT_EQ(Count(land, "CREATE_PLANNED_ABODE", {Ending(k_PlannedAbodeName, "_ABODE_F")}), 5u);

	// the trees and bushes, by kind
	EXPECT_EQ(Count(land, "CREATE_NEW_TREE"), 341u);
	EXPECT_EQ(Trees(land, TreeInfo::Oak), 101u);
	EXPECT_EQ(Trees(land, TreeInfo::OakA), 69u);
	EXPECT_EQ(Trees(land, TreeInfo::Bush), 51u);
	EXPECT_EQ(Trees(land, TreeInfo::BushA), 20u);
	EXPECT_EQ(Trees(land, TreeInfo::BushB), 15u);
	EXPECT_EQ(Trees(land, TreeInfo::Beech), 14u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmC), 13u);
	EXPECT_EQ(Trees(land, TreeInfo::ConiferA), 12u);
	EXPECT_EQ(Trees(land, TreeInfo::Palm), 12u);
	EXPECT_EQ(Trees(land, TreeInfo::Conifer), 7u);
	EXPECT_EQ(Trees(land, TreeInfo::Birch), 7u);
	EXPECT_EQ(Trees(land, TreeInfo::Pine), 6u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmB), 5u);
	EXPECT_EQ(Trees(land, TreeInfo::PalmA), 5u);
	EXPECT_EQ(Trees(land, TreeInfo::Olive), 4u);

	// the big forest models
	EXPECT_EQ(Count(land, "CREATE_NEW_BIG_FOREST"), 4u);

	// the features, by name
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Spikey Pilar Sand")}), 1u);
	EXPECT_EQ(Count(land, "CREATE_NEW_FEATURE", {Text(k_FeatureName, "Pier")}), 1u);

	// the fields, by kind
	EXPECT_EQ(Count(land, "CREATE_NEW_TOWN_FIELD"), 2u);
	EXPECT_EQ(Fields(land, FieldTypeInfo::Wheat), 2u);

	// the animals, by kind
	EXPECT_EQ(Animals(land, AnimalInfo::Swallow), 148u);
	EXPECT_EQ(Animals(land, AnimalInfo::Seagull), 129u);
	EXPECT_EQ(Animals(land, AnimalInfo::Sheep), 92u);
	EXPECT_EQ(Animals(land, AnimalInfo::Crow), 71u);
	EXPECT_EQ(Animals(land, AnimalInfo::Tortoise), 26u);
	EXPECT_EQ(Animals(land, AnimalInfo::Horse), 22u);

	// the flocks
	EXPECT_EQ(Count(land, "CREATE_FLOCK"), 19u);

	// the rocks, fences, toys, totems and stones, by kind
	size_t rocks = 0;
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder3Sand)), 12u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder2Sand)), 9u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::RockSandstone)), 8u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder3Lime)), 7u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::FlatrockSandstone)), 5u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder1Lime)), 5u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::FlatrockLimestone)), 5u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::LongrockSandstone)), 4u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::RockLimestone)), 4u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder2Lime)), 4u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::Boulder1Sand)), 3u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::SharprockLimestone)), 3u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::SharprockSandstone)), 2u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::SquarerockSandstone)), 2u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::LongrockLimestone)), 2u);
	EXPECT_EQ(Tally(rocks, Statics(land, MobileStaticInfo::SquarerockLimestone)), 1u);
	EXPECT_EQ(rocks, 76u);
	size_t toys = 0;
	EXPECT_EQ(Tally(toys, Statics(land, MobileStaticInfo::ToySkittle)), 16u);
	EXPECT_EQ(Tally(toys, Statics(land, MobileStaticInfo::ToyBowlingBall)), 5u);
	EXPECT_EQ(Tally(toys, Statics(land, MobileStaticInfo::ToyBall)), 4u);
	EXPECT_EQ(Tally(toys, Statics(land, MobileStaticInfo::ToyDie)), 2u);
	EXPECT_EQ(toys, 27u);
	EXPECT_EQ(Statics(land, MobileStaticInfo::WeepingStone), 2u);

	// the lanterns, by kind
	EXPECT_EQ(Count(land, "CREATE_STREET_LANTERN"), 6u);
	EXPECT_EQ(Lanterns(land, MobileStaticInfo::StreetLantern), 4u);
	EXPECT_EQ(Lanterns(land, MobileStaticInfo::CountryLantern), 2u);

	// the streams and their points
	EXPECT_EQ(Count(land, "CREATE_STREAM"), 1u);
	EXPECT_EQ(Count(land, "CREATE_STREAM_POINT"), 37u);
}

namespace
{
// The owner of the lines a computer god owns, by its place in the line from 0
constexpr size_t k_ToggleOwner = 0;  // TOGGLE_COMPUTER_PLAYER(player, on)
constexpr size_t k_ToggleOn = 1;     // TOGGLE_COMPUTER_PLAYER's on
constexpr size_t k_CitadelOwner = 2; // CREATE_CITADEL(place, ..., owner, ...)

/// A player's name in capitals: the game matches names without case
std::string Upper(std::string_view name)
{
	std::string upper(name);
	std::ranges::transform(upper, upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	return upper;
}

/// A computer god of a script: the player its TOGGLE_COMPUTER_PLAYER switches on, the line of the first such toggle,
/// and the lines of the towns, the temple and the worship sites it owns
struct ComputerGod
{
	std::string player;
	int toggleLine;
	std::vector<int> ownedLines;
};

/// Every player a script's lines switch on as a computer god, in the order of their first toggle, with what it owns
std::vector<ComputerGod> ComputerGods(const ReadScript& script)
{
	std::vector<ComputerGod> gods;
	for (const auto& line : script.lines)
	{
		if (line.command != "TOGGLE_COMPUTER_PLAYER" || !MatchesItsFormat(line))
		{
			continue;
		}
		const auto& on = line.arguments.at(k_ToggleOn);
		if (on.GetType() != Token::Type::Integer || *on.IntegerValue() == 0)
		{
			continue;
		}
		const auto player = Upper(line.arguments.at(k_ToggleOwner).StringValue());
		if (std::ranges::none_of(gods, [&player](const ComputerGod& god) { return god.player == player; }))
		{
			gods.push_back({.player = player, .toggleLine = line.line, .ownedLines = {}});
		}
	}
	for (auto& god : gods)
	{
		for (const auto& line : script.lines)
		{
			const auto owner = line.command == "CREATE_TOWN"           ? k_TownOwner
			                   : line.command == "CREATE_CITADEL"      ? k_CitadelOwner
			                   : line.command == "CREATE_WORSHIP_SITE" ? k_SiteOwner
			                                                           : line.arguments.size();
			if (owner < line.arguments.size() && MatchesItsFormat(line) &&
			    Upper(line.arguments.at(owner).StringValue()) == god.player)
			{
				god.ownedLines.push_back(line.line);
			}
		}
	}
	return gods;
}

/// The lines a computer god owns that come before its first toggle
std::vector<int> OwnedBeforeTheToggle(const ComputerGod& god)
{
	std::vector<int> early;
	std::ranges::copy_if(god.ownedLines, std::back_inserter(early), [&god](int line) { return line < god.toggleLine; });
	return early;
}

/// The land scripts of the game's Scripts folder that the progress pages count: the story's, the demo and test lands
/// and the playgrounds of the game's install
constexpr std::array<std::string_view, 5> k_StoryLands {"Land1", "Land2", "Land3", "Land4", "Land5"};
constexpr std::array<std::string_view, 3> k_DemoAndTestLands {"demo2", "comp", "dance"};
constexpr std::array<std::string_view, 3> k_Playgrounds {"Playgrounds/TwoGods", "Playgrounds/ThreeGods",
                                                         "Playgrounds/FourGods"};

/// How many lines of these scripts the reader runs are this command; a script that can't be read fails the test
size_t CountIn(const std::filesystem::path& game, std::span<const std::string_view> lands, std::string_view command)
{
	size_t total = 0;
	for (const auto land : lands)
	{
		const auto script = ReadRealLandScript(game, land);
		EXPECT_TRUE(script.has_value()) << land;
		if (script.has_value())
		{
			total += Count(*script, command);
		}
	}
	return total;
}
} // namespace

// A computer god's lines, as the reader takes them: a player switched on, the line of its first toggle and the towns,
// temple and worship sites it owns, its name matched without case as the game matches a player's name
// (docs/wiki/scripts/map-script-reader.md#towns-and-players). A player switched off, or never switched on, is no
// computer god.
TEST(LandScriptContents, AComputerGodsLinesAreTheTownsTempleAndSitesItOwns)
{
	const std::string source = "CREATE_TOWN(0, \"10.00,20.00\", \"PLAYER_ONE\", 7, \"NORSE\")\r\n"
	                           "CREATE_TOWN(1, \"30.00,20.00\", \"PLAYER_TWO\", 0, \"CELTIC\")\r\n"
	                           "TOGGLE_COMPUTER_PLAYER(\"PLAYER_TWO\", 1)\r\n"
	                           "CREATE_TOWN(2, \"40.00,20.00\", \"Player_Two\", 0, \"CELTIC\")\r\n"
	                           "CREATE_CITADEL(\"50.00,20.00\", 0, \"PLAYER_TWO\", 0, 1000)\r\n"
	                           "CREATE_WORSHIP_SITE(\"5.00,5.00\", 3, \"PLAYER_TWO\", \"CELTIC\", 0, 1000)\r\n"
	                           "TOGGLE_COMPUTER_PLAYER(\"PLAYER_THREE\", 0)\r\n"
	                           "CREATE_TOWN(3, \"60.00,20.00\", \"PLAYER_THREE\", 0, \"GREEK\")\r\n"
	                           "TOGGLE_COMPUTER_PLAYER(\"PLAYER_TWO\", 1)\r\n";
	const auto gods = ComputerGods(ReadLandScript(source));
	ASSERT_EQ(gods.size(), 1u);
	EXPECT_EQ(gods.front().player, "PLAYER_TWO");
	EXPECT_EQ(gods.front().toggleLine, 3);
	EXPECT_EQ(gods.front().ownedLines, (std::vector<int> {2, 4, 5, 6}));
	EXPECT_EQ(OwnedBeforeTheToggle(gods.front()), std::vector<int> {2});
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
/// With OPENBLACK_GAME_PATH set to the install: in every land script of the game's Scripts folder that switches a
/// computer god on, each god is switched on before any of its towns, its temple or its worship sites is made, so the
/// reader, which runs the lines from the first to the last as the original
/// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read), makes them for a computer player. The lands that make
/// computer gods are Land 2 (two), Land 3, Land 5, the test land and the three playgrounds (one to three gods)
TEST(LandScriptContents, realEveryComputerGodIsSwitchedOnBeforeItsTownsAreMade)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::vector<std::string_view> lands(k_StoryLands.begin(), k_StoryLands.end());
	lands.emplace_back("LandT");
	lands.insert(lands.end(), k_DemoAndTestLands.begin(), k_DemoAndTestLands.end());
	lands.insert(lands.end(), k_Playgrounds.begin(), k_Playgrounds.end());
	std::vector<std::pair<std::string_view, size_t>> godsPerLand;
	for (const auto land : lands)
	{
		const auto script = ReadRealLandScript(*game, land);
		ASSERT_TRUE(script.has_value()) << land;
		const auto gods = ComputerGods(*script);
		for (const auto& god : gods)
		{
			// each god owns towns of its own, and none of them is made before it is switched on
			EXPECT_FALSE(god.ownedLines.empty()) << land << " " << god.player;
			EXPECT_EQ(OwnedBeforeTheToggle(god), std::vector<int> {}) << land << " " << god.player;
		}
		if (!gods.empty())
		{
			godsPerLand.emplace_back(land, gods.size());
		}
	}
	EXPECT_EQ(godsPerLand, (std::vector<std::pair<std::string_view, size_t>> {{"Land2", 2},
	                                                                          {"Land3", 1},
	                                                                          {"Land5", 1},
	                                                                          {"comp", 1},
	                                                                          {"Playgrounds/TwoGods", 1},
	                                                                          {"Playgrounds/ThreeGods", 2},
	                                                                          {"Playgrounds/FourGods", 3}}));
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
/// With OPENBLACK_GAME_PATH set to the install: how many lines of the game's land scripts load a landscape, make mist,
/// start a stream, add a stream's point and set a climate's rain, by the story, the tutorial, the demo and test lands
/// and the playgrounds of the game's install, as the land script commands' page counts them
/// (docs/wiki/project/progress/scripts/land_script_commands.md). The page's playground counts also take in skirmish
/// lands that are not in the game's Scripts folder; these are the three that are
TEST(LandScriptContents, realTheShippedScriptsUseTheLandscapeMistStreamAndRainCommandsAsCounted)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const std::array<std::string_view, 1> tutorial {"LandT"};

	// LOAD_LANDSCAPE: one per land
	EXPECT_EQ(CountIn(*game, k_StoryLands, "LOAD_LANDSCAPE"), 5u);
	EXPECT_EQ(CountIn(*game, tutorial, "LOAD_LANDSCAPE"), 1u);
	EXPECT_EQ(CountIn(*game, k_DemoAndTestLands, "LOAD_LANDSCAPE"), 3u);
	EXPECT_EQ(CountIn(*game, k_Playgrounds, "LOAD_LANDSCAPE"), 3u);

	// CREATE_MIST
	EXPECT_EQ(CountIn(*game, k_StoryLands, "CREATE_MIST"), 111u);
	EXPECT_EQ(CountIn(*game, tutorial, "CREATE_MIST"), 0u);
	EXPECT_EQ(CountIn(*game, k_DemoAndTestLands, "CREATE_MIST"), 70u);
	EXPECT_EQ(CountIn(*game, k_Playgrounds, "CREATE_MIST"), 0u);

	// CREATE_STREAM_POINT
	EXPECT_EQ(CountIn(*game, k_StoryLands, "CREATE_STREAM_POINT"), 1015u);
	EXPECT_EQ(CountIn(*game, tutorial, "CREATE_STREAM_POINT"), 37u);
	EXPECT_EQ(CountIn(*game, k_DemoAndTestLands, "CREATE_STREAM_POINT"), 0u);
	EXPECT_EQ(CountIn(*game, k_Playgrounds, "CREATE_STREAM_POINT"), 0u);

	// CREATE_STREAM
	EXPECT_EQ(CountIn(*game, k_StoryLands, "CREATE_STREAM"), 55u);
	EXPECT_EQ(CountIn(*game, tutorial, "CREATE_STREAM"), 1u);
	EXPECT_EQ(CountIn(*game, k_DemoAndTestLands, "CREATE_STREAM"), 0u);
	EXPECT_EQ(CountIn(*game, k_Playgrounds, "CREATE_STREAM"), 0u);

	// CREATE_WEATHER_CLIMATE_RAIN
	EXPECT_EQ(CountIn(*game, k_StoryLands, "CREATE_WEATHER_CLIMATE_RAIN"), 21u);
	EXPECT_EQ(CountIn(*game, tutorial, "CREATE_WEATHER_CLIMATE_RAIN"), 0u);
	EXPECT_EQ(CountIn(*game, k_DemoAndTestLands, "CREATE_WEATHER_CLIMATE_RAIN"), 2u);
	EXPECT_EQ(CountIn(*game, k_Playgrounds, "CREATE_WEATHER_CLIMATE_RAIN"), 4u);
}
