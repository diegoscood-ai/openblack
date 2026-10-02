/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

/// The data files of the mods that come with openblack, in JSON (with // comments) or in their old .cfg form:
///
///   foliage.json  {"schema": 1, "rules": [{"section": "grass", "images": ["a.png"], "per_cell": 90, ...}, ...]}
///   textures.json {"schema": 1, "textures": {"2": "c814f509", ...}}
///
/// Their readers (3D/Foliage.cpp, Resources/HdTextures.cpp) still read the .cfg text, so a .json is turned into exactly
/// that text: "[section]" and "key = value" lines, a list joined with ", ", true/false as on/off. tools/mod_cfg_to_json.py
/// converts the old files and checks the result is the same.
namespace openblack::mods::rule_files
{

/// The rules of <folder>/<stem>.json as .cfg text, else <folder>/<stem>.cfg as it is; nullopt if neither exists (or the
/// JSON is broken: then `error` says why). `used` is the file that was read
[[nodiscard]] std::optional<std::string> Read(const std::filesystem::path& folder, std::string_view stem,
                                              std::filesystem::path& used, std::string& error);

/// The JSON text of a foliage or textures file as .cfg text (tests); nullopt and `error` if it is not one
[[nodiscard]] std::optional<std::string> JsonToCfg(std::string_view json, std::string& error);

} // namespace openblack::mods::rule_files
