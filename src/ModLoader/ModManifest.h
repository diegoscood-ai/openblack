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
#include <set>
#include <string>
#include <string_view>

// What the mod loader reads of a mods folder, as pure functions of the files' text: each mod's manifest
// (`<mod>/mod.txt`) and the list of the mods switched on (`enabled.txt`). The library reads and writes the files.
namespace mod_loader
{

/// The manifest format these rules read; a manifest of another format is not a mod (yet)
inline constexpr int k_ManifestFormat = 1;
/// A mod's manifest, in its own folder of the mods folder
inline constexpr std::string_view k_ManifestFile = "mod.txt";
/// The mods switched on, one folder name a line, in the mods folder
inline constexpr std::string_view k_EnabledFile = "enabled.txt";

/// A mod as its manifest names it
struct Manifest
{
	std::string name;
	std::string version;
	std::string author;
};

/// A manifest's text: `key: value` lines, whitespace around both trimmed, blank lines and lines starting with `#`
/// ignored. The first key must be `format`, holding k_ManifestFormat; `name` is needed and not empty; `version` and
/// `author` may be missing (empty); other keys are ignored, for later formats; a key given twice keeps its first value.
/// Nothing when the text is not such a manifest
[[nodiscard]] std::optional<Manifest> ParseManifest(std::string_view text);

/// The folder names enabled.txt switches on: one a line, trimmed, blank lines and `#` lines ignored
[[nodiscard]] std::set<std::string> ParseEnabled(std::string_view text);
/// enabled.txt's text for the folder names switched on, one a line in their order, each ended by a new line
[[nodiscard]] std::string WriteEnabled(const std::set<std::string>& enabled);

/// Whether a folder name can be a mod's: not empty, not `.` or `..`, and no path separator
[[nodiscard]] bool IsModFolderName(std::string_view name);

} // namespace mod_loader
