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
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Mod.h"
#include "Semver.h"

namespace openblack::mods
{

/// The version of the mod API this openblack offers (mod.json "api" is checked against it)
inline constexpr Version k_ApiVersion {1, 2, 0}; ///< 1.1: the game's geometry and clock; 1.2: sound (Api.h)
/// The newest mod.json / modpack.json "schema" this openblack reads
inline constexpr int k_ManifestSchema = 1;

/// What reading a manifest gave: the mod (null if the manifest is unusable) and what was wrong with it
struct ManifestResult
{
	std::unique_ptr<Mod> mod;
	std::vector<std::string> errors;   ///< the mod could not be made
	std::vector<std::string> warnings; ///< the mod was made, without the parts named here
};

/// A mod.json (docs/bw1-notes/mod-library.md, "mod.json"). `root` is its folder (empty for a manifest built into
/// openblack whose folder does not exist yet: the registry sets Mods/<id>); `pack` the modpack it is inside, if any
[[nodiscard]] ManifestResult ParseManifest(std::string_view json, const std::filesystem::path& root, std::string_view pack);

/// A modpack.json; nullopt (with the errors) if unusable
[[nodiscard]] std::optional<Modpack> ParseModpack(std::string_view json, const std::filesystem::path& root,
                                                  std::vector<std::string>& errors);

/// The language names and descriptions are shown in when a manifest gives several ("name": {"en": ..., "es": ...});
/// "en" by default
void SetLanguage(std::string language);
[[nodiscard]] const std::string& GetLanguage();

} // namespace openblack::mods
