/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The map script a new game reads before its first land (src/LHScriptX/MapScript.h), against the wiki
// (docs/wiki/scripts/land-change-and-saving.md#the-map-script-of-a-new-game): its LOAD_FEATURE_SCRIPT lines, read as a
// land script is, and the land's file in the Scripts folder whatever its case. Synthetic texts only.

#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "LHScriptX/MapScript.h"

namespace map_script = openblack::lhscriptx::map_script;

TEST(MapScript, TheShippedMapScriptNamesTheFirstLand)
{
	// the whole of the game's own Scripts/Map.txt
	const std::string source = "LOAD_FEATURE_SCRIPT(\".\\scripts\\land1.txt\")\r\n";
	EXPECT_EQ(map_script::FeatureScripts(source), std::vector<std::string> {".\\scripts\\land1.txt"});
}

TEST(MapScript, RemarksAndOtherCommandsNameNoLand)
{
	const std::string source = "REM LOAD_FEATURE_SCRIPT(\"rem.txt\")\n"
	                           "SET_DATE(1, 2, 3)\n"
	                           "LOAD_LANDSCAPE(\".\\data\\landscape\\land1.lnd\")\n"
	                           "PAUSE_GAME()\n";
	EXPECT_TRUE(map_script::FeatureScripts(source).empty());
}

TEST(MapScript, EachLandComesInTheOrderOfTheText)
{
	const std::string source = "LOAD_FEATURE_SCRIPT(\"a.txt\")\nSET_TIME(1, 2, 3)\n\nLOAD_FEATURE_SCRIPT(\"b.txt\")";
	EXPECT_EQ(map_script::FeatureScripts(source), (std::vector<std::string> {"a.txt", "b.txt"}));
}

TEST(MapScript, TheCommandIsReadInCapitals)
{
	EXPECT_EQ(map_script::FeatureScripts("load_feature_script(\"a.txt\")\n"), std::vector<std::string> {"a.txt"});
}

TEST(MapScript, AMissingArgumentStopsTheScript)
{
	const std::string source = "LOAD_FEATURE_SCRIPT\nLOAD_FEATURE_SCRIPT(\"b.txt\")\n";
	EXPECT_TRUE(map_script::FeatureScripts(source).empty());
}

TEST(MapScript, ALandInTheScriptsFolderWhateverItsCaseAndSeparators)
{
	EXPECT_EQ(map_script::InScriptsFolder(".\\scripts\\land1.txt"), std::optional<std::string>("land1.txt"));
	EXPECT_EQ(map_script::InScriptsFolder("Scripts/Land2.txt"), std::optional<std::string>("Land2.txt"));
	EXPECT_FALSE(map_script::InScriptsFolder(".\\data\\land1.txt").has_value());
	EXPECT_FALSE(map_script::InScriptsFolder(".\\scripts\\playgrounds\\land1.txt").has_value());
	EXPECT_FALSE(map_script::InScriptsFolder("land1.txt").has_value());
}

TEST(MapScript, TheFileIsFoundUnderTheNameItHasOnDisk)
{
	const std::vector<std::string> files {"InfoScript2.txt", "Land1.txt", "map.txt"};
	EXPECT_EQ(map_script::MatchingName("land1.txt", files), std::optional<std::string>("Land1.txt"));
	EXPECT_EQ(map_script::MatchingName("Map.txt", files), std::optional<std::string>("map.txt"));
	EXPECT_FALSE(map_script::MatchingName("land9.txt", files).has_value());
}
