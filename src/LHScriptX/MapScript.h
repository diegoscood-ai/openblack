/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// The game's map script (Scripts/Map.txt), which a new game runs before its first land: the shipped one has one line,
/// LOAD_FEATURE_SCRIPT, which names the first land's script. Pure functions with no game state; the game reads the file
/// and loads the land. Wiki: docs/wiki/scripts/land-change-and-saving.md#the-map-script-of-a-new-game.
namespace openblack::lhscriptx::map_script
{

/// The map script's name in the Scripts folder
inline constexpr std::string_view k_FileName = "Map.txt";
/// The command that loads a land's script
inline constexpr std::string_view k_LoadFeatureScript = "LOAD_FEATURE_SCRIPT";

/// The path of each LOAD_FEATURE_SCRIPT of the script's text, in order, as the script writes it (".\scripts\land1.txt"):
/// read line by line and word by word as a land script is, the rest of a line after REM passed over and every other
/// command left alone (no shipped map script has another). A LOAD_FEATURE_SCRIPT with no argument stops the script
[[nodiscard]] std::vector<std::string> FeatureScripts(std::string_view source);

/// The file name of a script path that lies in the Scripts folder of the game's folder, written as the map script
/// writes it (".\scripts\land1.txt", "scripts/land1.txt", any case); none for a path elsewhere
[[nodiscard]] std::optional<std::string> InScriptsFolder(std::string_view path);

/// The one of `names` (a folder's files) that is `name` whatever its case, the first one when several are; none when
/// none is
[[nodiscard]] std::optional<std::string> MatchingName(std::string_view name, std::span<const std::string> names);

} // namespace openblack::lhscriptx::map_script
