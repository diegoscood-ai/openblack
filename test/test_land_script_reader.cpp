/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The land-script reader (LHScriptX: the line reader's words and numbers, Script::Load and the command dispatch) driven
// by small in-memory scripts through the real command table. The commands used only write the map script's globals, a
// drink waypoint or a town, so their effects are read back from the registry and the globals; the land is a height
// field fake. No game data.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "CHLApi.h"
#include "Common/EventManager.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/Components/MapSimData.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Town/TownQueries.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "LHScriptX/LineReader.h"
#include "LHScriptX/Script.h"
#include "LHScriptX/ScriptWriter.h"
#include "Locator.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::lhscriptx::Script;
namespace line_reader = openblack::lhscriptx::line_reader;

namespace
{
/// Every word of a line, as the reader takes them one after the other, and whether the end of the line was seen after
/// each
std::vector<std::pair<line_reader::Word, bool>> WordsOf(std::string_view line, size_t count)
{
	line_reader::LineCursor cursor(line);
	std::vector<std::pair<line_reader::Word, bool>> words;
	for (size_t i = 0; i < count; ++i)
	{
		auto word = cursor.Next();
		words.emplace_back(std::move(word), cursor.EndOfLine());
	}
	return words;
}

class LandScriptReader: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the reader's warnings go to the scripts' logger: kept here, message only, to be read back
		spdlog::drop("scripting");
		_log = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(64);
		auto logger = std::make_shared<spdlog::logger>("scripting", _log);
		logger->set_pattern("%v");
		spdlog::register_logger(logger);

		test::EmplaceMapAndVillagerDefaults();
		Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::events::emplace<EventManager>();
		// the land's height is half the z of the point asked for, so a point's height shows where it was taken
		Locator::terrainSystem::emplace<openblack::test::HeightFieldIsland>([](glm::vec2 xz) { return xz.y * 0.5f; });
	}

	void TearDown() override
	{
		Locator::terrainSystem::reset();
		Locator::events::reset();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
		// the next tests find a quiet scripts' logger, as the other script tests make it
		spdlog::drop("scripting");
		spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	static MapScriptGlobals& Globals() { return Locator::mapScriptSystem::value().Globals(); }

	/// Reads the script and returns how many of its commands were carried out
	static int Run(const std::string& source)
	{
		int commands = 0;
		Script script;
		script.Load(source, [&commands]() { ++commands; });
		return commands;
	}

	/// The reader's warnings so far, joined
	[[nodiscard]] std::string Warnings() const
	{
		std::string all;
		for (const auto& line : _log->last_formatted())
		{
			all += line;
		}
		return all;
	}

	/// The drink waypoints' positions, in the order the land made them
	static std::vector<glm::vec3> Waypoints()
	{
		std::vector<glm::vec3> points(Reg().Size<DrinkWaypoint>());
		Reg().Each<const DrinkWaypoint>(
		    [&points](entt::entity /*waypoint*/, const DrinkWaypoint& point) { points.at(point.number) = point.position; });
		return points;
	}

	std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> _log;
};
} // namespace

// The reader cuts a command line into words: the command's name ends at its "(", each argument at its ",", the last at
// its ")", and the end of the line is seen after the last word. A bare word is read in capitals and a quoted text keeps
// its case, as the original (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST(LandScriptLineReader, CutsALineIntoTheCommandAndItsArguments)
{
	const auto words = WordsOf("CREATE_TOWN_VILLAGER(1, \"3079.69,3117.83\", celtic_fisherman, -26, 2.5)", 7);
	EXPECT_EQ(words[0].first.text, "CREATE_TOWN_VILLAGER");
	EXPECT_FALSE(words[0].first.quoted);
	EXPECT_EQ(words[1].first.text, "1");
	EXPECT_EQ(words[2].first.text, "3079.69,3117.83");
	EXPECT_TRUE(words[2].first.quoted);
	EXPECT_EQ(words[3].first.text, "CELTIC_FISHERMAN");
	EXPECT_EQ(words[4].first.text, "-26");
	EXPECT_FALSE(words[4].second);
	EXPECT_EQ(words[5].first.text, "2.5");
	EXPECT_TRUE(words[5].second);
	EXPECT_EQ(words[6].first.text, "");
	EXPECT_TRUE(words[6].second);
}

// A carriage return is a space like any other, so in a line of a CRLF file the end of the line is seen only one word
// later than in an LF file, as the original (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST(LandScriptLineReader, ACarriageReturnIsOneMoreSpace)
{
	const auto crlf = WordsOf("SET_LAND_NUMBER(4)\r", 3);
	EXPECT_EQ(crlf[1].first.text, "4");
	EXPECT_FALSE(crlf[1].second);
	EXPECT_EQ(crlf[2].first.text, "");
	EXPECT_TRUE(crlf[2].second);
	const auto lf = WordsOf("SET_LAND_NUMBER(4)", 2);
	EXPECT_TRUE(lf[1].second);
}

// "=", "!", "<" and ">" make words of their own, with a second "=" or ">"; ":" ends a word like ","; after a quoted text
// the spaces and one separator are passed over; a quoted text runs to its closing quote or to the end of the line, so a
// doubled quote starts another text; the line ends at its first NUL, as the original
// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST(LandScriptLineReader, OperatorsSeparatorsQuotesAndNul)
{
	const auto assignment = WordsOf("speed=5", 3);
	EXPECT_EQ(assignment[0].first.text, "SPEED");
	EXPECT_EQ(assignment[1].first.text, "=");
	EXPECT_EQ(assignment[2].first.text, "5");
	const auto compare = WordsOf("a<=b <> !c", 6);
	EXPECT_EQ(compare[0].first.text, "A");
	EXPECT_EQ(compare[1].first.text, "<=");
	EXPECT_EQ(compare[2].first.text, "B");
	EXPECT_EQ(compare[3].first.text, "<>");
	EXPECT_EQ(compare[4].first.text, "!");
	EXPECT_EQ(compare[5].first.text, "C");
	const auto colon = WordsOf("a:b \"Text\" , next", 4);
	EXPECT_EQ(colon[0].first.text, "A");
	EXPECT_EQ(colon[1].first.text, "B");
	EXPECT_EQ(colon[2].first.text, "Text");
	EXPECT_EQ(colon[3].first.text, "NEXT");
	const auto doubled = WordsOf("\"Defeat the Norse God!\"\", 0)", 2);
	EXPECT_EQ(doubled[0].first.text, "Defeat the Norse God!");
	EXPECT_FALSE(doubled[0].second);
	EXPECT_EQ(doubled[1].first.text, ", 0)");
	EXPECT_TRUE(doubled[1].first.quoted);
	EXPECT_TRUE(doubled[1].second);
	const auto nul = WordsOf(std::string_view("A(1)\0B(2)", 9), 3);
	EXPECT_EQ(nul[1].first.text, "1");
	EXPECT_TRUE(nul[1].second);
	EXPECT_EQ(nul[2].first.text, "");
}

// The spaces between words are the C locale's, and the eight bytes above 0x7F that the original's C runtime answers as
// spaces for a negative character (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST(LandScriptLineReader, TheSpacesBetweenWords)
{
	for (const char c : {' ', '\t', '\n', '\v', '\f', '\r', '\x9D', '\x9E', '\xAD', '\xAE', '\xB6', '\xBE', '\xFB', '\xFD'})
	{
		EXPECT_TRUE(line_reader::IsWordSpace(c)) << static_cast<int>(static_cast<unsigned char>(c));
	}
	for (const char c : {'\0', 'a', '_', ',', '\x80', '\xA0', '\xC3', '\xFF'})
	{
		EXPECT_FALSE(line_reader::IsWordSpace(c)) << static_cast<int>(static_cast<unsigned char>(c));
	}
}

// A whole number is read as the original's atol reads it: spaces, one sign, the digits to the first other character,
// wrapping modulo 2^32 (the original's answers, read with the oracle;
// docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST(LandScriptLineReader, AWholeNumberIsReadAsAtolReadsIt)
{
	EXPECT_EQ(line_reader::AtoL("99999900000"), 1215652192);
	EXPECT_EQ(line_reader::AtoL("-99999900000"), -1215652192);
	EXPECT_EQ(line_reader::AtoL("  +5"), 5);
	EXPECT_EQ(line_reader::AtoL(" \t\v\f\r12x"), 12);
	EXPECT_EQ(line_reader::AtoL("1.9"), 1);
	EXPECT_EQ(line_reader::AtoL("1e3"), 1);
	EXPECT_EQ(line_reader::AtoL(".5"), 0);
	EXPECT_EQ(line_reader::AtoL(""), 0);
	EXPECT_EQ(line_reader::AtoL("abc"), 0);
	EXPECT_EQ(line_reader::AtoL("-"), 0);
	EXPECT_EQ(line_reader::AtoL("+-5"), 0);
	EXPECT_EQ(line_reader::AtoL("2147483648"), std::numeric_limits<int32_t>::min());
	EXPECT_EQ(line_reader::AtoL("  -2147483649"), std::numeric_limits<int32_t>::max());
	EXPECT_EQ(line_reader::AtoL("4294967297"), 1);
	EXPECT_EQ(line_reader::AtoL("\xA0"
	                            "12"),
	          0);
}

// A fraction is read as the original's atof reads it: a sign, the digits with one point, an exponent after e, E, d or
// D; no hexadecimal, inf or nan (the original's answers, read with the oracle;
// docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST(LandScriptLineReader, AFractionIsReadAsAtofReadsIt)
{
	EXPECT_EQ(line_reader::AtoF("1e3"), 1000.0);
	EXPECT_EQ(line_reader::AtoF("1d3"), 1000.0);
	EXPECT_EQ(line_reader::AtoF("1D3"), 1000.0);
	EXPECT_EQ(line_reader::AtoF("1.5d-1"), 0.15);
	EXPECT_EQ(line_reader::AtoF("+5"), 5.0);
	EXPECT_EQ(line_reader::AtoF(".5"), 0.5);
	EXPECT_EQ(line_reader::AtoF("-.5e1"), -5.0);
	EXPECT_EQ(line_reader::AtoF(" 12.5xyz"), 12.5);
	EXPECT_EQ(line_reader::AtoF("1.2.3"), 1.2);
	EXPECT_EQ(line_reader::AtoF("12,34"), 12.0);
	EXPECT_EQ(line_reader::AtoF("1.5e"), 1.5);
	EXPECT_EQ(line_reader::AtoF("1.5e+"), 1.5);
	EXPECT_EQ(line_reader::AtoF("3079.69"), 3079.69);
	EXPECT_EQ(line_reader::AtoF("0x10"), 0.0);
	EXPECT_EQ(line_reader::AtoF("inf"), 0.0);
	EXPECT_EQ(line_reader::AtoF("nan"), 0.0);
	EXPECT_EQ(line_reader::AtoF("abc"), 0.0);
	EXPECT_EQ(line_reader::AtoF(""), 0.0);
	EXPECT_EQ(line_reader::AtoF("-.e1"), 0.0);
	EXPECT_EQ(line_reader::AtoF("1e-400"), 0.0);
	EXPECT_TRUE(std::isinf(line_reader::AtoF("1e400")));
}

// A position's text in map units: each fraction x 65536 / 10 in floats, truncated; z 0 with no comma; the altitude
// after a second comma as it is, as the original (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST(LandScriptLineReader, APositionIsReadInMapUnits)
{
	using map_coords::MapCoords;
	EXPECT_EQ(line_reader::PositionUnits("2185.72,2315.78"), (MapCoords {.x = 14324334, .z = 15176696}));
	EXPECT_EQ(line_reader::PositionUnits("1010.625,20.625"), (MapCoords {.x = 6623232, .z = 135168}));
	EXPECT_EQ(line_reader::PositionUnits("-3.7,0.55"), (MapCoords {.x = -24248, .z = 3604}));
	EXPECT_EQ(line_reader::PositionUnits("3252.30, 3431.16"), (MapCoords {.x = 21314274, .z = 22486450}));
	EXPECT_EQ(line_reader::PositionUnits("100"), (MapCoords {.x = 655360, .z = 0}));
	EXPECT_EQ(line_reader::PositionUnits("1.5,2.5,7.25"), (MapCoords {.x = 9830, .z = 16384, .altitude = 7.25f}));
	EXPECT_EQ(line_reader::PositionUnits("0.0001,99999.9"), (MapCoords {.x = 0, .z = 655359360}));
}

// A text is cut into lines at its line feeds: an empty line is passed over, a carriage return stays in its line, a
// last line with no line feed after it is read too, and the reading stops when a line says so, as the original
// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST(LandScriptLineReader, ATextIsCutIntoItsLines)
{
	std::vector<std::pair<std::string, int>> lines;
	line_reader::ForEachLine("A(1)\r\n\nB(2)\nC(3)", [&lines](std::string_view line, int number) {
		lines.emplace_back(line, number);
		return true;
	});
	EXPECT_EQ(lines, (std::vector<std::pair<std::string, int>> {{"A(1)\r", 1}, {"B(2)", 3}, {"C(3)", 4}}));

	lines.clear();
	line_reader::ForEachLine("A(1)\nB(2)\nC(3)\n", [&lines](std::string_view line, int number) {
		lines.emplace_back(line, number);
		return number < 2;
	});
	EXPECT_EQ(lines, (std::vector<std::pair<std::string, int>> {{"A(1)", 1}, {"B(2)", 2}}));
}

// A land script is read line by line, one command and its bracketed arguments a line, each carried out in turn; an
// empty line runs nothing and a last line with no newline after it still runs
// (docs/wiki/world/vortex.md#bringing-them-out-the-out-vortex, how the file is cut into lines). The lines here end in
// CRLF, as the shipped scripts do
TEST_F(LandScriptReader, ReadsOneCommandALine)
{
	EXPECT_EQ(Run("VERSION(2.300000)\r\n\r\nSET_LAND_NUMBER(4)\r\n\r\nSET_INTERACT_DESIRE(0.5)"), 3);
	EXPECT_FLOAT_EQ(Globals().version, 2.3f);
	EXPECT_EQ(Globals().landNumber, 4);
	EXPECT_FLOAT_EQ(Globals().interactDesire, 0.5f);
	EXPECT_TRUE(Warnings().empty());
}

// A word the game does not know runs nothing, and the lines after it still load
// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, UnknownCommandIsSkipped)
{
	EXPECT_EQ(Run("VERSION(2.3)\nNOT_A_COMMAND(1, \"10,20\", NAME)\nSET_LAND_NUMBER(4)\n"), 2);
	EXPECT_FLOAT_EQ(Globals().version, 2.3f);
	EXPECT_EQ(Globals().landNumber, 4);
}

// SET_A_TOWNS_INFLUENCE_MULTIPLIER is not a command of the original's land table: its line runs nothing and the
// lines after it load (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, SetATownsInfluenceMultiplierIsNoCommand)
{
	EXPECT_EQ(Run("SET_A_TOWNS_INFLUENCE_MULTIPLIER(1, 0.5)\nSET_LAND_NUMBER(4)\n"), 1);
	EXPECT_EQ(Globals().landNumber, 4);
}

// A rem line runs nothing, nor does a Player banner line, whose words the game does not know; the commands around them
// load (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, RemAndBannerLinesAreSkipped)
{
	EXPECT_EQ(Run("rem SET_LAND_NUMBER(9)\n**** Player[2] ****\nVERSION(2.3)\nREM VERSION(9)\nSET_LAND_NUMBER(4)\n"), 2);
	EXPECT_FLOAT_EQ(Globals().version, 2.3f);
	EXPECT_EQ(Globals().landNumber, 4);
}

// A position is written "x,z" in quotes: x and z in metres, with the relative height 0, so the point is on the ground
// there (docs/wiki/scripts/map-script-reader.md#the-map-script-reader-lhscriptx). 1010.625 and 20.625 m are whole
// numbers of map units (x 65536 / 10), so the original's truncation leaves them as they are
TEST_F(LandScriptReader, QuotedXZIsAPointOnTheGround)
{
	EXPECT_EQ(Run("CREATE_DRINK_WAYPOINT(\"1010.625,20.625\")\n"), 1);
	const auto points = Waypoints();
	ASSERT_EQ(points.size(), 1U);
	EXPECT_FLOAT_EQ(points[0].x, 1010.625f);
	EXPECT_FLOAT_EQ(points[0].z, 20.625f);
	EXPECT_FLOAT_EQ(points[0].y, 20.625f * 0.5f); // the fake land's height at that point
}

// When the position offset is set, it is added to the script's position in map units before the point is taken
// (docs/wiki/scripts/map-script-reader.md#the-map-script-reader-lhscriptx): 10 m and 5 m here
TEST_F(LandScriptReader, PositionOffsetIsAddedInMapUnits)
{
	Script::SetPositionOffset(map_coords::MapCoords {0x10000, 0x8000, 0.0f});
	EXPECT_EQ(Run("CREATE_DRINK_WAYPOINT(\"1010.625,20.625\")\n"), 1);
	Script::SetPositionOffset(std::nullopt);
	const auto points = Waypoints();
	ASSERT_EQ(points.size(), 1U);
	EXPECT_FLOAT_EQ(points[0].x, 1020.625f);
	EXPECT_FLOAT_EQ(points[0].z, 25.625f);
	EXPECT_FLOAT_EQ(points[0].y, 25.625f * 0.5f);
}

// A name may be written without quotes: the bare name is taken as the same text as the quoted one
// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read: a bare word is read as text, in capitals)
TEST_F(LandScriptReader, UnquotedNameIsTakenAsText)
{
	EXPECT_EQ(Run("CREATE_TOWN(4, \"120,80\", PLAYER_TWO, 0, NORSE)\n"
	              "CREATE_TOWN(5, \"300,80\", \"PLAYER_TWO\", 0, \"NORSE\")\n"),
	          2);
	const auto bare = ecs::town_queries::FindTownWithID(4);
	const auto quoted = ecs::town_queries::FindTownWithID(5);
	ASSERT_NE(bare, entt::entity {entt::null});
	ASSERT_NE(quoted, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<const Town>(bare).owner, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(Reg().Get<const Tribe>(bare), Tribe::NORSE);
	EXPECT_EQ(Reg().Get<const Town>(quoted).owner, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(Reg().Get<const Tribe>(quoted), Tribe::NORSE);
}

// Each argument becomes what the command wants. A fraction where a whole number is wanted is read as atol reads it,
// truncated towards zero, and goes to that argument's integer slot
// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read). A whole number where a fraction is wanted becomes that
// fraction, as atof reads it
TEST_F(LandScriptReader, ArgumentsAreConvertedToWhatTheCommandWants)
{
	EXPECT_EQ(Run("VERSION(2)\nSET_LAND_NUMBER(3.9)\n"), 2);
	EXPECT_FLOAT_EQ(Globals().version, 2.0f);
	EXPECT_EQ(Globals().landNumber, 3);
	EXPECT_EQ(Script::IntSlot(0), 3);

	EXPECT_EQ(Run("SET_LAND_NUMBER(-3.9)\n"), 1);
	EXPECT_EQ(Globals().landNumber, -3);
	EXPECT_EQ(Script::IntSlot(0), -3);
}

// CREATE_SPECIAL_TOWNVILLAGER, a misspelling of CREATE_SPECIAL_TOWN_VILLAGER, is not in the original's command table
// (docs/wiki/scripts/land-change-and-saving.md#the-writers, the table copied from the exe). As any word the game does
// not know, its line runs nothing, no villager is made, and the rest of the land loads
// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, MisspeltSpecialTownVillagerIsSkipped)
{
	EXPECT_EQ(lhscriptx::FindCommandFormat("CREATE_SPECIAL_TOWNVILLAGER"), nullptr);
	EXPECT_NE(lhscriptx::FindCommandFormat("CREATE_SPECIAL_TOWN_VILLAGER"), nullptr);

	EXPECT_EQ(Run("VERSION(2.3)\n"
	              "CREATE_SPECIAL_TOWNVILLAGER(1, \"100,200\", 3, 20)\n"
	              "CREATE_SPECIAL_TOWNVILLAGER(1, \"110,200\", 4, 30)\n"
	              "SET_LAND_NUMBER(4)\n"),
	          2);
	EXPECT_EQ(Reg().Size<Villager>(), 0U);
	EXPECT_EQ(Globals().landNumber, 4);
}

// VERSION keeps the land script's version in the map's globals, where the flocks read it, and the start of a map's
// features puts it back to 0, so a land without a VERSION line does not keep the last land's
// (docs/wiki/scripts/map-script-reader.md#globals,
// docs/wiki/scripts/map-script-reader.md#the-map-script-reader-lhscriptx)
TEST_F(LandScriptReader, VersionIsKeptUntilTheNextMapsFeatures)
{
	EXPECT_EQ(Run("VERSION(2.100000)\n"), 1);
	EXPECT_FLOAT_EQ(Globals().version, 2.1f);
	EXPECT_EQ(Run("SET_LAND_NUMBER(2)\n"), 1);
	EXPECT_FLOAT_EQ(Globals().version, 2.1f);
	Script::BeginMapFeatures();
	EXPECT_FLOAT_EQ(Globals().version, 0.0f);
}

// A script's line of land script (MAP_SCRIPT_FUNCTION) is a string of the program's, carried out with the land script's
// own commands. Nothing of the land's reading is set up again for it: the version an earlier line left and the position
// offset stay as they are (docs/wiki/scripts/map-script-reader.md#the-map-script-reader-lhscriptx)
TEST_F(LandScriptReader, AScriptsLandScriptLineRunsWithNothingOfTheLandsReadingSetUpAgain)
{
	const test::RestoreService<Locator::vm> restoreVm;
	auto& vm = Locator::vm::emplace<lhvm::LHVM>();
	const std::string landNumber = "SET_LAND_NUMBER(5)";
	const std::string waypoint = "CREATE_DRINK_WAYPOINT(\"1010.625,20.625\")";
	std::vector<char> data(landNumber.begin(), landNumber.end());
	data.push_back('\0');
	const auto waypointAt = static_cast<int32_t>(data.size());
	data.insert(data.end(), waypoint.begin(), waypoint.end());
	data.push_back('\0');
	ASSERT_EQ(vm.LoadBinary(lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, {}, {}, {}, {}, data)), EXIT_SUCCESS);
	chlapi::CHLApi api;
	const auto& table = api.GetFunctionsTable();
	const auto native = std::ranges::find_if(
	    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "MAP_SCRIPT_FUNCTION"; });
	ASSERT_NE(native, table.end());
	const auto runLine = [&vm, &native](int32_t line) {
		// a value of the script's own under the string, which the native leaves
		vm.Pushi(7777);
		vm.Pushi(line);
		native->impl();
		EXPECT_EQ(vm.Pop().intVal, 7777);
	};

	EXPECT_EQ(Run("VERSION(2.100000)\n"), 1);
	Script::SetPositionOffset(map_coords::MapCoords {0x10000, 0x8000, 0.0f});
	runLine(0);
	runLine(waypointAt);
	Script::SetPositionOffset(std::nullopt);
	EXPECT_EQ(Globals().landNumber, 5);
	EXPECT_FLOAT_EQ(Globals().version, 2.1f);
	const auto points = Waypoints();
	ASSERT_EQ(points.size(), 1U);
	EXPECT_FLOAT_EQ(points[0].x, 1020.625f);
	EXPECT_FLOAT_EQ(points[0].z, 25.625f);
}

// Only the whole word REM, in any case, is a remark, and it ends the line also after a command; a word that only starts
// with rem is a word the reader does not know, so a command after it runs, as the original
// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, OnlyTheWordRemIsARemarkAndItEndsTheLine)
{
	EXPECT_EQ(Run("SET_LAND_NUMBER(4) rem VERSION(9)\nRem SET_INTERACT_DESIRE(0.75)\nremark VERSION(2.5)\n"), 2);
	EXPECT_EQ(Globals().landNumber, 4);
	EXPECT_FLOAT_EQ(Globals().version, 2.5f);
	EXPECT_FLOAT_EQ(Globals().interactDesire, 0.0f);
}

// "*", "//" and a lone "/" are words the reader does not know, not remarks: the commands after them on their lines run,
// and a last line with no line feed after such a word ends the script, as the original
// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, StarsAndSlashesAreNoRemarks)
{
	EXPECT_EQ(Run("* SET_LAND_NUMBER(6)\n// VERSION(2.5)\n/ SET_INTERACT_DESIRE(0.25)\n****"), 3);
	EXPECT_EQ(Globals().landNumber, 6);
	EXPECT_FLOAT_EQ(Globals().version, 2.5f);
	EXPECT_FLOAT_EQ(Globals().interactDesire, 0.25f);
	EXPECT_EQ(Run("SET_LAND_NUMBER(2)\nrem the end"), 1);
	EXPECT_EQ(Globals().landNumber, 2);
}

// A whole number wraps modulo 2^32 as atol reads it, and a sign may be written, as the original
// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, AWholeNumberIsReadAsAtolReadsIt)
{
	EXPECT_EQ(Run("SET_LAND_NUMBER(99999900000)\n"), 1);
	EXPECT_EQ(Globals().landNumber, 1215652192);
	EXPECT_EQ(Script::IntSlot(0), 1215652192);
	EXPECT_EQ(Run("SET_LAND_NUMBER(+5)\n"), 1);
	EXPECT_EQ(Globals().landNumber, 5);
}

// A fraction may start with its point or a sign and have an exponent, as atof reads it, as the original
// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, AFractionIsReadAsAtofReadsIt)
{
	EXPECT_EQ(Run("VERSION(.5)\nSET_INTERACT_DESIRE(+2.5e-1)\n"), 2);
	EXPECT_FLOAT_EQ(Globals().version, 0.5f);
	EXPECT_FLOAT_EQ(Globals().interactDesire, 0.25f);
	EXPECT_EQ(Run("VERSION(1e3)\n"), 1);
	EXPECT_FLOAT_EQ(Globals().version, 1000.0f);
}

// No argument's kind is refused: a quoted text or a bare word given for a number is read as its number (0 when it does
// not start with one), as the original (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, ATextGivenForANumberIsReadAsANumber)
{
	EXPECT_EQ(Run("SET_LAND_NUMBER(\"7\")\nVERSION(\"2.5\")\n"), 2);
	EXPECT_EQ(Globals().landNumber, 7);
	EXPECT_FLOAT_EQ(Globals().version, 2.5f);
	EXPECT_EQ(Run("SET_LAND_NUMBER(12abc)\n"), 1);
	EXPECT_EQ(Globals().landNumber, 12);
	EXPECT_EQ(Run("SET_LAND_NUMBER(abc)\nVERSION(xyz)\n"), 2);
	EXPECT_EQ(Globals().landNumber, 0);
	EXPECT_FLOAT_EQ(Globals().version, 0.0f);
}

// A bare word is read in capitals, so a name written in small letters is the same name, as the original
// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, ABareWordIsReadInCapitals)
{
	EXPECT_EQ(Run("create_town(4, \"120,80\", player_two, 0, norse)\n"), 1);
	const auto town = ecs::town_queries::FindTownWithID(4);
	ASSERT_NE(town, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<const Town>(town).owner, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(Reg().Get<const Tribe>(town), Tribe::NORSE);
}

// SLONG, FLOAT and CBYTE make a script's variables, NAME = value gives one its value, and an argument that names a
// variable is read as its value: a whole number for a whole-number argument, a fraction for a fraction, a text for a
// text. The bare name is read in capitals. Making a variable runs no land command, as the original
// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, AVariablesValueIsReadForItsArgument)
{
	EXPECT_EQ(Run("SLONG(LAND)\nLAND = 6\nSET_LAND_NUMBER(land)\n"
	              "FLOAT(V)\nV=2.5\nVERSION(V)\n"
	              "CBYTE(TRIBE)\nTRIBE = norse\nCREATE_TOWN(4, \"120,80\", PLAYER_TWO, 0, TRIBE)\n"),
	          3);
	EXPECT_EQ(Globals().landNumber, 6);
	EXPECT_FLOAT_EQ(Globals().version, 2.5f);
	const auto town = ecs::town_queries::FindTownWithID(4);
	ASSERT_NE(town, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<const Tribe>(town), Tribe::NORSE);
}

// A variable of the wrong kind for its argument, a text variable never given a text, and the word ***VAR (read as the
// name of a variable that is not there) stop the script at their line, as the original
// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, AVariableThatDoesNotFitStopsTheScript)
{
	EXPECT_EQ(Run("SET_LAND_NUMBER(3)\nSLONG(N)\nVERSION(N)\nSET_LAND_NUMBER(4)\n"), 1);
	EXPECT_EQ(Globals().landNumber, 3);
	EXPECT_FLOAT_EQ(Globals().version, 0.0f);
	EXPECT_EQ(Run("CBYTE(T)\nSET_LAND_NUMBER(T)\nSET_LAND_NUMBER(5)\n"), 0);
	EXPECT_EQ(Globals().landNumber, 3);
	EXPECT_EQ(Run("***VAR\nSET_LAND_NUMBER(5)\n"), 0);
	EXPECT_EQ(Globals().landNumber, 3);
	EXPECT_NE(Warnings().find("the rest of the script is not read"), std::string::npos);
}

// A position takes its x before the first comma and its z after it, each read as atof reads a fraction: a third field
// is not part of x or z, a text with no comma gives z 0 and a text that is no number gives 0, all in map units, as the
// original
// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, APositionIsReadUpToItsCommas)
{
	EXPECT_EQ(Run("CREATE_DRINK_WAYPOINT(\"1010.625,20.625,3.5\")\n"
	              "CREATE_DRINK_WAYPOINT(\"30.5\")\n"
	              "CREATE_DRINK_WAYPOINT(\"x,20.625\")\n"),
	          3);
	const auto points = Waypoints();
	ASSERT_EQ(points.size(), 3U);
	EXPECT_FLOAT_EQ(points[0].x, 1010.625f);
	EXPECT_FLOAT_EQ(points[0].z, 20.625f);
	EXPECT_EQ(points[1].x, map_coords::ToMetres(199884)); // 30.5 m in map units, truncated
	EXPECT_FLOAT_EQ(points[1].z, 0.0f);
	EXPECT_FLOAT_EQ(points[1].y, 0.0f);
	EXPECT_FLOAT_EQ(points[2].x, 0.0f);
	EXPECT_FLOAT_EQ(points[2].z, 20.625f);
}

// In a file whose lines end in CRLF, the carriage return is one more word, so a command missing its last argument takes
// it as empty (0) and runs. An empty argument between two commas, or between the brackets, is an empty word too, as
// the original (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, AMissingLastArgumentInACrlfLineIsEmpty)
{
	EXPECT_EQ(Run("BRUSH_SIZE(1.5)\r\nVERSION(2.5)\r\n"), 2);
	EXPECT_FLOAT_EQ(Globals().version, 2.5f);
	EXPECT_EQ(Run("SET_LAND_NUMBER(7)\nSET_LAND_NUMBER()\n"), 2);
	EXPECT_EQ(Globals().landNumber, 0);
	EXPECT_EQ(Run("CREATE_TOWN(4, \"120,80\", PLAYER_TWO, , NORSE)\n"), 1);
	EXPECT_NE(ecs::town_queries::FindTownWithID(4), entt::entity {entt::null});
}

// A command missing more than its last argument in a CRLF file, or missing any in an LF file, stops the whole script
// at its line, as the original (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, MoreMissingArgumentsStopTheScript)
{
	EXPECT_EQ(Run("SET_LAND_NUMBER(7)\nBRUSH_SIZE(1.5)\nVERSION(2.5)\n"), 1);
	EXPECT_EQ(Globals().landNumber, 7);
	EXPECT_FLOAT_EQ(Globals().version, 0.0f);
	EXPECT_EQ(Run("SET_NIGHTTIME(1700.0)\r\nVERSION(2.5)\r\n"), 0);
	EXPECT_FLOAT_EQ(Globals().version, 0.0f);
	EXPECT_NE(Warnings().find("the rest of the script is not read"), std::string::npos);
}

// A doubled quote ends the quoted text, and the second quote starts another text that runs to the end of the line,
// read as any other word; the command still runs and the next line loads, as the original
// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, ADoubledQuoteIsReadAsTwoTexts)
{
	EXPECT_EQ(Run("SET_LAND_NUMBER(\"5\"\")\nVERSION(2.5)\n"), 2);
	EXPECT_EQ(Globals().landNumber, 5);
	EXPECT_FLOAT_EQ(Globals().version, 2.5f);
}

// Every command on a line runs, in turn, with or without a space between them; arguments beyond a command's own are
// words the reader does not know, as the original (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, EveryCommandOnALineRuns)
{
	EXPECT_EQ(Run("SET_LAND_NUMBER(4, 5, 6) VERSION(2.5)SET_INTERACT_DESIRE(0.25)\n"), 3);
	EXPECT_EQ(Globals().landNumber, 4);
	EXPECT_FLOAT_EQ(Globals().version, 2.5f);
	EXPECT_FLOAT_EQ(Globals().interactDesire, 0.25f);
}

// The reader's own commands for the front end (OPEN_SCREEN, LOAD_BMP...) are not ported: their arguments are read as
// the original reads them and nothing is done, so a command after them on the line runs
// (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read)
TEST_F(LandScriptReader, TheFrontEndsOwnCommandsAreReadAndDoNothing)
{
	EXPECT_EQ(Run("OPEN_SCREEN(640, 480, 16, 0) SET_LAND_NUMBER(4)\n"), 1);
	EXPECT_EQ(Globals().landNumber, 4);
}
