/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the skirmish lands' scripts and the demo's land script make, counted from their lines: every line read with the
// land-script lexer the way the reader reads it, each command checked against the command table, then the lines of a
// command counted by one of their arguments (a kind, a tribe, an owner, a miracle's name). The integration tests read
// the original game's scripts (OPENBLACK_GAME_PATH) and skip without them; the first test runs the same counting on a
// small script in memory (docs/wiki/project/progress/scripts/playground_scripts.md,
// docs/wiki/project/progress/scripts/map_and_demo_scripts.md)

#include <cstddef>
#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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
/// One command line of a script: its name, its arguments' tokens and its line, from 1
struct CommandLine
{
	std::string name;
	std::vector<Token> arguments;
	int line;
};

/// A whole script as the reader sees it: the command lines it reads, and the lines it would skip (text that is not a
/// command, a missing bracket, a character the lexer cannot read)
struct ReadScript
{
	std::vector<CommandLine> commands;
	std::vector<int> unreadLines;
};

/// The next token, a character the lexer cannot read given as an invalid token
Token NextToken(Lexer& lexer)
{
	try
	{
		return lexer.GetToken();
	}
	catch (const LexerException&)
	{
		return Token::MakeInvalidToken();
	}
}

/// Whether a token can be a command's argument: anything but an operator, the end of the line or of the input, or
/// what the lexer could not read
[[nodiscard]] bool IsArgument(const Token& token)
{
	switch (token.GetType())
	{
	case Token::Type::Identifier:
	case Token::Type::String:
	case Token::Type::Integer:
	case Token::Type::Float:
		return true;
	default:
		return false;
	}
}

/// Every line of a script read as the land-script reader reads it: a name, "(", arguments between commas, ")". Comment
/// lines are left out by the lexer itself
ReadScript ReadCommands(const std::string& source)
{
	ReadScript script;
	Lexer lexer(source);
	Token token = NextToken(lexer);
	while (!token.IsEOF())
	{
		if (token.IsEOL())
		{
			token = NextToken(lexer);
			continue;
		}
		const int line = lexer.GetLine();
		bool read = false;
		CommandLine command {.name = token.IsIdentifier() ? token.Identifier() : std::string {}, .arguments = {}, .line = line};
		if (token.IsIdentifier())
		{
			token = NextToken(lexer);
			if (token.IsOP(Operator::LeftParentheses))
			{
				token = NextToken(lexer);
				if (token.IsOP(Operator::RightParentheses))
				{
					read = true;
				}
				else
				{
					while (true)
					{
						command.arguments.push_back(token);
						token = NextToken(lexer);
						if (!token.IsOP(Operator::Comma))
						{
							break;
						}
						token = NextToken(lexer);
					}
					read = token.IsOP(Operator::RightParentheses) && std::ranges::all_of(command.arguments, IsArgument);
				}
				if (read)
				{
					token = NextToken(lexer);
					// nothing else may follow the bracket on its line
					read = token.IsEOL() || token.IsEOF();
				}
			}
		}
		if (read)
		{
			script.commands.push_back(std::move(command));
		}
		else
		{
			script.unreadLines.push_back(line);
		}
		while (!token.IsEOL() && !token.IsEOF())
		{
			token = NextToken(lexer);
		}
	}
	return script;
}

/// The number of arguments a command table entry takes: one per type letter, the rest being padding
[[nodiscard]] size_t ArgumentCount(const lhscriptx::CommandFormat& format)
{
	return static_cast<size_t>(std::ranges::count_if(
	    format.types, [](char letter) { return letter == 'A' || letter == 'L' || letter == 'N' || letter == 'F'; }));
}

/// The lines whose command is not in the command table, or has another number of arguments than its entry
std::vector<int> LinesOutOfTheTable(const ReadScript& script)
{
	std::vector<int> lines;
	for (const auto& command : script.commands)
	{
		const auto* format = lhscriptx::FindCommandFormat(command.name);
		if (format == nullptr || ArgumentCount(*format) != command.arguments.size())
		{
			lines.push_back(command.line);
		}
	}
	return lines;
}

/// An argument as text: a name or a string as written, a whole number in decimal
[[nodiscard]] std::string ArgumentText(const Token& token)
{
	switch (token.GetType())
	{
	case Token::Type::Identifier:
	case Token::Type::String:
		return token.StringValue();
	case Token::Type::Integer:
		return std::to_string(*token.IntegerValue());
	case Token::Type::Float:
		return std::to_string(*token.FloatValue());
	default:
		return {};
	}
}

/// How many lines of each value of one argument
using Tally = std::map<std::string, int>;

/// The number of lines of a command
[[nodiscard]] int CountOf(const ReadScript& script, std::string_view name)
{
	return static_cast<int>(
	    std::ranges::count_if(script.commands, [name](const CommandLine& command) { return command.name == name; }));
}

/// The lines of a command counted by the value of one of their arguments, from 0
Tally CountBy(const ReadScript& script, std::string_view name, size_t argument)
{
	Tally tally;
	for (const auto& command : script.commands)
	{
		if (command.name == name && argument < command.arguments.size())
		{
			++tally[ArgumentText(command.arguments[argument])];
		}
	}
	return tally;
}

/// The lines of a command counted by two of their arguments, as "first/second"
Tally CountByPair(const ReadScript& script, std::string_view name, size_t first, size_t second)
{
	Tally tally;
	for (const auto& command : script.commands)
	{
		if (command.name == name && first < command.arguments.size() && second < command.arguments.size())
		{
			++tally[ArgumentText(command.arguments[first]) + "/" + ArgumentText(command.arguments[second])];
		}
	}
	return tally;
}

/// Building names ("NORSE_ABODE_B") counted by their kind whatever the tribe ("B"); a name without a tribe's part stays
/// as it is
Tally ByAbodeKind(const Tally& names)
{
	constexpr std::string_view k_Abode = "_ABODE_";
	Tally kinds;
	for (const auto& [name, count] : names)
	{
		const auto at = name.find(k_Abode);
		kinds[at == std::string::npos ? name : name.substr(at + k_Abode.size())] += count;
	}
	return kinds;
}

/// A kind as a script writes it, its number
template <typename Kind>
std::string KindOf(Kind kind)
{
	return std::to_string(static_cast<int>(kind));
}

/// The number of lines of a text, each ended by its line break
[[nodiscard]] int LineCount(const std::string& source)
{
	return static_cast<int>(std::ranges::count(source, '\n'));
}

/// The original game's install, from OPENBLACK_GAME_PATH; absent when it is not set
std::optional<std::filesystem::path> RealGamePath()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		return std::nullopt;
	}
	return std::filesystem::path(game);
}

/// One of the game's scripts, from its Scripts folder; absent without the game or the file
std::optional<std::string> RealScript(std::string_view file)
{
	const auto game = RealGamePath();
	if (!game.has_value())
	{
		return std::nullopt;
	}
	std::ifstream stream(*game / "Scripts" / file, std::ios::binary);
	if (!stream.is_open())
	{
		return std::nullopt;
	}
	return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

/// Whether the game has a footpath file of this name among its landscapes
[[nodiscard]] bool HasFootpathFile(std::string_view file)
{
	const auto game = RealGamePath();
	return game.has_value() && std::filesystem::exists(*game / "Data" / "Landscape" / file);
}
} // namespace

// The counting on a small script in memory: comment lines are left out, a line that is not a command, a line missing
// its bracket and a line with an unreadable character are the unread lines, a command not in the table and a command
// with the wrong number of arguments are out of the table, and the rest is counted by its arguments
TEST(PlaygroundScriptContents, TheReaderCountsAnInMemoryScriptsCommandsAndKinds)
{
	const std::string source = "VERSION(2.300000)\r\n"
	                           "REM a comment\r\n"
	                           "// another comment\r\n"
	                           "**** Player[0] ****\r\n"
	                           "LOAD_LANDSCAPE(\".\\DATA\\LANDSCAPE\\Land3.lnd\")\r\n"
	                           "CREATE_TOWN(0, \"2185.72,2315.78\", \"PLAYER_ONE\", 0, \"CELTIC\")\r\n"
	                           "CREATE_TOWN(1, \"2230.91,2612.41\", \"NEUTRAL\", 4, \"NORSE\")\r\n"
	                           "CREATE_TOWN(2, \"2230.91,2612.41\", \"NEUTRAL\", 4, \"NORSE\")\r\n"
	                           "CREATE_NEW_TREE(6, \"2971.07,2636.57\", 4, 1, 2.443285, 1.020000, 1.020000)\r\n"
	                           "CREATE_NEW_TREE(6, \"2972.14,2632.19\", 4, 1, 4.699709, 0.971000, 0.971000)\r\n"
	                           "CREATE_NEW_TREE(6, \"2972.14,2632.19\", 10, 1, 4.699709, 0.971000, 0.971000)\r\n"
	                           "CREATE_PLANNED_ABODE(0, \"2587.96,2287.98\", \"NORSE_ABODE_GRAVEYARD\", -6559, 759, 0, 0)\r\n"
	                           "CREATE_PLANNED_ABODE(0, \"2554.56,2247.75\", \"NORSE_ABODE_B\", -1605, 939, 0, 0)\r\n"
	                           "CREATE_PLANNED_ABODE(1, \"2554.56,2247.75\", \"CELTIC_ABODE_B\", -1605, 939, 0, 0)\r\n"
	                           "CREATE_WORSHIP_SITE(\"1996.26,2399.37\", 4, \"PLAYER_ONE\", \"INDIAN\", 2552, 1000)\r\n"
	                           "EDIT_LEVEL()\r\n"
	                           "not a command\r\n"
	                           "CREATE_FLOCK 0, \"1,1\"\r\n"
	                           "CREATE_MIST(#)\r\n"
	                           "CREATE_SPECIAL_TOWNVILLAGER(0, \"1,1\", 4)\r\n"
	                           "CREATE_TOWN(3, \"1,1\", \"NEUTRAL\", 0)\r\n";

	const auto script = ReadCommands(source);
	EXPECT_EQ(LineCount(source), 21);
	EXPECT_EQ(script.unreadLines, (std::vector<int> {17, 18, 19}));
	EXPECT_EQ(LinesOutOfTheTable(script), (std::vector<int> {20, 21}));

	EXPECT_EQ(CountBy(script, "LOAD_LANDSCAPE", 0), (Tally {{".\\DATA\\LANDSCAPE\\Land3.lnd", 1}}));
	EXPECT_EQ(CountOf(script, "CREATE_TOWN"), 4);
	EXPECT_EQ(CountBy(script, "CREATE_TOWN", 4), (Tally {{"CELTIC", 1}, {"NORSE", 2}}));
	EXPECT_EQ(CountBy(script, "CREATE_TOWN", 2), (Tally {{"NEUTRAL", 3}, {"PLAYER_ONE", 1}}));
	EXPECT_EQ(CountBy(script, "CREATE_NEW_TREE", 2), (Tally {{KindOf(TreeInfo::ConiferA), 2}, {KindOf(TreeInfo::PalmB), 1}}));
	EXPECT_EQ(ByAbodeKind(CountBy(script, "CREATE_PLANNED_ABODE", 2)), (Tally {{"B", 2}, {"GRAVEYARD", 1}}));
	EXPECT_EQ(CountByPair(script, "CREATE_WORSHIP_SITE", 3, 2), (Tally {{"INDIAN/PLAYER_ONE", 1}}));
	EXPECT_EQ(CountOf(script, "EDIT_LEVEL"), 1);
	EXPECT_EQ(CountOf(script, "CREATE_MIST"), 0);
}

// Two Gods: the script is read whole, every command in the table with its arguments, and makes what its rows count
// (docs/wiki/project/progress/scripts/playground_scripts.md)
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(PlaygroundScriptContents, TwoGodsLoadsWholeAndMakesWhatItsRowsCount)
{
	const auto source = RealScript("Playgrounds/TwoGods.txt");
	if (!source.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set or Playgrounds/TwoGods.txt missing";
	}
	const auto script = ReadCommands(*source);

	// loading: 1,000 lines, all read and all known, on its landscape, with one temple per player and its footpaths
	EXPECT_EQ(LineCount(*source), 1000);
	EXPECT_TRUE(script.unreadLines.empty());
	EXPECT_TRUE(LinesOutOfTheTable(script).empty());
	EXPECT_EQ(CountBy(script, "LOAD_LANDSCAPE", 0), (Tally {{".\\DATA\\LANDSCAPE\\MULTI_PLAYER\\MPM_2P_1.lnd", 1}}));
	EXPECT_EQ(CountOf(script, "CREATE_CITADEL"), 2);
	EXPECT_TRUE(HasFootpathFile("twogods.fot"));

	// towns
	EXPECT_EQ(CountBy(script, "CREATE_TOWN", 4),
	          (Tally {{"NORSE", 3}, {"CELTIC", 2}, {"INDIAN", 2}, {"AFRICAN", 1}, {"GREEK", 1}, {"EGYPTIAN", 1}}));
	EXPECT_EQ(CountBy(script, "CREATE_TOWN", 2), (Tally {{"NEUTRAL", 6}, {"PLAYER_ONE", 2}, {"PLAYER_TWO", 2}}));
	EXPECT_EQ(CountBy(script, "CREATE_NEW_TOWN_SPELL", 1), (Tally {{"STORM", 6},
	                                                               {"FIRE", 4},
	                                                               {"STORM_PU1", 4},
	                                                               {"FLYING_FLOCK", 3},
	                                                               {"GROUND_FLOCK", 3},
	                                                               {"HEAL", 2},
	                                                               {"TELEPORT", 2},
	                                                               {"WATER", 2},
	                                                               {"WATER_PU1", 2},
	                                                               {"NATURE", 2},
	                                                               {"PHYSICAL_SHIELD", 2},
	                                                               {"STORM_PU2", 2},
	                                                               {"LIGHTNING_BOLT", 2},
	                                                               {"WOOD", 2},
	                                                               {"SHIELD", 1}}));

	// buildings
	EXPECT_EQ(CountByPair(script, "CREATE_WORSHIP_SITE", 3, 2),
	          (Tally {{"INDIAN/PLAYER_ONE", 1}, {"CELTIC/PLAYER_ONE", 1}, {"INDIAN/PLAYER_TWO", 1}, {"CELTIC/PLAYER_TWO", 1}}));
	EXPECT_EQ(CountBy(script, "CREATE_SPELL_DISPENSER", 3), (Tally {{"BEAM_EXPLOSION", 1}}));

	// nature
	EXPECT_EQ(CountBy(script, "CREATE_NEW_TREE", 2), (Tally {{KindOf(TreeInfo::ConiferA), 141},
	                                                         {KindOf(TreeInfo::PalmB), 57},
	                                                         {KindOf(TreeInfo::Bush), 43},
	                                                         {KindOf(TreeInfo::Palm), 25},
	                                                         {KindOf(TreeInfo::Birch), 22},
	                                                         {KindOf(TreeInfo::CypressA), 9},
	                                                         {KindOf(TreeInfo::PalmA), 7},
	                                                         {KindOf(TreeInfo::CopseA), 4}}));
	EXPECT_EQ(CountOf(script, "CREATE_NEW_BIG_FOREST"), 3);

	// food
	EXPECT_EQ(
	    CountBy(script, "CREATE_NEW_TOWN_FIELD", 2),
	    (Tally {{KindOf(FieldTypeInfo::Wheat), 13}, {KindOf(FieldTypeInfo::Cereal), 7}, {KindOf(FieldTypeInfo::Corn), 4}}));

	// animals
	EXPECT_EQ(CountBy(script, "CREATE_NEW_ANIMAL", 1), (Tally {{KindOf(AnimalInfo::Sheep), 51},
	                                                           {KindOf(AnimalInfo::Cow), 20},
	                                                           {KindOf(AnimalInfo::Pig), 16},
	                                                           {KindOf(AnimalInfo::Horse), 16},
	                                                           {KindOf(AnimalInfo::Seagull), 16},
	                                                           {KindOf(AnimalInfo::Pigeon), 16},
	                                                           {KindOf(AnimalInfo::Wolf), 8},
	                                                           {KindOf(AnimalInfo::Crow), 8}}));
	EXPECT_EQ(CountOf(script, "CREATE_FLOCK"), 22);

	// objects
	EXPECT_EQ(CountBy(script, "CREATE_MOBILE_STATIC", 1), (Tally {{KindOf(MobileStaticInfo::Rock), 39}}));
	EXPECT_EQ(CountBy(script, "CREATE_MOBILEOBJECT", 1), (Tally {{KindOf(MobileObjectInfo::Champi), 12}}));
	EXPECT_EQ(CountBy(script, "CREATE_STREET_LANTERN", 1), (Tally {{KindOf(MobileStaticInfo::StreetLantern), 31}}));

	// miracles
	EXPECT_EQ(CountBy(script, "CREATE_ONE_SHOT_SPELL_PU", 1),
	          (Tally {{"WATER", 4}, {"FLYING_FLOCK", 4}, {"BEAM_EXPLOSION", 1}}));
}

// Three Gods: the script is read whole, every command in the table with its arguments, and makes what its rows count
// (docs/wiki/project/progress/scripts/playground_scripts.md)
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(PlaygroundScriptContents, ThreeGodsLoadsWholeAndMakesWhatItsRowsCount)
{
	const auto source = RealScript("Playgrounds/ThreeGods.txt");
	if (!source.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set or Playgrounds/ThreeGods.txt missing";
	}
	const auto script = ReadCommands(*source);

	// loading: 2,063 lines, all read and all known, on its landscape, with one temple per player and its footpaths
	EXPECT_EQ(LineCount(*source), 2063);
	EXPECT_TRUE(script.unreadLines.empty());
	EXPECT_TRUE(LinesOutOfTheTable(script).empty());
	EXPECT_EQ(CountBy(script, "LOAD_LANDSCAPE", 0), (Tally {{".\\DATA\\LANDSCAPE\\MULTI_PLAYER\\MPM_3P_1.lnd", 1}}));
	EXPECT_EQ(CountOf(script, "CREATE_CITADEL"), 3);
	EXPECT_TRUE(HasFootpathFile("threegods.fot"));

	// towns
	EXPECT_EQ(CountBy(script, "CREATE_TOWN", 4), (Tally {{"NORSE", 4}, {"JAPANESE", 4}, {"CELTIC", 4}, {"TIBETAN", 1}}));
	EXPECT_EQ(CountBy(script, "CREATE_TOWN", 2),
	          (Tally {{"NEUTRAL", 10}, {"PLAYER_ONE", 1}, {"PLAYER_TWO", 1}, {"PLAYER_THREE", 1}}));
	EXPECT_EQ(CountBy(script, "CREATE_NEW_TOWN_SPELL", 1), (Tally {{"LIGHTNING_BOLT", 4},
	                                                               {"LIGHTNING_BOLT_PU1", 4},
	                                                               {"STORM", 4},
	                                                               {"PHYSICAL_SHIELD", 4},
	                                                               {"HEAL", 3},
	                                                               {"TELEPORT", 3},
	                                                               {"WATER", 3},
	                                                               {"SHIELD", 3},
	                                                               {"FLYING_FLOCK", 3},
	                                                               {"CREATURE_SPELL_FREEZE", 3},
	                                                               {"CREATURE_SPELL_WEAK", 3},
	                                                               {"FIRE", 3},
	                                                               {"BEAM_EXPLOSION", 1},
	                                                               {"STORM_PU2", 1}}));

	// buildings: the planned ones by kind, whatever their tribe
	EXPECT_EQ(ByAbodeKind(CountBy(script, "CREATE_PLANNED_ABODE", 2)), (Tally {{"A", 17},
	                                                                           {"GRAVEYARD", 12},
	                                                                           {"B", 11},
	                                                                           {"E", 11},
	                                                                           {"F", 9},
	                                                                           {"C", 9},
	                                                                           {"WORKSHOP", 8},
	                                                                           {"D", 3},
	                                                                           {"STORAGE_PIT", 1},
	                                                                           {"WONDER", 1}}));

	// nature
	EXPECT_EQ(CountBy(script, "CREATE_NEW_TREE", 2), (Tally {{KindOf(TreeInfo::Beech), 128},
	                                                         {KindOf(TreeInfo::ConiferA), 123},
	                                                         {KindOf(TreeInfo::Palm), 116},
	                                                         {KindOf(TreeInfo::OakA), 100},
	                                                         {KindOf(TreeInfo::Birch), 64},
	                                                         {KindOf(TreeInfo::Olive), 48},
	                                                         {KindOf(TreeInfo::PalmA), 43},
	                                                         {KindOf(TreeInfo::Pine), 36},
	                                                         {KindOf(TreeInfo::Conifer), 20},
	                                                         {KindOf(TreeInfo::Cypress), 6}}));
	EXPECT_EQ(CountOf(script, "CREATE_NEW_BIG_FOREST"), 6);

	// food
	EXPECT_EQ(CountBy(script, "CREATE_NEW_TOWN_FIELD", 2), (Tally {{KindOf(FieldTypeInfo::WheatWithFence), 82}}));

	// animals
	EXPECT_EQ(CountBy(script, "CREATE_NEW_ANIMAL", 1), (Tally {{KindOf(AnimalInfo::Sheep), 192},
	                                                           {KindOf(AnimalInfo::Pig), 54},
	                                                           {KindOf(AnimalInfo::Pigeon), 42},
	                                                           {KindOf(AnimalInfo::Cow), 36},
	                                                           {KindOf(AnimalInfo::Seagull), 26},
	                                                           {KindOf(AnimalInfo::Crow), 17},
	                                                           {KindOf(AnimalInfo::Dove), 7}}));
	EXPECT_EQ(CountOf(script, "CREATE_FLOCK"), 71);

	// objects
	EXPECT_EQ(CountBy(script, "CREATE_MOBILE_STATIC", 1), (Tally {{KindOf(MobileStaticInfo::Boulder1Lime), 13},
	                                                              {KindOf(MobileStaticInfo::Boulder2Lime), 7},
	                                                              {KindOf(MobileStaticInfo::SharprockLimestone), 5},
	                                                              {KindOf(MobileStaticInfo::FlatrockLimestone), 3},
	                                                              {KindOf(MobileStaticInfo::SquarerockLimestone), 3},
	                                                              {KindOf(MobileStaticInfo::RockLimestone), 3},
	                                                              {KindOf(MobileStaticInfo::Boulder3Lime), 1}}));
	EXPECT_EQ(CountBy(script, "CREATE_STREET_LANTERN", 1), (Tally {{KindOf(MobileStaticInfo::StreetLantern), 48}}));
}

// Four Gods: the script is read whole, every command in the table with its arguments, and makes what its rows count
// (docs/wiki/project/progress/scripts/playground_scripts.md)
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(PlaygroundScriptContents, FourGodsLoadsWholeAndMakesWhatItsRowsCount)
{
	const auto source = RealScript("Playgrounds/FourGods.txt");
	if (!source.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set or Playgrounds/FourGods.txt missing";
	}
	const auto script = ReadCommands(*source);

	// loading: 1,550 lines, all read and all known, on its landscape, with one temple per player and its footpaths
	EXPECT_EQ(LineCount(*source), 1550);
	EXPECT_TRUE(script.unreadLines.empty());
	EXPECT_TRUE(LinesOutOfTheTable(script).empty());
	EXPECT_EQ(CountBy(script, "LOAD_LANDSCAPE", 0), (Tally {{".\\DATA\\LANDSCAPE\\MULTI_PLAYER\\MPM_4P_1.lnd", 1}}));
	EXPECT_EQ(CountOf(script, "CREATE_CITADEL"), 4);
	EXPECT_TRUE(HasFootpathFile("fourgods.fot"));

	// towns
	EXPECT_EQ(CountBy(script, "CREATE_TOWN", 4), (Tally {{"CELTIC", 4}, {"JAPANESE", 4}, {"INDIAN", 4}, {"TIBETAN", 1}}));
	EXPECT_EQ(CountBy(script, "CREATE_TOWN", 2),
	          (Tally {{"NEUTRAL", 9}, {"PLAYER_ONE", 1}, {"PLAYER_TWO", 1}, {"PLAYER_THREE", 1}, {"PLAYER_FOUR", 1}}));
	EXPECT_EQ(CountBy(script, "CREATE_NEW_TOWN_SPELL", 1), (Tally {{"LIGHTNING_BOLT", 5},
	                                                               {"HEAL", 4},
	                                                               {"TELEPORT", 4},
	                                                               {"FOOD", 4},
	                                                               {"WATER", 4},
	                                                               {"CREATURE_SPELL_FREEZE", 4},
	                                                               {"SHIELD", 4},
	                                                               {"PHYSICAL_SHIELD", 4},
	                                                               {"CREATURE_SPELL_STRONG", 4},
	                                                               {"CREATURE_SPELL_INVISIBLE", 4},
	                                                               {"LIGHTNING_BOLT_PU2", 1},
	                                                               {"BEAM_EXPLOSION", 1},
	                                                               {"BEAM_EXPLOSION_PU2", 1}}));

	// nature
	EXPECT_EQ(CountBy(script, "CREATE_NEW_TREE", 2), (Tally {{KindOf(TreeInfo::ConiferA), 320},
	                                                         {KindOf(TreeInfo::PalmB), 200},
	                                                         {KindOf(TreeInfo::Palm), 77},
	                                                         {KindOf(TreeInfo::PalmA), 36},
	                                                         {KindOf(TreeInfo::Conifer), 17},
	                                                         {KindOf(TreeInfo::Pine), 14}}));
	EXPECT_EQ(CountOf(script, "CREATE_NEW_BIG_FOREST"), 9);

	// food
	EXPECT_EQ(
	    CountBy(script, "CREATE_NEW_TOWN_FIELD", 2),
	    (Tally {{KindOf(FieldTypeInfo::Wheat), 14}, {KindOf(FieldTypeInfo::Corn), 10}, {KindOf(FieldTypeInfo::Cereal), 6}}));

	// animals
	EXPECT_EQ(CountBy(script, "CREATE_NEW_ANIMAL", 1), (Tally {{KindOf(AnimalInfo::Cow), 50},
	                                                           {KindOf(AnimalInfo::Pig), 50},
	                                                           {KindOf(AnimalInfo::Sheep), 34},
	                                                           {KindOf(AnimalInfo::Pigeon), 24},
	                                                           {KindOf(AnimalInfo::Seagull), 24},
	                                                           {KindOf(AnimalInfo::Dove), 20},
	                                                           {KindOf(AnimalInfo::Wolf), 16}}));
	EXPECT_EQ(CountOf(script, "CREATE_FLOCK"), 28);
}

// The demo's version of the third land: the script is read whole, every command in the table with its arguments,
// with the older animal and town field commands, no footpath file, and makes what its rows count
// (docs/wiki/project/progress/scripts/map_and_demo_scripts.md)
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(PlaygroundScriptContents, TheDemoLandLoadsWholeAndMakesWhatItsRowsCount)
{
	const auto source = RealScript("demo2.txt");
	if (!source.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set or demo2.txt missing";
	}
	const auto script = ReadCommands(*source);

	// loading: all read and all known, on the shipped third landscape, with the older commands and no footpaths
	EXPECT_TRUE(script.unreadLines.empty());
	EXPECT_TRUE(LinesOutOfTheTable(script).empty());
	EXPECT_EQ(CountBy(script, "LOAD_LANDSCAPE", 0), (Tally {{".\\DATA\\LANDSCAPE\\Land3.lnd", 1}}));
	EXPECT_GT(CountOf(script, "CREATE_ANIMAL"), 0);
	EXPECT_EQ(CountOf(script, "CREATE_NEW_ANIMAL"), 0);
	EXPECT_GT(CountOf(script, "CREATE_TOWN_FIELD"), 0);
	EXPECT_EQ(CountOf(script, "CREATE_NEW_TOWN_FIELD"), 0);
	EXPECT_FALSE(HasFootpathFile("demo2.fot"));

	// towns
	EXPECT_EQ(CountBy(script, "CREATE_TOWN", 4),
	          (Tally {{"CELTIC", 2}, {"JAPANESE", 1}, {"TIBETAN", 1}, {"INDIAN", 1}, {"EGYPTIAN", 1}}));
	EXPECT_EQ(CountBy(script, "CREATE_TOWN", 2), (Tally {{"NEUTRAL", 3}, {"PLAYER_ONE", 2}, {"PLAYER_TWO", 1}}));
	EXPECT_EQ(CountBy(script, "CREATE_NEW_TOWN_SPELL", 1), (Tally {{"HEAL", 2},
	                                                               {"FOOD", 2},
	                                                               {"LIGHTNING_BOLT", 2},
	                                                               {"CREATURE_SPELL_COMPASSION", 1},
	                                                               {"FIRE", 1},
	                                                               {"WATER", 1},
	                                                               {"CREATURE_SPELL_BIG", 1},
	                                                               {"STORM_PU1", 1},
	                                                               {"PHYSICAL_SHIELD", 1},
	                                                               {"WOOD", 1}}));

	// buildings: the planned ones by kind, whatever their tribe
	EXPECT_EQ(
	    ByAbodeKind(CountBy(script, "CREATE_PLANNED_ABODE", 2)),
	    (Tally {
	        {"F", 19}, {"C", 12}, {"B", 11}, {"A", 11}, {"D", 9}, {"E", 4}, {"CRECHE", 1}, {"WORKSHOP", 1}, {"WONDER", 1}}));
	EXPECT_EQ(
	    CountByPair(script, "CREATE_WORSHIP_SITE", 3, 2),
	    (Tally {{"JAPANESE/PLAYER_ONE", 1}, {"CELTIC/PLAYER_ONE", 1}, {"EGYPTIAN/PLAYER_TWO", 1}, {"TIBETAN/PLAYER_TWO", 1}}));

	// nature
	EXPECT_EQ(CountBy(script, "CREATE_NEW_TREE", 2), (Tally {{KindOf(TreeInfo::PalmB), 599},
	                                                         {KindOf(TreeInfo::Palm), 363},
	                                                         {KindOf(TreeInfo::BushB), 154},
	                                                         {KindOf(TreeInfo::Conifer), 88},
	                                                         {KindOf(TreeInfo::Pine), 80},
	                                                         {KindOf(TreeInfo::PalmC), 72},
	                                                         {KindOf(TreeInfo::BushA), 55},
	                                                         {KindOf(TreeInfo::Beech), 52},
	                                                         {KindOf(TreeInfo::Cedar), 50},
	                                                         {KindOf(TreeInfo::HedgeA), 41},
	                                                         {KindOf(TreeInfo::PalmA), 24},
	                                                         {KindOf(TreeInfo::Olive), 18},
	                                                         {KindOf(TreeInfo::Hedge), 16},
	                                                         {KindOf(TreeInfo::Copse), 10},
	                                                         {KindOf(TreeInfo::Cypress), 9},
	                                                         {KindOf(TreeInfo::ConiferA), 8},
	                                                         {KindOf(TreeInfo::Bush), 4}}));
	EXPECT_EQ(CountOf(script, "CREATE_NEW_BIG_FOREST"), 16);
	EXPECT_EQ(CountBy(script, "CREATE_NEW_FEATURE", 1),
	          (Tally {{"Spikey Pilar Lime", 20}, {"Pilar2 Lime", 4}, {"Tibetan Large Pillar Feature", 1}}));
	EXPECT_EQ(CountOf(script, "CREATE_MIST"), 70);

	// food: the older town field command
	EXPECT_EQ(CountBy(script, "CREATE_TOWN_FIELD", 2), (Tally {{KindOf(FieldTypeInfo::Wheat), 8}}));

	// animals: the older animal command
	EXPECT_EQ(CountBy(script, "CREATE_ANIMAL", 1),
	          (Tally {{KindOf(AnimalInfo::Cow), 39}, {KindOf(AnimalInfo::Dove), 16}, {KindOf(AnimalInfo::Sheep), 6}}));
	EXPECT_EQ(CountOf(script, "CREATE_FLOCK"), 6);

	// objects
	EXPECT_EQ(CountBy(script, "CREATE_MOBILE_STATIC", 1), (Tally {{KindOf(MobileStaticInfo::Rock), 87},
	                                                              {KindOf(MobileStaticInfo::Boulder1Sand), 7},
	                                                              {KindOf(MobileStaticInfo::CeltFenceTall), 31},
	                                                              {KindOf(MobileStaticInfo::WeepingStoneReward), 18},
	                                                              {KindOf(MobileStaticInfo::Vortex), 1},
	                                                              {KindOf(MobileStaticInfo::CreatureCage), 1}}));
}
